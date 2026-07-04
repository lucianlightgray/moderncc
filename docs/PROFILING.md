# PROFILING — where `mcc` spends its time, and how to measure it

How to profile the `mcc` front end, what the numbers look like today, and the
record of an optimization pass on the lexer (`next` / `next_nomacro`). Everything
here is reproducible from the commands given — no figure is quoted without the
command that produced it.

- **Workload:** the `mcctest` translation unit, `tests/diff/full_language.c`
  (416 lines + its `tests/diff/parts/*` and `runtime/include` headers), compiled
  and run by `mcc` vs `gcc -O0` vs `clang -O0`. This is the same source the
  `mcctest` differential test drives (see `mccharness mcctest`,
  `CMakeLists.txt` ~2783), so the compile flags below mirror that harness.
- **Host of record:** Linux x86-64, 32 cores, gcc 15.3.0, clang 22.1.8,
  `perf_event_paranoid=2` (user-space sampling works; no root needed).
- **Reference date:** 2026-07-03.

---

## 1. Two builds: one to time, one to attribute

The release compiler is stripped and frame-pointer-omitting, so `perf` can time
it but cannot symbolize it. Keep two builds:

| Build dir                     | Flags                              | Use                          |
|-------------------------------|------------------------------------|------------------------------|
| `cmake-build-linux-gcc-release` | `-O2`, stripped (`MCC_BUILD_STRIP=ON`) | wall-clock timing + `run exe` (representative) |
| `cmake-build-prof`            | `-O2 -g`, **unstripped**, no bcheck/backtrace | `perf` self% + `perf annotate` |

Stripping and `-g` do not change codegen speed, so the symbolized build is a
faithful stand-in for `perf` attribution.

```sh
# timing build (canonical release)
cmake --build --preset linux-gcc-release -j"$(nproc)"

# symbolized profiling build
cmake -S . -B cmake-build-prof -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=gcc \
  -DMCC_BUILD_STRIP=OFF -DMCC_CONFIG_BCHECK=OFF -DMCC_CONFIG_BACKTRACE=OFF \
  -DMCC_BUILD_MUSL=OFF -DMCC_ONE_SOURCE=ON
cmake --build cmake-build-prof --target mcc -j"$(nproc)"
```

## 2. The compile invocations (mirror `mccharness mcctest`)

```sh
SRC=tests/diff/full_language.c
DEFS="-DCC_NAME=CC_gcc -DGCC_MAJOR=15"           # clang: -DCC_NAME=CC_clang
R=cmake-build-linux-gcc-release
MINC="-B$R -Iruntime/include -I. -I$R"           # mcc include/lib flags
CINC="-Iruntime/include -I. -I$R"                # gcc/clang include flags
CFLAGS="-w -O0 -std=gnu11 -fno-omit-frame-pointer"

$R/mcc $MINC $DEFS      $SRC -o out.mcc   -lm    # mcc
gcc            $CINC $DEFS   $CFLAGS $SRC -o out.gcc   -lm
clang          $CINC -DCC_NAME=CC_clang -DGCC_MAJOR=15 $CFLAGS $SRC -o out.clang -lm
```

> Note: `full_language.c` selects compiler-specific code via `CC_NAME`; clang
> **must** be given `CC_NAME=CC_clang` or it fails on gcc-only constructs (mcc,
> like the differential harness, compiles the `CC_gcc` path).

## 3. Timing method

No `hyperfine` on the host; a pinned loop over N iterations is enough. Pin to one
core, discard stdio, report ms/iter:

```sh
pin() { taskset -c 2 "$@"; }
time_loop() { local n=$1; shift; local t0 t1
  t0=$(date +%s.%N); for ((i=0;i<n;i++)); do "$@" >/dev/null 2>&1; done; t1=$(date +%s.%N)
  awk -v a=$t0 -v b=$t1 -v n=$n 'BEGIN{printf "%.3f ms/iter\n",(b-a)/n*1000}'; }

pin bash -c "$(declare -f time_loop); time_loop 200 $R/mcc $MINC $DEFS -E $SRC -o /dev/null"   # preprocess
pin bash -c "$(declare -f time_loop); time_loop 100 $R/mcc $MINC $DEFS -c $SRC -o /tmp/o.o"    # compile -c
```

