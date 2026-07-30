# PLAN: exhaustive type × value conformance suite (`tests/valmat`)

Status: proposal. Every decision in §10 is open; the table gives a recommendation per
row. Nothing below is implemented.

## 1. Goal, and what "100%" can mean

The ask: iterate the full range of values for every permutation × combination of types,
with bit-level coverage of every 0 and 1, across qualifiers, storage classes, function
linkages, and struct/union pairs and triples — 100% value coverage at CPU level.

Literal exhaustion is bounded by arithmetic, not by effort:

| Domain | Size | Verdict |
|---|---:|---|
| any 8-bit type, unary | 256 | exhaustive, free |
| 8-bit × 8-bit, binary | 65,536 | exhaustive, free |
| any 16-bit type, unary | 65,536 | exhaustive, free |
| 16-bit × 16-bit, binary | 4.29e9 | exhaustive, seconds–minutes per operator |
| any 32-bit type, unary (incl. every `float` bit pattern) | 4.29e9 | exhaustive, seconds–minutes per operator |
| 32-bit × 32-bit, binary | 1.84e19 | **unreachable** — ~200 years at 3e9 checks/s |
| any 64-bit type, unary | 1.84e19 | **unreachable** |
| 64-bit × 64-bit, 128-bit anything | 3.4e38 | **unreachable** |

So the suite is defined in two regimes, and the claim it can honestly make is:

- **Exhaustive regime** — every representable value of the domain is applied and checked.
  Covers all unary operations on ≤32-bit types and all binary operations on ≤16-bit types.
- **Lattice regime** — every *bit position* of the type is independently exercised as 0 and
  as 1, in both operands, plus every boundary and sign-transition value. This is the
  "bit-level tests for every 0 and 1" requirement, and it is complete in that sense even
  though the value cross-product is not.

Anything claiming more than that would be false-green, which rule 9 of `docs/TODO.md`
forbids. The suite prints which regime each cell ran in.

## 2. Constraints discovered (these shape the design)

From the harness, verified against the tree:

1. **Cost multiplier.** `tests/exec/goldens.h` has 313 rows; `CMakeLists.txt:3600-3980`
   replays every row under 22 optimizer/gate configurations, and `CMakeLists.txt:4302-4311`
   auto-registers 265 of them into `diff3/`. One new golden row costs ~23 ctest cells.
   Current total is 7,893 ctest names, ≈2,215 s serial. A few thousand new goldens would
   be ~70,000 cells. **The suite must not be one-file-per-case under `tests/exec/`.**
2. **Three glob sweeps silently absorb any new `.c` under `tests/exec/`:**
   `sanitize-selfcheck` (`CMakeLists.txt:3189`, every file × `-O0..-O3` under ASan/UBSan,
   `TIMEOUT 600`), `ast/treecheck` (`:3680`, × `-O2 -O3 -Os`, `TIMEOUT 900`), and
   `ast-verify-ratchet` (`:3655`, globs at *test* time and diffs against
   `tests/ast/verify-baseline/*.txt` — a new file with a recorder gap fails it until
   regenerated with `-DREGEN=1`).
3. **No `CONFIGURE_DEPENDS` anywhere.** New goldens need a CMake reconfigure to appear.
4. **`goldens.h` is compiled into two runners as string literals.** Long expectations hit
   MSVC's 65,535-byte literal cap and slow both TUs. Short `OK\n` / `chk=<u64>\n` is the
   shape already used by `tests/exec/optimizer/*`.
5. **`tests/fuzz/corpus` is the precedent to copy** (`CMakeLists.txt:4352-4357`): one ctest
   that takes `--corpus <dir> --replay` and enumerates the directory *at runtime* — no
   reconfigure, no ctest explosion, arbitrary corpus size.
6. **Free oracle, with a catch.** Any `run`/`run2` golden becomes a gcc+clang majority
   differential via `tests/diff3/runner.c`, comparing **stdout only**, requiring a strict
   majority (`runner.c:245-260`). Implementation-defined output silently passes as
   "no majority", so it is not a real check; genuinely divergent output must go on the
   `intentional_divergence()` allowlist (`runner.c:172-186`).
