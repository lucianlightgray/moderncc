#!/usr/bin/env python3
"""Cross-oracle conformance measurement for mcc's *runtime JIT*.

The static path has been measured for a long time; the JIT has not. This runs
external C corpora through the JIT and scores every program against the *other*
vendor's compiler, exactly as tools/xoracle.py does for the static path:

  gcc's gcc.c-torture/execute      -> judged by clang   (--gcc)
  llvm-test-suite SingleSource     -> judged by gcc     (--testsuite)
  llvm's compiler-rt builtins Unit -> judged by gcc     (--llvm)

clang's own test tree is lit/FileCheck over IR and is not an execution corpus;
llvm-test-suite is where the runnable clang-side programs live.

Two JIT surfaces exist in this tree and both are driven here:

  embed   `mcc -O2 --embed-jit prog.c -o prog` bakes the runtime JIT engine
          into the output; the produced program honours MCC_JIT=0/1 at its own
          runtime (src/mcc.c help text, `--embed-jit, --no-embed-jit`). Every
          program is then run *twice from the same binary*, MCC_JIT=1 and
          MCC_JIT=0, which isolates a JIT defect from a static-path defect: if
          the two disagree, the codegen the JIT installed is wrong and nothing
          about the AOT compiler is implicated.

  run     `mcc -run --jit prog.c` drives the in-process JIT of the -run path.

A program is only counted as JIT coverage when the engine is observed to boot
inside it -- MCC_JIT_VERBOSE=1 makes the engine print `mccjit-boot[...]` or
`mccjit-lazy[install]`, and a run without that line is recorded as NOT_BAKED
(no engine in the output at all) or NO_ENGINE (engine present, never booted)
rather than as a pass. A harness that counts programs the JIT never touched is
the exact failure this cell exists to prevent.

Usage:
  tools/jitconform.py --phase qualify --out DIR --gcc ~/Projects/gcc \
                      --llvm ~/Projects/llvm-project
  tools/jitconform.py --phase check --out DIR --mcc cmake-debug/mcc \
                      --surface embed [--opt=-O2] [--min-pass N]

`--opt` must be attached with `=`, for the argparse reason xsuite.py documents.
"""
import argparse, json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xsuite
import xoracle

SKIP = 77

ENGINE_RE = re.compile(rb"mccjit-(?:boot|lazy)\[")
OUTCOME_RE = re.compile(rb"(kept-aot|swapped|refused|over-budget|budget-skip)")
ENGINE_MARK = b"mccjit-boot"

JIT_ON = {"MCC_JIT": "1", "MCC_JIT_VERBOSE": "1"}
JIT_OFF = {"MCC_JIT": "0"}


def run_prog(argv, work, env, rtimeout):
    e = dict(os.environ)
    e.update(env)
    try:
        p = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           timeout=rtimeout, preexec_fn=xsuite.limits_run,
                           cwd=work, env=e)
    except subprocess.TimeoutExpired:
        return None, b"", b""
    except OSError:
        return None, b"", b""
    return p.returncode, (p.stdout or b""), (p.stderr or b"")


def bake(ph, mcc, o, idx, opt, work, dst):
    inc = ["-I" + os.path.dirname(o["file"])] + ["-I" + d for d in o.get("inc", [])]
    cmd = ([mcc, opt, "--embed-jit"] + o["flags"] + inc + [o["file"]]
           + list(o.get("extra", [])) + ["-o", dst, "-lm"])
    try:
        p = subprocess.run(cmd, capture_output=True, timeout=ph.ctimeout,
                           preexec_fn=xsuite.limits_compile, cwd=work)
    except subprocess.TimeoutExpired:
        return None, "timeout", False
    except OSError as ex:
        return None, str(ex), False
    err = (p.stderr or b"").decode("utf-8", "replace")
    if p.returncode != 0:
        if xsuite.REF_BADFLAG_RE.search(err):
            return None, "badflag", False
        return None, xsuite.err_key(err), False
    try:
        with open(dst, "rb") as f:
            baked = ENGINE_MARK in f.read()
    except OSError:
        return None, "no output", False
    return dst, "", baked


