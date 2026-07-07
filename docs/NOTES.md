# ModernCC — Notes & Detailed Overview

Supplementary reference material moved out of the top-level
[README](../README.md) to keep it focused on getting started. This file
collects the non-essential overview information: toolchain comparisons,
build-status reports, per-toolchain test coverage, and performance benchmarks.

## Comparisons

`Y` = supported, `~` = partially supported, `-` = not supported.

| Target / Format             | mcc | gcc | clang | mingw | msvc |
|-----------------------------|:---:|:----:|:-----:|:-----:|:---:|
| x86_64                      |  Y  |  Y   |   Y   |   Y   |  Y  |
| i386                        |  Y  |  Y   |   Y   |   Y   |  Y  |
| arm                         |  Y  |  Y   |   Y   |   ~   |  Y  |
| arm64                       |  Y  |  Y   |   Y   |   Y   |  Y  |
| riscv64                     |  Y  |  Y   |   Y   |   -   |  -  |
| ELF output                  |  Y  |  Y   |   Y   |   -   |  -  |
| PE/COFF output              |  Y  |  Y   |   Y   |   Y   |  Y  |
| Mach-O output               |  Y  |  ~¹  |   Y   |   -   |  -  |
| Multi-target from one build |  Y  |  Y   |   Y   |   -   |  -  |

¹ `gcc` with Apple patches supports Mach-O

---

| Capability                              | mcc | gcc | clang | mingw | msvc |
|-----------------------------------------|:---:|:---:|:-----:|:-----:|:----:|
| Compile + link in one step              |  Y  |  Y  |   Y   |   Y   |  Y   |
| `-run` (execute in memory, no a.out)    |  Y  |  -  |   -   |   -   |  -   |
| Embeddable compiler library             |  Y  | ~²  |  ~³   |   -   |  -   |
| Integrated assembler (no external `as`) |  Y  |  -  |   Y   |   -   |  Y   |
| Inline asm / `asm goto`                 |  Y  |  Y  |   Y   |   Y   |  ~⁴  |
| Runtime backtraces (`-bt`)              |  Y  |  ~  |   ~   |   ~   |  ~   |
| glibc + musl via `--sysroot`            |  Y  |  Y  |   Y   |   -   |  -   |
| Optimizing codegen                      |  -  |  Y  |   Y   |   Y   |  Y   |
| C99                                     |  Y  |  Y  |   Y   |   Y   |  Y   |
| C11                                     |  Y  |  Y  |   Y   |   ~   |  Y   |
| Single-pass / fast compile              |  Y  |  -  |   -   |   -   |  -   |
| Tiny footprint (~1 MB)                  |  Y  |  -  |   -   |   -   |  -   |

² via `libgccjit`
³ via `libclang`
⁴ MSVC has no inline `asm` on x64 (intrinsics only)

---

| Toolchain  | Notes                                          |
|------------|------------------------------------------------|
| **clang**  |                                                |
| **gcc**    | requires cross-compilers to target multi-archs |
| **mingw**  | same as gcc                                    |
| **MSVC**   | breaks C99/C11 standards; quirky               |
| **mcc**    | self-hosts and cross-compiles                  |

---

| libc                   | Via                                  | Coverage              |
|------------------------|--------------------------------------|-----------------------|
| **glibc** (ELF)        | `--sysroot` / default                | full                  |
| **musl** (ELF)         | `--sysroot` / auto-detected fallback | full                  |
| **msvcrt** (PE)        | Windows/PE target                    | wine + native Windows |
| **libSystem** (Mach-O) | macOS/Darwin target                  | qemu + native MacOS   |

## Platform ABI & runtime notes

**`long double`.** mcc follows each target's platform ABI, matching the native
compiler byte-for-byte:

| Target                              | `long double`            |
|-------------------------------------|--------------------------|
| x86_64 / i386 (ELF)                 | 80-bit x87 extended      |
| arm64 / riscv64 (Linux ELF)         | 128-bit IEEE quad        |
| **arm64 Darwin (Apple)**            | **64-bit — same as `double`** |
| **any PE / Windows target (MSVC ABI)** | **64-bit — same as `double`** |

On Apple arm64 and on all PE targets, `long double` is `double`
(`MCC_USING_DOUBLE_FOR_LDOUBLE`), which is the Apple / MSVC ABI, not an mcc
limitation — `sizeof(long double) == 8` there, agreeing with clang / MSVC.
The `cli/apple_arm64_long_double_is_double` test pins this on macOS arm64.

**`<math.h>` is host-provided.** mcc ships no freestanding `runtime/include/math.h`;
math declarations come from the host/target libc's `<math.h>`, and the math
functions link against the host `libm` (glibc/musl `-lm`, Apple libSystem, or
msvcrt). A non-glibc *freestanding* host with no `<math.h>` must supply one;
mcc does not synthesize the header or the transcendental functions.

**Mach-O thread-local storage.** A `__thread` variable *defined* in the module
compiles to a proper TLV descriptor (`__tlv_bootstrap`) and works. Referencing an
`extern __thread` variable *defined in another module/dylib* is an intentional
limitation — mcc errors "Mach-O: external thread-local '<x>' is unsupported"
rather than emitting a TLV import descriptor. Keep cross-module thread-locals
inside one TU on Darwin, or expose them through an accessor function.

**win32 runtime shims are intentional subsets** (`runtime/win32/include`):
`<pthread.h>` mutexes are non-recursive (`PTHREAD_MUTEX_RECURSIVE` is accepted
but behaves as a plain `SRWLOCK`), thread-key destructors are not run at thread
exit, and there is no cancellation; `<sched.h>` is a minimal shim, not a full
POSIX scheduling interface; `<fenv.h>` has no rounding-control-register access
on non-x86/arm64 PE arches (only the default rounding mode). Callers targeting
win32 should not rely on the missing semantics.

## Build status

**Linux status (2026-07, gcc 15.3 / clang 22):** every Linux preset is green —
`debug`, `release`, `sanitize`, `diagnostics`, `cross`, `matrix` (gcc/clang ×
native/cross superbuild), all 15 `linux-*` CI presets, both `dist-linux-*`
packagings, and the full `qemu` cross×libc matrix. Each test-bearing preset
passes its complete suite (39/39 portable tests; 22/22 in the qemu matrix,
all 5 arches × glibc+musl + `qemu-arm64-osx`). With the `cross` toolchain
built (`MCC_CROSS_DIR`, default `cmake-cross`), the wine PE-conformance
and the four host-runnable Mach-O drivers run natively and pass too.

**Windows status (2026-07-07, mingw gcc 13.1/16.1 / MSVC 19.51 / clang 22):**
every Windows-runnable preset is green. The suite is registered one CTest
per case (the `exec`/`cli`/`diff3`/`parts`/`preprocess` corpora fan out), so the
counts are per-case: `debug`, `release`, `diagnostics`, `cst` and `cross` all
run **812/812** (≈120 environment-gated skips; the exact total tracks upstream
test additions — new platform-gated cases register and self-skip here); `msvc`
(VS generator) runs **810/810** — two fewer only because its test preset also
filters the `wine` and `macho` labels the Ninja presets carry as counted-skips. On the MSVC
host `mcctest` still registers and passes — its gcc reference auto-resolves to
the vendored winlibs GCC (`MCC_REF_CC`) — so the MSVC pass count tracks the
mingw hosts. Those totals assume the vendored clang toolchain is present (`cmake
--build <bld> --target clang-toolchain`, or drop it under `vendor/llvm-clang`;
auto-wired on the next reconfigure); without it the ~260 three-way
`diff3`/`preprocess` cases self-skip. The `cli`/`diff3` suites are now
**self-sufficient for their shell helpers**: on WIN32 the harness prepends both
the resolved `MCC_TEST_SH` directory (which bundles `grep`/`sed`/`printf`) and a
resolved `nm`/`readelf` directory (found on `PATH`, else the vendored winlibs
`bin`) to each case's `PATH` via `ENVIRONMENT_MODIFICATION`, so the suites pass
without the invoker adding a UNIX-tools or binutils directory to `PATH` (git
`sh` + `cmake` alone suffice; on an MSVC host the binutils come from the
vendored winlibs). `mingw` (superbuild; fetches the pinned winlibs GCC and tests
with it) and `matrix` (gcc/clang × native/cross — **4 cells × 812/812**, clang
resolved from the fetched `vendor/llvm-clang`) are green too. Both `dist-*` packagings
build the full artifact matrix — mcc + `-static`/`-dynamic` +
`libmcc-static`/`-dynamic` + all 11 cross compilers; `dist-mingw` additionally
ships each cross compiler in a fully-static (`-static`) shape, while `dist-msvc`
keeps only the host `mcc-static` (`-static` is a GNU-ld flag `link.exe` doesn't
take, so the per-cross static shapes are intentionally skipped under MSVC). The
in-tree build/CI tools carry their own ctests that pass here too —
`host-gate-invariant`, `git-stamp`, `def-verify`, `build-md-nodes`,
`config-defines`, `host-detect`, `cross-factory`, `ci-matrix`, `ci-pkg-smoke`,
`qemu-fetch-parse`. `sanitize`
intentionally fails at configure — mingw ships no libasan/libubsan; use
`diagnostics`, which builds the coverage + profile variants and skips
sanitize. The PE target gets native-only extra coverage
(`pe-native-conformance`, `compile.win32.*`); remaining skips are
environment- or libc-gated with reasons (wine, macOS, X11, ELF-emitting
32-bit reference, the osx/arm64/riscv64 cross drivers when the `cross`
toolchain isn't built, msvcrt's reduced libm/complex surface for
`parts/*`, and PE bounds-checking for `mcctest-bcheck`). The `linux-*`
presets, `dist-linux-*` packagings, and the qemu grid also run from Windows,
via the Docker runners (`tests/ci/docker`, `tests/qemu/docker`).

## Per-toolchain test coverage

`P` = passes, `S` = skipped-with-reason (environment/config-gated, not a
failure), `—` = not applicable.

