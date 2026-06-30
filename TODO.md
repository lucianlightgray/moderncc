# TODO — test-coverage backlog

One actionable item per C99/C11 **feature** (every `###` subsection of
[C9911.md](C9911.md)): audit the existing cli/exec/diff3/preprocess regression
coverage for that feature and its sub-features, and add the missing cases. Each
line carries the feature's requirement count and how many of those are known
**mcc gaps** (`mcc:✗`/`mcc:~` in C9911.md) — prioritise features with gaps, and
the cases that pin the standard's *constraints* (negative tests) and *semantics*
(run-and-compare) rather than just "it compiles".

Scope: **238 features**, ~3928 requirements, ~169 flagged mcc gaps. Conventions: 3-way verify (mcc/gcc 15.3/clang 22),
ship a regression test with every fix, keep the full ctest suite + byte-identical
self-host fixpoint green. Check items off as each feature's coverage is audited.

> Test homes: `tests/cli/cases.h` (diagnostics/driver), `tests/exec/` goldens
> (compile-and-run), `tests/diff3/` (3-way stdout), `tests/preprocess/` (3-way `-E`).

---

## §4 Conformance & §5 Environment

- [ ] §4 Conformance — audit & extend test coverage (13 reqs).
- [ ] §5 Environment — audit & extend test coverage (1 req).
- [ ] §5.1 Conceptual models — audit & extend test coverage (90 reqs, **3 mcc-gaps**).
- [ ] §5.2 Environmental considerations — audit & extend test coverage (121 reqs).
- [ ] §6.11 Future language directions — audit & extend test coverage (9 reqs, **1 mcc-gap**).
- [ ] Added gaps & cross-checks (not in the consolidated draft) — audit & extend test coverage (16 reqs, **3 mcc-gaps**).

## §6.2.1-§6.2.4 Scopes, linkages, name spaces, storage durations

- [ ] §6.2.1 Scopes of identifiers — audit & extend test coverage (13 reqs).
- [ ] §6.2.2 Linkages of identifiers — audit & extend test coverage (7 reqs, **1 mcc-gap**).
- [ ] §6.2.3 Name spaces of identifiers — audit & extend test coverage (4 reqs, **1 mcc-gap**).
- [ ] §6.2.4 Storage durations of objects — audit & extend test coverage (12 reqs, **1 mcc-gap**).

## §6.2.5-§6.2.8 Types, representations, compatible/composite types, alignment

- [ ] §6.2.5 Types — audit & extend test coverage (34 reqs, **2 mcc-gaps**).
- [ ] §6.2.6 Representations of types — audit & extend test coverage (17 reqs).
- [ ] §6.2.7 Compatible type and composite type — audit & extend test coverage (9 reqs).
- [ ] §6.2.8 Alignment of objects (C11) — audit & extend test coverage (10 reqs).

## §6.3 Conversions

- [ ] §6.3 (introduction) — audit & extend test coverage (2 reqs).
- [ ] §6.3.1 Arithmetic operands — audit & extend test coverage (0 reqs).
- [ ] §6.3.1.1 Boolean, characters, and integers — audit & extend test coverage (5 reqs).
- [ ] §6.3.1.2 Boolean type — audit & extend test coverage (1 req).
- [ ] §6.3.1.3 Signed and unsigned integers — audit & extend test coverage (3 reqs).
- [ ] §6.3.1.4 Real floating and integer — audit & extend test coverage (4 reqs).
- [ ] §6.3.1.5 Real floating types — audit & extend test coverage (3 reqs).
- [ ] §6.3.1.6 Complex types — audit & extend test coverage (1 req).
- [ ] §6.3.1.7 Real and complex — audit & extend test coverage (2 reqs).
- [ ] §6.3.1.8 Usual arithmetic conversions — audit & extend test coverage (9 reqs).
- [ ] §6.3.2 Other operands — audit & extend test coverage (0 reqs).
- [ ] §6.3.2.1 Lvalues, arrays, and function designators — audit & extend test coverage (5 reqs, **1 mcc-gap**).
- [ ] §6.3.2.2 void — audit & extend test coverage (1 req).
- [ ] §6.3.2.3 Pointers — audit & extend test coverage (8 reqs).

