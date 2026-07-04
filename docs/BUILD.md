# BUILD.md — CMake configuration values to test during builds

This is the complete catalog of CMake cache values that influence an mcc build,
what they do, their type/default/allowed values, and the relevance gate that
decides whether they matter for a given host/target. Use it to drive CI and to
decide which build combinations are worth exercising.

Source of truth: the `mcc_config_node()` declarations in `CMakeLists.txt`
(section 1z) plus the loose `set(... CACHE ...)`/`option()` declarations for the
superbuild, toolchain-download, and test subsystems. A machine-generated version
of the node table can be emitted from the build itself via
`mcc_generate_node_doc()`.

**Executable targets & default build shape.** On the host the compiler ships in
three executable shapes, each with a musl sibling (`mcc-*-musl`):

| Target        | Exe TU                      | libc link          | Links `libmcc`?              | Built when                              |
|---------------|-----------------------------|--------------------|------------------------------|-----------------------------------------|
| `mcc`         | single-source (self-contained) | dynamic            | no (libc only)               | **always**                              |
| `mcc-static`  | `MCC_SINGLE_SOURCE` decides    | static (`-static`) | only if `MCC_SINGLE_SOURCE=OFF` | `MCC_BUILD_STATIC_EXE=ON`               |
| `mcc-dynamic` | not single-source              | dynamic            | yes → primary `libmcc`       | `MCC_BUILD_DYNAMIC_EXE=ON` **and** `MCC_SINGLE_SOURCE=OFF` |

`mcc-dynamic` is a non-amalgamated driver TU whose `mcctools.c` references
libmcc-internal `ST_FUNC` helpers (`pstrcpy`, …). Those have external linkage
**only** in a multi-TU `libmcc` (`MCC_SINGLE_SOURCE=OFF`); an amalgamated libmcc
keeps them `static`, and a *shared* libmcc never exports them on PE (no
`dllexport`). So `mcc-dynamic` is built only under `MCC_SINGLE_SOURCE=OFF` and links
the **primary** `libmcc` (the static archive when `MCC_BUILD_STATIC_LIB=ON`,
which resolves the helpers on every platform; the shared `libmcc.so` on ELF).
Under the default `MCC_SINGLE_SOURCE=ON` it is skipped with a status message. The
`dist-*` presets set `MCC_SINGLE_SOURCE=OFF` so releases ship it.

With `MCC_ENABLE_CROSS=ON` the same self-contained `mcc` shape is also built for
every foreign target below (all are host binaries; the `-dynamic` shape is
host-only, as there is no per-arch `libmcc`). Each takes a `-static` variant when
`MCC_BUILD_STATIC_EXE=ON` (non-MSVC host) and `-musl` siblings when
`MCC_BUILD_MUSL=ON`; each also produces its runtime archive `<arch>-libmccrt.a`:

The CMake target ids equal the output binary names (`mcc-<arch>`):

| Binary / CMake target | CPU     | Target OS | Extra variants                     |
|-----------------------|---------|-----------|------------------------------------|
| `mcc-i386`            | i386    | Linux     | `-static`, `-musl`, `-static-musl` |
| `mcc-x86_64`          | x86_64  | Linux     | `-static`, `-musl`, `-static-musl` |
| `mcc-arm`             | arm     | Linux     | `-static`, `-musl`, `-static-musl` |
| `mcc-arm64`           | arm64   | Linux     | `-static`, `-musl`, `-static-musl` |
| `mcc-riscv64`         | riscv64 | Linux     | `-static`, `-musl`, `-static-musl` |
| `mcc-i386-win32`      | i386    | WIN32     | `-static`                          |
| `mcc-x86_64-win32`    | x86_64  | WIN32     | `-static`                          |
| `mcc-arm64-win32`     | arm64   | WIN32     | `-static`                          |
| `mcc-arm-wince`       | arm     | WIN32     | `-static`                          |
| `mcc-x86_64-osx`      | x86_64  | Darwin    | `-static`                          |
| `mcc-arm64-osx`       | arm64   | Darwin    | `-static`                          |

`mcc` is the canonical, installed binary and the one the test suite drives; by
default it is a self-contained SINGLE_SOURCE build linked only to libc. The
`libmcc` library (shared `libmcc.so` by default, or static `libmcc.a` with
`MCC_BUILD_STATIC_LIB=ON`) is still built — for the embed API and for
`mcc-dynamic` to link against. One behavioral note: a **static** `mcc-static`
resolves `-run` libc symbols from a built-in table (common symbols only, via
`CONFIG_MCC_STATIC`), whereas the dynamic `mcc` / `mcc-dynamic` resolve
arbitrary libc symbols through `dlsym`.

