# ModernCC (`mcc`)

Small, fast, portable, one-pass compile-and-link (at `-O0`) C11 compiler.
Featuring in-memory execution (`-run`), embeds as a library (`libmcc`), and
cross-compiles 5 architectures × 3 object formats.

| Features      |                                                                                                                                                                                 |
|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Targets**   | x86_64 · i386 · arm · arm64 · riscv64                                                                                                                                           |
| **Formats**   | ELF · PE/COFF · Mach-O                                                                                                                                                          |
| **Libc**      | glibc · musl (`--sysroot`) · msvcrt · libSystem                                                                                                                                 |
| **Modes**     | compile+link · `-c` · `-S` (asm listing) · `-run` (JIT, no `a.out`) · `libmcc` C API                                                                                            |
| **Speed**     | single-pass — compiles and links in one pass when using `-O0`                                                                                                                   |
| **Size**      | <2MB executables/libraries                                                                                                                                                      |
| **Assembler** | integrated (`MCC_CONFIG_ASM`, incl. scalar SSE + `.cfi_*`) · inline asm · `asm goto` · `-S` via built-in disassembler (all 5 targets)                                           |
| **Safety**    | optional bounds checker (`-b`) · backtraces (`-bt`) · `-fsanitize=undefined` (trap mode; x86_64/arm64/riscv64) · `-fsanitize=address`/`=bounds` (built-in memory-error checker) |
| **Optimizer** | `-O1..-O3` AST-replay passes (byte-verified against `-O0`) · `-O<N>` (N≥4) superoptimizer search with a resumable per-user cache (`--clear-cache`)                              |
| **Cross**     | `mcc-<arch>` compilers via `MCC_ENABLE_CROSS`                                                                                                                                   |

## [Downloads](https://github.com/lucianlightgray/moderncc/releases)

| Archive       | Contains                                                    | Grab it to…                        |
|---------------|-------------------------------------------------------------|------------------------------------|
| `mcc-…`       | the `mcc` compiler + `-static`/`-dynamic`/`-musl` variants  | just compile C — the usual choice  |
| `libmcc-…`    | headers, `libmcc` static/shared libs, CMake package config  | embed mcc as a library             |
| `mcc-cross-…` | the `mcc-<arch>` cross compilers + per-target runtime       | cross-compile to other targets     |
| `bundle-…`    | **all three of the above**, in one archive                  | grab everything at once            |

## Building / Configuration

CMake (≥ 3.22) with CMakePresets.json:

```sh
cmake --preset debug # or: release
cmake --build --preset debug -j # build dir: cmake-debug or cmake-release
```

or, CMake (without presets):

```sh
cmake -S . -B cmake-release -DCMAKE_BUILD_TYPE=Release
ccmake cmake-release
cmake --build cmake-release -j
```

| Option                  | Default                        | Meaning                                                                     |
|-------------------------|:------------------------------:|-----------------------------------------------------------------------------|
| `MCC_BUILD_TESTS`       | ON                             | Register the CTest suite                                                    |
| `MCC_CONFIG_ASM`        | ON                             | Integrated assembler                                                        |
| `MCC_CONFIG_OPTIMIZER`  | ON                             | The `-O1`+ AST-replay optimizer                                             |
| `MCC_CONFIG_LSP`        | ON                             | `--lsp` concrete-syntax-tree capture                                        |
| `MCC_CONFIG_DIAG_RT`    | `bounds` in Debug, else `off`  | Runtime diagnostics: `off` / `backtrace` (`-bt`) / `bounds` (adds `-b`)     |
| `MCC_ENABLE_CROSS`      | OFF                            | Also build `mcc-<arch>` cross compilers                                     |
| `MCC_BUILD_STATIC_LIB`  | OFF                            | Build static `libmcc-static.a` instead of shared `libmcc.so`                |
| `MCC_BUILD_DYNAMIC_LIB` | OFF                            | Also build shared `libmcc-dynamic.so` alongside `libmcc-static.a`           |
| `MCC_SINGLE_SOURCE`     | ON                             | Amalgamate the compiler into one TU (`mcc` is self-contained)               |
| `MCC_BUILD_STATIC_EXE`  | OFF                            | Also build `mcc-static` (fully static `-static`; forced off on macOS)       |
| `MCC_BUILD_DYNAMIC_EXE` | ON                             | Also build `mcc-dynamic` (not single-source; links the shared `libmcc.so`)  |
| `MCC_BUILD_MUSL`        | OFF                            | Also build musl-targeting variants (`*-musl`, Linux only)                   |
| `MCC_BUILD_STRIP`       | ON for Release/MinSizeRel      | Strip symbols during link                                                   |
| `MCC_QEMU_TESTS`        | OFF                            | qemu-user cross-conformance matrix (below)                                  |

