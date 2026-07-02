# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (then removed).

---

## ★ TOP PRIORITY: implement `-S` (assembly output) — 2026-07-02

**Why:** the last cluster of non-actionable `[~]` rows in [C9911.md](C9911.md) —
§5.1.2.3/§5.1.2.4 volatile & atomic ordering (L218/L227/L255), §7.6.1 FENV pragma
state, §7.12.2 FP_CONTRACT contraction, §F.8 — are all tagged *"not separately
testable: requires `-S`/codegen inspection that mcc does not expose"*. Adding a
real `-S` closes that gap and lets those rows be verified from an assembly listing
the way they are against gcc/clang.

**Architecture constraint (read first).** mcc is TCC-derived: the per-arch backend
(`src/arch/<arch>/<arch>-gen.c`) emits **raw machine-code bytes straight into
`cur_text_section->data[]`** via `g()`/`o()`/`gen_le32()` (`x86_64-gen.c:137`).
There is **no textual-assembly IR** anywhere in the pipeline, and `-S` currently
hard-errors (`libmcc.c:2274`). So `-S` cannot be "stop before the assembler" —
there is no assembler stage to stop before. The realistic design is a
**relocation-aware disassembler** that runs *after* a normal object-style compile
(reloc entries kept symbolic, no link) and prints AT&T-syntax text plus section/
symbol directives, in a form that mcc's own `mccasm.c` (and gas) can re-assemble.
Round-trip identity — `mcc -S x.c -o x.s && mcc -c x.s` byte-equal to
`mcc -c x.c` — is the correctness oracle and the primary test.

### 1. Front end: `main` + arg parsing accept `-S` and a new output type

- [ ] **Add `MCC_OUTPUT_ASM`** — `include/libmcc.h:45-49` (new value `6`, next to
  `MCC_OUTPUT_OBJ 3`). This is the public output-type token everything else keys on.
- [ ] **`MCC_OPTION_S` handler** — `src/libmcc.c:2274`: delete the
  `mcc_error_noabort("-S … not supported")` stub and replace with
  `x = MCC_OUTPUT_ASM; goto set_output_type;`, mirroring `MCC_OPTION_c`
  (`libmcc.c:2038`) and `MCC_OPTION_E` (`libmcc.c:2243`). The option is already
  in the table (`libmcc.c:1611,1703`), so no table change is needed.
- [ ] **`mcc_set_output_type`** — `src/libmcc.c:~955-982`: give `MCC_OUTPUT_ASM`
  the same "compile only, don't link, don't pull startup files/libs" treatment as
  `MCC_OUTPUT_OBJ` (the `if (output_type == MCC_OUTPUT_OBJ)` block at `libmcc.c:982`).
- [ ] **Default output filename → `.s`** — `src/mcc.c:default_outputfile()`
  (`mcc.c:242-265`): add an `MCC_OUTPUT_ASM` branch that rewrites the extension to
  `.s` (parallel to the `.o` branch at `mcc.c:254`), matching gcc/clang
  (`foo.c` → `foo.s`).
- [ ] **`main` per-file compile loop + guards** — `src/mcc.c`:
  - `mcc.c:331` — extend the `MCC_OUTPUT_OBJ` "cannot specify libraries / one
    output file for many inputs" checks to also cover `MCC_OUTPUT_ASM` (gcc rejects
    `-S -o out.s a.c b.c`).
  - `mcc.c:375` — the `if (s->output_type == MCC_OUTPUT_OBJ && !s->option_r) break;`
    that forces **one object per input file** must also fire for `MCC_OUTPUT_ASM`
    (each `.c` → its own `.s`; no cross-file linking).
  - `mcc.c:391-400` — the `mcc_output_file(s, s->outfile)` dispatch already runs
    for any non-MEMORY/PREPROCESS/`syntax_only` type, so `MCC_OUTPUT_ASM` flows
    through once routed (see §3); verify `-S -` (stdin) and `-S -o -` (stdout) land.
- [ ] **Help text** — `src/mcc.c` `help[]` (~`mcc.c:15`, by the `-c` line): document
  `-S  Compile only; emit assembly (.s), do not assemble or link`.

### 2. Output routing: send `MCC_OUTPUT_ASM` to a new emitter

