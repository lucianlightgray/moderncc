# TESTS — C99/C11 conformance test index & gap analysis

Cross-index of the C99/C11 conformance tests in the GCC and Clang source trees
against mcc's own suites, with a per-feature gap assessment. Built by reading the
local clones; refresh when they are updated.

**Provenance**

| Source | Location | Ref (at index time) | C99 corpus | C11 corpus |
|---|---|---|---|---|
| GCC | `~/Projects/gcc` | `master` @ `31d967232a9` (2026-07-04, gcc-17 base) | `gcc.dg/c99-*.c` ×135 + `gcc.c-torture/**` | `gcc.dg/c11-*.c` ×126 + `gcc.dg/atomic/*.c` ×48 |
| Clang | `~/Projects/llvm-project` | `main` @ `0f1f456263b5` (2026-07-03, llvmorg-23-init) | `clang/test/C/C99/` ×19 | `clang/test/C/C11/` ×16 |
| mcc | this repo | `main` | `tests/exec/**` ×241 + `tests/diff/**` + `tests/diff3/**` + `tests/ast/**` + `tests/qemu/**` | same suites (unified) |

**How each project organizes C-standard tests**

- **GCC** — one file per feature+intent, named `c99-<feature>-N.c` / `c11-<feature>-N.c`
  under `gcc.dg/`, driven by `dg-do {compile,run,preprocess}` + inline
  `dg-error`/`dg-warning` directives. ~70% are *diagnostic* tests (often compiled
  `-std=c99 -pedantic-errors` or `-std=c90` to pin what each mode accepts/rejects,
  incl. many *negative* tests asserting a later-standard feature is absent). Runtime
  semantics live in `gcc.c-torture/execute/`; atomics get a dedicated 48-file
  `gcc.dg/atomic/` execute suite.
- **Clang** — one file per **WG14 paper** (`nNNNN.c`) under `clang/test/C/C99` and
  `/C11`, header-commented with the paper title and conformance status. Checks via
  `-verify` (`expected-error`/`-warning`), `_Static_assert`/`#if`, `-ast-dump`,
  `-emit-llvm`, or `-E` piped to `FileCheck`. Small, precise, edge-case-driven;
  most feature *breadth* lives elsewhere (`Sema/`, `CodeGen/`, `Preprocessor/`,
  and the larger `C/C23` tree).
- **mcc** — feature-grouped runtime suites. `tests/exec/` (241 `.c`, golden
  stdout/exit in `goldens.h`, run by `runner.c`) is the behavioral driver, with a
  dedicated **`features_c99_c11/` (46 cases)** plus `types/`, `structs_unions/`,
  `statements/`, `vla/`, etc. `tests/diff/` is a 3-way (gcc/clang/tcc) golden-diff
  suite: a `full_language.c` mega-TU + 33 clause-numbered `parts/run_s*.c` runners
  (§6.2…§7.28, Annexes C/D/E/F/G/K). `tests/qemu/` re-runs conformance on all 5
  arches. See also [`C9911.md`](C9911.md) — 3,838 clause-cited requirement rows
  each tagged mcc/gcc/clang, which is the authoritative gap ledger this file
  summarizes.

---

## 1. mcc suite inventory

| Suite | Purpose | ~cases |
|---|---|---|
| `tests/exec/` | Main behavioral/codegen driver; 17 category dirs incl. **`features_c99_c11` (46)**, types (30), statements (25), structs_unions (19), preprocessor (15), lexical (16), pointers_arrays (15), expressions (14), functions_abi (14), vla (5) | 241 |
| `tests/diff/` | 3-way diff vs gcc/clang/tcc: `full_language.c` + clause runners `parts/run_s6_2…s7_28` + Annex C/D/E/F/G/K + `complex_abi/` | ~35 |
| `tests/diff3/` | granular 3-way differential vs gcc **and** clang over the exec corpus (`runner.c` → one `diff3/<name>` per case); distinct from the tcc-based `tests/diff/` | (corpus) |
| `tests/ast/` | AST intention-IR replay differential (docs/AST.md): `asttool` pure-lib checks (`ast/{arena,validate,dump,cfg,provenance,template}`) + ~40 `ast/replay-<case>` exit-code/byte-identical `-O1`-replay-vs-`-O0` gates + the whole-corpus `exec-replay`/`exec-replay-tmpl` columns | 41 |
| `tests/preprocess/` | conditional, diagnostics, directives, expansion, stringize, token-pasting, variadic | ~38 |
| `tests/qemu/` | cross-target runtime (5 arches): `conformance/` (16) + `apple-libc/` (26); + the containerized `docker/` runner and `macho/` drivers | ~42 |
| `tests/cli/` | driver/CLI behavior (deps, aliases, wmain, TLS, suffixes) | 16 |
| `tests/cst/` | CST subsystem (roundtrip, symref, hashinv, macro, kinds) | 24 |
| `tests/sanitize/` | `sanitize-smoke`: compile+link+run a program with the instrumented `mcc_s` (registered only when `MCC_BUILD_SANITIZE`/`diagnostics`) | 1 |
| `tests/diagnostics/` | `compile_errors.c` — expected must-reject compile diagnostics | 1 |
| `tests/{tls,static}/` | TLS models (dyn/static) + static-link smoke | 2 |
| `tests/{embed,behavior,asm,ci,support,bench}/` | libmcc API, bounds stress, asm roundtrip, harness | ~30 |

