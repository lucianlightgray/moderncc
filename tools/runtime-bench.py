#!/usr/bin/env python3
"""Runtime (generated-code speed) benchmark for mcc-compiled programs.

The repo's `tests/bench` measures how fast mcc COMPILES. This measures how fast
the code mcc EMITS runs, which is what every pending FP/codegen flip actually
needs: a gate that says "this flip helped x86_64 and did not regress arm64".
Before this existed each such decision was re-measured ad hoc on one host with
throwaway programs, and those numbers could not be reproduced or compared later.

Every run also CHECKS OUTPUT against a reference compiler, so a "fast" result
that miscompiles fails instead of scoring well. In the main table correctness
is the gate and timing is advisory (wall-clock on a shared, unpinned box is
noise). The one place timing IS asserted is --assert-gate-wins, and only there
because it refuses to run on a busy host and pins every run to a fixed CPU.

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
                         [--write-baseline [PATH]] [--baseline [PATH]]
                         [--assert-baseline] [--json-out PATH]

  --gates may be repeated to compare configurations; the first is the baseline
  and later ones are reported as a delta against it. An empty --gates "" means
  stock defaults.

  --assert-gate-wins is a pass/fail gate measured in WALL-CLOCK only: for each
  GATE_WINS row it times the default-on optimization against -fno-<flag> and
  fails only if the optimization REGRESSED its kernel (a no-regression A/B, per
  the human answer to Q-mac-30011). Wall-clock is made trustworthy by the run
  conditions, not by a counter: it is skipped unless the host is under
  GATE_WIN_BUSY_MAX percent busy, and every run is pinned to one fixed CPU --
  the known-good resource slice (see assert_gate_wins).

  --write-baseline records this run's per-kernel instruction counts under
  tests/runtime/baselines/<cpu>-<os>.json, and --baseline diffs a later run
  against it -- the regression store the timing table could never be, because
  instructions are a property of the emitted code and milliseconds are a
  property of the box. --assert-baseline turns the diff into a gate at
  --baseline-tolerance percent (default 2%, against a measured run-to-run
  spread of 0.04%).

Exit status: 0 all good, 1 an output mismatch or build failure, 77 skip (no
reference compiler, or kernels missing).
"""
import argparse, json, os, shutil, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLB = os.path.join(ROOT, "vendor", "plb", "bench", "algorithm")
RT = os.path.join(ROOT, "tests", "runtime")

KERNELS = [
    ("nbody-plb",  os.path.join(PLB, "nbody", "2.c"),   ["10000000"], []),
    ("nsieve",     os.path.join(PLB, "nsieve", "1.c"),  ["12"],       []),
    ("mandelbrot", os.path.join(RT, "mandelbrot.c"),    ["3000"],     []),
    ("matmul",     os.path.join(RT, "matmul.c"),        ["600", "8"], []),
    ("spectral",   os.path.join(PLB, "spectral-norm", "3.c"), ["3000"],
                   ["-fc99-inline-body"]),

    # Added for the -O ladder re-assignment. Five kernels could not exercise 48
    # flags: nothing above measured an integer divide, a switch dispatch, a
    # struct copy, a call-heavy leaf, a narrowing conversion or string work, so
    # most of the table read as "does not move" for want of a subject rather
    # than for want of an effect. Each of these is here because a specific
    # family of rows had no other way to show itself, and the two vendor/plb
    # kernels the list leans on are not present in every checkout at all.
    ("sieve",      os.path.join(RT, "sieve.c"),      ["1000000", "20"], []),
    ("nbody",      os.path.join(RT, "nbody.c"),      ["120000"],        []),
    ("interp",     os.path.join(RT, "interp.c"),     ["1000000"],       []),
    ("structops",  os.path.join(RT, "structops.c"),  ["600"],           []),
    ("calls",      os.path.join(RT, "calls.c"),      ["2000000"],       []),
    ("divmod",     os.path.join(RT, "divmod.c"),     ["8000000"],       []),
    ("branchy",    os.path.join(RT, "branchy.c"),    ["120"],           []),
    ("hashmap",    os.path.join(RT, "hashmap.c"),    ["1000000"],       []),
    ("loopnest",   os.path.join(RT, "loopnest.c"),   ["3"],             []),
    ("vlaloop",    os.path.join(RT, "vlaloop.c"),    ["60000"],         []),
    ("regpress",   os.path.join(RT, "regpress.c"),   ["60000"],         []),
    ("poly",       os.path.join(RT, "poly.c"),       ["3000000"],       []),
    ("narrowops",  os.path.join(RT, "narrowops.c"),  ["260"],           []),
    ("mathfun",    os.path.join(RT, "mathfun.c"),    ["1600000"],       []),
    ("strproc",    os.path.join(RT, "strproc.c"),    ["140"],           []),
]

