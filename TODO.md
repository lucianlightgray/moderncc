# TODO — close test-coverage gaps (features implemented without tests)

Derived from a six-way audit of the implementation against `tests/` (2026-06-28),
then worked through. Each item names a feature that **is implemented and
reachable** but previously had **no test**. `file:line` anchors were accurate at
audit time — re-grep before editing.

## Status

**Done this pass — ~64 new checks, all green through `ctest` (23/23, 2 cross-only
skips):**
- **33 new golden rows** in `tests/exec/goldens.h` (language/builtins,
  preprocessor diagnostics, assembler data/inline-asm, lexer, char signedness,
  a `-b` bounds variant).
- **New structural suite** `tests/cli/` (`runner.c` + `cases.h`, wired as the
  `cli-suite` ctest): 26 cases that compile then inspect output with
  `readelf`/`nm` — the repo's first tests that assert output *structure*.
- **New embedding test** `tests/embed/api_extra.c` (`libtest-extra` ctest):
  5 previously-uncalled `libmcc` API paths.

**Differential conformance pass (gcc/clang as source of truth):**
- **`tests/support/vlog.h`** — opt-in, DEFINE-gated verbose logging (`-DVLOG_ENABLE`).
  No-op by default; when enabled, traces go to **stderr** so neither golden nor
  differential stdout comparisons are perturbed. `tests/support` is on every test
  runner's include path, so any test may `#include "vlog.h"`.
- **`tests/diff3/`** — three-way differential runner (`diff3-suite` ctest):
  builds+runs every portable `run` golden under **gcc, clang, and mcc** and
  compares program stdout. Verbose via `-DDIFF3_VERBOSE` or `MCC_DIFF3_VERBOSE=1`.
- **Result: 171 tests agree across gcc/clang/mcc, 0 mcc divergences.** The only
  divergence found was fixed: **x86_64 `=A` inline-asm constraint** was using the
  i386 `edx:eax` pair convention instead of plain `rax`
  (`src/arch/i386/i386-asm.c`, now `#ifdef MCC_TARGET_X86_64`-gated). 3 cases are
  genuinely gcc≠clang (impl-defined: `cleanup`, `predefined_macros`,
  `bitfields_ms` — mcc matches a reference); 4 are `#ifdef __MCC__` mcc-only
  sections; 9 use mcc-only features gcc/clang can't build.

How tests are wired:
- **Golden harness** (`tests/exec/runner.c` + `tests/exec/goldens.h`): modes
  `run`, `pp`, `brun`, `dt`, `run2`. Whitespace-canonicalised line compare;
  `...` (3+ dots) is a glob. `req` gates on `cpu=`/`os=`/`asm`/`bcheck`/
  `backtrace`/`note:`.
- **Structural harness** (`tests/cli/`): compiles then greps `readelf`/`nm`;
  same compare/glob semantics; `req` gates on `cpu=`/`os=`.

Legend: **[x]** done · **[ ]** open · **[~]** deferred (reason given) ·
`(asm)` integrated assembler · `(qemu)` needs cross/qemu · `(struct)` structural
suite.

> ### Bugs surfaced while testing (out of scope here — file separately)
> - `-ffunction-sections` / `-fdata-sections`: **accepted but no-op** (no per-symbol
>   sections emitted). Now smoke-tested as "accepted"; real splitting is unimplemented.
> - `-fvisibility=hidden` and `visibility` attribute set `st_other` correctly in
>   the **`.o`** (verified), but mcc's own `-shared` link does **not** demote hidden
>   symbols out of `.dynsym`.
> - mcc objects **omit `.note.GNU-stack`**, and `.eh_frame_hdr` has "overlapping
>   FDEs" → GNU `ld` refuses to link mcc `.o` into a `.so` ("final link failed").
>   This blocks the "link mcc `.o` with system `ld`" test (§5).
> - `mcc -Iinc -print-search-dirs` errors ("cannot parse … here"); works when `-I`
>   does not precede it. Option-ordering bug.
> - `mode()`/`__inf__` attribute & token spellings are `__mode__(__DI__)` / `__inf__`
>   (lowercase) — plain `mode(DI)`/`__INF__` are silently ignored / undeclared.

---

## 1. Language & codegen front-end (`src/mccgen.c`)