All binaries follow one **suffix convention**, appended in a fixed order:
`mcc[-<arch>][-static|-dynamic][-musl]` — architecture first (cross compilers
only, `MCC_ENABLE_CROSS`), then the link/single-source shape, `-musl` always last.
Cross compilers are self-contained host binaries, so they take `-static` (with
`MCC_BUILD_STATIC_EXE=ON`) but never `-dynamic` (there is no per-arch `libmcc`
to link). Examples: `mcc`, `mcc-static`, `mcc-dynamic`, `mcc-musl`,
`mcc-arm64`, `mcc-x86_64-static`, `mcc-x86_64-static-musl`.

Libraries use the same `libmcc[-static|-dynamic][-musl]` scheme, with one
exception matching the bare `mcc` default: when only the shared/dynamic lib is
built (the default, `MCC_BUILD_STATIC_LIB=OFF`) the shape suffix is **omitted**,
so it stays plain `libmcc.so`. Otherwise the shape is explicit:

| Config | Libraries produced (each with a `-musl` sibling) |
|---|---|
| `MCC_BUILD_STATIC_LIB=OFF` (default) | `libmcc.so` |
| `MCC_BUILD_STATIC_LIB=ON` | `libmcc-static.a` |
| `MCC_BUILD_STATIC_LIB=ON` + `MCC_BUILD_DYNAMIC_LIB=ON` | `libmcc-static.a` + `libmcc-dynamic.so` |

> Legend for the *Gate* column:
> **always** = relevant on every host/target · **ELF** = only on ELF targets
> (not WIN32, not Darwin) · **Darwin** = macOS/Mach-O only · **arm** = only when
> `MCC_CPU==arm` · **!static-lib** = only when `MCC_BUILD_STATIC_LIB=OFF`.
> Booleans are CMake `ON`/`OFF`. `''` means the empty string (an accepted value).

---

## 1. Standard CMake values (not mcc-specific, but they change the build)

| Value | Type | Default | Notes / what to test |
|---|---|---|---|
| `CMAKE_BUILD_TYPE` | STRING | *(empty)* | `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel`. Forwarded to matrix cells. |
| `CMAKE_C_COMPILER` | FILEPATH | auto | The real compiler switch (gcc / clang / mcc / cl). `MCC_TOOLCHAIN_PROFILE` only seeds defaults — it does **not** pick the compiler. |
| `CMAKE_CROSSCOMPILING_EMULATOR` | STRING | *(empty)* | Copied into `MCC_EMULATOR`. Required to run the just-built foreign `mcc` when `CMAKE_CROSSCOMPILING` is set (e.g. `wine`, `qemu-arm`). |
| `CMAKE_TOOLCHAIN_FILE` | FILEPATH | *(none)* | Selects a cross target; triggers the cross-compiling validation path. |
| `CMAKE_INSTALL_PREFIX` | PATH | platform default | Base for `MCC_INSTALL_MCCDIR` default (`<prefix>/<libdir>/mcc`). |
| `CMAKE_OSX_DEPLOYMENT_TARGET` | STRING | auto | Auto-pinned when a Homebrew GNU gcc host is detected on macOS (avoids the stale 10.6 default). |
| `CMAKE_EXPORT_COMPILE_COMMANDS` | BOOL | ON (always set) | Set unconditionally before `project()`. |
| Generator (`-G`) / `-A` | — | platform default | Matters for the `msvc` matrix cells (see `MCC_MSVC_GENERATOR`). |

---

## 2. CMakePresets.json — ready-made configurations

`CMakePresets.json` is the **single source of truth** for every build in the
repo: the CI workflows (`.github/workflows/*.yml`) and the docker runners
(`tests/*/docker/run-*.sh`) invoke `cmake --preset <name>` /
`cmake --build --preset <name>` / `ctest --preset <name>` rather than
hand-passing `-D` flags. A preset name is the canonical label for a scenario and
is reused verbatim as the CI job / matrix cell name and, where applicable, the
docker `PRESET` env — so `linux-gcc`, `msvc`, `qemu-arm64`, `dist-linux-gcc`
name the same thing everywhere.

