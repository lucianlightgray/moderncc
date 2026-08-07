#!/usr/bin/env python3
"""Cross-oracle differential runner for the gcc and llvm C test suites.

Each suite is scored by the *other* vendor's compiler, never by its own and
never by its own embedded expectations: gcc's tests are judged by clang, and
clang's tests are judged by gcc. A test passes when the mcc-built program has
the same observable behaviour -- exit status and stdout bytes -- as the
oracle-built program. What the suite's own directives predict is irrelevant
here; only agreement with a foreign implementation counts.

Two phases, because oracle qualification is expensive and mcc is not.

  qualify  Build every run-mode test with the oracle at -O0 and at -O2, run
           both, and run the -O0 binary twice. A test enters the oracle set
           only if all three runs agree. Disagreement means the program is
           nondeterministic or its behaviour is undefined-behaviour-sensitive,
           and in either case it cannot arbitrate anything. Result is cached
           to <out>/oracle.jsonl.

  check    Replay the cached oracle set against one mcc configuration. Cheap,
           so it can be run once per configuration (CPU baseline, GPU replay
           on, ...) and the per-test verdicts diffed against each other.

Usage:
  tools/xoracle.py --phase qualify --out cmake-release/xoracle \
                   --gcc ~/Projects/gcc --llvm ~/Projects/llvm \
                   --gcc-bin gcc --clang-bin clang [--jobs 32] [--limit N]

  tools/xoracle.py --phase check --out cmake-release/xoracle \
                   --mcc cmake-release/mcc --label cpu-baseline --opt=-O2 \
                   [--mcc-flag=-B...] [--mcc-env MCC_JIT_GPU=1]

`--opt` and `--mcc-env` must be attached with `=` when the value begins with a
dash, for the same argparse reason xsuite.py documents.
"""
import argparse, hashlib, json, os, subprocess, sys, threading
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xsuite

ORACLE_OPTS = ("-O0", "-O2")


def oracle_for(suite, gcc_bin, clang_bin):
    if suite.startswith("gcc:"):
        return clang_bin, "clang"
    if suite.startswith("llvm:"):
        return gcc_bin, "gcc"
    return None, ""


def digest(b):
    return hashlib.sha256(b).hexdigest()[:16]


def head(b, n=200):
    return b[:n].decode("utf-8", "replace")


class Phase:
    def __init__(self, out, jobs, ctimeout, rtimeout, name):
        self.out, self.jobs = out, jobs
        self.ctimeout, self.rtimeout = ctimeout, rtimeout
        self.lock = threading.Lock()
        self.fh = open(os.path.join(out, name), "w")
        self.n = 0
        self.tally = {}

    def emit(self, rec):
        with self.lock:
            self.fh.write(json.dumps(rec) + "\n")
            self.tally[rec["status"]] = self.tally.get(rec["status"], 0) + 1
            self.n += 1
            if self.n % 500 == 0:
                print(f"  ... {self.n} results", flush=True)

    def workdir(self, idx):
        w = os.path.join(self.out, "w%02d" % (idx % self.jobs))
        os.makedirs(w, exist_ok=True)
        return w

    def build(self, cc, src, flags, inc, dst, work, env=None, extra=()):
        cmd = [cc] + flags + inc + [src] + list(extra) + ["-o", dst, "-lm"]
        e = dict(os.environ)
        if env:
            e.update(env)
        try:
            p = subprocess.run(cmd, capture_output=True, timeout=self.ctimeout,
                               preexec_fn=xsuite.limits_compile, cwd=work, env=e)
        except subprocess.TimeoutExpired:
            return None, "timeout"
        except OSError as ex:
            return None, str(ex)
        if p.returncode != 0:
            err = (p.stderr or b"").decode("utf-8", "replace")
            if xsuite.REF_BADFLAG_RE.search(err):
                return None, "badflag"
            return None, xsuite.err_key(err)
        return dst, ""

    def execute(self, path, work, env=None):
        e = dict(os.environ)
        if env:
            e.update(env)
        try:
            p = subprocess.run([path], stdout=subprocess.PIPE,
                               stderr=subprocess.DEVNULL, timeout=self.rtimeout,
                               preexec_fn=xsuite.limits_run, cwd=work, env=e)
        except subprocess.TimeoutExpired:
            return None, None
        except OSError:
            return None, None
        return p.returncode, (p.stdout or b"")

    def close(self):
        self.fh.close()


def unlink(*paths):
    for p in paths:
        try:
            os.unlink(p)
        except OSError:
            pass