## §6.4 Lexical elements

- [ ] §6.4 General — audit & extend test coverage (11 reqs).
- [ ] §6.4.1 Keywords — audit & extend test coverage (5 reqs).
- [ ] §6.4.2 Identifiers — audit & extend test coverage (19 reqs).
- [ ] §6.4.3 Universal character names — audit & extend test coverage (5 reqs).
- [ ] §6.4.4 Constants — audit & extend test coverage (77 reqs, **2 mcc-gaps**).
- [ ] §6.4.5 String literals — audit & extend test coverage (21 reqs).
- [ ] §6.4.6 Punctuators — audit & extend test coverage (6 reqs).
- [ ] §6.4.7 Header names — audit & extend test coverage (8 reqs).
- [ ] §6.4.8 Preprocessing numbers — audit & extend test coverage (3 reqs).
- [ ] §6.4.9 Comments — audit & extend test coverage (6 reqs).
- [ ] §6.4 mcc-specific lexical gaps (added in gap analysis) — audit & extend test coverage (7 reqs, **2 mcc-gaps**).

## §6.5.1-§6.5.4 Primary/postfix/unary expressions

- [ ] §6.5 Expressions (general) — audit & extend test coverage (13 reqs).
- [ ] §6.5.1 Primary expressions — audit & extend test coverage (7 reqs).
- [ ] §6.5.1.1 Generic selection (C11) — audit & extend test coverage (11 reqs, **1 mcc-gap**).
- [ ] §6.5.2 Postfix operators — audit & extend test coverage (2 reqs).
- [ ] §6.5.2.1 Array subscripting — audit & extend test coverage (3 reqs).
- [ ] §6.5.2.2 Function calls — audit & extend test coverage (13 reqs).
- [ ] §6.5.2.3 Structure and union members — audit & extend test coverage (8 reqs).
- [ ] §6.5.2.4 Postfix increment and decrement operators — audit & extend test coverage (6 reqs, **1 mcc-gap**).
- [ ] §6.5.2.5 Compound literals (C99) — audit & extend test coverage (9 reqs).
- [ ] §6.5.3 Unary operators — audit & extend test coverage (1 req).
- [ ] §6.5.3.1 Prefix increment and decrement operators — audit & extend test coverage (3 reqs, **1 mcc-gap**).
- [ ] §6.5.3.2 Address and indirection operators — audit & extend test coverage (8 reqs).
- [ ] §6.5.3.3 Unary arithmetic operators — audit & extend test coverage (5 reqs).
- [ ] §6.5.3.4 The `sizeof` and `_Alignof` operators (C11; "The `sizeof` operator" in C99) — audit & extend test coverage (8 reqs).
- [ ] §6.5.4 Cast operators — audit & extend test coverage (7 reqs, **1 mcc-gap**).
- [ ] Added / cross-cutting items (gaps and corners the draft did not enumerate) — audit & extend test coverage (7 reqs, **1 mcc-gap**).

## §6.5.5-§6.5.14 Binary operators

- [ ] §6.5.5 Multiplicative operators — audit & extend test coverage (7 reqs).
- [ ] §6.5.6 Additive operators — audit & extend test coverage (14 reqs).
- [ ] §6.5.7 Bitwise shift operators — audit & extend test coverage (8 reqs).
- [ ] §6.5.8 Relational operators — audit & extend test coverage (9 reqs).
- [ ] §6.5.9 Equality operators — audit & extend test coverage (7 reqs, **1 mcc-gap**).
- [ ] §6.5.10 Bitwise AND operator — audit & extend test coverage (4 reqs).
- [ ] §6.5.11 Bitwise exclusive OR operator — audit & extend test coverage (4 reqs).
- [ ] §6.5.12 Bitwise inclusive OR operator — audit & extend test coverage (4 reqs).
- [ ] §6.5.13 Logical AND operator — audit & extend test coverage (4 reqs).
- [ ] §6.5.14 Logical OR operator — audit & extend test coverage (4 reqs).

## §6.5.15-§6.5.17 Conditional/assignment/comma & §6.6 Constant expressions