- [x] `__builtin_unreachable` → `features_c99_c11/builtins_extra.c`
- [x] `__builtin_constant_p` / `choose_expr` / `types_compatible_p` /
      `frame_address` → `builtins_extra.c`
- [x] GNU omitted-middle `a ?: b` (single-eval) → `expressions/gnu_cond_omitted.c`
- [x] `enum E : T` fixed underlying type → `types/enum_fixed_type.c`
- [x] `__attribute__((__mode__(__DI__)))` width → `types/attr_mode.c`
- [x] `__real`/`__imag` on real scalar → `features_c99_c11/realimag_scalar.c`
- [x] `__inf__`/`__nan__` IEEE tokens → `types/builtin_inf_nan.c`
- [x] `__attribute__((alias))` single-TU → `features_c99_c11/alias_single_tu.c`
- [x] `__attribute__((weak))` undefined-weak → `features_c99_c11/weak_undef.c`
- [x] `__attribute__((section(..)))` → `cli` `section_attribute`
- [x] `__attribute__((visibility(..)))` → `cli` `visibility_attribute`

## 2. Preprocessor (`src/mccpp.c`)

- [x] `#include_next` directive → `cli` `include_next_directive`
- [x] `#warning` → `preprocess/diagnostics/warning_directive.c`
- [x] Unknown `#pragma` passthrough → `directives/unknown_pragma_passthrough.c`
- [x] Conditional-nesting errors (endif/else-after-else/elif-after-else/elif-no-if)
      → `preprocess/diagnostics/cond_*.c`
- [x] `##` at macro-body end → `diagnostics/hashhash_at_end.c`
- [x] Macro arg-count + EOF-in-invocation → `diagnostics/macro_too_*` / `_eof_*`
- [x] Invalid token-paste warning → `diagnostics/invalid_paste.c`
- [x] Char constant + wide `L'x'` in `#if` → `conditional/charconst_in_if.c`
- [x] `#` not on a parameter → `diagnostics/stringize_non_param.c`
- [x] `\U` 8-digit UCN, GNU `\e`, wide char value → `exec/lexical/lex_extras.c`
- [x] Integer constant overflow → `diagnostics/int_overflow.c`
- [x] UTF-8 BOM skip → `exec/lexical/utf8_bom.c`
- [x] `#ifdef` non-identifier arg → `diagnostics/ifdef_non_identifier.c`
- [x] Missing `#endif` at EOF → `diagnostics/missing_endif.c`
- [~] Multi-char / empty char constant warnings — need `-Wall`; belong in the
      `dt` multi-section framework (see `errors_and_warnings.c`).
- [~] Integer suffix errors (`1lll`, `1uu`) — only fire on real compile, not
      `-E`; need a `dt`/compile-mode case.
- [~] `#pragma comment(lib,..)` — low value; link-effect only.

## 3. Inline asm & integrated assembler (`src/mccasm.c`, `arch/*-asm.c`)

- [x] `.rept`/`.fill`/`.skip`/`.asciz` vs `.ascii`/`.set`/`.long 2b-1b`
      → `exec/inline_asm/data_directives.c` `(asm)`
- [x] `.section`/`.pushsection`/`.popsection`/`.previous`
      → `exec/inline_asm/asm_sections.c` `(asm)`
- [x] x86 operand modifiers `%b %w %k %q`
      → `exec/inline_asm/asm_operand_modifiers.c` `(asm, x86_64)`
- [x] x86 fixed-reg/pair constraints (`=a/=b/=c/=d`, `A`), specific-reg clobbers,
      `register … asm("r15")` → `exec/inline_asm/asm_constraints_x86.c`
- [~] Standalone `.s`/`.S` → `mcc -c` driver path — needs multi-file harness.
- [~] `.weak` override / `.type`/`.size` spellings — multi-TU / `(struct)`; deferred.
- [~] `.reloc`, riscv `.option`, `.symver`, arm32 asm, i386 `.code16` — `(qemu)`/cross.

## 4. CLI driver & flags (`src/mcc.c`, `src/libmcc.c`, `src/mcctools.c`)