Three phases are measured: **preprocess** (`-E`, the purest lexer/PP signal),
**compile** (`-c`), and **run** (execute the produced binary). `preprocess -E` is
the metric to watch for lexer changes.

## 4. Baseline results

`mcc` vs `gcc -O0` vs `clang -O0`, ms/iter (lower is better), pinned to one core:

| workload        |    mcc | gcc -O0 | clang -O0 | mcc/gcc | mcc/clang |
|-----------------|-------:|--------:|----------:|--------:|----------:|
| preprocess `-E` |  6.75  |  15.98  |   19.60   |  0.42×  |   0.34×   |
| compile `-c`    |  8.23  | 217.86  |  124.69   |  0.04×  |   0.07×   |
| compile→exe     |  9.29  | 231.33  |  147.30   |  0.04×  |   0.06×   |
| run exe         | 10.08  |   9.23  |    9.83   |  1.09×  |   1.03×   |

`mcc` preprocesses ~2.4× faster than clang and ~2.4× faster than gcc; it compiles
the TU 15–26× faster (it is a simple, near-`-O0` codegen with an integrated
assembler/linker, so this is expected). Runtime of the produced binary is on par
across compilers (all effectively `-O0`).

## 5. Where the time goes — `perf` self%

```sh
pin perf record --call-graph=fp -o perf.data -- \
  bash -c "for i in {1..300}; do cmake-build-prof/mcc -Bcmake-build-prof \
    -Iruntime/include -I. -Icmake-build-prof $DEFS -c $SRC -o /tmp/pp.o >/dev/null 2>&1; done"
perf report -i perf.data --stdio --no-children | grep -E '\[\.\]|\[k\]' | head -12
```

Top self% during a `-c` loop (symbolized `-O2 -g` build):

```
   13.4%  next_nomacro        <- the lexer core (hottest function)
    5.2%  vsetc
    5.2%  macro_subst_tok
    4.9%  next                <- lexer/PP entry
    4.6%  tok_str_add2
    3.5%  gen_cast
    2.7%  __memmove_avx512…   (libc; identifier memcmp/copy)
    2.6%  preprocess
    2.5%  gfunc_call
    2.4%  macro_subst
```

**The lexer dominates.** `next_nomacro` + `next` alone are ~18% of self-time;
add `preprocess`, `macro_subst*`, and `tok_str_add*` (the token-stream plumbing
the lexer feeds) and the front end is the clear majority.

### 5a. Which instructions inside `next_nomacro` (perf annotate)

```sh
pin perf record -o ann.data -- bash -c "for i in {1..400}; do …mcc -c $SRC -o /tmp/a.o; done"
perf annotate -i ann.data --stdio -s next_nomacro | sort -rn -k1 | head
```

The hot instructions cluster in two places:

- **Hash-bucket chain walk** (`testq %r12,%r12; jne …` ≈ **14.6%** combined) —
  chasing the interning hash chain plus the `memcmp` in `tok_alloc`
  (`hash_ident[]` lookup, mccpp.c ~2942).
- **Identifier char-scan loop** (`orl` = `hi|=c`, plus the `TOK_HASH_FUNC`
  shifts/adds ≈ **13%**) — the per-byte hash + high-bit accumulation at
  mccpp.c ~2930.
- switch dispatch (jump-table `jmpq *%rdx`) ≈ 8%.

The whitespace-skip loop (mccpp.c ~2779) **does not appear** — it is cold on this
TU.

## 6. Optimization pass on `next` / `next_nomacro`

Four "less work per token" ideas were implemented **one at a time**, each
validated across the full preset matrix (§7) and re-profiled. Result: the lexer
is already tight enough that three of four land within the ±1% measurement noise
on this TU, and `perf annotate` shows why — they do not touch the two hot
instruction clusters above.

| # | Idea | Measured effect (preprocess `-E`) | Disposition |
|---|------|-----------------------------------|-------------|
| 1 | Cache the interned `TokenSym` in `next_nomacro`; read `ts->sym_define` in `next()` instead of re-deriving via `define_find(t)` | neutral (6.77 vs 6.75) but strictly fewer ops/identifier, no downside | **Kept** |
| 2 | SWAR word-at-a-time scanning | whitespace SWAR **slightly negative** (single-space gaps dominate → SWAR preamble is pure overhead on a cold path); identifier SWAR declined | **Reverted** |
| 3 | Fold `hi\|=c` UTF-8 high-bit detect into an `IS_UTF8` char-class bit | neutral; adds one first-char table load (original had the char free in-register) → marginally *more* work | **Reverted** |
| 4 | Hoist `parse_flags & PARSE_FLAG_ASM_FILE` out of the number-scan loop | neutral; the term is already short-circuited off the hot digit path and numbers are cold | **Reverted** |