def qualify_one(ph, t, idx, gcc_bin, clang_bin):
    cc, vendor = oracle_for(t["suite"], gcc_bin, clang_bin)
    rec = {"suite": t["suite"], "file": t["file"], "oracle": vendor,
           "flags": t["flags"]}
    if not cc:
        rec["status"] = "NO_ORACLE"
        return ph.emit(rec)
    work = ph.workdir(idx)
    inc = ["-I" + os.path.dirname(t["file"])] + ["-I" + d for d in t.get("inc", [])]
    bins = {}
    for o in ORACLE_OPTS:
        dst = os.path.join(work, "q%d%s.x" % (idx, o.replace("-", "")))
        b, why = ph.build(cc, t["file"], [o] + t["flags"], inc, dst, work,
                          extra=t.get("extra", []))
        if not b:
            unlink(*bins.values())
            rec["status"] = "ORACLE_BADFLAG" if why == "badflag" else "ORACLE_NOBUILD"
            rec["why"] = why
            return ph.emit(rec)
        bins[o] = b
    runs = []
    for o in ORACLE_OPTS:
        rc, so = ph.execute(bins[o], work)
        if rc is None:
            unlink(*bins.values())
            rec["status"] = "ORACLE_TIMEOUT"
            return ph.emit(rec)
        runs.append((o, rc, so))
    rc2, so2 = ph.execute(bins[ORACLE_OPTS[0]], work)
    unlink(*bins.values())
    if rc2 is None:
        rec["status"] = "ORACLE_TIMEOUT"
        return ph.emit(rec)
    o0, rc0, so0 = runs[0]
    if (rc2, so2) != (rc0, so0):
        rec.update(status="NONDET", detail="repeat run of -O0 binary disagrees")
        return ph.emit(rec)
    o2, rc1, so1 = runs[1]
    if (rc1, so1) != (rc0, so0):
        rec.update(status="UB_SENSITIVE",
                   detail=f"-O0 exit={rc0} sha={digest(so0)} vs "
                          f"-O2 exit={rc1} sha={digest(so1)}")
        return ph.emit(rec)
    rec.update(status="OK", exit=rc0, stdout_sha=digest(so0),
               stdout_len=len(so0), stdout_head=head(so0), inc=t.get("inc", []),
               extra=t.get("extra", []))
    return ph.emit(rec)


def check_one(ph, o, idx, mcc, opt, mcc_flags, mcc_env):
    rec = {"suite": o["suite"], "file": o["file"], "oracle": o["oracle"],
           "opt": opt}
    work = ph.workdir(idx)
    inc = ["-I" + os.path.dirname(o["file"])] + ["-I" + d for d in o.get("inc", [])]
    dst = os.path.join(work, "c%d.x" % idx)
    b, why = ph.build(mcc, o["file"], [opt] + o["flags"] + mcc_flags, inc, dst,
                      work, mcc_env, extra=o.get("extra", []))
    if not b:
        unlink(dst)
        rec.update(status="LINK_POLICY" if "defined twice" in why else "MCC_NOBUILD",
                   why=why)
        return ph.emit(rec)
    rc, so = ph.execute(dst, work, mcc_env)
    unlink(dst)
    if rc is None:
        rec["status"] = "MCC_TIMEOUT"
        return ph.emit(rec)
    if rc != o["exit"]:
        rec.update(status="DIFF_EXIT", got_exit=rc, want_exit=o["exit"],
                   got_head=head(so), want_head=o["stdout_head"])
        return ph.emit(rec)
    sha = digest(so)
    if sha != o["stdout_sha"]:
        rec.update(status="DIFF_STDOUT", got_sha=sha, want_sha=o["stdout_sha"],
                   got_head=head(so), want_head=o["stdout_head"])
        return ph.emit(rec)
    rec.update(status="PASS", exit=rc)
    return ph.emit(rec)


BUILTIN_SUPPORT = ("int_util.c", "udivmoddi4.c")


def collect_builtins(llvm):
    """Pair each compiler-rt builtins Unit test with the implementation it exercises.

    These are the only substantial body of *execution* tests in the llvm tree --
    clang's own suite is a compile-and-FileCheck suite and yields almost nothing
    runnable -- so without them the gcc-judges-clang direction has no corpus.
    The test file `X_test.c` names its subject directly, so `lib/builtins/X.c` is
    the implementation under test; both are compiled by the compiler under test,
    which is the point, since the arithmetic being measured lives in the latter.
    """
    unit = os.path.join(llvm, "compiler-rt/test/builtins/Unit")
    lib = os.path.join(llvm, "compiler-rt/lib/builtins")
    if not (os.path.isdir(unit) and os.path.isdir(lib)):
        return []
    support = [os.path.join(lib, s) for s in BUILTIN_SUPPORT
               if os.path.exists(os.path.join(lib, s))]
    out = []
    for f in sorted(os.listdir(unit)):
        if not f.endswith("_test.c"):
            continue
        impl = os.path.join(lib, f[:-len("_test.c")] + ".c")
        if not os.path.exists(impl):
            continue
        extra = [impl] + [s for s in support if s != impl]
        out.append({"suite": "llvm:builtins", "file": os.path.join(unit, f),
                    "mode": "run", "expect": "ok", "flags": ["-w"],
                    "extra": extra, "inc": [lib, unit]})
    return out


