# C99 & C11 Conformance & Platform TODO

Goal: implement every detail of ISO/IEC 9899:1999 (C99) **and** ISO/IEC 9899:2011
(C11), backed by regression tests across all targets (x86_64, i386, ARM, AArch64,
RISC-V 64), platforms (ELF/PE/Mach-O), and both glibc and musl.

References: **N1256** (the C99 working draft) and **N1570** (the C11 working
draft); clause numbers below cite N1570 unless a C99-only rule is noted.
Each item records the standard requirement, the current `mcc` behaviour
(with how it is observed — `file:line` or a test result), and the precise gap.

The default standard is **C99** (`cversion=199901`, `src/libmcc.c:860`); `-std=c11`
selects C11. Items below apply at the relevant `-std` as noted.

Severity tags:
- **[BUG]** — wrong codegen, crash, hang, or rejects-valid C99/C11.
- **[FEATURE]** — a standard feature that is absent.
- **[DIAG]** — a constraint violation that `mcc` accepts with no diagnostic, or a diagnostic that is wrong/misleading.
- **[OPT]** — optional in the standard (a conforming implementation may omit it); listed for the "implement everything" goal.
- **[TASK]** — test/infrastructure work, not a language feature.
- **[LIMITATION]** — a host/platform constraint outside `mcc`'s control.
- **[DIFF]** — a known, deliberate difference from a reference compiler; not a defect.

Legend: `[ ]` open · `[~]` partial · `[x]` done.

Every open item is 3-way verified against gcc 15.3 and clang 22 (the project has
a history of false-positive TODO entries from unverified audits — re-verify any
item against the live binary before "fixing" it). Per the standing goal, each
item must ship with a regression test (cli/exec/diff) across the relevant
targets before it is marked done.

This file tracks only **open and partial** work; verified-complete items are
dropped once their regression tests pass in the suite (cli-suite + exec-suite +
the macho/wine/qemu matrix). The remaining entries below are partials (`[~]`):
either ongoing audit tasks, deliberate documented differences from the reference
compilers, or low-priority residual gaps.

---

## Platform / host limitations (pre-existing)

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
  diagnostic fires and that the valid forms still compile. A full mechanical
  parity sweep across all ~312 `mcc_error`/`mcc_warning`/`mcc_pedantic` call
  sites in `src/` — to confirm the diagnostics that predate this tracker each
  have a regression test — remains an ongoing audit.

- [~] **[DIFF] diff3 differential divergences on macOS.**
  The `diff3` suite compiles each golden with `mcc`, gcc, and clang and flags
  cases where `mcc` differs from a gcc==clang consensus. Four such divergences
  remain on macOS, none of which is an `mcc` defect (`predefined_macros`
  __STDC_VERSION__ default; `bitfields_ms` implementation-defined layout;
  `cleanup` teardown ordering; `c11_freestanding_headers` where `mcc` is the
  most conformant of the three). The suite returns non-zero on any divergence
  by design; these four are the expected residual on a macOS host.

---

## §6.5–6.6 Expressions & constant expressions

- [~] **[DIAG] §6.5.16.1 — assignment to a const-qualified struct/union is diagnosed (as a warning).**
  Audit re-check correction: `a = b` where `a` is `const struct S` *does* warn
  "assignment of read-only location" — `vstore` calls `verify_assign_cast`
  (`src/mccgen.c:3557`) whose const check (`:3456`) fires for aggregates too,
  exactly as for the scalar case. mcc's long-standing stance is to *warn* on
  assignment-to-const (scalar and aggregate alike); gcc/clang *error*. Only the
  severity differs. Promoting to a default-on error would be more conformant but
  is a broad change to a long-standing lenient warning — deferred.
  3-way: mcc=warns | gcc=error | clang=error.

- [~] **[DIAG] §6.6p3 — comma operator in an integer constant expression diagnosed only under -pedantic.**
  A required constant expression "shall not contain ... comma operators" outside
  an unevaluated subexpression. `int a[(1,2)];` is `mcc_pedantic`
  (`src/mccgen.c:7448`): silent at default level, but `-pedantic` warns and
  `-pedantic-errors` hard-errors. clang is also lenient at default; gcc rejects
  the file-scope case by default via its VLA-at-file-scope path. Effectively
  handled — consider whether to promote to a default-on warning. Low priority.

---

## §6.7 Declarations

