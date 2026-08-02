#!/usr/bin/env python3
import json, os, shlex, subprocess, sys, tempfile

USAGE = """usage: selfhost-output-parity.py <build-dir> [--opt=-O2] [--limit=N]

Builds a self-hosted mcc at the given -O level, then has BOTH the stage-0 mcc
and that stage-1 mcc compile and run every program in tests/exec, and requires
their stdout and exit status to agree program by program.

The fixpoint gate compares stage2 against stage3, so a self-compile miscompile
that is STABLE survives it -- both stages are built by the same wrong compiler
and agree. selfhost-jit only exercises the JIT. This compares what the two
compilers PRODUCE, which is where a stable -O2/-O3-only miscompile shows.
"""


def flags_for(root, bdir):
    cc = json.load(open(os.path.join(root, bdir, "compile_commands.json")))
    rec = [x for x in cc if x["file"].endswith("/mcc.c")][0]
    return [a for a in shlex.split(rec["command"])[1:]
            if (a.startswith("-D") or a.startswith("-I")) and not a.endswith(".c")]


def build_stage1(root, bdir, mcc, opt, work):
    flags = flags_for(root, bdir)
    blob = os.path.join(root, bdir, "CMakeFiles", "mcc.dir", "mccrt_blob.c.o")
    link_objs, link_flags = [], []
    if os.path.exists(blob):
        link_objs.append(blob)
    else:
        for p in (os.path.join(root, bdir, "libmccrt.a"),
                  os.path.join(root, bdir, "lib", "libmccrt.a")):
            if os.path.exists(p):
                link_flags += ["-B", os.path.dirname(p)]
                break
        else:
            return None
    if any(a.startswith("-DMCC_EMBED_JIT_BLOB") for a in flags):
        jb = os.path.join(root, bdir, "CMakeFiles", "mcc.dir", "mccjit_blob.c.o")
        if not os.path.exists(jb):
            return None
        link_objs.append(jb)
    obj = os.path.join(work, "mcc-s1.o")
    subprocess.run([mcc, *flags, opt, "-c", os.path.join(root, "src/mcc.c"),
                    "-o", obj], cwd=root, check=True)
    s1 = os.path.join(work, "mcc-s1")
    subprocess.run([mcc, *link_flags, obj, *link_objs, "-o", s1, "-lm", "-ldl"],
                   cwd=root, check=True)
    return s1, link_flags


def run_one(cc, extra, src, out, root):
    r = subprocess.run([cc, *extra, "-O1", src, "-o", out],
                       cwd=root, capture_output=True)
    if r.returncode != 0:
        return None
    p = subprocess.run([out], capture_output=True, text=True, timeout=120)
    return (p.returncode, p.stdout)


def main():
    if len(sys.argv) < 2:
        sys.exit(USAGE)
    bdir = sys.argv[1]
    opt = "-O2"
    limit = 0
    for a in sys.argv[2:]:
        if a.startswith("--opt="):
            opt = a.split("=", 1)[1]
        elif a.startswith("--limit="):
            limit = int(a.split("=", 1)[1])
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    mcc = os.path.join(root, bdir, "mcc")
    if not os.access(mcc, os.X_OK):
        print(f"SKIP: no mcc at {mcc}")
        sys.exit(77)

    with tempfile.TemporaryDirectory() as work:
        built = build_stage1(root, bdir, mcc, opt, work)
        if built is None:
            print("SKIP: no runtime blob or sidecar libmccrt.a to link a stage 1")
            sys.exit(77)
        s1, link_flags = built
        inc = ["-I" + os.path.join(root, "runtime/include")]
        extra0 = ["-B", os.path.join(root, bdir)]
        extra1 = link_flags + inc

        srcs = []
        for dirpath, _, names in os.walk(os.path.join(root, "tests/exec")):
            for n in sorted(names):
                if n.endswith(".c") and n != "runner.c":
                    srcs.append(os.path.join(dirpath, n))
        srcs.sort()
        if limit:
            srcs = srcs[:limit]

        checked = skipped = 0
        bad = []
        for i, src in enumerate(srcs):
            a = run_one(mcc, extra0, src, os.path.join(work, f"a{i}"), root)
            if a is None:
                skipped += 1
                continue
            b = run_one(s1, extra1, src, os.path.join(work, f"b{i}"), root)
            if b is None:
                bad.append((src, "stage-1 could not build it; stage 0 could"))
                continue
            checked += 1
            if a != b:
                bad.append((src, f"stage0={a!r} stage1={b!r}"))

        print(f"selfhost-output-parity: {opt} checked={checked} "
              f"skipped={skipped} mismatches={len(bad)}")
        for src, why in bad[:10]:
            print(f"  MISMATCH {os.path.relpath(src, root)}: {why}")
        if checked == 0:
            print("FAIL: nothing was checked; the comparison is vacuous")
            sys.exit(1)
        if bad:
            sys.exit(1)
    print("selfhost-output-parity: OK")


if __name__ == "__main__":
    main()
