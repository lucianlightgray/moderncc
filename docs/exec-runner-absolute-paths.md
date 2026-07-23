---
name: exec-runner-absolute-paths
description: "Manually invoking tests/exec/runner.c (exec_runner) needs ABSOLUTE mcc + tests-root args, else it fails after its internal `cd work`"
metadata: 
  node_type: memory
  type: reference
  originSessionId: afb91e73-4bbc-457e-b6ba-b198d532da59
---

When reproducing an `exec-*`/`exec-search-*` ctest failure by running the `exec_runner`
binary directly (bypassing ctest), pass **absolute** paths for the mcc binary (arg1) and
the tests-root (arg4). The runner builds `path = <tests-root>/<src>` then, for `dt`/`brun`/
`run2`/default modes, shells out with `cd "<workdir>" && <mcc> ... "<path>"` (tests/exec/runner.c
~L445-482). A relative mcc or tests-root resolves against the *workdir* after the `cd`, so you
get spurious `mcc: file '…' not found` / `sh: …/mcc: No such file or directory` that look like
compiler bugs but are just the manual-invocation cwd. Real ctest uses `$<TARGET_FILE:mcc>` +
`${CMAKE_CURRENT_SOURCE_DIR}/tests` (both absolute), so it never hits this.

Arg order: `exec_runner <mcc-abs> <bdir> <idir> <tests-root-abs> <outdir> --only <groupname>`.
Env for a variant: copy the `ENVIRONMENT` string from that variant's `add_test` in CMakeLists.txt
(e.g. emitiso = `MCC_TEST_OPT=-O4;MCC_AST_SEARCH=1;MCC_AST_SEARCH_EMITSIZE=1;MCC_AST_SEARCH_EMITISO=1`).
`--only <groupname>` selects one goldens.h group; the group's `mode` (dt/brun/run2/…) decides
whether it runs the compiled program or `-dt -run`s the whole multi-test file in one process.