- [ ] §6.5.15 Conditional operator — audit & extend test coverage (9 reqs).
- [ ] §6.5.16 Assignment operators (general) — audit & extend test coverage (4 reqs, **1 mcc-gap**).
- [ ] §6.5.16.1 Simple assignment — audit & extend test coverage (9 reqs, **1 mcc-gap**).
- [ ] §6.5.16.2 Compound assignment — audit & extend test coverage (5 reqs, **1 mcc-gap**).
- [ ] §6.5.17 Comma operator — audit & extend test coverage (4 reqs).
- [ ] §6.6 Constant expressions — audit & extend test coverage (12 reqs, **1 mcc-gap**).
- [ ] Additional / edge-case checkboxes added this pass — audit & extend test coverage (6 reqs, **1 mcc-gap**).

## §6.7.1-§6.7.3 Declarations: storage/type specifiers/qualifiers

- [ ] §6.7 Declarations — audit & extend test coverage (13 reqs, **2 mcc-gaps**).
- [ ] §6.7.1 Storage-class specifiers — audit & extend test coverage (12 reqs, **1 mcc-gap**).
- [ ] §6.7.2 Type specifiers — audit & extend test coverage (6 reqs, **1 mcc-gap**).
- [ ] §6.7.2.4 Atomic type specifiers (C11) — audit & extend test coverage (6 reqs, **1 mcc-gap**).
- [ ] §6.7.3 Type qualifiers — audit & extend test coverage (16 reqs, **1 mcc-gap**).
- [ ] §6.7.3.1 Formal definition of restrict — audit & extend test coverage (10 reqs).

## §6.7.2.1-§6.7.2.4 Struct/union/enum/atomic specifiers

- [ ] §6.7.2.1 Structure and union specifiers — audit & extend test coverage (45 reqs, **1 mcc-gap**).
- [ ] §6.7.2.2 Enumeration specifiers — audit & extend test coverage (12 reqs).
- [ ] §6.7.2.3 Tags — audit & extend test coverage (12 reqs).
- [ ] §6.7.2.4 Atomic type specifiers (C11) — audit & extend test coverage (5 reqs).

## §6.7.4-§6.7.5 Function specifiers & alignment specifier

- [ ] §6.7.4 Function specifiers — audit & extend test coverage (14 reqs, **4 mcc-gaps**).
- [ ] §6.7.5 Alignment specifier (`_Alignas`) — C11 — audit & extend test coverage (15 reqs).

## §6.7.6 Declarators

- [ ] §6.7.6 Declarators (general) — audit & extend test coverage (6 reqs).
- [ ] §6.7.6.1 Pointer declarators — audit & extend test coverage (5 reqs, **2 mcc-gaps**).
- [ ] §6.7.6.2 Array declarators — audit & extend test coverage (23 reqs, **3 mcc-gaps**).
- [ ] §6.7.6.3 Function declarators (including prototypes) — audit & extend test coverage (22 reqs, **2 mcc-gaps**).

## §6.7.7-§6.7.9 Type names/typedef/initialization

- [ ] §6.7.7 Type names (C99 §6.7.6) — audit & extend test coverage (10 reqs).
- [ ] §6.7.8 Type definitions / typedef (C99 §6.7.7) — audit & extend test coverage (12 reqs, **1 mcc-gap**).
- [ ] §6.7.9 Initialization (C99 §6.7.8) — audit & extend test coverage (66 reqs, **4 mcc-gaps**).

## §6.8 Statements and blocks

