---
name: moderncc-windows-port-fixes
description: Windows/mingw bugs fixed in moderncc to get ctest green (2026-06-29)
metadata: 
  node_type: memory
  type: project
  originSessionId: 12bd2d8f-e136-4d9c-86c5-02aa3b2aea1f
---

Getting the `mcc` ctest suite green on Windows/mingw (34/34 pass; exec golden
236 pass / 23 skip) required these fixes — useful context if any regress:

Compiler/runtime (affect generated code, guarded for non-PE):
- `src/libmcc.c` `mcc_add_library`: on PE, `-lm` is a no-op (math is in msvcrt).
- `runtime/include/mccdefs.h`: dropped the `_WIN32` special-case so `__builtin_*`
  (memcpy/strcpy/abort/…) are declared on PE too.
- `src/mccpp.c` + `src/mccgen.c`: `U"…"`/`U'…'` (char32_t) used `nwchar_t` width,
  which is only 2 bytes on PE → truncated/mis-stored. Now re-encoded to explicit
  4-byte units, recombining UTF-16 surrogate pairs.
- `runtime/include/float.h`: PE/apple-arm64 (`long double == double`) now report
  64-bit LDBL_* macros instead of x86 80-bit (which underflowed to 0).
- `runtime/include/tgmath.h`: when `LDBL_MANT_DIG == DBL_MANT_DIG`, drop the
  duplicate `long double`/`long double _Complex` `_Generic` arms.

Build/test infra:
- `CMakeLists.txt`: on WIN32 place `libmcc1.a` + support objs in `{B}/lib` (PE
  search path) and copy win32 headers/`.def` into the build tree; skip `cli-suite`
  and X11 `compile.ex4`; guard `diag-coverage` on the (missing) script; replace
  external `diff` with `cmake -E compare_files` in mcctest/asm-c-connect.
- `tests/exec/runner.c` & `tests/cli/runner.c`: portable `mkdir` (no `mkdir -p`);
  wrap popen commands in an extra quote pair (cmd.exe `/c` quote-stripping); treat
  CR as whitespace; map POSIX `'…'` arg quoting to `"…"`; add `os!=` req token.
- `tests/embed/api_extra.c`: ported off `<sys/wait.h>`/`fork`.
- `tests/exec/goldens.h`: gated pthread/threads, LP64-assuming, and complex/fenv
  libm tests with `os!=WIN32:<reason>`.

Second pass (2026-06-29) — qemu-user-in-Docker + MSVC host build:
- `.gitattributes` (new): force `*.sh`/Dockerfile to `eol=lf`. With `core.autocrlf=true`
  the qemu-docker entrypoint `tests/qemu/docker/run-matrix.sh` got CRLF, so its
  `#!/usr/bin/env bash\r` shebang made the container exit 127. All `tests/qemu/*.sh`
  were CRLF too. (`grep $'\r'` is unreliable in this msys; trust `file`.)
- `CMakeLists.txt` tags glob (~2610): dropped `CONFIGURE_DEPENDS` from the whole-tree
  `GLOB_RECURSE`. It made the Ninja generator watch every dir under the source root —
  including in-source build dirs (build-*/) holding generated .c/.h — causing spurious
  "GLOB mismatch" reconfigures and, on Windows, a ninja doubled-path stat crash
  (`FindFirstFileExA(<src>/<abs-src>/...)`). Also broadened its EXCLUDE to `build-[^/]*`.
