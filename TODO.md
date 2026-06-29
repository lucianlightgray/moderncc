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
