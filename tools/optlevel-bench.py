#!/usr/bin/env python3
"""optlevel-bench.py -- rank the -O ladder's level-assignable flags by
emitted-code gain per unit of compile-time cost, in instructions retired.

WHY INSTRUCTIONS AND NOT WALL-CLOCK. Both sides of the ratio are counted with
`perf stat -e instructions:u`. Wall-clock on a shared box made every earlier
per-flag decision irreproducible; instructions retired are a property of the
code, not of the host. Measured here: the compiler self-compile reproduces to
5e-5 across runs, and a kernel binary to ~1e-4. That is three orders of
magnitude below the effects being ranked, which is what makes a ranking of 48
flags meaningful at all.

WHAT IS MEASURED, per flag F, against the shipped default set with F toggled
off (`-fno-F`) -- never against an empty flag set, because a flag's marginal
cost is only meaningful in the configuration it ships in:

  cost    instructions retired by the COMPILER, over two corpora:
            self    the amalgamated self-compile of src/mcc.c (one big TU)
            corpus  every tests/exec .c compiled -c (293 that build)
          cost > 0 means the flag makes the compiler do more work.

  gain    instructions retired by the EMITTED PROGRAM, over the kernel set
          below. Every kernel's output is checked against a reference compiler
          on every configuration, so a fast-but-wrong result fails instead of
          scoring. Reported as a geometric mean over kernels, so a kernel is
          weighted by its ratio and not by how long it happens to run.

  size    .text bytes of the emitted objects (kernels, and the self-compile
          object). Some passes pay in size rather than speed, and -Os exists.

  fires   how many of the 293 corpus objects, and which kernels, change byte
          content when the flag is toggled. This is the honesty axis: a flag
          that changes zero bytes anywhere has no gain to divide by a cost,
          and a gain/cost sort would happily rank it on noise/noise.

EFFICIENCY is gain% / cost%, and it is only computed for flags that cleared
both noise floors. The floors are not guessed: the base configuration is
measured TWICE (the `__noise__` pseudo-flag) and the observed self-difference
is reported alongside every threshold.

Usage:
  tools/optlevel-bench.py --mcc PATH [--cc PATH] [--jobs N] [--reads N]
                          [--base-level 3] [--out tests/optfire/levelbench.tsv]
                          [--json PATH] [--only a,b,c] [--kernels-only]

Exit status: 0 all good, 1 an output mismatch or a build failure, 77 skip (no
perf counter, or no reference compiler).
"""
import argparse, concurrent.futures, hashlib, json, os, re, shutil
import subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RT = os.path.join(ROOT, "tests", "runtime")
EXEC = os.path.join(ROOT, "tests", "exec")

KERNELS = [
    ("sieve",     "sieve.c",      ["1000000", "100"], []),
    ("nbody",     "nbody.c",      ["600000"],         []),
    ("interp",    "interp.c",     ["5000000"],        []),
    ("structops", "structops.c",  ["3000"],           []),
    ("calls",     "calls.c",      ["10000000"],       []),
    ("divmod",    "divmod.c",     ["40000000"],       []),
    ("branchy",   "branchy.c",    ["600"],            []),
    ("hashmap",   "hashmap.c",    ["5000000"],        []),
    ("loopnest",  "loopnest.c",   ["16"],             []),
    ("vlaloop",   "vlaloop.c",    ["300000"],         []),
    ("regpress",  "regpress.c",   ["300000"],         []),
    ("poly",      "poly.c",       ["15000000"],       []),
    ("narrowops", "narrowops.c",  ["1300"],           []),
    ("mathfun",   "mathfun.c",    ["8000000"],        []),
    ("strproc",   "strproc.c",    ["700"],            []),
    ("mandelbrot", "mandelbrot.c", ["1400"],          []),
    ("matmul",    "matmul.c",     ["400", "3"],       []),
]

