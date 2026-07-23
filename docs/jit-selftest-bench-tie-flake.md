---
name: jit-selftest-bench-tie-flake
description: "jit/selftest-bench tie-case flake is contention noise; fix is many SHORT bench rounds (min converges), not longer iters"
metadata: 
  node_type: memory
  type: project
  originSessionId: 0e23b11d-c35b-4529-991e-a7ec2049ddef
---

`jit/selftest-bench`'s tie case (slow_fn vs slow_fn, expect incumbent-wins) false-promotes under the CPU contention of a parallel `ctest -j` run. `mccjit_bench_pair` measures candidate then incumbent in separate sequential wall-clock windows and takes best-of-N (min); under contention whichever window is less contended can win by >the 10% hysteresis margin, flipping the tie to a false promote.

**Wrong lever:** lengthening the measurement window (commit 5be8343e raised `MCC_JIT_BENCH_ITERS` 3000→60000). The noise is *proportional* contention, not a fixed-duration stall, so a longer window is LESS likely to catch a fully-uncontended slice. Stayed red on macos-cross/gcc/arm64; reproduces locally at 8/25 tie-fails even at iters=60000.

**Right levers:** (a) MANY SHORT rounds — min-of-N converges to the uncontended runtime as N grows, and short runs catch a clean slice far more often; (b) a hysteresis band wider than the CI noise floor. Added `MCC_JIT_BENCH_ROUNDS` env knob (default 3 → production/`l4a`/`benchwire` unchanged). rounds=25 margin=10 held on this Apple-Silicon host (0/100 local) but **CI #1038 (linux-gcc-multisource / x86_64) still false-promoted the tie** — the GitHub x86_64 runner is far more oversubscribed under `ctest -j` and its min-of-25 asymmetry exceeded the 10% band. Fix (commit 3e1e95a1): selftest now uses iters=4000 **rounds=51 margin=30** — band ~3x above CI noise, ~2.8s. The 30x-gap faster/slower assertions are never at risk (only the sign matters). The flake is x86_64-CI-specific and does NOT reproduce natively here (8-way spinner stress = 0 fails on BOTH old and new binaries), so local runs only prove the fix doesn't break the test; green CI is the real proof. Deferred structural fix: paired per-round ratio + median in `mccjit_bench_pair` would cancel common-mode contention (env-independent) but changes production promotion semantics + other benches, needs own soak.

**Repro the flake deterministically:** build `jit_selftest_bench` (embedjit config), spawn `sysctl -n hw.ncpu` `while :; do :; done` spin loops, then loop the binary and grep `equal candidate ... FAIL`. Harsher than real CI. Faster/slower assertions (30x gap) were never at risk. See [[embedjit-x86-test-rosetta]], [[mcc-jit-unification]].