- [ ] **`mcc_output_file`** — `src/objfmt/mccelf.c:2874`: add
  `if (s->output_type == MCC_OUTPUT_ASM) return asm_output_file(s, filename);`
  ahead of the existing OBJ/PE/MachO/ELF dispatch. `asm_output_file` follows the
  **`elf_output_obj` model** (`mccelf.c:2853`): object-style, relocations stay
  **symbolic/section-relative** (never resolved to final addresses — that is what
  keeps operands printable as `sym+addend`).
- [ ] **Declare** `asm_output_file`/`mcc_output_asm` in `src/mcc.h` next to the
  other `ST_FUNC … output` decls.

### 3. New driver: `src/mccdis.c` — section/symbol/directive layer (arch-independent)

- [ ] **New file `src/mccdis.c`** implementing `asm_output_file(MCCState*, const
  char *filename)`. It walks `s1->sections[]` and `symtab` and emits a gas/AT&T
  listing (auto-picked up by the `src/*.c` glob at `CMakeLists.txt:1698`). Responsibilities:
  - Open `filename` (or stdout for `-`) via the existing `mcc_fopen` path used by
    the preprocessor (`mcc.c:326`).
  - For each output section emit the directive header — `.text` / `.data` /
    `.section .rodata` / `.bss`, plus `.align`, and `.globl`/`.weak`/`.type
    @function|@object`/`.size` derived from the `symtab` `ElfW(Sym)` entries
    (`st_info` bind/type, `st_shndx`, `st_value`, `st_size`).
  - Interleave **symbol labels** (`name:`) at their `st_value` offsets so text is
    grouped per function.
  - For `.data`/`.rodata`/`.bss` emit `.byte`/`.short`/`.long`/`.quad`/`.string`/
    `.zero` from the section bytes, splicing symbolic relocations (see below) as
    `.quad sym+addend` where a reloc covers those bytes.
  - Emit a trailing `.ident`/`.section .note.GNU-stack` epilogue to match gcc's
    non-exec-stack convention (keeps re-assembled objects identical).

### 4. New per-arch disassembler: `src/arch/<arch>/<arch>-dis.c` (the hard part)

- [ ] **New file per live arch** (`x86_64` first; then `arm64`, `i386`, `arm`,
  `riscv64`), auto-included by the backend glob `src/arch/${MCC_CPU}/*.c`
  (`CMakeLists.txt:1701`, `CONFIGURE_DEPENDS` — a re-configure picks it up, no edit
  needed). Export e.g. `ST_FUNC int mcc_disasm_insn(MCCState*, Section *text,
  addr_t off, addr_t end, FILE *out)` returning instruction length; loop it over
  each function's `[st_value, st_value+st_size)` range.
  - It is the **inverse of the existing assembler** `src/arch/<arch>/<arch>-asm.c`
    + tables in `<arch>-asm.h` — reuse the same mnemonic/opcode tables where
    possible so encode/decode stay in sync.
  - **Relocation-aware operands:** before printing an instruction, look up any
    reloc in `text->reloc` whose `r_offset` falls inside the instruction; render
    that operand as the **symbol name (+addend)** from `symtab`, not the raw
    immediate/displacement bytes. This is what makes the output re-assemblable and
    diffable, and is the whole point (it exposes the symbolic call/access sequence
    the volatile/atomic `[~]` rows need).
  - AT&T syntax + `%`-registers + `$`-immediates for x86, to match
    `gcc -S`/`clang -S` default so goldens and humans read the same dialect.

### 5. Build system

- [ ] No hand-edits needed for the globs (`CMakeLists.txt:1698` covers
  `src/mccdis.c`; `:1701` covers `src/arch/*/*-dis.c`) — but a **re-run of CMake
  configure** is required so `CONFIGURE_DEPENDS` re-globs. Confirm each backend's
  `-dis.c` compiles under the `ONE_SOURCE` single-TU build too (`src/mcc.c:1-9`
  includes `libmcc.c`; check whether `mccdis.c` should be `#include`d there or
  stay a separate TU).

### 6. Tests — prove `-S` matches gcc/clang conventions

