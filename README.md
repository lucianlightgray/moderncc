# ModernCC (`mcc`)

A small, fast C compiler derived from [TinyCC](https://repo.or.cz/tinycc.git),
modernized around C99/C11 and a CMake build. `mcc` compiles and links C in one
pass, can run programs directly without writing an executable (`-run`), and is
also usable as an embeddable compiler library (`libmcc`).

It is **single-pass and ~100× faster to compile than optimizing `gcc`/`clang`**,
ships as a **~1 MB** self-contained binary, targets **five CPU architectures ×
three object formats (ELF / PE / Mach-O) from one source tree**, and links
against **both glibc and musl**. See [How `mcc` compares](#how-mcc-compares) and
the [local verification run](#local-verification-run-2026-07-01-gentoo-x86_64)
(every applicable test green across gcc/clang builds, all 5 arches × glibc/musl
under qemu, PE under wine, and Mach-O).

Version: `0.9.28rc`.

## Features

- Multi-target code generation: **x86_64, i386, ARM, AArch64, RISC-V 64**
  (plus Windows/PE and macOS/Mach-O variants of several of these).
- Both **glibc** and **musl** C libraries are supported via `--sysroot`; the
  ELF interpreter is derived from the sysroot (musl loader auto-selected).
- Integrated assembler (`MCC_CONFIG_ASM`), inline asm, and `asm goto`.
- Optional runtime safety: bounds checking (`MCC_CONFIG_BCHECK`) and
  backtraces (`MCC_CONFIG_BACKTRACE`).
- `-run` to compile-and-execute in memory; `libmcc` C API for embedding.
- Cross compilers (`<arch>-mcc`) buildable from one tree.

## How `mcc` compares

Where `mcc` sits next to the mainstream C toolchains. `mcc` trades an
optimizer for speed, a tiny footprint, in-memory execution, embeddability, and
multi-target codegen out of a single source tree — the TinyCC lineage — while
`gcc`/`clang` remain the optimizing, standards-complete references.

Legend: `Y` = built-in / supported, `~` = partial or via an add-on (a
sanitizer, an external assembler, a separate per-target toolchain), `-` = not
supported.

**Targets and object formats**

| Target / format | mcc | tcc¹ | gcc | clang | mingw² | msvc |
|---|:--:|:--:|:--:|:--:|:--:|:--:|
| x86_64                       | Y | Y | Y | Y | Y  | Y |
| i386                         | Y | Y | Y | Y | Y  | Y |
| arm                          | Y | Y | Y | Y | ~  | Y |
| arm64                        | Y | Y | Y | Y | Y  | Y |
| riscv64                      | Y | Y | Y | Y | -  | - |
| ELF output                   | Y | Y | Y | Y | -  | - |
| PE/COFF output               | Y | Y | ~³| Y | Y  | Y |
| Mach-O output                | Y | ~ | ~⁴| Y | -  | - |
| Multi-target from one build  | Y | Y | - | Y | -  | - |

¹ `tcc` = TinyCC, `mcc`'s upstream — shown for lineage.
² `mingw` is GCC packaged to emit Windows PE; a GCC at heart.
³ `gcc` emits PE only as a mingw/Cygwin build.
⁴ a native Apple `gcc` can, but mainline `gcc` → Mach-O is rare.

**Capabilities**

| Capability | mcc | tcc | gcc | clang | mingw | msvc |
|---|:--:|:--:|:--:|:--:|:--:|:--:|
| Compile + link in one step          | Y | Y | Y | Y | Y | Y  |
| `-run` (execute in memory, no a.out)| Y | Y | - | - | - | -  |
| Embeddable compiler library         | Y | Y | ~⁵| ~⁶| - | -  |
| Integrated assembler (no external `as`)| Y | Y | - | Y | - | Y  |
| Inline asm / `asm goto`             | Y | Y | Y | Y | Y | ~⁷ |
| Built-in bounds checker (`-b`)      | Y | Y | ~⁸| ~⁸| ~⁸| ~⁸ |
| Runtime backtraces (`-bt`)          | Y | Y | ~ | ~ | ~ | ~  |
| glibc + musl via `--sysroot`        | Y | Y | Y | Y | - | -  |
| Optimizing codegen                  | - | - | Y | Y | Y | Y  |
| C99                                 | Y | Y | Y | Y | Y | Y  |
| C11                                 | Y | ~ | Y | Y | ~ | Y  |
| Single-pass / fast compile          | Y | Y | - | - | - | -  |
| Tiny footprint (~1 MB class)        | Y | Y | - | - | - | -  |

⁵ via `libgccjit`. ⁶ via the LLVM C API / `libclang` (a much larger dependency).
⁷ MSVC dropped inline `asm` on x64 (intrinsics only). ⁸ via sanitizers
(`-fsanitize=address,bounds`, `/RTC`), not a built-in `-b` flag.

### Toolchains and C libraries

Two distinct axes are often conflated — keep them separate:

**Host compilers that *build* `mcc`** (selectable via `MCC_TOOLCHAIN_PROFILE`;
the matrix superbuild can drive several at once). Because `mcc`'s codegen is
deterministic (the 3-stage self-host reaches a byte-identical fixpoint), the
compiled-program test results are independent of which of these built `mcc`:

| Builds `mcc` | Status | Notes |
|---|:--:|---|
| **gcc**   | Y | primary; `debug`/`release`/`diagnostics`/`asan` all green |
| **clang** | Y | green; also self-hosts |
| **mingw-w64** (gcc→PE) | Y | for a Windows `mcc.exe`; `MCC_TOOLCHAIN_PROFILE=mingw` |
| **MSVC** (`cl`) | Y | Visual Studio generator; green on Win |
| **`mcc` itself** | Y | self-hosting to a byte-identical fixpoint |

**C libraries / runtimes `mcc` *targets*** (chosen per target OS, or via
`--sysroot` for the ELF pair):

| Target libc | Via | Verified |
|---|---|:--:|
| **glibc** (ELF) | `--sysroot` / native | Y — all 5 arches under qemu |
| **musl** (ELF)  | `--sysroot` (loader auto-selected) | Y — all 5 arches under qemu |
| **msvcrt** (PE) | Windows/PE target | Y — under wine + native Win |
| **libSystem** (Mach-O) | macOS/Darwin target | Y (structural/codegen/image/apple-libc); full libSystem needs a Darwin host |

Every `(host-compiler) × (target libc)` combination the host can reach is
exercised in the [local verification run](#local-verification-run-2026-07-01-gentoo-x86_64)
below.

## Building

The project uses CMake (≥ 3.22) with presets:

```sh
cmake --preset debug          # or: release, asan, diagnostics, cross, matrix
cmake --build cmake-build-debug -j
```

Plain CMake works too:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Useful options (defaults in parentheses):

| Option | Default | Meaning |
|---|---|---|
| `MCC_BUILD_TESTS` | ON | Register the ctest suite |
| `MCC_CONFIG_ASM` | ON | Integrated assembler |
| `MCC_CONFIG_BCHECK` | ON | Bounds checker |
| `MCC_CONFIG_BACKTRACE` | ON | Runtime backtraces |
| `MCC_ENABLE_CROSS` | OFF | Also build `<arch>-mcc` cross compilers |
| `MCC_QEMU_TESTS` | OFF | qemu-user cross-conformance matrix (see below) |

## Usage

```sh
mcc hello.c -o hello          # compile + link
mcc -run hello.c              # compile and run in memory
mcc -c file.c -o file.o       # object only

# cross-compile against a sysroot (glibc or musl), then run under qemu-user:
arm64-mcc --sysroot=/path/to/rootfs hello.c -o hello
qemu-aarch64 -L /path/to/rootfs ./hello
```

## Testing

Tests are run through CTest. The suite is organized by *mechanism*:

| Directory | What it covers |
|---|---|
| `tests/exec/` | Golden run-and-diff suite, by topic (statements, expressions, types, structs, preprocessor, …) |
| `tests/preprocess/` | `-E` preprocessor-only tests (expansion, pasting, variadic, …) |
| `tests/diff/` | Differential test vs. a reference compiler (`full_language.c`) |
| `tests/embed/` | `libmcc` embedding API (single- and multi-threaded) |
| `tests/diagnostics/` | Expected errors/warnings |
| `tests/asm/` | Standalone assembler / asm↔C linkage |
| `tests/behavior/` | Self-checking runtime drivers (FP, bounds, VLA) |
| `tests/qemu/` | Cross-target conformance programs (see matrix below) |

Run everything:

```sh
ctest --test-dir cmake-build-debug
```

Tests that don't apply to the host/config are reported as **Skipped with a
reason** (e.g. `requires arm64 target`, `requires MCC_CONFIG_BCHECK`) rather
than silently omitted, so coverage gaps are visible.

### Per-toolchain results

The `ctest` suites against each host/toolchain, now filled on all three OSes.
The **Linux gcc / Linux clang** columns are from the local verification run
below (Gentoo x86_64, gcc 15.3 / clang 22, with the cross build so
qemu/wine/Mach-O are all exercised). The **Windows** columns and the Docker
qemu matrix are from earlier local runs. The **`mac clang`** column is a macOS
26 / arm64 host (Apple clang 21) with `-DMCC_DIAGNOSTICS=ON -DMCC_ENABLE_CROSS=ON`:
`34/34`, 0 fail, clean build. The same tree also builds clean with a Homebrew
GNU `gcc` (`-DCMAKE_C_COMPILER=gcc-<n>`; `setup-gcc` installs one) — `34/34`,
0 fail — which supplies the distinct second reference for `diff3`/`preprocess`
below (see note ⁴).

Legend: `P` = pass, `S` = skipped-with-reason here (environment/config-gated,
not a failure), `—` = not applicable, blank = not yet recorded on that
machine/toolchain.

| `ctest` suite | Win mingw¹ | Win gcc¹ | Win msvc¹ | Lin gcc | Lin clang | mac clang | docker² |
|---|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| `exec-suite` (golden run/diff)        | P | P | P | P | P | P | — |
| `mcctest` / `mcctest-bcheck`³         | P | P | P | P | P | P | — |
| `preprocess-suite`⁴                   | P | P | P | P | P | P | — |
| `diff3-suite`⁴                        | P | P | P | P | P | P | — |
| `cli-suite` (readelf/nm structural)⁵  | P | P | P | P | P | P | — |
| `libtest` / `-extra` / `-mt`, `abitest-cc` | P | P | P | P | P | P | — |
| `hello-run` / `hello-exe`, `vla_test-run`  | P | P | P | P | P | P | — |
| `compile.*` (orphan `-c`)⁶            | P | P | P | P | P | P | — |
| `asm-c-connect-test`                  | P | P | P | P | P | S | — |
| `asm-gas-directives`⁷                 | S | S | S | S | S | S | — |
| `i386-fastcall-abi`⁸                  | S | S | S | P | P | S | — |
| `compile.win32.*` / `pe-native-conformance` | P | P | P | — | — | S | — |
| `pe-wine-conformance` (label `wine`)  | S | S | S | P | P | S | — |
| `macho-*` (5 drivers, label `macho`)  | S | S | S | P¹⁰ | P¹⁰ | Pª | — |
| qemu cross×libc matrix (label `qemu`)⁹| S | S | S | P | P | S | P |

¹ **Win mingw** = CLion's bundled mingw GCC 13.1; **Win gcc** = winlibs
mingw-w64 GCC 16.1 (the `mingw-toolchain` download); **Win msvc** = VS 2026
`cl` 19.51 (Visual Studio generator). All three are green (`37/37` with
`-DMCC_DIAGNOSTICS=ON`, 0 fail); every `S` below is environment-gated, not a
Windows bug.
² **docker** = the `tests/qemu/docker` runner, which builds the cross compilers
and runs `ctest -L qemu` only — so it fills just the qemu row (per-cell grid in
the table below). A native Windows host has no user-mode qemu, so that row is
`S` there.
³ Differential vs. a GCC-compatible reference cc (needs the integrated
assembler); the MSVC host auto-detects a mingw/winlibs `gcc` reference
(`MCC_REF_CC`). `-bcheck` variant also needs `MCC_CONFIG_BCHECK`.
⁴ Needs **two distinct** reference compilers (gcc *and* clang) — else the
three-way differential degenerates and self-skips. On Windows, build the
self-contained `clang-toolchain`. On macOS, `gcc`/`cc` are the Apple clang
shim, so the build auto-detects a genuine Homebrew GNU `gcc` (`gcc-<n>`, which
`setup-gcc` installs) as the distinct second reference; a POSIX `sh` (native on
macOS) drives `diff3`.
⁵ Needs a POSIX `sh` (`MCC_TEST_SH`) plus mingw `nm`/`readelf` on `PATH`; the
~31 ELF-image cases self-skip on a PE target.
⁶ The X11 example (`compile.ex4`) skips when `<X11/Xlib.h>` is absent.
⁷ Unconditional skip everywhere: the integrated assembler lacks a few GAS
encodings (`sgdtq`/`sidtq`/`swapgs`).
⁸ Skips on Windows: the test mixes mcc objects (ELF) with reference-cc objects,
but mingw `gcc` emits PE/COFF — needs an ELF-emitting 32-bit reference cc. On
Linux (with the cross build) it **passes**.
⁹ See the per-cell grid below; on Windows the matrix runs via Docker. On a Linux
host with `qemu-user` it runs natively (`ctest -L qemu`) — see the local run
below, where all 11 combos passed.
¹⁰ On a Linux host, `P` = the four host-runnable Mach-O drivers pass
(`macho-structural`, `macho-codegen-run`, `macho-image-run`, `macho-apple-libc`);
only `macho-conformance-native` skips, as it needs a real Darwin/`darling` host.
ª On a **macOS host** the `macho` label is real native coverage:
`macho-structural` and `macho-conformance-native` (the full self-checking
conformance set built with the native `mcc` and run as real Mach-O images
against libSystem) **pass**; the Linux-approximation drivers
(`macho-codegen/image/apple-libc-run`) self-skip off x86_64, and
`macho-libsystem-kernel-fused` needs a darling host. On non-Darwin hosts the
whole label is `S`.

### Local verification run (2026-07-01, Gentoo x86_64)

A full local sweep on one Linux host with the complete toolchain stack. **Every
applicable suite passed; 0 failures.** The skips are all environment-gated
(cross/emulator/other-OS not present in that particular build dir), never bugs.

Environment: `mcc 0.9.28rc`, **gcc 15.3.0**, **clang 22.1.8**, mingw-w64
**gcc 15.2.0**, **wine 10.0 (Proton)**, **qemu-user 10.2.2** (i386/arm/aarch64/
riscv64/x86_64), binutils ld 2.46.

**Native builds** — configured with the built-in library search (no explicit
`--crtprefix`), all `ctest`:

| Build | Toolchain | Result |
|---|---|---|
| `debug`         | gcc   | **34/34 pass**, 0 fail |
| `release`       | gcc   | **34/34 pass**, 0 fail |
| `diagnostics`   | gcc   | **34/34 pass**, 0 fail (max warnings + sanitizer/coverage/profile variants) |
| `asan`          | gcc   | **34/34 pass**, 0 fail (ASan/UBSan) |
| `diagnostics`   | clang | **34/34 pass**, 0 fail |

A plain native build skips the cross-arch / PE / Mach-O drivers (no cross
compilers in that dir). The **cross build** (`-DMCC_ENABLE_CROSS=ON`, all 11
`<target>-mcc` compilers) fills them in — its full `ctest` passes with only 5
skips, all inherently host-gated: `asm-gas-directives` (a few GAS encodings the
integrated assembler omits), `compile.win32` + `pe-native-conformance` (native
Windows only), `macho-conformance-native` (native Darwin only), and
`macho-libsystem-kernel-fused` (`-DMCC_DARWIN_HOST=ON`). `i386-fastcall-abi`,
`pe-wine-conformance`, and the four host-runnable Mach-O drivers all **pass**.

**Cross-target × libc conformance under qemu-user** (`ctest -L qemu`, run against
prebuilt glibc/musl sysroots) — the self-checking `tests/qemu/conformance/`
programs, each built twice (default + `-fPIC -pie`):

| Arch | qemu | glibc | musl |
|---|---|:--:|:--:|
| x86_64  | `qemu-x86_64`  | **P** | **P** |
| i386    | `qemu-i386`    | **P** | **P** |
| arm     | `qemu-arm`     | **P** | **P** |
| arm64   | `qemu-aarch64` | **P** | **P** |
| riscv64 | `qemu-riscv64` | **P** | **P** |

Plus `qemu-arm64-osx` (arm64 **Darwin** codegen, linked against arm64 glibc and
run under `qemu-aarch64`) — **P**. Total: **11/11 qemu tests pass, 0 fail.**

The whole cross matrix was run twice — once with a **gcc-built** `mcc` and once
with a **clang-built** `mcc` — and both gave **identical results** (qemu 11/11,
wine, Mach-O 4/5; `ctest -L 'qemu|wine|macho'` = 28/28 including fetch fixtures),
exactly as the deterministic self-host fixpoint predicts.

**PE/Windows via wine:** `pe-wine-conformance` **passes** — mcc cross-compiles
the conformance set to real PE for `x86_64-win32` *and* `i386-win32` (msvcrt
libc, varargs, etc.) and runs each under `wine`.

**Mach-O:** the four host-runnable drivers pass — `macho-structural` (otool
parse), `macho-codegen-run`, `macho-image-run` (in-repo loader), and
`macho-apple-libc` (Apple's vendored libc sources compiled through mcc's Darwin
codegen). The linked binaries are genuine Mach-O (`cf fa ed fe`). Only the
native-Darwin driver skips.

**Docker:** the `tests/qemu/docker` runner also **passes** here. `docker build
-t mcc-qemu tests/qemu/docker` produces a Debian image with `qemu-user` + a
Linux toolchain; the container then stages the source, builds `mcc` and the
cross compilers from scratch, and runs `ctest -L qemu` against the mounted
sysroots — `qemu-x86_64-glibc`, `qemu-arm64-glibc`, and `qemu-arm64-osx` all
**Passed** (bind-mounting prebuilt `/qemu-roots` avoids re-downloading; without
it the runner fetches Gentoo stage3 rootfs itself). This is the same matrix that
ran natively above — it exists so a host *without* native `qemu-user` (e.g.
macOS) can run it too.

**Compile speed & footprint** (single-pass, no optimizer). Compiling mcc's own
whole-compiler translation unit (`src/mcc.c`, `ONE_SOURCE=1` — the entire
compiler as one large TU) to an object, best of 3:

| Compiler | Time | vs mcc |
|---|--:|--:|
| **mcc**       | **0.05 s** | 1× |
| clang `-O0`   | 0.31 s | 6× slower |
| gcc `-O0`     | 0.83 s | 17× slower |
| clang `-O2`   | 4.69 s | 94× slower |
| gcc `-O2`     | 5.58 s | 112× slower |

The `mcc` binary is **~0.95 MB**; gcc's `cc1` alone is ~43 MB. `mcc` trades the
optimizer for this: it emits correct, unoptimized code in a single pass, which
is why it is the fast/tiny/embeddable end of the spectrum and gcc/clang remain
the optimizing references.

## Cross-target × libc coverage (qemu-user matrix)

`-DMCC_QEMU_TESTS=ON` exercises the compiler across every architecture and both
C libraries. For each `(arch, libc)` it downloads a minimal **Gentoo stage3**
rootfs, cross-compiles the self-checking programs in `tests/qemu/conformance/`
against that sysroot, and runs them under `qemu-<arch> -L <rootfs>`.

```sh
cmake --preset cross                       # build native + cross compilers
cmake -DMCC_QEMU_TESTS=ON cmake-build-cross
cmake --build cmake-build-cross -j
ctest --test-dir cmake-build-cross -L qemu
```

The matrix is opt-in and offline-by-default; the normal `ctest` run is
unaffected. Everything is cache-driven (`MCC_QEMU_ARCHS`, `MCC_QEMU_LIBCS`,
`MCC_QEMU_MIRROR`, `MCC_QEMU_DLDIR`). Combos lacking a cross compiler or a
`qemu-<arch>` binary report a skip reason. The host arch uses the native `mcc`
(no cross build needed).

To skip the download, point `MCC_QEMU_DLDIR` at a directory that already holds
`<arch>-<libc>/` sysroots (each with a `.fetched` marker) — this is how the
[local run](#local-verification-run-2026-07-01-gentoo-x86_64) reused prebuilt
glibc/musl roots and got all 10 `(arch, libc)` combos to run:

```sh
cmake --preset cross -DMCC_QEMU_TESTS=ON -DMCC_QEMU_DLDIR=/path/to/qemu-roots
cmake --build cmake-build-cross -j
ctest --test-dir cmake-build-cross -L qemu --output-on-failure
```

Covered combinations:

| Arch | glibc | musl |
|---|:---:|:---:|
| x86_64  | Y | Y |
| i386    | Y | Y |
| arm     | Y | Y |
| arm64   | Y | Y |
| riscv64 | Y | Y |

Each combo compiles and runs the self-checking programs in
`tests/qemu/conformance/` (integers, floats, complex Annex-G multiply/divide,
aggregates/ABI, varargs, atomics, libc, lexical, control flow), built twice
(default codegen and `-fPIC -pie`); each program self-checks and exits non-zero
on the first failure, so it is independent of libc output formatting.

### Running the matrix off Linux (Docker)

macOS (and any host without user-mode QEMU) can run the same matrix via the
containerized runner in `tests/qemu/docker/` — it supplies the `qemu-<arch>`
emulators and a Linux toolchain, caching the sysroots in a named volume:

```sh
docker build -t mcc-qemu tests/qemu/docker
docker run --rm -v "$PWD":/work -v mcc-qemu-roots:/qemu-roots mcc-qemu
# narrow the grid: -e ARCHS=x86_64 -e LIBCS=glibc, or pass ctest flags (-R …)
```

### CI and test labels

CTests carry stable labels so a CI host can select what its tooling supports:

| Label | Selects | Needs |
|---|---|---|
| `qemu`  | cross-conformance matrix | qemu-user + per-arch glibc/musl sysroots |
| `wine`  | PE/Windows runtime conformance | the win32 cross compilers + `wine` |
| `macho` | Mach-O structural + self-contained image/codegen | native host (some need a Darwin/darling host) |

```sh
ctest --test-dir <build> -L qemu          # just the qemu matrix
ctest --test-dir <build> -LE 'qemu|wine|macho'   # everything else (portable)
```

`.github/workflows/ci.yml` runs the portable suites and the qemu matrix on
every push (the latter via the Docker runner above).

**Mach-O coverage by host.** On a **macOS host**, `macho-conformance-native`
compiles the full self-checking conformance set with the native `mcc` and runs
each as a real Mach-O image against the system libSystem — the most direct
coverage available (it subsumes the TLS and libc-dependent programs the Linux
approximations must exclude). On a **Linux/x86_64 host**, the structural,
self-contained-image, codegen and apple-libc drivers approximate the same
codegen via a loader/trampoline; off x86_64 (e.g. arm64 macOS, where the native
test covers it instead) they report **Skipped**, not a hollow pass. The
remaining libSystem/dyld-dependent path (libmalloc, locale stdio, dyld,
pthread/GCD, ObjC) is kernel-fused and needs a macOS or **darling** host
(`-DMCC_DARWIN_HOST=ON`); it is intentionally outside the default matrix.

## Repository layout

```
src/        compiler + libmcc (incl. src/arch/<cpu>, src/objfmt)
include/    public headers (libmcc.h)
runtime/    runtime support and bundled headers
examples/   small example programs
tests/      ctest suite (see table above)
tools/      build/dev helpers
```

## License

`mcc` derives from TinyCC by Fabrice Bellard and contributors, and is
distributed under the **GNU Lesser General Public License v2.1** (LGPL-2.1).