7. **Every test is auto-labeled `native` unless it declares `qemu`**
   (`CMakeLists.txt:6104-6111`). A new label alone does *not* make a suite opt-in; it needs
   either the `qemu`-style exclusion at `:6107` plus a `_test-<x>` preset filter
   (`CMakePresets.json:383-400`), or an `option(... OFF)` gate like `MCC_QEMU_TESTS`
   (`:5922`). `tools/ci.c` has no `-L` plumbing today — only `-E` and `-I` sharding.
8. **Sharding already exists**: `MCC_CI_CTEST_SHARD`/`NSHARDS` → `ctest -I s,,n`
   (`tools/ci.c:239-248`). CI job cap is `timeout-minutes: 20` (`.github/workflows/ci.yml:44`).
9. **`tests/fuzz/gen.h` is reusable** — header-only, dependency-free, deterministic
   splitmix64 generator with `fuzz_emit(seed, FILE*)`. But its value space is deliberately
   incapable of finding any of this: `unsigned long` only, no floats, divisors forced
   non-zero, shifts pre-masked, constants capped at `0xffffffff`, one struct shape
   (`tests/fuzz/NOTES.md`). It cannot be extended into this suite; it is a sibling.

From the type system (`src/mcc.h:1040-1138`, `src/mccgen.c`):

10. **`__int128` is x86_64-ELF/Mach-O only** — `mcc.h:1072-1076` gates `MCC_HAVE_INT128`;
    i386/arm/arm64/riscv64 and *all* PE targets hard-error `'__int128' is not supported on
    this target`. Int↔float conversion on it is rejected outright (`mccgen.c:4059`).
11. **`_BitInt`, `_Float16`, `__float128`, `_Decimal*` do not exist in mcc.** Zero tokens.
    `_Imaginary` parses and then hard-errors (`mccgen.c:6252`). These are out of scope, not
    gaps to fill.
12. **`long double` has four distinct target formats**: x87-80 in 16 bytes (x86_64 SysV),
    x87-80 in 12 bytes (i386), IEEE binary128 via soft-float libcalls (arm64, riscv64), and
    plain `double` (arm, every PE target, arm64-Mach-O — `MCC_USING_DOUBLE_FOR_LDOUBLE`).
    Bit-level `long double` tests are inherently per-target.
13. **`long` is 8 bytes except on i386, arm, and every PE target** (`mcc.h:172-176`), where
    it is 4. **`char` is unsigned by default on arm, arm64-ELF and riscv64**, signed on
    x86_64/i386 — and runtime-overridable via `-funsigned-char`/`-fsigned-char`.
    `double`/`long long` align to 4, not 8, on i386-non-PE and arm-non-EABI.
14. **Bitfields accept only `_Bool`/`char`/`short`/`int`/`long long`/enum**
    (`mccgen.c:5548-5553`); pointers, floats and `__int128` are rejected. Max width is 1 for
    `_Bool`, else the type's bit width; `BIT_POS`/`BIT_SIZE` are 6 bits each, so width 64 is
    a documented special case (`mccgen.c:5561`).
15. **Enums can be unsigned and 64-bit** (inferred, `mccgen.c:5427-5436`), support a C23
    fixed underlying type (`:5335-5343`), and shrink under `-fshort-enums` (`:5438-5450`).
16. **The AST recorder refuses `long double`, `__int128`, `_Complex` and bitfields**
    (`ast_bad_type`, `src/mccast.c:2473`). Those cells take the non-recorded path at `-O1+`,
    so the optimizer fan-out buys nothing for them — worth knowing before paying for it.

From existing coverage — the gaps are real and large:

17. **No `<limits.h>`-driven arithmetic exists anywhere in `tests/`.** All four files that
    include it assert the *value* of the macro or its `_Generic` type. Nothing computes
    `INT_MAX + 1`, `INT_MIN / -1`, `-INT_MIN`, `INT_MIN % -1`, `LLONG_MIN >> 1`, or
    `abs(INT_MIN)`.
18. **`tests/exec/types/floating_point.c` runs the entire float/double/long double operator
    matrix on exactly two values, 12.34 and 56.78.** No `FLT_MAX`, `DBL_MIN`, `-0.0`,
    subnormal, `INFINITY`, `NAN`, or overflow-to-inf.
