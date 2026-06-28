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
| x86_64  | ✅ | ✅ |
| i386    | ✅ | ✅ |
| arm     | ✅ | ✅ |
| arm64   | ✅ | ✅ |
| riscv64 | ✅ | ✅ |

Each combo compiles and runs six conformance programs (integers, floats,
aggregates/ABI, varargs, libc, control flow); each program self-checks and
exits non-zero on the first failure, so it is independent of libc output
formatting.

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
