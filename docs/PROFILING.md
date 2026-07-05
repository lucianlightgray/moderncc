# PROFILING — where `mcc` spends its time, and how to measure it

How to profile the `mcc` front end, what the numbers look like, and where the
lexer (`next` / `next_nomacro`) optimization stands. Everything here is
reproducible from the commands given — no figure is quoted without the command
that produced it. Timings are taken with [`hyperfine`][hf] on a kernel-tuned
quiet system (§3).

[hf]: https://github.com/sharkdp/hyperfine

- **Workloads:** two translation units, timed across a full spread of compilers
  (§4):
  - `tests/diff/full_language.c` — the `mcctest` differential TU (416 lines +
    its `tests/diff/parts/*` and `runtime/include` headers). Small; the
    established lexer-optimization workload (§6). Same source the `mcctest` test
    drives (`mccharness mcctest`, `CMakeLists.txt` ~2783), so the compile flags
    mirror that harness. **`-O0`-only:** its self-referential `__bug_table`
    inline-asm (`tests/diff/parts/legacy_meta.h:364`) miscompiles/traps under any
    optimizer, so it exercises the `-Ox` compilers at `-O0` and `-c` only (§4a).
  - `src/mcc.c` — the single-source `mcc` amalgamation (the whole compiler in a
    single TU, ~100 k preprocessed lines). Large; portable C that every compiler
    builds at `-O0` and `-O2 -g`, so it carries the release columns and is the
    self-host workload (§4b).
- **The spread (§4):** each workload is compiled by seven builds —
  `gcc-debug` (`-O0 -g`), `gcc-release` (`-O2 -g`), `clang-debug`,
  `clang-release`, and three builds of `mcc` itself: `mcc-gcc` (gcc-built),
  `mcc-clang` (clang-built), and self-hosted **`mcc-self`** (mcc-built) as the
  baseline. Three more self-hosted variants — `mcc-static`, `mcc-musl`,
  `mcc-static-musl` — are environment-limited n/a (§4e).
- **Host of record:** Linux x86-64 (Gentoo, kernel 6.18), 32 cores, gcc 15.3.0,
  clang 22.1.8. Measurements pinned to core 2 with ASLR off, turbo off, and the
  `performance` governor (§3a); `perf_event_paranoid` lowered to 1 for §5.
- **Reference date:** 2026-07-04.

---

## 1. The builds: three to compare, all to attribute

Each `mcc` in the spread is a `RelWithDebInfo` (`-O2 -g`, unstripped, no
bcheck/backtrace) build, so `perf` can both time and symbolize it. The only
variable is the compiler that built it:

| Build dir                | Compiler of `mcc`            | Role in §4        |
|--------------------------|------------------------------|-------------------|
| `cmake-prof-gcc`   | `gcc -O2 -g`                 | `mcc-gcc`; also the `perf` build (§5) |
| `cmake-prof-clang` | `clang -O2 -g`               | `mcc-clang`       |
| `cmake-prof-mcc`   | `mcc` itself (bootstrapped by `cmake-prof-gcc/mcc`) | **`mcc-self`** — baseline |

Stripping and `-g` do not change codegen speed, so any of these is a faithful
`perf` stand-in; `cmake-prof-gcc` is used for the attribution in §5.

```sh
# gcc- and clang-built mcc (mcc-gcc, mcc-clang)
for cc in gcc clang; do
  cmake -S . -B cmake-prof-$cc -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=$cc \
    -DMCC_BUILD_STRIP=OFF -DMCC_CONFIG_BCHECK=OFF -DMCC_CONFIG_BACKTRACE=OFF \
    -DMCC_BUILD_MUSL=OFF -DMCC_SINGLE_SOURCE=ON
  cmake --build cmake-prof-$cc --target mcc mccrt -j"$(nproc)"
done

# self-hosted mcc (mcc-self) — bootstrapped by the gcc-built mcc
cmake -S . -B cmake-prof-mcc -G Ninja \
  -DCMAKE_C_COMPILER="$PWD/cmake-prof-gcc/mcc" -DMCC_TOOLCHAIN_PROFILE=mcc \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DMCC_BUILD_STRIP=OFF -DMCC_CONFIG_BCHECK=OFF -DMCC_CONFIG_BACKTRACE=OFF \
  -DMCC_BUILD_MUSL=OFF -DMCC_SINGLE_SOURCE=ON
cmake --build cmake-prof-mcc --target mcc mccrt -j"$(nproc)"
```