19. **`tests/behavior/floating_point.c` is a 464-line bit-exact long-double engine that
    never runs.** It is registered only via the `_orphan_srcs` glob as `compile.floating_point`
    (`CMakeLists.txt:5649-5670`), an `mcc -c` compile-only test, and its `main` body is
    `#ifdef __aarch64__`. x87 80-bit `long double` has no bit-exact runtime test at all.
    **Reviving this file is the single highest-value cheap win in the plan.**
20. `__int128` coverage is one file (`tests/exec/types/int128.c`); no `__int128` struct
    member, by-value parameter, vararg, or `INT128_MIN` division.
21. Struct ABI is ~15 hand-picked shapes; the systematic size × class sweep exists only in
    `tests/exec/arch/arm64.c`, which is `cpu=arm64`-gated and SKIPs everywhere else.
22. `restrict` has no semantic test; `volatile` has no access-count or ordering test.

## 3. Architecture

Four artifacts, none of them under `tests/exec/`:

```
tests/valmat/
  gen.c          host tool: emits kernels + expectation tables    (build target valmat_gen)
  model.h        per-target type model (widths, signedness, ld format, availability)
  runner.c       driver: enumerate, build, run, diff, shard        (build target valmat_runner)
  kernels/       generated .c, checked in, regenerated by gen.c
  README.md
  NOTES.md       negative results, per rule 10
```

**Why outside `tests/exec/`:** constraints 1 and 2. Living elsewhere sidesteps the 22×
optimizer replay, the three glob sweeps, and the `goldens.h` string-literal bloat, and lets
the suite pick its own multipliers explicitly rather than inheriting them.

**Runner model** — copied from `tests/fuzz/corpus` (constraint 5): enumerate `kernels/` at
runtime, so adding kernels never needs a reconfigure and never adds a ctest name. Registered
as a fixed, small number of shards (§9), each `SKIP_RETURN_CODE 77`.

**Kernel shape** — self-checking, silent on success:

```
run each case; on mismatch print exactly one line:
  FAIL <case-id> <op> <type> lhs=<hex> rhs=<hex> got=<hex> want=<hex>
at end print:
  <tier>: <n> cases, <n> checks, mode=<exhaustive|lattice>, chk=<fnv64>
```

Constant output size regardless of case count (constraint 4), a checksum that is comparable
across compilers, and a first-failure record precise enough to reduce by hand. Values print
as raw hex of the object representation (via a `union` pun), never as decimal or `%f` —
`%f` formatting differences across libc would produce oracle noise.

**Generation, not hand-authoring.** `gen.c` is a host tool linked against the target model
in `model.h`; it computes every expected value at generation time using exact 128-bit host
arithmetic and emits it as a literal. Kernels are checked in so a plain `git clone` builds
and tests without running the generator, and so a regeneration diff is reviewable.

## 4. Type roster

Two distinct axes, deliberately separated — conflating them is what makes naive matrices
explode.

**Representation classes (14)** — drive *value* testing. Deduped by (width, signedness,
format), because `char` and `signed char` cannot differ in arithmetic:

`u1`(`_Bool`) · `s8` · `u8` · `s16` · `u16` · `s32` · `u32` · `s64` · `u64` · `s128`\* ·
`u128`\* · `f32` · `f64` · `f80`\*\*

\* x86_64-ELF/Mach-O only (constraint 10). \*\* format is per-target (constraint 12).

**Spellings (19+)** — drive *type-identity*, `_Generic`, `sizeof`/`_Alignof` and ABI
testing, where `char` ≠ `signed char` matters:

`_Bool` · `char` · `signed char` · `unsigned char` · `short` · `unsigned short` · `int` ·
`unsigned` · `long` · `unsigned long` · `long long` · `unsigned long long` · `float` ·
`double` · `long double` · `__int128` · `unsigned __int128` · `void *` · `enum` (4 flavors:
inferred-int, inferred-unsigned, inferred-64-bit, C23 fixed-underlying).

**Qualifier / storage axis** — applied as a cross-cut, not a full cross-product (§6, T5):
`plain` · `const` · `volatile` · `const volatile` · `_Atomic` × `auto` · `static` ·
`extern` · `register` · `_Thread_local`.