- [~] **[DIAG] §6.7.4p2 — `_Noreturn` on a non-function object diagnosed only under -pedantic.**
  Function specifiers shall declare only functions. `_Noreturn int x;` is
  diagnosed via `mcc_pedantic` (`src/mccgen.c:9509`, gated on bit 128 = the
  `_Noreturn` keyword vs `__attribute__((noreturn))`): silent at default,
  `-pedantic` warns, `-pedantic-errors` errors. gcc warns by default; clang
  errors by default. Effectively handled under `-pedantic`; consider promoting
  to a default-on warning to match gcc. (The parallel `inline int x;` is a
  default-on hard error at `src/mccgen.c:9504` — intentionally asymmetric since
  gcc treats `_Noreturn` on an object as an extension.)

---

## §6.10 / §5 Preprocessor & environment

- [~] **[DIFF] §6.10.3.3p3 — invalid token paste is warn-and-continue (deliberate leniency).**
  `C(+,-)` / `C(*,/)` emit a warning and continue (rc=0, output `+ -` / `* /`).
  The result is UB (no diagnostic strictly mandated); mcc deliberately recovers
  rather than aborting, and this is locked in by the `pp_invalid_paste` exec
  golden. gcc/clang hard-error; this is an intentional, tested difference, kept.
  (The genuinely-broken comment-introducer case `//`/`/*` IS a hard error now.)
  3-way: mcc=warn(rc=0) | gcc=error | clang=error.

---

## Sweep 2 — additional gaps

### §6.4 / §6.10 lexical & preprocessor

