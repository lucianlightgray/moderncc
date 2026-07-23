---
name: moderncc-windows-sanitizers
description: What sanitizer tech actually works for mcc on Windows (MSVC ASan / mingw trap-UBSan) and the alignment-UB gotcha
metadata: 
  node_type: memory
  type: project
  originSessionId: 10dce1d5-17da-4840-a3da-ec59de680bd3
---

`MCC_BUILD_SANITIZE` (the `sanitize` preset, builds the extra `mcc_s` exe) was a
configure-fatal on WIN32 + any non-GCC/Clang host. That was broader than the real
toolchain limits. Enabled 2026-07-08 (commit `ccba8d22`). What's actually available
on this Windows host:

- **MSVC `cl /fsanitize=address`** — AddressSanitizer WORKS. Runtime DLL ships with
  VS 2026 at `<VC>/Tools/MSVC/<ver>/bin/Hostx64/x64/clang_rt.asan_dynamic-x86_64.dll`
  (== `dirname(cl.exe)`), must be on PATH to *run* an ASan exe. Add `/Zi`+`/DEBUG`
  to silence C5072/LNK4302 and symbolize reports. MSVC has **no UBSan/TSan/MSan** on
  Windows — ASan is the whole story.
- **mingw (CLion 13.1 AND vendored winlibs 16.1)**: ship **no libasan/libubsan**
  (`gcc -print-file-name=libasan.a` returns the bare name = not found; `-fsanitize=address`
  → `cannot find -lasan`). So **ASan is impossible on mingw** — a genuine toolchain
  gap, not fixable in the build. BUT **trap-mode UBSan links with no runtime**:
  `-fsanitize=undefined -fsanitize-trap=undefined` (emits `ud2` on UB, SIGILL/exit 132).
- **KEY GOTCHA — mcc has real alignment UB**: a trapping-UBSan mcc SIGILLs when
  compiling anything that `#include <stdio.h>` (the preprocessor/tokenizer does
  unaligned loads — a benign, intentional tcc idiom, safe on x86/x64/arm64). Bisected
  to exactly `-fsanitize=alignment`; `-fno-sanitize=alignment` → a clean, *runnable*
  sanitized mcc. So mcc_s uses `-fno-sanitize=alignment` on ALL platforms (ELF too),
  else a runnable UBSan build is noise. (`complex_annexg` with a bare `extern printf`
  does NOT trip it — you need the header path to hit it.)

Design landed:
- `mcc_validate_config`: PROFILE still needs GCC/Clang; SANITIZE also accepts MSVC;
  dropped the WIN32 fatal. Flavor resolved where `mcc_s` is defined (CMakeLists
  ~2211): MSVC→`/fsanitize=address /Zi`; mingw+libasan→ASan+UBSan−align;
  mingw no-libasan→trap-UBSan−align; ELF/Mach→ASan+UBSan−align. Guard the
  `-lm`/Threads/dl links to non-WIN32 (mcc.c is self-contained on PE).
- **`mcc_s` was only ever BUILT, never run by ctest** (`_mcc_exe` is always the
  normal `mcc`). Added a `sanitize-smoke` ctest (`tests/sanitize/{smoke.c,run_sanitize.cmake}`,
  gated on `TARGET mcc_s`) that runs mcc_s to compile+link+execute a header-free
  program — the one place the sanitizer actually executes. Prepends `_san_asan_bindir`
  to PATH for the MSVC ASan DLL.
- New **`sanitize-msvc`** preset (VS gen + `/fsanitize=address`); `sanitize` preset
  stays mingw (trap-UBSan). `MCC_ALL_DIAGNOSTICS` now also builds mcc_s on mingw.
- Gotcha: `mcc_profile_seed` only seeds on a FRESH configure — to re-test the
  diagnostics preset's mcc_s you must delete `cmake-diagnostics/` first (flipping an
  existing cache to ON doesn't re-seed). Same trap noted in [[moderncc-windows-build]].
- Stray `vc140.pdb`: the VS generator drops it in the repo root during custom build
  steps; added to `.gitignore`.

Verified green: sanitize (mingw) 1416/1416, sanitize-msvc 1414/1414, diagnostics
1416/1416, Docker linux-gcc-sanitize 1483/1483 (smoke clean, no leaked reports).
Not done (out of scope / not possible): mingw ASan (no runtime exists); the vendored
LLVM clang (windows-msvc) *could* add a third ASan+UBSan path if ever wanted.
See [[moderncc-windows-build]].