SELF_INCLUDES = ["src", "src/formats", "src/objfmt", "src/arch/i386",
                 "src/arch/x86_64", "include"]

COST_NOISE = 0.02
GAIN_NOISE = 0.10
TEXT_NOISE = 0


def flag_table(path):
    """(name, level) for every row in mccopt.h that a level can be assigned to.

    ALWAYS/OFF/SPECIAL rows are not on the ladder: ALWAYS is not level-gated,
    OFF is the dump-* and opt-search-* diagnostics, SPECIAL computes its own
    default. Levels above 3 are the in-development rungs and are swept
    separately."""
    rows = []
    txt = open(path).read()
    pat = r'MCC_OPT_ROW\(\s*\w+,\s*"([^"]+)",\s*MCC_OPTD_LEVEL\((\d+)\)\s*\)'
    for m in re.finditer(pat, txt):
        lvl = int(m.group(2))
        if lvl <= 3:
            rows.append((m.group(1), lvl))
    return rows


def cold_cache_env(cachedir):
    """A private, empty XDG_CACHE_HOME for one measurement.

    opt-slice memoises to ~/.cache/mcc/sl-*.ck and the memo is salted by the
    target, not by what was compiled, so a shared cache makes the emitted
    object depend on what else has been compiled recently -- and under
    --jobs>1 it makes it depend on what the OTHER workers are doing right now.
    That turns a fire count into a coin flip. Every measurement gets its own
    empty one, so every configuration sees the same cold cache."""
    env = dict(os.environ)
    env["XDG_CACHE_HOME"] = cachedir
    shutil.rmtree(cachedir, ignore_errors=True)
    os.makedirs(cachedir, exist_ok=True)
    return env


def perf_insns(argv, env=None):
    p = subprocess.run(["perf", "stat", "-e", "instructions:u", "-x,"] + argv,
                       capture_output=True, text=True, env=env)
    for line in p.stderr.splitlines():
        f = line.split(",")
        if len(f) > 2 and f[2].startswith("instructions"):
            try:
                return int(f[0]), p.returncode
            except ValueError:
                return None, p.returncode
    return None, p.returncode


def perf_insns_min(argv, reads, env=None, cachedir=None):
    """Minimum of `reads` counts. The counter is architectural but a run that
    first-touches more pages charges the fault work to the same counter, so the
    minimum is the reproducible end -- the same rule tools/runtime-bench.py
    settled on after nsieve measured 1.4% apart from one binary."""
    vals, rc = [], 0
    for i in range(reads):
        if cachedir:
            env = cold_cache_env(os.path.join(cachedir, "xdg%d" % i))
        v, rc = perf_insns(argv, env)
        if rc != 0:
            return None, rc
        if v:
            vals.append(v)
    return (min(vals) if vals else None), rc


def text_bytes(path):
    """.text size of an ELF object or executable, or None."""
    p = subprocess.run(["readelf", "-S", "-W", path], capture_output=True,
                       text=True)
    if p.returncode != 0:
        return None
    for line in p.stdout.splitlines():
        f = line.replace("[", " ").replace("]", " ").split()
        if len(f) > 6 and f[1] == ".text" and f[2] == "PROGBITS":
            try:
                return int(f[5], 16)
            except ValueError:
                return None
    return None


def sha(path):
    try:
        with open(path, "rb") as fh:
            return hashlib.sha1(fh.read()).hexdigest()
    except OSError:
        return None


def self_compile_argv(mcc, obj, opt, flags, builddir):
    argv = [mcc, opt] + flags + ["-c", os.path.join(ROOT, "src", "mcc.c"),
                                 "-o", obj, "-I" + builddir, "-I" + ROOT]
    argv += ["-I" + os.path.join(ROOT, d) for d in SELF_INCLUDES]
    argv += ["-B" + ROOT, "-B" + builddir]
    return argv