| `ctest` suite | Win mingw | Win gcc | Win msvc | Lin gcc | Lin clang | mac clang |
|---|:--:|:--:|:--:|:--:|:--:|:--:|
| `exec/*` (golden run/diff)            | P | P | P | P | P | P |
| `mcctest`¹                            | P | P | P | P | P | P |
| `mcctest-bcheck`¹                     | S | S | S | P | P | P |
| `preprocess/*`²                       | P | P | P | P | P | P |
| `diff3/*`²                            | P | P | P | P | P | P |
| `parts/*`² (per-unit 3-way diff)⁹     | S | S | S | P | P | P |
| `cli/*` (readelf/nm structural)³      | P | P | P | P | P | P |
| `libtest` / `-extra` / `-mt`, `abitest-cc` | P | P | P | P | P | P |
| `hello-run` / `hello-exe`, `vla_test-run`  | P | P | P | P | P | P |
| `compile.*` (orphan `-c`)⁴            | P | P | P | P | P | P |
| `asm-c-connect-test`                  | P | P | P | P | P | S |
| `dash-s-roundtrip` (`-S` → asm → run)¹⁰ | P | P | P | P | P | S |
| `asm-gas-directives`⁵                 | S | S | S | S | S | S |
| `i386-fastcall-abi`⁶                  | S | S | S | P | P | S |
| `compile.win32.*` / `pe-native-conformance` | P | P | P | — | — | S |
| `pe-wine-conformance` (label `wine`)⁷ | S | S | S | P | P | S |
| `macho-*` (7 drivers, label `macho`)⁷ | S | S | S | P | P | P |
| qemu cross×libc matrix (label `qemu`)⁸| S | S | S | P | P | S |

¹ Differential vs. a GCC-compatible reference cc (needs the integrated
assembler); MSVC host auto-detects a mingw/winlibs `gcc` (`MCC_REF_CC`).
`-bcheck` variant also needs `MCC_CONFIG_BCHECK`, and skips on the PE/msvcrt
target, where mcc bounds-checking is unsupported (faults in msvcrt
callbacks/library calls).
² Needs **two distinct** references (gcc *and* clang) or the three-way
differential self-skips. On macOS `gcc`/`cc` are the Apple clang shim, so a
genuine Homebrew `gcc-<n>` (installed by `setup-gcc`) is auto-detected. On
Windows no system clang is needed: `cmake --build <bld> --target
clang-toolchain` fetches a pinned, SHA256-verified LLVM into `vendor/llvm-clang/`,
auto-wired by the next reconfigure (both suites then run and pass).
³ Needs POSIX `sh` (`MCC_TEST_SH`) + `nm`/`readelf`; ~31 ELF-image cases
self-skip on a PE target. On WIN32 the harness auto-prepends the `sh` directory
(for its bundled `grep`/`sed`/`printf`) and a resolved `nm`/`readelf` directory
(system `PATH`, else the vendored winlibs `bin`) to each case's `PATH`, so no
UNIX-tools/binutils directory need be on the invoker's `PATH`.
⁴ X11 example (`compile.ex4`) skips when `<X11/Xlib.h>` is absent.
⁵ Always skipped: integrated assembler lacks a few GAS encodings
(`sgdtq`/`sidtq`/`swapgs`).
⁶ Needs the i386 cross compiler (`mcc-i386`, preset `cross`) + an ELF-emitting
32-bit reference cc; skips on Windows (mingw `gcc` emits PE/COFF).
⁷ Both need the cross toolchain (preset `cross`, or a populated
`MCC_CROSS_DIR` — default `cmake-cross`): wine + the win32 cross
compilers for `pe-wine-conformance`; the osx cross compilers +
`llvm-otool`/`otool` for the Mach-O drivers. Linux: four host-runnable drivers
pass (`macho-structural`, `macho-codegen-run`, `macho-image-run`,
`macho-apple-libc`); `macho-conformance-native` and
`macho-libsystem-kernel-fused` skip (need Darwin/darling). macOS:
`macho-structural` + `macho-conformance-native` are native; Linux-approximation
drivers self-skip off x86_64.
⁸ Windows runs it via the Docker runner; a Linux host with `qemu-user` runs it
natively (`ctest -L qemu`).
⁹ Native 3-way per-unit differential of each `tests/diff/parts/run_*.c`
wrapper; needs a shared C99 libc across gcc/clang/mcc, so the PE/msvcrt
target skips (the same units are covered in aggregate by `mcctest`).
¹⁰ Hermetic `mcc -S` → mcc's own assembler → mcc link → run, byte-identical
to the direct build; needs x86_64 + the integrated assembler (skips on arm64
macOS). With the cross compilers built, `dash-s-bytes-{arm64,riscv64}`
additionally assert **object-byte-exact** `-S` roundtrips on the fixed-width
targets (i386/arm are instruction-exact; their assemblers legally re-encode
some widths/branch fields).

## Compile speed & footprint

Compiling `mcc`'s own whole-compiler TU (`src/mcc.c`, `SINGLE_SOURCE=1`) to an
object, best of 3 (gcc 15.3 / clang 22, 2026-07). Stripped release binaries:
dynamic `mcc` ≈ **0.6 MB**, `mcc-static` ≈ **1.3 MB**.

| Compiler | Time | vs mcc |
|---|--:|--:|
| **mcc**       | **0.05 s** | 1× |
| clang `-O0`   | 0.36 s | 7× slower |
| gcc `-O0`     | 0.97 s | 19× slower |
| clang `-O2`   | 5.40 s | 108× slower |
| gcc `-O2`     | 7.03 s | 141× slower |

## Profiling — measured results & lexer-optimization findings

Moved from [PROFILING.md](PROFILING.md), which keeps the reproducible *method*
(§1 the builds, §2 the compile invocations, §3 the timing method). These are the
results and analysis: the seven-build spread, `perf` attribution, the applied
lexer optimization, its validation, and the conclusions. Section numbers (§4–§8)
and cross-references are preserved from `PROFILING.md`. Host of record: Linux
x86-64 (Gentoo, kernel 6.18), 32 cores, gcc 15.3.0, clang 22.1.8; reference date
2026-07-06. Re-measured with every `mcc` built as a plain `-O0 -g` debug build
(sanitizers off) and the static/musl self-host rows added — see §1/§4e.

### 4. The spread — nine builds, two workloads

All figures ms unless noted, mean ± σ over ≥40 runs (≥8 for the multi-second
`-O2` amalgamation), pinned to core 2, quiet-system config (§3a). Lower is
better. Baseline for every ratio is **`mcc-self`**. **Every `mcc` build here is a
plain `-O0 -g` debug build** with sanitizers off (§1) — so the `mcc` rows measure
a *debug* compiler; a release (`-O2`) `mcc` runs ~3–4× faster still, widening
every gap below.

#### 4a. `full_language.c` (small TU — the `-O0` differential fixture)

| config        | preprocess `-E` |  compile `-c` |  compile→exe |     run exe |
|---------------|----------------:|--------------:|-------------:|------------:|
| gcc-debug     |    34.67 ± 0.41 |  500.0 ± 1.7  |  530.2 ± 1.9 | 19.56 ± 0.12|
| gcc-release   |           n/a ¹ |         n/a ¹ |        n/a ¹ |       n/a ¹ |
| clang-debug   |    41.49 ± 3.99 |  331.7 ± 1.3  |  379.7 ± 1.6 | 20.99 ± 0.22|
| mcc-gcc       |    24.08 ± 0.29 |  71.43 ± 0.81 | 75.38 ± 0.46 |           — |
| mcc-clang     |    21.91 ± 0.34 |  92.44 ± 0.42 | 97.53 ± 0.91 |           — |
| **mcc-self**  |    22.14 ± 0.26 |  68.78 ± 0.43 | 72.67 ± 0.23 | 21.47 ± 0.22|

`run exe` is codegen-identical across the three compilers (§1): mcc 21.47, gcc
19.56, clang 20.99. Ratio vs `mcc-self`, `compile→exe`: gcc-debug **7.3×**,
clang-debug **5.2×**. `mcc-self` builds the TU end to end in 73 ms — 7.3× under
gcc's `-O0` and 5.2× under clang's `-O0`. (The old RelWithDebInfo spread put
`mcc-self` at 33 ms / 16×; the mcc binary is now `-O0`-built, hence slower, but
the reference compilers are unchanged so the *shape* holds.)

> ¹ `gcc -O2` cannot build `full_language.c`. Its `__bug_table` inline-asm
> (`tests/diff/parts/legacy_meta.h:364`) defines a *global* symbol inside
> `get_asm_string()`; at `-O2` gcc inlines that function into every caller and
> emits the symbol once per copy → assembler `symbol 'some_symbol' is already
> defined`. The fixture is genuinely `-O0`-only (`clang -O2` compiles it via the
> `#ifndef __clang__` branch but the binary then `SIGABRT`s on the same
> assumptions). Preprocessing is optimization-independent, so a `-release -E`
> would equal `-debug`'s. The optimized-compile spread lives on `src/mcc.c` (§4b).

#### 4b. `src/mcc.c` (large TU — the amalgamation, carries the release columns)

| config          | preprocess `-E` |   compile `-c` | `-c` vs mcc-self |
|-----------------|----------------:|---------------:|-----------------:|
| gcc-debug       |     129.0 ± 1.3 |    2245.0 ± 6.7 |           8.0×  |
| gcc-release     |       = debug ³ |   16058 ± 25   |           57.4×  |
| clang-debug     |     99.0 ± 0.6  |    1108.3 ± 2.5 |           4.0×  |
| clang-release   |       = debug ³ |   13395 ± 118  |           47.9×  |
| mcc-gcc         |     106.7 ± 0.6 |   290.5 ± 7.8  |            1.04× |
| mcc-clang       |      96.5 ± 0.5 |   369.0 ± 1.0  |            1.32× |
| **mcc-self**    |      97.3 ± 1.0 |   279.9 ± 0.8  |            1.00× |
| mcc-musl        |     114.0 ± 0.5 |   307.4 ± 6.9  |            1.10× |
| mcc-static      |     113.5 ± 1.0 |   304.9 ± 0.8  |            1.09× |

`mcc-self` compiles the entire ~100 k-line amalgamation to an object in **280 ms**;
`gcc -O2` needs **16.1 s** for the same TU — a **57×** gap — and `clang -O2`
13.4 s (48×). Even against a plain `-O0` reference, a *debug* `mcc` is 4.0–8.0×
faster; a release `mcc` (the shipped build) widens that severalfold. This is the
near-`-O0`, integrated-assembler design paying off, and the gap grows with TU
size and opt level.

> ³ preprocessing is optimization-independent, so `gcc-release`/`clang-release`
> `-E` equal their `-debug` values (measured once each).

