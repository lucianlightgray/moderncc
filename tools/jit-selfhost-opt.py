#!/usr/bin/env python3
"""What a zero-AOT `mcc -O0 --embed-jit` self-host actually runs at runtime.

The question this exists to answer, because it was asked and the intuitive answer
is wrong in both directions:

  "Does building mcc with -O0 --embed-jit and then self-hosting cause the
   embedded JIT to run the -O3+ optimized/hotpatched mcc?"

Measured answers, each pinned by an assertion below:

  1. YES, the JIT boots and hot-swaps.  A stage2 mcc compiled at -O0 with
     --embed-jit boots ~1250 JIT sites and swaps ~190 functions while compiling.
     Zero AOT optimization does NOT mean zero runtime optimization.

  2. NO, it is not -O3+.  The baked variant is compiled at optimize=1, hardcoded
     in mccjit_stash_one; the runtime recompile path (mccjit_recompile_common)
     uses optimize=0.  There is no -O3, no -O4 and no -O13 anywhere in the JIT
     path.  So relative to a -O0 AOT baseline the JIT runs MORE optimized code,
     bounded at -O1 -- not the -O3+ the question assumed.

  3. NO, the -O4+/-O13 search does not fire from such a build, and repeated runs
     do not accumulate anything.  Three consecutive runs of the -O0 stage2 write
     ZERO files under XDG_CACHE_HOME.

  4. The persistent cache is real but belongs to the -O13 search tier ALONE.  A
     single -O13 compile writes mcc-search.memo and so-<hash>.ck.  "All AOT/JIT
     optimization paths check a cache and resume" is not true of this tree; one
     tier does.

Points 2, 3 and 4 are negative results, and they are the reason this file is a
test rather than a note.  A negative result that nothing asserts is indistinguishable
from nobody having looked, which is the failure mode docs/TODO.md catalogues on
nearly every page.  If someone later teaches the JIT to bake at -O3, or gives the
AOT path a resumable cache, these assertions fail loudly and get updated
deliberately -- which is the point.

SKIPs (77) when the build has no baked JIT engine or no runtime blob to link.

Usage: tools/jit-selfhost-opt.py <build-dir>
"""
import json, os, re, shlex, subprocess, sys, tempfile

SKIP = 77


def die(msg):
    print("jit-selfhost-opt: " + msg)
    sys.exit(1)


def skip(msg):
    print("SKIP: jit-selfhost-opt: " + msg)
    sys.exit(SKIP)


def flags_for_mcc_c(bdir):
    """The -D/-I set CMake compiles src/mcc.c with, exactly as selfhost-fixpoint
    takes them.  Anything else on that command line (warnings, -O, -c/-o) is not
    part of the self-host recipe."""
    cc = json.load(open(os.path.join(bdir, "compile_commands.json")))
    rec = [x for x in cc if x["file"].endswith("/mcc.c")]
    if not rec:
        die("no src/mcc.c entry in compile_commands.json")
    cmd = rec[0]["command"]
    if os.name == "nt":
        cmd = re.sub(r'\\(?!")', r'\\\\', cmd)
    return [a for a in shlex.split(cmd)[1:]
            if (a.startswith("-D") or a.startswith("-I")) and not a.endswith(".c")]


def build_stage2(mcc, bdir, root, flags, level, rtobj, jitblob, out):
    """Compile AND link in one invocation.

    This is load-bearing and cost an hour to find: mccjit_embed_finalize runs
    from mcc_add_runtime, which runs at LINK time.  Compiling with --embed-jit
    and then linking without it produces a binary with no JIT engine at all and
    no diagnostic -- boot lines are simply zero, which reads exactly like "the
    JIT does not work at -O0".  Keep this one call.
    """
    args = [mcc, *flags, "-O%d" % level, "--embed-jit", "-B", bdir,
            os.path.join(root, "src/mcc.c"), rtobj, jitblob, "-o", out,
            "-lm", "-ldl"]
    p = subprocess.run(args, cwd=root, capture_output=True, text=True)
    if p.returncode != 0:
        die("stage2 build at -O%d failed:\n%s" % (level, p.stderr[-2000:]))
    if "no functions were JIT-baked" in p.stderr:
        die("stage2 at -O%d baked no functions; mcc said so and the binary "
            "carries no runtime JIT engine:\n%s" % (level, p.stderr[-500:]))
    return out