def check_embed(ph, o, idx, mcc, opt, lazy):
    rec = {"suite": o["suite"], "file": o["file"], "oracle": o["oracle"],
           "opt": opt, "surface": "embed"}
    work = ph.workdir(idx)
    dst = os.path.join(work, "j%d.x" % idx)
    b, why, baked = bake(ph, mcc, o, idx, opt, work, dst)
    if not b:
        xoracle.unlink(dst)
        rec.update(status="LINK_POLICY" if "defined twice" in why else "MCC_NOBUILD",
                   why=why)
        return ph.emit(rec)
    rec["baked"] = baked
    env_on = dict(JIT_ON)
    if lazy:
        env_on["MCC_JIT_LAZY"] = "1"
        env_on["MCC_JIT_HOT_CALLS"] = "1"
    rc1, out1, err1 = run_prog([dst], work, env_on, ph.rtimeout)
    if rc1 is None:
        xoracle.unlink(dst)
        rec["status"] = "MCC_TIMEOUT"
        return ph.emit(rec)
    engaged = bool(ENGINE_RE.search(err1))
    m = OUTCOME_RE.search(err1)
    rec["engaged"] = engaged
    if m:
        rec["outcome"] = m.group(1).decode()
    rc0, out0, _ = run_prog([dst], work, JIT_OFF, ph.rtimeout)
    xoracle.unlink(dst)
    if rc0 is not None and (rc1, out1) != (rc0, out0):
        rec.update(status="JIT_MISCOMPILE", jit_exit=rc1, aot_exit=rc0,
                   jit_sha=xoracle.digest(out1), aot_sha=xoracle.digest(out0),
                   jit_head=xoracle.head(out1), aot_head=xoracle.head(out0))
        return ph.emit(rec)
    rec["aot_agrees"] = rc0 is not None and rc0 == o["exit"] \
        and xoracle.digest(out0) == o["stdout_sha"]
    if rc1 != o["exit"]:
        rec.update(status="DIFF_EXIT", got_exit=rc1, want_exit=o["exit"],
                   got_head=xoracle.head(out1), want_head=o["stdout_head"])
        return ph.emit(rec)
    if xoracle.digest(out1) != o["stdout_sha"]:
        rec.update(status="DIFF_STDOUT", got_sha=xoracle.digest(out1),
                   want_sha=o["stdout_sha"], got_head=xoracle.head(out1),
                   want_head=o["stdout_head"])
        return ph.emit(rec)
    if engaged:
        rec["status"] = "PASS"
    else:
        rec["status"] = "NOT_BAKED" if not baked else "NO_ENGINE"
    return ph.emit(rec)


