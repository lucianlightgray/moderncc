# ModernCC (`mcc`)

An extremely small, fast, portable, one-pass compile-and-link C11 compiler.
Features in-memory execution (`-run`), embeddable as a
library (`libmcc`), and cross-compilation of 5 architectures × 3 object formats
from a single source tree.

| Features      |                                                                                                                                       |
|---------------|---------------------------------------------------------------------------------------------------------------------------------------|
| **Targets**   | x86_64 · i386 · arm · arm64 · riscv64                                                                                                 |
| **Formats**   | ELF · PE/COFF · Mach-O                                                                                                                |
| **Libc**      | glibc · musl (`--sysroot`) · msvcrt · libSystem                                                                                       |
| **Modes**     | compile+link · `-c` · `-S` (asm listing) · `-run` (JIT, no `a.out`) · `libmcc` C API                                                  |
| **Speed**     | single-pass (~75–90× faster to compile than `-O2`, vs clang/gcc — [measured](docs/NOTES.md#compile-speed--footprint))                  |
| **Size**      | ~0.7 MB dynamic · ~1.5 MB static self-contained binary                                                                                |
| **Assembler** | integrated (`MCC_CONFIG_ASM`, incl. scalar SSE + `.cfi_*`) · inline asm · `asm goto` · `-S` via built-in disassembler (all 5 targets) |
| **Safety**    | optional bounds checker (`-b`) and backtraces (`-bt`)                                                                                 |
| **Cross**     | `mcc-<arch>` compilers via `MCC_ENABLE_CROSS`                                                                                         |

> Detailed toolchain comparisons, build-status reports, per-toolchain test
> coverage, and performance benchmarks live in [docs/NOTES.md](docs/NOTES.md).

## Downloads

Prebuilt archives are attached to each
[release](https://github.com/lucianlightgray/moderncc/releases), named
`<what>-<version>-<os>-<arch>[-<cc>]` — `.tar.xz` on Linux/macOS, `.zip` on
Windows. Pick by what you need:

| Archive       | Contains                                                    | Grab it to…                        |
|---------------|-------------------------------------------------------------|------------------------------------|
| `mcc-…`       | the `mcc` compiler + `-static`/`-dynamic`/`-musl` variants  | just compile C — the usual choice  |
| `libmcc-…`    | headers, `libmcc` static/shared libs, CMake package config  | embed mcc as a library             |
| `mcc-cross-…` | the `mcc-<arch>` cross compilers + per-target runtime       | cross-compile to other targets     |
| `bundle-…`    | **all three of the above**, in one archive                  | grab everything at once            |

Each platform also ships `checksums-<os>-<arch>.txt`, and every release carries a
combined `SHA256SUMS.txt`.

## Building / Configuration

CMake (≥ 3.22) with CMakePresets.json:

```sh
cmake --preset debug # or: release, sanitize, diagnostics, cross, matrix
cmake --build --preset debug -j
```

The developer presets above are the ones you'll use by hand; CI/dist presets
(`linux-*`, `macos*`, `msvc`, `sanitize-msvc`, `mingw`, `qemu*`, `dist-*`) are also
defined and drive the workflows + docker runners. See [BUILD.md §2](docs/BUILD.md) for the full
preset catalog and naming conventions. Current per-platform build status lives
in [docs/NOTES.md](docs/NOTES.md#build-status).

or, CMake (without presets):

```sh
cmake -S . -B cmake-release -DCMAKE_BUILD_TYPE=Release
ccmake cmake-release
cmake --build cmake-release -j
```

| Option                  | Default | Meaning                                                                    |
|-------------------------|:-------:|----------------------------------------------------------------------------|
| `MCC_BUILD_TESTS`       |   ON    | Register the CTest suite                                                   |
| `MCC_CONFIG_ASM`        |   ON    | Integrated assembler                                                       |
| `MCC_CONFIG_BCHECK`     |   ON    | Bounds checker                                                             |
| `MCC_CONFIG_BACKTRACE`  |   ON    | Runtime backtraces                                                         |
| `MCC_ENABLE_CROSS`      |   OFF   | Also build `mcc-<arch>` cross compilers                                    |
| `MCC_BUILD_STATIC_LIB`  |   OFF   | Build static `libmcc-static.a` instead of shared `libmcc.so`               |
| `MCC_BUILD_DYNAMIC_LIB` |   OFF   | Also build shared `libmcc-dynamic.so` alongside `libmcc-static.a`          |
| `MCC_SINGLE_SOURCE`     |   ON    | Amalgamate the compiler into one TU (`mcc` is self-contained)              |
| `MCC_BUILD_STATIC_EXE`  |  OFF¹   | Also build `mcc-static` (fully static `-static`)                           |
| `MCC_BUILD_DYNAMIC_EXE` |   ON    | Also build `mcc-dynamic` (not single-source; links the shared `libmcc.so`) |
| `MCC_BUILD_MUSL`        |  OFF²   | Also build musl-targeting variants (`*-musl`, Linux only)                  |
| `MCC_BUILD_STRIP`       |   OFF   | Strip symbols during link                                                  |
| `MCC_QEMU_TESTS`        |   OFF   | qemu-user cross-conformance matrix (below)                                 |

All compiler binaries follow one suffix convention:
`mcc[-<arch>][-static|-dynamic][-musl]` — arch first (cross targets only),
then the link/single-source shape, with `-musl` always last. `mcc` — the default,
installed binary — is a self-contained SINGLE_SOURCE build linked only to libc,
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

## Testing

CTest, organized by mechanism. Every corpus is registered as one CTest per case
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

Per-toolchain coverage tables (which suites pass/skip on each host) and
compile-speed/footprint benchmarks live in
[docs/NOTES.md](docs/NOTES.md#per-toolchain-test-coverage).

## Cross-target × libc matrix (qemu-user)

`-DMCC_QEMU_TESTS=ON` exercises every arch × both C libraries. For each
`(arch, libc)` it fetches a minimal **Gentoo stage3** rootfs, cross-compiles
`tests/qemu/conformance/` against that sysroot, and runs it under
`qemu-<arch> -L <rootfs>`. Opt-in and offline-by-default; the normal `ctest` run
is unaffected.

```sh
cmake --preset cross                       # native + cross compilers
cmake -DMCC_QEMU_TESTS=ON cmake-cross
cmake --build cmake-cross -j
ctest --test-dir cmake-cross -L qemu
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

The rootfs trees are vendored under `vendor/gentoo-stage3-<arch>-<libc>/` (each
with a `.fetched` marker); point `MCC_QEMU_DLDIR` at a directory of prefetched
`gentoo-stage3-<arch>-<libc>/` sysroots to skip the download:

```sh
cmake --preset cross -DMCC_QEMU_TESTS=ON            # roots land in vendor/gentoo-stage3-*
cmake --build cmake-cross -j
ctest --test-dir cmake-cross -L qemu --output-on-failure
```

### Off-Linux (Docker)

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
programs); `macho-stack-protector` and `macho-universal` (the `machofat` fat-binary
combiner, its 2-slice case via `xcrun --show-sdk-path`) run natively too. Linux/x86_64:
the `structural`, `image-run`, `codegen-run`, `apple-libc`, and `stack-protector`
drivers approximate the same codegen via a loader/trampoline (and consume the
`mcc-x86_64-osx` cross); off x86_64 they **Skip**, not hollow-pass. The remaining libSystem/dyld path
(libmalloc, locale stdio, dyld, pthread/GCD, ObjC) is kernel-fused and needs a
macOS or **darling** host (`-DMCC_DARWIN_HOST=ON` — the `macos` preset sets it,
so the macOS CI runner covers this); intentionally outside the ELF matrix.

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

Derived from [TinyCC](https://repo.or.cz/tinycc.git) by Fabrice Bellard and contributors; distributed under the
**GNU Lesser General Public License v2.1** (LGPL-2.1).
