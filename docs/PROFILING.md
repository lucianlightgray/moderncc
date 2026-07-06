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
- **The spread (§4):** each workload is compiled by nine builds — four reference
  compilers `gcc-debug` (`-O0 -g`), `gcc-release` (`-O2 -g`), `clang-debug`,
  `clang-release`, and five builds of `mcc` itself, **all `-O0 -g` debug builds**
  (§1): `mcc-gcc` (gcc-built), `mcc-clang` (clang-built), self-hosted **`mcc-self`**
  (mcc-built, the baseline), **`mcc-musl`** (self-hosted, musl-linked), and
  **`mcc-static`** (self-hosted, fully-static musl). The `mcc` builds are `-O0`, so
  they measure a *debug* compiler; a release `mcc` runs ~3–4× faster still.
- **Host of record:** Linux x86-64 (Gentoo, kernel 6.18), 32 cores, gcc 15.3.0,
  clang 22.1.8. Measurements pinned to core 2 with ASLR off, turbo off, and the
  `performance` governor (§3a); `perf_event_paranoid` lowered to 1 for §5.
- **Reference date:** 2026-07-06.

---

## 1. The builds: five to compare, all to attribute

Each `mcc` in the spread is a **plain `Debug` (`-O0 -g`) build with sanitizers
off** (`MCC_BUILD_SANITIZE=OFF`; the default, and never on the primary `mcc`
target — it only ever builds a separate `mcc_s`), unstripped, no bcheck/backtrace.
`-O0 -g` symbolizes cleanly for `perf` (no inlining). `mcc` ignores `-O`, so
`mcc-self` codegen is unaffected by the build type; only the gcc/clang-built rows
change (their `mcc` binary is now `-O0`, hence slower than the old `-O2` build).

| Build dir             | Compiler of `mcc`          | Row(s) in §4                       |
|-----------------------|----------------------------|------------------------------------|
| `cmake-prof-gcc`      | `gcc -O0 -g`               | `mcc-gcc`; also the `perf` build (§5); glibc `mcc-static` via gcc |
| `cmake-prof-clang`    | `clang -O0 -g`             | `mcc-clang`                        |
| `cmake-prof-mcc`      | `mcc` (bootstrapped by `cmake-prof-gcc/mcc`) | **`mcc-self`** — baseline (glibc) |
| `cmake-prof-mcc-musl` | `mcc-musl` (musl-targeting) | `mcc-musl`, `mcc-static` (musl)   |

```sh
# gcc- and clang-built mcc (mcc-gcc, mcc-clang) — plain -O0, sanitizers off
for cc in gcc clang; do
  cmake -S . -B cmake-prof-$cc -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=$cc \
    -DMCC_BUILD_SANITIZE=OFF -DMCC_BUILD_STRIP=OFF \
    -DMCC_CONFIG_BCHECK=OFF -DMCC_CONFIG_BACKTRACE=OFF \
    -DMCC_BUILD_MUSL=OFF -DMCC_SINGLE_SOURCE=ON
  cmake --build cmake-prof-$cc --target mcc mccrt -j"$(nproc)"
done

# self-hosted mcc (mcc-self) — bootstrapped by the gcc-built mcc
cmake -S . -B cmake-prof-mcc -G Ninja \
  -DCMAKE_C_COMPILER="$PWD/cmake-prof-gcc/mcc" -DMCC_TOOLCHAIN_PROFILE=mcc \
  -DCMAKE_BUILD_TYPE=Debug -DMCC_BUILD_SANITIZE=OFF -DMCC_BUILD_STRIP=OFF \
  -DMCC_CONFIG_BCHECK=OFF -DMCC_CONFIG_BACKTRACE=OFF \
  -DMCC_BUILD_MUSL=OFF -DMCC_SINGLE_SOURCE=ON
cmake --build cmake-prof-mcc --target mcc mccrt -j"$(nproc)"
```

