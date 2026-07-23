---
name: mcc-dynamic-onesource
description: mcc-dynamic is now a self-contained single-source exe (was: needed MCC_ONE_SOURCE=OFF + linked libmcc)
metadata:
  node_type: memory
  type: project
  originSessionId: 3f7b48ce-5f42-4712-97ef-6491079b8f3e
---

**Current state (verified 2026-07-16, `CMakeLists.txt:2037`):** `mcc_dynamic`
is now a **self-contained single-source** executable — it compiles `src/mcc.c`
directly (with `mcctools.c` as an OBJECT_DEPENDS), links only `m`/Threads/dl,
and does **not** link `libmcc`. It is gated on plain `if(MCC_BUILD_DYNAMIC_EXE)`
— no `NOT MCC_ONE_SOURCE` condition. It's simply "the dynamic counterpart to
`mcc-static`" (dynamically linked to libc, vs `mcc-static`'s `-static`). The
CMake option is `MCC_SINGLE_SOURCE` (CMake) / `MCC_AMALGAMATED` (Makefile);
`MCC_ONE_SOURCE` no longer exists as the CMake name.

This means the README once described mcc-dynamic inverted ("non-amalgamated
driver linked against `libmcc.so`") — fixed in the 2026-07-16 doc audit.

**Historical (no longer applies):** mcc-dynamic *used* to be the non-amalgamated
driver that linked `libmcc` and called libmcc-internal `ST_FUNC` helpers
(`pstrcpy`, ...), which have external linkage only in a multi-TU libmcc — so it
required `MCC_ONE_SOURCE=OFF`. That was the original Windows/msvc
"pstrcpy LNK2019 in mcc_tool_impdef" CI failure. The target has since been
refactored to the single-source form above, dissolving that dependency. See
[[moderncc-windows-build]] for the docker validation loop.
