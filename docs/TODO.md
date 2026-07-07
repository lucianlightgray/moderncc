# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (then removed).

---

# Now

- [ ] Normalize as much of the CMake code as possible: 1) minimize gating instead preferring autodetecting the existence of tools and enabling as many tests/targets/configs as are available on the host, 2) reduce CMake usage by relying on `tools` where advantageous, 3) fold in separate .cmake files into CMakeLists.txt
- [ ] Can a fully static build use an internal minimalistic `-run`/JIT to sidestep the dynamic linking limitations of static (and use libc/musl in-memory?)
- [ ] **`exec/tls` skipped on arm64+WIN32 (`skipon=arm64/WIN32`, 2026-07-05).**
  On the `msvc / arm64` runner, `exec/tls` intermittently hung (ctest 63 min,
  manual cancel). Root cause is **not** in mcc: **MSVC's arm64 code generator
  miscompiles mcc itself** on the static-`__thread` emission path. The
  MSVC-arm64-built `mcc.exe` nondeterministically drops functions when it
  compiles a `__thread` TU (`tls.c`): some builds/runs lose `main`, others
  truncate a trampoline → the linked exe hangs. Isolation was exhaustive and
  conclusive:
  - mcc's arm64 codegen is **correct** — the same mcc source built by **gcc**
    (x86_64 *and* arm64 Linux) and by **MSVC-x64 cross-targeting arm64-win32**
    all emit a byte-identical, deterministic, correct `tls.s` (50×/30× runs).
  - No mcc source UB: Valgrind clean; `-ftrivial-auto-var-init=pattern` clean.
  - Only **MSVC's arm64 backend building mcc** fails, and it is build/run
    nondeterministic (a later CI build was 0/30 corrupt), so it cannot be
    reproduced or bisected without an arm64 Windows + MSVC box.
  - `exec/tls` still runs (and passes) on **x86_64 WIN32** and on every gcc/clang
    arm64 target, so `__thread` codegen stays covered. The `msvc` ctest step also
    carries `--timeout 300` so any future flaky hang fails fast instead of
    stalling the job.
  - **Tried and reverted (435087ee):** a scoped `#pragma optimize("", off)` around
    the arm64 TLS-access codegen (`arm64_tls_base_x30` + `load`/`store`) did **not**
    fix the hang (`exec/tls` still timed out), so the miscompiled construct is
    *outside* that region. It also used raw `_MSC_VER`/`_M_ARM64`, which the
    `host-gate-invariant` test forbids outside `src/mcchost.{h,c}` — any future
    host-conditional must route through an `MCC_HOST_*` macro defined there.
  - _Next (needs arm64 Windows + MSVC):_ bisect the miscompiled mcc construct.
    A whole-mcc-TU `/Od` on that build is the last-resort blunt workaround, but
    the earlier `MCC_NOOPT` `/Od`-vs-`/O2` probe was inconclusive (the intermittent
    corruption did not reproduce that run), so even that is unverified.
- [ ] **Reconcile divergent test-count claims across docs (validate).**
  `docs/NOTES.md` "Build status" (39/39, 22/22 and 782/782 / 520/520) vs its
  Profiling §7 validation matrix (804/772) cite different totals with no stated
  basis (all three moved out of README/PROFILING in the 2026-07-06 reorg).
  → Regenerate from one `ctest -N` per host/preset and state the per-case vs
  aggregate counting basis; make the docs cite the same source of truth.
- [ ] **Trace the "~100× faster than gcc -O2" headline to a measurement (validate).**
  `README.md:15` says "~100×"; `docs/NOTES.md` Profiling §4b measures 118–204×
  (TU/opt dependent); the NOTES.md "Compile speed & footprint" table shows
  108–141×. → Pick the documented benchmark and make the headline a measured
  range, not a round number.
- [ ] **Re-measure & date-stamp the README speed/size table post-lexer-change
  (validate).** The `docs/NOTES.md` "Compile speed & footprint" table (0.05 s;
  7/19/108/141×) and the ~0.6 MB / ~1.3 MB size claims (`README.md:16` + that
  table) predate the `TOK_HASH_SIZE` change and are toolchain/host-sensitive. →
  Re-run `mccbench` + `size`/`strip` a `dist-*` build; refresh, noting the host
  as PROFILING does.
- [ ] **Regenerate the dated "all green" status prose from CI (validate).**
  `docs/NOTES.md` "Build status" (moved from README) narrates per-preset
  pass/skip counts across ~35 presets; this rots silently. → Derive from the
  latest workflow run, or add a check that fails when the prose diverges from
  actual CTest output.