**Conventions.** Generator is **Ninja** everywhere except the `msvc`/`dist-msvc`
presets, which omit a generator so CMake picks the default Visual Studio
generator (VS can't be driven by a fixed `-G` string across runner images).
Every preset builds into `cmake-<presetName>/`. Names encode
`platform[-compiler][-axis…]`; the compiler is baked in when it is a stable
plain name (`gcc`/`clang`), and comes from `$env{CC}` only where the path is
host-dynamic (macOS Homebrew gcc). Hidden bases are prefixed `_`
(`_base`, `_ninja`, `_dist`, `_test*`).

**Developer presets** — interactive use (`cmake --preset debug`):

| Preset | CMAKE_BUILD_TYPE | Key overrides |
|---|---|---|
| `debug` | Debug | musl OFF, bcheck ON, backtrace ON, strip OFF |
| `release` | Release | musl ON, strip ON, bcheck OFF, backtrace OFF (SINGLE_SOURCE dynamic exe) |
| `sanitize` | Debug | `MCC_BUILD_SANITIZE=ON` |
| `diagnostics` | Debug | `MCC_ALL_DIAGNOSTICS=ON` (warnings + debug + mcc_s/mcc_p/mcc_c) |
| `cross` | Debug | `MCC_ENABLE_CROSS=ON` |
| `matrix` | Debug | `MCC_TOOLCHAIN_PROFILE=gcc;clang` × `MCC_TARGETS=native;cross` |

**CI presets** — one per workflow matrix cell (`ci.yml`):

| Preset | Used by (job / runner) | Shape |
|---|---|---|
| `linux-gcc`, `linux-clang` | `linux` (docker) | Debug, native, `CMAKE_C_COMPILER` pinned |
| `linux-gcc-cross`, `linux-clang-cross` | `linux` | + `MCC_ENABLE_CROSS=ON` |
| `linux-gcc-musl` | `linux` | + `MCC_BUILD_MUSL=ON` |
| `linux-gcc-release`, `linux-clang-release` | `linux` | Release, stripped, bcheck/backtrace off |
| `linux-gcc-static` | `linux` | + `MCC_BUILD_STATIC_EXE=ON` (`mcc-static`, `CONFIG_MCC_STATIC`) |
| `linux-gcc-multisource` | `linux` | + `MCC_SINGLE_SOURCE=OFF` (multi-TU; builds `mcc-dynamic`) |
| `linux-gcc-asm-off` | `linux` | + `MCC_CONFIG_ASM=OFF` (mccrt via host cc) |
| `linux-gcc-predefs-off` | `linux` | + `MCC_CONFIG_PREDEFS=OFF` (runtime mccdefs) |
| `linux-gcc-pie` | `linux` | + `MCC_CONFIG_PIE=ON` `MCC_CONFIG_PIC=ON` |
| `linux-gcc-dwarf` | `linux` | + `MCC_CONFIG_DWARF=5` |
| `linux-gcc-sanitize` | `linux` | + `MCC_BUILD_SANITIZE=ON` (gcc libasan) |
| `linux-gcc-diagnostics` | `linux` | + `MCC_ALL_DIAGNOSTICS=ON` (`mcc_s`/`mcc_p`/`mcc_c`) |
| `macos`, `macos-cross` | `macos` | Debug, `CMAKE_C_COMPILER=$env{CC}` (clang / Homebrew gcc), `MCC_DARWIN_HOST=ON` |
| `msvc` | `msvc` | Release, `MCC_TOOLCHAIN_PROFILE=msvc`, diff3 refs from `$env{}` |
| `mingw` | `mingw` | Release, `MCC_TOOLCHAIN_PROFILE=mingw` (build-only) |

The `linux`, `macos`, `msvc`, and `mingw` build jobs upload their build
targets (executables + libraries) as artifacts — `mcc-<preset>-<arch>`
(macOS inserts the compiler: `mcc-macos-<cc>-arm64`): the job configures
with `CMAKE_INSTALL_PREFIX` pointing at a `ci-out/` dir, runs
`cmake --install` after the tests, and uploads the tree as a tar.gz (tar
first — GitHub artifacts don't preserve the exec bit). The docker `linux`
job does this by mounting that dir at `/out`; `tests/ci/docker/run-ci.sh`
picks the prefix up from the mount. The prefix must be set at *configure*
time: the mcc runtime dir (`lib*/mcc`) installs to an absolute path baked
into the install rules, so an install-time `--prefix` cannot re-root it.
The `dist` jobs instead package their preset's `${sourceDir}/stage` prefix
via the `package-dist` build target (which drives `ci pkg`, the C port of the
former `cmake/package.cmake`); the `qemu` job is test-only and uploads
nothing.

**qemu presets** — the `qemu` job passes `PRESET=qemu-<arch>` to the docker
runner; `qemu` alone is the full local matrix:

| Preset | `MCC_QEMU_ARCHS` | Common |
|---|---|---|
| `qemu` | all (`x86_64;i386;arm;arm64;riscv64`) | Debug, `MCC_ENABLE_CROSS=ON`, `MCC_QEMU_TESTS=ON`, `MCC_QEMU_LIBCS=glibc;musl` |
| `qemu-x86_64` … `qemu-riscv64` | that one arch | inherit `qemu` |

**dist presets** — release artifacts (`ci.yml` `dist`, `release.yml`). Every dist
build produces *all* permutations: Release, `MCC_BUILD_TESTS=OFF`,
`MCC_SINGLE_SOURCE=OFF` (so `mcc-dynamic` builds), `MCC_BUILD_STATIC_LIB=ON` **and**
`MCC_BUILD_DYNAMIC_LIB=ON` (both `libmcc-static.a` + `libmcc-dynamic.so`),
`MCC_ENABLE_CROSS=ON` (all cross compilers), `CMAKE_INSTALL_PREFIX=${sourceDir}/stage`:

| Preset | Compiler / profile | Extra |
|---|---|---|
| `dist-linux-gcc`, `dist-linux-clang` | `gcc` / `clang` | static exe, stripped, musl |
| `dist-macos` | `clang` | dynamic, no musl (post-`strip -x`), no static exe |
| `dist-mingw` | mingw profile | static exe, stripped (PE) |
| `dist-msvc` | MSVC profile | static exe, stripped (PE) |

Build presets exist for every configure preset; test presets exist for all
except the build-only (`mingw`), test-less (`dist-*`), and superbuild
(`matrix`) ones — the superbuild registers no top-level tests and runs
`ctest` per sub-build via `MCC_SUPERBUILD_TEST=ON` instead. The label
filter is baked in (`qemu`-excluded on ELF hosts, `qemu|wine` on macOS,
`qemu|wine|macho` on MSVC, `qemu`-only for the qemu presets). The `macos`
preset also sets `MCC_DARWIN_HOST=ON` (the CI runner is a real Darwin
host), enabling the kernel-fused apple-libc suite.

---

## 3. Build-target knobs (`GROUP "Build targets"`)

| Value | Type | Default | Choices | Gate | Purpose |
|---|---|---|---|---|---|
| `MCC_TOOLCHAIN_PROFILE` | STRING (list) | `auto` | auto, gcc, clang, mcc, msvc, mingw | always | Toolchain(s) to build with, in order. A single value seeds profile defaults; a list (or `mingw`) triggers the superbuild matrix. |
| `MCC_ENABLE_CROSS` | BOOL | OFF | | always | Build all cross compilers `mcc-<arch>` (ordinary targets, not a `cross` meta-target); each also gets a `-static` variant when `MCC_BUILD_STATIC_EXE=ON` and a `-musl` sibling. |
| `MCC_BUILD_STATIC_LIB` | BOOL | OFF | | always | Static `libmcc-static.a` (ON) vs shared `libmcc.so` (OFF, default). |
| `MCC_BUILD_STATIC_EXE` | BOOL | OFF | | always (forced OFF on macOS) | Build the `mcc-static` target (fully static `-static`). `MCC_SINGLE_SOURCE` decides its compile+link path: self-contained (ON) or linking `libmcc.a` (OFF, then also needs `MCC_BUILD_STATIC_LIB=ON`). macOS has no static libc → forced OFF. |
| `MCC_BUILD_DYNAMIC_LIB` | BOOL | OFF | | always | Also build a shared `libmcc-dynamic.so` alongside the `libmcc-static.a` (only meaningful with `MCC_BUILD_STATIC_LIB=ON`). |
| `MCC_BUILD_DYNAMIC_EXE` | BOOL | **ON** | | **MCC_SINGLE_SOURCE=OFF** | Build the `mcc-dynamic` target: NOT single-source, driver TU linked against the primary `libmcc`. Requires a multi-TU libmcc (`MCC_SINGLE_SOURCE=OFF`) so libmcc's internal helpers are linkable; under the default `MCC_SINGLE_SOURCE=ON` it is skipped with a status message. |
| `MCC_BUILD_MUSL` | BOOL | OFF | | always | Also build musl-targeting variants (`mcc*-musl`). Opt-in — enabled only by the explicit `-musl` presets/targets (`release`, `linux-gcc-musl`, `dist-linux-*`). |
| `MCC_BUILD_STRIP` | BOOL | OFF | | always | Strip symbols at link (`-s`). |
| `MCC_ENABLE_RPATH` | BOOL | **ON** | | **!static-lib** | Bake `-rpath` into binaries linking `libmcc.so` so they find the shared lib at runtime (relevant by default, since the lib is shared). |
| `MCC_SINGLE_SOURCE` | BOOL | **ON** | | always | Build libmcc from a single TU (amalgamation). Also seeded ON by the `mcc` profile. Set OFF for a multi-TU library. |
| `MCC_BUILD_TESTS` | BOOL | **ON** | | always | Build/enable the CTest suite. |
| `MCC_MCCRT_USE_HOSTCC` | BOOL | OFF | | always | Build native `mccrt` with the host CC instead of `mcc` (faster bcheck). Auto-forced ON when no emulator / asm disabled. |
| `MCC_EMBED_MCCRT` | BOOL | **ON** | | always (ELF/Mach-O; forced OFF on WIN32) | Bake `libmccrt.a` into the `mcc` binary (self-contained; no sidecar `.a` needed at link time). The embedded loader streams it through a temp fd to the ordinary alacarte archive loader. Forces the native `mccrt` to be host-CC built (like `MCC_MCCRT_USE_HOSTCC`) to break the mcc→archive→mcc build cycle. Only the primary `mcc` target embeds; static/musl/cross variants keep the sidecar. |
| `MCC_CONFIG_AUTOCORRECT` | BOOL | OFF | | always (advanced) | Non-strict: auto-correct inert/non-runnable combos instead of only warning. |
| `MCC_MINGW_SOURCE` | STRING | `winlibs` | winlibs, multilib | always (advanced) | Source for the `mingw-toolchain` download. |

---

## 4. Diagnostics / instrumentation (`GROUP "Diagnostics"`)

| Value | Type | Default | Gate | Purpose |
|---|---|---|---|---|
| `MCC_ALL_DIAGNOSTICS` | BOOL | OFF | GNU/Clang host (advanced) | Everything-on: verbose warnings + debug info + build `mcc_s`/`mcc_p`/`mcc_c`. |
| `MCC_BUILD_SANITIZE` | BOOL | OFF | GNU/Clang host; **not** WIN32/mingw | Build `mcc_s` (`-fsanitize=address,undefined`). Fatal on a PE target. |
| `MCC_BUILD_PROFILE` | BOOL | OFF | GNU/Clang host; **not** Darwin | Build `mcc_p` (`-pg -static`). Fatal on Darwin (no static crt0). |
| `MCC_BUILD_COVERAGE` | BOOL | OFF | GNU/Clang host; needs runnable mcc | Build `mcc_c` (coverage instrumentation). |

`MCC_BUILD_SANITIZE`/`MCC_BUILD_PROFILE` require a GCC/Clang **host** compiler
(fatal otherwise).

---

## 5. mcc feature toggles (`GROUP "mcc features"` / `"Darwin"`)

These bake `CONFIG_*` values into the compiler and change its runtime behavior.

| Value | Type | Default | Choices | Gate | Purpose |
|---|---|---|---|---|---|
| `MCC_CONFIG_MINGW` | BOOL | OFF | | always | Build a WIN32/mingw target (forces `MCC_TARGETOS=WIN32`). |
| `MCC_CONFIG_BACKTRACE` | BOOL | **ON** | | always | Stack backtraces (`-bt` / `-run`). |
| `MCC_CONFIG_BCHECK` | BOOL | **ON** | | **MCC_CONFIG_BACKTRACE** | Bounds checker (`-b`). Requires backtrace to link. |
| `MCC_CONFIG_ASM` | BOOL | **ON** | | always | Integrated assembler (inline/global asm, `.s` files, asm labels). Disabling forces `MCC_MCCRT_USE_HOSTCC=ON`. |
| `MCC_CONFIG_PREDEFS` | BOOL | **ON** | | always | Compile `mccdefs.h` into the binary (c2str). OFF ⇒ runtime dependency on `<mccdefs.h>`. |
| `MCC_CONFIG_PIE` | BOOL | OFF | | **ELF** | mcc emits position-independent executables. |
| `MCC_CONFIG_PIC` | BOOL | OFF | | **ELF** | Position-independent code. |
| `MCC_RUN_MMAP_EXEC` | BOOL | OFF | | always | Allocate `-run` executable memory via `mmap` (separate RW/RX) instead of `mprotect`; needed on SELinux/PaX hardened (W^X) kernels. |
| `MCC_CONFIG_NEW_DTAGS` | BOOL | OFF | | **ELF** | `DT_RUNPATH` instead of `DT_RPATH` (mcc-emitted). |
| `MCC_AUTO_MCCDIR` | BOOL | **ON** | | always | Build-tree mcc auto-discovers `libmccrt.a` + headers locally, else system `CONFIG_MCCDIR`. |
| `MCC_CONFIG_LIBC` | STRING | `''` | uClibc, musl, `''` | **ELF** | Target libc. `uClibc` is a legacy selector (warns). |
| `MCC_CONFIG_DWARF` | STRING | `''` | 0, 2, 3, 4, 5, `''` | always | DWARF debug version; empty = stabs. |
| `MCC_CONFIG_SEMLOCK` | STRING | `''` | numeric | always | `CONFIG_MCC_SEMLOCK` value; empty = mcc.h default (1). Must be numeric (fatal otherwise). |
| `MCC_CONFIG_NEW_MACHO` | STRING | `''` | yes, no, auto, `''` | **Darwin** | Force apple object format. |
| `MCC_CONFIG_CODESIGN` | STRING | `''` | yes, no, auto, `''` | **Darwin** | Use `codesign` to sign executables. |

---

## 6. Runtime path overrides (`GROUP "Runtime paths"`, advanced)

All STRING, default `''`, mirror `configure --…` flags. Empty = autodetect.

| Value | configure flag |
|---|---|
| `MCC_SYSROOT` | `--sysroot` |
| `MCC_TRIPLET` | `--triplet` |
| `MCC_SYSINCLUDEPATHS` | `--sysincludepaths` (colon sep) |
| `MCC_LIBPATHS` | `--libpaths` (colon sep) |
| `MCC_CRTPREFIX` | `--crtprefix` (colon sep) |
| `MCC_ELFINTERP` | `--elfinterp` |
| `MCC_SWITCHES` | `--mcc-switches` |
| `MCC_OS_RELEASE` | `--os-release` |
| `MCC_INSTALL_MCCDIR` | `CONFIG_MCCDIR`; empty = `<prefix>/<libdir>/mcc` (`GROUP "Install"`) |

## 7. Extra build flags (`GROUP "Build flags"`, advanced)

| Value | Type | Default | Purpose |
|---|---|---|---|
| `MCC_EXTRA_CFLAGS` | STRING | `''` | `configure --extra-cflags` (space separated). |
| `MCC_EXTRA_LDFLAGS` | STRING | `''` | `configure --extra-ldflags`. |
| `MCC_EXTRA_LIBS` | STRING | `''` | `configure --extra-libs`. |

## 8. ARM ABI (`GROUP "ARM ABI"`, advanced, gate = `MCC_CPU==arm`)

All autodetected from the host cc; override only to force an ABI.

| Value | Type | Default | Detected from |
|---|---|---|---|
| `MCC_ARM_EABI` | BOOL | OFF | host cc |
| `MCC_ARM_VFP` | BOOL | OFF | host cc |
| `MCC_ARM_HARDFLOAT` | BOOL | OFF | host cc |
| `MCC_ARM_IDIV` | BOOL | OFF | `__ARM_FEATURE_IDIV` |
| `MCC_CPUVER` | STRING | `''` (4/5/6/7) | `__ARM_ARCH` |

---

## 9. Superbuild / matrix orchestration

Active when `MCC_TOOLCHAIN_PROFILE` is a list, `MCC_TARGETS` ≠ `native`, or
`mingw` is in the profile.

| Value | Type | Default | Purpose |
|---|---|---|---|
| `MCC_TARGETS` | STRING (list) | `native` | Targets to build: `native`, `cross`, or a custom name (needs `MCC_TARGET_ARGS_<name>`). |
| `MCC_SUPERBUILD_TEST` | BOOL | ON | Run `ctest` after each matrix sub-build. |
| `MCC_SUPERBUILD_SEQUENTIAL` | BOOL | ON | Build toolchain stages in order (targets within a stage stay parallel). |
| `MCC_SUPERBUILD_CHILD` | BOOL | OFF (internal) | Set on matrix child cells to suppress recursion. |
| `MCC_MSVC_GENERATOR` | STRING | `''` (auto) | CMake generator for `msvc` cells (auto-detected via vswhere on Windows). |
| `MCC_MSVC_PLATFORM` | STRING | `x64` | Generator platform (`-A`) for `msvc` cells. |
| `MCC_TARGET_ARGS_<name>` | STRING | — | Extra `-D` args for a custom target name. |
| `MCC_CC_<tc>` | FILEPATH | — | Override the compiler for toolchain `<tc>` (gcc/clang/mcc/msvc/mingw). |
| `MCC_GNU_GCC` | FILEPATH | auto | Pin the "real GNU gcc" used by the `gcc` profile. |

---

## 10. Self-contained toolchain downloads (opt-in targets)

Only consumed by the `mingw-toolchain` / `clang-toolchain` fetch targets.

| Value | Type | Default | Purpose |
|---|---|---|---|
| `MCC_MINGW_DIR` | PATH | source dir | Parent dir for `cmake-mingw-*` trees. |
| `MCC_MINGW_WINLIBS_X86_64_URL` / `_SHA256` | STRING | pinned | WinLibs x86_64 zip + hash. |
| `MCC_MINGW_WINLIBS_I686_URL` / `_SHA256` | STRING | pinned | WinLibs i686 zip + hash. |
| `MCC_MINGW_MULTILIB_URL` / `_SHA256` / `_SUBDIR` | STRING | `''` / `''` / `mingw64` | Single-gcc multilib archive (required when `MCC_MINGW_SOURCE=multilib`). |
| `MCC_CLANG_DIR` | PATH | source dir | Parent dir for the `cmake-clang` tree. |
| `MCC_CLANG_URL` / `_SHA256` / `_SUBDIR` | STRING | host-dependent | LLVM release archive, hash, and top-level dir. |
| `MCC_CONFIG_EXTRA` | FILEPATH | `config-extra.cmake` | Optional extra CMake config included before options. |

---

## 11. Test-suite knobs (only when `MCC_BUILD_TESTS=ON`)

| Value | Type | Default | Purpose |
|---|---|---|---|
| `MCC_REF_CC` | FILEPATH | host cc | GCC-compatible reference compiler for differential tests (mcctest/-bcheck). |
| `MCC_GCCTESTSUITE_PATH` | PATH | `''` | `gcc.c-torture` dir for the `gcctestsuite` target. |
| `MCC_ARM_CROSS_COMPILE` | STRING | `''` | binutils prefix (e.g. `arm-linux-gnueabi-`) for `arm-asm-testsuite`. |
| `MCC_DARWIN_HOST` | BOOL | OFF | Tests run on a macOS/darling host with real libSystem (enables mach-o kernel-fused tests). |
| `MCC_QEMU_MIRROR` | STRING | Gentoo releases | Base URL for qemu-user rootfs downloads. |
| `MCC_QEMU_ARCHS` | STRING (list) | `x86_64;i386;arm;arm64;riscv64` | Architectures to exercise under qemu-user. |
| `MCC_QEMU_LIBCS` | STRING (list) | `glibc;musl` | C libraries to exercise. |
| `MCC_QEMU_DLDIR` | PATH | `<build>/qemu-roots` | Where rootfs trees are extracted. |
| `MCC_QEMU_DOCKER_ARCHS` / `_LIBCS` | STRING | `''` | Passed to the `qemu-docker` matrix (empty = image default). |

---

## 12. Derived / status values (read-only — assert, don't set)

Reported in the `================ mcc configuration ================` block and
useful to assert in CI. They are computed, not user inputs.

| Value | Derived from |
|---|---|
| `MCC_CPU` | `CMAKE_SYSTEM_PROCESSOR` → `i386`, `x86_64`, `arm64`, `arm`, `riscv64` |
| `MCC_TARGETOS` | `WIN32`, `Darwin`, `Android`, or `CMAKE_SYSTEM_NAME` |
| `MCC_CC_NAME` | detected host compiler family (gcc/clang/mcc/msvc) |
| `MCC_EMULATOR` | `CMAKE_CROSSCOMPILING_EMULATOR` |
| `cross_compiling` | `CMAKE_CROSSCOMPILING` |
| `arm_abi` | the five `MCC_ARM_*`/`MCC_CPUVER` values (arm only) |

---

## 13. Cross-value validation (combinations the configure step checks)

`mcc_validate_config()` enforces these (two live elsewhere but still fire at
configure time: the `mcc-dynamic`-skipped-under-SINGLE_SOURCE status is printed
at the target definition, and the toolchain-profile-entry fatal at the node
declaration); test both the pass and the warn/fatal path. With
`MCC_CONFIG_AUTOCORRECT=ON` several fatals become auto-corrections instead.

- **Sanitize/profile need GCC/Clang host** → fatal on MSVC host.
- **`MCC_BUILD_SANITIZE` + WIN32** → fatal (mingw has no libasan/libubsan).
- **`MCC_BUILD_PROFILE` + Darwin** → fatal (no static crt0).
- **Cross-compiling with empty `MCC_EMULATOR`** → fatal (can't run the foreign
  `mcc` to build mccrt/tests/coverage). Autocorrect: force `MCC_MCCRT_USE_HOSTCC=ON`,
  `MCC_BUILD_TESTS=OFF`, `MCC_BUILD_COVERAGE=OFF`. WIN32 shared lib still fatal
  (needs `mcc -impdef`).
- **`MCC_CONFIG_BCHECK` + `!MCC_CONFIG_BACKTRACE`** → warn/autocorrect (bcheck is
  gated behind backtrace; `-b` won't link).
- **`MCC_BUILD_STATIC_EXE` + `!MCC_SINGLE_SOURCE` + `!MCC_BUILD_STATIC_LIB`** → warn
  (a non-single-source `mcc-static` links the libmcc archive, but a static link
  can't resolve the shared `libmcc.so`; keep `MCC_SINGLE_SOURCE=ON` so `mcc-static`
  is self-contained, or set `MCC_BUILD_STATIC_LIB=ON`).
- **`MCC_BUILD_DYNAMIC_EXE` + `MCC_SINGLE_SOURCE`** → `mcc-dynamic` skipped (status).
  A non-single-source driver needs libmcc's internal `ST_FUNC` helpers with external
  linkage, which only a multi-TU libmcc (`MCC_SINGLE_SOURCE=OFF`) provides; on PE a
  *shared* libmcc never exports them, so `mcc-dynamic` links the primary libmcc
  (static archive when `MCC_BUILD_STATIC_LIB=ON`). The `dist-*` presets set
  `MCC_SINGLE_SOURCE=OFF`.
- **`!MCC_ENABLE_RPATH` + `MCC_BUILD_STATIC_LIB`** → warn (rpath only for shared lib).
- **`!MCC_CONFIG_ASM`** → autocorrect `MCC_MCCRT_USE_HOSTCC=ON`.
- **`MCC_TOOLCHAIN_PROFILE` ≠ detected `MCC_CC_NAME`** → warn (profile seeds
  defaults, does not switch compilers).
- **`!MCC_CONFIG_PREDEFS`** → warn (runtime `<mccdefs.h>` dependency).
- **ELF-only knobs (NEW_DTAGS/PIE/PIC/LIBC) on WIN32/Darwin** → warn (inert).
- **CODESIGN/NEW_MACHO on non-Darwin** → warn (inert).
- **`MCC_CONFIG_SEMLOCK` non-numeric** → fatal.
- **`MCC_TOOLCHAIN_PROFILE` entry not in {auto,gcc,clang,mcc,msvc,mingw}** → fatal.
- **STRING nodes with `CHOICES`** warn on an unlisted value (typo guard).

---

## 14. Suggested test matrix

A practical set of combinations that covers the meaningful axes. Presets cover
several of these already (§2).

**Per host (Linux/macOS/Windows) — core linkage/feature axes:**

1. `debug` preset (baseline new defaults: self-contained single-source `mcc` +
   lib-linking `mcc-dynamic` against shared `libmcc.so`, bcheck+backtrace on, rpath path).
2. `release` preset (SINGLE_SOURCE dynamic exe, stripped, musl, bcheck/backtrace off).
3. `MCC_BUILD_STATIC_EXE=ON` → builds `mcc-static` (self-contained static by
   default; exercises `CONFIG_MCC_STATIC` / built-in `-run` table). Add
   `MCC_SINGLE_SOURCE=OFF` + `MCC_BUILD_STATIC_LIB=ON` to cover the static-against-`libmcc.a` path.
4. `MCC_SINGLE_SOURCE=OFF` (non-default multi-TU): `mcc`/`mcc-static` become
   driver-TU + `libmcc` builds; keeps the non-amalgamated compile path covered.
5. `MCC_CONFIG_ASM=OFF` (forces mccrt via host CC).
6. `MCC_CONFIG_PREDEFS=OFF` (runtime mccdefs path).
7. `MCC_CONFIG_BACKTRACE=OFF` (implies bcheck off) vs default on.

**Instrumentation (GCC/Clang hosts only):**

8. `sanitize` preset (`MCC_BUILD_SANITIZE=ON`).
9. `MCC_BUILD_COVERAGE=ON`, `MCC_BUILD_PROFILE=ON` (skip profile on Darwin),
   and `diagnostics` preset.

**ELF-only feature toggles (Linux):**

10. `MCC_CONFIG_PIE=ON`, `MCC_CONFIG_PIC=ON`, `MCC_CONFIG_NEW_DTAGS=ON`,
    `MCC_CONFIG_LIBC=musl`, `MCC_CONFIG_DWARF` ∈ {2,4,5,''}.

**Cross / multi-toolchain:**

11. `cross` preset (`MCC_ENABLE_CROSS=ON`) — all cross compilers.
12. `matrix` preset (`gcc;clang` × `native;cross`).
13. WIN32 target: `MCC_CONFIG_MINGW=ON` (verify sanitize is rejected).
14. A real cross target with `CMAKE_TOOLCHAIN_FILE` + `CMAKE_CROSSCOMPILING_EMULATOR`
    (e.g. qemu-arm / wine), and the same without an emulator to hit the fatal /
    autocorrect path.

**Runtime (opt-in, heavy):**

15. `MCC_QEMU_ARCHS` × `MCC_QEMU_LIBCS` under qemu-user; `qemu-docker` matrix.

Run `ctest --output-on-failure` after each (or rely on `MCC_SUPERBUILD_TEST=ON`
for matrix builds).