**Function linkage axis:** `plain` · `static` · `inline` · `static inline` ·
`extern inline` — five, matching the existing matrix in
`tests/exec/functions_abi/inline.c`, which already covers linkage *emission*; this suite
covers value fidelity *through* each linkage.

## 5. The value lattice

For an integer class of width `W`, the lattice is (≈`4W + 14` values; 268 at W=64, 46 at W=8):

- boundaries: `0`, `1`, `-1`, `2`, `-2`, `MIN`, `MIN+1`, `MAX`, `MAX-1`, `MIN/2`, `MAX/2`
- walking one: `1<<i` for `i` in `0..W-1` — **every bit set alone**
- walking zero: `~(1<<i)` — **every bit clear alone**
- low masks: `(1<<i)-1`, and `-(1<<i)`
- patterns: `0x55…`, `0xAA…`, `0x0F…`, `0xF0…`
- per-byte sign probes: `0x7F`/`0x80`/`0xFF` positioned at each byte offset

Walking-one and walking-zero together are the literal "every 0 and every 1" requirement:
each bit position is independently observed set and clear.

For floats, the lattice is over *bit patterns*, not decimal values: `±0`, `±1`, `±2`,
smallest/largest subnormal, smallest/largest normal, `±INF`, quiet NaN, signaling NaN, NaN
with a payload, the round-to-nearest-even tie values, the `2^24`/`2^53` integer-precision
cliffs, and one walking-one sweep across the mantissa and exponent fields. `f80` adds the
explicit-integer-bit patterns and the pseudo-denormal/unnormal encodings that only x87 has —
these are exactly what `tests/behavior/floating_point.c` already knows how to build via its
`make(sgn, exp, hi, lo)` helper (constraint 19).

## 6. Tiers

| Tier | Scope | Regime | Est. checks | Est. kernels |
|---|---|---|---:|---:|
| **T0** | Type identity: `sizeof`, `_Alignof`, `offsetof`, `_Generic` dispatch, signedness of plain `char`, rank ordering, `<limits.h>`/`<float.h>` self-consistency, per-target availability | n/a | ~2,000 | 1 |
| **T1** | Unary per class: `+ - ~ ! ++ -- (cast to each class)` over the full lattice | exhaustive ≤32-bit, lattice ≥64-bit | ~180,000 | 14 |
| **T2** | Binary same-class: `+ - * / % << >> & \| ^ == != < <= > >=` over lattice × lattice | exhaustive ≤16-bit, lattice ≥32-bit | ~1.1M | 14 |
| **T3** | **Conversions**, all 14×14 ordered class pairs over the source lattice — historically the densest bug area, and the one `tests/exec/types/int_conversion.c` only samples | exhaustive ≤16-bit source | ~400,000 | 14 |
| **T4** | Aggregates: ordered **pairs** (196) and **triples** (2,744) of classes as `struct` and `union` — layout (`sizeof`/`_Alignof`/`offsetof`/tail padding), member store→load roundtrip at lattice values, by-value argument, by-value return, vararg pass | lattice, reduced (≈12 values/member) | ~350,000 | ~30 |
| **T5** | Qualifiers × storage × function linkage: value fidelity through each of the 5 linkages × 5 qualifier states × 5 storage classes, plus `volatile` access-*counting* (gap 22) and `_Atomic` RMW at boundary values | lattice, reduced | ~60,000 | 6 |
| **T6** | Bitfields: every legal base type × every width `1..W` × signed/unsigned × PCC and MS layout, value roundtrip at each width's own MIN/MAX | exhaustive over widths | ~40,000 | 4 |

Aggregate triples at 2,744 are the largest single generated artifact; at ~40 lines of C each
that is ~110k lines in one kernel, which is why T4 splits across ~30 kernels.

## 7. Oracles

No single oracle is sufficient; three are used, and they are independent.

1. **Generator-computed expectations (primary).** `gen.c` computes each expected result in
   exact 128-bit host arithmetic under an explicit per-target model and bakes it in as a
   literal. Catches everything, works cross-arch under qemu with no cross toolchain, and is
   the only oracle that works when no reference compiler exists for the target. Its risk is
   that a bug in `model.h` is invisible — mitigated by oracle 2.