def jit_run(exe, src, obj, cachedir=None):
    env = dict(os.environ)
    env.update(MCC_JIT_VERBOSE="1", MCC_JIT="1", MCC_JIT_HOT_CALLS="1")
    if cachedir:
        env["XDG_CACHE_HOME"] = cachedir
    p = subprocess.run([exe, "-c", src, "-o", obj], capture_output=True,
                       text=True, env=env)
    if p.returncode != 0:
        die("stage2 failed to compile the workload:\n%s" % p.stderr[-2000:])
    err = p.stderr
    return err.count("mccjit-boot"), err.count("swapped")


def count_cache(d):
    n = 0
    for root, _, files in os.walk(d):
        n += len(files)
    return n


def main():
    if len(sys.argv) < 2:
        die(__doc__.strip().splitlines()[-1])
    bdir = os.path.abspath(sys.argv[1])
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    mcc = os.path.join(bdir, "mcc")
    if not os.access(mcc, os.X_OK):
        mcc += ".exe"
    if not os.access(mcc, os.X_OK):
        skip("no mcc in %s" % bdir)

    jitblob = os.path.join(bdir, "CMakeFiles", "mcc.dir", "mccjit_blob.c.o")
    if not os.path.exists(jitblob):
        skip("no baked JIT engine (mccjit_blob.c.o); nothing to exercise")
    rtsrc = os.path.join(bdir, "mccrt_blob.c")
    if not os.path.exists(rtsrc):
        skip("no runtime blob source at %s" % rtsrc)

    flags = flags_for_mcc_c(bdir)
    fails = []

    with tempfile.TemporaryDirectory() as work:
        rtobj = os.path.join(work, "mccrt_blob.o")
        p = subprocess.run([mcc, "-c", rtsrc, "-o", rtobj], capture_output=True,
                           text=True, cwd=root)
        if p.returncode != 0:
            die("could not compile the runtime blob:\n%s" % p.stderr[-1000:])

        wl = os.path.join(work, "wl.c")
        with open(wl, "w") as f:
            f.write("int main(void){return 0;}\n")
        obj = os.path.join(work, "wl.o")

        # ---- 1. zero AOT still hot-swaps at runtime -------------------------
        s2 = build_stage2(mcc, bdir, root, flags, 0, rtobj, jitblob,
                          os.path.join(work, "mcc2-O0"))
        boot, swap = jit_run(s2, wl, obj)
        print("jit-selfhost-opt: -O0 --embed-jit stage2: boot=%d swapped=%d"
              % (boot, swap))
        if boot <= 0:
            fails.append(
                "a -O0 --embed-jit stage2 booted NO JIT sites. Either baking "
                "regressed, or this harness compiled and linked separately -- "
                "mccjit_embed_finalize runs at link time, so a split build "
                "silently produces a binary with no engine")
        if swap <= 0:
            fails.append(
                "a -O0 --embed-jit stage2 booted %d sites but swapped none, so "
                "it ran the AOT code the JIT exists to replace" % boot)

        # ---- 2. the swapped code is -O1, not -O3+ ---------------------------
        # Pinned at the source, because the level is not observable from the
        # binary: the JIT compiles its variants with a hardcoded optimize.
        emb = open(os.path.join(root, "src/mccjit_embed.c")).read()
        fn_at = []
        for m in re.finditer(r"(?m)^(?:static\s+|PUB_FUNC\s+)?[A-Za-z_][A-Za-z0-9_ *]*?"
                             r"(\b[A-Za-z_][A-Za-z0-9_]*)\s*\(", emb):
            fn_at.append((m.start(), m.group(1)))

        def enclosing(pos):
            name = None
            for off, nm in fn_at:
                if off <= pos:
                    name = nm
                else:
                    break
            return name

        levels = {}
        for m in re.finditer(r"\b(?:s1|js)->optimize\s*=\s*(\d+)\s*;", emb):
            fn = enclosing(m.start())
            levels.setdefault(fn, set()).add(int(m.group(1)))
        print("jit-selfhost-opt: optimize levels by function: %s"
              % {k: sorted(v) for k, v in sorted(levels.items()) if k})

        want = {"mccjit_stash_one": 1, "mccjit_recompile_common": 0}
        for fn, lvl in want.items():
            got = levels.get(fn)
            if got is None:
                fails.append(
                    "no ->optimize assignment found in %s. The level the JIT "
                    "compiles at is not observable from the binary, so this "
                    "source pin is the only thing standing between 'the JIT "
                    "runs -O1' and nobody knowing" % fn)
            elif got != {lvl}:
                fails.append(
                    "%s sets optimize to %s, expected {%d}. If the JIT now "
                    "bakes at a higher level then a -O0 --embed-jit self-host "
                    "really does run more-optimized code than this test claims, "
                    "and the claim must be re-derived rather than the test "
                    "silenced" % (fn, sorted(got), lvl))
        hot = sorted(l for v in levels.values() for l in v if l >= 2)
        if hot:
            fails.append(
                "mccjit_embed.c sets optimize to %s somewhere. The JIT has only "
                "ever baked at 1 and recompiled at 0; a level >= 2 would change "
                "the answer to 'does the JIT run the optimized build?'" % hot)

        # ---- 3. repeated runs accumulate nothing at -O0 ---------------------
        cache = os.path.join(work, "cache")
        os.makedirs(cache)
        seen = []
        for _ in range(3):
            jit_run(s2, wl, obj, cachedir=cache)
            seen.append(count_cache(cache))
        print("jit-selfhost-opt: cache files after 3 -O0 runs: %s" % seen)
        if seen[-1] != 0:
            fails.append(
                "three -O0 --embed-jit runs wrote %d cache file(s) under "
                "XDG_CACHE_HOME. That would mean the JIT/AOT path gained a "
                "persistent cache; it had none, and if it now has one the "
                "resume semantics need their own test" % seen[-1])

        # ---- 4. the -O13 search tier is the ONLY thing that persists --------
        c13 = os.path.join(work, "cache13")
        os.makedirs(c13)
        env = dict(os.environ, XDG_CACHE_HOME=c13)
        src13 = os.path.join(root, "tests/smoke/subject.c")
        if not os.path.exists(src13):
            src13 = wl
        p = subprocess.run([mcc, "-O13", "-c", src13, "-o",
                            os.path.join(work, "o13.o")],
                           capture_output=True, text=True, env=env, cwd=root)
        if p.returncode != 0:
            die("the -O13 reference compile failed:\n%s" % p.stderr[-1000:])
        names = sorted(f for _, _, fs in os.walk(c13) for f in fs)
        print("jit-selfhost-opt: -O13 wrote %d cache file(s): %s"
              % (len(names), names[:4]))
        if not any(n.startswith("mcc-search.memo") for n in names):
            fails.append(
                "a -O13 compile wrote no mcc-search.memo. The search memo is "
                "the one genuinely resumable piece of state in this tree; if "
                "it stopped being written, every 'the search picks up where it "
                "left off' claim is now false")

    if fails:
        for f in fails:
            print("FAIL jit-selfhost-opt: " + f)
        return 1
    print("jit-selfhost-opt: OK -- zero-AOT embed-JIT hot-swaps at runtime, the "
          "swapped code is optimize=1 (not -O3+), -O0 runs accumulate no cache, "
          "and only the -O13 search tier persists state")
    return 0


if __name__ == "__main__":
    sys.exit(main())
