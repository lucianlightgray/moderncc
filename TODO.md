# C11 Conformance & Platform TODO

Goal: implement every detail of ISO/IEC 9899:2011 (C11), backed by regression
tests across all targets (x86_64, i386, ARM, AArch64, RISC-V 64), platforms
(ELF/PE/Mach-O), and both glibc and musl.

Reference: **N1570** (the C11 working draft; clause numbers below cite it).
Each item records the standard requirement or task, the current `mcc` behaviour
(with how it is observed — `file:line` or a test result), and the precise gap.

The default standard is **C99** (`cversion=199901`, `src/libmcc.c`); `-std=c11`
selects C11. Items below apply at `-std=c11` unless noted.

Severity tags:
- **[BUG]** — wrong codegen, crash, or rejects-valid C11.
- **[FEATURE]** — a C11 feature that is absent.
- **[DIAG]** — a constraint violation that `mcc` accepts with no diagnostic, or a diagnostic that is wrong/misleading.
- **[OPT]** — optional in C11 (a conforming implementation may omit it); listed for the "implement everything" goal.
- **[TASK]** — test/infrastructure work, not a language feature.
- **[LIMITATION]** — a host/platform constraint outside `mcc`'s control.
- **[DIFF]** — a known, deliberate difference from a reference compiler; not a defect.

Legend: `[ ]` open · `[~]` partial · `[x]` done.

---

## Open items

- [ ] **[BUG] Mach-O thread-local storage (`__thread` / `_Thread_local`) is unimplemented.**
  On arm64 the backend emits ELF local-exec TLS relocations
  (`R_AARCH64_TLSLE_ADD_TPREL_HI12/LO12`, base `tpidr_el0 + sym@tprel`;
  `src/arch/arm64/arm64-gen.c`). Mach-O has no such relocations: a thread-local
  resolves to a wrong address, so any `__thread` access segfaults (single- and
  multi-threaded alike). Mach-O thread-locals use TLV descriptors — the
  `__thread_vars`/`__thread_data`/`__thread_bss` sections plus a `tlv_get_addr`
  call sequence — which the Mach-O object writer (`src/objfmt/mccmacho.c`) does
  not emit. The `tls` exec golden carries `os!=Darwin` so it skips on Darwin and
  runs on ELF.

- [~] **[DIAG] §6.5.1.1p2 `_Generic` association-type completeness.**
  The "no two compatible association types" rule is enforced
  (`src/mccgen.c`, `TOK_GENERIC`; `_Generic(1, long:1, long:2, int:3)` errors,
  cli `generic_duplicate_assoc`). The sub-rule that an association type is a
  complete object type — not a VLA and not a function type — is unenforced:
  `mcc` accepts all three forms. gcc also accepts all three; clang rejects only
  the function-type form; C2y permits the incomplete-type case. Deferred —
  enforcing it diverges from gcc (the leniency reference) and from where the
  standard is heading.

- [x] **[TASK] The qemu cross-conformance matrix is wired into CI.**
  `.github/workflows/ci.yml` runs on every push/PR/dispatch: a `portable` job
  (gcc + clang on ubuntu) configures, builds, and runs `ctest -LE
  "qemu|macho|wine"` (the host-portable suites), and a `qemu-matrix` job builds
  the repo's Docker runner (`tests/qemu/docker`) and executes the full
  x86_64/i386/arm/arm64/riscv64 × glibc/musl matrix via `ctest -L qemu`.

- [~] **[LIMITATION] The kernel-fused Mach-O / libSystem path needs a macOS or darling host.**
  The self-contained Mach-O tests run on any host: structural validation
  (`macho-structural`), Darwin-codegen execution against host glibc
  (`macho-codegen-run`), Mach-O image execution via the in-repo loader
  (`macho-image-run`), and Apple's own string/memory + self-contained printf
  sources run as Mach-O images (`macho-apple-libc`). The remainder of
  `libSystem` is kernel-fused and unrunnable off Darwin: `libmalloc`
  (magazine/nano/xzone zones on `os_unfair_lock`/Mach VM), FILE-backed/locale
  `stdio` (`xlocale`/`__sFILE`/gdtoa), and `dyld`/`pthread`/GCD/ObjC/Mach IPC.
  These require `-DMCC_DARWIN_HOST=ON` (a macOS or darling host); the boundary
  is documented in `tests/qemu/apple-libc/PROVENANCE.md`.

- [~] **[TASK] Systematic diagnostic-coverage sweep.**
  Each `[DIAG]`/`[FEATURE]` item ships with a cli/exec test that asserts the
  diagnostic fires and that the valid forms still compile; the one open DIAG
  item (`_Generic` association completeness) has coverage (`generic_*` cases in
  `tests/cli/cases.h`). A full mechanical parity sweep across all ~312
  `mcc_error`/`mcc_warning`/`mcc_pedantic` call sites in `src/` — to confirm the
  diagnostics that predate this tracker each have a regression test — is an
  ongoing audit and remains outstanding (not blocked on anything).

- [ ] **[LIMITATION] The macOS SDK ships no C11 `<threads.h>`.**
  Apple's libc provides no `<threads.h>` / `thrd_*` runtime, so a program using
  C11 threads cannot compile or link on a stock macOS host. The `c11_threads`
  exec golden carries `os!=Darwin` and skips on Darwin (it runs on the
  glibc/musl ELF targets). Not an `mcc` gap.