def corpus_sources():
    out = []
    for dirpath, _, names in os.walk(EXEC):
        for n in sorted(names):
            if n.endswith(".c"):
                out.append(os.path.join(dirpath, n))
    return sorted(out)


def corpus_script(mcc, srcs, outdir, opt, flags):
    """A shell loop, perf-stat'ed once, rather than 293 perf invocations.

    perf's own startup would be 293 x ~10ms of wall and, worse, 293 samples of
    a fixed overhead added to what is supposed to be the compiler's number. The
    shell's fork/exec work is charged to the count too, but it is the same work
    in every configuration and therefore cancels in the delta, which is the
    only quantity read out of this."""
    lines = ["#!/bin/sh"]
    q = " ".join('"%s"' % f for f in flags)
    for s in srcs:
        name = os.path.relpath(s, EXEC).replace("/", "_")[:-2]
        lines.append('"%s" %s %s -c "%s" -o "%s/%s.o" -I"%s" >/dev/null 2>&1 || :'
                     % (mcc, opt, q, s, outdir, name, EXEC))
    return "\n".join(lines) + "\n"


def measure(mcc, cc, label, flags, opt, srcs, refs, reads, builddir, keep=None):
    """Everything measurable about one configuration of the flag set."""
    res = {"label": label, "flags": flags, "kernels": {}}
    td = tempfile.mkdtemp(prefix="optlevel-")
    try:
        obj = os.path.join(td, "mcc.o")
        argv = self_compile_argv(mcc, obj, opt, flags, builddir)
        ins, rc = perf_insns_min(argv, reads, cachedir=os.path.join(td, "c1"))
        if rc != 0 or ins is None:
            res["error"] = "self-compile failed (rc=%d)" % rc
            return res
        res["self_insns"] = ins
        res["self_text"] = text_bytes(obj)
        res["self_sha"] = sha(obj)

        cdir = os.path.join(td, "corpus")
        os.makedirs(cdir)
        script = os.path.join(td, "corpus.sh")
        with open(script, "w") as fh:
            fh.write(corpus_script(mcc, srcs, cdir, opt, flags))
        ins, rc = perf_insns_min(["sh", script], reads,
                                 cachedir=os.path.join(td, "c2"))
        res["corpus_insns"] = ins
        objs = sorted(os.listdir(cdir))
        res["corpus_n"] = len(objs)
        res["corpus_text"] = sum(text_bytes(os.path.join(cdir, o)) or 0
                                 for o in objs)
        res["corpus_sha"] = {o: sha(os.path.join(cdir, o)) for o in objs}

        for name, src, kargv, kflags in KERNELS:
            path = os.path.join(RT, src)
            if not os.path.exists(path):
                continue
            exe = os.path.join(td, name)
            kobj = os.path.join(td, name + ".o")
            kenv = cold_cache_env(os.path.join(td, "ck-" + name))
            build = [mcc, opt] + flags + kflags + [path, "-o", exe, "-lm"]
            p = subprocess.run(build, capture_output=True, text=True, env=kenv)
            if p.returncode != 0 or not os.path.exists(exe):
                res["kernels"][name] = {"error": "build failed: "
                                        + p.stderr.strip()[:200]}
                continue
            kenv = cold_cache_env(os.path.join(td, "cko-" + name))
            subprocess.run([mcc, opt] + flags + kflags + ["-c", path, "-o", kobj],
                           capture_output=True, text=True, env=kenv)
            run = subprocess.run([exe] + kargv, capture_output=True, text=True)
            got = run.stdout.strip()
            if run.returncode != 0 or got != refs[name]:
                res["kernels"][name] = {"error": "output mismatch",
                                        "want": refs[name][:200],
                                        "got": got[:200]}
                continue
            ins, rc = perf_insns_min([exe] + kargv, reads)
            res["kernels"][name] = {"insns": ins, "text": text_bytes(kobj),
                                    "sha": sha(kobj)}
        return res
    finally:
        shutil.rmtree(td, ignore_errors=True)