- [ ] **Round-trip identity (primary oracle)** — new `tests/asm/s_roundtrip/` +
  CMake case beside the existing `asm_c_connect` harness (`CMakeLists.txt:2618`):
  for a spread of inputs, assert `mcc -S x.c -o x.s && mcc -c x.s -o x.s.o`
  produces an object **byte-identical** to `mcc -c x.c -o x.o` (or, if directive
  ordering perturbs layout, functionally-identical via symbol/reloc compare).
- [ ] **gcc/clang parity smoke** — `mcc -S` and `gcc -S`/`clang -S` on the same
  input both produce a `.s` that **gas and mcc's assembler both accept**; diff the
  *symbol/section/reloc* shape (not exact text — instruction selection differs by
  design), à la the 3-way `mcctest` philosophy.
- [ ] **cli-suite cases** — `tests/cli/cases.h`: `-S` default naming (`foo.c`→
  `foo.s`), `-S -o out.s`, `-S -o -` (stdout), `-S a.c b.c` rejected with libs,
  `-S` + `-c` mutual-override warning (the `set_output_type` path, `libmcc.c:2041`).
- [ ] **Motivating conformance test** — `tests/exec/` (or new `tests/asm/observe/`):
  compile a `volatile` a=b=c chain and an atomic-ordering snippet with `-S`, grep
  the listing for the expected load/store count — the exact check C9911 L218/L227/
  L255 say is impossible today.
- [ ] **Once green, retag C9911** — flip the six "requires `-S`" `[~]` rows
  (§5.1.2.3/§5.1.2.4 L218/L227/L255, §7.6.1p2/p3 L2361/L2365, §7.12.2p1 L2728) to
  `mcc:✓` with an `-S`-verified note, and update the TODO "NOT-TESTABLE = 6" tally
  (§ 2026-06-30 crawl, line ~90) and the Totals header in C9911.md.

### Sequencing

1 → 2 → 3 give a runnable `-S` that emits directives + labels + `.byte` blobs for
text (correct but unreadable); 4 replaces the text `.byte` blobs with real
mnemonics; 6 locks it in. Ship x86_64 end-to-end before fanning the disassembler
out to the other four arches.

---

## C9911 → full_language.c coverage sweep — 2026-07-01 (worked to completion 2026-07-01)

Drove every runtime-observable requirement in [C9911.md](C9911.md) into the
differential test `tests/diff/full_language.c` (69 3-way-validated functions,
~570 clauses; `mcctest` driver now links `-lm`). All follow-up items below have
been implemented or resolved; suite is 34/34 green (incl. both mcctest variants,
exec-suite, cli-suite) and mcc self-compiles all 8 of its own sources.

### Landed this round (implemented + verified)

- **#line presumed name in `__FILE__`** (§6.10.4p3) — FIXED in `mccpp.c`
  (`mccpp_putfile` no longer resolves a relative `#line`/marker name against the
  current file's dir; `__FILE__` is now verbatim like gcc/clang; `true_filename`
  still holds the real path). Full suite green.
- **§7.6 `<fenv.h>`, §7.3.7–9 complex libm, §7.25 `<tgmath.h>` evaluated dispatch**
  — added `s7_6_fenv_test` / `s7_1_complex_libm_test` / `s7_23_tgmath_eval_test`
  to full_language.c (unblocked by the `-lm` driver change); pass mcctest +
  mcctest-bcheck, byte-identical to gcc.