**Total ≈ 400+ case files** (the exec corpus grows; the `diff3`/`ast` replay columns
re-run it). Suites auto-enable when the reference toolchain / qemu is detected.

**Platform-gated harnesses (labels & `SKIP_RETURN_CODE 77`).** Beyond the portable
suites, several run only where their toolchain/host is present and otherwise
**skip-with-reason** rather than fail.

Every test that is not qemu-labelled additionally carries the **`native`** label
(applied in one pass at the end of configure), and the single `_test-native`
preset base runs `ctest -L native` on every host — there are no per-OS exclude
lists. What actually executes is decided at configure time by the host probes
(wine, the osx cross compilers, a Darwin host, …): where a capability is
missing the test is either registered as a skip-stub or self-skips with
exit 77, so the native suite is always safe to run in full. `ctest -L native -N`
lists exactly what the current tree considers native. The capability labels:

- **macOS / Mach-O** (label `macho`). Cross-consuming: `macho-structural`,
  `macho-codegen-run`, `macho-image-run`, `macho-apple-libc` need the `mcc-x86_64-osx`
  cross (`cmake --preset cross` / `MCC_CROSS_DIR`) and self-skip off x86_64. Native
  self-skipping: `macho-conformance-native`, `macho-stack-protector`, `macho-universal`
  (the `machofat` fat-binary tool; 2-slice case via `xcrun --show-sdk-path`).
  `macho-libsystem-kernel-fused` needs `MCC_DARWIN_HOST=ON` (a macOS/darling host).
  `qemu-arm64-osx` covers arm64-Darwin codegen under qemu. `ast/replay-ld_fallback`
  is excluded on Darwin+arm64; `mcctest`/`-bcheck` skip when the reference cc is a
  Homebrew GNU gcc on Darwin/arm64.
- **Windows / PE** (label `wine`). `pe-wine-conformance` runs mcc's PE output under
  `wine` using the `mcc-x86_64-win32` cross; `pe-native-conformance` and `compile.win32.*`
  run only on a native WIN32 target. mcc's PE output links legacy `msvcrt.dll`, so `-b`
  bounds checking (`mcctest-bcheck`), the `parts-suite`, and (on arm64) `mcctest` self-skip.
  `MCC_BUILD_SANITIZE` now works on Windows too — MSVC AddressSanitizer or mingw trap-mode
  UBSan — feeding `sanitize-smoke`.
- **qemu-user** (label `qemu` — the only non-`native` label). The foreign-arch
  emulation matrix scheduled exclusively by the `qemu-*` presets (`ctest -L qemu`);
  `qemu-arm64-osx` carries `qemu;macho` and stays out of the native suite.

---

## 2. GCC C99 index (`gcc.dg/c99-*.c` ×135 + torture)

