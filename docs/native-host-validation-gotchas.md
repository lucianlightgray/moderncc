---
name: native-host-validation-gotchas
description: "Native arm64 macOS host — the 7 exec/ fails + the cross-build ast_alloc_loc break are now BOTH FIXED (2026-07-16); historical context only"
metadata: 
  node_type: memory
  type: project
  originSessionId: 8aaf941c-37d2-40c4-9669-2e5fabff1ecc
---

Both gotchas noted 2026-07-15 on the native arm64 macOS dev host are now **RESOLVED** (2026-07-16, CI #846/#847 recovery). Kept for historical context — do not re-apply the old "don't chase" guidance.

**FIXED — the 7 native `exec/` failures were the arm64 far-libc `R_AARCH64_CALL26` bug, not env noise.** `builtins`/`atomic_misc`/`bound_global`/`bound_test_b`/`run_atexit`/`errors_and_warnings`/`nodata_wanted` all failed because `mcc -run` mmaps run-memory tens of GB from libc and a direct `bl` (±128MB) can't reach it. Fixed by `arm64_veneer_memory_calls` (commit `95980d3a`, [[arm64-run-far-call-veneer]]): arm64 MEMORY-output CALL26/JUMP26 to out-of-run-memory targets (SHN_UNDEF dlsym / SHN_ABS host addrs) now route through a `.mcc.veneer` `ldr x16,[pc,#8];br x16;.quad` stub. `ctest -R '^exec/'` is now 297/297 on this host; replay/vlat/diff/basic 593/593; jit-selftests 32/32. A codegen-regression check here is now a clean 297/297 baseline, not 290/297.

**FIXED — the cross-run `ast_alloc_loc` build break.** `gfunc_return`'s `ast_alloc_loc` call is now `#if MCC_CONFIG_OPTIMIZER`-guarded with the plain loc-decrement fallback (commit `f5cdcce9`), so cross targets (MCC_INTERNAL/optimizer off) link again — native x86_64/riscv64 cross-run differential testing is unblocked. Related: [[arm64-linux-runtime-libcalls]] (separate Linux-CI libcall issue), [[docker-amd64-repro]] (qemu-amd64 emulation noise is still real — run failing emulated tests individually).