> **Codegen is identical across the three.** `mcc`'s backend is deterministic and
> host-compiler-independent: `cmp` on the `-c` object of either workload is
> byte-identical whether emitted by `mcc-gcc`, `mcc-clang`, or `mcc-self`. So the
> three differ only in *how fast the `mcc` binary runs*, never in what it emits —
> which is exactly what makes `mcc-self` a clean bootstrap-cost baseline.

## 2. The compile invocations

### 2a. `full_language.c` (mirror `mccharness mcctest`)

```sh
SRC=tests/diff/full_language.c
DEFS="-DCC_NAME=CC_gcc -DGCC_MAJOR=15"           # clang: -DCC_NAME=CC_clang
R=cmake-prof-gcc
MINC="-B$R -Iruntime/include -I. -I$R"           # mcc include/lib flags
CINC="-Iruntime/include -I. -I$R"                # gcc/clang include flags
CFLAGS="-w -O0 -g -std=gnu11"                    # release variant: -O2 -g

$R/mcc $MINC $DEFS      $SRC -o out.mcc   -lm    # mcc-gcc / -clang / -self swap $R
gcc            $CINC $DEFS   $CFLAGS $SRC -o out.gcc   -lm
clang          $CINC -DCC_NAME=CC_clang -DGCC_MAJOR=15 $CFLAGS $SRC -o out.clang -lm
```

> Note: `full_language.c` selects compiler-specific code via `CC_NAME`; clang
> **must** be given `CC_NAME=CC_clang` or it fails on gcc-only constructs (mcc,
> like the differential harness, compiles the `CC_gcc` path).

### 2b. `src/mcc.c` (the amalgamation)

The amalgamation's `-D`/`-I` set is large and generated; the faithful way to get
it is straight from the self-host build, then swap only the compiler and `-O`:

```sh
# exact flags the self-host build uses for this TU (the -D/-I salad)
BASE=$(ninja -C cmake-prof-mcc -t commands CMakeFiles/mcc.dir/src/mcc.c.o | tail -1)
ARGS=${BASE#*/mcc }                                       # drop the compiler token
ARGS=${ARGS/-o CMakeFiles\/mcc.dir\/src\/mcc.c.o /}       # drop -o <obj>; keeps -c src/mcc.c

gcc   -O2 $ARGS -o /tmp/o.o          # gcc-release  (-O0 for gcc-debug)
clang -O2 $ARGS -o /tmp/o.o          # clang-release
cmake-prof-gcc/mcc  $ARGS -o /tmp/o.o     # mcc-gcc   (mcc ignores -O)
cmake-prof-mcc/mcc  $ARGS -o /tmp/o.o     # mcc-self
# preprocess: replace `-c` with `-E`, `-o /tmp/o.o` with `-o /dev/null`
```

`src/mcc.c` needs no `CC_NAME` guard and no `-lm` — it is a library TU compiled
`-c`, never linked to an exe, so its phases are **preprocess `-E`** and
**compile `-c`** only.

## 3. Timing method — `hyperfine`

`hyperfine` handles warmup, outlier detection, and mean ± σ; pin it to one core
so a busy 32-way host does not skew the samples. Its `-N`/`--shell=none` mode
removes shell-spawn overhead — important for the `-E` phase, which is single-digit
ms. It exports JSON for post-processing.

```sh
HF="taskset -c 2 hyperfine -N --warmup 10 --min-runs 50 --style none"

$HF --export-json prep.json \
  -n mcc-gcc  "cmake-prof-gcc/mcc  $MINC $DEFS -E $SRC -o /dev/null" \
  -n mcc-self "cmake-prof-mcc/mcc  -Bcmake-prof-mcc -Iruntime/include -I. \
                 -Icmake-prof-mcc $DEFS -E $SRC -o /dev/null" \
  -n gcc-debug "gcc $CINC $DEFS -w -O0 -g -std=gnu11 -E $SRC -o /dev/null"      # …etc
```

Up to four phases are measured: **preprocess** (`-E`, the purest lexer/PP signal),
**compile** (`-c`), **compile→exe** (full link), and **run exe** (execute the
produced binary). `preprocess -E` is the metric to watch for lexer changes.
`src/mcc.c` is compile-only, so it uses just `-E` and `-c`.

### 3a. Quiet-system kernel tuning (`sudo`)

`hyperfine`'s own outlier warning is the tell: on a stock system, turbo/boost
clock drift and ASLR jitter widen σ enough to swamp a 1–2 % lexer change. Four
root-only knobs pin the environment; run them once before a measurement session
(all reversible):