**The musl and static self-host rows** need a musl sysroot (built once from
vendored source) and a musl-targeting `mcc` to bootstrap with:

```sh
# 1. vendored musl sysroot (headers + crt + libc + ld-musl loader), any host
cmake --build cmake-prof-gcc --target vendor-musl musl-sysroot -j"$(nproc)"

# 2. a musl-targeting mcc (its OWN binary is still glibc, but it emits musl) +
#    the fully-static musl mcc, both compiled by the gcc-built bootstrap mcc
cmake -S . -B cmake-prof-mcc-var -G Ninja \
  -DCMAKE_C_COMPILER="$PWD/cmake-prof-gcc/mcc" -DMCC_TOOLCHAIN_PROFILE=mcc \
  -DCMAKE_BUILD_TYPE=Debug -DMCC_BUILD_SANITIZE=OFF -DMCC_SINGLE_SOURCE=ON \
  -DMCC_CONFIG_BCHECK=OFF -DMCC_CONFIG_BACKTRACE=OFF \
  -DMCC_BUILD_MUSL=ON -DMCC_BUILD_STATIC_EXE=ON
cmake --build cmake-prof-mcc-var --target mcc_musl mccrt -j"$(nproc)"

# 3. genuine musl-LINKED self-host mcc (mcc-musl) + its fully-static twin
#    (mcc-static), both compiled AND linked by the musl-targeting mcc above
cmake -S . -B cmake-prof-mcc-musl -G Ninja \
  -DCMAKE_C_COMPILER="$PWD/cmake-prof-mcc-var/mcc-musl" -DMCC_TOOLCHAIN_PROFILE=mcc \
  -DCMAKE_BUILD_TYPE=Debug -DMCC_BUILD_SANITIZE=OFF -DMCC_SINGLE_SOURCE=ON \
  -DMCC_CONFIG_BCHECK=OFF -DMCC_CONFIG_BACKTRACE=OFF -DMCC_BUILD_STATIC_EXE=ON
cmake --build cmake-prof-mcc-musl --target mcc mcc_static mccrt -j"$(nproc)"
# cmake-prof-mcc-musl/mcc        -> mcc-musl   (interp = vendored ld-musl)
# cmake-prof-mcc-musl/mcc-static -> mcc-static (fully static, no interpreter)
```

> **Codegen is identical across all `mcc` rows.** `mcc`'s backend is deterministic
> and host-compiler-independent: the `-c` object of the amalgamation is
> byte-identical whether emitted by `mcc-gcc`, `mcc-clang`, or `mcc-self` (the
> musl rows differ only where musl vs glibc *headers* change preprocessing). So the
> builds differ only in *how fast the `mcc` binary runs*, never in what it emits.

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
so a busy 32-way host does not skew the samples. It exports JSON for
post-processing.

> **Do not use `-N`/`--shell=none` for the amalgamation.** The `src/mcc.c` `$ARGS`
> from §2b carry shell-escaped string defines (`-DCONFIG_MCCDIR=\"…\"`); `-N` splits
> the command itself without a shell and passes the backslash-quotes literally, so
> some compiles error and hyperfine drops those rows. Run through the default shell
> (drop `-N`) — the ~1 ms shell-spawn overhead is constant across configs and
> negligible even for the `-E` phase. `-N` is fine for the `full_language.c`
> commands, whose flags are un-escaped. If a row still drops, measure it alone
> (a one-line wrapper script that `exec`s the command, timed with `-N`).

```sh
HF="taskset -c 2 hyperfine --warmup 10 --min-runs 40 --style none"

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

## Results & findings (§4–§8)

The measured numbers and analysis produced by the method above — the seven-build
spread (§4), the `perf` self%/annotate attribution (§5), the applied lexer
optimization and what doesn't fit (§6), the validation matrix (§7), and the
conclusions (§8) — live in
[docs/NOTES.md § Profiling](NOTES.md#profiling--measured-results--lexer-optimization-findings).
Section numbers and cross-references there are preserved from this file.