2. **gcc/clang differential (independent check on oracle 1).** The same kernel compiled by
   gcc and clang must produce the same `chk=` checksum. Uses the reference-discovery already
   in `CMakeLists.txt:4229-4289` (`MCC_DIFF3_GCC`/`MCC_DIFF3_CLANG`), and majority-consensus
   logic patterned on `tests/diff3/runner.c:245-260`. Requires the UB policy of §8 to hold,
   or the references legitimately disagree.
3. **Self-consistency identities (free, target-independent).** `(T)(x + y) - y == x` under
   wraparound, `(T)~x == -x - 1`, `x / y * y + x % y == x`, `(u)(s) round-trips`,
   `f == f` false iff NaN, monotonicity of comparison. These hold with no external
   reference and are the only oracle available on a target with no gcc and no qemu.

Checksums, not full output, are compared across compilers — a full transcript of 1.1M
checks would be ~80 MB per leg.

## 8. UB policy

This is a correctness precondition, not a detail. `has_ub()` in `tests/fuzz/runner.c:224-246`
exists precisely because a UB-containing case makes the differential oracle unsound —
gcc and clang are each free to do anything, so a "divergence" proves nothing.

The lattice deliberately walks straight into UB: `INT_MIN / -1` traps on x86, `INT_MAX + 1`
is UB, `1 << 31` on `int` is UB, shift-by-width is UB, `(int)3e9` is UB, `(int)NAN` is UB.
These are also exactly the cases gap 17 says are untested. Three partitions:

- **Defined** — unsigned wraparound, all bitwise ops, in-range conversions, shifts below
  width, float ops with defined results. Full oracle stack applies.
- **UB, testable as mcc's documented choice** — signed overflow, `INT_MIN / -1`,
  out-of-range float→int. Run under `-fwrapv`-equivalent semantics where mcc defines them;
  compare mcc against *itself* across `-O` levels and against the generator model, but
  **exclude from the gcc/clang differential** and mark the cell `mode=ub-defined`. Any
  case in this partition that is *not* stable across mcc's own `-O0..-O3` is a real finding.
- **UB, not testable** — anything that may trap. Compiled and linked, not executed, unless
  the trap is the assertion (`INT_MIN / -1` under `-fsanitize=undefined` trap mode).

Every kernel declares its partition in a header line; the runner refuses to send a
`ub-*` kernel to the differential leg.

## 9. Budget and CI integration

Est. runtime, to be measured not trusted (rule 18):

| Tier set | Checks | Est. wall (`-O0`, 1 core) |
|---|---:|---|
| T0–T1 lattice, T2 lattice, T3 ≤8-bit exhaustive, T4 pairs, T5, T6 | ~600k | 10–25 s |
| \+ T2/T3 16-bit exhaustive, T4 triples | ~9e9 | 20–60 min |
| \+ T1 32-bit exhaustive (incl. every `float` bit pattern) | ~1e11 | hours |

Three registration tiers accordingly:

- **`valmat/*` — default-on, `native` label, ~8 shards, target <30 s total.** Fits inside
  the existing 20-minute CI cap (`ci.yml:44`) and auto-runs in every linux/macos/windows job
  with no YAML edit (constraint 7).
- **`valmat-soak/*` — `option(MCC_VALMAT_SOAK ... OFF)`**, the 16/32-bit exhaustive sweeps.
  Opt-in via the `MCC_QEMU_TESTS` pattern (`CMakeLists.txt:5922`), because a new label alone
  would still be auto-labeled `native` and run everywhere (constraint 7).
- **`valmat` under qemu — `LABELS qemu`** inside the existing `if(MCC_QEMU_TESTS)` block,
  reusing the `MCC_TEST_RUNEMU`/`MCC_TEST_SYSROOT` env hook (`CMakeLists.txt:6047`). This is
  where i386 (`long`=4, `long double`=12 bytes, `double` aligned 4) and arm (unsigned `char`,
  `long double`=`double`) earn their keep — rule 27 names these as the two targets that
  actually catch bugs.

