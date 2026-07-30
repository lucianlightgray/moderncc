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
                         [--gates "K=V K=V"]...  [--json] [--assert-gate-wins]

  --gates may be repeated to compare configurations; the first is the baseline
  and later ones are reported as a delta against it. An empty --gates "" means
  stock defaults.

  --assert-gate-wins is the one timing mode that IS a pass/fail gate: it times
  the GATE_WINS table's default-on flips against the single kernel each one
  moves and fails if the win is gone. It asserts only that a large win still
  exists, and skips (77) whenever the box is too noisy to say -- see the
  comments on GATE_WINS and assert_gate_wins.

Exit status: 0 all good, 1 an output mismatch or build failure, 77 skip (no
reference compiler, or kernels missing).
"""
import argparse, json, os, shutil, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLB = os.path.join(ROOT, "vendor", "plb", "bench", "algorithm")
RT = os.path.join(ROOT, "tests", "runtime")

KERNELS = [
    ("nbody",      os.path.join(PLB, "nbody", "2.c"),   ["10000000"], []),
    ("nsieve",     os.path.join(PLB, "nsieve", "1.c"),  ["12"],       []),
    ("mandelbrot", os.path.join(RT, "mandelbrot.c"),    ["3000"],     []),
    ("matmul",     os.path.join(RT, "matmul.c"),        ["600", "8"], []),
    ("spectral",   os.path.join(PLB, "spectral-norm", "3.c"), ["3000"],
                   ["-fc99-inline-body"]),
]

GATE_WINS = [
    ("MCC_AST_OPASSIGN",   "nbody",
     os.path.join(PLB, "nbody", "2.c"), ["5000000"], [], 8.0),
    ("MCC_AST_CHAINSTORE", "spectral",
     os.path.join(PLB, "spectral-norm", "3.c"), ["2000"],
     ["-fc99-inline-body"], 8.0),
]

GATE_WIN_NOISE_MAX = 0.12
GATE_WIN_CONFIRM_NOISE_MAX = 0.05
GATE_WIN_MIN_MS = 60.0


def find_cc(explicit):
    if explicit:
        return explicit
    for c in (os.environ.get("MCC_BENCH_CC"), "gcc", "clang", "cc"):
        if c and shutil.which(c):
            return shutil.which(c)
    return None


def build(cc, src, out, extra_env=None, mcc=False, flags=None, opt="-O2"):
    cmd = [cc, opt] + (flags or []) + [src, "-o", out, "-lm"]
    if not mcc:
        cmd.insert(1, "-w")
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


def run_once_cpu(exe, argv):
    """Like run_once, but the metric is the child's own CPU time, not wall.

    This is what makes the gate-win assertion usable inside a `ctest -j` run.
    Wall-clock counts the time the kernel had the process descheduled, so on a
    loaded box every configuration converges towards "slow" and a real 16-31%
    codegen win reads as 0% -- measured, not theorised: an in-suite run reported
    both gates at ~0% with each binary reproducing itself to within 5%, which is
    a confident wrong answer, the one failure mode a perf gate must not have.
    CPU time is charged only while the process is on a core, so contention
    stretches the run without changing what it measures."""
    if not hasattr(os, "wait4"):
        return run_once(exe, argv)
    with tempfile.TemporaryFile() as fo:
        p = subprocess.Popen([exe] + argv, stdout=fo, stderr=subprocess.DEVNULL)
        _, status, ru = os.wait4(p.pid, 0)
        p.returncode = (os.WEXITSTATUS(status) if os.WIFEXITED(status)
                        else -os.WTERMSIG(status))
        fo.seek(0)
        out = fo.read().decode("utf-8", "replace").strip()
    return p.returncode, out, (ru.ru_utime + ru.ru_stime) * 1000.0


def counter_backend():
    """Which instructions-retired source this host has, or None.

    perf works only if the binary exists AND the kernel lets us count. Both
    checks matter: a container with perf installed but perf_event_paranoid
    locked down fails at run time, not at which().

    Darwin has no perf, but on Apple Silicon /usr/bin/time -l reports the same
    counter out of rusage_info(RUSAGE_INFO_V4) -- no root, no entitlement.
    Intel Macs have no such counter, so the probe demands a non-zero count from
    a real workload rather than trusting the label; /usr/bin/true retires far
    more than 1000 instructions through dyld alone."""
    if shutil.which("perf"):
        p = subprocess.run(["perf", "stat", "-e", "instructions", "-x,", "true"],
                           capture_output=True, text=True)
        if p.returncode == 0 and "instructions" in p.stderr:
            return "perf"
    if sys.platform == "darwin" and os.path.exists("/usr/bin/time"):
        n = darwin_instructions("/usr/bin/true", [])
        if n and n > 1000:
            return "darwin"
    return None


def darwin_instructions(exe, argv):
    p = subprocess.run(["/usr/bin/time", "-l", exe] + argv,
                       capture_output=True, text=True)
    if p.returncode != 0:
        return None
    for line in p.stderr.splitlines():
        f = line.split()
        if len(f) >= 3 and f[1] == "instructions" and f[2] == "retired":
            try:
                return int(f[0]) or None
            except ValueError:
                return None
    return None


def perf_instructions(exe, argv):
    p = subprocess.run(["perf", "stat", "-e", "instructions:u", "-x,",
                        exe] + argv, capture_output=True, text=True)
    if p.returncode != 0:
        return None
    for line in p.stderr.splitlines():
        f = line.split(",")
        if len(f) > 2 and f[2].startswith("instructions"):
            try:
                return int(f[0])
            except ValueError:
                return None
    return None


def instructions_retired(exe, argv, backend):
    """Dynamic instruction count -- the metric that separates 'emits less work'
    from 'got lucky on code layout'. nsieve showed why this belongs here:
    +8.5% cycles with the instruction count unchanged to within 36 out of 4.6
    billion, i.e. a front-end effect and not a codegen regression. Returns None
    when the backend cannot count."""
    if backend == "darwin":
        return darwin_instructions(exe, argv)
    if backend == "perf":
        return perf_instructions(exe, argv)
    return None


def box_too_busy():
    """Circuit breaker for a hammered box only. Deliberately loose (4x the core
    count): the 1-minute average still reads far above the core count for
    minutes after a parallel `ctest -j` run even once the box is idle again --
    which is exactly when this gate runs -- so a tight limit here would make it
    skip every time it matters. Real noise is caught by the interleaving and
    the repeat probe in assert_gate_wins, which measure the box instead of
    guessing at it; this only avoids burning a minute on a hopeless one."""
    try:
        load = os.getloadavg()[0]
    except (OSError, AttributeError):
        return None
    ncpu = os.cpu_count() or 1
    return "%.1f over %d cpus" % (load, ncpu) if load > 4 * ncpu else None


def gate_win_round(exes, argv, expect, runs):
    """One interleaved measurement round: returns (times, drift, mismatch).

    Interleaved so a frequency or load ramp hits BOTH configs, never one.
    `drift` is the worst first-half-minimum vs second-half-minimum spread of
    either config against ITSELF -- a binary failing to reproduce its own
    number is the cheapest honest measure of how far this box can be trusted,
    and taking the worse of the two makes the probe harder to pass by luck than
    the comparison it guards. Output is compared with the reference on every
    single run, so a "fast but wrong" binary reports a mismatch instead of a
    good number."""
    times = {label: [] for label, _ in exes}
    for _, exe in exes:
        run_once_cpu(exe, argv)
    for _ in range(runs):
        for label, exe in exes:
            rc, out, ms = run_once_cpu(exe, argv)
            if rc != 0 or out != expect:
                return times, None, f"[{label}]: output mismatch\n" \
                                    f"    want: {expect}\n    got:  {out}"
            times[label].append(ms)
    half = runs // 2
    drift = 0.0
    for label, _ in exes:
        a, b = min(times[label][:half]), min(times[label][half:])
        drift = max(drift, abs(a - b) / min(a, b))
    return times, drift, None


def gate_win_best_pair(times):
    """The most favourable win any single interleaved on/off pair showed.

    A regression verdict has to survive this too: if even one pair -- one
    on-run and the off-run beside it, the two least likely to be disturbed
    differently -- still saw the win, the aggregate saying otherwise is noise
    talking, not a lost optimization."""
    pairs = zip(times["on"], times["off"])
    return max(((off - on) / off * 100.0) for on, off in pairs)


def assert_gate_wins(mcc, cc, runs):
    """Perf ratchet for GATE_WINS: fail if a gate's measured win disappears.

    Every rule here exists to keep this from crying wolf on a loaded box. Each
    config keeps the MINIMUM of its interleaved runs (the least-interrupted
    run; an average can only be inflated by noise). A win at or above the
    threshold passes immediately. A win BELOW it is never failed on one round:
    it is re-measured, both rounds are pooled, and the failure only stands if
    the pooled win is still short AND both rounds reproduced themselves to
    within GATE_WIN_CONFIRM_NOISE_MAX. Anything less conclusive skips."""
    busy = box_too_busy()
    if busy:
        print("load average %s; timing unreliable, skipping" % busy)
        return 77
    entries = [e for e in GATE_WINS if os.path.exists(e[2])]
    if not entries:
        print("no gate-win kernels present; skipping")
        return 77

    failures, measured, skipped = [], 0, 0
    with tempfile.TemporaryDirectory() as td:
        for gate, name, src, argv, mflags, min_win in entries:
            ref = os.path.join(td, name + ".ref")
            ok, err = build(cc, src, ref)
            if not ok:
                print(f"SKIP {gate}/{name}: reference build failed: {err.strip()[:120]}")
                skipped += 1
                continue
            rc, expect, _ = run_once(ref, argv)
            if rc != 0:
                print(f"SKIP {gate}/{name}: reference run failed")
                skipped += 1
                continue

            exes, bad = [], False
            for label, env in (("on", None), ("off", {gate: "0"})):
                exe = os.path.join(td, f"{name}.{label}")
                ok, err = build(mcc, src, exe, env, mcc=True, flags=mflags, opt="-O3")
                if not ok:
                    failures.append(f"{gate}/{name}: mcc -O3 {label} build failed: "
                                    f"{err.strip()[:160]}")
                    bad = True
                    break
                exes.append((label, exe))
            if bad:
                continue

            times, drift, mismatch = gate_win_round(exes, argv, expect, runs)
            if mismatch:
                failures.append(f"{gate}/{name} {mismatch}")
                continue
            if drift > GATE_WIN_NOISE_MAX:
                print(f"SKIP {gate}/{name}: the same binary measured "
                      f"{drift * 100:.1f}% apart from itself (limit "
                      f"{GATE_WIN_NOISE_MAX * 100:.0f}%); box too noisy to judge")
                skipped += 1
                continue
            on_ms, off_ms = min(times["on"]), min(times["off"])
            if off_ms < GATE_WIN_MIN_MS:
                print(f"SKIP {gate}/{name}: {off_ms:.0f}ms is too short to time")
                skipped += 1
                continue

            win = (off_ms - on_ms) / off_ms * 100.0
            nruns, verdict = runs, "ok"
            if win < min_win:
                print(f"{gate:<20} {name:<10} win {win:+6.1f}% is under the "
                      f"{min_win:.0f}% floor; re-measuring before failing")
                times2, drift2, mismatch = gate_win_round(exes, argv, expect, runs)
                if mismatch:
                    failures.append(f"{gate}/{name} {mismatch}")
                    continue
                on_ms = min(on_ms, min(times2["on"]))
                off_ms = min(off_ms, min(times2["off"]))
                win, nruns = (off_ms - on_ms) / off_ms * 100.0, runs * 2
                best_pair = max(gate_win_best_pair(times),
                                gate_win_best_pair(times2))
                if win >= min_win:
                    verdict = "ok"
                elif (max(drift, drift2) > GATE_WIN_CONFIRM_NOISE_MAX
                      or best_pair >= min_win):
                    verdict = "noisy"
                else:
                    verdict = "regressed"

            print(f"{gate:<20} {name:<10} on {on_ms:8.1f}ms  off {off_ms:8.1f}ms"
                  f"  win {win:+6.1f}%  (cpu, need >= {min_win:.0f}%, min of {nruns})")
            if verdict == "noisy":
                print(f"SKIP {gate}/{name}: win is short of the floor, but the box "
                      f"drifted {max(drift, drift2) * 100:.1f}% against itself "
                      f"(need <= {GATE_WIN_CONFIRM_NOISE_MAX * 100:.0f}%) and the "
                      f"best single pair still saw {best_pair:+.1f}% (need < "
                      f"{min_win:.0f}%) -- not failing on this evidence")
                skipped += 1
            elif verdict == "regressed":
                failures.append(
                    f"{gate}/{name}: win collapsed to {win:+.1f}% over {nruns} runs "
                    f"(need >= {min_win:.0f}%; on {on_ms:.0f}ms vs off {off_ms:.0f}ms of cpu). "
                    f"Either the gate stopped firing on this kernel, it is no longer "
                    f"on by default at -O3, or something else subsumed it -- "
                    f"re-measure by hand before touching this threshold")
            else:
                measured += 1

    for f in failures:
        print(f"FAIL {f}")
    if failures:
        return 1
    if not measured:
        print(f"no gate could be timed reliably ({skipped} skipped); skipping")
        return 77
    print(f"\nruntime-bench: gate wins OK ({measured} gate(s), min cpu of {runs} "
          f"interleaved runs, output verified vs {os.path.basename(cc)})")
    return 0


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
    ap.add_argument("--no-perf", action="store_true",
                    help="skip the instructions-retired columns even where a "
                         "counter is available (perf, or Apple Silicon time -l)")
    ap.add_argument("--assert-gate-wins", action="store_true",
                    help="ratchet: fail if a GATE_WINS flip's measured win is gone")
    args = ap.parse_args()

    cc = find_cc(args.cc)
    if not cc:
        print("no reference C compiler; skipping")
        return 77
    if not os.access(args.mcc, os.X_OK):
        print(f"no mcc at {args.mcc}; skipping")
        return 77
    if args.assert_gate_wins:
        return assert_gate_wins(args.mcc, cc, max(args.runs, 4))
    kernels = [k for k in KERNELS if os.path.exists(k[1])]
    if not kernels:
        print("no benchmark kernels present; skipping")
        return 77

    backend = None
    if not args.check_only and not args.no_perf:
        backend = counter_backend()
        if backend is None:
            print("runtime-bench: no instructions-retired counter on this host "
                  "(no usable perf, no Apple Silicon /usr/bin/time -l); the table "
                  "is TIMING-ONLY and its deltas are advisory")
    use_perf = backend is not None
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
            if use_perf:
                ref_ins = instructions_retired(ref, argv, backend)
                if ref_ins is not None:
                    results.setdefault(name, {})["ref_ins"] = ref_ins

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
                    if use_perf:
                        ins = instructions_retired(exe, argv, backend)
                        if ins is not None:
                            results[name].setdefault("ins", {})[gates or "defaults"] = ins

    if not args.check_only and results:
        cols = [g or "defaults" for g in gate_sets]
        labels = ["cfg%d" % i for i in range(len(cols))]
        hdr = f"\n{'kernel':<12}{'ref(ms)':>9}" + "".join(f"{l:>9}" for l in labels)
        hdr += f"{'vs ref':>9}" + ("".join(f"{'d'+l:>9}" for l in labels[1:]) if len(cols) > 1 else "")
        if any(r.get("ins") for r in results.values()):
            hdr += f"{'insn/ref':>9}{'insns':>10}" + "".join(f"{'d'+l:>9}" for l in labels[1:])
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
            ins = r.get("ins", {})
            base_ins = ins.get(cols[0])
            if base_ins:
                ri = r.get("ref_ins")
                line += f"{base_ins / ri:>8.2f}x" if ri else f"{'-':>9}"
                line += f"{base_ins / 1e9:>9.2f}G"
                for c in cols[1:]:
                    line += (f"{(ins[c] - base_ins) / base_ins * 100:>+8.1f}%"
                             if ins.get(c) else f"{'-':>9}")
            print(line)
        print()
        for l, c in zip(labels, cols):
            print(f"  {l} = {c}")
        print("\ntiming is advisory: wall-clock varies with load, and only the")
        print("output check above is a pass/fail gate")
        if any(r.get("ins") for r in results.values()):
            print("insn/ref = mcc instructions retired divided by the reference")
            print("compiler's: the real work gap, free of layout and cache noise.")
            print("A time delta with NO insn delta is layout, not codegen --")
            print("judge flips on the insn columns, not the time columns")

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
