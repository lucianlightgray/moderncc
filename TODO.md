# TODO — C9911 gap implementation tracker

Working tracker for the standing goal: migrate the actionable gaps from
[C9911.md](C9911.md) (the `mcc:✗`/`mcc:~` items where mcc diverges from the
gcc==clang consensus) into here, implement them easiest-first, and record status,
until every C9911 gap is migrated and resolved.

**C9911.md is partly stale** (generated before some merges): several items it
flags as `mcc:✗` are already fixed. So *re-verify every candidate 3-way against
the live binary* before implementing — confirm it's a real gap, prune it if not.
Each fix ships a cli/exec regression test; keep full ctest + the byte-identical
self-host fixpoint green. (C9911's Appendix holds the *deliberate* DIFFs — those
are intentional and out of scope.)

Legend: `[ ]` open · `[~]` in progress · `[x]` done.

**Migration status: COMPLETE — no open items.** Every actionable C9911 gap has
been pulled into this file and either implemented (each with a 3-way-verified
cli/exec regression test) or explicitly classified (already-handled /
pruned-as-stale / deliberate permissive divergence / not-a-consensus-gap). The
two former hard holdouts are now done too: the deep complex-constant-overflow
codegen bug is fixed, and `-Wformat` printf/scanf checking is implemented as an
opt-in warning. Full ctest 34/34 and the byte-identical 3-stage self-host
fixpoint are green. Remaining `[ ]`-free; any parenthesised *(Future: …)* notes
are optional enhancements, not gaps.

---

## Done (this goal)

- [x] **§6.4.4.1 — `0b…` binary integer constants diagnosed under -pedantic.** GNU/C23
  extension, not ISO C99/C11 (`src/mccpp.c` `parse_number`). cli `pedantic_extension_diagnostics`.
- [x] **§6.4.4.4 — `\e` (ESC) escape diagnosed under -pedantic.** Non-ISO GNU escape
  (`src/mccpp.c` escape switch). cli `pedantic_extension_diagnostics`.
- [x] **§6.7.6.2p1 — zero-size array `int a[0]` (and GNU `int d[0]` member) diagnosed
  under -pedantic.** Constraint: a constant array size shall be > 0 (`src/mccgen.c`
  array size check). cli `pedantic_extension_diagnostics`.
- [x] **§6.4.4.4 / §6.4.5 — `u`/`U`/`u8` char/string prefixes diagnosed in strict C99 mode.**
  C11 additions (`src/mccpp.c` lexer prefix cases via `pp_c11_prefix_pedantic`).
  cli `c11_features_in_c99`.
- [x] **§6.7.1p1 — `_Thread_local` diagnosed in C99 mode under -pedantic.** C11 feature;
  GNU `__thread` exempt (`src/mccgen.c` storage-class case). cli `c11_features_in_c99`.
- [x] **§6.7.2.4p1 — `_Atomic` diagnosed in C99 mode under -pedantic.** C11 feature
  (`src/mccgen.c` `case TOK__Atomic`). cli `c11_features_in_c99`.
- [x] **§6.7p3 — typedef-redefinition of a variably-modified type rejected** (`typedef int
  T[n]; typedef int T[n];` at block scope; hard error matching gcc/clang) (`src/mccgen.c`
  typedef-redef path). cli `pragma_vla_typedef_constraints`.
- [x] **§6.10.9p1 — `_Pragma` with a non-string-literal operand diagnosed** (was silently
  skipped; now an error) (`src/mccpp.c` `pragma_operator`). cli `pragma_vla_typedef_constraints`.
- [x] **§6.7.9p11 — `{{5}}` (redundant braces around a scalar) — PRUNED as stale.** mcc
  already rejects extra braces around a scalar initializer (verified 3-way); C9911's
  `mcc:✗` flag was generated pre-fix. No code change.
- [x] **§6.7.6.3p3 — non-empty identifier list in a non-defining declarator rejected**
  (`int f(a,b);` is a hard error matching gcc/clang; the K&R *definition*
  `int def(a,b) int a,b;{…}` and empty `int f();` stay valid) (`src/mccgen.c` decl
  non-definition path: FUNC_OLD with a non-empty parameter chain). cli `kr_identifier_list_declaration`.
