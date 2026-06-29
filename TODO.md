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

- [ ] **[TASK] The qemu cross-conformance matrix is not wired into CI.**
  `tests/qemu/conformance/` (gated by `MCC_QEMU_TESTS`) covers
  x86_64/i386/arm/arm64/riscv64 × glibc/musl, built twice (default and
  `-fPIC -pie`), and runs either under native qemu-user or via the Docker
  runner (`tests/qemu/docker`, `docker build -t mcc-qemu …`). No CI workflow
  invokes it — `.github/workflows/` is empty. Task: add a workflow that runs
  the portable suites plus the qemu matrix on every push.

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

- [ ] **[TASK] Systematic diagnostic-coverage sweep.**
  Each `[DIAG]`/`[FEATURE]` item ships with a cli/exec test that asserts the
  diagnostic fires and that the valid forms still compile. A mechanical sweep
  to confirm coverage parity for the few diagnostics that predate this tracker
  is outstanding (not blocked on anything).

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

- [ ] **[BUG] PE thread-local storage (`_Thread_local` / `__thread`) reads the wrong address.**
  On a PE target the backend still emits the **ELF** Local-Exec TLS sequence —
  i386 `movl %gs:0,%reg ; addl $sym@ntpoff,%reg` with `R_386_TLS_LE`
  (`src/arch/i386/i386-gen.c:284`, `:337`); x86_64 `movq %fs:0,%reg ; addq
  $sym@tpoff,%reg` with `R_X86_64_TPOFF32` (`src/arch/x86_64/x86_64-gen.c:344`,
  `:353`) — and the PE object writer never populates the TLS data directory
  (`IMAGE_DIRECTORY_ENTRY_TLS` = 9 is `#define`d at `src/objfmt/mccpe.c:154` but
  otherwise unused): no `.tls` template section, no `IMAGE_TLS_DIRECTORY`, no
  `_tls_index`, no TLS-callback list. Windows TLS is unrelated to the ELF thread
  pointer — `_Thread_local x` is reached through the TEB (x64: `%gs:0x58`
  `ThreadLocalStoragePointer` → `[_tls_index]` → `+ offset`; x86: `%fs:0x2c`),
  with the loader copying the `.tls` template per thread. So the emitted
  `%fs/%gs`-relative access lands on an unrelated address and every thread-local
  reads as 0/garbage. Observed: `tests/qemu/conformance/tls.c` returns 1 under
  wine (`pe-wine-conformance`, x86_64-win32 + i386-win32) — the first check
  `counter != 100` fails; an instrumented probe under wine prints `counter=0`
  (want 100). Directly mirrors the Mach-O TLS item above (ELF on all five arches
  is correct). Fix: emit a `.tls` section + `IMAGE_TLS_DIRECTORY` (index var,
  raw-data start/end, callback array) in `src/objfmt/mccpe.c`, and switch the
  PE TLS access in i386/x86_64 codegen to the TEB `_tls_index` sequence.

- [ ] **[BUG] PE thread-local aggregates / pointer-init / `&tls`-into-libc paths.**
  `tests/qemu/conformance/tls_aggr.c` returns 1 under wine (x86_64-win32 +
  i386-win32). Same root cause as the PE `_Thread_local` item above (no
  `IMAGE_TLS_DIRECTORY` / wrong access sequence), but it is the only conformance
  program exercising three further codegen paths, which stay blocked until PE
  TLS lands and must be re-checked then: a `_Thread_local struct` (member access
  = thread-pointer + sym + member offset), a `_Thread_local` pointer initialised
  to a global's address (a relocation that must live in the per-thread `.tls`
  init image, not `.data`), and materialising `&tls_buffer` to hand to libc
  (`memset`/`snprintf`) then reading it back via direct TLS. Fix with the PE TLS
  item above.

- [ ] **[LIMITATION] msvcrt prints `%e`/`%g` exponents with 3 digits — `varargs_fp` golden is glibc-shaped.**
  `tests/qemu/conformance/varargs_fp.c` returns 4 under wine (x86_64-win32 +
  i386-win32). The FP-varargs ABI is **correct**: checks 1–3 pass (`%.2f`;
  interleaved int/double; ten doubles spilling past the 8 FP registers), so the
  values reach libc in the right class and order. The sole divergence is the
  exponent width — a probe under wine prints `100 5.000000e-001` where the golden
  (and glibc, per C §7.21.6.1 "the exponent contains at least two digits") is
  `100 5.000000e-01`. Microsoft's CRT has long emitted a ≥3-digit exponent. Not
  an `mcc` defect — outside its control. Task: make the FP-varargs golden
  libc-aware (normalise/accept the msvcrt exponent form) so the row passes under
  wine/Windows.

- [ ] **[LIMITATION] msvcrt rounds `%.*Lf` half-away-from-zero; `floats_libc` golden assumes round-to-even.**
  `tests/qemu/conformance/floats_libc.c` returns 3 under wine (x86_64-win32 +
  i386-win32). The long-double-varargs ABI is **correct**: check 1 (`%.2Lf` of
  `3.5L` → `3.50`) and check 2 pass, so the value reaches libc. Check 3 formats
  `%.1Lf` of `0.25L`: glibc rounds to even → `0.2` (the golden), msvcrt rounds
  half away from zero — a probe under wine prints `7 0.5 0.3`. A C-library
  rounding-mode difference, not `mcc`. (Checks 4–5, the `strtod`/`strtold`
  round-trips, are unreached; `strtold` may additionally be absent from msvcrt.)
  Task: make the golden rounding-aware, or skip the exact half-ulp case on msvcrt.

- [ ] **[LIMITATION] wine's builtin msvcrt lacks `lldiv`, aborting `libc_struct` before `main`.**
  `tests/qemu/conformance/libc_struct.c` returns 1 under wine (x86_64-win32 +
  i386-win32). `lldiv` is present in the import lib (`runtime/win32/lib/msvcrt.def`
  — added so the link resolves; real `msvcrt.dll` exports it), but **wine's**
  builtin `msvcrt.dll` does not implement it, so the loader aborts at import
  resolution — `wine: ... unimplemented function msvcrt.dll.lldiv, aborting` —
  before `main` runs; the rc=1 is wine's abort, not a failing check. The
  small-struct-return ABI the test targets is in fact correct: a probe under wine
  shows `div(17,5)=q3 r2`, `div(-17,5)=q-3 r-2`, `ldiv=q1000 r3` all correct.
  Only `lldiv` (16-byte `lldiv_t` via the win64 hidden-sret path) stays
  unverified because it can't load under wine; it should pass on real Windows.
  Task: gate the `lldiv` case behind a runtime availability check, or run this
  row on a non-wine Windows host.