| Feature area | # | Example files | Kind |
|---|---|---|---|
| Constant expressions (null-ptr, VLA-size, offsetof, overflow) | 15 | `c99-const-expr-1..15` | compile + diag |
| Flexible array members | 13 | `c99-flex-array-*`, `c99-flex-array-typedef-*` | mostly diag |
| Tags / typedef redecl / type-specifier combos / anon-struct | 9 | `c99-tag-1..6`, `c99-typespec-1` (1055 dg-error!), `c99-anon-struct-1` | diag |
| Non-lvalue array decay / array lvalues | 8 | `c99-array-lval-1..8` | compile + diag |
| Variable-length arrays | 8 | `c99-vla-1/2`, `c99-vla-jump-1..5`, torture `vla-dealloc-1` | compile + diag + exec |
| `<stdint.h>`/`<inttypes.h>` types & limits | 8 | `c99-stdint-1..8` | compile |
| Array declarators (`[static]`, `[*]`, param quals) | 5 | `c99-arraydecl-1..4`, `c99-array-nonobj-1` | diag |
| Integer/long-long constants, promotion, left-shift | ~9 | `c99-intconst-*`, `c99-longlong-*`, `c99-left-shift-1..3`, `c99-intprom-1` | compile + diag |
| Hex float & `<float.h>` | 4 | `c99-hexfloat-1..3`, `c99-float-1` | compile/exec/diag/pp |
| `__func__` | 4 | `c99-func-1..4` | exec + diag |
| `restrict` (incl. param) | 4+ | `c99-restrict-1..4`, torture `restrict-1` | compile + diag + exec |
| `<tgmath.h>` | 4 | `c99-tgmath-1..4` | pp + compile |
| Idempotent/duplicate qualifiers | 4 | `c99-idem-qual-1..3`, `c99-dupqual-1` | compile |
| `_Bool`/`<stdbool.h>` | 4 | `c99-bool-1..4` | exec + diag |
| `_Complex`/`_Imaginary` | 3 + torture | `c99-complex-1/3`, torture `complex-1..7` (exec), `complex-1..6` (compile) | diag + exec |
| Designated initializers | 6 | `c99-init-1..6` | exec (1) + diag |
| for-loop decls / mixed decls / new scopes | 6 | `c99-fordecl-1..3`, `c99-mixdecl-1`, `c99-scope-1/2` | exec + diag |
| Compound literals | 2 + torture | `c99-complit-1/2`, torture `compndlit-1`, `compound-literal-1..3` | exec + diag |
| Implicit-int / implicit-decl **removed** (negative) | 3 | `c99-impl-int-1/2`, `c99-impl-decl-1` | diag (C90-only) |
| Post-C99 features **rejected** under `-std=c99` (negative) | 6 | `c99-{align,atomic,noreturn,static-assert,thread-local}-*` | diag |
| Misc (enum comma, cond-expr, main return, compare-incomplete, digraphs, version/predef) | ~12 | `c99-enum-comma-1`, `c99-condexpr-1`, `c99-main-1`, `c99-digraph-1`, `c99-version-1` | mixed |

Split: ~95 diagnostic / ~15 execute / a few preprocess. No dedicated `c99-inline-*`
or variadic-macro file (inline → torture `inline-1.c`; `__VA_ARGS__` → `gcc.dg/cpp/`).

## 3. GCC C11 index (`gcc.dg/c11-*.c` ×126 + `atomic/` ×48)

| Feature area | # | Example files | Kind |
|---|---|---|---|
| Atomic ops & memory model (**execute**) | 48 | `atomic/c11-atomic-exec-1..7`, `atomic/stdatomic-{load,store,exchange,compare-exchange,op,fence,flag,lockfree}*` | 45 exec + 3 compile |
| FP: `<float.h>`, `_FloatN`, DFP, complex | 21 | `c11-float-1..8`, `c11-floatn-1..8`, `c11-float-dfp-*`, `c11-true_min-1`, `c11-complex-1` | pp/exec/diag |
| `_Static_assert` | 10 | `c11-static-assert-1..10` | compile + diag |
| `_Atomic` syntax/type/constraints | 9 | `c11-atomic-1..6`, `c11-stdatomic-1..3` | compile/diag/pp |
| `_Alignas`/`_Alignof` | 9 | `c11-align-1..9` | compile/exec/diag |
| Unprototyped / old-style / omitted-param fns | 9 | `c11-unproto-1..3`, `c11-old-style-definition-*`, `c11-parm-omit-1..4` | compile + diag |
| Enum (C23 fixed-type rejected) | 6 | `c11-enum-1..6` | compile + diag |
| `_Noreturn` | 5 | `c11-noreturn-1..5` | compile + exec(-2) |
| char16/32_t, u/U/u8 literals, `<uchar.h>` | 4 | `c11-uni-string-1/2`, `c11-utf8char-1`, `c11-utf8str-type` | exec + compile + diag |
| `_Generic` | 4 | `c11-generic-1` (exec), `-2/3/4` (diag) | exec + diag |
| `nullptr` / null-ptr-constant (C23 negative) | 4 | `c11-nullptr-1..3`, `c11-null-pointer-constant-1` | compile |
| for-loop decls | 4 | `c11-fordecl-1..4` | compile + diag |
| `stdarg` | 4 | `c11-stdarg-1..4` | compile + pp |
| `typeof`/`typeof_unqual` (C23 negative) | 3 | `c11-typeof-1..3` | compile/exec/diag |
| Empty init `{}` (C23 negative) | 3 | `c11-empty-init-1..3` | diag |
| Compound literals / labels / `[[attr]]` (C23) | 9 | `c11-complit-1..3`, `c11-labels-1..3`, `c11-attr-syntax-1..3` | compile + diag |
| Anonymous struct/union | 3 | `c11-anon-struct-1..3` | compile + diag |
| `_Thread_local` | 2 | `c11-thread-local-1/2` | compile + diag |
| `_Bool` / bool-limits / version / keywords / binary-const / digit-sep | ~10 | `c11-bool-1`, `c11-version-1/2`, `c11-keywords-1`, `c11-binary-constants-*` | mixed |