def check_run(ph, o, idx, mcc, opt, lazy):
    rec = {"suite": o["suite"], "file": o["file"], "oracle": o["oracle"],
           "opt": opt, "surface": "run"}
    work = ph.workdir(idx)
    inc = ["-I" + os.path.dirname(o["file"])] + ["-I" + d for d in o.get("inc", [])]
    argv = ([mcc, opt, "--jit"] + o["flags"] + inc + list(o.get("extra", []))
            + ["-lm", "-run", o["file"]])
    env_on = dict(JIT_ON)
    if lazy:
        env_on["MCC_JIT_LAZY"] = "1"
        env_on["MCC_JIT_HOT_CALLS"] = "1"
    rc1, out1, err1 = run_prog(argv, work, env_on, ph.rtimeout + ph.ctimeout)
    if rc1 is None:
        rec["status"] = "MCC_TIMEOUT"
        return ph.emit(rec)
    engaged = bool(ENGINE_RE.search(err1))
    m = OUTCOME_RE.search(err1)
    rec["engaged"] = engaged
    if m:
        rec["outcome"] = m.group(1).decode()
    derr = err1.decode("utf-8", "replace")
    if rc1 != o["exit"] and xsuite.REF_BADFLAG_RE.search(derr):
        rec.update(status="MCC_NOBUILD", why="badflag")
        return ph.emit(rec)
    if rc1 != o["exit"] and ("error:" in derr or "not supported" in derr):
        rec.update(status="MCC_NOBUILD", why=xsuite.err_key(derr))
        return ph.emit(rec)
    argv0 = ([mcc, opt, "--no-jit"] + o["flags"] + inc + list(o.get("extra", []))
             + ["-lm", "-run", o["file"]])
    rc0, out0, _ = run_prog(argv0, work, {"MCC_JIT": "0"},
                            ph.rtimeout + ph.ctimeout)
    if rc0 is not None and (rc1, out1) != (rc0, out0):
        rec.update(status="JIT_MISCOMPILE", jit_exit=rc1, aot_exit=rc0,
                   jit_sha=xoracle.digest(out1), aot_sha=xoracle.digest(out0),
                   jit_head=xoracle.head(out1), aot_head=xoracle.head(out0))
        return ph.emit(rec)
    if rc1 != o["exit"]:
        rec.update(status="DIFF_EXIT", got_exit=rc1, want_exit=o["exit"],
                   got_head=xoracle.head(out1), want_head=o["stdout_head"])
        return ph.emit(rec)
    if xoracle.digest(out1) != o["stdout_sha"]:
        rec.update(status="DIFF_STDOUT", got_sha=xoracle.digest(out1),
                   want_sha=o["stdout_sha"], got_head=xoracle.head(out1),
                   want_head=o["stdout_head"])
        return ph.emit(rec)
    rec["status"] = "PASS" if engaged else "NOT_BAKED"
    return ph.emit(rec)


def verdict_of(status):
    if status == "PASS":
        return "AGREE"
    if status in ("JIT_MISCOMPILE", "DIFF_EXIT", "DIFF_STDOUT"):
        return "DIFFER"
    if status in ("MCC_NOBUILD", "LINK_POLICY", "MCC_TIMEOUT"):
        return "MCC-REJECTED"
    return "UNSUPPORTED"


def summarize(recs, surface, opt, out, label):
    tally = {}
    for r in recs:
        tally[r["status"]] = tally.get(r["status"], 0) + 1
    verd = {}
    for r in recs:
        v = verdict_of(r["status"])
        verd[v] = verd.get(v, 0) + 1
    outcomes = {}
    for r in recs:
        if r.get("outcome"):
            outcomes[r["outcome"]] = outcomes.get(r["outcome"], 0) + 1
    why = {}
    for r in recs:
        if r["status"] in ("MCC_NOBUILD", "LINK_POLICY") and r.get("why"):
            why[r["why"]] = why.get(r["why"], 0) + 1
    suites = {}
    for r in recs:
        s = suites.setdefault(r["suite"], {"n": 0, "pass": 0})
        s["n"] += 1
        if r["status"] == "PASS":
            s["pass"] += 1
    total = len(recs) or 1
    print(f"\njit conformance: surface={surface} {opt} label={label}")
    print("-" * 62)
    for k in sorted(tally, key=lambda k: -tally[k]):
        print(f"  {k:<22}{tally[k]:>8}{100.0 * tally[k] / total:>8.2f}%")
    print("-" * 62)
    print(f"  {'total':<22}{len(recs):>8}")
    print("\n  verdicts")
    for k in sorted(verd, key=lambda k: -verd[k]):
        print(f"  {k:<22}{verd[k]:>8}{100.0 * verd[k] / total:>8.2f}%")
    if outcomes:
        print("\n  engine routing outcome (MCC_JIT_VERBOSE)")
        for k in sorted(outcomes, key=lambda k: -outcomes[k]):
            print(f"  {k:<22}{outcomes[k]:>8}")
    if why:
        print("\n  mcc refused to build (a language/builtin gap, not a JIT verdict)")
        for k in sorted(why, key=lambda k: -why[k])[:20]:
            print(f"  {why[k]:>6}  {k}")
    print("\n  per suite")
    for k in sorted(suites):
        s = suites[k]
        print(f"  {k:<30}{s['pass']:>6} / {s['n']:<6}")
    summary = {"surface": surface, "opt": opt, "label": label, "total": len(recs),
               "status": tally, "verdict": verd, "outcome": outcomes,
               "refusal": why, "suites": suites}
    with open(os.path.join(out, "summary-%s.json" % label), "w") as f:
        json.dump(summary, f, indent=1, sort_keys=True)
    return summary


