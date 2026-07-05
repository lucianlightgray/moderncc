# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (then removed).

---

## Open items (priority order)

_None currently open._ Resolved 2026-07-04: arm64 atomics/bounds/complex link
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