- **§7.28–7.31 `<uchar.h>/<wchar.h>/<wctype.h>` runtime** — added
  `tests/exec/types/wchar_library.c` + goldens.h entry (`wchar_library`, gated
  `os=linux`); exercises char16/32_t, mbrtoc*/c*rtomb, wcs*/wmem*, swprintf/
  swscanf, wcstod/wcstol family, isw*/tow*/wctrans, wcstok. exec-suite green.
  (Kept in the exec-suite rather than full_language.c: system `<wchar.h>`
  redefines `FILE`, clashing with the file's `mcclib.h` opaque `FILE`.)
- **Diagnostic/constraint requirements → `tests/cli/cases.h`** — added 10 validated
  cases covering the previously-uncovered high-value ones: `static_assert_fail`,
  `static_assert_nonconst` (§7.2), `switch_duplicate_case`, `goto_undefined_label`
  (§6.8), `redefinition_object`, `array_of_functions`, `conflicting_redecl`,
  `void_param_named` (§6.9), `bitfield_nonint` (§6.7.2.1), `computed_goto_ext`
  (§6.8.6.1 GNU ext — confirms mcc supports it). cli-suite green. The remaining
  routed §4/§5, §6.2–6.5, §6.10 and Annex diagnostics are already exercised by
  the existing cases.h / tests/diagnostics / tests/exec/errors corpus or are
  `mcc:✓` per C9911 (spot-verified: read-only/case-label/duplicate-case/flexible-
  array/_Alignas already covered); this backlog is closed as representative +
  pre-existing coverage.

### Resolved as NOT actionable (by-design / optional-feature / non-deterministic)

Consistent with the 2026-06-30 classification below; each verified this round:

- **`_Alignof` on an automatic `_Alignas` object** (§6.2.8p5, GNU ext): works for
  static/global objects (via `VT_SYM`); only automatic-storage objects return the
  natural alignment. A fix needs the local's alignment plumbed through the SValue;
  an attempted change regressed `_Alignof(a[0])`, so reverted. GNU-extension corner
  (ISO `_Alignof` takes a type), low value, high risk — left as a documented partial.
- **array-parameter `[const]` not enforced** (§6.7.6.3p4): verified mcc is
  uniformly permissive about body-assignment through *any* qualified parameter
  (`const int a`, `int *const a`, `int a[const]` are all silent; a plain
  `const int` **local** does warn) — this is mcc's documented permissive-by-default
  stance, not a bracket-specific bug.
- **bcheck miscompiles `_Complex ==`** (§6.5.9): `mcc -b` on a `_Complex`
  equality compare hits an invalid memory access (complex temporaries created by
  `cplx_local` aren't registered with the bounds checker). Real but niche
  (optional `-b` feature + complex equality), deep bounds-checker codegen with
  high regression risk; already worked around in full_language.c (`s7_1_complex`
  compares parts). Left as a documented known limitation. Repro:
  `double complex a=1+2*I,b=a; a==b;` + `-b`.
- **`runtime/include/stdatomic.h` is clang-incompatible** (§7.17): the shim uses
  GNU `__atomic_*` on `_Atomic`-qualified pointers (clang needs `__c11_atomic_*`)
  and its `__atomic_is_lock_free` decl collides with clang's builtin. mcc itself
  compiles+links+runs all of C11 atomics correctly, and clang is **not** the
  `mcctest` gate (mcc-vs-gcc); the shim's clang-compat is a portability nice-to-have,
  not a conformance gap. Left tracked as a known header-shim limitation.
- **§7.26 `<threads.h>` / §7.27 `timespec_get`/`TIME_UTC`**: runtime-observable
  values are non-deterministic (scheduling, thread ids, wall-clock) and need
  `-pthread`; enum constants are implementation-defined. Not expressible as a
  deterministic differential sub-test. (Compile/link is exercised elsewhere.)
- **§6.7.1p3 thread-local-init "leak" / §6.7.5p1 `_Alignas` on an automatic
  array**: flagged by the sweep but did **not** reproduce on minimal 3-way probes
  (all three agree: `41 0` and correctly aligned). False positives — not bugs.

## C9911 re-verification crawl — 2026-06-30 (present status of every flagged divergence)

Re-verified **all 169** `mcc:✗`/`mcc:~` rows in [C9911.md](C9911.md) empirically
against the live binary (`0.9.28rc mob@a25f149b`, x86_64 Linux), 3-way vs
gcc 15.3 / clang 22. Each row got a minimal probe run with the exact flags its
note names; library/runtime rows were compiled **and run**. Verdict tally:

| Verdict | Count | Meaning |
|---|---|---|
| **FIXED ✓** | 34 | mcc now matches the gcc==clang consensus (C9911 row is stale) |
| **CHANGED** | 11 | behavior moved since the map was written — partial improvement, not yet ✓ |
| **STILL diverges** | 118 | reproduces as recorded |
| **NOT-TESTABLE** | 6 | no `-S`/codegen-ordering/optimization-barrier probe exists |

Of the 118 that still diverge, **most are *not* actionable** (consensus
`✗`/`✗`/`✗`, no gcc==clang consensus, UB/recommended-practice, or mcc's
documented permissive-by-default philosophy). The genuinely actionable residue
is the short list under **Open items** below. Two C9911 rows that an automated
pass flagged as regressions were **false alarms** (the probe omitted `-Wall`):
`#pragma STDC …  BOGUS` (§6.10.6p2) and label-at-end-of-block (§6.8.3p5) both
still warn under `-Wall` — see notes.

### FIXED since the map was written (34 — C9911 rows now stale, retag to `mcc:✓`)

Confirmed working on the live binary; these need only a C9911 retag, no code:

- **C11 predefined macros / headers:** `__STDC_UTF_16__`/`__STDC_UTF_32__` now
  defined `1` (§6.10.8.3, L475/L476); `<threads.h>` + `thread_local` compile and
  run (§7.26.1, L539/L4703/L4705).
- **Same-scope redeclaration constraints now hard-error:** `typedef int T; int T;`
  (§6.2.3p1, L530); `typedef`'d function type as a definition (§6.9.1p2,
  L1923/L1961); incomplete return type `struct S f(void){}` (§6.9.1p3, L1924);
  incomplete parameter type in a definition (§6.9.1p7, L1938); object vs typedef
  name-space clash (§6.7.8p3, L1657).
- **C11-feature-in-C99 pedantic diagnostics now fire** under `-std=c99
  -pedantic-errors`: `_Thread_local` (L1326), `_Atomic(...)` (L1354), typedef
  redefinition incl. variably-modified (§6.7p3, L1310/L1311), `u`/`U`/`u8`
  char/string prefixes (§6.4.4.4/§6.4.5, L5057).
- **Non-ISO extensions now diagnosed** under `-pedantic-errors`: binary `0b…`
  constants (§6.4.4.1, L925), `\e` escape (§6.4.4.4, L926), zero-size array
  `int a[0]` (§6.7.6.2p1, L1582), K&R parameter names without types (§6.7.6.3p3,
  L1606).
- **tgmath fully repaired (§7.25, L4654/L4658/L4662/L4663/L4668/L4672/L4673):**
  `nexttoward(f,ld)`→`nexttowardf`, type-generic `creal`/`cimag`, correct
  element-type resolution table — all match gcc/clang now.
- **Complex `*`/`/` infinity-recovery (Annex G.5.1, L5897/L5898/L5903/L5904):**
  mcc now implements the §G.5.1 recovery tables at `-O0` and `-O2`;
  `(INF+0i)*(1+1i)` → `inf+inf·i`, matching gcc/clang.
- **printf format checking (§7.21.6.1, L3807):** `%d` vs non-int now warns under
  `-Wall`.
- `_Imaginary` / inline-static handled: static object in extern-inline now warns
  (§6.7.4p2, L1521).

### CHANGED — partial improvements, not yet at consensus (11)

- **§7.27.1/§7.27.2 `struct timespec`/`timespec_get`/`TIME_UTC` (L4911):** now
  compile in **default** mode (glibc POSIX gate), no longer needing `-std=c11`;
  `TIME_UTC`/`timespec_get` still gated on `-std=c11`.
- **§D UCN identifier validation (L6111/L6120/L6121):** the `\uXXXX` **escape**
  form is now validated (rejects out-of-range and initial combining marks,
  matching gcc/clang) — but **raw UTF-8** extended identifiers are still accepted
  unchecked. Partial.
- **§G.5.1p5 (L5905):** mcc's default complex path now recovers infinities (no
  longer naïve); `CX_LIMITED_RANGE ON` still has no observable effect.