# Each row is (flag, kernel, src, argv, mflags, expect_win_pct). The first field
# is the -f flag the A/B toggles: "on" builds plain -O3, "off" builds -O3
# -fno-<flag>, so the flag must be DEFAULT-ON at -O3 or both arms compile the
# identical binary and the win reads a vacuous 0% (exactly how chain-store failed
# once it was demoted to -O4). expect_win_pct is the wall-clock win measured when
# the subject was banked; it is INFORMATIONAL (printed so an eroding win shows up
# in the log). The gate does not require it -- it fails only on a real regression
# (see assert_gate_wins).
#
# Subject: promote-locals on nbody, per the human answer to Q-mac-30011. It
# replaced chain-store/spectral, which p5 (172a2f31) turned into a ~2x
# pessimization and whose vendor/plb source is absent on most checkouts -- a
# permanent 77 that hid the breakage for weeks. nbody.c is in-tree and
# git-tracked, so the gate now actually runs on every box; register promotion is
# the most fundamental optimization mcc has and wins ~16-19% of wall-clock here,
# reproducibly, on a quiet pinned core.
GATE_WINS = [
    ("promote-locals", "nbody", os.path.join(RT, "nbody.c"), ["120000"], [], 18.0),
]

GATE_WIN_NOISE_MAX = 0.12
GATE_WIN_CONFIRM_NOISE_MAX = 0.05
GATE_WIN_MIN_MS = 60.0
GATE_WIN_REGRESS_FLOOR = 2.0   # min noise band %: fail only when the on-arm is
                               # slower than off by more than max(this, drift)
GATE_WIN_BUSY_MAX = 90.0       # skip if the host is more than this percent busy


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


def instructions_retired_min(exe, argv, backend, reads=3):
    """Minimum of `reads` counts. The counter is architectural but not
    deterministic across runs: a kernel that first-touches a large allocation
    pays page-fault work that lands in the same rusage counter, and nsieve
    measured 5.2116..5.2826 G (1.4%) from ONE binary that way. The minimum is
    the run that faulted least, which is the reproducible end."""
    vals = [instructions_retired(exe, argv, backend) for _ in range(reads)]
    vals = [v for v in vals if v]
    return min(vals) if vals else None


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


def host_busy_pct():
    """Aggregate CPU utilisation right now as a percent, or None if unknown.

    The gate refuses to time on a host more than GATE_WIN_BUSY_MAX percent busy:
    wall-clock only means what it says on a quiet box. A loaded box deschedules
    the kernel and every arm converges towards "slow", reading a real win as 0%
    -- the one failure mode a timing gate must not have. This replaces the old
    load-average circuit breaker with a true short-window utilisation sample.

    linux: two /proc/stat reads ~150ms apart -> 1 - idle_delta / total_delta.
    else, with getloadavg: loadavg[0] / ncpu, a coarser proxy. None where
    neither exists (e.g. win32, which the gate cell already excludes upstream)."""
    if os.path.exists("/proc/stat"):
        def sample():
            with open("/proc/stat") as f:
                vals = [int(x) for x in f.readline().split()[1:]]
            return sum(vals), vals[3] + (vals[4] if len(vals) > 4 else 0)
        try:
            t0, i0 = sample()
            time.sleep(0.15)
            t1, i1 = sample()
        except (OSError, ValueError, IndexError):
            return None
        dt = t1 - t0
        if dt <= 0:
            return None
        return max(0.0, min(100.0, (1.0 - (i1 - i0) / dt) * 100.0))
    try:
        return os.getloadavg()[0] / (os.cpu_count() or 1) * 100.0
    except (OSError, AttributeError):
        return None