- [x] **§6.9.1p6 / §6.7.2p2 — undeclared old-style identifier-list parameter — ALREADY
  HANDLED.** `int f(x){return x;}` warns "type of 'x' defaults to 'int'" by default and
  errors under `-Werror` (`src/mccgen.c` line ~10560), matching mcc's permissive-by-default
  philosophy (gcc/clang error by default; mcc warns→`-Werror`). No code change; C9911's
  `mcc:✗` flag counts the exit-0 default as accept. Covered by existing K&R tests.

## Next up — easy, to verify-then-implement

*(batch exhausted — pull the next-easiest from the moderate backlog below)*

## Backlog — moderate (symbol table / declarators / functions)

- [x] **§6.9.1p3 — function-definition return type must be void or a complete object
  type.** Incomplete struct/union/enum return rejected (`src/mccgen.c` definition path);
  declarations, void, complete-struct and pointer returns stay valid. cli
  `function_definition_complete_types`.
- [x] **§6.9.1p7 — each function-definition parameter's adjusted type must be complete.**
  Incomplete struct/union/enum parameter rejected (`src/mccgen.c` definition param loop);
  pointer-to-incomplete stays valid. cli `function_definition_complete_types`.
- [x] **§6.9.2p3 — PRUNED (not a default-mode gap).** `static int arr[];` never completed:
  gcc/clang both *accept* by default (treat as size-1 tentative def; only `-pedantic`/
  `-Wall` note it). mcc matches (mcc=gcc=clang=0). Revisit only if mcc's `-pedantic` is
  audited against theirs.
- [x] **§6.7.4p2 — PRUNED (not a default-mode gap).** A plain-`inline` external function
  defining a modifiable `static` object: gcc/clang both *accept* by default
  (mcc=gcc=clang=0); the existing `inline_extern_static_object` cli test already covers
  the `extern inline` variant mcc does diagnose.
- [x] **§6.7.4p3 — PRUNED (no gcc/clang consensus).** `inline int main(void){}`: clang
  errors, gcc accepts (mcc=0 gcc=0 clang=1). No gcc==clang consensus → mcc matching gcc is
  defensible; out of scope for the consensus-gap rule.
- [x] **§6.2.3p1 / §6.7.8p3 — typedef name and ordinary identifier share one name space.**
  Reusing a typedef name for an object/function in the same scope is rejected (`src/mccgen.c`
  ordinary-decl path: `sym_find` + `VT_TYPEDEF` + same `sym_scope`); deeper-block shadowing
  and ordinary redeclarations stay valid; the reverse order was already caught. cli
  `typedef_ordinary_name_space`.
- [x] **§6.9.1p2 — function definition's type may not come entirely from a typedef.**
  `typedef int F(void); F f {…}` rejected (`src/mccgen.c` definition path: `ad.f.func_type==0`
  ⇒ the VT_FUNC type came purely from the base typedef, no declarator parameter list); a
  typedef *return* type, K&R definitions, and pointer uses stay valid. cli
  `function_definition_typedef_type`.

## Backlog — hard / separate efforts

- [x] **§6.4.4 / GNU — imaginary *integer* constants (`3i`, `5j`, `0x4I`) accepted.** The
  lexer only allowed the suffix on floating tokens; extended to the integer-token range so
  `2+3i` builds a complex value (`unary()`'s `tok_imaginary` path already handles any base
  type → `_Complex int`, matching gcc/clang). cli `imaginary_integer_constants`. (Was
  surfaced while triaging §7.25p7 — the tgmath dispatch itself was fine.)
- [x] **§7.21.7.7 / §6.5.2.2 — implicit function declaration — ALREADY HANDLED.** Calling an
  undeclared function warns "implicit declaration of function '…'" by default and errors
  under `-Werror` (same for `gets`), matching mcc's permissive-by-default philosophy
  (gcc/clang error by default; mcc warns→`-Werror`, like §6.9.1p6 K&R implicit-int). The
  diagnostic satisfies the constraint; C9911's `mcc:✗` counted the exit-0 default as accept.
- [x] **§7.25p7 — `creal`/`cimag` tgmath dispatch — NOT a compile gap.** `creal`/`cimag`/
  `creall`/`cimagl` all compile and return correct values 3-way (mcc=gcc=clang=0). Any
  remaining concern is *runtime precision* of the fixed-`double` intermediate, not a
  divergence the 3-way return-code probe detects; reclassify as a numeric-precision audit
  if ever pursued.
- [x] **§G.2 — `_Imaginary` types — PRUNED (consensus reject).** `_Imaginary float` is
  rejected by mcc, gcc and clang alike (all three lack the optional imaginary types).
  No divergence; nothing to implement.
