# `static` usage audit

A codebase-wide sweep of the `static` keyword (`grep -rIw static`), split into the
two categories requested:

1. **Core `static` feature** — usages that *are the C `static` storage-class feature*
   MCC must compile per C99/C11, plus the tests that exercise it. These are load-bearing
   and must not be "refactored away."
2. **Implementation `static` refactorable to the stack** — MCC's *own* source using
   `static` storage duration to hold mutable per-compilation state. Each of these is a
   reentrancy/thread-safety hazard and is a candidate to move onto the stack (or into a
   passed-in context struct) instead.

Raw counts from the sweep:

| Location | `static` word matches |
|----------|----------------------|
| `src/`   | 1276 |
| `tools/` | 185  |
| `tests/` | 631  |
| `runtime/` | 196 |
| `docs/`  | 136  |

Of the `src/` matches: ~874 are internal-linkage **function** definitions
(`static void foo(...)`, many via the `ST_FUNC`/`ST_INLN` macros — 540 macro uses),
~113 are function-local `static const` **lookup tables**, and ~62 are file-scope
`static const` tables. Those are covered under "Not refactor candidates" at the end —
they are neither the language feature nor movable to the stack.

---

## Category 1 — Core `static` feature (C99/C11) and its tests

These implement / verify the `static` **storage-class specifier** and the closely
related `_Static_assert`. Do not touch as part of any "stack refactor."

### 1a. Keyword and type-flag definitions

| File:line | Usage |
|-----------|-------|
| `src/mcctok.h:18`  | `DEF(TOK_STATIC, "static")` — the keyword token |
| `src/mcctok.h:43`  | `DEF(TOK_STATIC_ASSERT, "_Static_assert")` |
| `src/mcc.h:910`    | `#define VT_STATIC 0x00002000` — the internal-linkage type flag |
| `src/mcc.h:932`    | `VT_STORAGE = (VT_EXTERN \| VT_STATIC \| VT_TYPEDEF \| VT_INLINE \| VT_TLS)` |

### 1b. Parser — recognizing and applying the `static` storage class (`src/mccgen.c`)

Storage-class parsing, linkage/redeclaration rules, tentative definitions, and
static-local emission:

- `5237–5244` — `case TOK_STATIC: g = VT_STATIC;` in the declaration-specifier parser,
  with the duplicate/conflict diagnostic.
- `5503–5506` — `case TOK_STATIC` inside array declarators (`arr[static N]`, C99 §6.7.6.3).
- `5215`, `5339`, `5423`, `5537` — storage-class validation and propagation of `VT_STATIC`.
- `1322–1368` — redeclaration merge logic: `static_proto`, "static follows non-static
  declaration" diagnostic, linkage-change checks.
- `829` — block-scope `static ... extern` interaction.
- `10080`, `10162–10182`, `10217`, `10368–10382` — declaration handling of `static`
  locals, static initialization, `for`-init restriction, inline-extern rules.
- `1003`, `1234`, `2645`, `4340–4342`, `7232` — pushing/tagging syms as `VT_STATIC`
  (static locals, enum constants, compound literals).
- `4426`, `10118` — `_Static_assert` handling (`TOK_STATIC_ASSERT`).
- `621`, `635`, `715`, `766`, `9954` — inline/static function emission gating.
- `8405`, `8413` — `TOK_STATIC` / `TOK_STATIC_ASSERT` in the "is this a decl?" lookahead.

### 1c. Code generation & linking — honoring `VT_STATIC` (no GOT/PLT for internal syms)

| File | Lines |
|------|-------|
| `src/arch/x86_64/x86_64-gen.c` | 255, 309, 465, 591 |
| `src/arch/i386/i386-gen.c` | 115, 156, 222, 314, 376, 435 |
| `src/arch/arm/arm-gen.c` | 484 |
| `src/arch/riscv64/riscv64-gen.c` | 174, 186, 471, 1378 |
| `src/arch/riscv64/riscv64-asm.c` | 206, 208, 210, 233, 250, 791 |
| `src/mccasm.c` | 69, 76, 654 (asm symbol linkage) |
| `src/mccdbg.c` | 2162, 2198, 2200, 2286, 2375 (DWARF/stabs `static` vs global function flag) |

### 1d. Tests that power the `static` feature

- `tests/exec/functions_abi/static.c` — the canonical test: static file-scope and
  static-local variables, persistence across calls, shadowing.
- `tests/exec/types/storage_tentative.c` — tentative definitions with `static`.
- `tests/exec/statements/scopes.c` — static-local scope/linkage.
- `tests/exec/functions_abi/inline.c`, `inline2.c`, `local_extern.c` — static/inline/extern
  linkage interactions.
- `tests/exec/statements/tentative_array.c`, `tests/exec/types/tentative_*` — tentative
  static arrays.
- `_Static_assert` coverage across `tests/exec/` and `tests/qemu/conformance/`.

---

