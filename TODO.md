# TODO

Legend: `[ ]` open · `[~]` in progress · `[x]` done (then removed).

---

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

### Open items — actionable residual gaps (mcc behind a real gcc==clang consensus)

Diagnostic gaps (mcc silent or weaker where **both** gcc and clang diagnose):

- [ ] §6.11.6p1 (L453) — no `-Wstrict-prototypes` for `int f();` empty-paren
      (non-prototype) declarator; gcc/clang warn.
- [ ] §6.2.2p7 (L523) — `static int x; int x;` (internal→external linkage clash)
      accepted silently; gcc/clang reject. (Reverse order is already rejected.)
- [ ] §6.7.2.1p2 (L1416) — a tagged member that declares nothing
      (`struct S { union T { int x; }; };`) accepted silently; gcc/clang warn
      "declaration does not declare anything".
- [ ] §6.7.9p1 (L1676) — designated initializer `{[1]=2}` not diagnosed under
      `-std=c89 -pedantic-errors`; gcc/clang error.
- [ ] §6.7.9p11 (L1700/L1750) — redundant braces around a scalar `int y={{5}}`
      draw no diagnostic even at `-Wall`; gcc/clang warn.
- [ ] §6.7.9p13 (L1752) — initializing a struct from an incompatible scalar gives
      a misleading "'{' expected" message instead of an incompatible-type
      diagnostic (quality).
- [ ] §6.8.3p5 (L1797) — label at end of a block is only a **`-Wall` warning**
      ("deprecated use of label at end of compound statement"), not escalated by
      `-pedantic-errors`; gcc/clang error under `-pedantic-errors`. (NB: not a
      regression — the diagnostic exists at mccgen.c:8664, just `warn_all`-gated.)
- [ ] §6.9p3 (L1908) — a `static` function declared and used but never defined
      draws no diagnostic; gcc/clang warn.
- [ ] §6.9.2p3 (L1954/L1960) — file-scope tentative `static int arr[];` (no size)
      draws no diagnostic; gcc/clang warn "assumed to have one element".
- [ ] §6.10.3.2p1 (L2043) — a malformed `#`/`##` operator in a macro that is
      never invoked is not diagnosed at definition; gcc/clang error at definition.
- [ ] §7.21.6.2 (L3849) — **no `scanf` format-argument checking**: `scanf("%d",
      &dbl)` (double* vs `%d`) is silent even at `-Wall`; gcc/clang warn. (printf
      checking now exists — see FIXED L3807; scanf is the remaining half.)

Semantic / runtime divergences:

- [ ] §6.7.4p6 (L1529) — a plain `inline`-only function with no external
      definition still links (mcc emits an out-of-line body); gcc/clang give
      "undefined reference" — mcc does not implement C99 inline-definition
      semantics (no emitted external symbol).
- [ ] §7.12 NaN sign (L2695/L2766) — `printf("%f", NAN)` prints `-nan` and
      `signbit(NAN)` is nonzero; `signbit(-3.0)`/`signbit(-0.0)` return `128`
      (non-normalized) rather than `1`; gcc/clang give `+nan` and `1`.
- [ ] §F.10.11 (L5868) — `isgreater(NaN, 1.0)` raises `FE_INVALID` (mcc uses an
      ordered comparison); the macros must be quiet — gcc/clang raise nothing.

Target-specific partials (correct on some arches, unreachable on x86_64):

- [ ] §7.16.1.4p3 / §7.15.4p3 (L3214/L3317) — the `va_start` second-arg-not-last
      and register-parameter `-Wvarargs` checks fire on **arm64/arm** but are
      unreachable on **x86_64 SysV / i386**, where `va_start` expands to a
      frame-reading macro (mccdefs.h) that bypasses the parser hook
      (check_va_start_last_param, mccgen.c:5736). Same target limitation noted in
      commit 06692300; C9911 retag to `mcc:✓` was arm64-only.

By-design ergonomics (root cause: default `__STDC_VERSION__ == 199901L`):

- [ ] §6.10.8.1 (L473) — mcc defaults to C99 (`199901L`) while gcc/clang default
      to a later edition, so C11 names need explicit `-std=c11`: `static_assert`
      (§7.2, L2198/L2211), `aligned_alloc` (§7.22, L4437), `TIME_UTC`/
      `timespec_get` (§7.27, L4908/L4948). mcc ships the full C11 freestanding
      surface, so this is an advertised-version choice, not a missing feature.

### Classified — NOT actionable (no migration needed)

The remaining ~100 still-diverging rows are intentionally out of scope:

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

**Bottom line:** since C9911.md was generated, **34 flagged gaps have closed**
(retag those rows `mcc:✓`) and **11 more partially improved**; the actionable
backlog is the 16 Open items above — chiefly diagnostic-strength gaps, three
real semantic divergences (C99 `inline` linkage, NaN sign, `isgreater` quietness),
the x86_64 `va_start` hook, raw-UTF-8 UCN validation, and the C11 default-std
ergonomics.

---