- MSVC host build now works (`-G "Visual Studio 18 2026" -DMCC_TOOLCHAIN_PROFILE=msvc`,
  31/33 ctest pass, rest skipped). Fixes: `src/mcc.h` `_MSC_VER` block `#include
  <sys/types.h>` (off_t; mingw pulls it via <io.h>, MSVC doesn't); msvc profile no
  longer forces `MCC_ONE_SOURCE=ON` (it made ST_FUNC `static`, so mcc.exe's impdef
  ref to pstrcpy went unresolved); WIN32 `-s` strip link flag now skipped for msvc
  (LNK4044); `tests/{exec,cli}/runner.c` map `popen/pclose`→`_popen/_pclose` for
  `_MSC_VER`; `mcctest`/`mcctest-bcheck` skipped under MSVC (differential test needs a
  gcc-compatible reference cc for mcc's `__SIZE_TYPE__` headers + gnu99/-O0 flags).
- Superbuild matrix is now msvc-aware: `msvc` cells use the VS generator (auto-detected
  via vswhere → `MCC_MSVC_GENERATOR`/`MCC_MSVC_PLATFORM`), skip the cl-path requirement,
  and run `ctest -C <cfg>`. Ninja+cl is avoided (needs vcvars + hits the glob bug).

Third pass (2026-06-29) — `MCC_DIAGNOSTICS=ON` on an MSVC host (`CMakeLists.txt`):
- The flag unconditionally seeded `MCC_BUILD_SANITIZE/COVERAGE/PROFILE` (mcc_s/mcc_c/mcc_p,
  built with -fsanitize/gcov/-pg), which `mcc_validate_config` rejects on a non-GCC/Clang
  host → MSVC configure aborted. Now the seeding is guarded to `CMAKE_C_COMPILER_ID
  MATCHES "GNU|Clang"`; on MSVC diagnostics still turns on the portable half (warnings +
  debug info) and logs that it skipped the sanitize/coverage/profile variants.
- `MCC_DIAG_FLAGS` was a GNU `-Wall …` list. cl.exe *accepts* `-Wall` but aliases it to
  `/Wall` (floods with system-header noise). Now MSVC gets `/W4 /Zi` (no `/WX` — surface,
  don't fail). With this, `-DMCC_DIAGNOSTICS=ON` MSVC build is 0 errors / 202 benign /W4
  warnings (C4201 anon-union ×126, C4100 unused-param, C4324 align-pad, C4701/C4702
  flow-analysis FPs, C4295 = the intentionally non-NUL-terminated `ar` header fields in
  `src/mcctools.c`). 33/33 ctest. qemu-docker matrix re-verified green: 21/21.

Fourth pass (2026-06-29) — TODO.md items implemented + verified natively:
- **PE static TLS** (the big one; `_Thread_local` segfaulted on PE). Windows TLS is
  unrelated to the SysV thread pointer: reach a thread-local via the TEB
  (x64 `%gs:0x58`, x86 `%fs:0x2c` = ThreadLocalStoragePointer) → `[_tls_index]` →
  index*8/4 → block base → `+ template offset`. Three parts: (1) `src/objfmt/mccpe.c`
  `pe_add_tls`/`pe_set_tls` synthesise `_tls_index` (use **set_elf_sym** not
  put_elf_sym so the codegen's undef ref resolves), a NULL callback array, and a
  populated `IMAGE_TLS_DIRECTORY` → DataDirectory[9]; (2) `src/arch/{i386,x86_64}/
  *-gen.c` emit the TEB sequence for load/store/&tls (guarded `#ifdef MCC_TARGET_PE`);
  (3) `*-link.c` PE branch of `R_X86_64_TPOFF32`/`R_386_TLS_LE` = `val - tls_start`
  (offset into template), not tp-relative. Two gotchas burned: **PE layout never sets
  `s->sh_size`** — use `data_offset` (fallback `sh_size ? sh_size : data_offset`); and
  tcc's **`o(0)` emits NOTHING** (it shifts-while-nonzero) so a zero disp8 needs `g(0)`.
  Default x86/x64 exes are non-relocatable so the directory's absolute VAs need no base
  relocs (ASLR/DLL would). Verify natively: `mcc tls.c -o t.exe && ./t.exe` (x64); build
  `i386-win32-mcc` via `-DMCC_ENABLE_CROSS=ON` and run the 32-bit exe under WoW64.
- **msvcrt conformance goldens** made libc-aware: `varargs_fp` accepts ≥3-digit
  exponent, `floats_libc` accepts half-away rounding, `libc_struct` resolves `lldiv`
  via `GetProcAddress` on `_WIN32` (manual fallback) so wine's builtin msvcrt (no
  `lldiv`) doesn't abort at load. glibc/musl rows unchanged.
- **CI**: `.github/workflows/ci.yml` — portable ctest (gcc+clang) + qemu-docker matrix.
- Native PE verification trick: this host *is* Windows x64, so `mcc`-built PE exes run
  directly (no wine); 32-bit via WoW64. Much faster than the wine path.

Fifth pass (2026-06-30) — enable mcctest/cli-suite on Windows + a real PE bug:
- **`mcc -M` (deps-only) named the dependency-rule target `foo.exe:` on PE** instead
  of `foo.o:` (gcc/mingw and every other mcc target use `.o`). Root cause in
  `src/mcc.c` `default_outputfile`: the `#ifdef MCC_TARGET_PE` `output_type==EXE →
  .exe` check ran *before* the `just_deps||OUTPUT_OBJ → .o` check, and bare `-M`
  leaves output_type at its EXE default. Fixed by making the `.o` (deps/obj) case
  win first; the PE .dll/.exe branch is now the `else`. Non-PE behaviour identical.
  Caught by the newly-enabled cli-suite `deps_M_rule` case.
- **cli-suite enabled on WIN32**: `tests/cli/runner.c` `run_capture` routed through
  cmd.exe via popen() (no POSIX tools). Added `shell_popen()` (_WIN32 only): writes
  the pipeline to `<work>/_clicmd.sh` and runs `"sh" "script"` (outer-quoted for
  cmd.exe's one-pair strip). `timeout_prefix` probes via sh too. Added `os!=NAME`
  req token (mirrors exec runner). cases.h: `string_init_element_mismatch` gated
  `os!=WIN32` — `int b[]=L"abc"` is clean only where wchar_t==int; PE's 16-bit
  wchar_t makes it legitimately warn. CMakeLists registers cli-suite when a shell
  is found (`MCC_TEST_SH`), no longer hard-skipping WIN32.
- **mcctest/-bcheck enabled under MSVC host** via `MCC_REF_CC` (see
  [[moderncc-windows-build]]). CMakeLists: refflags on WIN32 are always the
  gcc-as-reference flags now (the dead cl-as-reference sub-branch removed); the
  test uses `_ref_cc`/`_ref_cc_name`/`_ref_cc_major` (probed `-dumpversion`).
- MSVC `/W4 /Zi` clean build = 0 errors, 205 warnings, all benign (anon-union
  C4201 x126, padding, unused-param, intentional ar fields, TCC flow-analysis FPs,
  negative-addend→Elf64_Addr, -run ptr casts). No real correctness bug. Both
  Windows flavors (MSVC + plain mingw) now 33/33 with 9 skips.

Sixth pass (2026-06-30) — three-toolchain max-diagnostics run + un-gating tests:
- **`MCC_DIAGNOSTICS=ON` was broken on a fresh mingw/PE configure**: it seeded
  `MCC_BUILD_SANITIZE` (mcc_s, `-fsanitize=address,undefined`), but mingw GCC
  (both CLion 13.1 and winlibs 16.1) ships **no libasan/libubsan** (`cannot find
  -lasan/-lubsan`), so the mcc_s link aborted the build. `--coverage` and
  `-pg -static` DO work on mingw. Fix in `CMakeLists.txt`: the diagnostics seed
  now skips SANITIZE when `WIN32 OR MCC_CONFIG_MINGW32` (still seeds COVERAGE +
  PROFILE), and `mcc_validate_config` fatals on `MCC_BUILD_SANITIZE AND
  MCC_TARGETOS==WIN32`. Fresh diagnostics build now green on CLion-mingw and
  winlibs-gcc: builds mcc_c (coverage, no .exe suffix — built by running mcc) +
  mcc_p, skips mcc_s. (The earlier "mingw diag worked" was an artifact: flipping
  an existing cache to ON doesn't re-seed, so the variants stayed OFF.)
- **`pe-native-conformance` (new test, `tests/qemu/run_pe_native.cmake`)**: on a
  native WIN32 host the default mcc already targets x86_64-win32 and its PE exes
  run directly, so the whole `tests/qemu/conformance/*.c` corpus (16 programs)
  runs natively — no wine, no win32 cross compiler. All 16 PASS. Registered when
  `MCC_TARGETOS==WIN32 AND NOT CMAKE_CROSSCOMPILING` (else a documented skip,
  like pe-wine/macho). `-B${B} -I${B}/include -I.../win32/include -I.../include`.
  pe-wine-conformance still skips on native Windows (it's the Linux/wine path);
  its coverage is now provided natively.
- **`expect_win32` golden field** (`tests/exec/goldens.h` + `runner.c`): optional
  8th field `*expect_win32`; the exec runner uses it instead of `expect` when
  `MCC_TEST_OS==WIN32`. Lets ABI-specific codegen goldens run on WIN32 (LLP64,
  long==4) instead of skipping. Un-gated 3 from `os!=WIN32`: `integer_constants`
  (widths line 4 4 4 4 8 vs LP64 4 4 8 8 8), `struct_byval` (S3l: three 4-byte
  longs), `cast_operator` (NB: its WIN32 golden must include the
  `cast between pointer and integer of different size` warning — pointer 8 >
  long 4). exec-suite now 242 pass / 0 fail / 22 skip (was 239/25). 7-field
  initializers leave expect_win32 NULL → unchanged on Linux/Darwin.
- **Three build flavors, all 37/37 (25 pass, 12 skip, 0 fail) with diagnostics**:
  `cmake-windows-mingw` (CLion gcc), `cmake-windows-gcc` (winlibs gcc 16.1, the
  "gcc" flavor — fresh dir, `-DCMAKE_C_COMPILER=cmake-mingw-x86_64/.../gcc.exe`),
  `cmake-windows-msvc`. Remaining 12 skips are all genuinely env-gated, NOT
  improper WIN32 gates: diff3-suite/preprocess-suite (need **clang**, absent on
  this host — would skip on Linux too), asm-gas-directives (unconditional, real
  missing assembler encodings), i386-fastcall-abi (needs ELF-emitting 32-bit
  ref), compile.ex4 (X11), pe-wine (wine), 6×macho (macOS/cross).

Seventh pass (2026-06-30) — toolchain sweep caught 3 ABI-gating misses + 2
warnings (commit 4edaa9ce). All four flavors green: cmake-windows-mingw (CLion
gcc 13.1), cmake-windows-gcc (winlibs 16.1), cmake-windows-msvc — each 37/37 —
plus qemu-docker 22/22. Three recent-commit tests assumed LP64/Linux:
- exec `cast_operator` expect_win32 was missing the **line-36** warning: on
  LLP64 BOTH casts are size-mismatched (line 36 ptr 8→ulong 4, line 37 ulong
  4→ptr 8) so both emit "cast between pointer and integer of different size".
  The 6th-pass golden listed only line 37 (transcription slip). Linux LP64
  emits neither (ulong 8 == ptr 8). The cast-warn logic (mccgen.c ~3485) was
  unchanged — only the golden was wrong.
- cli `tss_dtor_iterations_ice` (`<threads.h>`→`<pthread.h>`, absent on WIN32)
  and `tgmath_creal_cimag_precision` (expects `16 4 8`; WIN32 long double==
  double → `8 4 8`) gated `os!=WIN32:<reason>`. cli_case_t has no expect_win32
  field (only name/req/cmd/expect), so ABI-divergent cli cases must skip, not
  branch.
- Warnings: `src/mcc.h` MCC_PROFILE block now `#undef inline` before redefining
  (on _WIN32 `inline` is already `__inline` → "inline redefined" in mcc_p);
  `src/mccpp.c` get_tok_str got a `/* fall through */` on TOK_U8STR→TOK_STR.

Eighth pass (2026-06-30) — clang-toolchain + diff3/preprocess on Windows
(commit ed19a2b0; see [[moderncc-windows-build]] for the clang-toolchain target).
Porting `tests/diff3/runner.c` to Windows surfaced two real bugs in the runner:
- **timeout-wraps-`cd` bug** (cross-platform, not just Windows): the hang guard
  prepended `timeout N` to the whole `cd "work" && prog` string, so `timeout N cd`
  tried to exec the `cd` builtin ("timeout: failed to run command 'cd'"), the &&
  short-circuited, the program never ran, and all three captures came back empty
  → compared equal → **vacuous pass**. Fixed: timeout wraps ONLY the program; cd
  stays outside. (This means the suite had been a partial no-op anywhere GNU
  `timeout` is on PATH.)
- **unquoted reference-cc path**: `cc` was interpolated unquoted into the sh build
  command, so a backslashed Windows path lost its separators under MSYS sh (and a
  space would split). Now quoted like mcc's path already was.
- The Windows port itself mirrors the cli runner: route system()/popen() through
  MSYS sh via a scratch `_diff3cmd.sh` (RUN_SYSTEM/RUN_POPEN macros, `g_work` set
  + work dir created before same_compiler), `.exe` suffix for run exes, _MSC_VER
  popen→_popen. New **`diff3!=OS`** req token (diff3-only skip; exec ignores it).
- **Comment-strip regression (fixed, commit 027c1060)**: the remote's *"Strip
  code comments project-wide"* commit (835c3258) removed `/* fall through */`
  annotations across src/, reintroducing `-Wimplicit-fallthrough` at 14 sites
  (mccpp.c ×5, mccgen.c ×4, libmcc.c ×3, i386-asm.c ×1, mccpe.c ×1) on every GCC
  build. Fixed with a comment-independent **`FALLTHROUGH`** macro in `src/mcc.h`
  (GCC≥7/clang≥10 → `__attribute__((fallthrough))`, else incl. MSVC → `((void)0)`)
  so a future strip can't re-break it. Use `FALLTHROUGH;` as the last stmt before
  the next `case`. The strip also removed C comments from `tests/diff3/runner.c`,
  so keep that runner comment-free to match. CMake `#` comments were NOT stripped.

Ninth pass (2026-07-01) — GitHub-Actions Windows CI (windows-msvc + windows-mingw
jobs) failed `parts-suite` + `mcctest`. Both roots: the CI reference toolchains are
**UCRT**-based (winlibs 16.1 `mingw-w64ucrt`, and LLVM clang targeting
windows-msvc), whose C99 printf can't match mcc's **legacy msvcrt.dll** printf.
- **mcctest** (`CMakeLists.txt` ~3178, WIN32 `_mcctest_refflags`): the ref gcc
  compiled full_language.c with UCRT printf → "nan(ind)" vs mcc's "1.#IND00",
  full-precision `%a`/`%f`, and a **0xC0000409 fastfail** inside asm_test's printf
  (UCRT `_invalid_parameter` on a specifier msvcrt tolerates) → truncated output +
  mismatch. Passed locally only because CLion mingw 13.1 is msvcrt-based. Fix:
  force the ref onto legacy msvcrt printf with **`-D__USE_MINGW_ANSI_STDIO=0`**
  (suppress the `__mingw_printf` inline that mingw <stdio.h> uses by default) AND
  **drop `-lmsvcrt`** (its import lib binds printf to ucrtbase), resolving the C
  runtime straight from `c:/windows/system32/msvcrt.dll` placed inside the
  `--start-group` (so libmingwex's helpers back-reference the same DLL). Verified
  byte-identical (1475 lines, exit 0) with BOTH winlibs 16.1 UCRT and CLion 13.1
  msvcrt. Note mcc ships no `stdio.h`, so the ref uses mingw's system one — that's
  why the CRT family of the ref gcc decides printf behaviour. `__USE_MINGW_ANSI_STDIO=0`
  alone is insufficient on UCRT (the fallback is still ucrtbase's conformant printf).
- **parts-suite** (`CMakeLists.txt` ~2795): a native 3-way gcc/clang/mcc
  differential whose run_*.c wrappers carry **no** legacy-msvcrt ref flags, so on
  WIN32 the gcc/clang(UCRT) legs can never be byte-identical to mcc(msvcrt); clang
  additionally can't even compile `<tgmath.h>` against UCRT's struct `_Fcomplex`,
  and mcc has no msvcrt libm for the complex/tgmath/fenv units. It only started
  running in CI because clang is on the windows-latest PATH (absent on this host →
  it skipped locally). Fix: **`mcc_skip_test` on `MCC_TARGETOS==WIN32`** with a
  reason — the same parts/*.h units are already covered on Windows in aggregate by
  mcctest (msvcrt ref flags + `MCC_HAS_C99_LIBM` drops the libm surface). Unchanged
  on Linux/macOS where all three share one C99 libc. Reproduced the CI failure by
  downloading the pinned winlibs 16.1 UCRT gcc and running the real
  `run_mcctest.cmake` driver against it.

Ninth pass addendum (2026-07-01) — after the msvcrt-printf fix, the
windows-latest **system mingw** (`C:\mingw64`, gcc 15.2, NOT winlibs) failed
mcctest at link: `undefined reference to __intrinsic_setjmpex`. That runtime's
<setjmp.h> reaches SEH setjmp via `__intrinsic_setjmpex`, an extern-inline
intrinsic with no out-of-line def, so the **-O0** ref build emits an unresolved
call; the symbol is in libmsvcrt.a there, which we can't link (drags ucrtbase
printf back in). winlibs headers dodge it by calling `_setjmpex` directly (a real
msvcrt.dll export), which is why the pinned toolchains passed. **`-D__USE_MINGW_SETJMP_NON_SEH`
did NOT fix it** (commit 88786f92, reverted): C:\mingw64's header reaches the
intrinsic regardless of that macro. Real fix (commit 5c60cf44,
**`tests/support/msvcrt_start.c`**): define `__intrinsic_setjmpex`/`__intrinsic_setjmp`
as bare **tail jumps** (`jmp *__imp__setjmp{,ex}(%rip)`) to msvcrt.dll's own
`_setjmp`/`_setjmpex` (msvcrt exports everywhere, msvcrt.def 713/714). Tail-jump
not call, so `_setjmp*` captures the caller's frame+return addr -> setjmp/longjmp
semantics intact (verified r=val + volatile preserved, with NULL *and* real frame
args; disasm = clean IAT `jmp`). x86_64-only, `#ifndef __MCC__`; inert on runtimes
whose header calls `_setjmp*` directly. Ref link stays `-lmingwex + msvcrt.dll`
only. Note: trailing `-lmsvcrt` is NOT a way to supply missing CRT symbols -- ld
prefers the archive's static printf over the msvcrt.dll export even when trailing,
re-breaking the format match (exit-127 crash on ucrt winlibs). full_language.c only
longjmps within one frame, so ref `_setjmp*` vs mcc's `_setjmpex` is output-identical.

Tenth pass (2026-07-01) — **arm64 Windows** job (windows-11-arm, MCC_CPU=arm64)
failures, none reproducible on this x86_64 host (no arm64 PE execution):
- **exec `cast_operator`** crashed (only the 2 compile warnings, no program
  output). It round-tripped `&obj` through `unsigned long` (4 bytes on LLP64),
  truncating the address, then dereferenced it -- fine on x86_64's low stack
  addresses, faults on arm64's high ones, and fully-buffered stdout is lost on
  the crash. Fix (`tests/exec/expressions/cast_operator.c`): keep the two
  size-mismatch casts at lines 36/37 (the warnings are the point; golden pins
  them by line) but do the value round-trip through pointer-sized `unsigned long
  long` (lossless). Golden unchanged. (commit 90b24a84)
- **mcctest** failed at link: `c:/windows/system32/msvcrt.dll: file format not
  recognized`. The WIN32 differential links the system msvcrt.dll directly (only
  way to reach msvcrt's legacy printf); on arm64 the runner's only gcc-style ref
  is an x86_64 mingw under emulation whose bfd ld can't parse the arm64/ARM64X
  DLL, and no native arm64 mingw ships. Never green there (direct-DLL link
  predates the refflags work). Skip mcctest/-bcheck on `WIN32 AND arm64`
  (CMakeLists ~3226), mirroring the Darwin/arm64 skip. (commit 2c33aa6c)
- **pe-native-conformance** `tls`/`tls_aggr` access-violated: **arm64 PE
  _Thread_local is unimplemented**. arm64-gen.c still emits the ELF `mrs x30,
  tpidr_el0` + R_AARCH64_TLSLE_ADD_TPREL_{HI12,LO12} for VT_TLS (load() ~553 and
  3 more sites). Windows/arm64 has no thread pointer in TPIDR_EL0 -- TLS is via
  the TEB: `ldr x30,[x18,#0x58]` (ThreadLocalStoragePointer) -> load `_tls_index`
  -> `ldr x30,[x30, idx, lsl #3]` (block base), the analogue of x86_64
  `gen_pe_tls_base` (%gs:0x58). **Implemented** (commit 34f8b74f, superseding the
  59b425a1 skip): arm64-gen.c `arm64_tls_base_x30()` emits the TEB sequence under
  MCC_TARGET_PE (`str x16,[sp,#-16]!; ldr x30,[x18,#0x58]; x16=&_tls_index; ldr
  w16,[x16]; add x30,x30,x16,lsl#3; ldr x30,[x30]; ldr x16,[sp],#16`) at all 4
  VT_TLS sites; ELF path unchanged. arm64-link.c PE TPREL = `val - tls_start` (no
  +16), matching x86_64 PE TPOFF32. x16 saved/restored (it is value-allocatable;
  x18 is reserved on PE per reg_classes so it stays the TEB). mccpe.c infra was
  already arch-agnostic. **Verification without an arm64 host**: build the
  `arm64-win32` cross compiler (`-DMCC_ENABLE_CROSS=ON` -> mcc-arm64-win32.exe),
  compile a TLS program, parse the ELF .o and disassemble .text with **Python
  capstone** (`pip install capstone`; `Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN)`)
  -- confirms the exact encodings + that .rela.text has _tls_index ADR/ADD and
  TPREL_HI12/LO12 on the right vars; also disasm the arm64 ELF (mcc-arm64.exe) .o
  to confirm the Linux path still emits `mrs x30,tpidr_el0` (0xd53bd05e).
  **Runtime bug found via CI + fixed (commit 3301eb61)**: the codegen/relocs were
  right but tls still faulted -- the arm64-link.c TPREL `tls_start` loop gated on
  `s->sh_size`, which **PE never sets** (uses data_offset), so the .tls section
  was skipped, tls_start=0, and the resolved offset became the whole .tls RVA
  (+0x3040) not the in-block offset. Fix: `ssz = sh_size ? sh_size : data_offset`
  (as x86_64 TPOFF32 does). To catch this without an arm64 host: build a full
  arm64 PE exe with the cross compiler (construct a -B sysroot: copy
  `arm64-win32-libmcc1.a`, `lib-arm64-win32/*.o`, and `lib/*.def` into
  `<B>/lib/`) and parse the PE in Python -- check DataDirectory[9] (TLS dir:
  raw+zerofill sizes, AddressOfIndex) and disassemble .text to read the resolved
  `add x30,x30,#hi,lsl#12 ; add x30,x30,#lo` pairs (offsets must be in-block, not
  the .tls RVA). Cross-check vs the native x86_64 tls.exe (runs here, exit 0).
- **mcctest re-enabled on arm64** (commit ac56a1c3, superseding the 2c33aa6c
  skip). The skip was because the WIN32 refflags link the system msvcrt.dll
  directly and arm64's emulated x86_64 bfd ld can't parse the ARM64X DLL. But
  that direct-DLL dance only exists to defeat a *UCRT* ref's C99 printf; the arm64
  runner's C:\mingw64 is **msvcrt-based** (its setjmp uses `_setjmpex`), so a
  plain link with just `-D__USE_MINGW_ANSI_STDIO=0` gives legacy printf and links
  via the import lib (no system DLL). Added an arm64 WIN32 refflags branch; it's
  cross-arch (x86_64 ref exe under emulation vs native arm64 mcc) but
  full_language.c is byte-identical across arches on WIN32. Runtime confirmed on
  the arm64 CI runner.

Eleventh pass (2026-07-03) — full preset sweep on Windows, 3 fixes (commits
4a0a4b1e, d5491f37, 44e83c44):
- **CONFIG_MCC_STATIC breaks compile on WIN32**: mcc_p and the -static cross
  variants defined it unconditionally; mcchost.c's built-in `-run` symbol
  table takes `&stdin/&stdout/&stderr`, which are NOT lvalues under mingw
  (msvcrt and UCRT both expand them to call/rvalue macros) -> "lvalue required
  as unary '&' operand". Not a regression from the mcchost refactor (parent
  commit failed identically) — introduced when the build-normalize commit gave
  mcc_p the define. Fix: gate CONFIG_MCC_STATIC on `MCC_TARGETOS != WIN32` at
  both sites (mcc_p ~2104, cross static variants ~2404), mirroring the
  existing mcc_static gate: on WIN32 even a -static exe resolves `-run` libc
  via LoadLibrary. This unblocked `diagnostics` and `dist-mingw` (which builds
  mcc-<arch>-static for all 12 cross targets).
- **matrix preset on Windows**: (1) mcc_map_toolchain_cc('clang') now falls
  back to the fetched cmake-clang toolchain when find_program(clang) fails
  (mirrors 'mingw'); (2) superbuild cells pass
  -DCMAKE_RC_COMPILER=<ccdir>/llvm-rc.exe — CMake's Windows-Clang (GNU-like)
  platform hard-requires a resource compiler and LLVM's llvm-rc is off PATH.
- **mcctest under a clang-msvc host**: the WIN32 refflags are mingw-gcc-only;
  the mingw-gcc auto-detect fired only for CMAKE_C_COMPILER_ID==MSVC, so the
  matrix clang-native cell used clang itself as reference — which can't even
  compile full_language.c against UCRT <fenv.h> (_SW_INVALID undeclared).
  Now: `MCC_TARGETOS==WIN32 AND NOT GNU` -> resolve winlibs gcc, else PATH
  gcc, else skip-with-reason naming the host compiler id.
- Note: `mcctest-bcheck` now SKIPS on all WIN32 flavors ("mcc bounds-checking
  is unsupported on the PE/msvcrt target") — README table updated accordingly.
- Windows suite is 40 tests now (was 37): + host-gate-invariant,
  dash-s-roundtrip, macho-libsystem-kernel-fused. All presets verified green
  2026-07-03; see [[moderncc-windows-build]].

**Stale exec-golden line numbers after a source reformat** (2026-07-04). When a
fixture under `tests/exec/**` is reformatted/shortened, the diagnostic line
numbers baked into `tests/exec/goldens.h` (the 7th `expect` field) go stale and
the exec test fails with a line-number-only mismatch -- compiler-independent, so
it fails on EVERY preset (mingw AND msvc), and the program's own stdout is still
correct. Two instances so far both caused by commit `6005bca7` "Strip comments
and reformat to tab-indented K&R": `arm64_errors` (fixed in `4de2f4a8`, golden
lowered) and `cast_operator` (fixed 2026-07-04: casts moved 36/37 -> 24/25 after
the leading blank lines + comment block were stripped; golden updated to 24/25).
Debugging trap: a `git pull --rebase` updates the working-tree FIXTURE but you
may still be reasoning about the pre-rebase file -- always re-read the fixture and
run `mcc -c <fixture>` to get the CURRENT line numbers, then match the golden to
that. This is test-data only; NOT a compiler bug. (I initially misdiagnosed it as
a GCC-16 `-O3` strict-aliasing miscompile and added `-fno-strict-aliasing -fwrapv`
to CMakeLists -- reverted once the MSVC preset showed the same failure, proving it
was compiler-independent.)

Twelfth pass (2026-07-06) — full Windows preset sweep, one real harness fix
(`CMakeLists.txt`, cli/diff3 test PATH self-sufficiency). The `cli`/`diff3`
suites route their pipelines through MSYS `sh` (`MCC_TEST_SH` = git's
`usr/bin/sh.exe`). A non-login `sh script.sh` does NOT add git's `/usr/bin` to
PATH — it inherits only the ctest process PATH — so `grep`/`sed`/`printf` were
"command not found" unless the *invoker* happened to have git's usr/bin on PATH.
That made a bare `ctest --preset debug` (mingw + cmake + ninja + shims on PATH,
but no git usr/bin) fail **365/810** (all 141 cli + 224 diff3). Fix: on WIN32 the
cli/diff3 test registration now sets **`ENVIRONMENT_MODIFICATION`** =
`PATH=path_list_prepend:<sh dir>` (bundles grep/sed/printf) **and**
`PATH=path_list_prepend:<nm dir>`. The nm dir is `find_program(MCC_TEST_NM nm)`
first, else `mcc_mingw_resolve()`'s vendored winlibs `bin` (EXISTS check, no
download). So the suites are self-contained: **git `sh` + `cmake` alone** run them
green with zero UNIX-tools/binutils on the invoker PATH; on a pure MSVC host the
binutils come from the vendored winlibs. Verified: debug/release/diagnostics/cst/
cross **810/810**, matrix 4×810, mingw cell 810, **msvc 808/808** run with a
minimal PATH (proved nm self-resolved from vendored winlibs). Committed+pushed
as **`f310e769`** (on main). Between my first sweep and the push the remote landed
three x86_64 codegen/TLS commits (`2a3f83ef` glibc-static reloc/TLS-layout,
`fb6d7144` TLS-model GD/LD linking, `0ecec9b6` -static non-PIE) — all touch
shared `x86_64-link.c`/`mccelf.c`/`libmcc.c`. Rebased over each and re-validated
on Windows PE (they're ELF-static / ELF-TLS-model gated — PE TLS uses the TEB
path and doesn't emit TLSGD/GOTTPOFF/__tls_get_addr, so no PE impact): full
`debug` **812/812** + `msvc` **810/810** green each rebase (exec/tls, tls_aggr,
c11_threads, atomics, exec/static all pass). Their new `static-glibc-run` +
`tls-models` ctests register and self-skip on Windows (counted → totals crept
810→812 native, 808→810 msvc). Lesson: against an active remote, gate the push on
the two canonical x86_64-PE presets (debug mingw-built + msvc cl-built) per
rebase rather than the full 45-min matrix. Two non-issues ruled
out during the sweep: (1) `git-stamp` "fails" only when `git` is absent from PATH
(a legitimate build req, not a bug — it embeds `branch@hash`); (2) a one-off
`gcc -E charconst_in_if.c` hang at 0 CPU was a transient pipe stall under two
concurrent heavy test suites — the identical invocation passes in every preset
and in a standalone rerun (0.08 s). Lesson: don't run two heavy ctest suites
concurrently on this host. `sanitize` still an intentional configure-fatal on
WIN32; both `dist-*` build+`package-dist` exit 0 (4 zips each); `local-ci`
configures+builds clean (its orchestrated msvc/mingw/dist-* are each verified).
Docs refreshed: NOTES.md Windows status (782→810 native, msvc 808, self-sufficiency
note) + footnote 3.

Thirteenth pass (2026-07-07) — two Windows/PE fixes for newly-added upstream
C99/C11 tests (commits on main after `5d7584f1`; the GitHub `msvc / x86_64` job
was red). Both are PE-header/test-harness issues, NOT codegen:
- **`float_t`/`double_t` undeclared on PE** (`runtime/win32/include/math.h`):
  the curated mingw `<math.h>` mcc ships for PE omitted the C99 `float_t`/
  `double_t` typedefs (§7.12p2). On Linux mcc uses the *system* `<math.h>`
  (glibc defines them), so `exec/flt_eval_method` + `diff3/flt_eval_method` only
  failed to *build* on WIN32 (`'float_t' undeclared`). Fix: add the typedefs
  keyed off the same `__i386__` condition `runtime/include/float.h` uses for
  FLT_EVAL_METHOD — i386 (method 2, x87) → `long double`; x86_64/arm64
  (method 0) → `float`/`double`. long double==double on PE so the method-2
  width asserts hold. (The exec test carries `-lm`, a no-op on PE.)
- **cli `c11_noreturn_returns` missing `-B{B} -I{I}`** (`tests/cli/cases.h`):
  the new test `#include <stdnoreturn.h>` but invoked `{MCC}` bare, unlike every
  other header-using cli case (cf. `uchar_header`). `{MCC}` is JUST the mcc path
  (runner subst, no implicit -B/-I); the header only resolved via mcc's
  exe-relative mccdir auto-detect, which works for the mingw build layout
  (`mcc.exe` beside `include/`) but NOT MSVC's (`Release/mcc.exe`, headers a
  level up → not found → mcc errors before the noreturn diag → grep captures
  nothing). Fix: add the standard `-B{B} -I{I}` (inert on Linux/mingw where
  they're already the norm). Passed on debug only by build-tree accident; the CI
  msvc job runs the same `Release/` layout, so this greened it.
Validated on the two canonical x86_64-PE gates: debug (mingw-built) **825/825**,
msvc (cl-built = the CI job) **823/823**. Pushed as `064c48c8`. NOTE the counts
now drift every few upstream commits (each new platform-gated test registers +
self-skips on Windows: static-glibc-run, tls-models, +C99/C11 cases) — 810→825
native over this session; don't treat a specific total as fixed. Gotcha:
runtime headers are COPIED into each build tree at configure, so a
`runtime/win32/include/*.h` edit needs a **reconfigure** (`cmake --preset <p>`)
before it takes effect in that build dir.

Fourteenth pass (2026-07-08) — two AST-replay bugs surfaced by the full preset
sweep after the `_Complex`/AST-replay commit run (`28aff147` etc). Both landed on
`main` as `5b749f99` + `2f1ea759` (rebased over 8 active-remote commits incl. the
imaginary-literal/complex-cast replay work — no conflict; my areas untouched):
- **AST replay `ast_fconst` stored dangling `Sym*`** → `exec-replay/complex_annexg`
  + `-tmpl` failed on WIN32 only with **`mcc: error: unresolved reference to ''`**
  (an empty-named GLOBAL UND OBJECT sym; the compile+`-c` succeed, it fails at
  LINK because mcc's linker rejects any undefined global even if no reloc uses it).
  Root cause: `gv`'s float-const-pool reuse (`src/mccgen.c` ~2085) recorded the anon
  rodata const **`Sym*`** at parse-build and reused it at replay (to keep the reloc
  byte-identical). But that Sym is FREED during CAPTURE (its statement scope ends)
  and its memory RECYCLED — normally harmless because replay allocates no Syms, so
  the freed slot still reads like a valid const sym. `gen_complex_call` broke that:
  it built a fresh helper functype (5 `SYM_FIELD` param syms) AND `mk_pointer`'d the
  result (another `SYM_FIELD` struct sym) **on every call incl. during replay**,
  recycling exactly the freed const slots `ast_fconst` pointed at → the recorded
  entry became a live field sym, emitted as empty-named undef. LP64 passed by luck
  (malloc order left the freed memory intact); WIN32 malloc reused those slots.
  **Fix: record the ELF symtab index (`Sym->c`, stable for the whole TU), not the
  `Sym*`; at replay alias it via a fresh anon Sym whose `->c` = recorded index →
  byte-identical reloc without depending on freed sym memory.** Also cached
  `gen_complex_call`'s functype per base variant (mcc.h `gen_complex_call_ftype[4]`,
  reset in `mccgen_finish`) — mirrors `mk_complex_type`'s cache, kills a per-call
  6-sym global_stack leak. Debug trick that nailed it: instrument `put_extern_sym2`
  to print on empty name, correlate the recorded fs pointer's `v` at record-time
  (`0x1000000x` anon) vs reuse-time (`0x20000000` = SYM_FIELD) — same address, proof
  of recycling. **Verify byte-identity: `cmp` the `-O0` .o vs `MCC_AST_REPLAY=1` .o
  — identical ⇒ genuinely replaying, not falling back.** The remote's own TODO
  ("__builtin_complex I unit needs const-symbol reuse") wanted exactly this
  robustness.
- **arm64 `va_arg` `ast_bail` unguarded** → `cross` (+ would break `dist-mingw`)
  failed to BUILD: `src/mccgen.c` ~7568 `if (ast_active) ast_bail = 1;` in the
  `TOK_builtin_va_arg` case sits under `#ifdef MCC_TARGET_ARM64` but NOT
  `#if CONFIG_AST`; `ast_active`/`ast_bail` are declared only under CONFIG_AST, and
  **cross compilers build without CONFIG_AST** → `'ast_active' undeclared` for every
  arm64 cross target. Invisible to native x86_64 (never compiles the ARM64 branch).
  Class of bug: any `ast_active`/`ast_bail` use inside `#ifdef MCC_TARGET_*` code
  must be `#if CONFIG_AST`-guarded (the ~30 sites in the `ast_hook_*` cluster are
  fine — that whole block is under one CONFIG_AST). Fix: wrap the hook in the guard.
Full native sweep this pass (rebased tree): debug/ast/cst/release/cross/diagnostics
**1415/1415**, msvc (cl) **1413/1413**, Docker linux-gcc **1482/1482** (proves the
`ast_fconst` change is byte-clean on LP64 too). `CONFIG_AST` default-ON so the
exec-replay + exec-replay-tmpl columns register on every native preset.

Fifteenth pass (2026-07-08, pushed `88a49e54`) — full Windows preset sweep at an
active remote (rebased 3× mid-sweep: `3e6f0079`→`d6740df9`→`ab7f5ee9`). **No code
fixes needed — every runnable preset green.** Only a docs count-refresh committed
(NOTES.md + TODO.md Windows status). Key facts for the next sweep:
- **Count basis jumped 1415 → ~1696** because `d6740df9` (Tier-3 register promotion)
  added the whole-corpus **`exec-replay-promote`** column (~281 programs run with
  `MCC_AST_REPLAY=1 MCC_AST_PROMOTE=1`), which registers on Windows x86_64 too. New
  native basis: debug/ast/cst/cross/release = **1696**, diagnostics/sanitize = **1697**
  (+`sanitize-smoke`), sanitize-msvc = **1695**, msvc = **1694**. msvc runs the *same*
  1515 cases as debug (all pass) — it's 2 fewer *registered* only because the two
  macOS-only `macho-*` cross cases aren't emitted under the VS generator (they're
  skip-stubs on Ninja). Don't cite "msvc equals debug" as exact equality.
- **Register promotion FIRES correctly on Win64 PE.** The R11/R10/R9/R8 pin pool is
  x86_64-arch (not SysV-ABI) specific, so it engages on Win64 despite the different
  caller-saved/arg-reg map. Verified manually: `mcc -B<bld>` with
  `MCC_AST_REPLAY=1 MCC_AST_REPLAY_DUMP=1 MCC_AST_PROMOTE=1` on `tests/ast/replay/
  promote.c` prints `[ast-replay] calc` + `[ast-promote] 3 calc` (3 locals pinned)
  and the exe exits 42 == -O0. (The **dump markers need `MCC_AST_REPLAY_DUMP=1`** —
  the plain replay/promote env vars alone emit nothing.)
- **The dedicated `ast/replay-*` fixtures are WIN32-excluded** (CMakeLists ~2957:
  `if(NOT CMAKE_CROSSCOMPILING AND NOT WIN32)`) — pre-existing; `replay-promote` is
  additionally x86_64-gated (`ab7f5ee9`). Windows gets its replay coverage from the
  whole-corpus `exec-replay`/`-tmpl`/`-promote` columns instead, so a missing
  `ast/replay-promote` in a Windows ctest run is expected, not a regression.
- **`sanitize` is NO LONGER configure-fatal on Windows** (landed `ccba8d22`, before
  this sweep): mingw → trap-mode UBSan (−alignment), `sanitize-msvc` preset → MSVC
  ASan; both run a `sanitize-smoke` ctest that executes a sanitized `mcc`. Earlier
  passes in [[moderncc-windows-build]] that call it "intentional configure-fatal"
  are STALE — see [[moderncc-windows-sanitizers]].
- Lesson reconfirmed: gate the push on the two canonical PE presets per rebase
  (debug mingw + msvc cl); a code commit like the promotion optimization must be
  re-verified on PE (both were 1696/1694, exec-replay-promote column 0 fail).

**UCRT `environ` dllimport warning (2026-07-10, commit dc804450)** — on the CI
winlibs GCC 16 **UCRT** toolchain (`vendor/winlibs-mingw-w64-16.1.0-ucrt-*`),
`<stdlib.h>` defines `environ` as the macro `(*__p__environ())`, so
`mcchost.c`'s POSIX-style `extern char **environ;` in `host_environ()` expanded
to a bad redeclaration of `__p__environ` (no dllimport) → `-Wattributes` warning
×45 across every toolhost TU + the amalgamated build. Older MSVCRT mingw (CLion
gcc 13) and MSVC declare `environ` as a plain extern, so **neither reproduces it
— it's UCRT-only**; use the vendored winlibs gcc to repro Windows warnings, not
CLion's. Fix: `#elif defined(_WIN32) return environ;` (use the header-provided
symbol/macro directly). Verified clean on winlibs UCRT x86_64+i686 and MSVC cl.

Benchmark n/a note: the CI `mcc` bench rows go all-n/a when the self-hosted
stage2 mcc crashes; the win64-alloca/promote-spill fixes (8191e9ea) cured
default/-O1/-O2 on PE, but `-O3` (which adds the AST inliner) still SIGSEGVs the
stage2 on corpus/mcc.c (full_language survives) — that's an AST_INLINE codegen
regression tracked/fixed separately, NOT the environ issue.

See [[moderncc-windows-build]] for how to build/test (incl. MSVC + qemu-docker).