- [ ] **`va_start` non-last / `register` param check never fires on x86_64
  (impl).** `C9911.md:3215` §7.16.1.4p3 — the SysV macro (`__builtin_va_start` in
  `runtime/include/mccdefs.h`) reads the reg-save area from the frame and never
  references `parmN`, so the misuse diagnostic (present on arm64/riscv64/PE via
  the real `TOK_builtin_va_start` case) is absent on x86_64-SysV and i386.
  → _Deferred:_ making the check target-independent needs x86_64-SysV to lower
  `va_start` through the real builtin (implement `gen_va_start` for SysV) instead
  of the frame-address macro — a codegen rework of the primary target's varargs
  for a diagnostic-only gain; not worth the risk without a driving need.
  Reference test to mirror once fixed: gcc `c-c++-common/Wvarargs-2.c`
  (`va_start` on a non-last / fixed-arg param). See docs/TESTS.md §6-A.3.
- [ ] **External (SHN_UNDEF) thread-local symbols hard-error on Mach-O — TLV
  imports unimplemented (impl).** `src/objfmt/mccmacho.c:2099`. Locally-defined
  `__thread` works (TLV descriptors via `__tlv_bootstrap`); cross-module
  `extern __thread` errors. Documented as an intentional limitation in
  `docs/NOTES.md` (Platform ABI & runtime notes) — revisit only if a real
  cross-module-TLS-on-Darwin need appears; the fix is emitting TLV *import*
  descriptors.
- [ ] **ARM far-branch has no veneer — errors past ±32 MB (fix).**
  `src/arch/arm/arm-gen.c:326` `"FIXME: function bigger than 32MB"`. → Emit a
  long-branch trampoline/island, or downgrade to a documented diagnostic (not FIXME).
- [ ] **i386 fastcall/thiscall: non-register arg before a register arg
  unsupported (impl).** `src/arch/i386/i386-gen.c:530`. → Handle the
  spilled-then-register ordering, or document the accepted ABI limitation.
- [ ] **Validate the remaining i386 TLS + x86_64 32[S] large-address pattern
  assumptions (validate).** x86_64 GD/LD/IE/LE is now covered by the `tls-models`
  ctest (`tests/tls/`, links gcc/clang objects in all four models, dynamic +
  static) — that push fixed real bugs: TLSGD→LE used only the symbol's own
  section size for the TP offset (wrong with a 2nd TLS section), and static GD/LD
  links failed on `__tls_get_addr` (relaxed away, now resolved to 0). STILL OPEN:
  (a) the i386 `R_386_TLS_GD/LDM` pattern paths (`i386-link.c`) need an i386 cross
  build to exercise; (b) the `R_X86_64_32[S] out of range` check (`x86_64-link.c`)
  has no positive test — needs a >2 GB text/data layout to trigger. → Add an i386
  TLS gate under the cross preset, and a forced-high-address link case.
- [ ] **ARM inline-asm `long long` operands unimplemented (impl).**
  `src/arch/arm/arm-asm.c:2465` hard-errors — handle the 64-bit register-pair case.
- [ ] **arm64 inline assembler errors on unmodeled mnemonics (impl).**
  `src/arch/arm64/arm64-asm.c:1877` (+ `:1298/:1441/:1651`). → Enumerate the common
  missing mnemonics; expand the table or document the supported subset.
- [ ] **Resolve/remove the 6 permanently-masked ARM asm encodings (fix).**
  `ARM_KNOWN_FAIL` (tools/mccharness.c:2540) never fails on `bl r3`, `b r3`,
  `mov #0xEFFF`, `mov #0x0201`, two `vmov.f32` forms — real encoding defects. → Fix
  the `mov #imm`/`vmov.f32` cases and drop the entries.
- [ ] **Reference-harness `exec`/`diff3` goldens are effectively dead (validate).**
  `tests/exec/goldens.h:19/53/54/62` (inline multi-unit, backtrace, btdll, alias)
  carry full expected output but SKIP for lack of a reference harness. → Confirm
  each is exercised elsewhere (mcctest/diff); otherwise wire up the harness.
- [ ] **Windows keeps diagnostic color off unconditionally (validate).**
  src/mcchost.c:21 — suppresses color even on VT-enabled Windows Terminal. →
  Probe `ENABLE_VIRTUAL_TERMINAL_PROCESSING`; confirm `-fdiagnostics-color=always`
  still forces it. Low priority.