TESTSUITE_DIRS = (("SingleSource/Regression/C", "llvm:ts-regression"),
                  ("SingleSource/UnitTests", "llvm:ts-unittests"))


def collect_testsuite(root):
    """Collect llvm-test-suite SingleSource programs as gcc-judged run tests.

    These are the executable clang-side corpus that clang's own suite is not:
    single-file, self-checking C with a `.reference_output` beside it. The
    reference file is deliberately ignored -- the cross-oracle rule is that
    gcc adjudicates the llvm corpus, never llvm's own recorded expectations.

    `-std=gnu89` is not a thumb on the scale: the corpus is pre-C99 K&R-era C
    and SingleSource/Regression/C/CMakeLists.txt passes `-Wno-implicit-int` for
    exactly this reason. Under a C23-default gcc, 640 of these fail to build at
    all on implicit-int and implicit-declaration, which would classify them out
    as untestable rather than measure anything. The oracle and mcc get the
    identical flag, so the comparison is unaffected.
    """
    out = []
    for sub, name in TESTSUITE_DIRS:
        base = os.path.join(root, sub)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, files in os.walk(base):
            dirnames[:] = [d for d in dirnames if d not in ("Inputs", "CMakeFiles")]
            inc, d = [], dirpath
            while len(d) >= len(base):
                inc.append(d)
                nd = os.path.dirname(d)
                if nd == d:
                    break
                d = nd
            for f in sorted(files):
                if not f.endswith(".c"):
                    continue
                out.append({"suite": name, "file": os.path.join(dirpath, f),
                            "mode": "run", "expect": "ok",
                            "flags": ["-w", "-std=gnu89", "-fcommon"],
                            "extra": [], "inc": list(inc)})
    return out


def load_oracle(out, suite, limit):
    src = os.path.join(out, "oracle.jsonl")
    if not os.path.exists(src):
        return None
    oracle = [json.loads(l) for l in open(src)]
    oracle = [o for o in oracle if o["status"] == "OK"]
    oracle.sort(key=lambda o: (o["suite"], o["file"]))
    if suite:
        oracle = [o for o in oracle if any(x in o["suite"] for x in suite)]
    if limit:
        seen, kept = {}, []
        for o in oracle:
            n = seen.get(o["suite"], 0)
            if n < limit:
                seen[o["suite"]] = n + 1
                kept.append(o)
        oracle = kept
    return oracle


def do_qualify(args):
    if not (args.gcc or args.llvm or args.testsuite):
        print("jitconform: SKIP (no --gcc, --llvm or --testsuite corpus)")
        return SKIP
    if args.gcc and not os.path.isdir(os.path.join(args.gcc, "gcc", "testsuite")):
        print(f"jitconform: SKIP (no gcc testsuite under {args.gcc})")
        return SKIP
    xsuite.ANSI_OK = True
    tests = xoracle.load_tests(args) if (args.gcc or args.llvm) else []
    if args.testsuite:
        ts = collect_testsuite(args.testsuite)
        if args.suite:
            ts = [t for t in ts if any(x in t["suite"] for x in args.suite)]
        tests += ts[:args.limit] if args.limit else ts
    if not tests:
        print("jitconform: SKIP (corpus collected zero run-mode tests)")
        return SKIP
    print(f"jitconform: qualifying {len(tests)} run-mode tests "
          f"(gcc -> {args.clang_bin}, llvm -> {args.gcc_bin}), jobs={args.jobs}",
          flush=True)
    ph = xoracle.Phase(args.out, args.jobs, args.ctimeout, args.rtimeout,
                       "oracle.jsonl")
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        list(ex.map(lambda a: xoracle.qualify_one(ph, a[1], a[0], args.gcc_bin,
                                                  args.clang_bin),
                    list(enumerate(tests))))
    ph.close()
    xoracle.report(ph.tally, ph.n, "oracle qualification")
    ok = ph.tally.get("OK", 0)
    classified_out = ph.n - ok
    print(f"\njitconform: {ok} programs entered the oracle set; "
          f"{classified_out} classified out "
          f"(oracle could not build, timed out, or the program is "
          f"nondeterministic / UB-sensitive)")
    with open(os.path.join(args.out, "qualify.json"), "w") as f:
        json.dump({"total": ph.n, "oracle_set": ok,
                   "classified_out": classified_out, "status": ph.tally}, f,
                  indent=1, sort_keys=True)
    return 0