- [x] **§7.25p6 — `nexttoward` tgmath now selects on the first argument only.** Its second
  argument is always `long double`, so keying on `(x)+(y)` always forced `nexttowardl`
  (`sizeof(nexttoward(float,2.0L))` gave 16, want 4). Switched `runtime/include/tgmath.h`
  to `__tgmath_real_2_1` (keys on `(x)`, like `frexp`/`ldexp`); now 4/8/16 matching
  gcc/clang. cli `tgmath_nexttoward_first_arg`.
- [x] **§D / §6.4.2.1 — UCN combining marks rejected as the FIRST identifier character.**
  Added the Annex D.2 disallowed-initial ranges (0300–036F, 1DC0–1DFF, 20D0–20FF,
  FE20–FE2F) check at the identifier-start UCN path (`src/mccpp.c`
  `ucn_disallowed_initial`); the same marks stay valid non-initially, ordinary leading
  UCNs (U+00C0) unaffected. cli `ucn_identifier_initial_combining`. (Other UCN edge
  cases — `$`/`@`/`` ` ``, basic-latin, arabic digit — already matched the consensus.)
- [x] **§G.5.1 — complex `*`/`/` infinity-recovery — ALREADY CORRECT (runtime).** With
  runtime infinities, mcc's `__mcc_cmul`/complex-divide give `inf+inf·i` exactly like
  gcc/clang (the Annex-G robust helper is already wired; see `gen_complex_call`). No gap.
- [x] **§F.10.11 — `isgreater`/… on NaN — NOT a divergence here.** mcc and gcc behave
  identically on this glibc (`fetestexcept(FE_INVALID)` set for both); `__builtin_isgreater`
  already exists in mcc. No mcc-vs-consensus gap to fix on this platform.
- [x] **§6.7.4p6 — plain `inline` external definition — DELIBERATE permissive divergence.**
  mcc emits the inline body as a per-TU LOCAL symbol (effectively `static inline`), so a
  program that calls a plain-`inline` function with no external definition *links and runs*;
  gcc/clang leave the symbol UND and fail to link. C99 leaves the inline-vs-external choice
  unspecified, so mcc's choice is conforming and strictly more useful — and matching
  gcc/clang would break working code and is self-host-sensitive. Resolved by decision.
- [x] **§7.21.6.1/2 — `-Wformat` printf/scanf format-string vs argument checking implemented.**
  Opt-in (`-Wformat`, NOT under `-Wall`) so default builds and the self-host are byte-identical;
  all logic is gated on `warn_format` (`src/mcc.h`, `options_W` in `src/libmcc.c`). At a
  recognized printf/scanf-family call (`format_func_spec` name table) with a string-literal
  format, the directives are parsed and each variadic argument checked by broad class
  (integer / floating / pointer) plus argument count, including `*` width/precision args, `%%`,
  length modifiers and scanf's pointer operands (`src/mccgen.c` `format_check` +
  `format_str_literal`, hooked at the call site once args are on the value stack). Diagnostics
  match gcc's `-Wformat` on the mismatch cases; kept class-coarse (not exact width) to avoid
  false positives. cli `wformat_printf_scanf_checking`. *(Future: honor user
  `__attribute__((format))`, exact length-modifier widths, and `-Wformat-nonliteral`.)*
- [x] **§6.6/§G — complex constant init with an over-range real part now yields `inf` in a
  LOCAL initializer too.** A *local* `double complex z = <overflowing-float-const> + 0.0*I`
  (e.g. `INFINITY + 0.0*I`, since mcc's `INFINITY` is `1e10000f`) used to store `0` for the
  real part (static was already correct). Root cause: `gen_complex_op`'s constant-fold path
  called `init_putv` assuming `gen_op` folds, but `gen_op` declines to fold a non-finite FP
  op (`inf+0`) outside a `CONST_WANTED` context (line ~2627) — so in a local init the runtime
  result was computed then discarded and `init_putv` stored 0. Fix: gate that fold path on
  `CONST_WANTED` (its documented purpose is static initializers); non-const contexts use the
  robust runtime path, which handles infinities correctly (`src/mccgen.c` `gen_complex_op`).
  Static/aggregate/struct complex consts still fold; full ctest 34/34 + self-host byte-identical.
  cli `complex_const_init_overflow`.
