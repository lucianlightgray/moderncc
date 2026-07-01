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
| **Modes**     | compile+link · `-c` · `-run` (JIT, no `a.out`) · `libmcc` C API |
| **Speed**     | single-pass (~100× faster to compile than `gcc -O2`)            |
| **Size**      | ~1 MB self-contained binary                                     |
| **Assembler** | integrated (`MCC_CONFIG_ASM`) · inline asm · `asm goto`         |
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
cmake --build cmake-build-debug -j
```

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
| `MCC_BUILD_STATIC_LIB` |   ON    | Build static libmcc library                |
| `MCC_BUILD_DYNAMIC_LIB`|   OFF   | Build shared libmcc library                |
| `MCC_BUILD_STATIC_EXE` |   ON¹   | Link executable(s) fully static (`-static`); enables `CONFIG_MCC_STATIC` so `-run` resolves libc via a built-in symbol table |
| `MCC_BUILD_DYNAMIC_EXE`|   OFF   | Also build self-contained `mcc-dynamic`, linked only to libc |
| `MCC_BUILD_MUSL`       |   ON²   | Also build musl-targeting variants (`*-musl`, Linux only) |
| `MCC_BUILD_STRIP`      |   OFF   | Strip symbols during link                  |
| `MCC_QEMU_TESTS`       |   OFF   | qemu-user cross-conformance matrix (below) |

¹ The `debug`/`asan`/`diagnostics` presets keep it OFF (dynamic) so the full
test suite can `-run` the whole libc surface; auto-forced OFF on macOS (no
fully-static libc). ² Linux only; a no-op on macOS/Windows.

Compiler binaries follow `mcc-<arch>[-dynamic][-musl]`: `<arch>` for cross
targets, `-musl` for the musl-targeting variant. `mcc` is a fully static,
self-contained binary; `mcc-dynamic` is a self-contained (ONE_SOURCE) build
dynamically linked only to libc (no `libmcc.so` dependency). The default host
build is plain `mcc`. Examples: `mcc`, `mcc-dynamic`, `mcc-musl`, `mcc-arm64`,
`mcc-arm64-musl`.

## Usage

```sh
mcc hello.c -o hello          # compile + link
mcc -run hello.c              # compile and run in memory
mcc -c file.c -o file.o       # object only

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
| `tests/diff/`        | Differential vs. a reference compiler (`full_language.c`) |
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
| `mcctest` / `mcctest-bcheck`¹         | P | P | P | P | P | P |
| `preprocess-suite`²                   | P | P | P | P | P | P |
| `diff3-suite`²                        | P | P | P | P | P | P |
| `cli-suite` (readelf/nm structural)³  | P | P | P | P | P | P |
| `libtest` / `-extra` / `-mt`, `abitest-cc` | P | P | P | P | P | P |
| `hello-run` / `hello-exe`, `vla_test-run`  | P | P | P | P | P | P |
| `compile.*` (orphan `-c`)⁴            | P | P | P | P | P | P |
| `asm-c-connect-test`                  | P | P | P | P | P | S |
| `asm-gas-directives`⁵                 | S | S | S | S | S | S |
| `i386-fastcall-abi`⁶                  | S | S | S | P | P | S |
| `compile.win32.*` / `pe-native-conformance` | P | P | P | — | — | S |
| `pe-wine-conformance` (label `wine`)  | S | S | S | P | P | S |
| `macho-*` (5 drivers, label `macho`)⁷ | S | S | S | P | P | P |
| qemu cross×libc matrix (label `qemu`)⁸| S | S | S | P | P | S |

¹ Differential vs. a GCC-compatible reference cc (needs the integrated
assembler); MSVC host auto-detects a mingw/winlibs `gcc` (`MCC_REF_CC`).
`-bcheck` variant also needs `MCC_CONFIG_BCHECK`.
² Needs **two distinct** references (gcc *and* clang) or the three-way
differential self-skips. On macOS `gcc`/`cc` are the Apple clang shim, so a
genuine Homebrew `gcc-<n>` (installed by `setup-gcc`) is auto-detected.
³ Needs POSIX `sh` (`MCC_TEST_SH`) + `nm`/`readelf`; ~31 ELF-image cases
self-skip on a PE target.
⁴ X11 example (`compile.ex4`) skips when `<X11/Xlib.h>` is absent.
⁵ Always skipped: integrated assembler lacks a few GAS encodings
(`sgdtq`/`sidtq`/`swapgs`).
⁶ Needs an ELF-emitting 32-bit reference cc; skips on Windows (mingw `gcc`
emits PE/COFF).
⁷ Linux: four host-runnable drivers pass (`macho-structural`,
`macho-codegen-run`, `macho-image-run`, `macho-apple-libc`),
`macho-conformance-native` skips (needs Darwin/darling). macOS:
`macho-structural` + `macho-conformance-native` are native; Linux-approximation
drivers self-skip off x86_64.
⁸ Windows runs it via the Docker runner; a Linux host with `qemu-user` runs it
natively (`ctest -L qemu`).

### Compile speed & footprint

Compiling `mcc`'s own whole-compiler TU (`src/mcc.c`, `ONE_SOURCE=1`) to an
object, best of 3. Static `mcc` binary ≈ **1.3 MB**.

| Compiler | Time | vs mcc |
|---|--:|--:|
| **mcc**       | **0.05 s** | 1× |
| clang `-O0`   | 0.31 s | 6× slower |
| gcc `-O0`     | 0.83 s | 17× slower |
| clang `-O2`   | 4.69 s | 94× slower |
| gcc `-O2`     | 5.58 s | 112× slower |

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