- [ ] §6.8 Statements and blocks (general) — audit & extend test coverage (10 reqs).
- [ ] §6.8.1 Labeled statements — audit & extend test coverage (6 reqs, **1 mcc-gap**).
- [ ] §6.8.2 Compound statement — audit & extend test coverage (2 reqs).
- [ ] §6.8.3 Expression and null statements — audit & extend test coverage (7 reqs, **1 mcc-gap**).
- [ ] §6.8.4 Selection statements — audit & extend test coverage (3 reqs).
- [ ] §6.8.4.1 The if statement — audit & extend test coverage (4 reqs).
- [ ] §6.8.4.2 The switch statement — audit & extend test coverage (15 reqs, **1 mcc-gap**).
- [ ] §6.8.5 Iteration statements — audit & extend test coverage (6 reqs, **1 mcc-gap**).
- [ ] §6.8.5.1 The while statement — audit & extend test coverage (1 req).
- [ ] §6.8.5.2 The do statement — audit & extend test coverage (1 req).
- [ ] §6.8.5.3 The for statement — audit & extend test coverage (5 reqs).
- [ ] §6.8.6 Jump statements — audit & extend test coverage (2 reqs).
- [ ] §6.8.6.1 The goto statement — audit & extend test coverage (6 reqs, **1 mcc-gap**).
- [ ] §6.8.6.2 The continue statement — audit & extend test coverage (3 reqs).
- [ ] §6.8.6.3 The break statement — audit & extend test coverage (2 reqs).
- [ ] §6.8.6.4 The return statement — audit & extend test coverage (8 reqs, **2 mcc-gaps**).

## §6.9 External definitions

- [ ] §6.9 External definitions (general) — audit & extend test coverage (9 reqs, **1 mcc-gap**).
- [ ] §6.9.1 Function definitions — audit & extend test coverage (24 reqs, **6 mcc-gaps**).
- [ ] §6.9.2 External object definitions — audit & extend test coverage (5 reqs, **1 mcc-gap**).
- [ ] Added gaps and edge cases (not in the consolidated draft) — audit & extend test coverage (8 reqs, **4 mcc-gaps**).

## §6.10.1-§6.10.3 Conditional inclusion, source inclusion, macro replacement

- [ ] §6.10 General (directive structure) — audit & extend test coverage (8 reqs).
- [ ] §6.10.1 Conditional inclusion — audit & extend test coverage (18 reqs).
- [ ] §6.10.2 Source file inclusion — audit & extend test coverage (7 reqs).
- [ ] §6.10.3 Macro replacement (general) — audit & extend test coverage (17 reqs, **1 mcc-gap**).
- [ ] §6.10.3.1 Argument substitution — audit & extend test coverage (3 reqs).
- [ ] §6.10.3.2 The `#` (stringize) operator — audit & extend test coverage (6 reqs, **1 mcc-gap**).
- [ ] §6.10.3.3 The `##` (token paste) operator — audit & extend test coverage (7 reqs).
- [ ] §6.10.3.4 Rescanning and further replacement — audit & extend test coverage (5 reqs).
- [ ] §6.10.3.5 Scope of macro definitions — audit & extend test coverage (3 reqs).

## §6.10.4-§6.10.9 Line/error/pragma/null/predefined macros/_Pragma

- [ ] §6.10.4 Line control — audit & extend test coverage (6 reqs).
- [ ] §6.10.5 Error directive — audit & extend test coverage (2 reqs).
- [ ] §6.10.6 Pragma directive — audit & extend test coverage (7 reqs, **1 mcc-gap**).
- [ ] §6.10.7 Null directive — audit & extend test coverage (1 req).
- [ ] §6.10.8 Predefined macros — audit & extend test coverage (18 reqs).
- [ ] §6.10.9 _Pragma operator (C99/C11) — audit & extend test coverage (5 reqs, **1 mcc-gap**).

## §7.1-§7.5 Library intro, assert, complex, ctype, errno

- [ ] §7.1.1 Definitions of terms — audit & extend test coverage (5 reqs).
- [ ] §7.1.2 Standard headers — audit & extend test coverage (10 reqs).
- [ ] §7.1.3 Reserved identifiers — audit & extend test coverage (9 reqs).
- [ ] §7.1.4 Use of library functions — audit & extend test coverage (12 reqs).
- [ ] §7.2 Diagnostics `<assert.h>` — audit & extend test coverage (11 reqs, **2 mcc-gaps**).
- [ ] §7.3 Complex arithmetic `<complex.h>` — audit & extend test coverage (53 reqs).
- [ ] §7.4 Character handling `<ctype.h>` — audit & extend test coverage (21 reqs).
- [ ] §7.5 Errors `<errno.h>` — audit & extend test coverage (8 reqs).