def gate_win_cpu():
    """The known-good resource slice: the one fixed CPU the A/B is pinned to, so
    wall-clock is a property of the emitted code and not of which core the
    scheduler happened to pick. The highest-numbered allowed CPU is chosen on
    purpose -- core 0 fields most of the IRQ/softirq and kernel housekeeping, and
    measuring divmagic there read -2.9% (a phantom regression) where a clean core
    read +24%. None where the platform has no affinity call (darwin, win32); the
    darwin substitute is taskpolicy -b, applied in run_pinned."""
    try:
        return max(os.sched_getaffinity(0))
    except (AttributeError, OSError):
        return None


def run_pinned(exe, argv, cpu):
    """One wall-clock run confined to the known-good resource slice.

    linux: the child is pinned to `cpu` via sched_setaffinity in a preexec_fn.
    darwin: no affinity API in the stdlib, so fall back to `taskpolicy -b` (the
    repo's established substitute, see tests/optfire/flagsweep.sh); both A/B arms
    get it identically, so the comparison stays fair. Elsewhere: unpinned (win32
    is excluded from the gate cell upstream)."""
    cmd, pre = [exe] + argv, None
    if cpu is not None:
        pre = lambda: os.sched_setaffinity(0, {cpu})
    elif sys.platform == "darwin" and shutil.which("taskpolicy"):
        cmd = ["taskpolicy", "-b"] + cmd
    t0 = time.perf_counter()
    p = subprocess.run(cmd, capture_output=True, text=True, preexec_fn=pre)
    return p.returncode, p.stdout.strip(), (time.perf_counter() - t0) * 1000.0