- **§7.21.7.7 `gets` (L3978):** now warns `implicit declaration of 'gets'` (was
  silent); gcc/clang still hard-error (removed in C11) — improved, still diverges.
- **Annex I informative warnings now emitted under `-Wall -Wextra` (L6173/L6175/
  L6176/L6178/L6182):** multi-char constant, `i = i++` may-be-undefined, call
  without prototype, unused variable, value-computed-not-used. Informative-only,
  so mcc was conforming either way; these move it toward gcc/clang.

### Open items — actionable residual gaps

*(none — every actionable consensus gap has been implemented and removed. The
diagnostic gaps, raw-UTF-8 validation, and the foldable NaN/Inf builtins landed
this round; the remaining flagged rows are classified below as deliberate or
not-actionable, each with rationale.)*

### Classified — NOT actionable (no migration needed)

The remaining flagged rows are intentionally out of scope:

- **Deliberate, test-encoded design choices:**
  - §6.7.4p6 (L1529) — a plain `inline`-only function used without an external
    definition: mcc emits a **local (non-exported)** definition so the call
    resolves, where gcc/clang (all `-std` modes) give "undefined reference".
    mcc's *symbol export* already matches C99 (the plain-inline symbol is **not**
    globally emitted — verified by `tests/exec/functions_abi/inline.c` +
    `inline2.c`, golden `0 inline_inline_undeclared`); the permissive local
    emission is intentional (plain-`inline`-in-header "just works") and that
    comprehensive export-matrix test asserts it. Changing it would break the test
    and regress real code.
  - §6.10.8.1 (L473) — mcc advertises C99 (`__STDC_VERSION__ == 199901L`) so the
    handful of C11-only names need explicit `-std=c11`: `static_assert`
    (§7.2, L2198/L2211), `aligned_alloc` (§7.22, L4437), `TIME_UTC`/
    `timespec_get` (§7.27, L4908/L4948). mcc ships the full C11 freestanding
    surface — this is an advertised-version identity choice, not a missing
    feature; flipping the default is out of scope.
  - §6.7.2.1p2 (L1416) — a tagged member that declares nothing
    (`struct S { union T { int x; }; };`): the warning exists (mccgen.c:4550) but
    is gated off by `ms_extensions` (on by default) to support MS
    anonymous-by-tag members. Distinguishing an inline-defined tag from a
    referenced one would risk regressing that feature for a cosmetic,
    no-conformance-impact diagnostic.
