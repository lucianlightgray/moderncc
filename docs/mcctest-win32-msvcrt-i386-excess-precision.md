---
name: mcctest-win32-msvcrt-i386-excess-precision
description: "Why WIN32 mcctest mismatches mean differing bits (shared msvcrt printf), and the i386 x87 excess-precision fix"
metadata:
  node_type: memory
  type: project
  originSessionId: 88984382-c60f-4e42-9204-661a0dd031c3
---

A `mcctest mismatch (cc vs mcc)` that reproduces **only on i686 / only on a
new CI image** (e.g. `windows-2025-vs2026`, UCRT 10.0.26100) while x86_64 is
green is almost certainly **i386 x87 excess precision**, not a codegen
regression. Ruled out the 11 suspect commits by proving mcc's i386 `.text`
for `full_language.c` was **byte-identical** to the parent (only the PE
timestamp + embedded githash differed) and codegen was deterministic
(bit-identical ×5).

Key WIN32 facts that shape the diagnosis:
- On WIN32 both the reference (`msvcrt_start.c` + `-lmingwex` +
  `c:/windows/system32/msvcrt.dll`, built with `-D__USE_MINGW_ANSI_STDIO=0`)
  **and** the mcc binary resolve `printf` to the **same frozen `msvcrt.dll`**.
  So a mismatch can't come from printf/formatting — it can only come from a
  compared value whose **bits differ** between cc and mcc. (Verified: `%f`,
  `%g`, ull→float/double conversions all give bit-identical output cc vs mcc.)
- `full_language.c` gates its whole libm/`sprintf`-float/transcendental suite
  (`s7_6`,`s7_12`,`s7_21`,`s7_22`,`s7_23`) behind `#if MCC_HAS_C99_LIBM`, which
  is **0 on WIN32** — those lines are never compiled/compared there. Don't
  chase them.
- The remaining differing-bits axis on i386 is **x87 80-bit intermediates**:
  gcc mingw defaults to `FLT_EVAL_METHOD=2`, but the corpus asserts
  `FLT_EVAL_METHOD==0` and mcc targets that; x86_64 uses SSE (unaffected).

Fix landed (commit 31385e21): append `-fexcess-precision=standard` to the
i386 WIN32 mcctest refflags in `CMakeLists.txt` (~line 3799) — rounds gcc's
intermediates to declared precision, aligning the reference with the corpus
model and mcc; no-op on SSE targets. Speculative for the exact line (only
repro's on the image). Also landed a mcctest diagnostic (commit 9367bae3):
`mcctest_report_diff` in `tools/mccharness.c` now prints the first differing
line/byte + both renderings on mismatch, so the next CI run self-localizes.
See [[mcctest-ast-promote-debug]] and [[ci-log-zip-commit-ahead]].