def gate_win_round(exes, argv, expect, runs, cpu):
    """One interleaved measurement round: returns (times, drift, mismatch).

    Interleaved so a frequency or load ramp hits BOTH configs, never one. Every
    run is pinned to `cpu` (run_pinned). `drift` is the worst first-half-minimum
    vs second-half-minimum spread of either config against ITSELF -- a binary
    failing to reproduce its own number is the cheapest honest measure of how
    far this box can be trusted, and taking the worse of the two makes the probe
    harder to pass by luck than the comparison it guards. Output is compared with
    the reference on every single run, so a "fast but wrong" binary reports a
    mismatch instead of a good number."""
    times = {label: [] for label, _ in exes}
    for _, exe in exes:
        run_pinned(exe, argv, cpu)
    for _ in range(runs):
        for label, exe in exes:
            rc, out, ms = run_pinned(exe, argv, cpu)
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
    """No-regression wall-clock ratchet for GATE_WINS (the human answer to
    Q-mac-30011). The metric is WALL-CLOCK only -- no instructions-retired path
    -- made trustworthy by two conditions rather than a counter: the host must
    be under GATE_WIN_BUSY_MAX percent busy, and every run is pinned to one
    fixed CPU, the known-good resource slice. It asserts NO REGRESSION: the
    default-on optimization must not make its kernel slower than -fno-<flag> by
    more than the run's own measured noise band (max of GATE_WIN_REGRESS_FLOOR
    and the self-drift). The win is printed for visibility but is not required.

    The anti-crying-wolf machinery is kept: each config keeps the MINIMUM of its
    interleaved runs; a suspected regression is never failed on one round but
    re-measured, both rounds pooled, and only stands if the box also reproduced
    itself to within GATE_WIN_CONFIRM_NOISE_MAX and no single on/off pair held."""
    busy = host_busy_pct()
    if busy is not None and busy > GATE_WIN_BUSY_MAX:
        print(f"host {busy:.0f}% busy (limit {GATE_WIN_BUSY_MAX:.0f}%); wall-clock "
              f"unreliable, skipping")
        return 77
    entries = [e for e in GATE_WINS if os.path.exists(e[2])]
    if not entries:
        print("no gate-win kernels present; skipping")
        return 77

    cpu = gate_win_cpu()
    slice_desc = (f"pinned cpu {cpu}" if cpu is not None else
                  "taskpolicy -b" if sys.platform == "darwin"
                  and shutil.which("taskpolicy") else "unpinned")
    failures, measured, skipped = [], 0, 0
    with tempfile.TemporaryDirectory() as td:
        for gate, name, src, argv, mflags, expect_win in entries:
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
            for label, gflag in (("on", []), ("off", ["-fno-" + gate])):
                exe = os.path.join(td, f"{name}.{label}")
                ok, err = build(mcc, src, exe, None, mcc=True,
                                flags=mflags + gflag, opt="-O3")
                if not ok:
                    failures.append(f"{gate}/{name}: mcc -O3 {label} build failed: "
                                    f"{err.strip()[:160]}")
                    bad = True
                    break
                exes.append((label, exe))
            if bad:
                continue

            times, drift, mismatch = gate_win_round(exes, argv, expect, runs, cpu)
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
            band = max(GATE_WIN_REGRESS_FLOOR, drift * 100.0)
            nruns, verdict = runs, "ok"
            if win < -band:
                print(f"{gate:<20} {name:<10} win {win:+6.1f}% is past the "
                      f"-{band:.1f}% noise band; re-measuring before failing")
                times2, drift2, mismatch = gate_win_round(exes, argv, expect, runs, cpu)
                if mismatch:
                    failures.append(f"{gate}/{name} {mismatch}")
                    continue
                on_ms = min(on_ms, min(times2["on"]))
                off_ms = min(off_ms, min(times2["off"]))
                win, nruns = (off_ms - on_ms) / off_ms * 100.0, runs * 2
                band = max(GATE_WIN_REGRESS_FLOOR, max(drift, drift2) * 100.0)
                best_pair = max(gate_win_best_pair(times),
                                gate_win_best_pair(times2))
                if win >= -band:
                    verdict = "ok"
                elif (max(drift, drift2) > GATE_WIN_CONFIRM_NOISE_MAX
                      or best_pair >= -band):
                    verdict = "noisy"
                else:
                    verdict = "regressed"

            print(f"{gate:<20} {name:<10} on {on_ms:8.1f}ms  off {off_ms:8.1f}ms"
                  f"  win {win:+6.1f}%  (wall, {slice_desc}, need >= -{band:.1f}%, "
                  f"min of {nruns}; banked ~{expect_win:.0f}%)")
            if verdict == "noisy":
                print(f"SKIP {gate}/{name}: on-arm looks slower but the box "
                      f"drifted {max(drift, drift2) * 100:.1f}% against itself "
                      f"(need <= {GATE_WIN_CONFIRM_NOISE_MAX * 100:.0f}%) or a "
                      f"single pair still held ({best_pair:+.1f}%) -- not failing "
                      f"on this evidence")
                skipped += 1
            elif verdict == "regressed":
                failures.append(
                    f"{gate}/{name}: on-arm is {-win:.1f}% SLOWER than off over "
                    f"{nruns} runs (noise band -{band:.1f}%; on {on_ms:.0f}ms vs off "
                    f"{off_ms:.0f}ms wall). The default-on -f{gate} regressed this "
                    f"kernel, or is no longer on by default at -O3 -- re-measure by "
                    f"hand before touching this subject")
            else:
                measured += 1

    for f in failures:
        print(f"FAIL {f}")
    if failures:
        return 1
    if not measured:
        print(f"no gate could be timed reliably ({skipped} skipped); skipping")
        return 77
    print(f"\nruntime-bench: no gate regressed ({measured} gate(s), min wall of "
          f"{runs} interleaved runs on {slice_desc}, output verified vs "
          f"{os.path.basename(cc)})")
    return 0