- [x] `-shared` → ET_DYN + `-Wl,-soname` → `cli` `shared_dyn_soname`
- [x] `-r` relocatable partial link → `cli` `relocatable_partial_link`
- [x] `-fvisibility=hidden` (+ explicit-default-wins) → `cli` `fvisibility_hidden_default_wins`
- [x] `-fleading-underscore` → `cli` `leading_underscore`
- [x] `-rdynamic` → `cli` `rdynamic_exports_main`
- [x] `-ffunction-sections`/`-fdata-sections` accepted (no-op) → `cli` `function_data_sections_accepted`
- [x] `-fstack-protector-all` / `-fno-stack-protector` → `cli` `stack_protector_on`/`_off`
- [x] `-M` / `-MD`/`-MF` dependency gen → `cli` `deps_M_rule` / `deps_MD_MF_file`
- [x] `-U` / `-E -dM` / `-nostdinc` → `cli` `undef_flag` / `dM_dump_macros` / `nostdinc_drops_system`
- [x] `-dumpmachine` / `-dumpversion` / `-print-search-dirs` → `cli` (3 cases)
- [x] `-ar` create/list / `@file` response → `cli` `ar_create_list` / `response_file`
- [x] `-funsigned-char` / `-fsigned-char` → `exec/types/char_signedness.c`
- [~] `-static` / `-nostdlib` — host lacks static libc; link fails (can't assert success).
- [~] `-fno-common` — mcc already defaults to `.bss` (no observable difference).
- [~] `-x`, `-isystem`/`--sysroot`, `-include`, `-Werror`, `-Wwrite-strings`,
      `-Wl,-rpath`/dtags, `-soname` standalone, `-w`/`-Wp`/`-v` — lower value; add as needed.

## 5. Object formats & debug info (`src/objfmt/*`, `src/mccdbg.c`)

- [x] `-shared` ET_DYN / `DT_SONAME` → `cli` `shared_dyn_soname`
- [x] Relocatable output symbols/type → `cli` `relocatable_partial_link`
- [x] DWARF `.debug_info` (`DW_TAG_subprogram` + fn name) → `cli` `debug_dwarf5_info`
- [x] DWARF version select (`-gdwarf-5` → v5) → `cli` `debug_dwarf_version_select`
- [x] Default `-g` = stabs / `-gstabs` `.stab` → `cli` `debug_default_stabs` / `debug_gstabs`
- [x] Weak/visibility symbol emission in `.o` → `cli` `visibility_attribute`
- [x] Constructor `.init_array` → `cli` `constructor_init_array`
- [~] `.debug_line` program / `DW_OP_fbreg` locals — have DIE-level coverage; line
      tables + gdb-level local checks deferred.
- [~] Dynamic tags / RELRO / GNU-vs-SysV hash / `.symver` emission / ctor priority
      ordering / common+copy relocs / `-s` strip — deferred (`struct`, lower value).
- [~] Link mcc `.o` with system `ld` — **blocked** by the `.note.GNU-stack` /
      overlapping-FDE bug (see top).
- [~] TLS `PT_TLS` + per-arch relocs — `(qemu)`.
- [~] Mach-O writer / PE writer + `.idata`/`.reloc`/`.pdata` — need macOS / wine.

## 6. Embedding API, JIT & bounds runtime (`src/libmcc.c`, `src/mccrun.c`, `runtime/`)

- [x] `mcc_list_symbols` → `embed/api_extra.c`
- [x] `mcc_undefine_symbol` → `api_extra.c`
- [x] `mcc_add_library` via API → `api_extra.c`
- [x] `mcc_run` argv passthrough + return code → `api_extra.c`
- [x] `mcc_output_file` + `MCC_OUTPUT_OBJ` → `api_extra.c`
- [x] `bound_test.c` run *with* `-b` → golden `bound_test_b` (`brun`, `req=bcheck`)
- [~] `mcc_set_realloc`, `mcc_add_sysinclude_path`, `-run` stdin/`-e` entry,
      `mcc_relocate` double-call guard, bcheck `__bound_never_fatal`/mmap,
      `mcc_set_backtrace_func` return semantics — low-pri; add incrementally.
- [~] Reactivate skipped `backtrace` / `btdll` goldens — need the reference
      backtrace harness they were written against.

---

### Remaining work is intentionally deferred to:
1. **Cross/qemu** builds (`cmake-build-cross`) — non-x86 relocs, TLS, arm/riscv
   asm, Mach-O, PE.
2. The **`dt` multi-section framework** — `-Wall`-gated lexer diagnostics, suffix
   errors.
3. **Upstream bug fixes** (see top box) before their tests can assert success —
   function/data-sections, hidden-symbol demotion, GNU-stack/eh_frame, deps of
   `-static`.