- **Target-codegen limitation (works on arm64):** §7.16.1.4p3 / §7.15.4p3
  (L3214/L3317) — the `va_start` second-arg-not-last and register-parameter
  `-Wvarargs` checks fire on **arm64/arm** but are unreachable on **x86_64 SysV /
  i386**, where `va_start` expands to a frame-reading macro (mccdefs.h) that
  bypasses the parser hook (`check_va_start_last_param`, mccgen.c). Same target
  limitation noted in commit 06692300; correct usage stays clean on all targets.
- **Cosmetic message quality:** §6.7.9p13 (L1752) — a struct initialized from an
  incompatible scalar reports "'{' expected" rather than "incompatible type"
  (an error either way; rewording touches initializer parsing — not worth the
  regression risk).
- **`mcc:✗ gcc:✗ clang:✗` consensus** — all Annex G `_Imaginary`-type rows
  (§6.2.5p11, §6.7.2p2, §G.2–§G.7: L571/L1345/L5876–L5894/L5901/L5902/L5908/
  L5952). No compiler implements the optional imaginary types — no divergence.
- **No gcc==clang consensus** (mcc matching one side is defensible) — comma in a
  constant expression §6.6p3 (L1277, gcc rejects/clang accepts, mcc=clang);
  inline-definition constraints §6.7.4p2/p3 (L1522/L1523, gcc warns/clang
  varies); atomic-pointer `fetch_add` §7.17.7.5 (L3458, clang-only); atomic flag
  init §7.17.8.1 (L3473, gcc/clang split); `FLT_ROUNDS` after `fesetround`
  (L2470, gcc static like mcc).
- **UB / recommended-practice only** — hex-float-inexact diagnostic §6.4.4.2p7
  (L831, all three silent); incomplete generic-association §6.5.1.1p2 (L974/
  L1117, latent in all three); `__int128` optional extended type §6.2.5p4 (L563).
- **mcc permissive-by-default philosophy** (warns, errors under `-Werror`) —
  the modifiable-lvalue / const-assignment family §6.3.2.1p1, §6.5.2.4p1,
  §6.5.3.1p1, §6.5.4p2, §6.5.16.*, §6.7.3p6, §6.7.6.1p2 (L697/L1030/L1061/L1102/
  L1242/L1251/L1265/L1296/L1370/L1570/L1571/L1592); return-value mismatches
  §6.8.6.4p1 (L1883/L1884); old-style / implicit-int parameters §6.9.1p6, §6.9p1
  (L1928/L1930/L1959/L1963). mcc warns (exit 0) by default, matching its
  documented stance; gcc/clang hard-error. Counted as "accept" by C9911.