**Why #2's identifier variant was declined (not just measured):** the
id-continue predicate `isidnum_table[c] & (IS_ID|IS_NUM)` covers multiple
disjoint ranges *and* two entries (`$`, `.`) that `set_idnum()` toggles at
runtime (mccpp.c ~4004) — so a hardcoded SWAR predicate would be wrong under
`#pragma`/asm-mode. And because short identifiers dominate, a separate SWAR
boundary pass + separate hash pass is two passes where there is now one → a
likely regression. SWAR is the textbook lever but it does not fit this lexer.

### The kept change (idea #1)

`src/mccpp.c`, +3/−1:

```c
static TokenSym *tok_ts;                     // file-scope

/* in next_nomacro(), where both identifier paths converge: */
tok_ts = ts;
tok = ts->tok;

/* in next(): */
if (t >= TOK_IDENT && (parse_flags & PARSE_FLAG_PREPROCESS)) {
        Sym *s = tok_ts->sym_define;         // was: define_find(t)
```

**Correctness:** every token value `>= TOK_IDENT` (256) is produced by the
identifier path in `next_nomacro`, which always sets `ts` — the special tokens
(`TOK_PPNUM=0xcd`, `TOK_PPSTR=0xce`, `TOK_TWOSHARPS=0xa3`, `TOK_EOF=-1`, …) are
all `< 256`. So whenever `next()` takes the `t >= TOK_IDENT` branch, `tok_ts` is
the freshly-interned symbol and `tok_ts->sym_define` is exactly what
`define_find(t)` (`table_ident[t-TOK_IDENT]->sym_define`, mccpp.c ~1343) would
return — minus the subtract, bounds-check and array load.

## 7. Validation matrix

Every implementation was configured + built + tested across the **20
Linux-runnable presets** before moving on (each is the full 804-test CTest suite;
`asm-off` legitimately runs 772). All 20 stayed **100% green** at baseline, after
idea #1, and at the final state:

```
debug  asan  diagnostics  linux-gcc  linux-clang
linux-gcc-onesource-off  linux-gcc-asm-off  linux-gcc-predefs-off
linux-gcc-pie  linux-gcc-dwarf  linux-gcc-diagnostics
linux-gcc-release  linux-clang-release  linux-gcc-asan  linux-gcc-static
release  linux-gcc-musl  cross  linux-gcc-cross  linux-clang-cross
```

The 5 `qemu-*` presets are **excluded**: they fail identically at baseline
because their `*-fetch` steps download target glibc/musl sysroots, which needs
network the host lacks (environmental, not a code failure). macOS / MSVC / mingw
presets do not run on a Linux host.

Per-implementation smoke sets used between full sweeps:

```sh
ctest --preset debug -R 'mcctest|preprocess|lex|ident|whitespace|comment|string'   # lexer
ctest --preset debug -R 'utf8|ucn|ident|raw_utf8'                                    # idea #3
ctest --preset debug -R 'integer|float|hex|literal|suffix|asm|imaginary|complex'     # idea #4
```

## 8. Conclusions — where a *measurable* win would come from

- The lexer is the right place to look (`next_nomacro` is #1 by a wide margin),
  but the per-token dispatch micro-costs targeted by ideas #1–#4 are already
  below the noise floor on a 416-line TU.
- The two genuinely hot instruction clusters are the **identifier interning hash
  chain walk** and the **per-byte hash computation** — not whitespace, not
  number scanning, not the `define_find` lookup. A real improvement would attack
  those: e.g. hash-table load factor / chain length (`TOK_HASH_SIZE`), or the
  per-char `TOK_HASH_FUNC` cost. Both are riskier than the ideas tried here and
  want a bigger benchmark to see signal.
- **Measure on a larger input.** 416 lines is too small to separate a 1–2% lexer
  change from noise. A preprocessed amalgamation (e.g. the one-source `mcc` TU
  itself, or `full_language.c` concatenated with distinct macro guards) would
  lift the signal above the ±1% floor.
