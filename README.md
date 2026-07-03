# ModernCC (`mcc`)

A small, fast, portable C11 compiler in the [TinyCC](https://repo.or.cz/tinycc.git)
lineage. One-pass compile-and-link, in-memory execution (`-run`), embeddable as a
library (`libmcc`), and cross-compilation of 5 architectures × 3 object formats
from a single source tree. Trades an optimizer for speed, size, and portability;
`gcc`/`clang` remain the optimizing references.

| Features      |                                                                 |
|---------------|-----------------------------------------------------------------|
| **Targets**   | x86_64 · i386 · arm · arm64 · riscv64                           |
| **Formats**   | ELF · PE/COFF · Mach-O                                          |
| **Libc**      | glibc · musl (`--sysroot`) · msvcrt · libSystem                 |
| **Modes**     | compile+link · `-c` · `-S` (asm listing) · `-run` (JIT, no `a.out`) · `libmcc` C API |
| **Speed**     | single-pass (~100× faster to compile than `gcc -O2`)            |
| **Size**      | ~0.6 MB dynamic · ~1.3 MB static self-contained binary          |
| **Assembler** | integrated (`MCC_CONFIG_ASM`) · inline asm · `asm goto` · `-S` via built-in disassembler (x86_64) |
| **Safety**    | optional bounds checker (`-b`) and backtraces (`-bt`)           |
| **Cross**     | `mcc-<arch>` compilers via `MCC_ENABLE_CROSS`                   |

## Comparisons

`Y` = supported, `~` = partially supported, `-` = not supported.

| Target / format | mcc | gcc | clang | mingw | msvc |
|---|:--:|:---:|:--:|:-----:|:--:|
| x86_64                       | Y |  Y | Y |   Y   | Y |
| i386                         | Y |  Y | Y |   Y   | Y |
| arm                          | Y |  Y | Y |   ~   | Y |
| arm64                        | Y |  Y | Y |   Y   | Y |
| riscv64                      | Y |  Y | Y |   -   | - |
| ELF output                   | Y |  Y | Y |   -   | - |
| PE/COFF output               | Y |  Y | Y |   Y   | Y |
| Mach-O output                | Y | ~¹ | Y |   -   | - |
| Multi-target from one build  | Y |  Y | Y |   -   | - |

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

## Building / Configuration

CMake (≥ 3.22) with CMakePresets.json:

```sh
cmake --preset debug # or: release, asan, diagnostics, cross, matrix
cmake --build --preset debug -j
```

The developer presets above are the ones you'll use by hand; CI/dist presets
(`linux-*`, `macos*`, `msvc`, `mingw`, `qemu*`, `dist-*`) are also defined and
drive the workflows + docker runners. See [BUILD.md §2](BUILD.md) for the full
preset catalog and naming conventions.

**Linux status (2026-07, gcc 15.3 / clang 22):** every Linux preset is green —
`debug`, `release`, `asan`, `diagnostics`, `cross`, `matrix` (gcc/clang ×
native/cross superbuild), all 15 `linux-*` CI presets, both `dist-linux-*`
packagings, and the full `qemu` cross×libc matrix. Each test-bearing preset
passes its complete suite (37/37 portable tests; 22/22 in the qemu matrix,
all 5 arches × glibc+musl + `qemu-arm64-osx`). With the `cross` toolchain
built (`MCC_CROSS_DIR`, default `cmake-build-cross`), the wine PE-conformance
and the four host-runnable Mach-O drivers run natively and pass too.

or, CMake (without presets):

```sh
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
ccmake cmake-build-release
cmake --build cmake-build-release -j
```

| Option                 | Default | Meaning                                    |
|------------------------|:-------:|--------------------------------------------|
| `MCC_BUILD_TESTS`      |   ON    | Register the CTest suite                   |
| `MCC_CONFIG_ASM`       |   ON    | Integrated assembler                       |
| `MCC_CONFIG_BCHECK`    |   ON    | Bounds checker                             |
| `MCC_CONFIG_BACKTRACE` |   ON    | Runtime backtraces                         |
| `MCC_ENABLE_CROSS`     |   OFF   | Also build `mcc-<arch>` cross compilers    |
| `MCC_BUILD_STATIC_LIB` |   OFF   | Build static `libmcc-static.a` instead of shared `libmcc.so` |
| `MCC_BUILD_DYNAMIC_LIB`|   OFF   | Also build shared `libmcc-dynamic.so` alongside `libmcc-static.a` |
| `MCC_ONE_SOURCE`       |   ON    | Amalgamate the compiler into one TU (`mcc` is self-contained) |
| `MCC_BUILD_STATIC_EXE` |   OFF¹  | Also build `mcc-static` (fully static `-static`); enables `CONFIG_MCC_STATIC` so *its* `-run` resolves libc via a built-in symbol table |
| `MCC_BUILD_DYNAMIC_EXE`|   ON    | Also build `mcc-dynamic` (not one-source; links the shared `libmcc.so`) |
| `MCC_BUILD_MUSL`       |   OFF²  | Also build musl-targeting variants (`*-musl`, Linux only) |
| `MCC_BUILD_STRIP`      |   OFF   | Strip symbols during link                  |
| `MCC_QEMU_TESTS`       |   OFF   | qemu-user cross-conformance matrix (below) |

¹ Auto-forced OFF on macOS (no fully-static libc). The default `mcc` is a
dynamic, self-contained binary whose `-run` resolves the full libc surface via
`dlsym`; the opt-in `mcc-static` instead uses a built-in symbol table (common
symbols only). On Windows even a `-static` exe still reaches msvcrt.dll via
`LoadLibrary`, so `mcc-static` keeps the full dynamic `-run` resolution there
(no built-in table). ² Linux only (a no-op on macOS/Windows); opt-in, enabled only by
the explicit `-musl` presets/targets (`release`, `linux-gcc-musl`, `dist-linux-*`).

All compiler binaries follow one suffix convention:
`mcc[-<arch>][-static|-dynamic][-musl]` — arch first (cross targets only),
then the link/one-source shape, with `-musl` always last. `mcc` — the default,
installed binary — is a self-contained ONE_SOURCE build linked only to libc,
with no `libmcc.so` dependency. `mcc-static` is the same, statically linked
(`-static`). `mcc-dynamic` is a non-amalgamated driver linked against the shared
`libmcc.so`. Cross compilers (`MCC_ENABLE_CROSS`) are self-contained host
binaries, so they take `-static` (with `MCC_BUILD_STATIC_EXE`) but not
`-dynamic`. Each shape has a `-musl` sibling. Examples: `mcc`, `mcc-static`,
`mcc-dynamic`, `mcc-musl`, `mcc-arm64`, `mcc-x86_64-static`,
`mcc-arm64-musl`, `mcc-x86_64-static-musl`.

The `libmcc` libraries follow the same convention
(`libmcc[-static|-dynamic][-musl]`), except a lone shared library keeps the bare
`libmcc` default: the default build produces `libmcc.so`; `MCC_BUILD_STATIC_LIB`
gives `libmcc-static.a`; building both gives `libmcc-static.a` +
`libmcc-dynamic.so` (each with a `-musl` sibling).

## Usage

```sh
mcc hello.c -o hello          # compile + link
mcc -run hello.c              # compile and run in memory
mcc -c file.c -o file.o       # object only
mcc -S file.c                 # AT&T assembly listing (file.s)

# cross-compile against a sysroot (glibc or musl), then run under qemu-user:
mcc-arm64 --sysroot=/path/to/rootfs hello.c -o hello   # or mcc-arm64-musl
qemu-aarch64 -L /path/to/rootfs ./hello
```

## Testing

CTest, organized by mechanism. Inapplicable tests report **Skipped with a
reason** (e.g. `requires arm64 target`) rather than silently omitting, so gaps
stay visible.

```sh
ctest --test-dir cmake-build-debug
```

| Directory | Covers |
|---|---|
| `tests/exec/`        | Golden run-and-diff, by topic (statements, expressions, types, structs, preprocessor, …) |
| `tests/preprocess/`  | `-E` preprocessor-only (expansion, pasting, variadic, …) |
| `tests/diff/`        | Differential vs. a reference compiler (`full_language.c` + per-unit `parts/` wrappers) |
| `tests/diff3/`       | Three-way differential (mcc vs. gcc **and** clang) over the exec corpus |
| `tests/cli/`         | Driver/CLI cases (`cases.h`): flags, diagnostics, `readelf`/`nm` structural checks |
| `tests/embed/`       | `libmcc` embedding API (single- and multi-threaded) |
| `tests/diagnostics/` | Expected errors/warnings |
| `tests/asm/`         | Standalone assembler / asm↔C linkage |
| `tests/behavior/`    | Self-checking runtime drivers (FP, bounds, VLA) |
| `tests/qemu/`        | Cross-target conformance (matrix below) |

### Per-toolchain coverage

`P` = passes, `S` = skipped-with-reason (environment/config-gated, not a
failure), `—` = not applicable.

| `ctest` suite | Win mingw | Win gcc | Win msvc | Lin gcc | Lin clang | mac clang |
|---|:--:|:--:|:--:|:--:|:--:|:--:|
| `exec-suite` (golden run/diff)        | P | P | P | P | P | P |
| `mcctest`¹                            | P | P | P | P | P | P |
| `mcctest-bcheck`¹                     | S | S | S | P | P | P |
| `preprocess-suite`²                   | P | P | P | P | P | P |
| `diff3-suite`²                        | P | P | P | P | P | P |
| `parts-suite`² (per-unit 3-way diff)⁹ | S | S | S | P | P | P |
| `cli-suite` (readelf/nm structural)³  | P | P | P | P | P | P |
| `libtest` / `-extra` / `-mt`, `abitest-cc` | P | P | P | P | P | P |
| `hello-run` / `hello-exe`, `vla_test-run`  | P | P | P | P | P | P |
| `compile.*` (orphan `-c`)⁴            | P | P | P | P | P | P |
| `asm-c-connect-test`                  | P | P | P | P | P | S |
| `dash-s-roundtrip` (`-S` → asm → run)¹⁰ | P | P | P | P | P | S |
| `asm-gas-directives`⁵                 | S | S | S | S | S | S |
| `i386-fastcall-abi`⁶                  | S | S | S | P | P | S |
| `compile.win32.*` / `pe-native-conformance` | P | P | P | — | — | S |
| `pe-wine-conformance` (label `wine`)⁷ | S | S | S | P | P | S |
| `macho-*` (6 drivers, label `macho`)⁷ | S | S | S | P | P | P |
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
clang-toolchain` fetches a pinned, SHA256-verified LLVM into `cmake-clang/`,
auto-wired by the next reconfigure (both suites then run and pass).
³ Needs POSIX `sh` (`MCC_TEST_SH`) + `nm`/`readelf`; ~31 ELF-image cases
self-skip on a PE target.
⁴ X11 example (`compile.ex4`) skips when `<X11/Xlib.h>` is absent.
⁵ Always skipped: integrated assembler lacks a few GAS encodings
(`sgdtq`/`sidtq`/`swapgs`).
⁶ Needs the i386 cross compiler (`mcc-i386`, preset `cross`) + an ELF-emitting
32-bit reference cc; skips on Windows (mingw `gcc` emits PE/COFF).
⁷ Both need the cross toolchain (preset `cross`, or a populated
`MCC_CROSS_DIR` — default `cmake-build-cross`): wine + the win32 cross
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
macOS).

### Compile speed & footprint

Compiling `mcc`'s own whole-compiler TU (`src/mcc.c`, `ONE_SOURCE=1`) to an
object, best of 3 (gcc 15.3 / clang 22, 2026-07). Stripped release binaries:
dynamic `mcc` ≈ **0.6 MB**, `mcc-static` ≈ **1.3 MB**.

| Compiler | Time | vs mcc |
|---|--:|--:|
| **mcc**       | **0.05 s** | 1× |
| clang `-O0`   | 0.36 s | 7× slower |
| gcc `-O0`     | 0.97 s | 19× slower |
| clang `-O2`   | 5.40 s | 108× slower |
| gcc `-O2`     | 7.03 s | 141× slower |

## Cross-target × libc matrix (qemu-user)

`-DMCC_QEMU_TESTS=ON` exercises every arch × both C libraries. For each
`(arch, libc)` it fetches a minimal **Gentoo stage3** rootfs, cross-compiles
`tests/qemu/conformance/` against that sysroot, and runs it under
`qemu-<arch> -L <rootfs>`. Opt-in and offline-by-default; the normal `ctest` run
is unaffected.

```sh
cmake --preset cross                       # native + cross compilers
cmake -DMCC_QEMU_TESTS=ON cmake-build-cross
cmake --build cmake-build-cross -j
ctest --test-dir cmake-build-cross -L qemu
```

| Arch | glibc | musl |
|---|:---:|:---:|
| x86_64  | Y | Y |
| i386    | Y | Y |
| arm     | Y | Y |
| arm64   | Y | Y |
| riscv64 | Y | Y |

Each combo is built twice (default codegen and `-fPIC -pie`); each program
self-checks (integers, floats, complex Annex-G, aggregates/ABI, varargs,
atomics, libc, lexical, control flow) and exits non-zero on first failure, so it
is independent of libc output formatting. `qemu-arm64-osx` additionally covers
arm64 **Darwin** codegen linked against arm64 glibc. Host arch uses native `mcc`
(no cross build). Cache-driven (`MCC_QEMU_ARCHS`, `MCC_QEMU_LIBCS`,
`MCC_QEMU_MIRROR`, `MCC_QEMU_DLDIR`); combos lacking a cross compiler or
`qemu-<arch>` report a skip reason.

Point `MCC_QEMU_DLDIR` at prefetched `<arch>-<libc>/` sysroots (each with a
`.fetched` marker) to skip the download:

```sh
cmake --preset cross -DMCC_QEMU_TESTS=ON -DMCC_QEMU_DLDIR=/path/to/qemu-roots
cmake --build cmake-build-cross -j
ctest --test-dir cmake-build-cross -L qemu --output-on-failure
```

### Off-Linux (Docker)

macOS and any host without user-mode QEMU run the same matrix via the
containerized runner in `tests/qemu/docker/` — it supplies the `qemu-<arch>`
emulators and a Linux toolchain, caching sysroots in a named volume:

```sh
docker build -t mcc-qemu tests/qemu/docker
docker run --rm -v "$PWD":/work -v mcc-qemu-roots:/qemu-roots mcc-qemu
# narrow the grid: -e ARCHS=x86_64 -e LIBCS=glibc, or pass ctest flags (-R …)
```

It builds `mcc` + cross compilers from scratch, then runs `ctest -L qemu`
against the mounted sysroots. Bind-mounting prebuilt `/qemu-roots` avoids
re-downloading.

### CI and labels

Stable labels let a CI host select what it supports.

| Label | Selects | Needs |
|---|---|---|
| `qemu`  | cross-conformance matrix | qemu-user + per-arch glibc/musl sysroots |
| `wine`  | PE/Windows runtime conformance | win32 cross compilers + `wine` |
| `macho` | Mach-O structural + self-contained image/codegen | native host (some need Darwin/darling) |

```sh
ctest --test-dir <build> -L qemu                 # just the qemu matrix
ctest --test-dir <build> -LE 'qemu|wine|macho'   # everything else (portable)
```

`.github/workflows/ci.yml` runs the portable suites and the qemu matrix (via the
Docker runner) on every push.

**Mach-O by host.** macOS: `macho-conformance-native` compiles the full
self-checking set with native `mcc` and runs each as a real Mach-O image against
system libSystem — the most direct coverage (subsumes TLS and libc-dependent
programs). Linux/x86_64: structural, self-contained-image, codegen and
apple-libc drivers approximate the same codegen via a loader/trampoline; off
x86_64 they **Skip**, not hollow-pass. The remaining libSystem/dyld path
(libmalloc, locale stdio, dyld, pthread/GCD, ObjC) is kernel-fused and needs a
macOS or **darling** host (`-DMCC_DARWIN_HOST=ON`); intentionally outside the
default matrix.

## Repository layout

```
src/        compiler + libmcc (incl. src/arch/<cpu>, src/objfmt)
include/    public headers (libmcc.h)
runtime/    runtime support and bundled headers
examples/   small example programs
tests/      ctest suite (see tables above)
tools/      build/dev helpers
```

## License

Derived from TinyCC by Fabrice Bellard and contributors; distributed under the
**GNU Lesser General Public License v2.1** (LGPL-2.1).
