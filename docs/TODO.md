# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (then removed).

---

## Open items (priority order)

_From CI run [28741671440](https://github.com/lucianlightgray/moderncc/actions/runs/28741671440)
(`0e700c7`, push 2026-07-05) — overall **green** (0 failures across linux×18,
macos×4, msvc×2, mingw, qemu×10, dist×9). These are the warnings / skips / matrix
open questions that green run left standing._

- [ ] **CI: Node.js 20 deprecation (all 10 qemu jobs).** The run's only annotations
  are 10 identical warnings: `actions/cache@v4` targets Node.js 20 and is being
  force-run on Node.js 24 (GitHub is
  [removing Node 20](https://github.blog/changelog/2025-09-19-deprecation-of-node-20-on-github-actions-runners/)).
  Bump `actions/cache` (and audit any other `@v*` actions still on Node 20) to a
  Node-24-native release before the forced fallback is withdrawn.

- [ ] **bench: CPU model unresolved on aarch64 Linux.** The benchmark "System"
  block prints `cpu : ?` on both `dist-linux-gcc-arm64` and `dist-linux-clang-arm64`,
  while every other target (x86_64 Linux `AMD EPYC`, Windows-arm64 `Cobalt 100`,
  macOS `Apple M1`) resolves correctly. The host probe in the benchmark tool
  doesn't parse arm64 `/proc/cpuinfo` (no `model name` field there). Add an
  aarch64 fallback (`CPU implementer`/`CPU part`, or `/sys/.../midr_el1`).

- [ ] **bench: "no JUnit file bench-tests.xml" in 12/16 benchmark jobs.** Every
  msvc / mingw / windows-dist / macos-dist / linux-dist benchmark report ends with
  `Test results: (no JUnit file at .../bench-tests.xml)`; only the 4 native macOS
  jobs populate it. Either wire the benchmark ctest invocation to emit
  `--output-junit bench-tests.xml` so the "Test results" section is real on those
  jobs, or drop the dead lookup from the report so it stops implying a missing file.

- [ ] **test: `mcctest` / `mcctest-bcheck` skipped when the host cc is invoked as
  `gcc` on macOS.** `macos-gcc` and `macos-cross-gcc` SKIP both; `macos-clang` /
  `macos-cross-clang` PASS them — the only coverage delta between the two host
  compilers (clang 707 pass / gcc 705 pass). On macOS "gcc" *is* Apple clang, so
  the gate that skips the bounds-checker under a gcc host leaves it unexercised on
  that runner for no real reason. Confirm intended, else loosen the gate so the
  bcheck suite runs regardless of the host-compiler alias on Darwin.

- [ ] **CI coverage: native linux jobs surface no test/benchmark summary — enable it.**
  In the run page only macos/msvc/mingw/dist jobs emit a ctest table and benchmark
  report; the 18-job linux matrix contributes neither, so its skips/timings are
  invisible on the run summary (this report can only confirm linux is green, not
  *what* it ran). Investigate why the linux jobs don't attach the job-summary,
  then enable/implement the same ctest-table + benchmark job-summary the macOS
  jobs produce (`--output-junit`, benchmark report step) for every linux matrix cell.

- [ ] **CI coverage: qemu jobs surface no test/benchmark summary — enable it.**
  Same as above for the 10 qemu jobs (x86_64/i386/arm/arm64/riscv64 × glibc/musl):
  they contribute neither a ctest table nor a benchmark report to the run summary,
  so per-arch skips and cross-run regressions are invisible here. Investigate
  whether the benchmark tool even runs under qemu-user (timing under emulation is
  noisy — decide correctness-only vs. timed), then enable/implement a qemu
  job-summary: at minimum the ctest PASS/SKIP table, and a benchmark report if
  emulated timings are meaningful enough to track.

### macho-* on native macOS

- [ ] **Investigate how much of the `macho-*` suite can run natively on macOS.**
  The four `macho-apple-libc`, `macho-codegen-run`, `macho-image-run`,
  `macho-structural` tests SKIP on the native macos-arm64 jobs — today they are the
  *Linux-hosted* Darwin cross-codegen tests (mcc emits Mach-O on Linux, validated
  without a real Apple linker/loader; see the Mach-O real-Apple-libc harness). On a
  genuine macOS runner far more is available: the real `ld`, `dyld`, and system
  libSystem. Determine, per test, how much can execute end-to-end natively
  (structural checks → link → run) instead of skipping, and use **`mcchost`** as the
  Linux↔macOS bridge/abstraction so the same test body runs the emulated Mach-O
  path on Linux and the native path on macOS. Goal: the macos jobs stop skipping
  the Mach-O suite outright and cover whatever the native toolchain makes runnable.

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

- [ ] **exec suite — per-test runner gates (arch-specific asm, backtrace config, x86 ABI).**
  These are the `exec/` cases skipped on macos-arm64. Some are legitimately
  arch/feature-specific (`riscv_asm`, `winarm64_interlocked`, `asm_constraints_x86`,
  `al_ax_extend`/`fastcall` = x86 ABI); others (`inline`, `alias`, `array_assignment`,
  `backtrace`, `btdll`) may only need a config knob (e.g. `MCC_CONFIG_BACKTRACE`) or
  may be runnable on Darwin/arm64 as-is. Verify each gate; enable where a subset runs:
  - [ ] `exec/al_ax_extend`  - [ ] `exec/alias`  - [ ] `exec/array_assignment`
  - [ ] `exec/asm_constraints_x86`  - [ ] `exec/asm_data_directives`  - [ ] `exec/asm_goto`
  - [ ] `exec/asm_lvalue_cast`  - [ ] `exec/asm_operand_modifiers`  - [ ] `exec/asm_sections`
  - [ ] `exec/backtrace`  - [ ] `exec/btdll`  - [ ] `exec/fastcall`  - [ ] `exec/inline`
  - [ ] `exec/riscv_asm`  - [ ] `exec/winarm64_interlocked`

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
  - [ ] `mcctest` / `mcctest-bcheck` — see the macos-gcc host-cc item above.

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