## §7.6-§7.8 fenv, float, inttypes

- [ ] §7.6 `<fenv.h>` — audit & extend test coverage (71 reqs, **3 mcc-gaps**).
- [ ] §7.7 `<float.h>` — audit & extend test coverage (25 reqs, **1 mcc-gap**).
- [ ] §7.8 `<inttypes.h>` — audit & extend test coverage (26 reqs).

## §7.9-§7.11 iso646, limits, locale

- [ ] §7.9 Alternative spellings `<iso646.h>` — audit & extend test coverage (15 reqs).
- [ ] §7.10 Sizes of integer types `<limits.h>` — audit & extend test coverage (26 reqs).
- [ ] §7.11 Localization `<locale.h>` — audit & extend test coverage (64 reqs).

## §7.12 <math.h>

- [ ] §7.12 Introduction — types and macros — audit & extend test coverage (22 reqs, **3 mcc-gaps**).
- [ ] §7.12.1 Treatment of error conditions — audit & extend test coverage (13 reqs).
- [ ] §7.12.2 The FP_CONTRACT pragma — audit & extend test coverage (6 reqs, **1 mcc-gap**).
- [ ] §7.12.3 Classification macros — audit & extend test coverage (19 reqs, **1 mcc-gap**).
- [ ] §7.12.4 Trigonometric functions — audit & extend test coverage (21 reqs).
- [ ] §7.12.5 Hyperbolic functions — audit & extend test coverage (18 reqs).
- [ ] §7.12.6 Exponential and logarithmic functions — audit & extend test coverage (42 reqs).
- [ ] §7.12.7 Power and absolute-value functions — audit & extend test coverage (15 reqs).
- [ ] §7.12.8 Error and gamma functions — audit & extend test coverage (12 reqs).
- [ ] §7.12.9 Nearest integer functions — audit & extend test coverage (24 reqs).
- [ ] §7.12.10 Remainder functions — audit & extend test coverage (9 reqs).
- [ ] §7.12.11 Manipulation functions — audit & extend test coverage (12 reqs).
- [ ] §7.12.12 Maximum, minimum, and positive difference functions — audit & extend test coverage (9 reqs).
- [ ] §7.12.13 Floating multiply-add — audit & extend test coverage (3 reqs).
- [ ] §7.12.14 Comparison macros — audit & extend test coverage (22 reqs).

## §7.13-§7.16 setjmp, signal, stdalign, stdarg

- [ ] §7.13 Non-local jumps `<setjmp.h>` — audit & extend test coverage (13 reqs).
- [ ] §7.14 Signal handling `<signal.h>` — audit & extend test coverage (15 reqs).
- [ ] §7.15 Alignment `<stdalign.h>` (C11) — audit & extend test coverage (17 reqs, **1 mcc-gap**).
- [ ] §7.16 Variable arguments `<stdarg.h>` — audit & extend test coverage (24 reqs, **2 mcc-gaps**).

## §7.16-§7.18 stdarg/stdbool/stdint-or-atomic boundary

- [ ] §7.15 / §7.16 (C11) Variable arguments `<stdarg.h>` — audit & extend test coverage (28 reqs, **1 mcc-gap**).
- [ ] §7.16 / §7.18 (C11) Boolean type and values `<stdbool.h>` — audit & extend test coverage (9 reqs).
- [ ] §7.17 (C11) Atomics `<stdatomic.h>` (C11-only; not present in C99) — audit & extend test coverage (86 reqs, **7 mcc-gaps**).

## <stddef.h> & <stdint.h>

- [ ] §7.17 [§7.19] Common definitions <stddef.h> — audit & extend test coverage (14 reqs).
- [ ] §7.18 [§7.20] Integer types <stdint.h> — audit & extend test coverage (44 reqs).
- [ ] Cross-cutting: freestanding & namespace — audit & extend test coverage (3 reqs).

## §7.21 <stdio.h>