def perf_pair(argv, reads):
    """Instructions AND cycles for one binary, minimum over `reads` runs.

    Both are needed because on this compiler they routinely disagree in SIGN.
    divmagic is the clean case: it replaces one `idiv` with a multiply-shift
    sequence, so a division-heavy loop retires far MORE instructions and takes
    far FEWER cycles. Ranking such a flag on instructions alone would demote
    the single most valuable integer transform in the table. Cycles are the
    noisier counter -- roughly 1% run to run against 0.003% for instructions --
    which is why this path is serial, interleaved and only ever asked about the
    handful of flags whose two metrics point opposite ways."""
    best = {}
    for _ in range(reads):
        p = subprocess.run(["perf", "stat", "-e", "instructions:u,cycles:u",
                            "-x,"] + argv, capture_output=True, text=True)
        if p.returncode != 0:
            return None
        for line in p.stderr.splitlines():
            f = line.split(",")
            if len(f) > 2 and f[2].split(":")[0] in ("instructions", "cycles"):
                try:
                    v = int(f[0])
                except ValueError:
                    continue
                k = f[2].split(":")[0]
                best[k] = v if k not in best else min(best[k], v)
    return best or None


def cycles_adjudicate(mcc, wanted, refs, opt, reads, base_json):
    """Cycles for the kernels a flag actually moves, on and off, serially.

    Only the kernels whose object CHANGES are measured: a kernel the flag does
    not touch contributes a pair of identical binaries and a pair of noise
    samples, which can only dilute the answer."""
    out = {}
    for flag in wanted:
        row = base_json["rows_by_flag"].get(flag, {})
        movers = row.get("fires_kernels") or []
        if not movers:
            out[flag] = {"error": "changes no kernel object; nothing to adjudicate"}
            continue
        res = {}
        for name in movers:
            entry = [k for k in KERNELS if k[0] == name]
            if not entry:
                continue
            _, src, kargv, kflags = entry[0]
            path = os.path.join(RT, src)
            td = tempfile.mkdtemp(prefix="adjud-")
            try:
                exes = {}
                ok = True
                for label, extra in (("on", []), ("off", ["-fno-" + flag])):
                    exe = os.path.join(td, name + "." + label)
                    env = cold_cache_env(os.path.join(td, "c" + label))
                    p = subprocess.run([mcc, opt] + extra + kflags +
                                       [path, "-o", exe, "-lm"],
                                       capture_output=True, text=True, env=env)
                    if p.returncode != 0:
                        ok = False
                        break
                    r = subprocess.run([exe] + kargv, capture_output=True,
                                       text=True)
                    if r.returncode != 0 or r.stdout.strip() != refs[name]:
                        res[name] = {"error": "output mismatch"}
                        ok = False
                        break
                    exes[label] = exe
                if not ok:
                    continue
                acc = {"on": {}, "off": {}}
                for _ in range(reads):
                    for label in ("on", "off"):
                        got = perf_pair([exes[label]] + kargv, 1)
                        if not got:
                            continue
                        for k, v in got.items():
                            cur = acc[label].get(k)
                            acc[label][k] = v if cur is None else min(cur, v)
                if not acc["on"] or not acc["off"]:
                    continue
                res[name] = {
                    "cycles_on": acc["on"]["cycles"],
                    "cycles_off": acc["off"]["cycles"],
                    "insns_on": acc["on"]["instructions"],
                    "insns_off": acc["off"]["instructions"],
                    "cycle_gain": (acc["off"]["cycles"] - acc["on"]["cycles"])
                    / acc["off"]["cycles"] * 100.0,
                    "insn_gain": (acc["off"]["instructions"]
                                  - acc["on"]["instructions"])
                    / acc["off"]["instructions"] * 100.0,
                }
            finally:
                shutil.rmtree(td, ignore_errors=True)
        out[flag] = res
    return out


