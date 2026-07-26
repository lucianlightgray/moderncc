#!/usr/bin/env python3
"""Runtime (generated-code speed) benchmark for mcc-compiled programs.

The repo's `tests/bench` measures how fast mcc COMPILES. This measures how fast
the code mcc EMITS runs, which is what every pending FP/codegen flip actually
needs: a gate that says "this flip helped x86_64 and did not regress arm64".
Before this existed each such decision was re-measured ad hoc on one host with
throwaway programs, and those numbers could not be reproduced or compared later.

Every run also CHECKS OUTPUT against a reference compiler, so a "fast" result
that miscompiles fails instead of scoring well. Correctness is the gate; timing
is advisory (see --check-only, which is what CI runs -- wall-clock on a shared
CI box is noise, so we do not assert on it).

Kernels: the in-tree vendor/plb C kernels that mcc can build, plus two compute
kernels kept in tests/runtime. Deliberately excluded, with reasons, so nobody
re-discovers them:
  spectral-norm/3.c  plain C99 `inline` with no external definition; mcc emits
                     no out-of-line body => unresolved 'A' (see the AOT item)
  nbody/5.c          x86 SIMD builtins + vector types; mcc has no VT_VECTOR
  binarytrees/2.c    needs libapr
  mandelbrot/1-ffi.c needs libcrypto MD5

Usage:
  tools/runtime-bench.py [--mcc PATH] [--cc PATH] [--runs N] [--check-only]
                         [--gates "K=V K=V"]...  [--json]

  --gates may be repeated to compare configurations; the first is the baseline
  and later ones are reported as a delta against it. An empty --gates "" means
  stock defaults.

Exit status: 0 all good, 1 an output mismatch or build failure, 77 skip (no
reference compiler, or kernels missing).
"""
import argparse, json, os, shutil, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLB = os.path.join(ROOT, "vendor", "plb", "bench", "algorithm")
RT = os.path.join(ROOT, "tests", "runtime")

# (name, source, argv, mcc_flags) -- argv sized so each kernel runs ~0.3-0.5s
# under gcc -O2; anything shorter and process startup dominates, making the
# timing useless. mcc_flags are extra flags only mcc gets (the reference
# compiler needs no equivalent).
KERNELS = [
    ("nbody",      os.path.join(PLB, "nbody", "2.c"),   ["10000000"], []),
    ("nsieve",     os.path.join(PLB, "nsieve", "1.c"),  ["12"],       []),
    ("mandelbrot", os.path.join(RT, "mandelbrot.c"),    ["3000"],     []),
    ("matmul",     os.path.join(RT, "matmul.c"),        ["600", "8"], []),
    # plain C99 `inline` with no external definition; mcc needs the weak
    # out-of-line body to resolve `A` (gcc supplies one by default)
    ("spectral",   os.path.join(PLB, "spectral-norm", "3.c"), ["3000"],
                   ["-fc99-inline-body"]),
]


def find_cc(explicit):
    if explicit:
        return explicit
    for c in (os.environ.get("MCC_BENCH_CC"), "gcc", "clang", "cc"):
        if c and shutil.which(c):
            return shutil.which(c)
    return None


def build(cc, src, out, extra_env=None, mcc=False, flags=None):
    cmd = [cc, "-O2"] + (flags or []) + [src, "-o", out, "-lm"]
    if not mcc:
        cmd.insert(1, "-w")
        # keep the reference honest: no FP contraction, so a fused multiply-add
        # in the reference does not read as an mcc miscompile
        cmd.insert(1, "-ffp-contract=off")
    env = dict(os.environ)
    if extra_env:
        env.update(extra_env)
    p = subprocess.run(cmd, env=env, capture_output=True, text=True)
    return (p.returncode == 0 and os.path.exists(out)), p.stderr


def run_once(exe, argv):
    t0 = time.perf_counter()
    p = subprocess.run([exe] + argv, capture_output=True, text=True)
    dt = (time.perf_counter() - t0) * 1000.0
    return p.returncode, p.stdout.strip(), dt