- **Optional / diagnostic-only with no consensus target** — atomic memory-order
  argument checks §7.17.7.* (L3425/L3432/L3445/L3478, all three silent at the
  source level); `#pragma STDC … BOGUS` §6.10.6p2 (L2100 — **mcc does warn under
  `-Wall`**, false-alarm regression report); FENV/FP_CONTRACT codegen effects
  §7.6.1, §F.8 (not separately observable); informative Annex I rows.
- **Not separately testable** — §5.1.2.3/§5.1.2.4 volatile & atomic ordering
  (L218/L227/L255), §7.6.1p2/p3 pragma state (L2361/L2365), §7.12.2p1 FP_CONTRACT
  contraction (L2728): all require `-S`/codegen inspection that mcc does not
  expose; observable behavior is correct.
- **Gated by the deliberate no-`__GNUC__` stance** — `<math.h>` `NAN` sign /
  `signbit` §7.12 (L2695/L2766) and `isgreater(NaN,…)` raising `FE_INVALID`
  §F.10.11 (L5868). Root cause: glibc gates its clean `NAN (__builtin_nanf(""))`
  behind `__GNUC_PREREQ(3,3)` and falls back to the trapping `(0.0f/0.0f)` for
  non-GNU compilers (gcc itself traps + prints `-nan` on that fallback — verified).
  **Landed:** mcc's `__builtin_{nan,nanf,nanl,inf,inff,infl,huge_val,huge_valf,
  huge_vall}` are now real foldable, non-trapping constant builtins (mccgen.c +
  mcctok.h; macros dropped from mccdefs.h) that match gcc exactly — clean `+nan`/
  `+inf`, usable in static initializers, no FE_INVALID/FE_OVERFLOW. The residual
  user-facing divergence would close only by predefining `__GNUC__`, a deliberate
  compiler-identity choice out of scope here (would broadly change which
  GNU-gated system-header paths mcc must support, risking the self-host fixpoint).

**Bottom line:** the 2026-06-30 verification found 34 already-closed gaps (retagged
`mcc:✓`) plus 11 partial improvements. This round then **implemented and removed
every remaining actionable item** (each with a cli/exec regression test, full
ctest 34/34, and the byte-identical self-host fixpoint kept green):

- **Diagnostics added/strengthened:** internal→external linkage clash §6.2.2p7;
  tentative-array assumed-one-element §6.9.2p3; braces-around-scalar under `-Wall`
  §6.7.9p11; static-used-never-defined §6.9p3; designated-init pedantic §6.7.9p1;
  label-at-end `-pedantic-errors` escalation §6.8.3p5; new `-Wstrict-prototypes`
  §6.11.6p1; **scanf** format-argument (pointed-to-type) checking §7.21.6.2;
  definition-time bad-`#` macro diagnostic §6.10.3.2p1.
- **Lexer:** raw-UTF-8 identifier validation against Annex D.1/D.2 §D.
- **Builtins:** `__builtin_{nan,nanf,nanl,inf,inff,infl,huge_val,huge_valf,
  huge_vall}` are now real foldable, non-trapping constant builtins matching gcc.

The flagged rows that remain are all **deliberate or not-actionable**, documented
with rationale in the Classified section above (C99 inline local-emission choice,
C99 default-std identity, `ms_extensions` tagged-member gate, x86_64 `va_start`
codegen limitation, cosmetic message wording, Annex-G `_Imaginary` consensus,
no-consensus splits, UB/recommended-practice, permissive-by-default philosophy,
not-separately-testable codegen items, and the `<math.h>` NaN behavior gated by
the deliberate no-`__GNUC__` stance).

---

WARNING!!! DO NOT DO!!! ACHTUNG!!! 
• CST Database for LSP and Optimization layer
• Database uses hierarchical incremental hashes to enable bidirectional lookups starting from any character index in the code
• Hot reload by saving/loading CST snapshots on the fly and on run with --hotreload arg
• Run hotreloads from reconcoliled CST snapshots
• Search the entire codebase for all mac/macos/apple/darwin/mach related conditionals, build a MAC.md documenting where and why the code is gated. 
ACHTUNG!!! DO NOT DO!!! WARNING!!!
