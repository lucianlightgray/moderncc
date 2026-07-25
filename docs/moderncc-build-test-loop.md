---
name: moderncc-build-test-loop
description: "How to build and test moderncc incrementally (mcc target, asttool selftests, ctest)"
metadata: 
  node_type: memory
  type: project
  originSessionId: 1dc0b998-1252-43b3-bd9b-3ae613ca9127
---

Fast edit→build→test loop for moderncc:
- Build main compiler: `cmake --build cmake-debug --target mcc -j` (the `cmake-debug` dir is configured; ninja incremental is seconds).
- AST-internal unit selftests live in `tools/asttool.c` (`#include "mccast.c"` directly, so it sees statics + needs VT_* fallbacks since it compiles the non-`MCC_INTERNAL` region without mcc.h). Framework = `CHECK(cond, "msg")`; suites dispatched by `argv[1]`. Build: `--target asttool`; run `./cmake-debug/asttool <suite>` or all suites with no arg.
- Register a new suite in BOTH `tools/asttool.c` main dispatch AND the `foreach(_an ...)` list in `CMakeLists.txt` (~line 3292), then `cmake -S . -B cmake-debug` to register `ast/<name>` as a ctest.
- Run tests: `ctest --test-dir cmake-debug -R "^ast/" --output-on-failure`. The `mcc_build` fixture (~66s) rebuilds mcc before dependent tests. Broader suites: `exec/*`, `diff/*`, `abidiff`, `selfhost`.

Related: [[slice-cache-refactor]].