def parse_gates(s):
    env = {}
    for tok in (s or "").split():
        k, _, v = tok.partition("=")
        if k:
            env[k] = v
    return env


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mcc", default=os.path.join(ROOT, "cmake-debug", "mcc"))
    ap.add_argument("--cc", default=None)
    ap.add_argument("--runs", type=int, default=5)
    ap.add_argument("--check-only", action="store_true",
                    help="one run per kernel, correctness only, no timing table")
    ap.add_argument("--gates", action="append", default=None)
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    cc = find_cc(args.cc)
    if not cc:
        print("no reference C compiler; skipping")
        return 77
    if not os.access(args.mcc, os.X_OK):
        print(f"no mcc at {args.mcc}; skipping")
        return 77
    kernels = [k for k in KERNELS if os.path.exists(k[1])]
    if not kernels:
        print("no benchmark kernels present; skipping")
        return 77

    gate_sets = args.gates if args.gates else [""]
    runs = 1 if args.check_only else args.runs
    failures, results = [], {}

    with tempfile.TemporaryDirectory() as td:
        for name, src, argv, mflags in kernels:
            ref = os.path.join(td, name + ".ref")
            ok, err = build(cc, src, ref)
            if not ok:
                print(f"SKIP {name}: reference build failed: {err.strip()[:120]}")
                continue
            rc, expect, ref_ms = run_once(ref, argv)
            if rc != 0:
                print(f"SKIP {name}: reference run failed")
                continue
            for _ in range(runs - 1):
                _, _, t = run_once(ref, argv)
                ref_ms = min(ref_ms, t)

            for gi, gates in enumerate(gate_sets):
                env = parse_gates(gates)
                exe = os.path.join(td, f"{name}.mcc{gi}")
                ok, err = build(args.mcc, src, exe, env, mcc=True, flags=mflags)
                if not ok:
                    failures.append(f"{name} [{gates or 'defaults'}]: mcc build failed: {err.strip()[:160]}")
                    continue
                best, bad = None, False
                for _ in range(runs):
                    rc, out, t = run_once(exe, argv)
                    if rc != 0 or out != expect:
                        failures.append(
                            f"{name} [{gates or 'defaults'}]: output mismatch\n"
                            f"    want: {expect}\n    got:  {out}")
                        bad = True
                        break
                    best = t if best is None else min(best, t)
                if not bad:
                    results.setdefault(name, {})["ref"] = ref_ms
                    results[name][gates or "defaults"] = best

    if not args.check_only and results:
        # gate strings are far too long to use as column headers, so number them
        # and print a legend underneath
        cols = [g or "defaults" for g in gate_sets]
        labels = ["cfg%d" % i for i in range(len(cols))]
        hdr = f"\n{'kernel':<12}{'ref(ms)':>9}" + "".join(f"{l:>9}" for l in labels)
        hdr += f"{'vs ref':>9}" + ("".join(f"{'d'+l:>9}" for l in labels[1:]) if len(cols) > 1 else "")
        print(hdr)
        for name, _, _, _ in kernels:
            if name not in results:
                continue
            r = results[name]
            line = f"{name:<12}{r['ref']:>9.0f}"
            for c in cols:
                v = r.get(c)
                line += f"{v:>9.0f}" if v else f"{'-':>9}"
            first = r.get(cols[0])
            line += f"{first / r['ref']:>8.2f}x" if first and r["ref"] else f"{'-':>9}"
            for c in cols[1:]:
                line += f"{(r[c] - first) / first * 100:>+8.1f}%" if r.get(c) and first else f"{'-':>9}"
            print(line)
        print()
        for l, c in zip(labels, cols):
            print(f"  {l} = {c}")
        print("\ntiming is advisory: wall-clock varies with load, and only the")
        print("output check above is a pass/fail gate")

    if args.json:
        print(json.dumps({"results": results, "failures": failures}, indent=2))
    for f in failures:
        print(f"FAIL {f}")
    if failures:
        return 1
    print(f"\nruntime-bench: OK ({len(results)} kernels x {len(gate_sets)} config(s), output verified vs {os.path.basename(cc)})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