Sharding uses the existing `MCC_CI_CTEST_SHARD`/`NSHARDS` path (`tools/ci.c:239-248`); no new
plumbing. A `junit-assert --expect valmat/=1` guard (mirroring `ci.yml:266-268`) prevents the
suite silently no-op'ing.

## 10. Decisions

Recommendation in bold. Rows are independent unless noted.

| # | Decision | Options | Rec. | Consequence |
|---|---|---|---|---|
| **A** | **Coverage regime** | | | |
| A1 | What "100% value coverage" commits to | a) lattice only, everywhere · b) **exhaustive where finite-in-practice, lattice elsewhere** · c) exhaustive only, drop ≥32-bit types | **b** | (a) is cheap but overclaims nothing; (b) is the honest maximum; (c) leaves `long long` untested |
| A1a | Exhaustive cutoff for binary ops | a) 8-bit · b) **16-bit** · c) 16-bit in CI, 32-bit in soak | **b** | 16-bit binary = 4.29e9 pairs/op — minutes; 32-bit binary is 200 years |
| A1b | Exhaustive cutoff for unary ops | a) 16-bit · b) **32-bit (soak only)** · c) 32-bit in CI | **b** | 32-bit unary sweeps every `float` bit pattern — high value, too slow for the 20-min cap |
| A1c | Lattice density (§5) | a) boundaries only (~14) · b) **boundaries + walking-1 + walking-0 + patterns (~4W+14)** · c) (b) + all byte permutations | **b** | (b) is the literal reading of "every 0 and 1"; (c) multiplies cost with no new bit coverage |
| **B** | **Placement and registration** | | | |
| B1 | Where the suite lives | a) `tests/exec/` as goldens · b) **new `tests/valmat/`, runtime-enumerated** · c) extend `tests/diff/parts/` | **b** | (a) costs ~23 ctest cells per case and detonates 3 glob sweeps (constraints 1–2); (c) has no value-generation story |
| B1a | ctest granularity | a) one cell per kernel (~80) · b) **fixed ~8 shards** · c) one cell total | **b** | (a) re-explodes the count; (c) gives no parallelism and one useless failure name |
| B1b | Optimizer fan-out | a) all 22 configs · b) **`-O0`, `-O2`, `-Os` + the 4 gates that touch integer width** · c) `-O0` only | **b** | 22× is unaffordable; note constraint 16 — the recorder refuses `long double`/`__int128`/bitfields, so those cells gain nothing from fan-out |
| B1c | Soak opt-in mechanism | a) new ctest label · b) **`option(MCC_VALMAT_SOAK ... OFF)`** · c) separate nightly workflow | **b** | (a) is broken by the auto-`native` epilogue at `CMakeLists.txt:6104-6111`; (c) is additive later |
| **C** | **Generation and authoring** | | | |
| C1 | How kernels are produced | a) hand-written · b) **`gen.c` host tool, output checked in** · c) generated at build time | **b** | (a) cannot reach 2,744 triples; (c) makes a clean clone untestable and hides diffs from review |
| C1a | Case dispatch inside a kernel | a) unrolled constants · b) runtime value tables + loops · c) **both: tables for bulk, unrolled for a curated subset** | **c** | Constants exercise constant-folding; tables exercise the register path. Only (c) covers both, and folding bugs are exactly what `-O2` introduces |
| C1b | Reuse `tests/fuzz/gen.h` | a) extend it · b) **independent generator, shared nothing** | **b** | Its value space is structurally incapable of boundary values (constraint 9); extending it would regress the fuzzer's UB-freedom guarantee |
| **D** | **Oracles** | | | |
| D1 | Primary oracle | a) gcc/clang differential · b) **generator-computed expectations** · c) mcc self-consistency | **b** | Only (b) works cross-arch with no reference toolchain; (a) becomes the independent check on the model |
| D1a | Run the gcc/clang leg? | a) no · b) **yes, on the defined-behavior partition only** | **b** | Free bug-finding on `model.h` itself; §8 makes it sound |
| D1b | Output shape | a) full transcript · b) **checksum + first-failure line** · c) checksum only | **b** | 1.1M checks ≈ 80 MB of transcript; (c) makes a failure unreducible |
| **E** | **UB partition** | | | |
| E1 | Signed overflow / `INT_MIN / -1` | a) exclude entirely · b) **test as mcc's documented behavior, excluded from the differential** · c) include in the differential | **b** | (c) is unsound — gcc and clang may each do anything; (a) forgets gap 17, the single largest hole |
| E1a | Trapping cases | a) skip · b) **compile+link only, execute only under `-fsanitize=undefined` trap mode** | **b** | `INT_MIN / -1` genuinely faults on x86; the trap becomes the assertion instead of a crash |
| **F** | **Type roster scope** | | | |
| F1 | Unsupported types (`_BitInt`, `_Float16`, `__float128`, `_Decimal*`) | a) **out of scope; assert they are rejected** · b) implement then test | **a** | mcc has no tokens for any of them (constraint 11); a rejection test is honest and cheap |
| F1a | `__int128` | a) skip (not portable) · b) **full lattice on x86_64-ELF, `req`-gated off elsewhere** | **b** | Gap 20 — one file today, no struct member, no by-value param, no `INT128_MIN` |
| F1b | `long double` | a) one shared kernel · b) **per-format kernels: x87-80/16, x87-80/12, binary128, `double`-alias** | **b** | Four genuinely different formats (constraint 12); one kernel cannot express them |
| F1c | `_Complex` | a) **in scope, T1–T3** · b) out of scope | **a** | Supported on all 5 targets and already differential-tested; `tests/diff/complex_abi/` is currently orphaned — no CMake reference — so it is nearly free to adopt |
| F1d | Aggregate depth | a) pairs only · b) **pairs + triples** · c) pairs + triples + depth-4 nesting | **b** | The ask says pairs/triples; (c) is 38k aggregates for little new layout signal |
| **G** | **Scope adjacencies (cheap wins found during research)** | | | |
| G1 | Revive `tests/behavior/floating_point.c` | a) leave it · b) **register it as a real run test and drop the `#ifdef __aarch64__`** | **b** | 464 lines of working bit-exact long-double machinery that has never executed on any target (gap 19). Highest value per hour in the plan |
| G1a | `volatile` access-counting test | a) skip · b) **include in T5** | **b** | Gap 22 — no test today asserts `volatile` access *count*, only that it compiles |
| G1b | `restrict` semantics | a) **skip** · b) include | **a** | mcc accepts and ignores it (`mccgen.c:6340`, `storage_class \|= 4`); there is no behavior to assert |
| G1c | Adopt orphaned `tests/diff/complex_abi/` | a) leave · b) **register it** | **b** | Already written, zero CMake references — free coverage |