## Category 2 — Implementation `static` refactorable to the stack — **DONE**

MCC (like its TinyCC ancestor) held most parser / preprocessor / codegen state in
**file-scope mutable statics** and a few **function-local mutable statics**. Each was
process-global mutable state: it defeats reentrancy and blocks concurrent compilation in
one process.

**Status: refactored.** Every per-compilation mutable static below has been moved off
static storage — genuine per-call scratch onto the **stack** (threaded through
prototypes / owned by an already-stack-allocated context struct), and persistent
per-instance state **rehomed into `MCCState`** (the codebase's existing
`#define name mcc_state->name` redirect idiom, as already used for `qrel`). The two
build shapes were verified after every increment: `debug` (host x86_64, SINGLE_SOURCE) and
`cross` (all five backends) — **804/804 tests pass in both.**

The handful that remain static are process-global *by nature* (async signal/fault
handlers, the DLL loader, process-wide allocator accounting, read-only tables) — moving
them into `MCCState` would only relocate a global, not remove one. They are listed under
"Deliberately retained" below.

### 2a. Function-local mutable statics — **moved to the stack**

Each was a hidden static buffer/counter making its function non-reentrant. Now owned on
the stack (via a wrapper or the caller's already-stack-allocated context), or rehomed to
`MCCState` where the value must persist across calls:

| Former static | Where it went |
|---------------|---------------|
| `mccgen.c` `restrict_ptr_pointee[8]`, `nb_restrict_ptr`, `type_decl_depth` | `struct restrict_ctx` on the stack of a new `type_decl()` wrapper, threaded through `type_decl_1()`; public signature unchanged |
| `mccgen.c` `mk_complex_type` `cache[4]`, `re_tok`, `im_tok` | `MCCState.gen_complex_type_cache` / `gen_complex_re_tok` / `gen_complex_im_tok` (persistent memo) |
| `mccgen.c` `sizeof_parsed_type` | `MCCState.gen_sizeof_parsed_type` (cross-frame flag) |
| `mccgen.c` `prec[256]` | `MCCState.gen_prec` (lazily-filled table) |
| `mccdis.c` `disasm_reloc`/`disasm_label` `buf[256]`/`buf[32]` | `disasm_ctx.relocbuf` / `.labelbuf` (dc is stack-allocated; single-buffer aliasing contract preserved) |
| `mccdis.c` `g_uniq` | `MCCState.disasm_uniq` (all three accessors already take `s1`) |
| `x86_64-dis.c` `gpr`/`xmm` `buf[8]` | `Dis.regbuf`; threaded `d` into `xmm()` |
| `i386-dis.c` `gpr` `buf[8]` | `Dis.regbuf`; threaded `d` into `gpr()` |
| `arm64-dis.c` `rpool` `pool[8][24]` + `i` | `disasm_ctx.dis_namepool` + `dis_namepool_i`; threaded `dc` through `ir`/`irsp`/`fr`/`rpool` (106 call sites) |
| `i386-asm.c` `already` | plain stack local (the block is dead code: `if (0 && …)`) |

> `mcchost.c:895` `host_macos_sdk_root` `buf`/`done` is **deliberately retained** — see below.
> `tools/build.c` scratch statics are in a standalone build tool (not the compiler) and
> were left as-is.

### 2b. File-scope parser/typechecker state (`mccgen.c`) — **rehomed to `MCCState`**

All moved to `MCCState` (struct types + size macros lifted into `mcc.h`; bare-name
`#define` redirects in `mccgen.c`): `sym_free_first`, `sym_pools`, `nb_sym_pools`,
`all_cleanups`, `pending_gotos`, `local_scope`, the value stack `_vstack` →
`MCCState.gen_vstack`, `func_old`, `cur_func_noreturn`, `cur_func_last_param`,
`expr_was_assign`, `expr_has_effect`, `cur_func_inline_extern`, `ice_float_op`,
`ice_nonconst`, `initstr`, `cur_switch`, `atomic_lowering`, `in_for_init`, the VLA
tracking (`vla_seq`, `vla_open_birth[]`, `nb_vla_open`, `vla_track_ovf`), the temp-local
pool (`arr_temp_local_vars[]`, `nb_temp_local_vars`), the scope chain (`cur_scope`,
`loop_scope`, `root_scope`), and sequence-point tracking (`seqp_ev[]`, `nb_seqp`,
`seqp_overflow`). `mccgen.c` now has **zero** mutable statics.

### 2c. File-scope preprocessor/lexer state (`mccpp.c`) — **rehomed to `MCCState`**

All moved: `tok_ts`, `hash_ident[]`, `token_buf[]`, `cstr_buf`, `tokstr_buf`,
`unget_buf`, `isidnum_table[]`, `pp_debug_tok`, `pp_debug_symv`, `pp_counter`,
`toksym_alloc`, `tokstr_alloc`, `macro_stack`, and the `PP_DEBUG` `indent`
(→ `MCCState.pp_debug_indent`). `struct TinyAlloc` is forward-declared in `mcc.h`.

### 2d. Assembler / disassembler state — **rehomed to `MCCState`**

- `mccasm.c` `last_text_section`, `asmgoto_n`, and the CFI struct `asm_cfi`
  (→ `MCCState`; `struct asm_cfi_state` + `ASM_CFI_MAX` lifted to `mcc.h`).

### 2e. Per-function codegen state (`arch/*-gen.c`) — **rehomed to `MCCState`**

Not in the original audit but surfaced during the sweep — per-function backend state.
Because only the active target's backend compiles, shared names collapse onto shared
`cg_*` fields in `MCCState`: `func_sub_sp_offset`, `func_ret_sub`, `func_stack_chk_loc`,
`func_bound_offset`, `func_bound_ind`, `func_scratch`, `func_alloca` (x86_64/i386);
`last_itod_magic`, `leaffunc`, `float_type`/`double_type`/`func_float_type`/
`func_double_type` (arm); `arm64_func_va_list_stack`/`_gr_offs`/`_vr_offs`,
`arm64_func_sub_sp_offset`, `arm64_func_start_offset` (arm64); `num_va_regs`,
`func_va_list_ofs` (riscv64).

Also `arm-gen.c` `float_abi` (a per-function copy of the ABI, temporarily forced to
soft-float for variadic calls). It needed a manual rename first: the identifier was used
both for the static **and** as a same-named `assign_regs()` parameter, and was seeded from
`s->float_abi`, so a bare-name redirect would corrupt those. The static was renamed
`cg_float_abi` (leaving the parameter alone) and rehomed to `MCCState.cg_arm_float_abi`;
verified under qemu-arm (glibc + musl), which exercises ARM's hard/soft-float ABI split.

**After this pass, no per-compilation mutable static remains in `src/`** — every one is
either moved (above) or deliberately retained as process-global (below).

---

## Deliberately retained (process-global by nature — *not* movable to the stack)

Moving these into `MCCState` would only relocate a global (the async/loader contexts
that reach them receive no `MCCState`), so they stay file-static:

- **Async signal/fault handler state** — `mccrun.c` `g_rc`, `g_s1`, `signal_set`;
  `mcchost.c` `host_fault_cb`. Reached from signal/fault handlers that take no argument.
- **DLL loader global** — `mcchost.c` `mcc_module` (set by `DllMain`, Windows only).
- **Process-wide allocator accounting** — `libmcc.c` `mem_debug_chain`, `mem_cur_size`,
  `mem_max_size`, `nb_states` (spans *all* states; `nb_states` literally counts them),
  and the `reallocator` hook.
- **Immutable/cached process singletons** — `libmcc.c` `auto_mccdir_buf` (install dir);
  `mcchost.c` `host_macos_sdk_root` `buf`/`done` (macOS SDK path, an immutable system
  property, `#ifdef __APPLE__`); `mcchost.c` `mcc_syms[]` (read-only `-run` symbol table).

---

## Bugs found and fixed during this work

- **Complex-type cache use-after-free (`mccgen.c`)** — the memo populated by
  `mk_complex_type` caches `CType`s whose `.ref` points into `global_stack`, which
  `mccgen_finish` frees. It was never cleared, so a second TU compiled on the same
  `MCCState` (e.g. `mcc a.c b.c`, or two `mcc_compile_string` calls) that used `_Complex`
  reused dangling syms — a **reproducible segfault**. Pre-dated this work (the old
  process-global static had it too) but fixed here by clearing the cache in
  `mccgen_finish`. Guarded by a new `test_multi_tu_complex` case in
  `tests/embed/api_extra.c` (verified it fails without the fix).
- **`qemufetch` directory ordering (`tools/mccharness.c`)** — the stage3 pointer file was
  downloaded with `curl -o <dldir>/<arch>.ptr` *before* `host_mkdirs(dest)` created the
  directory, so a local `ctest --preset qemu` failed instantly (CI masks it with a
  pre-mounted volume). Fixed by hoisting `host_mkdirs(dest)` before the download.

---

## Not refactor candidates (excluded from both categories)

- **Internal-linkage functions** (~874 in `src/`, incl. `ST_FUNC`/`ST_INLN`): `static`
  here means "file-local symbol," not "static storage duration." Not stack-movable.
- **`static const` lookup tables** (~113 local + ~62 file-scope): read-only immutable data
  (opcode/register/mnemonic tables in `*-dis.c`/`*-asm.c`, `mcc_keywords`, `mcc_options`,
  `help`/`version`). Stack-allocating them would only force needless re-init.
- **`ST_DATA` macro sites** — gated `static` vs `extern` for the single-source build; a
  build-mode toggle, not a hazard.
- **`runtime/` and `tests/` ordinary `static`** — freestanding library code and test
  fixtures whose `static` is either the tested feature (see 1d) or normal library usage.