Plus C11-relevant families outside the `c11-` prefix: broad Unicode string/char
(`utf16/32/8-*`, ~60), UCN-in-identifier (`ucnid-*`, ~30), and DFP (`gcc.dg/dfp/`,
121). **No Annex K** (`__STDC_LIB_EXT1__`/`aligned_alloc`/`quick_exit`) — GCC does
not implement it.

## 4. Clang C99/C11 index (`test/C/C99` ×19 + `/C11` ×16)

| Feature area | # | Example (WG14 paper) | Kind |
|---|---|---|---|
| `_Complex` / `complex.h` / Annex G | 6 | `n809*` (C99), `n1464` (CMPLX), `n1514` (Annex G), `n1460` | diag+AST+IR |
| FP eval-method / Annex F wide returns / `<float.h>` | 4 | `n1365`, `n1396` (6 targets), `float_h-characteristics`, `dr290` | AST+IR |
| Preprocessor (empty args, intmax arith, limits, STDC pragma) | 4 | `n570`, `n736`, `n590`, `n696` | pp+compile+diag |
| Integer const/promotion/division | 3 | `n629`, `n725`, `n617` | compile |
| Extended identifiers / UCN grammar / UAX#31 | 3 | `n717`, `n1518`, `block-scopes` | diag |
| `_Bool` conversions / bit-field width / signed-char padding | 3 | `n1356`, `n1391` (NaN/inf→bool), `n1310` | diag+IR |
| Aggregate/static init | 3 | `n782`, `n1311`, `n809_3` | diag+IR |
| Temporary lifetime of array-containing rvalues | 2 | `n1285`, `n1285_1` | diag+IR |
| `_Static_assert` / restrict / digraphs / idempotent-qual / implicit-decl-removal / seq-points / ptr↔float / subset macros | 1 ea | `n1330`,`n448`,`digraphs`,`n505`,`n636`,`n1282`,`n1316`,`n1460` | diag/compile |
| `_Atomic` LOCK_FREE macros | 1 | `n1482` | compile |

Edge-case-driven; the `drs/` defect-report tests double as `-std=c99` conformance
(`dr290`, `dr491`, `dr253/466/206`, codegen `dr011/060/094/…`).

---

## 5. Gap matrix — mcc vs gcc/clang

Density: gcc/clang columns = # dedicated feature files; mcc = coverage level.
Gap = assessment for **mcc's** suite.