- [ ] §7.21.1 Introduction — audit & extend test coverage (15 reqs).
- [ ] §7.21.2 Streams — audit & extend test coverage (8 reqs).
- [ ] §7.21.3 Files — audit & extend test coverage (17 reqs).
- [ ] §7.21.4 Operations on files — audit & extend test coverage (18 reqs).
- [ ] §7.21.5 File access functions — audit & extend test coverage (31 reqs).
- [ ] §7.21.6 Formatted input/output functions — audit & extend test coverage (117 reqs, **2 mcc-gaps**).
- [ ] §7.21.7 Character input/output functions — audit & extend test coverage (37 reqs, **1 mcc-gap**).
- [ ] §7.21.8 Direct input/output functions — audit & extend test coverage (6 reqs).
- [ ] §7.21.9 File positioning functions — audit & extend test coverage (19 reqs).
- [ ] §7.21.10 Error-handling functions — audit & extend test coverage (12 reqs).

## §7.22 <stdlib.h>

- [ ] §7.22 General — audit & extend test coverage (11 reqs).
- [ ] §7.22.1 Numeric conversion functions — audit & extend test coverage (45 reqs).
- [ ] §7.22.2 Pseudo-random sequence generation functions — audit & extend test coverage (10 reqs).
- [ ] §7.22.3 Memory management functions — audit & extend test coverage (35 reqs).
- [ ] §7.22.4 Communication with the environment — audit & extend test coverage (56 reqs).
- [ ] §7.22.5 Searching and sorting utilities — audit & extend test coverage (22 reqs).
- [ ] §7.22.6 Integer arithmetic functions — audit & extend test coverage (10 reqs).
- [ ] §7.22.7 Multibyte/wide character conversion functions — audit & extend test coverage (27 reqs).
- [ ] §7.22.8 Multibyte/wide string conversion functions — audit & extend test coverage (16 reqs).
- [ ] §7.22 Added gap items (beyond the consolidated draft) — audit & extend test coverage (8 reqs, **1 mcc-gap**).

## §7.23 <string.h> & §7.24 <tgmath.h>

- [ ] §7.24.1 / §7.21.1 String function conventions — audit & extend test coverage (5 reqs).
- [ ] §7.24.2 / §7.21.2 Copying functions — audit & extend test coverage (14 reqs).
- [ ] §7.24.3 / §7.21.3 Concatenation functions — audit & extend test coverage (7 reqs).
- [ ] §7.24.4 / §7.21.4 Comparison functions — audit & extend test coverage (19 reqs).
- [ ] §7.24.5 / §7.21.5 Search functions — audit & extend test coverage (31 reqs).
- [ ] §7.24.6 / §7.21.6 Miscellaneous functions — audit & extend test coverage (15 reqs).
- [ ] §7.25 / §7.22 Type-generic math `<tgmath.h>` — audit & extend test coverage (24 reqs, **7 mcc-gaps**).

## §7.25-§7.27 threads(C11)/time

- [ ] §7.26 Threads `<threads.h>` (C11) — audit & extend test coverage (136 reqs, **9 mcc-gaps**).
- [ ] §7.27 Date and time `<time.h>` (C11 §7.27 / C99 §7.23) — audit & extend test coverage (106 reqs, **3 mcc-gaps**).

## §7.28-§7.31 uchar(C11)/wchar/wctype

- [ ] §7.28.1 `<uchar.h>` — Introduction (C11) — audit & extend test coverage (9 reqs, **1 mcc-gap**).
- [ ] §7.28.2 Restartable multibyte/wide character conversion functions (C11) — audit & extend test coverage (30 reqs).
- [ ] §7.29.1 `<wchar.h>` — Introduction — audit & extend test coverage (9 reqs).
- [ ] §7.29.2 Formatted wide character input/output functions — audit & extend test coverage (61 reqs).
- [ ] §7.29.3 Wide character input/output functions — audit & extend test coverage (34 reqs).
- [ ] §7.29.4 General wide string utilities — audit & extend test coverage (82 reqs).
- [ ] §7.29.5 Wide character time conversion functions — audit & extend test coverage (3 reqs).
- [ ] §7.29.6 Extended multibyte/wide character conversion utilities — audit & extend test coverage (38 reqs).
- [ ] §7.30.1 `<wctype.h>` — Introduction — audit & extend test coverage (5 reqs).
- [ ] §7.30.2 Wide character classification utilities — audit & extend test coverage (35 reqs).
- [ ] §7.30.3 Wide character case mapping utilities — audit & extend test coverage (14 reqs).
- [ ] §7.31.15 Future library directions — `<wchar.h>` (C11 numbering; §7.26.12 in C99) — audit & extend test coverage (1 req).
- [ ] §7.31.16 Future library directions — `<wctype.h>` (C11 numbering; §7.26.13 in C99) — audit & extend test coverage (1 req).