All compiler binaries follow one suffix convention:
`mcc[-<arch>][-static|-dynamic][-musl]` — arch first (cross targets only),
then the link/single-source shape, with `-musl` always last. `mcc` — the default,
installed binary — is a self-contained amalgamated build linked only to libc,
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

# macOS: link Apple frameworks (headers + SDK stub auto-resolved), e.g.
mcc app.c -framework CoreFoundation -o app     # also works with -run
mcc app.c -F/opt/Frameworks -framework MyKit -o app   # -F adds a search dir

# macOS: fuse per-arch builds into one universal binary (self-contained, no lipo):
mcc            app.c -o app.arm64
mcc-x86_64-osx --sysroot="$(xcrun --show-sdk-path)" app.c -o app.x86_64
machofat app app.arm64 app.x86_64              # -> universal (arm64 + x86_64)

# cross-compile against a sysroot (glibc or musl), then run under qemu-user:
mcc-arm64 --sysroot=/path/to/rootfs hello.c -o hello   # or mcc-arm64-musl
qemu-aarch64 -L /path/to/rootfs ./hello
```

## Embedding (`libmcc`)

`include/libmcc.h` exposes a TinyCC-style C API: `mcc_new` / `mcc_delete`,
`mcc_set_options`, `mcc_add_file`, `mcc_compile_string`, `mcc_set_output_type`
(memory / exe / dll / obj / preprocess / asm), include/lib/framework path
adders, `mcc_add_symbol` / `mcc_get_symbol` / `mcc_list_symbols`,
`mcc_output_file`, `mcc_relocate`, `mcc_run`, `mcc_set_error_func`,
`mcc_set_backtrace_func`, `mcc_setjmp`, plus the mcc-specific `mcc_cache_dir`
and `mcc_intention_hash`.

## Testing

CTest, organized by mechanism. Every corpus registers as one CTest per case
(`exec/<name>`, `cli/<name>`, `diff3/<name>`, `parts/<name>`, `preprocess/<name>`),
so `ctest -j` fans them across cores and each reports independently; only the
`full_language` differential (`mcctest`) runs many cases in a single process.
Inapplicable tests report **Skipped with a reason** (e.g. "requires arm64
target") rather than silently omitting, so gaps stay visible.

```sh
ctest --test-dir cmake-debug -j"$(nproc)"
```

| Directory            | Covers                                                                                   |
|----------------------|------------------------------------------------------------------------------------------|
| `tests/exec/`        | Golden run-and-diff, by topic (statements, expressions, types, structs, preprocessor, …) |
| `tests/preprocess/`  | `-E` preprocessor-only (expansion, pasting, variadic, …)                                 |
| `tests/diff/`        | Differential vs. a reference compiler (`full_language.c` + per-unit `parts/` wrappers)   |
| `tests/diff3/`       | Three-way differential (mcc vs. gcc **and** clang) over the exec corpus                  |
| `tests/cli/`         | Driver/CLI cases (`cases.h`): flags, diagnostics, `readelf`/`nm` structural checks       |
| `tests/embed/`       | `libmcc` embedding API (single- and multi-threaded)                                      |
| `tests/diagnostics/` | Expected errors/warnings                                                                 |
| `tests/asm/`         | Standalone assembler / asm↔C linkage                                                     |
| `tests/behavior/`    | Self-checking runtime drivers (FP, bounds, VLA)                                          |
| `tests/qemu/`        | Cross-target conformance (matrix below)                                                  |

## Cross-target × libc matrix (qemu-user)

`-DMCC_QEMU_TESTS=ON` exercises every arch × both C libraries. For each
`(arch, libc)` it fetches a minimal **Gentoo stage3** rootfs, cross-compiles
`tests/qemu/conformance/` against that sysroot, and runs it under
`qemu-<arch> -L <rootfs>`. It is opt-in and offline-by-default; the normal
`ctest` run is unaffected.

```sh
cmake --preset cross                       # native + cross compilers
cmake -DMCC_QEMU_TESTS=ON cmake-cross
cmake --build cmake-cross -j
ctest --test-dir cmake-cross -L qemu
```

| Arch    | glibc | musl |
|---------|:-----:|:----:|
| x86_64  |   Y   |  Y   |
| i386    |   Y   |  Y   |
| arm     |   Y   |  Y   |
| arm64   |   Y   |  Y   |
| riscv64 |   Y   |  Y   |

Each combo builds twice (default codegen and `-fPIC -pie`); each program
self-checks (integers, floats, complex Annex-G, aggregates/ABI, varargs,
atomics, libc, lexical, control flow) and exits non-zero on first failure, so it
is independent of libc output formatting. `qemu-arm64-osx` additionally covers
arm64 **Darwin** codegen linked against arm64 glibc. The host arch uses the
native `mcc` (no cross build). Cache variables (`MCC_QEMU_ARCHS`,
`MCC_QEMU_LIBCS`, `MCC_QEMU_MIRROR`, `MCC_QEMU_DLDIR`) drive the grid; combos
lacking a cross compiler or `qemu-<arch>` report a skip reason.

The rootfs trees vendor under `vendor/gentoo-stage3-<arch>-<libc>/` (each with a
`.fetched` marker); point `MCC_QEMU_DLDIR` at a directory of prefetched
`gentoo-stage3-<arch>-<libc>/` sysroots to skip the download:

```sh
cmake --preset cross -DMCC_QEMU_TESTS=ON            # roots land in vendor/gentoo-stage3-*
cmake --build cmake-cross -j
ctest --test-dir cmake-cross -L qemu --output-on-failure
```

### QEMU / Docker

macOS and any host without user-mode QEMU run the same matrix via the
containerized runner in `tests/qemu/docker/` — it supplies the `qemu-<arch>`
emulators and a Linux toolchain, caching sysroots in your `vendor/` tree:

```sh
docker build -t mcc-qemu tests/qemu/docker
docker run --rm -v "$PWD":/work -v "$PWD/vendor:/vendor" mcc-qemu
# narrow the grid: -e ARCHS=x86_64 -e LIBCS=glibc, or pass ctest flags (-R …)
```

It builds `mcc` + cross compilers from scratch, then runs `ctest -L qemu`
against the sysroots in `vendor/gentoo-stage3-*`. The shared `/vendor` mount
means the (large) rootfs downloads happen once and are reused by the host and
every container.

### CI and labels

Stable labels let a CI host select what it supports. A `native` label marks
every test that is not part of the qemu matrix.

| Label    | Selects                                          | Needs                                    |
|----------|--------------------------------------------------|------------------------------------------|
| `native` | native matrix                                    | glibc/musl sysroots                      |
| `qemu`   | non-native cross-conformance matrix              | qemu-user + per-arch glibc/musl sysroots |
| `wine`   | PE/Windows runtime conformance                   | win32 cross compilers + `wine`           |
| `macho`  | Mach-O structural + self-contained image/codegen | native host (some need Darwin/darling)   |

```sh
ctest --test-dir <build> -L qemu                 # just the qemu matrix
ctest --test-dir <build> -L native               # everything except the qemu matrix
```

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

Derived from [TinyCC](https://repo.or.cz/tinycc.git) by Fabrice Bellard and
contributors; distributed under the **GNU Lesser General Public License v2.1**
(LGPL-2.1).