def load_tests(args):
    args.suite_default_flags = False
    tests, _ = xsuite.collect(args)
    tests = [t for t in tests if t["mode"] == "run"]
    if args.llvm:
        tests += collect_builtins(args.llvm)
    if args.suite:
        tests = [t for t in tests if any(x in t["suite"] for x in args.suite)]
    if args.limit:
        tests = tests[:args.limit]
    return tests


def report(tally, total, title):
    print(f"\n{title}")
    print("-" * 56)
    for k in sorted(tally, key=lambda k: -tally[k]):
        print(f"  {k:<22}{tally[k]:>8}{100.0 * tally[k] / total:>8.2f}%")
    print("-" * 56)
    print(f"  {'total':<22}{total:>8}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--phase", choices=("qualify", "check"), required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--gcc", default="")
    ap.add_argument("--llvm", default="")
    ap.add_argument("--gcc-bin", default="gcc")
    ap.add_argument("--clang-bin", default="clang")
    ap.add_argument("--mcc", default="")
    ap.add_argument("--label", default="check")
    ap.add_argument("--opt", default="-O2")
    ap.add_argument("--mcc-flag", action="append", default=[])
    ap.add_argument("--mcc-env", action="append", default=[])
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--suite", action="append", default=[])
    ap.add_argument("--ctimeout", type=float, default=25.0)
    ap.add_argument("--rtimeout", type=float, default=15.0)
    args = ap.parse_args()
    args.out = os.path.abspath(args.out)
    os.makedirs(args.out, exist_ok=True)

    if args.phase == "qualify":
        if not (args.gcc or args.llvm):
            sys.exit("xoracle: qualify needs --gcc and/or --llvm")
        xsuite.ANSI_OK = True
        tests = load_tests(args)
        print(f"xoracle: qualifying {len(tests)} run-mode tests "
              f"(gcc tests -> {args.clang_bin}, llvm tests -> {args.gcc_bin}), "
              f"jobs={args.jobs}", flush=True)
        ph = Phase(args.out, args.jobs, args.ctimeout, args.rtimeout,
                   "oracle.jsonl")
        with ThreadPoolExecutor(max_workers=args.jobs) as ex:
            list(ex.map(lambda a: qualify_one(ph, a[1], a[0], args.gcc_bin,
                                              args.clang_bin),
                        list(enumerate(tests))))
        ph.close()
        report(ph.tally, ph.n, "oracle qualification")
        ok = ph.tally.get("OK", 0)
        print(f"\nxoracle: {ok} tests entered the oracle set -> "
              f"{os.path.join(args.out, 'oracle.jsonl')}")
        return

    if not args.mcc:
        sys.exit("xoracle: check needs --mcc")
    src = os.path.join(args.out, "oracle.jsonl")
    if not os.path.exists(src):
        sys.exit(f"xoracle: no oracle set at {src}; run --phase qualify first")
    oracle = [json.loads(l) for l in open(src)]
    oracle = [o for o in oracle if o["status"] == "OK"]
    if args.suite:
        oracle = [o for o in oracle if any(x in o["suite"] for x in args.suite)]
    if args.limit:
        oracle = oracle[:args.limit]
    env = {}
    for kv in args.mcc_env:
        k, _, v = kv.partition("=")
        env[k] = v
    print(f"xoracle: checking {len(oracle)} oracle cases against "
          f"{args.mcc} {args.opt} {' '.join(args.mcc_flag)} "
          f"env={env or '{}'} jobs={args.jobs}", flush=True)
    ph = Phase(args.out, args.jobs, args.ctimeout, args.rtimeout,
               "check-%s.jsonl" % args.label)
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        list(ex.map(lambda a: check_one(ph, a[1], a[0], os.path.abspath(args.mcc),
                                        args.opt, args.mcc_flag, env),
                    list(enumerate(oracle))))
    ph.close()
    report(ph.tally, ph.n, f"cross-oracle check: {args.label} {args.opt}")
    print(f"\nxoracle: wrote {os.path.join(args.out, 'check-%s.jsonl' % args.label)}")


if __name__ == "__main__":
    main()