## Annexes F, G, K (normative)

- [ ] Conditional-conformance macros (gap-probe summary) — audit & extend test coverage (3 reqs).
- [ ] §F.1 Introduction — audit & extend test coverage (3 reqs).
- [ ] §F.2 Types — audit & extend test coverage (8 reqs).
- [ ] §F.3 Operators and functions — audit & extend test coverage (8 reqs).
- [ ] §F.4 Floating to integer conversion — audit & extend test coverage (3 reqs, **1 mcc-gap**).
- [ ] §F.5 Binary-decimal conversion — audit & extend test coverage (4 reqs).
- [ ] §F.6 The return statement (C11) *(new in C11; no C99 counterpart)* — audit & extend test coverage (1 req).
- [ ] §F.7 Contracted expressions *(C99 §F.6)* — audit & extend test coverage (3 reqs).
- [ ] §F.8 Floating-point environment *(C99 §F.7)* — audit & extend test coverage (10 reqs, **3 mcc-gaps**).
- [ ] §F.9 Optimization *(C99 §F.8)* — audit & extend test coverage (13 reqs).
- [ ] §F.10 Mathematics `<math.h>` *(C99 §F.9)* — audit & extend test coverage (60 reqs, **1 mcc-gap**).
- [ ] §G.1 Introduction — audit & extend test coverage (2 reqs).
- [ ] §G.2 Types — audit & extend test coverage (3 reqs, **2 mcc-gaps**).
- [ ] §G.3 Conventions — audit & extend test coverage (3 reqs).
- [ ] §G.4 Conversions — audit & extend test coverage (5 reqs, **5 mcc-gaps**).
- [ ] §G.5 Binary operators — audit & extend test coverage (9 reqs, **9 mcc-gaps**).
- [ ] §G.6 Complex arithmetic `<complex.h>` — audit & extend test coverage (16 reqs).
- [ ] §G.7 Type-generic math `<tgmath.h>` — audit & extend test coverage (2 reqs, **2 mcc-gaps**).
- [ ] §K.1 Background *(C11; the entire Annex K is optional and conditional on `__STDC_LIB_EXT1__`)* — audit & extend test coverage (2 reqs).
- [ ] §K.2 Scope — audit & extend test coverage (2 reqs).
- [ ] §K.3 Library — audit & extend test coverage (85 reqs).

## Annexes C, D, E, H, I, J (sequence points, UCN ranges, limits, portability)

- [ ] §C Sequence points (normative) — audit & extend test coverage (15 reqs).
- [ ] §D Universal character names for identifiers — C99 §D / C11 §D.1, §D.2 (normative) — audit & extend test coverage (12 reqs, **10 mcc-gaps**).
- [ ] §E Implementation limits (normative) — audit & extend test coverage (20 reqs).
- [ ] §H Language-independent arithmetic — relation to LIA-1 (informative) — audit & extend test coverage (11 reqs).
- [ ] §I Common warnings (informative) — audit & extend test coverage (17 reqs, **16 mcc-gaps**).
- [ ] §J.1 Unspecified behavior (informative) — audit & extend test coverage (24 reqs).
- [ ] §J.2 Undefined behavior (informative) — audit & extend test coverage (87 reqs).
- [ ] §J.3 Implementation-defined behavior (informative) — audit & extend test coverage (80 reqs).
- [ ] §J.4 Locale-specific behavior (informative) — audit & extend test coverage (8 reqs).
- [ ] §J.5 / feature-test macros — *(added gaps)* implementation-defined feature reporting — audit & extend test coverage (11 reqs).