```sh
sudo sysctl -w kernel.randomize_va_space=0        # ASLR off: stable RSS + timings
echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost   # turbo off: no clock drift
sudo cpupower frequency-set -g performance        # lock the governor (no ramp-up lag)
sudo sysctl -w kernel.perf_event_paranoid=1       # let §5's perf sample without root
```

Turbo-off trades a little absolute speed for a much tighter σ — every figure in
§4 is taken with all four set, so absolute ms here run slightly higher than a
turbo-on box but are far more reproducible run-to-run. To restore: set
`randomize_va_space=2`, `boost` back to `1`, and `perf_event_paranoid` back to
your distro default (often `2` or `4`).

## 4. The spread — seven builds, two workloads

All figures ms unless noted, mean ± σ over ≥50 runs (≥15 for the multi-second
`-O2` amalgamation), pinned to core 2, quiet-system config (§3a). Lower is
better. Baseline for every ratio is **`mcc-self`**.

### 4a. `full_language.c` (small TU — the `-O0` differential fixture)

| config        | preprocess `-E` |  compile `-c` |  compile→exe |     run exe |
|---------------|----------------:|--------------:|-------------:|------------:|
| gcc-debug     |    34.19 ± 0.63 |   502.4 ± 3.0 |  533.9 ± 8.4 | 19.39 ± 0.11|
| gcc-release   |           n/a ¹ |         n/a ¹ |        n/a ¹ |       n/a ¹ |
| clang-debug   |    40.18 ± 0.82 |  336.6 ± 12.8 |  380.3 ± 3.0 | 21.00 ± 0.21|
| clang-release |    40.90 ± 0.96 |   833.9 ± 2.8 |  881.2 ± 2.4 |      trap ² |
| mcc-gcc       |    14.15 ± 0.11 |  17.40 ± 0.18 | 19.90 ± 0.24 | 21.34 ± 0.14|
| mcc-clang     |    13.64 ± 0.13 |  17.51 ± 0.42 | 19.90 ± 0.25 | 21.38 ± 0.48|
| **mcc-self**  |    21.36 ± 0.45 |  29.00 ± 0.23 | 32.79 ± 0.36 | 21.36 ± 0.16|

Ratio vs `mcc-self`, `compile→exe`: gcc-debug **16.3×**, clang-debug 11.6×,
clang-release 26.9×, mcc-gcc/mcc-clang **0.61×**. `mcc-self` builds the TU end to
end in 33 ms — 16× under gcc's `-O0` and 27× under clang's `-O2`.

> ¹ `gcc -O2` cannot build `full_language.c`. Its `__bug_table` inline-asm
> (`tests/diff/parts/legacy_meta.h:364`) defines a *global* symbol inside
> `get_asm_string()`; at `-O2` gcc inlines that function into every caller and
> emits the symbol once per copy → assembler `symbol 'some_symbol' is already
> defined`. `-fno-inline` clears the assembler error but the resulting binary
> then segfaults — the fixture is genuinely `-O0`-only. Preprocessing is
> optimization-independent, so `gcc-release -E` would simply equal `gcc-debug`'s.
> The optimized-compile spread lives on `src/mcc.c` instead (§4b).
>
> ² `clang -O2` *compiles* the TU (it takes the `#ifndef __clang__` branch and
> never sees the asm), so its `-E`/`-c`/`→exe` numbers are real — but the
> optimized binary aborts at runtime (`SIGABRT`) on the same `-O0`-only
> assumptions. `run exe` is only valid for the `-O0` builds and the (`-O0`-codegen)
> `mcc` binaries, which all land at ~19–21 ms.

### 4b. `src/mcc.c` (large TU — the amalgamation, carries the release columns)

| config          | preprocess `-E` |   compile `-c` | `-c` vs mcc-self |
|-----------------|----------------:|---------------:|-----------------:|
| gcc-debug       |     127.4 ± 1.1 |     2060 ± 6   |           16.5×  |
| gcc-release     |     133.8 ± 2.7 |    14812 ± 48  |          118.3×  |
| clang-debug     |      98.5 ± 0.6 |     1023 ± 18  |            8.2×  |
| clang-release   |     102.4 ± 0.7 |    12546 ± 53  |          100.2×  |
| mcc-gcc         |      62.2 ± 0.5 |   72.55 ± 0.42 |           0.58×  |
| mcc-clang       |      60.1 ± 0.5 |   73.67 ± 0.48 |           0.59×  |
| **mcc-self**    |      95.0 ± 0.9 |  125.21 ± 0.62 |           1.00×  |
| mcc-static      |             —   |        n/a ³   |             —    |
| mcc-musl        |             —   |        n/a ⁴   |             —    |
| mcc-static-musl |             —   |      n/a ³ ⁴   |             —    |