- [ ] `-fverbose-asm`-style operand comments: meaningful comments need
  codegen-side variable/spill metadata that is discarded after emission;
  classified low-value (reloc symbol names are already printed). Revisit
  only if a debugging workflow materializes that needs it.
- [ ] **CST slice-I symbol resolution is last-declaration-wins (validate/decide).**
  NOTES CST slice I: no scope stack, so a name shadowed across scopes can
  mis-resolve `use→def`. the CST D4 gap analysis flagged this for a failing `sym_ref` shadowing
  fixture to force the decision. → Add the shadowing test; either build a scope
  stack or record the limitation as intentional with the test as the boundary.
- [ ] **CST slice-J macro-invocation v1 imprecisions (validate/decide).**
  NOTES CST slice J: function-like invocations may drop the trailing `)`, and
  object-like macros used inside another macro's args stay plain tokens. Round-trip
  still holds. the CST D4 gap analysis flagged failing tests to decide fix-vs-keep. → Add the
  fixtures; fix or record as accepted v1 with the test pinning the boundary.
- [ ] **CST 5B incremental splice + `H_e` epoch hash are designed, not built (impl).**
  NOTES CST §3.1/§10: the invertible epoch hash + tombstone sweep (O(1)-per-level
  incremental rehash for live edits) and the 5B splice are reserved (slot-key field
  + frontier-scoped `H_s`-recompute ship) but unbuilt; they're LSP/5B-era and gated
  on 4B rolling-hash + error-recovery + `Error`/`Missing` nodes. → Build when the
  LSP consumer lands. Note: D3 repurposed `slot_key` for branch tags, so an `H_e`
  build must reconcile that column's dual use.
- [ ] **Write `docs/CONFIG.md` reconciling code preprocessor names vs. CMake
  config (doc/tooling).** Enumerate every unique preprocessor name in the codebase
  — `#define`/`#ifdef`/`#if defined` macros, especially the `CONFIG_MCC_*` family
  (~30 in `src/`) and any `MCC_*` build/host gates — and cross-check them against
  the CMake config surface (the 55 `mcc_config_node` declarations in
  `CMakeLists.txt`, the `target_compile_definitions`, and preset/cache flags).
  Flag: (a) `CONFIG_MCC_*`/`MCC_*` macros the code reads but no `mcc_config_node`
  defines (undocumented/implicit), (b) config nodes defined but never read,
  (c) name-drift between the CMake option and the emitted `-D`. Prefer a `tools/`
  checker (mirror `tools/hostgate.c` / `ckbuildmd.c`) that greps both sides and
  fails on divergence, so CONFIG.md can't rot. → Then update `docs/BUILD.md` (which
  already tables the CMake nodes, §3–§14) to become the ongoing source of truth for
  in-code flags, cross-linked to CONFIG.md, and wire the checker into ctest.

---

# C99/C11 test-coverage backlog (from docs/TESTS.md)

Each item ports/mirrors a specific gcc/clang conformance test into an mcc test —
runtime cases go in `tests/exec/features_c99_c11/`, diagnostics/negatives in
`tests/diff/parts/` (or a new reject corpus). Reference paths are relative to
`~/Projects/gcc/gcc/testsuite` (gcc) and `~/Projects/llvm-project/clang/test/C`
(clang). Context + gap matrix: docs/TESTS.md §5–§6. The `va_start` diagnostic gap
(§6-A.3) is tracked with its own item above.

## Real semantic/diagnostic gaps — fix mcc, then add the test

- [ ] **§6.7.4p6 — plain `inline` w/o external definition emits a global (fix+test).**
  An `-O0` call to an inline-only function *links* in mcc (rc 0) where gcc/clang fail
  with `undefined reference` (rc 1): an inline definition is not an external
  definition. Fix the linkage semantics, then add a 2-TU test asserting the call is
  unresolved (and that adding `extern` makes it resolve). _Ref:_ gcc
  `gcc.dg/inline-20.c` (C99 `-fno-gnu89-inline` emission, `scan-assembler`),
  `gcc.dg/inline-15.c`, `gcc.dg/inline-19.c`.
- [ ] **§6.9.1p6 — K&R identifier-list params default to `int` with only a warning
  (fix+test).** `int g(x){return x;}` compiles in mcc (rc 0); gcc `-std=c11` and
  clang reject (C99 removed implicit int). Promote to a default-mode error; add a
  reject test. _Ref:_ gcc `gcc.dg/c99-impl-int-1.c`, `c99-impl-int-2.c`,
  `c11-old-style-definition-1.c`, `c11-unproto-1.c`.