## 11. Milestones

1. **M0 — skeleton.** `tests/valmat/{model.h,gen.c,runner.c}`, one hand-written T0 kernel,
   ~8 shards registered, green in CI. Proves registration, sharding, skip codes and the
   `77` path without any generated content. Also lands G1/G1c, which need no generator.
2. **M1 — integers.** T1–T3 lattice for the 11 integer classes, all three oracles, UB
   partitioning. This is where gap 17 closes and where the first real bugs are expected.
3. **M2 — floats.** T1–T3 for `f32`/`f64`, then the four `long double` formats. Depends on
   F1b; reuses the `make(sgn,exp,hi,lo)` machinery revived in M0.
4. **M3 — aggregates.** T4 pairs, then triples across ~30 kernels. Watch total build time
   here; this is the tier most likely to need its own shard budget.
5. **M4 — qualifiers, linkage, bitfields.** T5–T6.
6. **M5 — cross-arch.** Register under `LABELS qemu` for i386 and arm first (rule 27), then
   arm64/riscv64. Expect `model.h` bugs to surface here, not mcc bugs — i386's 12-byte
   `long double` and 4-byte `long` are the model's hardest cases.
7. **M6 — soak.** `MCC_VALMAT_SOAK` exhaustive 16-bit binary and 32-bit unary sweeps.

Per rule 1, every milestone lands behind a default-OFF or additive registration whose off
state leaves existing ctest output byte-identical. Per rule 9, any tier blocked on hardware
or a toolchain is skip-marked with its exact blocker, never quietly dropped.
