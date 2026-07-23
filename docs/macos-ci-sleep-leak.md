---
name: macos-ci-sleep-leak
description: macOS CI BAD_COMMAND storms = fuzz/diff3 timeout fallback leaking orphaned sleep procs → RLIMIT_NPROC
metadata: 
  node_type: memory
  type: project
  originSessionId: 9bd9e324-1267-4171-9bfa-1e5adcd69786
---

macOS CI cells failing with a large `BAD_COMMAND` cascade (ctest can't fork/exec the test binary) across unrelated cells (preprocess/parts/jit/regression) + starvation-induced fuzz "divergences" = the runner exhausted `RLIMIT_NPROC`. Root cause (fixed commit a0357638, 2026-07-20): the fuzz (`tests/fuzz/runner.c`) and diff3 (`tests/diff3/runner.c`) `timeout_wrap` shell fallback — reached because macOS CI brew-installs only cmake/ninja/gcc/mingw/wine, **no coreutils, so no `timeout`/`gtimeout`** — wrapped every compile/run in `( sleep N; kill $cmd ) & __w=$!; ... kill $__w`. Killing the watcher SUBSHELL leaves its `sleep 20` child orphaned for 20s; thousands of wrapped invocations under `--gates` pile up hundreds of concurrent orphans → process-table exhaustion. Fix: background `sleep` directly (`sleep N & __s=$!`) and kill it by its own PID.

Debugging notes: `BAD_COMMAND` (not Failed/Timeout) = ctest itself couldn't spawn the test — a runner-level resource problem, NOT a test bug; in-test harness retries can't help (they never run). Repro the leak locally on macOS (also lacks gtimeout): run the fallback in a loop, `pgrep -x sleep` before/after. host_spawn_ex (parts/cli, src/mcchost.c) is clean — polls with in-process usleep + kills the process group. If starvation recurs from raw concurrency (not the leak): ctest `RESOURCE_LOCK` across fuzz/parts, or add coreutils to the macOS brew install. Related: [[docker-macos-tmp-mount]], the macOS-starvation TODO item.
