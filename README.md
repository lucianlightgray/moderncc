# ModernCC (`mcc`)

A small, fast C compiler derived from [TinyCC](https://repo.or.cz/tinycc.git),
modernized around C99/C11 and a CMake build. `mcc` compiles and links C in one
pass, can run programs directly without writing an executable (`-run`), and is
also usable as an embeddable compiler library (`libmcc`).

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

A standing scaffold of the `ctest` suites against each host/toolchain. The
Windows columns and the Docker qemu matrix are filled from local runs; the
Linux and macOS columns are **left blank for other machines to record** (drop
in `P`/`S` as you run them).

Legend: `P` = pass, `S` = skipped-with-reason here (environment/config-gated,
not a failure), `—` = not applicable, blank = not yet recorded on that
machine/toolchain.

| `ctest` suite | Win mingw¹ | Win gcc¹ | Win msvc¹ | Lin gcc | Lin clang | mac clang | docker² |
|---|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| `exec-suite` (golden run/diff)        | P | P | P |   |   |   | — |
| `mcctest` / `mcctest-bcheck`³         | P | P | P |   |   |   | — |
| `preprocess-suite`⁴                   | P | P | P |   |   |   | — |
| `diff3-suite`⁴                        | P | P | P |   |   |   | — |
| `cli-suite` (readelf/nm structural)⁵  | P | P | P |   |   |   | — |
| `libtest` / `-extra` / `-mt`, `abitest-cc` | P | P | P |   |   |   | — |
| `hello-run` / `hello-exe`, `vla_test-run`  | P | P | P |   |   |   | — |
| `compile.*` (orphan `-c`)⁶            | P | P | P |   |   |   | — |
| `asm-c-connect-test`                  | P | P | P |   |   |   | — |
| `asm-gas-directives`⁷                 | S | S | S |   |   |   | — |
| `i386-fastcall-abi`⁸                  | S | S | S |   |   |   | — |
| `compile.win32.*` / `pe-native-conformance` | P | P | P |   |   |   | — |
| `pe-wine-conformance` (label `wine`)  | S | S | S |   |   |   | — |
| `macho-*` (5 drivers, label `macho`)  | S | S | S |   |   |   | — |
| qemu cross×libc matrix (label `qemu`)⁹| S | S | S |   |   |   | P |

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
⁴ Needs a **second** reference compiler — build the self-contained
`clang-toolchain` (and a POSIX `sh`/`MCC_TEST_SH` for `diff3`); reported
`Skipped` without it.
⁵ Needs a POSIX `sh` (`MCC_TEST_SH`) plus mingw `nm`/`readelf` on `PATH`; the
~31 ELF-image cases self-skip on a PE target.
⁶ The X11 example (`compile.ex4`) skips when `<X11/Xlib.h>` is absent.
⁷ Unconditional skip everywhere: the integrated assembler lacks a few GAS
encodings (`sgdtq`/`sidtq`/`swapgs`).
⁸ Skips on Windows: the test mixes mcc objects (ELF) with reference-cc objects,
but mingw `gcc` emits PE/COFF — needs an ELF-emitting 32-bit reference cc.
⁹ See the per-cell grid below; on Windows the matrix runs via Docker.

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
