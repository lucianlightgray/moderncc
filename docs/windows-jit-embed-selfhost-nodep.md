---
name: windows-jit-embed-selfhost-nodep
description: "mcc self-hosts an embed-JIT build with NO external windows.h — mccjit_win32.h now defines the 3 Vista sync primitives (SRWLOCK/CONDITION_VARIABLE/INIT_ONCE) locally, guarded by *_INIT macros"
metadata: 
  node_type: memory
  type: project
  originSessionId: 82bd4594-2214-403b-bbc5-50d2c3d937dd
---

**Fixed 2026-07-22.** Previously an mcc-self-hosted embed-JIT build (mcc
compiling mcc.c with `-DMCC_EMBED_JIT=1`) FAILED to parse `src/mccjit_win32.h`:
that header does `#include <windows.h>`, and mcc's curated
`runtime/win32/include/winapi` is a **pre-Vista mingw snapshot** that has
VirtualAlloc/CreateFileMapping/HeapAlloc/QPC/SwitchToThread but NOT the slim
reader/writer lock, condition variable, or one-time-init types — so
`static SRWLOCK ... = SRWLOCK_INIT;` errored (SRWLOCK undefined). This forced a
dependency on an external toolchain's windows.h and blocked the literal
self-host bootstrap on Windows.

**Fix (surgical, in `src/mccjit_win32.h` right after the include block):** define
just the used subset — `SRWLOCK`/`PSRWLOCK`/`SRWLOCK_INIT` +
Initialize/Acquire/ReleaseSRWLockExclusive; `CONDITION_VARIABLE` +
Initialize/SleepConditionVariableSRW/Wake/WakeAll; `INIT_ONCE` +
InitOnceExecuteOnce — each block **guarded by `#ifndef SRWLOCK_INIT` /
`#ifndef CONDITION_VARIABLE_INIT` / `#ifndef INIT_ONCE_STATIC_INIT`**. Every real
toolchain windows.h (mingw + MSVC) defines those `*_INIT` macros, so the blocks
are skipped verbatim there → **zero change to the normal gcc/cl build** (if the
guard failed to skip, gcc would error on the SRWLOCK redefinition — it built
clean, proving the skip). kernel32 already exports the entry points
(`runtime/win32/lib/kernel32.def` ~778-792, added for the pthread shim), so the
self-hosted link resolves. Types are opaque `{ PVOID Ptr; }` structs/union — the
same pattern `runtime/win32/include/pthread.h` uses (`__mcc_srwlock` etc.).

**Verified:** native x86_64-PE literal 3-stage bootstrap — mcc.exe → `mcc -O0
mcc.c` = stage1 (plain baseline, optimizer/JIT compiled out) → `stage1
-DMCC_CONFIG_OPTIMIZER=1 -DMCC_EMBED_JIT=1 mcc.c` = stage2 (1.18MB, JIT engine
baked in, **no external windows.h**) → `MCC_JIT=1 -O4 -run` on stage2 recompiles
the hot fn in memory from the AOT-submitted AST (`mccjit-override[busy]`),
output identical to the -O2 ref. Incremental gcc rebuild of build-verify + `ctest
-R 'jit/|regression/'` = **49/49** (incl regression/o4-aot-jit +
jit-submit-aot-diff). Note the target defines (OPTIMIZER/EMBED_JIT/JIT_DEFAULT)
come from CMake -D, NOT from -I$BLD, so a bare `mcc mcc.c` builds a compiler with
neither. See [[windows-jit-embed-port]], [[coff-reader-embed-blob]],
[[moderncc-windows-build]].