The three new libc rows — **`mcc-musl`** (musl-linked, dynamic), **`mcc-static`**
(musl, fully static), and their `mcc-gcc`/`mcc-clang`/`mcc-self` glibc siblings —
are all now producible (§4e); the musl-linked `mcc` binary runs ~10 % slower than
its glibc twin (`307` vs `280` ms `-c`; the musl allocator on this workload), and
static vs dynamic musl is within noise (`305` vs `307`).

#### 4c. The self-host cost inverts at `-O0`

With every `mcc` built `-O0` (not RelWithDebInfo), the "which compiler built
`mcc`" comparison flips from the old RelWithDebInfo result:

- **`mcc-self` is now the *fastest* glibc `mcc`.** It compiles the amalgamation in
  **280 ms** and preprocesses in **97 ms** — beating the gcc-`-O0`-built `mcc-gcc`
  (290 / 107 ms) and the clang-`-O0`-built `mcc-clang` (369 / 96 ms). `mcc`'s own
  near-`-O0` codegen produces a *faster* `mcc` binary than `gcc -O0` or `clang -O0`
  do; `clang -O0` is the worst of the three (369 ms `-c`).
- **The old "~1.7× bootstrap cost" was an artifact of the RelWithDebInfo baseline.**
  When `mcc-gcc`/`mcc-clang` were `-O2` builds they beat `mcc-self` 1.7×; at an
  even `-O0` footing the direction reverses — `mcc`'s codegen is competitive with,
  and here ahead of, an unoptimized gcc/clang. Since all three emit byte-identical
  code (§1), this is purely a speed-of-the-driver effect. (A release `mcc` would be
  faster than all of these; the `-O0` spread is what this doc measures.)

#### 4d. Memory — peak RSS (`/usr/bin/time -v`, compile `-c` of `src/mcc.c`)

```sh
taskset -c 2 /usr/bin/time -v <compile -c cmd> 2>&1 | grep 'Maximum resident'
```

| config        | peak RSS |
|---------------|---------:|
| gcc-debug     |  157 MB  |
| gcc-release   |  329 MB  |
| clang-debug   |  144 MB  |
| clang-release |  242 MB  |
| mcc-gcc       | 29.4 MB  |
| **mcc-self**  | 29.7 MB  |
| mcc-musl      | 29.3 MB  |
| mcc-static    | 28.7 MB  |

`mcc`'s footprint is ~**29 MB** where `gcc -O2` needs **329 MB** (11×) and
`clang -O2` 242 MB — flat across all `mcc` builds (same allocator, same codegen),
so the libc/link choice in §4b costs *time*, not *memory*. (This is up from the
prior ~12 MB figure: the CST subsystem is now on by default and the `-O0` `mcc`
binary is larger; the relative story is unchanged.)

#### 4e. The static / musl self-host variants — now producible

All three extra rows in §4b are real builds now, not `n/a`. Two things changed
since the earlier "environment-limited" note:

- **A musl sysroot is built from vendored source, on any host.**
  `cmake --build <dir> --target vendor-musl musl-sysroot` clones musl `v1.2.5`
  and builds `vendor/musl-sysroot` (headers + crt + `libc.a/.so` + the
  `ld-musl-*.so.1` loader). The `MCC_BUILD_MUSL` variants bake that loader as the
  interpreter, so **`mcc-musl`** is a genuine musl-linked binary
  (`readelf -l` → `…/vendor/musl-sysroot/lib/ld-musl-x86_64.so.1`), not a glibc
  one. The self-hosted musl `mcc` is built with `mcc-musl` as the compiler
  (`CMAKE_C_COMPILER=…/mcc-musl`, `MCC_TOOLCHAIN_PROFILE=mcc`).
- **`mcc`'s own `-static` self-link was fixed** (2026-07-06). It previously left
  `__ehdr_start`, the 128-bit soft-float `__multf3/…`, and `_Unwind_*` unresolved;
  `mcc` now synthesizes `__ehdr_start`, carries the soft-float + weak unwind stubs
  in `mccrt`, and resolves static IFUNC/TLS. So **`mcc-static`** — a fully static,
  self-hosted musl `mcc` — links and runs (`mcc-musl -static`, no interpreter).

One caveat remains: a fully-static link against **glibc** (not musl) now gets all
the way through IFUNC resolution and TLS setup but then hits a glibc `libc.a`
archive-member gap (`__pthread_initialize_minimal`), so the static row measured
here is the **musl** one; glibc-static via `mcc` is tracked in `docs/TODO.md`.
The gcc-driven `linux-gcc-static` preset still builds a glibc `mcc-static` fine.

### 5. Where the time goes — `perf` self%

With `perf_event_paranoid=1` (§3a), user-space sampling needs no root. Attribute
against the gcc-built profiling compiler (`cmake-prof-gcc`, now `-O0 -g`, which
symbolizes cleanly with no inlining) on the include-heavy amalgamation:

```sh
P=cmake-prof-gcc
taskset -c 2 perf record --call-graph=fp -o perf.data -- \
  sh -c "for i in \$(seq 1 40); do $P/mcc $ARGS -o /tmp/pp.o; done"   # $ARGS = §2b
perf report -i perf.data --stdio --no-children | grep -E '\[\.\]|\[k\]' | head -12
```

The `-E` and `-c` profiles now diverge sharply, because the **CST subsystem is on
by default** (`MCC_CST`) and its per-node hash-consing runs only in the compile,
not in bare preprocessing:

```
compile -c (src/mcc.c):            preprocess -E (src/mcc.c):
  21.8%  cst_mix64      <- CST        18.2%  next_nomacro   <- lexer core
   8.5%  cst_hash_bytes    hash        9.4%  get_tok_str
   6.2%  next_nomacro                  4.5%  tok_str_add2
   3.0%  cst_build_sourcefile          4.4%  _IO_fputs (libc; -E output)
   2.1%  tok_str_add2                  4.2%  mcc_preprocess
   1.4%  preprocess_skip               3.7%  preprocess_skip
   1.3%  unary / cst_node_at           2.7%  next
   ~1%   cst_render_node / cst_alloc_node / cst_leaf_kinded …
```

**Two different hot spots depending on phase.** In `-E`, the lexer still dominates
(`next_nomacro` + `next` ≈ 21%) exactly as before — the §6/§8 lexer story holds
for preprocessing. But in a full `-c`, **CST hash-consing is now #1**:
`cst_mix64` + `cst_hash_bytes` are **~30%** of self-time (the amalgamation
`#include`s every `src/*.c`, and each file is interned into a hash-consed
`SourceFile` template — see docs/CST.md), pushing `next_nomacro` down to 6%. On an
include-heavy compile the CST hasher, not the lexer, is the first thing to
optimize (tracked in `docs/TODO.md`).

#### 5a. Which instructions inside `cst_mix64` (perf annotate)

```sh
perf annotate -i perf.data --stdio -s cst_mix64 | sort -rn | head
```

`cst_mix64` is a multiply-xor hash finalizer; its cost is exactly the mix itself —
the `imulq %rdx,%rax` and the two `xorq %rax,-8(%rbp)` fold steps are ~23%
combined, with the trailing `retq` (call overhead, `-O0`) another ~10%. It is
called once per interned CST node, so the win is *fewer calls* (coarser hashing
granularity / caching) rather than a cheaper mix.

The whitespace-skip loop (mccpp.c ~2779) **does not appear** — it is cold on this
TU.

### 6. Lexer optimization — what's applied, and what doesn't fit

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

#### The kept change (idea #1)

`src/mccpp.c`, +3/−1:

```c
#define tok_ts (mcc_state->tok_ts)           // MCCState member (embeddable/thread-safe)

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

### 7. Validation matrix

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

### 8. Conclusions — where a *measurable* win would come from

- For **preprocessing** the lexer is the right place to look (`next_nomacro` is #1
  by a wide margin), but the per-token dispatch micro-costs targeted by ideas
  #1–#4 are already below the noise floor on a 416-line TU. For a full **compile**
  the picture changed: CST hash-consing (`cst_mix64`, §5) is now the hottest code
  on include-heavy input and is the higher-value target.
- The two genuinely hot instruction clusters are the **identifier interning hash
  chain walk** and the **per-byte hash computation** — not whitespace, not
  number scanning, not the `define_find` lookup. A measurable win has to come
  from those, not from more per-token dispatch micro-tuning.
  - **Applied (2026-07-05): `TOK_HASH_SIZE` 16384 → 65536.** A `hyperfine` A/B on
    identifier-dense 2 MB TUs measured a consistent **1.03–1.06× faster** `mcc -E`
    (statistically significant, ±0.02–0.04) by lowering the intern-hash load factor
    so the chain walk shortens. The intermediate 32768 showed no realistic-density
    gain (threshold effect). Cost is a *fixed* +384 KB hash table that does not
    scale with input, so the low-RSS profile is preserved; correctness is unaffected
    (still a power of two for the `& (TOK_HASH_SIZE-1)` mask; full suite green).
    `TOK_HASH_FUNC` itself was left alone — it is already a cheap shift-add-mix.
- **The larger input separates signal from noise.** 416 lines is too small to
  separate a 1–2% lexer change from noise. The amalgamation spread (§4b) does
  better: `src/mcc.c` preprocesses in ~107 ms (`mcc-gcc`) with σ ≈ 0.6 ms — a
  ~0.6 % floor vs `full_language.c`'s ~1 %, and 4× the absolute signal — so a
  lexer change shows up there long before it clears noise on the small TU. Lexer
  experiments belong on `-E src/mcc.c`, not `full_language.c` (tracked in
  `docs/TODO.md`).

- **The full spread (§4) reframes "fast" quantitatively.** Even a *debug* (`-O0`)
  `mcc` beats `gcc -O2` on the amalgamation by **57×** in time and **11×** in peak
  RSS, and the gap grows with TU size, opt level, and a release `mcc` build. At an
  even `-O0` footing `mcc`'s own codegen is no longer a bootstrap *cost*:
  `mcc-self` is the **fastest** of the three glibc `mcc` builds (§4c), ahead of a
  gcc-`-O0`- or clang-`-O0`-built `mcc`. Making the shipped (`-O2`) `mcc` faster
  still is a codegen-backend project, separate from the front-end work above.

## C99/C11 conformance — mcc project status: deliberate differences & limitations

Moved from the appendix of [C9911.md](C9911.md), whose clause-by-clause map
remains the source of truth for *what the standards require* and *the
per-compiler status*. This records the things that are mcc-project-specific
rather than standard-derived: mcc's conformance philosophy, host/platform
limitations, and its **deliberate** documented differences from gcc/clang (`[~]`
items kept on purpose). Marker legend: `[ ]` open · `[~]` partial/deliberate-DIFF.

### Conformance philosophy & severity tags

Goal: implement every detail of C99 (N1256) and C11 (N1570), backed by regression
tests across all targets (x86_64, i386, ARM, AArch64, RISC-V 64), platforms
(ELF/PE/Mach-O), and both glibc and musl. The default standard is **C11**
(`cversion=201112`, set in `mcc_new()` at `src/libmcc.c:911`, and predefined as
`__STDC_VERSION__`); `-std=c99` selects C99, `-std=c17`/`-std=c23` the later ones.

mcc's stance on constraint violations is **permissive-by-default**: many
constraint violations are emitted as a *warning* (a diagnostic of any severity
satisfies the standard's "shall ... a diagnostic" requirement) and are upgraded
to hard errors under `-Werror`; `-pedantic`/`-pedantic-errors` add the ISO
pedantic diagnostics. gcc/clang choosing to make some of these hard errors by
default is their policy, not a standard mandate — hence several `[~]`/`[DIFF]`
entries below are *conformant* differences, not defects.

Test discipline: every fix ships with a cli/exec/diff regression test; the suite
(`ctest`, the cli+exec suites, the diff3 differential suite, the macho/wine/qemu
matrices) and the byte-identical 3-stage self-host fixpoint stay green. The
project has a history of false-positive audit entries — re-verify any candidate
gap 3-way against the live binary before acting on it.

Severity tags used below: **[BUG]** wrong codegen/crash/hang/rejects-valid ·
**[FEATURE]** absent standard feature · **[DIAG]** undiagnosed or
wrong/misleading diagnostic · **[OPT]** standards-optional · **[TASK]**
test/infra · **[LIMITATION]** host/platform constraint outside mcc · **[DIFF]**
deliberate, tested difference from a reference compiler (not a defect).

### Platform / host limitations

- [ ] **[LIMITATION] The kernel-fused Mach-O / libSystem path needs a macOS or darling host.**
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

- [ ] **[TASK] Systematic diagnostic-coverage sweep.**
  Each `[DIAG]`/`[FEATURE]` item ships with a cli/exec test that asserts the
  diagnostic fires and that the valid forms still compile. A full mechanical
  parity sweep across all ~312 `mcc_error`/`mcc_warning`/`mcc_pedantic` call
  sites in `src/` — to confirm the diagnostics that predate this tracker each
  have a regression test — remains an ongoing audit.

- [ ] **[DIFF] diff3 differential divergences on macOS.**
  The `diff3` suite compiles each golden with `mcc`, gcc, and clang and flags
  cases where `mcc` differs from a gcc==clang consensus. Four such divergences
  remain on macOS, none of which is an `mcc` defect (`predefined_macros`
  __STDC_VERSION__ default; `bitfields_ms` implementation-defined layout;
  `cleanup` teardown ordering; `c11_freestanding_headers` where `mcc` is the
  most conformant of the three). The suite returns non-zero on any divergence
  by design; these four are the expected residual on a macOS host.

---

### §6.5–6.6 Expressions & constant expressions

- [ ] **[DIAG] §6.5.16.1 — assignment to a const-qualified struct/union is diagnosed (as a warning).**
  Audit re-check correction: `a = b` where `a` is `const struct S` *does* warn
  "assignment of read-only location" — `vstore` calls `verify_assign_cast`
  (`src/mccgen.c:3555`) whose const check (`:3458`) fires for aggregates too,
  exactly as for the scalar case. mcc's long-standing stance is to *warn* on
  assignment-to-const (scalar and aggregate alike); gcc/clang *error*. Only the
  severity differs. Promoting to a default-on error would be more conformant but
  is a broad change to a long-standing lenient warning — deferred.
  3-way: mcc=warns | gcc=error | clang=error.

- [ ] **[DIAG] §6.6p3 — comma operator in an integer constant expression diagnosed only under -pedantic.**
  A required constant expression "shall not contain ... comma operators" outside
  an unevaluated subexpression. `int a[(1,2)];` is `mcc_pedantic`
  (`src/mccgen.c` in `gexpr`, ~`:8024`): silent at default level, but `-pedantic` warns and
  `-pedantic-errors` hard-errors. clang is also lenient at default; gcc rejects
  the file-scope case by default via its VLA-at-file-scope path. Effectively
  handled — consider whether to promote to a default-on warning. Low priority.

---

### §6.7 Declarations

- [ ] **[DIAG] §6.7.4p2 — `_Noreturn` on a non-function object diagnosed only under -pedantic.**
  Function specifiers shall declare only functions. `_Noreturn int x;` is
  diagnosed via `mcc_pedantic` (`src/mccgen.c` declarator storage-class check, ~`:10274`, gated on bit 128 = the
  `_Noreturn` keyword vs `__attribute__((noreturn))`): silent at default,
  `-pedantic` warns, `-pedantic-errors` errors. gcc warns by default; clang
  errors by default. Effectively handled under `-pedantic`; consider promoting
  to a default-on warning to match gcc. (The parallel `inline int x;` is a
  default-on hard error at `src/mccgen.c` ~`:10270` — intentionally asymmetric since
  gcc treats `_Noreturn` on an object as an extension.)

---

### §6.10 / §5 Preprocessor & environment

- [ ] **[DIFF] §6.10.3.3p3 — invalid token paste is warn-and-continue (deliberate leniency).**
  `C(+,-)` / `C(*,/)` emit a warning and continue (rc=0, output `+ -` / `* /`).
  The result is UB (no diagnostic strictly mandated); mcc deliberately recovers
  rather than aborting, and this is locked in by the `pp_invalid_paste` exec
  golden. gcc/clang hard-error; this is an intentional, tested difference, kept.
  (The genuinely-broken comment-introducer case `//`/`/*` IS a hard error now.)
  3-way: mcc=warn(rc=0) | gcc=error | clang=error.

---

### Additional deliberate differences & gaps

#### §6.4 / §6.10 lexical & preprocessor