BASELINE_DIR = os.path.join(RT, "baselines")


def baseline_key():
    """<cpu>-<os>, the same shape the journal/recorder baselines use. Kernel
    instruction counts are comparable across boxes of one key and meaningless
    across keys, which is why the file is keyed rather than global."""
    import platform
    m = platform.machine()
    m = {"aarch64": "arm64", "AMD64": "x86_64", "amd64": "x86_64"}.get(m, m)
    o = {"darwin": "darwin", "linux": "linux", "win32": "win32"}.get(sys.platform,
                                                                    sys.platform)
    return f"{m}-{o}"


def baseline_path(explicit):
    if explicit:
        return explicit
    return os.path.join(BASELINE_DIR, baseline_key() + ".json")


def baseline_snapshot(results, cols, cc):
    """Instructions only. Wall-clock and cpu ms are properties of the box on the
    day, so storing them would produce a file nothing can honestly diff; the
    instruction count is a property of the emitted code."""
    kern = {}
    for name, r in results.items():
        ins = r.get("ins", {}).get(cols[0])
        if ins:
            kern[name] = {"ins": ins, "ref_ins": r.get("ref_ins")}
    return {"key": baseline_key(), "cc": os.path.basename(cc),
            "config": cols[0], "kernels": kern}


def baseline_report(cur, path, tolerance):
    """Print the per-kernel instruction delta against a stored baseline.

    Returns the number of kernels that regressed past `tolerance` percent, or
    None when there is nothing to compare -- a missing file, a different key,
    or a run with no counter."""
    try:
        with open(path) as f:
            base = json.load(f)
    except (OSError, ValueError) as e:
        print(f"runtime-bench: no usable baseline at {path} ({e})")
        return None
    if base.get("key") != cur["key"]:
        print(f"runtime-bench: baseline is for {base.get('key')}, this host is "
              f"{cur['key']}; instruction counts are not comparable across keys")
        return None
    if not cur["kernels"]:
        print("runtime-bench: this run counted no instructions; nothing to diff")
        return None
    regressed, improved, rows = 0, 0, []
    for name in sorted(set(base["kernels"]) | set(cur["kernels"])):
        b = base["kernels"].get(name, {}).get("ins")
        c = cur["kernels"].get(name, {}).get("ins")
        if not b or not c:
            rows.append((name, b, c, None))
            continue
        d = (c - b) / b * 100.0
        rows.append((name, b, c, d))
        if d > tolerance:
            regressed += 1
        elif d < -tolerance:
            improved += 1
    print(f"\n{'kernel':<12}{'base(G)':>10}{'now(G)':>10}{'delta':>9}   (vs {path})")
    for name, b, c, d in rows:
        bs = f"{b / 1e9:>10.3f}" if b else f"{'-':>10}"
        cs = f"{c / 1e9:>10.3f}" if c else f"{'-':>10}"
        ds = f"{d:>+8.2f}%" if d is not None else f"{'-':>9}"
        print(f"{name:<12}{bs}{cs}{ds}")
    if improved:
        print(f"\nruntime-bench: {improved} kernel(s) improved by more than "
              f"{tolerance:.1f}% -- the baseline is now stale HIGH and the "
              f"ratchet is guarding a wider band than it could be. Re-bank it "
              f"with --write-baseline.")
    return regressed