def geomean(xs):
    if not xs:
        return None
    acc = 0.0
    for x in xs:
        acc += __import__("math").log(x)
    return __import__("math").exp(acc / len(xs))


def analyse(base, noise, runs, flags):
    """Turn raw counts into the ranking and the three honesty buckets."""
    out = []
    kn = [k for k in base["kernels"] if "insns" in base["kernels"][k]]
    floor = {}
    for k in kn:
        b, n = base["kernels"][k]["insns"], noise["kernels"].get(k, {}).get("insns")
        floor[k] = abs(b - n) / b * 100.0 if n else 0.0
    floor["self"] = abs(base["self_insns"] - noise["self_insns"]) \
        / base["self_insns"] * 100.0
    floor["corpus"] = abs(base["corpus_insns"] - noise["corpus_insns"]) \
        / base["corpus_insns"] * 100.0

    for name, level in flags:
        r = runs.get(name)
        row = {"flag": name, "level": level}
        if r is None or "error" in r:
            row["bucket"] = "error"
            row["note"] = (r or {}).get("error", "not measured")
            out.append(row)
            continue
        row["cost_self"] = (base["self_insns"] - r["self_insns"]) \
            / r["self_insns"] * 100.0
        row["cost_corpus"] = (base["corpus_insns"] - r["corpus_insns"]) \
            / r["corpus_insns"] * 100.0
        row["self_insns_on"] = base["self_insns"]
        row["self_insns_off"] = r["self_insns"]
        row["corpus_insns_on"] = base["corpus_insns"]
        row["corpus_insns_off"] = r["corpus_insns"]

        differing = [o for o, h in base["corpus_sha"].items()
                     if r["corpus_sha"].get(o) != h]
        row["fires_corpus"] = len(differing)
        row["corpus_total"] = base["corpus_n"]
        row["fires_self"] = int(r.get("self_sha") != base.get("self_sha"))

        ratios, moved, kdelta, bad = [], [], {}, []
        text_on = text_off = 0
        for k in kn:
            kr = r["kernels"].get(k, {})
            if "error" in kr:
                bad.append("%s: %s" % (k, kr["error"]))
                continue
            if "insns" not in kr or not kr["insns"]:
                continue
            on, off = base["kernels"][k]["insns"], kr["insns"]
            ratios.append(off / on)
            d = (off - on) / off * 100.0
            kdelta[k] = d
            if kr.get("sha") != base["kernels"][k].get("sha"):
                moved.append(k)
            text_on += base["kernels"][k]["text"] or 0
            text_off += kr["text"] or 0
        row["kernel_delta"] = kdelta
        row["fires_kernels"] = moved
        row["errors"] = bad
        g = geomean(ratios)
        row["gain"] = (g - 1.0) * 100.0 if g else 0.0
        row["text_kernels_on"] = text_on
        row["text_kernels_off"] = text_off
        row["text_delta"] = (text_on - text_off) / text_off * 100.0 if text_off else 0.0
        row["self_text_on"] = base["self_text"]
        row["self_text_off"] = r["self_text"]
        row["self_text_delta"] = (base["self_text"] - r["self_text"]) \
            / r["self_text"] * 100.0 if r["self_text"] else 0.0

        fires = row["fires_corpus"] or row["fires_self"] or moved
        cost = max(row["cost_self"], row["cost_corpus"])
        has_cost = abs(row["cost_self"]) >= max(COST_NOISE, floor["self"]) or \
            abs(row["cost_corpus"]) >= max(COST_NOISE, floor["corpus"])
        has_gain = abs(row["gain"]) >= GAIN_NOISE and \
            any(abs(v) >= 2 * floor.get(k, 0.05) for k, v in kdelta.items())
        row["has_cost"] = has_cost
        row["has_gain"] = has_gain
        if bad:
            row["bucket"] = "error"
        elif not fires:
            row["bucket"] = "inert"
        elif has_gain:
            row["bucket"] = "ranked"
        else:
            row["bucket"] = "cost-no-gain"
        row["efficiency"] = (row["gain"] / cost) if (has_gain and cost > 0.005) \
            else (float("inf") if has_gain else 0.0)
        out.append(row)
    return out, floor


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mcc", required=True)
    ap.add_argument("--cc", default=None)
    ap.add_argument("--builddir", default=None)
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--reads", type=int, default=3)
    ap.add_argument("--opt", default="-O3")
    ap.add_argument("--only", default=None)
    ap.add_argument("--out", default=os.path.join(ROOT, "tests", "optfire",
                                                  "levelbench.tsv"))
    ap.add_argument("--json", default=None)
    ap.add_argument("--cycles", default=None,
                    help="comma-separated flags to adjudicate in CYCLES as well "
                         "as instructions, serially -- for the rows where the "
                         "two counters disagree in sign")
    ap.add_argument("--cycle-reads", type=int, default=9)
    ap.add_argument("--from-json", default=None,
                    help="reuse a previous run's --json instead of re-measuring; "
                         "only meaningful with --cycles")
    args = ap.parse_args()

    if not shutil.which("perf"):
        print("no perf; skipping")
        return 77
    cc = args.cc
    if not cc:
        for c in ("gcc", "clang", "cc"):
            if shutil.which(c):
                cc = shutil.which(c)
                break
    if not cc:
        print("no reference compiler; skipping")
        return 77
    mcc = os.path.abspath(args.mcc)
    builddir = args.builddir or os.path.dirname(mcc)

    flags = flag_table(os.path.join(ROOT, "src", "mccopt.h"))
    if args.only:
        want = set(args.only.split(","))
        flags = [f for f in flags if f[0] in want]

    refs = {}
    td = tempfile.mkdtemp(prefix="optlevel-ref-")
    try:
        for name, src, kargv, _ in KERNELS:
            path = os.path.join(RT, src)
            if not os.path.exists(path):
                continue
            exe = os.path.join(td, name)
            p = subprocess.run([cc, "-O2", "-w", "-ffp-contract=off", path,
                                "-o", exe, "-lm"], capture_output=True, text=True)
            if p.returncode != 0:
                print("reference build failed for %s: %s" % (name, p.stderr[:200]))
                return 1
            r = subprocess.run([exe] + kargv, capture_output=True, text=True)
            if r.returncode != 0:
                print("reference run failed for %s" % name)
                return 1
            refs[name] = r.stdout.strip()
    finally:
        shutil.rmtree(td, ignore_errors=True)

    if args.cycles and args.from_json:
        prev = json.load(open(args.from_json))
        prev["rows_by_flag"] = {r["flag"]: r for r in prev["rows"]}
        adj = cycles_adjudicate(mcc, args.cycles.split(","), refs, args.opt,
                                args.cycle_reads, prev)
        print("# cycles adjudication (%d reads, serial, minimum)"
              % args.cycle_reads)
        print("%-24s %-11s %10s %10s %8s %8s"
              % ("flag", "kernel", "cycles_on", "cycles_off", "cyc%", "insn%"))
        for flag, res in adj.items():
            if "error" in res:
                print("%-24s %s" % (flag, res["error"]))
                continue
            for k, v in sorted(res.items()):
                if "error" in v:
                    print("%-24s %-11s %s" % (flag, k, v["error"]))
                    continue
                print("%-24s %-11s %10.4g %10.4g %+8.3f %+8.3f"
                      % (flag, k, v["cycles_on"], v["cycles_off"],
                         v["cycle_gain"], v["insn_gain"]))
        if args.json:
            with open(args.json, "w") as fh:
                json.dump(adj, fh, indent=1, sort_keys=True)
            print("wrote %s" % args.json)
        return 0

    srcs = corpus_sources()
    jobs = [("__base__", []), ("__noise__", [])]
    jobs += [(n, ["-fno-" + n]) for n, _ in flags]

    results = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        futs = {ex.submit(measure, mcc, cc, lbl, fl, args.opt, srcs, refs,
                          args.reads, builddir): lbl for lbl, fl in jobs}
        done = 0
        for f in concurrent.futures.as_completed(futs):
            lbl = futs[f]
            results[lbl] = f.result()
            done += 1
            print("[%d/%d] %s" % (done, len(futs), lbl), file=sys.stderr)

    base, noise = results["__base__"], results["__noise__"]
    if "error" in base:
        print("base configuration failed: %s" % base["error"])
        return 1
    rows, floor = analyse(base, noise, results, flags)

    rows_sorted = sorted(rows, key=lambda r: (-{"ranked": 3, "cost-no-gain": 2,
                                                "inert": 1, "error": 0}[r["bucket"]],
                                              -r.get("efficiency", 0),
                                              -r.get("gain", 0)))
    hdr = ["flag", "level", "bucket", "gain_pct", "cost_self_pct",
           "cost_corpus_pct", "efficiency", "text_kernels_pct", "text_self_pct",
           "fires_corpus", "corpus_total", "fires_kernels", "best_kernel",
           "best_kernel_pct"]
    with open(args.out, "w") as fh:
        fh.write("# optlevel-bench: emitted-code gain per unit of compile cost, "
                 "instructions retired\n")
        fh.write("# base=%s opt=%s kernels=%d corpus=%d\n"
                 % (os.path.basename(mcc), args.opt, len(base["kernels"]),
                    base["corpus_n"]))
        fh.write("# noise floor (base measured twice): self %.4f%%  corpus %.4f%%"
                 "  kernels max %.4f%%\n"
                 % (floor["self"], floor["corpus"],
                    max(v for k, v in floor.items() if k not in ("self", "corpus"))))
        fh.write("# gain>0 means the flag makes the emitted program retire fewer "
                 "instructions; cost>0 means it makes the compiler retire more\n")
        fh.write("\t".join(hdr) + "\n")
        for r in rows_sorted:
            kd = r.get("kernel_delta", {})
            bk = max(kd, key=lambda k: abs(kd[k])) if kd else ""
            fh.write("\t".join([
                r["flag"], str(r["level"]), r["bucket"],
                "%.4f" % r.get("gain", 0.0),
                "%.4f" % r.get("cost_self", 0.0),
                "%.4f" % r.get("cost_corpus", 0.0),
                ("inf" if r.get("efficiency") == float("inf")
                 else "%.2f" % r.get("efficiency", 0.0)),
                "%.4f" % r.get("text_delta", 0.0),
                "%.4f" % r.get("self_text_delta", 0.0),
                str(r.get("fires_corpus", 0)), str(r.get("corpus_total", 0)),
                ",".join(r.get("fires_kernels", [])) or "-",
                bk or "-", "%.4f" % (kd[bk] if bk else 0.0)]) + "\n")
    print("wrote %s" % args.out)

    if args.json:
        with open(args.json, "w") as fh:
            json.dump({"base": base, "noise": noise, "rows": rows,
                       "floor": floor}, fh, indent=1, sort_keys=True)
        print("wrote %s" % args.json)

    errs = [r for r in rows if r["bucket"] == "error"]
    for r in errs:
        print("EXCLUDED %s: %s" % (r["flag"], r.get("note") or r.get("errors")))
    if errs:
        print("%d configuration(s) excluded from the ranking: a flag set that "
              "will not build or whose output does not match the reference is "
              "removed from the table, never ranked poorly. This is a report, "
              "not a failure -- the gate that fails on a broken compiler is "
              "ctest, not the benchmark that found it." % len(errs))
    return 0


if __name__ == "__main__":
    sys.exit(main())