- [~] **[DIFF] diff3 differential divergences on macOS.**
  The `diff3` suite compiles each golden with `mcc`, gcc, and clang and flags
  cases where `mcc` differs from a gcc==clang consensus. Four such divergences
  remain on macOS, none of which is an `mcc` defect:
  - `predefined_macros` — `mcc` defaults `__STDC_VERSION__` to C99 (`199901`);
    the system clang defaults to gnu17 (`201710`).
  - `bitfields_ms` — the MS-bitfield (`ms_struct`) layout is
    implementation-defined and differs from the system clang's.
  - `cleanup` — the `__cleanup__` teardown ordering is a deliberate `mcc`
    choice (the golden notes gcc/clang differ from the intended semantics).
  - `c11_freestanding_headers` — `mcc`'s bundled freestanding headers satisfy
    the C11 checks that the macOS *system* headers (used by gcc/clang) fail, so
    here `mcc` is the most conformant of the three.
  The suite returns non-zero on any divergence by design; these four are the
  expected residual on a macOS host.

- [x] **[BUG] PE thread-local storage (`_Thread_local` / `__thread`) now uses the TEB.**
  The PE backend emitted the ELF Local-Exec sequence (`%fs:0`/`%gs:0` thread
  pointer + `sym@tpoff`) and the object writer never populated the TLS data
  directory, so every thread-local read landed on an unrelated address (a
  segfault natively; `tls.c` returned 1 under wine). Fixed in three parts:
  (1) `src/objfmt/mccpe.c` `pe_add_tls`/`pe_set_tls` synthesise `_tls_index`, a
  NULL-terminated callback array, and a populated `IMAGE_TLS_DIRECTORY`
  (StartAddressOfRawData/EndAddressOfRawData over `.tdata`, `SizeOfZeroFill`
  over `.tbss`, AddressOfIndex/Callbacks), pointed to by DataDirectory[9];
  (2) i386/x86_64 codegen (`src/arch/{i386,x86_64}/*-gen.c`) reach a
  `_Thread_local` through the TEB — x64 `%gs:0x58`, x86 `%fs:0x2c`
  ThreadLocalStoragePointer → `[_tls_index]` → `+ template offset` — for the
  load, store, and `&tls` paths; (3) the PE branch of `R_X86_64_TPOFF32` /
  `R_386_TLS_LE` resolution (`*-link.c`) now yields the offset from the start of
  the TLS template instead of the SysV tp-relative offset. Verified natively:
  `tls.c` and `tls_aggr.c` return 0 on x86_64 and on i386 (WoW64), and the full
  16-program conformance set passes on both. (Default x86/x64 exes are
  non-relocatable, so the directory's absolute VAs need no base relocations; an
  ASLR/DLL build would additionally need `.reloc` entries for them.)

- [x] **[BUG] PE thread-local aggregates / pointer-init / `&tls`-into-libc paths.**
  `tests/qemu/conformance/tls_aggr.c` (a `_Thread_local struct` member access, a
  TLS pointer initialised to a global's address, and `&tls_buffer` handed to
  libc `memset`/`snprintf` then read back via direct TLS) returned 1 under wine.
  Fixed together with the PE TLS item above — `tls_aggr.c` now returns 0 on both
  x86_64 (native) and i386 (WoW64).

- [x] **[LIMITATION] `varargs_fp` golden is now libc-aware (msvcrt 3-digit exponent).**
  `tests/qemu/conformance/varargs_fp.c` returned 4 under wine: the FP-varargs
  ABI is correct (checks 1–3 pass), but check 4 compared `%g %e` against the
  glibc 2-digit-exponent form `100 5.000000e-01` while Microsoft's CRT emits a
  ≥3-digit exponent (`…e-001`). The golden now accepts either form, so the row
  passes against glibc, musl, and msvcrt alike. Verified: returns 0 natively
  (x86_64 + i386).

- [x] **[LIMITATION] `floats_libc` golden is now rounding-aware (msvcrt half-away).**
  `tests/qemu/conformance/floats_libc.c` returned 3 under wine: the
  long-double-varargs ABI is correct (checks 1–2 pass), but check 3 formats
  `%.1Lf` of the exact half `0.25L`, where glibc rounds to even → `0.2` and
  msvcrt rounds half-away-from-zero → `0.3` (both conforming). The golden now
  accepts either, and checks 4–5 (`strtod`/`strtold` round-trips) then run and
  pass on msvcrt. Verified: returns 0 natively (x86_64 + i386).

- [x] **[LIMITATION] `libc_struct` resolves `lldiv` at runtime (wine's builtin msvcrt lacks it).**
  `tests/qemu/conformance/libc_struct.c` returned 1 under wine: a static import
  of `lldiv` aborts the loader before `main` (`wine: … unimplemented function
  msvcrt.dll.lldiv`), even though the small-struct-return ABI under test is
  correct. On `_WIN32` the test now resolves `lldiv` via `GetProcAddress` and
  calls it through a function pointer — so on real msvcrt the 16-byte `lldiv_t`
  hidden-sret ABI is exercised, and under wine (where the export is absent) it
  falls back to a manual computation rather than aborting (the lldiv-vs-libc ABI
  is simply unverified there; `div`/`ldiv` still validate struct return against
  the platform libc). ELF builds keep the direct call. Verified: returns 0
  natively (x86_64 + i386), and links even against an import lib without
  `lldiv`.
