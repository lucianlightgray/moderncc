---
name: mingw-i686-cross-width-jit
description: The mingw i686 CI cell now targets native i386 (was a mis-targeted x86_64 cross); its i386-Windows codegen gaps and fixes
metadata: 
  node_type: memory
  type: project
  originSessionId: 427be5c9-d40e-4f7d-8f07-d5a737295bbd
---

The experimental **mingw i686** CI cell (`MCC_MINGW_ARCH=i686`, from the
"unified windows job" refactor) builds mcc as a **4-byte i686 binary that still
targets x86_64** — `MCC_MINGW_ARCH` only selects the 32-bit winlibs *toolchain*;
`MCC_CPU` derives from the x86_64 runner's `CMAKE_SYSTEM_PROCESSOR`, not the
toolchain width. So it's a legit cross-width host (can't JIT-run x86_64 code in
a 32-bit process).

`src/mcc.h` defines `MCC_TARGET_IS_HOST` only when the backend CPU macro matches
the arch mcc runs on. On this cell it's undefined, so the whole body of
`src/mccrun.c` (the in-process JIT run API: `mcc_run`, `mcc_relocate`) compiles
to nothing. The embed test exes (libmcc_test/-extra/-mt, abitest, mcchv) call
that API, so they died at link: `undefined reference to mcc_relocate` — halting
the i686 build.

**Fix (commit f925c9de):** derive a CMake `MCC_TARGET_IS_HOST` = (target-CPU
pointer width == `CMAKE_SIZEOF_VOID_P`), gate those run-API test targets on it,
skip with a reason on a cross-width host. Native builds unchanged. Also fixed a
32-bit UB the cell surfaced: `(long)1 << 60` sentinel in `ast_search_select`
(mccast.c) → `LONG_MAX` (sorts last on any width; 64-bit order byte-identical).

The cell is `continue-on-error` / `matrix.experimental` (ci.c PLAN_MINGW
`{"i686", 1}`), so it's non-gating until i386-Windows codegen is proven; this
fix lets it build + run ctest, a prerequisite for promoting it off experimental.

**Retarget to native i386 (commit 9c4b3ff9)** — the real fix. Targeting x86_64
was an oversight: `MCC_MINGW_ARCH=i686` selected the toolchain but not the target.
Made `MCC_MINGW_ARCH=i686` imply `MCC_TARGET_ARCH=i386` (CMakeLists ~line 1244,
before `_proc`), so the cell is a genuine **native i386 compiler**:
MCC_TARGET_IS_HOST holds, `-run` works, embed tests build+run (the f925c9de gate
no longer trips there — still a valid guard for true cross-width configs). Full
i386 ctest went 45 fail → 12 → **3557/3557**. Three real i386-Windows gaps fixed:
- **copysignf/copysignl**: 32-bit msvcrt exports `_copysign` but NOT `_copysignf`;
  mcc still emitted the import (`msvcrt.def` lists `_copysignf`), so non-folded
  `copysignf` calls failed to resolve at -run/load (exec/libm_builtin_fold).
  Mapped both to `__builtin_*` inline forms in `runtime/win32/include/math.h`
  (gcc-like, no libcall). x86_64-Win unaffected (also inlines).
- **_Alignas over-aligned stack locals**: the `overalign_indirect` path (route an
  over-aligned local through an aligned alloca) was gated on `STACK_OVERALIGN_MAX`,
  left UNDEFINED for i386-PE (`mccgen.c` ~11188: `MCC_TARGET_I386 && !MCC_TARGET_PE`).
  Enabled it for i386-PE (=8), and made the PE `gen_vla_alloc` honor alignment:
  i386 `__alloca` returns a 4-aligned ptr, so `and esp, -align` (i386-gen.c) for
  align>8. Caught by mcctest full_language `overalign` check.
- **host-detect** compares derived MCC_CPU vs the *native host* CPU
  (`mccbuild --detect` → `host_sys_info`), which differs for an i386 target on an
  x86_64 runner. Gated on `MCC_TARGET_ARCH` being unset (only validates
  auto-detection). NOT a local-repro artifact — it fails in the real cell too.

Repro locally without the superbuild: leaf-configure with the vendored i686 gcc
(`vendor/winlibs-mingw-w64-16.1.0-ucrt-i686/mingw32/bin/gcc.exe`) as
`CMAKE_C_COMPILER` PLUS `-DMCC_MINGW_ARCH=i686` (or `-DMCC_TARGET_ARCH=i386`) —
same derivation as the inner cell. See [[moderncc-windows-build]],
[[ci-log-zip-commit-ahead]].