| Feature | gcc | clang | mcc | Gap |
|---|---|---|---|---|
| `_Bool` / stdbool | 4 | 3 | Y (6) | none |
| `_Complex` (arithmetic/ABI) | 3+13 | 6 | Y (6 + `diff/complex_abi`) | **thin on diagnostics/Annex-G edge cases** |
| `_Imaginary` | 3 | (subset macros) | **N** | consensus omission (all 3 lack it) — conforming |
| VLAs (runtime) | 8 | gated only | Y (5 + behavior) | none for runtime; **goto-into-VLA-scope diag untested** |
| VLA jump/scope diagnostics | 5 (`vla-jump`) | — | partial | **gap: goto/switch into VLA scope** |
| Flexible array members | 13 | — | partial (~1) | **under-tested: union FAM, string-init, typedef FAM, invalid uses** |
| Compound literals | 2+torture | — | Y (~2 + 39 incidental) | ok; lifetime-in-block edge untested |
| Designated initializers | 6 | 1 | Y (~72 incidental) | ok; **negative/constraint cases thin** |
| `inline` semantics (§6.7.4) | torture | — | partial (5) | **real bug: no-extern-def emits global (see §6-A)** |
| `restrict` | 4 | 1 | Y (~8 incidental) | ok |
| for-loop / mixed decls / scopes | 6 | 1 | Y (many) | none |
| `__func__` | 4 | — | Y (6) | none |
| Hex float | 4 | — | Y (1) | thin but adequate |
| Universal char names / UCN-in-ident | 3 + ~30 | 3 | Y (4) | **under-tested vs gcc's `ucnid-*` breadth** |
| Digraphs / trigraphs | 1 | 1 | Y (3/1) | none |
| Integer constants / promotion / left-shift | ~9 | 3 | Y (16 stdint + incidental) | ok |
| `<stdint.h>`/`<inttypes.h>` | 8 | — | Y (16) | none |
| `_Static_assert` | 10 | 2 | Y (3–6) | adequate |
| `_Generic` | 4 | incidental | Y (3–8) | ok; **incomplete-assoc-type diag untested** (consensus) |
| `_Alignas` / `_Alignof` | 9 | — | Y (2–5) | **thin vs gcc: over-align, on-type-vs-decl, constraint diags** |
| `_Noreturn` | 5 | — | **partial (1)** | **gap: only 1 case; add call-returns/UB + constraint** |
| `_Atomic` / stdatomic | 9+48 | 1 | Y (9, strong) | ops/memory-model good; **`<float.h>`-style breadth of RMW ordering thinner than gcc's 48** |
| `_Thread_local` / TLS | 2 | — | Y (3 + `tls/` 4-model) | strong (exceeds gcc's runtime coverage) |
| char16/32_t, u/U/u8 literals | 4 | (C23) | Y (4–7) | ok |
| Anonymous struct/union | 3 | — | Y | ok |
| `<tgmath.h>` | 4 | — | Y (9) | strong |
| `<fenv.h>` | (dfp-adjacent) | 1 | Y (7) | strong |
| K&R / old-style / unprototyped fns | 9 | — | **partial** | **real gap: implicit-int params accepted (§6-A.2)** |
| Implicit function decl **removed** | (negative) | `n636` | Y (errors) | none (mcc errors by default — ledger was stale) |
| `va_start` misuse diagnostic | (constraint) | — | partial | **gap on x86_64-SysV/i386 (§6-A, tracked in TODO)** |
| Annex K bounds-checking | none | none | none | consensus omission — conforming |
| FP eval-method / Annex F wide returns | 21 | 4 | partial (fenv/annexF parts) | **under-tested: `FLT_EVAL_METHOD`, wide-return precision** |
| Translation limits (nesting/include depth) | — | `n590` | **N** | low priority; add a stress case |
| `-Wsequence-point` / unsequenced-mod diag | (cpp) | `n1282` | **N** | low priority |

---

## 6. Findings

### A. Real mcc-specific gaps (mcc diverges from the gcc==clang consensus)

These are semantic/diagnostic defects, not just missing tests — drawn from
[`C9911.md`](C9911.md) (23 `mcc:✗`, 87 `mcc:~`) but **re-verified against the
current build** (the ledger dates to 2026-06-30 and had drifted — see the note):

1. **§6.7.4p6 — plain `inline`, no external definition.** mcc emits a global
   definition, so an `-O0` call *links* (rc 0) where gcc and clang correctly fail
   with `undefined reference` (rc 1). ✓ *Confirmed real semantic gap.*
2. **§6.9.1p6 — old-style K&R identifier-list params.** mcc accepts implicit-`int`
   params (`int g(x){return x;}`) with only a warning (rc 0); gcc `-std=c11` and
   clang reject (rc 1) — C99 removed implicit int. ✓ *Confirmed: missing required
   diagnostic (default-mode error).*
3. **§7.16.1.4p3 — `va_start` 2nd arg not the last named param.** Silently accepted
   on x86_64-SysV / i386 (rc 0, no diagnostic); the check *does* fire on
   arm64/riscv64/PE. Deferred (diagnostic-only; needs a SysV `gen_va_start` rework) —
   see `docs/NOTES.md` "Completed — Now-queue decisions". ✓ *Confirmed; diagnostic-only.*
4. **§7.26.1 — `<threads.h>` precedence.** mcc's bundled `include/threads.h`
   resolves *ahead* of the system header (confirmed via `-M`); basic `thrd_t`/`tss`
   usage still compiles and runs, but this shadowing is the root of the ledger's
   `c11_threads` divergence. *Header-precedence issue, not a hard failure.*

> **Corrected during verification:** C9911.md flags *implicit function declaration*
> (§6.5.2.2) as silently accepted — **stale.** Current mcc errors by default
> (`error: implicit declaration of function …`, rc 1), matching gcc. Not a gap.
> The ledger should be re-swept.

### B. Test-coverage gaps (mcc implements the feature but under-tests vs gcc/clang)

mcc passes these today, but its suite lacks the breadth gcc/clang pin — regressions
would slip through. Ranked by value:

1. **Flexible array members** — gcc has 13 (union FAM, string-init, typedef FAM,
   sizeof, invalid nesting); mcc has ~1. **Add a `features_c99_c11/flexarray_*`
   set of constraint + runtime cases.**
2. **`_Noreturn`** — 1 case vs gcc's 5. Add: noreturn-fn-that-returns (UB/diag),
   noreturn on `main`, constraint violations, `<stdnoreturn.h>` macro.
3. **`_Alignas`/`_Alignof`** — add over-alignment runtime, alignas-on-type vs
   -decl, under-align constraint errors (gcc `c11-align-3` has 28).
4. **VLA jump/scope diagnostics** — gcc `c99-vla-jump-1..5` (goto/switch into a
   VLA scope). mcc tests VLA runtime but not this constraint.
5. **UCN-in-identifier breadth** — gcc `ucnid-*` (~30). mcc has 4 UCN cases.
6. **FP eval-method / Annex F wide returns** — `FLT_EVAL_METHOD`, intermediate
   precision, Annex-F wide-return conformance (gcc 21, clang 4).
7. **`_Complex` diagnostics / Annex G edge cases** — mcc's complex coverage is
   arithmetic/ABI-strong but light on constraint diagnostics and Annex-G special
   values (CMPLX with NaN/inf, `n1464`-style).
8. **Diagnostic/negative tests generally** — ~70% of gcc's C99/C11 files are
   `dg-error`/negative; mcc's `exec/` is execute-oriented. The `tests/diff/` and
   `tests/cli/` suites cover some, but mcc has no systematic *"this must be
   rejected"* corpus mirroring gcc's `-pedantic-errors` negatives.

### C. Consensus omissions (all three lack; conforming — do **not** chase)

- Entire **`_Imaginary`** type family (Annex G §G.2–G.7, ~15 rows) — optional.
- **Annex K** bounds-checking (`__STDC_LIB_EXT1__`) — not in gcc, clang, or mcc.
- Inexact-hex-float-constant diagnostic (recommended practice only).
- Incomplete-`_Generic`-association type not diagnosed.

---

## 7. Recommended next steps

1. **Fix the §6-A gaps** — these are the only places mcc is *wrong* vs the
   gcc==clang consensus; each is small and well-scoped. Priority: #1 (`inline`
   linkage) has the widest correctness impact (silent link where the standard
   requires failure); #2 (K&R implicit-int) and #3 (`va_start`) are diagnostic-only.
2. **Add a negative/diagnostic test tier.** mcc's suites are execute-first; port a
   focused slice of gcc's `-pedantic-errors` negatives (implicit-int, VLA-jump,
   FAM misuse, alignas constraints) into a `tests/diff/parts` or a new
   `tests/reject/` golden-stderr suite.
3. **Close the B-list coverage gaps** in `tests/exec/features_c99_c11/` — FAM,
   `_Noreturn`, `_Alignas`, and complex-diagnostic cases are the highest-value,
   lowest-effort additions.
4. **Keep `C9911.md` as the ledger** — it already tracks all 3,838 rows 3-way; this
   file is its narrative summary. Regenerate both when the gcc/clang clones advance.