`mcc-gcc` compiles the entire ~100 k-line amalgamation to an object in **73 ms**;
`gcc -O2` needs **14.8 s** for the same TU — a **204×** gap — and `clang -O2`
12.5 s. Even against a plain `-O0` reference, `mcc` is 14–28× faster. This is the
near-`-O0`, integrated-assembler design paying off, and the gap *widens* with TU
size and opt level (cf. §4a's 12–27×).

### 4c. The self-host cost, and compiler-of-mcc parity

Reading down the three `mcc` rows in both tables:

- **`mcc-gcc` ≈ `mcc-clang`.** A gcc `-O2` and a clang `-O2` build of `mcc` run
  within ~2 % of each other (amalgamation `-c`: 72.6 vs 73.7 ms; `-E`: 62 vs
  60 ms). Which optimizing compiler bootstraps `mcc` does not matter.
- **`mcc-self` is the outlier — ~1.7× slower to compile, ~1.5× to preprocess.**
  Self-hosted `mcc` compiles the amalgamation in 125 ms vs 73 ms for the
  `-O2`-built ones (1.7×), and preprocesses in 95 vs 61 ms (1.5×; `full_language.c
  -E`: 21.4 vs 13.9 ms, also 1.5×). The reason is exactly the §4b headline
  turned inward: `mcc`'s own near-`-O0` codegen produces a slower `mcc` binary
  than `gcc -O2`/`clang -O2` do. That 1.7× *is* the measured cost of bootstrapping
  the compiler with itself — and since all three emit byte-identical code (§1),
  it is purely a speed-of-the-driver effect, not a code-quality one.

### 4d. Memory — peak RSS (`/usr/bin/time -v`, compile `-c`)

```sh
taskset -c 2 /usr/bin/time -v <compile -c cmd> 2>&1 | grep 'Maximum resident'
```

| config        | `src/mcc.c` | `full_language.c` |
|---------------|------------:|------------------:|
| gcc-debug     |    149 MB   |      61.7 MB      |
| gcc-release   |    325 MB   |        — ¹        |
| clang-debug   |    140 MB   |     101.7 MB      |
| clang-release |    234 MB   |     119.1 MB      |
| mcc-gcc       |   11.9 MB   |       5.2 MB      |
| mcc-clang     |   12.0 MB   |       5.3 MB      |
| **mcc-self**  |   12.3 MB   |       5.3 MB      |

`mcc`'s footprint is ~**12 MB** on a TU where `gcc -O2` needs **325 MB** (27×) and
`clang -O2` 234 MB — and it is flat across the three `mcc` builds (same allocator,
same codegen), so the self-host slowdown in §4c costs *time*, not *memory*. On the
small TU `mcc` holds ~5.3 MB against gcc/clang's 62–119 MB.

### 4e. The static / musl self-host variants — environment-limited on this host

The four extra self-hosted `mcc` builds cannot be produced on this glibc-only
dev host; the failures are reproducible and are what the `n/a` cells above
record:

- **³ `mcc-static`, `mcc-static-musl` don't self-link.** `mcc`'s internal
  `-static` linker leaves the compiler-runtime and unwinder unresolved —
  `__ehdr_start` (a linker-synthesized symbol `mcc` does not define for a static
  image), plus `__multf3`/`__addtf3`/… (128-bit soft-float) and
  `_Unwind_Resume`/`__gcc_personality_v0`. Supplying `libmccrt.a` and the host
  `libgcc.a` on the link line does not clear them (`__ehdr_start` cannot come
  from an archive). The gcc-driven `linux-gcc-static` preset builds `mcc-static`
  fine — gcc adds those libraries — so this is a gap in `mcc`'s *own* static
  self-link, not a missing object.
- **⁴ `mcc-musl`, `mcc-static-musl` have no musl to target.** There is no
  `/lib/ld-musl-x86_64.so.1` and no musl `libc.{a,so}` on this host. The
  `mcc-musl` target still *builds*, but it silently links the **glibc** loader
  (`readelf -l` → `/lib64/ld-linux-x86-64.so.2`), so it is not a genuine musl
  binary and would only duplicate `mcc-self` — measuring it would be misleading.
  A real musl row needs a musl sysroot (the CI host's `release`/`linux-gcc-musl`
  presets have one; this box does not).

## 5. Where the time goes — `perf` self%

With `perf_event_paranoid=1` (§3a), user-space sampling needs no root. Attribute
against the gcc-built profiling compiler (`cmake-prof-gcc`, symbolized
`-O2 -g`):

```sh
P=cmake-prof-gcc
taskset -c 2 perf record --call-graph=fp -o perf.data -- \
  bash -c "for i in {1..300}; do $P/mcc -B$P \
    -Iruntime/include -I. -I$P $DEFS -c $SRC -o /tmp/pp.o >/dev/null 2>&1; done"
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
taskset -c 2 perf record -o ann.data -- \
  bash -c "for i in {1..400}; do $P/mcc -B$P -Iruntime/include -I. -I$P \
    $DEFS -c $SRC -o /tmp/a.o; done"
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

## 6. Lexer optimization — what's applied, and what doesn't fit

The lexer is already tight enough that per-token dispatch micro-optimizations
land within the ±1% measurement noise on this TU, and `perf annotate` shows why —
they do not touch the two hot instruction clusters in §5a. Of four "less work per
token" changes considered, only idea #1 is applied:

| # | Idea | Effect (preprocess `-E`) | Status |
|---|------|--------------------------|--------|
| 1 | Cache the interned `TokenSym` in `next_nomacro`; read `ts->sym_define` in `next()` instead of re-deriving via `define_find(t)` | neutral, but strictly fewer ops/identifier, no downside | **applied** |
| 2 | SWAR word-at-a-time scanning | whitespace SWAR **slightly negative** (single-space gaps dominate → SWAR preamble is pure overhead on a cold path); identifier SWAR unfit (below) | **not applied** |
| 3 | Fold `hi\|=c` UTF-8 high-bit detect into an `IS_UTF8` char-class bit | neutral; adds one first-char table load (the original has the char free in-register) → marginally *more* work | **not applied** |
| 4 | Hoist `parse_flags & PARSE_FLAG_ASM_FILE` out of the number-scan loop | neutral; the term is already short-circuited off the hot digit path and numbers are cold | **not applied** |

**Why #2's identifier variant does not fit (beyond the measurement):** the
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

The change is validated across the **20 Linux-runnable presets** (each the full
804-test CTest suite; `asm-off` legitimately runs 772), all **100% green**:

```
debug  sanitize  diagnostics  linux-gcc  linux-clang
linux-gcc-multisource  linux-gcc-asm-off  linux-gcc-predefs-off
linux-gcc-pie  linux-gcc-dwarf  linux-gcc-diagnostics
linux-gcc-release  linux-clang-release  linux-gcc-sanitize  linux-gcc-static
release  linux-gcc-musl  cross  linux-gcc-cross  linux-clang-cross
```

The 5 `qemu-*` presets are **excluded**: they fail identically at baseline
because their `*-fetch` steps download target glibc/musl sysroots, which needs
network the host lacks (environmental, not a code failure). macOS / MSVC / mingw
presets do not run on a Linux host.

Per-implementation smoke sets for the affected lexer areas:

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
  number scanning, not the `define_find` lookup. A measurable win has to come
  from those, not from more per-token dispatch micro-tuning (tracked in
  `docs/TODO.md`).
- **The larger input separates signal from noise.** 416 lines is too small to
  separate a 1–2% lexer change from noise. The amalgamation spread (§4b) does
  better: `src/mcc.c` preprocesses in ~62 ms (`mcc-gcc`) with σ ≈ 0.5 ms — a
  ~0.8 % floor vs `full_language.c`'s ~1 %, and 4× the absolute signal — so a
  lexer change shows up there long before it clears noise on the small TU. Lexer
  experiments belong on `-E src/mcc.c`, not `full_language.c` (tracked in
  `docs/TODO.md`).

- **The full spread (§4) reframes "fast" quantitatively.** `mcc` is not merely
  "faster than `-O0`": on the amalgamation it beats `gcc -O2` by **204×** in time
  and **27×** in peak RSS, and the gap grows with TU size and opt level. The one
  place `mcc` pays is bootstrapping itself — `mcc-self` runs ~1.7× slower than a
  `gcc`/`clang`-`-O2`-built `mcc` (§4c) because its own near-`-O0` codegen is the
  compiler it just built. Shrinking that 1.7× is the same problem as making `mcc`
  emit faster code in general — i.e. the codegen backend, a different project from
  the lexer work above.
