---
name: cli-runner-env-filters
description: "Running cli_runner directly (not via ctest) skips the elf/os/cpu filter tags, producing spurious \"failures\""
metadata: 
  node_type: memory
  type: reference
  originSessionId: 50380c9a-e3de-41d4-8d8f-7f8c2a25aa75
---

The cli test cases in `tests/cli/cases.h` have a filter tag as their 2nd field
(e.g. `"elf"`, `"os!=WIN32"`, `"cpu=x86_64,os=linux,asm"`). These are evaluated
against `MCC_TEST_*` env vars (MCC_TEST_OS, MCC_TEST_CPU, MCC_TEST_ASM, ...).

Running `cli_runner <mcc> <B> <inc> <work> <cases>` **directly** does NOT set
those env vars, so filter-gated cases that should be skipped instead RUN and can
"fail" — e.g. on arm64/macOS, `atomic_rmw_unsupported` and
`atomic_inlang_aggregate` (both `"elf"`-gated) and target-specific cases appear
as failures. These are false: under `ctest` (which sets MCC_TEST_OS=Darwin etc.)
they correctly Skip, and `symbol_type_func_object` passes. Confirmed the full
`ctest -R '^cli/'` suite is 100% green (279 tests) on this host.

Takeaway: to check regressions locally, run via `ctest -R '^cli/'` (or set the
MCC_TEST_* env before cli_runner). A quick per-case check with cli_runner
`--only <name>` is fine for cases with no filter tag, but treat any "FAIL" from a
direct full-suite cli_runner run as suspect until reproduced under ctest.
Related: [[exec-runner-absolute-paths]], [[macos-arm64-status]].