- [ ] **[BUG] §5.1.1.2 / §6.10 — `\`+whitespace+newline not spliced (now terminates, still not gcc-compat).**
  The earlier hang is fixed; the residual is behavioral: `-c` hard-errors "stray
  '\'" and `-E` emits a stray `\` token, whereas gcc/clang treat `\`+ws+NL as a
  line-continuation splice with a `-Wbackslash-newline-escape` warning. Making
  `handle_stray` consume `\`+whitespace+newline as a warned splice would match
  the references. Low priority (extension; mcc's strict-ISO rejection is
  defensible).
  3-way: mcc=stray-`\`/no-splice | gcc/clang=splice+warn.

#### §6.7 / §6.2 / §6.9 declarations

- [ ] **[DIFF] §6.7.2.1 — no-declarator tagged struct *member*: mcc matches gcc `-fms-extensions` (silent).**
  `struct S { struct T { int x; }; };` — re-verified 3-way: gcc *default* (no
  `-fms-extensions`) warns "declaration does not declare anything"; but gcc
  **`-fms-extensions` is silent even under `-pedantic`**, and clang
  `-fms-extensions -pedantic` instead warns "anonymous structs are a Microsoft
  extension". mcc enables MS extensions by default (so the nested tag becomes an
  anonymous member), which matches gcc `-fms-extensions` exactly — silent. The two
  references disagree once MS extensions are on, and mcc tracks gcc; adopting
  clang's MS-extension warning would diverge from gcc. Left as a defensible DIFF.
  (`src/mccgen.c` already warns when `ms_extensions == 0`.)

- [ ] **[DIFF] §6.8.6.4p1 — `return <expr>;` in `void` / `return;` in non-`void`: CONFORMANT (diagnosed as a default-on warning).**
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

#### §7 library / floating-point builtins

- [ ] **[FEATURE] §F/§7.12 — GCC/Clang floating-point builtins (constants + classification done).**
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
  `__fpclassify` function (not the gcc builtin) because mcc's predefined
  `__GNUC__ 4` (4.2.1) is below glibc's `__GNUC_PREREQ(4,4)` threshold for the
  type-aware `__builtin_fpclassify`, so they need `-lm` where gcc's builtin does
  not — a usability DIFF, not a correctness gap. They cannot be replaced by clean
  constant-foldable macros: the subnormal test needs the *per-type* smallest-normal
  threshold, which a type-agnostic macro can't know (gcc uses a type-aware
  builtin). The macro builtins above also multi-evaluate their argument (intrinsics
  don't). The `NAN`-sign and `signbit`-return behavior matches gcc/clang: mcc
  predefines `__GNUC__ 4`, so glibc's `<math.h>` takes its GCC path — `NAN` prints
  `nan` and `signbit` returns a normalized `0`/`1` (§7.12p5, §7.12.3.6p3).

## CST database — design record (frozen spec §0–§11)

The authoritative CST design, folded in from the former `docs/PLAN.md` (frozen
spec) + `docs/IMPLEMENTATION.md` (build order) + `docs/CST.md` (post-completion
decisions) when those files were consolidated (2026-07-06). Section numbers are
preserved so the `NOTES CST §N` citations throughout `src/mcccst.{c,h}`,
`src/mcc.h`, `src/mccpp.c`, `src/mccgen.c` still resolve here. The subsystem is
**complete and gated** (see the slice-by-slice completion record + the D1–D5
gap-closure record below); this section is the *why*, those are the *what
landed*. Everything here is verified against the shipped source unless flagged.

### §0 — North-star invariants (hold at every commit; each is a *tested* gate)
1. **Byte-identical codegen** CST-in or CST-out — the CST is a pure side-effect,
   never feeds a codegen decision (gate §8.5). 2. **Zero-cost when off** — one
   CMake node → PP define; off ⇒ every hook compiles to nothing and `mcccst.c` is
   an empty TU. 3. **Total self-containment** — the CST shares no memory with the
   compiler: own arena, own copy of each file's bytes, own interned strings, all
   cross-refs as CST-internal node-ids (never `Sym*`, never `BufferedFile.buffer`
   pointers). The compiler is an oracle consulted during construction, never a
   backing store. 4. **Pure reflection** — round-trips to byte-identical source;
   every datum derivable from source text alone. 5. **No new external deps.**

### §1 — Frozen design decisions
- **Representation:** flat data-oriented SoA — `kind[] parent[] first_child[]
  next_sib[] width[] struct_hash[] trivia_hash[] sym_ref[]` + reserved
  `slot_key[]` + a `Token` leaf side-table. Index-addressed, mmap-serializable
  1:1. Children **linked** (`first_child`/`next_sib`), not contiguous ranges, so a
  future 5B splice is a pointer patch, no array shift.
- **Node identity:** tagged 64-bit `(u32 file : u32 local)`. SoA indexed by bare
  `u32` local; any cross-file ref (sym use→def, `IncludeDirective` target, dedup
  entry) stores the full 64-bit id. Reserves multi-file + dedup room so id width
  never migrates.
- **Construction:** side-recording from the existing `mccgen.c` recursive descent;
  oracle-only. **Positioning:** relative widths (green-node style); absolute char
  offset never stored/hashed; a **mandatory** `offset→node` index is a rebuildable
  accelerator over the canonical widths (LSP hits it per keystroke).
- **Hashing:** non-crypto 128-bit Merkle, two channels — position-independent
  **structural** `H_s` (identity/reuse/dedup/TDD) + separate **trivia/layout**
  `H_t` (comment/whitespace edits).
- **Persistence:** v1 = mmap-able arena snapshot behind a versioned header
  (magic + format-version + endianness + section table) — never a raw dump.
- **Symbols:** consult the live `Sym` at build to learn a use's def-site, store
  only `use-node → def-node` ids (8A). **Includes:** per-file subtrees stitched by
  id — each file its own `TranslationUnit`/`SourceFile` over its own owned buffer;
  `IncludeDirective` holds a cross-file node-id (the LSP document model; one edited
  header reparses without rebuilding every includer).
- **Trivia:** `(kind, rel-span)[]` on leaves, excluded from `H_s`. **Resilience:**
  `Error`/`Missing` node kinds reserved now (recovery is an LSP-era addition) so
  adding recovery never reshapes the node format.
- **Consumer order (each a later milestone):** TDD "pure reflection" harness
  (done) → `-g` debugging → LSP → optimization layer.

### §2 — Node schema (all reserved kinds now *produced* after D1)
Structural: `TranslationUnit Declaration FunctionDef Declarator ParamList
StructOrUnion Enum TypeName Initializer CompoundStmt If/While/For/Do/Switch/
Return/Goto/Label/ExprStmt Binary Unary Call Member Index Cast Cond Comma Paren
Primary`. Preprocessor (concrete): `MacroInvocation IncludeDirective PPDirective
PPConditional`. Leaf: `Token` (token-kind + owned byte span + trivia pieces).
Still reserved-only: `Error`, `Missing` (LSP resilience). `Comment` was reserved
in the frozen plan but is now produced (D1d). Width is **relative** = children +
own leaf bytes + attached trivia; absolute offset = prefix-sum of preceding
siblings' widths + parent offset, served by the mandatory `offset→node` index.

### §3 — Hashing specification
`H_s(leaf) = mix(salt(token_kind), bytes(leaf_span))` (trivia excluded);
`H_s(internal): h = salt(kind, child_count); for c: h = mix(h, H_s(c))` — order-
and kind-sensitive, child-count salt disambiguates `a+b` vs `a+(b)`. `H_t` folds
comment/whitespace bytes + widths so format-only edits localize without dirtying
`H_s`. Incremental update rehashes only root-path nodes whose child-hash multiset
changed — O(depth); this *is* the hierarchical incremental hash.

**§3.1 — Invertible epoch hash `H_e` + tombstone sweep (5B-era, DESIGNED / NOT
BUILT).** The deferred incremental-rehash layer for live edits. Canonical `H_s`
is non-invertible, so a changed child forces an O(fanout) sibling re-scan; in C
the trees are wide-and-shallow so fanout dominates. Design: a dual-hash where the
combine is a slot-keyed group op `H_e(parent) = Σ_i M(key_i)·H_e(child_i) (mod
2^128)`, `M(key)` odd — subtractable in O(1) per level (`H_e += M(key_i)·(new −
old)`; a removed child becomes a Null **tombstone** with `H_e = 0`, dropping its
term exactly). Edits do O(1) algebraic patches into a dirty-set with no ancestor
walk; an explicit later **sweep** compacts tombstones, renormalizes slot keys, and
reconciles `H_s` over the touched frontier only. Changes the cost model (per-level
O(fanout)→O(1)), not the O(depth) bound. Four caveats to design around: slot-bind
via `M(key_i)` never a bare sum (order-collisions); use **stable/fractional keys**
so inserts don't renumber siblings; linear combiner is weaker collision-resistance
so `H_e` never replaces `H_s` as the content address; Null must be true group
identity **and** zero-width until swept or offset arithmetic drifts. Reserved for
now: the per-node slot-key field + frontier-scoped `H_s`-recompute (both shipped).
**Drift note:** the frozen plan said `slot_key` stays *unused/0 in v1*; D3 now
**repurposes `slot_key`** to hold the 1-based branch-body tag
(`cst_mark_branch`, `src/mcccst.c:512`). It is still 0 for the epoch-hash purpose,
but no longer universally zero — a future `H_e` build must account for that.

### §4 — Memory & preprocessor model
CST owns the source (per-file owned buffer copied on first read; leaf spans point
into that copy, survives `BufferedFile` recycle, serializes fixup-free). Owned
`CstArena` bump allocator, `free`/`snapshot`/`load` as a unit. Symbol refs are
node-ids (no `Sym*` in CST memory). **Preprocessor/macros** (highest-risk, §11):
parser sees post-expansion tokens (`next()`), but pure-reflection needs *written*
source — so leaf spans are captured at the lexer/source boundary and a macro use
becomes a `MacroInvocation` node whose span covers the use-text (round-trip/LSP)
and whose children hold the expansion (for future `-g`/opt). Phased: M1 handled
the expansion-transparent subset, full fidelity landed as dedicated milestone Mμ.

### §5 — Byte-offset facility
As shipped: a per-file base `cst_base` (`src/mcc.h:464`), maintained in
`handle_eob`, combined with buffer-pointer arithmetic so `abs_off(p) == cst_base
+ (p - buffer)`. `cst_base` advances by the discarded-window length at each chunk
refill; offsets captured as absolute values at token start/end so they survive
mid-token refills. (The frozen plan's earlier "monotonic per-byte counter" phrasing
was never built — this base-offset model is the shipped one.) Zero-cost off.

### §6 — Integration points (hooks)
Recording driven from the existing descent; hook macros expand to nothing when
off. Primary `mccgen.c` sites: `decl()` (Declaration/FunctionDef), `struct_decl()`
/`post_type()`/`type_decl()` (type structure), `block()`/`gexpr_decl()`/`lblock()`
(statements), the `unary`/`expr`/`gexpr` expression family, `next`/`skip` (leaf
capture). Pattern: a small explicit node stack; each grammar fn brackets with
`cst_open(kind)`/`cst_close()`. Debug builds assert stack balance (each open
closed once; empty at `decl()` end) — catches hook-coverage drift directly.

### §7 — CMake gating
`mcc_config_node(MCC_CST TYPE BOOL DEFAULT ON GROUP "Advanced" ADVANCED)`
(`CMakeLists.txt:1087`) — default **ON**; codegen byte-identical on/off, guarded
by §8.5. ON ⇒ `PRIVATE CONFIG_MCC_CST=1` + `src/mcccst.c` in sources; a `cst`
preset in `CMakePresets.json`; `#if CONFIG_MCC_CST` guards every hook + new file.

### §8 — TDD "pure reflection" gates (`tools/csttool` + `tests/cst/`)
1. **Round-trip:** CST→source byte-identical over the corpus (strongest
   proof). 2. **Span coverage:** child spans tile the parent, sum-of-widths ==
   parent, every source byte covered once. 3. **Bidirectional lookup:**
   offset→node→span round-trips; def↔use ids round-trip. 4. **Hash invariance:**
   `H_s` unchanged by whitespace/comment edits, changes iff token structure does;
   identical subtrees share a hash. 5. **Codegen-identity:** mcc with/without
   `CONFIG_MCC_CST` emits identical output over the corpus (protects §0.1).
   6. **Snapshot round-trip:** dump→reload identical tree+hashes; version/endian
   skew rejected cleanly.

### §9 — Milestones (all landed) & consumers (future)
M0 scaffolding · M1 leaf+owned-source+round-trip · M2 structure+widths+lookup ·
M3 hashing+snapshot ("CST complete for source-of-truth") · Mμ macro fidelity ·
M4 symbol refs — **all done** (slice records below). **M5+ consumers are separate
future plans** (see TODO Later): `-g` (span↔PC via `mccdbg.c`) → LSP (8C lazy
resolution + `Error`/`Missing` resilience + 5B incremental splice + 6B store) →
optimization layer.

### §10 — Deferred / reserved (roadmap; status as of 2026-07-06)
- **5B incremental splice** — DEFERRED. Needs 4B rolling-hash re-lex-window
  finder, parser error-recovery (LSP-era), `Error`/`Missing` nodes. Batch rebuild
  is the same code minus the splice.
- **`H_e` epoch hash + tombstone sweep** (§3.1) — DEFERRED (designed; slot-key
  field + frontier-scoped recompute reserved/shipped).
- **4C hash-consing** (physical subtree dedup keyed by `H_s`) — **now BUILT**,
  pulled forward by D3 as the content-addressed `SourceFile` template store
  (`cst_store_intern`).
- **6B content-addressed store** — PARTIAL: the in-memory `H_s`-keyed store ships
  (D3); the on-disk archival format layered over the 6A working image (backbone
  for hot-reload / reconciled-CST snapshots — see TODO Later) is still deferred.
- **9B trivia-as-nodes** (`Comment`) — **now DONE** (D1d).
- **6C embedded KV** — REJECTED (dependency conflict); LSP queries synthesized
  from hand-rolled `offset→node`/`name→def`/`def→uses` indices over the flat
  arrays.

### §11 — Risk register (each pinned to a tripwire test)
Macro-fidelity vs pure reflection (grown under the round-trip corpus, not up
front) · hook-coverage drift (span-coverage/tiling §8.2 + debug balance assert) ·
zero-cost-off regressions (codegen-identity §8.5 in CI) · width/offset arithmetic
bugs (tiling invariant §8.2).