def do_check(args):
    oracle = load_oracle(args.out, args.suite, args.limit)
    if oracle is None:
        print(f"jitconform: SKIP (no oracle set at "
              f"{os.path.join(args.out, 'oracle.jsonl')}; run --phase qualify)")
        return SKIP
    if not oracle:
        print("jitconform: SKIP (oracle set is empty)")
        return SKIP
    mcc = os.path.abspath(args.mcc)
    print(f"jitconform: {len(oracle)} oracle programs through the "
          f"{args.surface} JIT surface of {mcc} {args.opt} jobs={args.jobs}",
          flush=True)
    ph = xoracle.Phase(args.out, args.jobs, args.ctimeout, args.rtimeout,
                       "jit-%s.jsonl" % args.label)
    fn = check_embed if args.surface == "embed" else check_run
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        list(ex.map(lambda a: fn(ph, a[1], a[0], mcc, args.opt, args.lazy),
                    list(enumerate(oracle))))
    ph.close()
    recs = [json.loads(l)
            for l in open(os.path.join(args.out, "jit-%s.jsonl" % args.label))]
    s = summarize(recs, args.surface, args.opt, args.out, args.label)
    return gate(s, recs, args)


def gate(s, recs, args):
    npass = s["status"].get("PASS", 0)
    differ = s["verdict"].get("DIFFER", 0)
    bad = []
    if npass < args.min_pass:
        bad.append(f"floor: {npass} programs ran correctly under the JIT, "
                   f"below the --min-pass {args.min_pass} floor -- an empty or "
                   f"vacuous corpus must fail here, not pass quietly")
    if args.max_differ >= 0 and differ > args.max_differ:
        bad.append(f"regression: {differ} programs DIFFER from the cross oracle, "
                   f"above the --max-differ {args.max_differ} pin")
    mis = [r for r in recs if r["status"] == "JIT_MISCOMPILE"]
    if args.max_miscompile >= 0 and len(mis) > args.max_miscompile:
        bad.append(f"miscompile: {len(mis)} programs where MCC_JIT=1 and "
                   f"MCC_JIT=0 disagree in the same binary, above the "
                   f"--max-miscompile {args.max_miscompile} pin")
    for r in mis[:10]:
        print(f"  JIT_MISCOMPILE {r['file']} jit_exit={r.get('jit_exit')} "
              f"aot_exit={r.get('aot_exit')}")
    if bad:
        for b in bad:
            print("jitconform: FAIL " + b)
        return 1
    print(f"\njitconform: OK ({npass} programs executed correctly under the "
          f"{args.surface} JIT, {differ} differ, "
          f"{s['verdict'].get('MCC-REJECTED', 0)} refused by mcc)")
    return 0


SELFCHECK_HOT = """extern int printf(const char *, ...);
static int fib(int n){ return n < 2 ? n : fib(n-1) + fib(n-2); }
int main(void){ int s = 0; for (int i = 0; i < 30; i++) s += fib(i % 15);
  printf("emb %d\\n", s); return s & 0xff; }
"""

SELFCHECK_COLD = """int main(void){ return 7; }
"""