def parse_gates(s):
    """One extra configuration, as compiler FLAGS on argv.

    This took `NAME=VALUE` environment knobs until the optimizer knobs moved to
    argv (`MCC_OPT_ROW` in src/mccopt.h). An env token is now read by nobody, so
    both arms of a `--gates` comparison compiled the IDENTICAL binary and the
    delta columns below reported the difference between one program and itself
    -- a +0.0% instruction column that is maximally convincing and completely
    empty. GATE_WINS was moved to flags when that bit it; this path was not.
    Refuse the retired spelling rather than measure nothing, the same way
    tools/selfhost-smoke.py refuses it."""
    flags = []
    for tok in (s or "").split():
        if not tok.startswith("-"):
            sys.exit("--gates takes compiler flags (-fno-chain-store), not the "
                     "retired NAME=VALUE environment spelling: %r reaches no "
                     "part of mcc, so both arms would compile the same binary "
                     "and the delta would read 0.0%% for every kernel" % tok)
        flags.append(tok)
    return flags


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
                    help="wall-clock no-regression ratchet for GATE_WINS: fail if a "
                         "default-on flag regresses its kernel (quiet host, pinned cpu)")
    ap.add_argument("--write-baseline", nargs="?", const="", default=None,
                    metavar="PATH",
                    help="record this run's instruction counts as the baseline "
                         "for this <cpu>-<os> (default tests/runtime/baselines/)")
    ap.add_argument("--baseline", nargs="?", const="", default=None,
                    metavar="PATH",
                    help="diff this run's instruction counts against a stored "
                         "baseline (default tests/runtime/baselines/<cpu>-<os>.json)")
    ap.add_argument("--assert-baseline", action="store_true",
                    help="with --baseline, exit 1 when a kernel regresses past "
                         "--baseline-tolerance")
    ap.add_argument("--baseline-tolerance", type=float, default=2.0,
                    metavar="PCT",
                    help="instruction regression allowed per kernel (default 2%%)")
    ap.add_argument("--json-out", metavar="PATH",
                    help="write the --json payload to a file as well as stdout")
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
                ref_ins = instructions_retired_min(ref, argv, backend)
                if ref_ins is not None:
                    results.setdefault(name, {})["ref_ins"] = ref_ins

            for gi, gates in enumerate(gate_sets):
                gflags = parse_gates(gates)
                exe = os.path.join(td, f"{name}.mcc{gi}")
                ok, err = build(args.mcc, src, exe, None, mcc=True,
                                flags=mflags + gflags)
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
                        ins = instructions_retired_min(exe, argv, backend)
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

    payload = {"results": results, "failures": failures}
    if args.json:
        print(json.dumps(payload, indent=2))
    if args.json_out:
        with open(args.json_out, "w") as f:
            json.dump(payload, f, indent=2)
        print(f"runtime-bench: wrote {args.json_out}")

    snap = baseline_snapshot(results, [g or "defaults" for g in gate_sets], cc)
    if args.write_baseline is not None:
        path = baseline_path(args.write_baseline)
        os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
        with open(path, "w") as f:
            json.dump(snap, f, indent=2, sort_keys=True)
            f.write("\n")
        print(f"runtime-bench: wrote baseline {path} "
              f"({len(snap['kernels'])} kernel(s), key {snap['key']})")
    if args.baseline is not None:
        regressed = baseline_report(snap, baseline_path(args.baseline),
                                    args.baseline_tolerance)
        if regressed is None:
            if args.assert_baseline:
                print("runtime-bench: nothing comparable; skipping")
                return 77
        elif regressed and args.assert_baseline:
            failures.append(f"{regressed} kernel(s) retired more than "
                            f"{args.baseline_tolerance:.1f}% extra instructions "
                            f"against the stored baseline")

    if not results:
        failures.append(
            f"{len(kernels)} kernel source(s) were present and NONE was "
            f"measured -- every one lost its reference build or reference run "
            f"above. Without this line the run prints 'OK (0 kernels ...)' and "
            f"exits 0, which reads as 'mcc's output was verified against "
            f"{os.path.basename(cc)}' when nothing was compiled with mcc at all")

    for f in failures:
        print(f"FAIL {f}")
    if failures:
        return 1
    print(f"\nruntime-bench: OK ({len(results)} kernels x {len(gate_sets)} config(s), output verified vs {os.path.basename(cc)})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