### The seam (as-shipped interface contract, from the former IMPLEMENTATION.md §3)
Pure slices (B store, C hashing, D geometry, E serialization) publish plain
functions in `src/mcccst.h`; compiler-side slices drive them mostly through the
`CST_*` hook macros (`CST_OPEN`/`CST_OPEN_AT`/`CST_MARK`/`CST_CLOSE`/`CST_LEAF`,
all `((void)0)` when off). The deferred-capture model (slice H) also calls a few
hook functions directly under `#if CONFIG_MCC_CST` — `cst_hook_token` (leaf
capture; the `CST_LEAF`/`cst_hook_leaf` pair ended up **vestigial**, 0 uses),
`cst_hook_def`/`cst_hook_use` (slice I), `cst_hook_wrap` (slice J), `cst_cur_tok_off`.
All structure/hashing/geometry/IO stays inside `src/mcccst.c` behind those
functions, so §0.2 stays mechanical. The per-slice `mcccst_{hash,geom,io}.c` split
sketched in the original build order was **not taken** — all slices live in the
single `src/mcccst.c`; the harness is the single file `tools/csttool.c`.

---

## Completed work — CST database (all vertical slices landed)

The CST (Concrete Syntax Tree) database subsystem is complete — every
vertical slice (S0, B–J, the weaves, FINAL) landed and is gated in CTest.
This is the folded completion record moved out of `docs/TODO.md`; the design
lives in [§ CST database — design record](#cst-database--design-record-frozen-spec-011) above, the
implementation in `src/mcccst.{c,h}` (+ `tools/csttool`, `tests/cst/`).
`MCC_CST` is now built **on by default** (CMakeLists.txt:1087), codegen
byte-identical either way. The slice-H/G follow-ups noted here originally
(Declaration/FunctionDef grouping + ParamList; per-file include stitching) were
subsequently closed by the D1b/D3 gap-closure work (see the next section);
what remains open is the future-consumer / 5B roadmap in `docs/TODO.md`.

Two headline deliverables, implemented via the vertical slices below:
- [x] CST Database for Debugging, LSP, and Optimization data/layers — the
  side-recorded, self-contained CST substrate is built, populated from real
  compilation, round-trips byte-identically, hashes, serializes, and carries
  symbol refs. The *consumer* layers (-g/LSP/opt) are separate future plans
  (NOTES CST §9 M5+); this delivers the data layer they build on.
- [x] CST Database uses hierarchical incremental hashes to enable bidirectional
  lookups starting from any character index in any file — 128-bit hierarchical
  Merkle hashing (struct + trivia channels, frontier-scoped incremental rehash,
  epoch-hash seam reserved) + mandatory offset→node index; bidirectional
  lookup verified (§8.3) on 308 corpus files.

Legend for slices: each has a status line. `[ ]` open · `[~]` in progress ·
`[x]` done. Slice IDs + dependencies are given per-slice below (the `Deps:` lines).

### S0 — Gating & harness skeleton  ·  status: [x]
Deps: — · Kind: build · PLAN §7, §8
- [x] `MCC_CST` config node in CMakeLists.txt (mirror diagnostics node) → `CONFIG_MCC_CST`
- [x] `cst` preset in CMakePresets.json (configure/build/test, mirrors diagnostics)
- [x] `src/mcccst.{c,h}` present, self-guarded; `#include "mcccst.c"` in libmcc.c
- [x] `CONFIG_MCC_CST` source guards + no-op hook macros in mcccst.h
- [x] `tools/csttool` self-contained harness (#includes mcccst.c, links no compiler)
- [x] `tests/cst/{store,hash,geom,serial}` registered; 4/4 green via ctest
- [x] Codegen-identity gate (§8.5): mcc CST-on vs CST-off byte-identical over 42 files
- Notes: define appended to `_mccdefs` (CMakeLists.txt ~1642); mcccst.c un-poisons
  malloc/realloc/free (mcc.h:1215) via push/pop_macro for self-containment. mcc
  binary grows (~26KB, dead code until Weave 1) but its *output* is identical.

### B — Node store core (pure)  ·  status: [x]
Deps: S0 · PLAN §1, §2
- [x] `CstArena` growable SoA store + free/reset
- [x] SoA columns incl. reserved `slot_key`; linked `first_child`/`next_sib`
- [x] Tagged id scheme: `u32` local index + 64-bit `(file,local)` cross-file
- [x] `cst_node_open/close`, `cst_leaf`, append_child, column accessors
- [x] Synthetic-tree builder in harness + topology/id round-trip tests (cst/store)
- Notes: width accumulates bottom-up (leaf sets own len; close bubbles into parent).

### C — Hashing library (pure)  ·  status: [x]
Deps: S0 · PLAN §3, §3.1
- [x] 128-bit non-crypto hash (two lanes, splitmix-style finalizer)
- [x] `cst_hash_leaf` (kind salt + token bytes, trivia carved out)
- [x] `cst_hash_internal` (salt(kind,count) + Merkle fold)
- [x] `cst_hash_eq`, frontier-scoped `cst_rehash_frontier`; epoch-hash seam reserved
- [x] Invariance property tests (cst/hash): ws-invariance, token-sensitivity,
      identical-subtree equality, child-count salt, frontier==full
- Notes: leaf token bytes = owned span minus leading-trivia prefix (`tok_rel`).

### D — Geometry & offset→node index (pure)  ·  status: [x]
Deps: B · PLAN §1, §2, §5
- [x] Relative-width finalize on close; `cst_abs_offset` prefix-sum
- [x] Mandatory `offset→node` index build + `cst_node_at` (binary search)
- [x] Tiling invariant test (§8.2) + per-offset round-trip (§8.3) (cst/geom)
- Notes:

### E — Serialization (pure)  ·  status: [x]
Deps: B (+G stub) · PLAN §1, §8.1, §8.6
- [x] Versioned snapshot header (magic+version+endian) save/load, all columns
- [x] `cst_reflect` CST→source emitter (emits owned leaf spans in DFS order)
- [x] Save/load equality + version-skew rejection + reflect round-trip (cst/serial)
- Notes:

### F — Byte-offset facility (compiler)  ·  status: [x]
Deps: — · PLAN §5
- [x] Monotonic byte cursor `BufferedFile.cst_base` (guarded), maintained in
      handle_eob so abs_off(p) == cst_base + (p - buffer)
- [x] Correct across handle_eob refills (validated: 12KB file round-trips)
- [x] Validated via round-trip over 305 corpus files (offset model exercised)
- Notes: cursor advances by discarded-window length at each refill; captured as
  absolute values at token start/end so it survives mid-token refills.

### G — Owned source & trivia (compiler)  ·  status: [x]
Deps: B, F · PLAN §4, §1
- [x] Per-file byte copy into arena (cst_slurp of the main file) = LSP doc model
- [x] Trivia classified on real leaves (cst_leading_trivia: whitespace + line/
      block comments) → tok_rel set → excluded from H_s. Verified: whitespace/
      comment-only edits leave H_s unchanged, token edits change it (cst/hashinv)
- [x] Round-trip proves owned buffer + spans correct over corpus (309/0)
- [ ] Per-file subtree ownership for include stitching (main file only; multi-
      file include stitching is an LSP-era refinement, PLAN §1 Includes)
- Notes: trivia lumped as one leading WS piece per leaf; finer per-piece
  classification (separate comment pieces, 9B Comment nodes) is a later refinement.

### H — Recording hooks (WEAVE 1 + M2)  ·  status: [x]
Deps: B, D, F, G · PLAN §6
- [x] Leaf capture at next_nomacro exit (mccpp.c:2996), [prev_end,end) tiling;
      cst_capture_begin/end bracket mccgen_compile in mcc_compile
- [x] Deferred-capture model resolves single-pass lookahead skew: flat leaves
      (round-trip) + structural specs as leaf-index ranges materialized in
      cst_hook_end (node spans [open_count-1, close_count-1))
- [x] Structural brackets on the single-exit grammar functions: block()
      (statement kinds If/While/For/Do/Switch/Return/Goto/CompoundStmt/ExprStmt),
      type_decl() (Declarator), struct_decl() (StructOrUnion). basic.c: 33 flat →
      357 nodes; round-trip stays exact.
- [x] Corpus gate (cst_validate): round-trip §8.1 + tiling §8.2 + offset→node
      §8.3 all pass on 312/312 compilable files, 0 failures. Balance verified: 0
      hook imbalances across the corpus (temporary instrumentation, now assert).
- [x] Debug-build balance assert in cst_hook_end (CST_ASSERT cst_sstop==1);
      fires in assert-enabled builds (sanitize/diagnostics).
- [x] Expression nodes via retroactive range-based wrapping (cst_hook_open_at +
      cst_nest_specs rebuilds nesting from leaf-range containment, solving the
      precedence-climbing left-recursion cleanly): CST_Binary (only when an infix
      operator is present — no degenerate chains), CST_Cond (?:), assignment as
      Binary, CST_Comma, and postfix CST_Member/CST_Index/CST_Call. Verified on
      real code: `p[0].x` → Member(Index(p,0),x), `f(1,2)` → Call, in one flat
      Binary. Crash-hardened: empty-range specs (macro-expanded exprs) dropped.
- [~] Minor remaining (non-blocking refinements): top-level Declaration/
      FunctionDef grouping and ParamList — decl()/post_type() have many
      exit/continue points inside loops, making per-item retroactive wrapping
      awkward for marginal gain (Declarator + Compound already capture the
      structure). lblock()/gexpr_decl() are low-value wrappers.
- Coverage now: TU, Compound + all statement kinds, Declarator, StructOrUnion,
  Binary, Cond, Comma, Member, Index, Call, MacroInvocation, Token — a rich,
  correct, round-trip-preserving concrete syntax tree.

### WEAVE 2 — Hash & snapshot online  ·  status: [x]
- [x] Hashing runs on every real tree (cst_rehash_all in cst_hook_end)
- [x] Snapshot save/load of real compiled trees: reload validates + identical
      struct hash (MCC_CST_SNAPSHOT), gated in ctest via roundtrip.cmake (§8.6)
- Notes: moved up from the milestone list — landed together with M2.
- Notes (verified 2026-07-05 against source):
  - PLAN's `expr`/`cond_expr`/`binary` DO NOT EXIST. Real expr cascade:
    `gexpr`(7962)→`expr_eq`(7910)→`expr_cond`(7785)→`expr_lor`(7665)→
    `expr_land`(7659)→`expr_or`(7648)→`expr_xor`(7639)→`expr_and`(7630)→
    `expr_cmpeq`(7619)→`expr_cmp`(7607)→`expr_shift`(7596)→`expr_sum`(7585)→
    `expr_prod`(7574)→`unary`(6453). `expr_const`(8007). Each cascade level is
    single-exit fall-through — trivial to bracket.
  - `decl(int l)`@10074 has MULTIPLE returns (10089/10128/10386/10394) — needs a
    goto-epilogue or wrap at the caller for `cst_close`.
  - `block(int flags)`@8515 single-exit (converges at 8942) but has `again:`@8525
    loop + gotos — place `cst_open` AFTER the `again:` label or guard re-entry.
  - Token consumption = direct `next()`(mccpp.c:4057)/`skip()`(mccpp.c:71) calls;
    hook leaves either by wrapping those or reading `tok` at boundaries.
  - Add `#include "mcccst.h"` after mcc.h:190; hook prototypes near mcc.h:1462;
    mirror `CONFIG_MCC_ASM` #ifdef style (mccgen.c:10233).

### I — Symbol refs (WEAVE 3)  ·  status: [x]
Deps: H, B · PLAN §1(Symbols), §4
- [x] Def hook at type_decl_1 (declarator name) + use hook at unary() identifier;
      token-value→def-offset side table, resolved to node-ids in cst_hook_end
- [x] Stored as tagged `(file,local)` sym_ref; survives snapshot (cst/sym)
- [x] def↔use correctness verified on real code (cst/symref): p→param,
      myglobal→global, helper→function, local→local all correct
- Notes: v1 is last-declaration-wins (no scope stack) — shadowing across scopes
  can mis-resolve; documented. `name→def`/`def→uses` reverse indices: reuse
  sym_ref column, build lazily when the LSP consumer needs them.

### J — Macro fidelity / Mμ (WEAVE 3)  ·  status: [x]
Deps: H, F · PLAN §4, §11
- [x] Expansion-transparent subset (PLAN §4 M1): macro invocations captured as
      written source leaves; round-trips byte-identical over the corpus.
- [x] `MacroInvocation` nodes: at the macro-expansion boundary in next()
      (mccpp.c:4113), the written macro-use span (name + args) is wrapped as a
      CST_MacroInvocation via cst_hook_wrap. Object-like and function-like both
      covered; nests correctly with enclosing expressions. Gated by cst/macro
      (>=3 MacroInvocation nodes + round-trip on SQUARE/LIMIT/ADD fixture).
- Notes / accepted v1 imprecisions (PLAN §11 "grow under test"): (1) function-
  like invocations may exclude the trailing ')' to avoid overrunning the next
  construct (arg-scan lookahead varies); (2) object-like macros used *inside*
  another macro's args stay plain tokens; (3) expansion *children* (for -g/opt)
  are not attached — the node reflects *written* source, which is what round-
  trip/LSP need. All are refinements, not correctness gaps.

### FINAL — corpus & hardening  ·  status: [x]
- [x] All gates (round-trip §8.1 + tiling §8.2 + offset→node §8.3) over 308/308
      compilable corpus files; snapshot §8.6 + hash §8.4 gated in ctest
- [x] §0.1/§0.2 codegen-identity gate holds; full ctest CST-ON 819/819 and
      CST-OFF 811/811 (shared-file edits inert when off)
- [x] Risk items pinned: hook-coverage→cst_validate tiling; zero-cost-off→codegen
      gate; width arithmetic→tiling invariant; macro→round-trip corpus
- Notes: "tests2" corpus not present in this tree; used tests/** (379 files).

## Completed — CST next-phase gap closure (docs/CST.md D1–D5, 2026-07-06)

The CST D1–D5 decision plan (recorded in the design record above, folded from the
former `docs/CST.md`) is fully landed on top of the
vertical slices above: node-kind fill-in, PP-concrete capture, and the
`SourceFile` template/binding/render model. Migrated here from `docs/TODO.md`.
`MCC_CST` stays **on by default** (`CMakeLists.txt:1087`); codegen is
byte-identical CST-on vs CST-off. The CST hooks are pure side-effect recording —
the compiler never reads the CST — so the §8.5 codegen-identity invariant holds
for *any* hook change; the live risk is CST round-trip/tiling correctness, gated
by the `cst/*` ctest suite (20/20 green in the `cst`-on build; verified 2026-07-06).

- [x] **D1a — Expression fill-in (`Unary`/`Cast`/`Paren`/`Primary`).** Retroactive
  range-wrap in `unary()` (mccgen.c): prefix-op → `Unary`, `(type)e` → `Cast`,
  `(e)` → `Paren`, atoms → `Primary`. Gated `cst/kinds-expr`.
- [x] **D1b — Declaration structure via D2 range-wrap** (`Declaration`,
  `FunctionDef`, `ParamList`, `Enum`, `TypeName`, `Initializer`, `Label`). Gated
  `cst/kinds-decl`.
- [x] **D1c — PP-concrete** (`IncludeDirective`, `PPDirective`, `PPConditional`),
  full-concrete: capture *all* `#if`/`#else` branches as concrete nodes. Prereq
  for D3. Gated `cst/kinds-pp`.
- [x] **D1d — `Comment` promotion** (line/inline/block), `H_t`-only so §8.4 holds.
  Gated `cst/kinds-comment`.
- [x] **D3+D5 — `SourceFile` template + renderer.** The full template/binding/
  render model (mcccst.{c,h}): full-concrete branch tagging
  (`cst_mark_branch`/`slot_key`), a content-addressed store with pure-`H_s(body)`
  hash-consing + dedup (`cst_store_*`), per-instance recursive bindings
  (`cst_binding_*`), and the `render(template, binding)` fold with a threaded PP
  environment (`cst_render`) — plus `cst_render_identity` as the round-trip
  oracle. The **headline recursive re-include branch-selection gate**
  (`cst/template`) passes all five assertions. **Live capture wired** (`cst/incstore`):
  every real `#include` during a compile interns its file as a hash-consed
  `SourceFile` template (`cst_hook_include`, mccpp `parse_include`) and binds the
  `IncludeDirective` node to it — two `#include`s of one header (incl. via a
  nested header and guard-skipped repeats) collapse to a single template id.
  - [x] **DEBUG-build hash-collision tripwire.** `cst_store_intern` verifies, in
    a debug build, that an existing same-`H_s` entry reflects byte-identically
    to the interned body; on mismatch it `abort()`s with a fatal "hash collision
    … cst_hash_* must be fixed" — never silently deduping two different bodies.
  - [x] *Full-concrete live templates.* `cst_build_sourcefile` lexes each
    captured file line-by-line into a real tree: every line a leaf, each
    `#if/#else/#endif` a `PPConditional` whose branch bodies (dead branches
    included) are tagged `CompoundStmt` groups (`cst_mark_branch`) a binding can
    select among. Verified via the `MCC_CST_STORE` dump: increment.h → 2
    `PPConditional`, leaf.h → 1 (its guard), all `render_identity` round-trip
    exactly (`cst/incstore`, `cst/increment`).
- [x] **FINAL** — every `cst/*` gate green over the corpus; §0.1/§0.2 re-confirmed:
  CST-on ctest **830/830**, CST-off **811/811**, object output **byte-identical**
  CST-on vs CST-off across the sampled corpus, and the CST-off build compiles the
  now-empty `mcccst.c` (zero-cost-off). CST D1–D5 plan complete.

## Completed — docs-accuracy reconciliation (2026-07-06)

A validation pass over `docs/*.md` against the codebase (three agents + spot
checks) found the rest accurate and these items stale; all were fixed in the docs
and re-verified against the source. Migrated here from `docs/TODO.md`.

- **`MCC_CST` default OFF→ON.** `BUILD.md` (row moved out of the §4 Diagnostics
  table into §5 with `DEFAULT ON` / `GROUP "Advanced"`, "(experimental)" dropped)
  and `PLAN.md §7` (snippet now `DEFAULT ON GROUP "Advanced" ADVANCED`) matched to
  `CMakeLists.txt:1087`.
- **`cst` preset documented.** Added the `cst` developer preset to `BUILD.md §2`
  and named `local-ci` in the "no test preset" exception list.
- **Default C standard corrected C99→C11.** The `NOTES.md` conformance section now
  says the default is C11 (`cversion=201112`, set in `mcc_new()` at
  `src/libmcc.c:911`, predefined as `__STDC_VERSION__`); `-std=c99` selects C99.
  (Was wrongly "C99, `cversion=199901`, `src/libmcc.c:836`" — `:836` is unrelated
  `#else` code and `199901` is only the `-std=c99` value.)
- **PLAN §5 byte-cursor rewritten.** Now describes the shipped `cst_base +
  (p - buffer)` model (`src/mcc.h:464`; `cst_base` bumped in `handle_eob`) instead
  of the never-built "monotonic per-byte counter"; `IMPLEMENTATION.md §2-F` matched.
- **IMPLEMENTATION.md §3 seam refreshed.** Relabelled as the original design
  sketch plus an as-shipped correction: `CST_LEAF(tk, off, n)`, added `CST_OPEN_AT`
  / `CST_MARK`, and the rule now notes the direct `cst_hook_*` calls and that
  `CST_LEAF`/`cst_hook_leaf` are vestigial (0 uses; leaves captured via
  `cst_hook_token`). The §0 "only the hook macros" line was softened to match.
- **IMPLEMENTATION.md §2 file names fixed.** The per-slice
  `src/mcccst_{hash,geom,io}.c` (never created) → the single `src/mcccst.c`;
  `tools/csttool/` directory → the single file `tools/csttool.c`.
- **NOTES.md Profiling snippet fixed.** `static TokenSym *tok_ts; // file-scope` →
  the real `#define tok_ts (mcc_state->tok_ts)` (`src/mccpp.c:12`), an `MCCState`
  member.
- **Drifted conformance refs re-anchored.** `src/mccgen.c` comma-in-ICE
  `:7448`→~`:8059`, `_Noreturn`-on-object `:9509`→~`:10319`, `inline`-on-object
  `:9504`→~`:10315` (now symbol-anchored; the `:3557`/`:3456` const-assignment refs
  were already accurate).

Still open in `docs/TODO.md` (these need a measurement/CI run, not a doc edit):
the test-count reconcile, the "~100×" headline, the speed/size table re-measure,
the PROFILING §4–§5 pre-`TOK_HASH_SIZE` baselines, and the "all green" status
prose regeneration.

## Migrated from code comments (2026-07-06 comment-strip)

The codebase's inline comments were removed in favour of self-documenting code;
the non-obvious *why* — design rationale, cross-compiler/ABI gotchas, and
deliberate decisions — was moved here. Open-work items went to `docs/TODO.md`.

### CST recording — deferred-capture implementation (`src/mcccst.c`)

§6 above describes the hook *API* (`cst_open`/`cst_close` bracketing). The
*implementation* does not build the tree live: mcc is a single-pass parser with
one token of lookahead, so when a grammar function brackets a construct its first
token has **already** been lexed and captured. Capture therefore records a flat,
source-ordered **leaf list** plus structural **specs** as leaf-index ranges, and
materializes the nested tree in `cst_hook_end`.

- **Lookahead rule.** A node opened while `tok` is its first token spans leaves
  `[open_leafcount - 1, close_leafcount - 1)` — `cst_hook_close` excludes the
  lookahead token that belongs to the next construct.
- **Spec nesting (`cst_nest_specs`).** Parent/child links are rebuilt purely from
  `[first_leaf, last_leaf)` containment (not the open-time parent) so
  retroactively-opened expression nodes nest correctly. All grammar constructs
  are disjoint-or-nested, so a stack over the sorted specs yields the true tree.
  *Partial-overlap* specs (e.g. a `Declaration` whose start-mark is stale across
  an `#include` that straddles a PP-boundary node) are **dropped**, not tiled
  under two siblings — tiling them would break the round-trip oracle (§8.1).
  *Empty* specs (`first == last`, i.e. retroactive wraps around macro-expanded
  expressions that captured no source leaves) are also dropped, except the root.
- **Sort tie-break (`cst_spec_cmp`).** Order is `first_leaf` asc, then wider range
  first (`last_leaf` desc), then later-opened first (id desc). A grouping wrapper
  (`Initializer`/`Declaration`/…) is opened *after* the sub-expression it contains
  (retroactive range-wrap); when its span coincides byte-for-byte with its single
  child (e.g. `int c = (int)a;` where the `Initializer` equals the `Cast`) the
  higher id must sort first so the containment stack makes it the parent.
- **Self-contained memory.** `src/mcccst.c` uses only the C library so
  `tools/csttool.c` can link it alone (invariant §0.3). `mcc.h` poisons
  `malloc`/`realloc`/`free` to force mcc's allocators, so the file `push_macro`s
  and `#undef`s them (a no-op in the standalone `csttool` build).
- **Column reuse.** An `IncludeDirective`'s cross-file target (the interned
  `SourceFile` template id) is stored in the `sym_ref` column as `(template_id, 0)`;
  a `PPConditional` branch ordinal is stored in the reserved `slot_key` column
  (the D3 `slot_key` dual-use is also noted in §10 / TODO). `cst_store_intern`
  dedups by pure `H_s(root)`; a non-`NDEBUG` build verifies the two entries reflect
  byte-identically as a hash-collision tripwire before fusing them.

### Diagnostics — deliberate error-vs-warning choices (`src/libmcc.c`)

- **Calling an undeclared function is a hard error** (C99 §6.9.1/§7.1.4 constraint
  violation, not a warning). This prevents implicit-`int`/pointer-truncation
  miscompiles and makes autoconf-style feature detection behave. See
  `docs/C9911.md` §6.5.1p2 (undeclared identifier).
- **Return-type mismatch is an error**, downgradable via
  `-Wno-error=return-type` / `-Wno-return-type`: a `return <value>;` in a `void`
  function, or a value-less `return;` in a non-`void` function.
- **Caret rendering** never colorizes the embedder callback path (`error_func`)
  — embedders want plain text; `auto` follows the tty. The `CString` printf
  helpers keep `data[size]` a NUL not counted in `size`, so `"%s"` stops there.

### Runtime library — cross-compiler & ABI notes (`runtime/`)

The runtime is built by the *host* cc (gcc/clang/MSVC) as well as by mcc, so many
constructs exist to satisfy a foreign toolchain's type checker or assembler:

- **Builtin-name conflicts → assembler `.set` / asm labels.** Clang refuses to
  see several libcall names *defined* in C because it types them as builtins
  (`__atomic_*`, `__builtin_bswap64` as `uint64_t`, the four generic `libatomic`
  entry points). Mach-O also rejects non-weak `__attribute__((alias))` and gcc
  ICEs on `weak,alias` there. The runtime therefore exports those symbols through
  an assembler `.set` / asm label rather than a C alias
  (`builtin.c`, `stdatomic.c`, `atomic.c`).
- **`stdatomic.h` pointee types** must match Clang's builtin prototypes: a plain
  `void *` pointee conflicts with `__atomic_is_lock_free`; `bcheck.c`'s
  `never_fatal` is a plain `int` (not `_Atomic`) because the non-`_c11`
  `__atomic_fetch_add` rejects an `_Atomic(int)*` pointee under Clang.
- **Mach-O assembler quirks.** A foreign (host-CC) Mach-O assembler has no
  `.type`/`.size` directives and needs assembler-local labels to start with `L`,
  not `.L` (`atomic.c`).
- **`regparm`** is x86-only: gcc silently ignores it off-x86, clang errors — apply
  it only where meaningful (`bcheck.c`).
- **Cache-flush builtins.** `armflush.c` / `lib-riscv.c` exist only for the mcc
  build; gcc and clang provide the cache flush as a builtin (there is no
  `__arm64_clear_cache` / `__riscv64_clear_cache` symbol outside mcc).
- **`__unordtf2`** helper: clang `-O2` (unlike gcc) emits it for the
  NaN/unordered checks in the 128-bit `long double _Complex` routines;
  `f3_cmp` already returns the unordered sentinel `2` (`lib-arm64.c`).
- **win32 `fenv`.** The rounding-mode value is the C99 `FE_*` encoding, which
  matches the x87 control-word field (bits 10-11): `FE_TONEAREST` 0x000,
  `FE_DOWNWARD` 0x400, etc. On x86 set both the x87 CW and the SSE `MXCSR`
  (bits 13-14 == the x87 field `<< 3`) since scalar FP uses SSE while `long
  double` uses x87. On arm64 the mode is `FPCR` bits 23:22 (00 RN, 01 RP, 10 RM,
  11 RZ).
- **win32 `errno`.** The POSIX thread codes are not in msvcrt's set; the
  `<pthread.h>`/`<threads.h>` shims use the same values mingw does.
- **win32 `pthread`.** The control block is intentionally *leaked* rather than
  freed under the still-running thread; `pthread_self`/`equal` only need to be
  self-consistent for identity checks, so the thread id serves as the token.

### Tooling (`tools/`) — design & portability notes

- **`csttool.c`** is a pure-library harness proving slices B–E against synthetic
  trees before Weave 1; it links nothing from the compiler (invariant §0.3).
  Gates: hashing invariance (struct hash matches, trivia hash differs), symbol
  refs as node-ids (def↔use round-trip), D1d comment promotion (excluded from
  `H_s`, feeds `H_t`), and the D3/D5 template + content-addressed store dedup.
- **`bench.c`** (default-off; CI enables via `MCC_BENCH`; host-only): `ru_maxrss`
  is KiB on Linux but bytes on Darwin; `/proc` and `/sys` files report
  `st_size == 0` so they must be streamed; aarch64 has no model name so the CPU
  name is decoded from `MIDR` (implementer, part); CPUID leaf-1 ECX bit 31 means
  running under a hypervisor; `cl` rejects GNU probe flags (MSB8066) so its
  version is parsed from the banner; the JUnit parse is bounded so a later case's
  `<skipped>` can't leak in.
- **`ci.c`**: stage3 CRT lands in `usr/lib64` but the multilib driver looks in
  `usr/lib`, so it is mirrored; x86_64 sysroots are pre-fetched idempotently via
  a marker file; the dist flow is exactly `configure→build→install→(strip)→
  bench→package-dist` from `release.yml`; `MCC_BUILD_STRIP` is off for
  `dist-macos` (a full strip trips codesigning); `ctest` wants the JUnit path as
  a separate token (`--output-junit=…` is rejected); in the scratch/out cleanup,
  `stage == out` so `out` must never be removed; the `bundle-` asset prefix
  signals the archive that holds the others; the JUnit summarizer is
  self-contained so linux/qemu jobs still emit PASS/FAIL/SKIP.
- **`mccharness.c`**: with no gcc/clang consensus it does a 2-way check that
  matches *either* reference and never introduces a FAIL (impl-defined
  divergence); `curl -o` fails on a fresh tree (a docker volume masks this, local
  ctest hit it) so the dir is created first; the 1-slice arm64 suite is the hard
  guarantee, 2-slice needs cross+SDK and self-skips.
- **`machofat.c`** is a post-link combiner (a single mcc can't emit fat):
  page-alignment (arm64 16 KiB, others 4 KiB; `≥` the arch page size) keeps
  signatures/segments valid; it ad-hoc re-signs because Apple silicon rejects a
  mismatched signature / stale AMFI cache; `codesign` exit 127 means absent
  (skip), other nonzero means it ran and failed (surface it).
- **`objcheck.c`**: legacy `version_min` packs X.Y.Z at offset 8 vs
  `LC_BUILD_VERSION` `minos` at 12; slice offsets are per sub-arch for fat, single
  0 for thin; `--arch` restricts to the named sub-arch, else a later-slice
  mismatch still fails.
- **`build.c` / `bin2c.c`**: `build.c` bounds a `%s` so gcc
  `-Wformat-truncation` sees it fits; `bin2c.c` runs at build time so its input
  may be a build product (e.g. `libmccrt.a`).

### Test harness — directive & runner semantics (`tests/`)

- **`skipon=<cpu>/<os>[:reason]`** excludes exactly one cpu/os combination while
  running everywhere else — unlike the require-style gates, which are inclusive
  (`tests/exec/runner.c`).
- **`MCC_TEST_RUNEMU`** launches the *compiled exe* under an emulator (e.g.
  `qemu-aarch64 -L <sysroot>`) while the cross compiler itself runs natively with
  `MCC_TEST_SYSROOT` flags; only `run`-mode goldens apply (the `-run`/JIT modes
  need a native/foreign compiler).
- **CST fixtures keep their comments on purpose.** `tests/cst/**` and
  `tests/exec/preprocessor/comment.c` treat comments as parsed input (captured as
  trivia, folded into `H_t`, or the literal subject of the test), so those
  comments are payload, not decoration, and were left in place.
