# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (then removed).

---

## Open items (priority order)

_From CI run [28741671440](https://github.com/lucianlightgray/moderncc/actions/runs/28741671440)
(`0e700c7`, push 2026-07-05) — overall **green** (0 failures across linux×18,
macos×4, msvc×2, mingw, qemu×10, dist×9). These are the warnings / skips / matrix
open questions that green run left standing._

### macho-* on native macOS

- [~] **`macho-*` native macOS — partially enabled (2026-07-05).**
  Root cause found and fixed for the structural test: the four tests gated on
  `EXISTS "${MCC_CROSS_DIR}/mcc-x86_64-osx"`, a **configure-time** file check that is
  always false during a self-contained `MCC_ENABLE_CROSS` build (the cross compiler
  is built later, by that same build). Changed the gate to accept the in-tree
  `TARGET mcc-x86_64-osx` too — **scoped to Darwin** (`_have_osx_cross` in
  CMakeLists.txt) so Linux/`cross` behaviour is left exactly as before (the
  exec-based macho harnesses run x86_64 code via a custom loader and I could not
  verify them on an x86_64-Linux runner from this host). On the `macos-cross` jobs
  (which build `mcc-x86_64-osx` in `all`) the tests are now registered instead of
  dropped at configure.
  - **`macho-structural` now RUNS natively on macos-cross** (verified: PASS on
    macos-arm64) — it is structural-only (parses the emitted Mach-O with `objcheck`,
    host-independent), so no x86_64 execution is needed.
  - **`macho-codegen-run` / `macho-image-run` / `macho-apple-libc` still SKIP**, now
    at runtime with `SKIP: host is not x86_64`: each *executes* the emitted
    **x86_64** Mach-O and is hard-gated by `host_is_x86_64()` (compile-time
    `__x86_64__`) in `tools/mccharness.c`. On the Apple-Silicon `macos` runners
    (arm64, Rosetta not installed on the non-dist jobs) this is correct. Remaining
    work to cover them natively: either (a) install Rosetta 2 on the macos-cross
    jobs and relax `host_is_x86_64()` to a runtime "can we exec an x86_64 Mach-O"
    probe, or (b) add native **arm64-osx** variants (`mcc-arm64-osx` +
    `--arch arm64`) so the run path exercises the native slice with no emulation.
    The plain (non-cross) `macos` jobs still cover native Mach-O end-to-end via the
    unconditional `macho-conformance-native` + `macho-stack-protector` +
    `macho-universal` tests (all PASS).

## CI skip-validation audit (2026-07-05, run 28741671440)

**Guiding principle:** a SKIP must be justified *per platform*. Suite-level gates
(e.g. "needs gcc **and** clang", "needs POSIX shell", "native host only") currently
drop whole groups on macOS even when an individual test's **exec-only or 2-way
(mcc-vs-clang) subset** would run fine, or when the test is natively applicable to
this arch. For every entry below: confirm the gate is genuinely unsatisfiable on
that platform; **if any subset can run, split it out and run it** rather than
skipping the whole test. 106 unique skipped tests observed across the 4 macOS jobs.

- [ ] **diff3 suite — gated by `diff3-suite "needs native host + both gcc and clang"`.**
  On macOS `gcc` *is* Apple clang, so the 3-way diff has no second distinct
  reference and the entire suite is dropped. Investigate a 2-way fallback
  (mcc-vs-clang) and/or routing each case's `exec/` equivalent so codegen is still
  exercised on Darwin/arm64. Note especially `arm64_extasm` (natively arm64 —
  should run here) and the many pure-C cases (`alignas`, `bitfields`, `builtins`,
  `c11_complex_convert`, `c11_threads`, `cleanup`, `floating_point_literals`,
  `scopes`, `struct_init`, `ternary_op`, `types`, `old_func`) that are not
  platform-specific at all. Validate each:
  - [ ] `diff3/al_ax_extend`  - [ ] `diff3/alias`  - [ ] `diff3/alias_single_tu`
  - [ ] `diff3/alignas`  - [ ] `diff3/arm64_extasm`  - [ ] `diff3/array_assignment`
  - [ ] `diff3/asm_constraints_x86`  - [ ] `diff3/asm_data_directives`
  - [ ] `diff3/asm_goto`  - [ ] `diff3/asm_lvalue_cast`  - [ ] `diff3/asm_operand_modifiers`
  - [ ] `diff3/asm_outside_function`  - [ ] `diff3/asm_sections`
  - [ ] `diff3/atomic_inlang_rmw`  - [ ] `diff3/backtrace`  - [ ] `diff3/bitfields`
  - [ ] `diff3/bound_setjmp`  - [ ] `diff3/bound_setjmp2`  - [ ] `diff3/bound_signal`
  - [ ] `diff3/btdll`  - [ ] `diff3/builtin_inf_nan`  - [ ] `diff3/builtins`
  - [ ] `diff3/c11_complex_convert`  - [ ] `diff3/c11_threads`  - [ ] `diff3/cleanup`
  - [ ] `diff3/fastcall`  - [ ] `diff3/floating_point_literals`  - [ ] `diff3/grep`
  - [ ] `diff3/inline`  - [ ] `diff3/old_func`  - [ ] `diff3/riscv_asm`
  - [ ] `diff3/scopes`  - [ ] `diff3/struct_init`  - [ ] `diff3/ternary_op`
  - [ ] `diff3/types`  - [ ] `diff3/weak_undef`  - [ ] `diff3/winarm64_interlocked`

- [x] **exec suite — per-test runner gates: CONFIRMED INTENDED (2026-07-05).**
  Audited every `exec/` case that SKIPs on macos-arm64 against the golden `flags`
  field in `tests/exec/goldens.h` and the gate in `tests/exec/runner.c`. All are
  correctly gated, none is a macOS-specific miss:
  - Arch/ISA-specific inline asm — `flags` carries `cpu=x86` / `cpu=x86_64` /
    `cpu=i386` / `cpu=riscv64` / `cpu=arm64,os=WIN32`: `asm_goto`, `asm_lvalue_cast`,
    `asm_operand_modifiers`, `asm_constraints_x86`, `al_ax_extend`, `fastcall`,
    `riscv_asm`, `winarm64_interlocked`. Cannot run on Darwin/arm64 by construction.
  - ELF-only GAS features — `flags` carries `asm,elf`: `asm_data_directives`,
    `asm_sections` (Mach-O has no equivalent for those `.section`/data directives).
  - Reference-harness placeholder goldens — `flags` carries `note:…`, which the
    runner skips on **every** platform (they also SKIP on x86_64 Linux):
    `inline` (multi-unit symbol-export harness), `alias` (multi-unit alias harness),
    `backtrace`/`btdll` (backtrace reference harness), `array_assignment` (xfail
    whole-array assignment). Not a Darwin gap — they need a reference harness that
    does not exist on any target yet.

- [ ] **cli suite — gated by `cli-suite "structural readelf/nm suite runs on native host only"` / ELF-isms.**
  Many are genuinely ELF/Linux-only (`shared_dyn_soname`, `rpath_new_dtags_runpath`,
  `tls_segment_and_run`, `fpic_pie_dyn`, `fno_pic_exec`, `section_attribute`,
  `visibility_attribute`, `fvisibility_hidden_default_wins`, `stack_protector_on/off`,
  the `debug_*`/`dwarf*`/`stabs` group). But several are format-agnostic and should
  have a Mach-O equivalent on macOS: `dumpmachine`, `uchar_header`,
  `builtin_nan_inf_const`, `builtin_signbit_no_trap`, `complex_creal_function`,
  `constructor_init_array` (→ `__mod_init_func`), `leading_underscore` (Mach-O
  *does* lead with `_`), `symbol_type_func_object`, `assemble_dot_s_file`,
  `dash_S_emits_assembly`, `common_symbol_merge`, `weak_override_multi_tu`,
  `atomic_*`, `nostdinc_drops_system`. Validate each; port the format-agnostic ones
  to run on Darwin:
  - _Port mechanism (2026-07-05):_ each case in `tests/cli/cases.h` skips via its
    requirement string (`"cpu=x86_64,os=linux"` → `os_eq(os,"linux")` in
    `tests/cli/runner.c`). A port is **two** edits per case, not one: (1) loosen the
    requirement (drop `os=linux`/`cpu=x86_64` where truly format-agnostic), **and**
    (2) make the `expect` shell pipeline target-adaptive — today they grep for ELF/
    x86_64 literals (e.g. `dumpmachine` greps `x86_64`; the readelf/nm probes assume
    ELF). The runner already exports `MCC_TEST_CPU`/`MCC_TEST_OS`, so pipelines can
    branch on those and use `otool`/`nm` on Darwin. This must not regress x86_64
    Linux, so each ported case needs both a Mach-O/arm64 and an ELF/x86_64
    expectation — do it per-case with local `macos` + a Linux/Docker cross-check.
  - [ ] `cli/assemble_dot_s_file`  - [ ] `cli/atomic_inlang_aggregate`
  - [ ] `cli/atomic_rmw_unsupported`  - [ ] `cli/builtin_nan_inf_const`
  - [ ] `cli/builtin_signbit_no_trap`  - [ ] `cli/common_symbol_merge`
  - [ ] `cli/complex_creal_function`  - [ ] `cli/constructor_init_array`
  - [ ] `cli/dash_S_emits_assembly`  - [ ] `cli/debug_default_stabs`
  - [ ] `cli/debug_dwarf5_info`  - [ ] `cli/debug_dwarf_version_select`
  - [ ] `cli/debug_gstabs`  - [ ] `cli/dumpmachine`  - [ ] `cli/dwarf_line_table`
  - [ ] `cli/fcommon_vs_default`  - [ ] `cli/fno_pic_exec`  - [ ] `cli/fpic_pie_dyn`
  - [ ] `cli/function_data_sections_accepted`  - [ ] `cli/fvisibility_hidden_default_wins`
  - [ ] `cli/leading_underscore`  - [ ] `cli/nostdinc_drops_system`
  - [ ] `cli/rdynamic_exports_main`  - [ ] `cli/relocatable_partial_link`
  - [ ] `cli/rpath_new_dtags_runpath`  - [ ] `cli/section_attribute`
  - [ ] `cli/shared_dyn_soname`  - [ ] `cli/shared_dynamic_tags`
  - [ ] `cli/stack_protector_off`  - [ ] `cli/stack_protector_on`
  - [ ] `cli/strip_symbols`  - [ ] `cli/symbol_type_func_object`
  - [ ] `cli/tls_segment_and_run`  - [ ] `cli/uchar_header`
  - [ ] `cli/visibility_attribute`  - [ ] `cli/weak_override_multi_tu`

- [ ] **preprocess 3-way cases — gated with the parts/diff 3-way (needs distinct gcc+clang).**
  Pure preprocessor behavior, not platform-specific; a 2-way (mcc-vs-clang) or
  self-check variant should run on macOS. Validate each:
  - [ ] `preprocess/conditional/directives_in_args`
  - [ ] `preprocess/expansion/standard_example`
  - [ ] `preprocess/variadic/gnu_comma_paste`

- [ ] **standalone / cross-target tests skipped on macos-arm64.** Confirm each is
  truly inapplicable natively; where a subset applies (e.g. `dash-s-roundtrip`,
  `asm-gas-directives`, `asm-c-connect-test` are not obviously Linux-only), enable it:
  - [ ] `asm-c-connect-test`  - [ ] `asm-gas-directives`  - [ ] `compile.win32` (PE — x-target)
  - [ ] `dash-s-bytes-arm64`  - [ ] `dash-s-bytes-riscv64`  - [ ] `dash-s-roundtrip`
  - [ ] `i386-fastcall-abi` (x86-only)  - [ ] `pe-native-conformance`  - [ ] `pe-wine-conformance`
  - [x] `mcctest` / `mcctest-bcheck` — **CONFIRMED INTENDED (2026-07-05).** The
    macos-gcc / macos-cross-gcc jobs resolve `CC` to a real Homebrew **GNU gcc**
    (not Apple clang — the job does `gcc-<N>` off `brew --prefix`), and the
    `mcctest` differential compares mcc byte-for-byte against that reference.
    `CMakeLists.txt` skips it on Darwin because a GNU gcc reference diverges from
    mcc (which matches the Apple/clang ABI + impl-defined choices) on
    impl-defined / UB corners; the Apple-clang `mcctest` (macos-clang jobs) plus
    the diff3-suite already cover that surface. Decision, not a gap.

- [ ] **Larger-input lexer benchmarking.** Re-run the idea #1–#4 style per-token
  lexer micro-optimization experiments against the amalgamation TU
  (`mcc-gcc … -E src/mcc.c`) instead of the 416-line `full_language.c`, where a
  1–2% change sits below the noise floor. `src/mcc.c` gives a ~0.8% floor
  (σ ≈ 0.5 ms) with 4× the absolute signal (see `docs/PROFILING.md` §4b, §8).
- [ ] **Attack the hot lexer clusters.** The two genuinely hot instruction
  clusters in `next_nomacro` are the identifier-interning hash-chain walk and the
  per-byte hash computation (`docs/PROFILING.md` §5a). A measurable win targets
  those — e.g. hash-table load factor / chain length (`TOK_HASH_SIZE`) or the
  per-char `TOK_HASH_FUNC` cost. Riskier than the applied micro-optimizations, and
  wants a bigger benchmark to see signal.

Resolved 2026-07-05 (run 28741671440 follow-ups): **CI Node.js 20 deprecation**
(`actions/cache@v4` → `@v5`, Node-24 native; every other action already on
checkout@v5 / artifact@v7); **bench aarch64 CPU model** (`decode_arm_midr` in
`tools/bench.c` maps `/proc/cpuinfo` `CPU implementer`+`CPU part` → friendly name,
e.g. `ARM Neoverse-N1`, with vendor/hex fallback); **bench dead JUnit lookup**
(`write_tests` now emits nothing when no XML is present instead of the misleading
"no JUnit file at …" line); **linux + qemu job summaries** (new
`ci junit-summary <xml>` verb renders a PASS/FAIL/SKIP markdown table into
`$GITHUB_STEP_SUMMARY`; `ci run-preset`/`ci qemu` now pass `--output-junit` so the
18-cell linux matrix and 10 qemu cells surface their ctest results — benchmark
report intentionally omitted under qemu-user, where emulated timings are noise);
**exec-suite skip audit** and **macos-gcc `mcctest` skip** confirmed intended
(above).

Resolved 2026-07-04: arm64 atomics/bounds/complex link
failures (outline-atomics + `__unordtf2`), Rosetta x86_64 macOS (validated
end-to-end locally under Rosetta — build, `-run`, ctest subset; CI confirms on
push), the Mach-O fat-binary suite (landed with the universal-binary work), and
the conformance batch (see Notes). Broader conformance tracking continues in
`docs/C9911.md`.

## Notes

- **Conformance (2026-07-04).** Now default errors (downgradable via
  `-Wno-error=`): return-mismatch (§6.8.6.4p1) and implicit-function-declaration
  (§7.21.7.7); `va_start` non-last-param (§7.16.1.4p3) is warned. Two further
  candidate flips were evaluated and **intentionally not made** — they contradict
  mcc's demonstrated design, so they are decisions, not gaps: (a) *implicit-int on
  K&R params → error* — mcc deliberately **supports** K&R old-style functions (the
  `#if __MCC__` block in `tests/diff/parts/legacy_preproc.h` exists specifically to
  test mcc compiling them, where gcc/clang reject them), so it stays a warning; a
  whole-`warn_implicit_int` flip also over-reaches (breaks `cli/implicit_int_diag`
  + `exec/errors_and_warnings`). (b) *inline-only linkage (§6.7.4p6)* — mcc
  inlines-and-links an inline function with no external definition, which the
  standard *permits*; strictly more useful than gcc/clang's `-O0` link failure, so
  it stays.

- **`static` reentrancy sweep (done; from the former `STATIC.md`).** Every
  per-compilation mutable static in `src/` was moved off static storage —
  per-call scratch onto the stack, persistent per-instance state rehomed into
  `MCCState` (the `#define name mcc_state->name` idiom). `mccgen.c` and `mccpp.c`
  now have **zero** mutable statics; per-function backend state (`arch/*-gen.c`)
  collapsed onto shared `cg_*` fields since only the active target compiles.
  Verified `debug` + `cross` (all five backends), 804/804.
  - *Deliberately retained as process-global* (moving into `MCCState` would only
    relocate a global — the async/loader contexts reach them with no `MCCState`):
    async signal/fault handler state (`mccrun.c` `g_rc`/`g_s1`/`signal_set`,
    `mcchost.c` `host_fault_cb`); the Windows DLL loader `mcc_module`;
    process-wide allocator accounting (`libmcc.c` `mem_debug_chain`/`mem_cur_size`/
    `mem_max_size`/`nb_states`/`reallocator`); immutable cached singletons
    (`libmcc.c` `auto_mccdir_buf`, `mcchost.c` `host_macos_sdk_root`, `mcc_syms[]`).
    Do not "fix" these in a future sweep.
  - *Bugs fixed along the way* (both covered by tests, in git history): complex-type
    cache use-after-free across TUs on one `MCCState` (now cleared in
    `mccgen_finish`; `tests/embed/api_extra.c` `test_multi_tu_complex`); qemufetch
    dir-ordering (`host_mkdirs(dest)` hoisted before the `curl -o` download in
    `tools/mccharness.c`).

- `-fverbose-asm`-style operand comments: meaningful comments need
  codegen-side variable/spill metadata that is discarded after emission;
  classified low-value (reloc symbol names are already printed). Revisit
  only if a debugging workflow materializes that needs it.

---

ACHTUNG!!! DO NOT DO!!! WARNING!!!

• Implement/finish `-g` debugging/debugger and flesh out gdb/etc test cases
• Can a fully static build use a minimalistic `-run` to sidestep the dynamic linking limitations and use libc or musl in-memory?
• What is the purpose of libmccrt.a ? Can it be replaced with ctests?
• Use only human friendly warnings/errors, backed by tests that check formatted output against terminal dimensions/configuration
• CST Database for Debugging, LSP, and Optimization data/layers
• CST Database uses hierarchical incremental hashes to enable bidirectional lookups starting from any character index in any file
• Hot reload by saving/loading CST snapshots on the fly and on run with --hotreload arg
• Run hotreloads from reconcoliled CST snapshots

ACHTUNG!!! DO NOT DO!!! WARNING!!!

---