- [~] **[BUG] §5.1.1.2 / §6.10 — `\`+whitespace+newline not spliced (now terminates, still not gcc-compat).**
  The earlier hang is fixed; the residual is behavioral: `-c` hard-errors "stray
  '\'" and `-E` emits a stray `\` token, whereas gcc/clang treat `\`+ws+NL as a
  line-continuation splice with a `-Wbackslash-newline-escape` warning. Making
  `handle_stray` consume `\`+whitespace+newline as a warned splice would match
  the references. Low priority (extension; mcc's strict-ISO rejection is
  defensible).
  3-way: mcc=stray-`\`/no-splice | gcc/clang=splice+warn.

### §6.7 / §6.2 / §6.9 declarations

- [~] **[DIFF] §6.7.2.1 — no-declarator tagged struct *member*: mcc matches gcc `-fms-extensions` (silent).**
  `struct S { struct T { int x; }; };` — re-verified 3-way: gcc *default* (no
  `-fms-extensions`) warns "declaration does not declare anything"; but gcc
  **`-fms-extensions` is silent even under `-pedantic`**, and clang
  `-fms-extensions -pedantic` instead warns "anonymous structs are a Microsoft
  extension". mcc enables MS extensions by default (so the nested tag becomes an
  anonymous member), which matches gcc `-fms-extensions` exactly — silent. The two
  references disagree once MS extensions are on, and mcc tracks gcc; adopting
  clang's MS-extension warning would diverge from gcc. Left as a defensible DIFF.
  (`src/mccgen.c` already warns when `ms_extensions == 0`.)

- [~] **[DIFF] §6.8.6.4p1 — `return <expr>;` in `void` / `return;` in non-`void`: CONFORMANT (diagnosed as a default-on warning).**
  §6.8.6.4 is a *constraint*, which requires only that a conforming implementation
  issue **a diagnostic** — it does not mandate an error. mcc emits a default-on
  warning ("void function returns a value" / "'return' with no value") and
  upgrades it to an error under `-Werror`, so the standard's requirement is met.
  This is mcc's deliberate, **consistent** lenient-warning stance for constraint
  violations — verified identical to the const-assignment §6.5.16.1 case (both
  warn by default, both become errors under `-Werror`). gcc/clang choosing to make
  it a hard error by default is their policy, not a standard requirement. Left as a
  conformant DIFF (forcing an error would diverge from mcc's coherent
  permissive-by-default / `-Werror`-enforces philosophy for no conformance gain).

### §7 library / floating-point builtins

- [~] **[FEATURE] §F/§7.12 — GCC/Clang floating-point builtins (constants + classification done).**
  Added to `runtime/include/mccdefs.h` as constant-foldable macros (`#ifndef`-
  guarded so the BSD/Apple defines win where present): `__builtin_inf`/`inff`/
  `infl`, `huge_val`/`valf`/`vall`, `nan`/`nanf`/`nanl`/`nans*` (inf = folded
  overflow product, nan = `0.0/0.0`), and the `isnan`/`isinf`/`isfinite`/
  `isunordered`/`isgreater`/`isgreaterequal`/`isless`/`islessequal`/`islessgreater`
  classification + comparison builtins. Direct use compiles, links, and folds in
  a constant expression (`static int a=__builtin_isnan(0.0/0.0)`), matching gcc on
  all 5 arches. exec `fp_builtins`. Also added `fabs`/`fabsf`/`fabsl`,
  `signbit`/`signbitf`/`signbitl` (incl. `-0.0` detection via `1/x`), and
  `copysign`/`copysignf`/`copysignl` as constant-foldable macros — verified 3-way.
  `fpclassify`/`isnormal`: **work correctly via glibc + `-lm`** (verified:
  `fpclassify` returns `FP_ZERO`/`FP_NORMAL`/`FP_INFINITE`/`FP_NAN`/`FP_SUBNORMAL`
  and `isnormal` is correct for normal/zero/subnormal/inf). They route to glibc's
  `__fpclassify` function (not the gcc builtin) because mcc defines no `__GNUC__`,
  so they need `-lm` where gcc's builtin does not — a usability DIFF, not a
  correctness gap. They cannot be replaced by clean constant-foldable macros: the
  subnormal test needs the *per-type* smallest-normal threshold, which a
  type-agnostic macro can't know (gcc uses a type-aware builtin). The macro
  builtins above also multi-evaluate their argument (intrinsics don't). The two
  DIFFs below (NAN sign, signbit return) come from glibc's *own* fallback macros
  via the same no-`__GNUC__` routing — both verified conforming.

- [~] **[DIFF] §7.12/F.2.1 — `NAN` yields a *negative* NaN: CONFORMANT (sign unspecified); root cause = mcc doesn't define `__GNUC__`.**
  glibc's `NAN`/math macros pick their implementation by `__GNUC_PREREQ`; mcc
  defines **no `__GNUC__`** (a deliberate, conservative tcc-lineage choice — not
  claiming GCC compatibility avoids pulling in gcc-only header code paths), so
  glibc falls to its non-builtin fallback (`(0.0f/0.0f)` → sign-bit-set NaN).
  Annex F leaves the NaN sign **unspecified**, so `-nan` is fully conforming; it
  merely differs cosmetically from gcc/clang's `+nan`. The only "fix" is to make
  mcc advertise `__GNUC__ >= 6` so glibc routes to `__builtin_nan` — a broad,
  risky GCC-compat claim that changes behavior across *every* system header, taken
  purely to match an unspecified sign. Not worth it; left as a conformant DIFF.
  3-way: mcc=`-nan` | gcc/clang=`nan` (all conforming).

- [~] **[DIFF] §7.12.3.6 — `signbit(-1.0)` returns 128: CONFORMANT (nonzero iff negative); same `__GNUC__` root cause.**
  §7.12.3.6 requires only a **nonzero** value when the sign is negative — `128`
  is nonzero, and verified mcc+glibc gives nonzero iff negative / zero iff
  non-negative, so it is **conforming**. As above, glibc routes `signbit` to its
  extern `__signbit` (returns the 0x80 sign byte) rather than `__builtin_signbit`
  because mcc defines no `__GNUC__`; mcc's own `__builtin_signbit(-1.0)` correctly
  returns `1` (used directly, off the glibc path). Matching gcc/clang's exact `1`
  would again require the risky blanket `__GNUC__` claim for no conformance gain.
  Left as a conformant DIFF. 3-way: mcc=128 | gcc/clang=1 (all conforming).

---

## Landed (2026-06-30 audit cycle)

A fresh 4-agent clause-by-clause differential sweep (each finding 3-way verified
vs gcc 15.3 / clang 22 against the live binary). 12 confirmed silent-acceptance /
rejects-valid gaps closed; each ships with a cli regression test and the full
suite (ctest 30/30) + self-host byte-identical fixpoint stay green.

- [x] §6.10.3p6 — duplicate function-like macro parameter rejected — cli `pp_macro_name_constraints`.
- [x] §6.10.8p4 — `defined` / `__VA_ARGS__` rejected as a `#define`/`#undef` macro name — cli `pp_macro_name_constraints`.
- [x] §6.10.8p2 — `#define`/`#undef` of the `__STDC__`/`__STDC_VERSION__`/`__STDC_HOSTED__` predef family diagnosed (user-settable `__STDC_WANT_*` left alone) — cli `pp_macro_name_constraints`.
- [x] §6.5.3.4 — ISO `_Alignof(expression)` diagnosed under -pedantic (GNU `__alignof__` exempt) — cli `sizeof_alignof_void`.
- [x] §6.5.3.4 — `_Alignof(void)` rejected; `sizeof(void)` / `sizeof(*void*)` diagnosed under -pedantic (GNU `__alignof__` exempt) — cli `sizeof_alignof_void`.
- [x] §6.9.1p4 — function definition declared `typedef` rejected — cli `function_def_typedef`.
- [x] §6.7.2.1p18 — initialization of a struct flexible array member diagnosed under -pedantic — cli `init_brace_constraints`.
- [x] §6.7.9p11 — too many braces around a scalar initializer (`int x={{1}}`) diagnosed under -pedantic — cli `init_brace_constraints`.
- [x] §6.5.3.2 — dereference of a `void *` diagnosed under -pedantic (skipped in unevaluated/`sizeof` contexts) — cli `void_pointer_deref`.
- [x] §6.6p4 — signed `+`/`-`/`*` constant-expression overflow diagnosed under -pedantic — cli `const_integer_overflow`.

---

## Landed (2026-06-30 — round 2)

A 14-dimension differential workflow surfaced 17 confirmed findings; **all are now
implemented**. 3-way verified, each ships a cli/exec regression test; full ctest
30/30, the diff3 differential suite, and the self-host byte-identical fixpoint
stay green.

- [x] §6.7.9p17-20 — designated-initializer positional continuation out of a sub-aggregate (`{.in[0]=1, 2, 3}` → in={1,2}, t=3); the elided sub-list ends and hands the comma back when a designator can't apply to it (`.field` not a member / `.` in an array / `[` in a struct), so designator-after-designator runs and deep nesting work — cli `designated_init_continuation`.
- [x] §6.7.6.2 — pointer-to-VLA parameter `int (*a)[m]` keeps its VLA dimension (TYPE_NEST now reaches the nested `[m]`) — cli `pointer_to_vla_param`.
- [x] §6.10.8.1 — `__LINE__` as a multi-line macro argument uses its own token's line (snapped at argument collection) — cli `line_macro_arg`.
- [x] §6.6p6 — conditional with a non-constant operand in the discarded arm rejected as a non-ICE (bit-field/enum/case) — cli `conditional_ice`.
- [x] §6.4.5p2 — `u8` mixed with a wide string literal rejected; `u8`+narrow concat still valid (new `TOK_U8STR`) — cli `u8_string_concat`.
- [x] §6.7.2.1p11 — consecutive `_Bool` bit-fields pack into one byte (ABI) — cli `bool_bitfield_packing`.
- [x] §6.3.1.8 — mixed `_Complex`×real arithmetic keeps the wider real type (no float narrowing) — cli `complex_real_precision`.
- [x] §7.20.4.1p1 — `INT64_C`/`UINT64_C` typed `int_least64_t`/`uint_least64_t` (hosted + freestanding) — cli `int64_c_type`.
- [x] §6.7.9p4 — block-scope (automatic) compound-literal address rejected in a static initializer — cli `static_init_and_ucn`.
- [x] §6.5.16.1p1/§6.3.2.1p1 — whole-struct assignment with a const-qualified member diagnosed — cli `expr_constraints`.
- [x] §6.6p6 — floating-folded bit-field width / enumerator / case label diagnosed under -pedantic — cli `ice_float_constraints`.
- [x] §6.5.15p3 — `?:` with exactly one `void` operand diagnosed under -pedantic — cli `expr_constraints`.
- [x] §6.5.4 — float↔pointer constant cast (both directions) rejected — cli `expr_constraints`.
- [x] §6.4.3p2 — UCN < 0x00A0 in a string/char literal diagnosed under -pedantic (pre-C23) — cli `static_init_and_ucn`.
- [x] §6.7.1p7 — block-scope function declaration with auto/register rejected — cli `param_and_blockfn_storage`.
- [x] §6.5.1.1p2 — `_Generic` association with an incomplete type (void / forward struct) diagnosed under -pedantic — cli `generic_atomic_restrict_constraints`.
- [x] §6.7.2.4p3 — `_Atomic(qualified-type)` / `_Atomic(atomic-type)` rejected — cli `generic_atomic_restrict_constraints`.
- [x] §6.7.6.3p2 — storage-class specifier other than `register` on a parameter rejected — cli `param_and_blockfn_storage`.
- [x] §6.7.3p2 — `restrict` on a non-pointer (array) type rejected — cli `generic_atomic_restrict_constraints`.