- [ ] **§7.26.1 — bundled `<threads.h>` shadows the system header (fix).** mcc's
  `include/threads.h` resolves ahead of glibc's (`-M` confirms), the root of the
  `c11_threads` divergence. Fix header-search precedence / align the shim. _No direct
  gcc/clang test to port_ (mcc-internal include-path behavior); validate against the
  existing `tests/exec/features_c99_c11/c11_threads.c`. See docs/TESTS.md §6-A.4.

## Coverage-depth gaps — mcc passes but under-tests vs gcc/clang; add tests

- [ ] **Flexible array members — add a constraint+runtime suite.** mcc has ~1 case,
  gcc 12. Cover union FAM, string-init, typedef FAM, `sizeof`, invalid nesting.
  _Ref:_ gcc `gcc.dg/c99-flex-array-{1,2,3,5,6,7}.c`,
  `gcc.dg/c99-flex-array-typedef-{1,2,3,5,7,8}.c`.
- [ ] **`_Noreturn` — expand from 1 case to full coverage.** noreturn-fn-that-returns
  (UB), `_Noreturn main`, constraint violations, `<stdnoreturn.h>` macro. _Ref:_ gcc
  `gcc.dg/c11-noreturn-{1,2,3,4,5}.c` (`-2` is execute).
- [ ] **`_Alignas`/`_Alignof` — add over-align runtime + constraint diagnostics.**
  _Ref:_ gcc `gcc.dg/c11-align-{1,2,3,4,5,6,7,8,9}.c` (`-2`/`-6` execute; `-3` 28
  dg-error, `-5` 14).
- [ ] **VLA goto/switch-into-scope diagnostics.** mcc tests VLA runtime but not the
  jump-into-VLA-scope constraint. _Ref:_ gcc `gcc.dg/c99-vla-jump-{1,2,3,4,5}.c`.
- [ ] **UCN-in-identifier breadth.** mcc has 4 UCN cases, gcc ~30. _Ref:_ gcc
  `gcc.dg/ucnid-*.c`; clang `C99/n717.c` (UCN grammar), `C11/n1518.c` (UAX#31).
- [ ] **FP evaluation-method / Annex F wide returns.** `FLT_EVAL_METHOD`,
  intermediate precision, wide-return conformance. _Ref:_ gcc
  `gcc.dg/c11-float-{1..8}.c`; clang `C11/n1365.c`, `C11/n1396.c` (per-target IR).
- [ ] **`_Complex` diagnostics + Annex G special values.** mcc is arithmetic/ABI
  strong but light on constraint diagnostics and CMPLX/NaN/inf edge cases. _Ref:_
  clang `C11/n1464.c` (CMPLX/`__builtin_complex`), `C11/n1514.c` (Annex G); gcc
  `gcc.dg/c99-complex-{1,3}.c`.
- [ ] **Negative/diagnostic test tier.** mcc's exec suite is execute-first; ~70% of
  gcc's C99/C11 files are `dg-error` negatives. Build a golden-stderr reject corpus
  (`tests/reject/` or a `tests/diff/parts` extension) mirroring the
  `-pedantic-errors` negatives. _Seed refs:_ gcc `gcc.dg/c99-typespec-1.c` (1055
  dg-error), `c11-align-3.c`, the `c99-flex-array-*` / `c11-*` negative files.

## Conforming omissions — assert, don't implement

- [ ] **Pin the feature-subset macros with a test.** `_Imaginary` (Annex G) and Annex
  K are consensus omissions (mcc==gcc==clang); add a test asserting mcc advertises
  them correctly: `__STDC_NO_COMPLEX__` absent, `__STDC_NO_VLA__`/`ATOMICS`/`THREADS`
  as appropriate, `__STDC_IEC_559_COMPLEX__`, `ATOMIC_*_LOCK_FREE`. _Ref:_ clang
  `C11/n1460.c`, `C11/n1514.c`, `C11/n1482.c`.

---

ACHTUNG!!! DO NOT DO!!! WARNING!!!

* Use only human friendly warnings/errors, backed by tests that check formatted output against terminal dimensions/configuration
* Implement/finish `-g` debugging/debugger and flesh out gdb/etc test cases, check against gcc and clang sources of truth
* Optimization -O1...100 levels measured in max seconds to spend optimizing?
* Hot reload by saving/loading CST snapshots on the fly and on run with --hotreload arg
* Run hot-reloads from reconciled CST snapshots

ACHTUNG!!! DO NOT DO!!! WARNING!!!

---