def selfcheck(args):
    work = os.path.join(args.out, "selfcheck")
    os.makedirs(work, exist_ok=True)
    hot = os.path.join(work, "hot.c")
    cold = os.path.join(work, "cold.c")
    with open(hot, "w") as f:
        f.write(SELFCHECK_HOT)
    with open(cold, "w") as f:
        f.write(SELFCHECK_COLD)
    cases = [
        ("hot-truth", hot, 1972 & 0xff, "emb 1972\n", "PASS"),
        ("hot-lie", hot, (1972 & 0xff) ^ 1, "emb 1972\n", "DIFF_EXIT"),
        ("cold-truth", cold, 7, "", ("PASS", "NOT_BAKED")),
        ("cold-lie", cold, 8, "", "DIFF_EXIT"),
    ]
    oracle = []
    for name, src, exit_, out, _ in cases:
        oracle.append({"suite": "selfcheck:" + name, "file": src, "oracle": "fixed",
                       "flags": [], "inc": [], "extra": [], "status": "OK",
                       "exit": exit_, "stdout_sha": xoracle.digest(out.encode()),
                       "stdout_len": len(out), "stdout_head": out})
    ph = xoracle.Phase(args.out, 1, args.ctimeout, args.rtimeout,
                       "selfcheck.jsonl")
    mcc = os.path.abspath(args.mcc)
    fn = check_embed if args.surface == "embed" else check_run
    for i, o in enumerate(oracle):
        fn(ph, o, i, mcc, args.opt, args.lazy)
    ph.close()
    got = [json.loads(l)
           for l in open(os.path.join(args.out, "selfcheck.jsonl"))]
    got = {r["suite"].split(":", 1)[1]: r["status"] for r in got}
    bad = 0
    for name, _, _, _, want in cases:
        want = (want,) if isinstance(want, str) else want
        have = got.get(name, "MISSING")
        ok = have in want
        print("jitconform selfcheck: %-12s want=%-22s got=%s %s"
              % (name, "|".join(want), have, "OK" if ok else "FAIL"))
        if not ok:
            bad += 1
    if got.get("hot-truth") != "PASS":
        print("jitconform selfcheck: FAIL the hot program did not reach PASS, so "
              "the runtime JIT engine never booted inside it -- every corpus "
              "number this harness reports would be measuring the AOT path")
        bad += 1
    if bad:
        print(f"jitconform selfcheck: FAIL ({bad} case(s))")
        return 1
    print("jitconform selfcheck: OK (a JIT-engaged program passes, and a "
          "falsified oracle expectation is caught as DIFF_EXIT on both a "
          "JIT-engaged and a JIT-declined program)")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--phase", choices=("qualify", "check", "both", "selfcheck"),
                    required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--gcc", default="")
    ap.add_argument("--llvm", default="")
    ap.add_argument("--testsuite", default="")
    ap.add_argument("--gcc-bin", default="gcc")
    ap.add_argument("--clang-bin", default="clang")
    ap.add_argument("--mcc", default="")
    ap.add_argument("--surface", choices=("embed", "run"), default="embed")
    ap.add_argument("--lazy", action="store_true")
    ap.add_argument("--label", default="jit")
    ap.add_argument("--opt", default="-O2")
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--suite", action="append", default=[])
    ap.add_argument("--ctimeout", type=float, default=25.0)
    ap.add_argument("--rtimeout", type=float, default=15.0)
    ap.add_argument("--min-pass", type=int, default=0)
    ap.add_argument("--max-differ", type=int, default=-1)
    ap.add_argument("--max-miscompile", type=int, default=-1)
    args = ap.parse_args()
    args.out = os.path.abspath(args.out)
    os.makedirs(args.out, exist_ok=True)
    if args.phase == "selfcheck":
        if not args.mcc or not os.path.exists(args.mcc):
            print(f"jitconform: SKIP (no mcc at {args.mcc})")
            return SKIP
        return selfcheck(args)
    if args.phase in ("qualify", "both"):
        rc = do_qualify(args)
        if rc != 0 or args.phase == "qualify":
            return rc
    if not args.mcc or not os.path.exists(args.mcc):
        print(f"jitconform: SKIP (no mcc at {args.mcc})")
        return SKIP
    return do_check(args)


if __name__ == "__main__":
    sys.exit(main())
