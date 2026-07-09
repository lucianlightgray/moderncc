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

> **Code-facing subset:** most nodes here only steer the build. The ones that
> reach the compiler as a `-DCONFIG_MCC_*` the source reads are documented, with
> their naming convention and the drift checker, in **[CONFIG.md](CONFIG.md)**
> (ctest `config-drift-invariant`). Keep the two in sync when adding a flag.

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
| `CMAKE_INSTALL_PREFIX` | PATH | `MCC_DIST_DIR` (`<src>/dist`) | Install/staging prefix; when unset by the user it defaults to the shared `dist/` tree (see §10). Base for `MCC_INSTALL_MCCDIR` (`<prefix>/<libdir>/mcc`). |
| `CMAKE_OSX_DEPLOYMENT_TARGET` | STRING | auto | Auto-pinned when a Homebrew GNU gcc host is detected on macOS (avoids the stale 10.6 default). |
| `CMAKE_EXPORT_COMPILE_COMMANDS` | BOOL | ON (always set) | Set unconditionally before `project()`. |
| Generator (`-G`) / `-A` | — | platform default | Matters for the `msvc` matrix cells (see `MCC_MSVC_GENERATOR`). |

---

## 2. CMakePresets.json — ready-made configurations

`CMakePresets.json` is the **single source of truth** for every build in the
repo: the CI workflows (`.github/workflows/*.yml`) and the docker runners
(`tests/*/docker/run-*.sh`) invoke presets rather than hand-passing `-D`
flags — and every workflow job drives its preset through the `ci` tool
(`tools/ci.c`), not per-shell cmake step lists: `ci run-preset <name>`
(configure + build + ctest-with-junit + optional `--bench`/install, `-D`
overrides forwarded), `ci qemu`, and `ci dist` are the same C flow whether the
runner shell is bash or pwsh, and `ci bench-summary --append` writes the
step-summary section shell-agnostically. The job *matrices* are generated too:
a `plan` job runs `ci plan --job <linux|macos|msvc|qemu|dist-unix|dist-windows>`
and the jobs consume the JSON via `fromJSON`, so the schedule lives in one
place — the preset ledger tables in `tools/ci.c`, shared with `ci local`.
The dist pipeline is one reusable workflow (`dist.yml`, `workflow_call`)
invoked by both `ci.yml` (version `ci-<sha12>`) and `release.yml` (the tag);
artifacts are `mcc-<plat>` with release-style plats (`linux-x86_64-gcc`,
`macos-arm64-clang`, `windows-x86_64-msvc`, …) in both pipelines.
A preset name is the canonical label for a scenario and
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

**Developer presets** — interactive use (`cmake --preset debug`). CI and
`ci local` schedule the two that cover unique scenarios — `release` (the only
Release+musl test run) and `ast` (the only `MCC_AST` build) — plus `matrix`
(the superbuild, build-only via `--no-test`: no top-level test preset, each
cell ctests during the build), each on both x86_64 and arm64 runners. The
rest (`debug`, `cst`, `sanitize`, `diagnostics`, `cross`) are **aliases**: they
build the identical tree to a pinned `linux-gcc*` cell with an unpinned host
cc, so they are parity-exempt interactive conveniences, not CI cells
(`local-ci` likewise — it *is* the orchestrator):

| Preset | CMAKE_BUILD_TYPE | Key overrides |
|---|---|---|
| `debug` | Debug | musl OFF, bcheck ON, backtrace ON, strip OFF |
| `release` | Release | musl ON, strip ON, bcheck OFF, backtrace OFF (SINGLE_SOURCE dynamic exe) |
| `sanitize` | Debug | `MCC_BUILD_SANITIZE=ON` |
| `diagnostics` | Debug | `MCC_ALL_DIAGNOSTICS=ON` (warnings + debug + mcc_s/mcc_p/mcc_c) |
| `cross` | Debug | `MCC_ENABLE_CROSS=ON` |
| `matrix` | Debug | `MCC_TOOLCHAIN_PROFILE=gcc;clang` × `MCC_TARGETS=native;cross` |
| `local-ci` | Debug | `MCC_LOCAL_CI_AS_TEST=ON` — the `test` target reproduces the whole CI + release matrix this host can run (see `cmake/ci-local.cmake`) |
| `cst` | Debug | `MCC_CST=ON` (explicit — now the build-wide default, so effectively `debug`): the named scenario for the CST subsystem + its `tests/cst` suite |
| `ast` | Debug | `MCC_AST=ON` + `MCC_CST=ON`: the named scenario for the AST intention-IR subsystem + its `asttool` pure-lib suite and the `tests/exec` `-O1-replay` column |

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
| `macos`, `macos-cross` | `macos` | Debug, `CMAKE_C_COMPILER=$env{CC}` (clang / Homebrew gcc), `MCC_DARWIN_HOST=ON`; arm64 natively + x86_64 under Rosetta 2 (clang only — Homebrew gcc is single-target arm64) |
| `msvc` | `msvc` | Release, `MCC_TOOLCHAIN_PROFILE=msvc`, diff3 refs from `$env{}` |
| `sanitize-msvc` | `msvc` | + `MCC_BUILD_SANITIZE=ON` — MSVC AddressSanitizer (`/fsanitize=address`); test preset excludes `qemu\|wine\|macho` |
| `mingw` | `mingw` | Release, `MCC_TOOLCHAIN_PROFILE=mingw` (build-only) |

The `linux`, `macos`, `msvc`, and `mingw` build jobs upload their build
targets (executables + libraries) as artifacts — `mcc-<preset>-<arch>`
(macOS inserts the compiler: `mcc-macos-<cc>-<arch>`): the job configures
with `CMAKE_INSTALL_PREFIX` pointing at a `dist/<artifact>` dir, runs
`cmake --install` after the tests, and uploads the tree as a tar.gz (tar
first — GitHub artifacts don't preserve the exec bit). The docker `linux`
job does this by bind-mounting the host `dist/` tree at `/dist`;
`tests/ci/docker/run-ci.sh` installs into `/dist/<artifact>`. The prefix must
be set at *configure* time: the mcc runtime dir (`lib*/mcc`) installs to an
absolute path baked into the install rules, so an install-time `--prefix`
cannot re-root it.
The `dist` jobs instead package their preset's `${sourceDir}/dist` prefix
via the `package-dist` build target (which drives `ci pkg`); the `qemu` job is
test-only and uploads nothing.

**qemu presets** — CI runs one job per `(host × arch × libc)` cell natively on
the Linux runners, both x86_64 and arm64 hosts (`PRESET=qemu-<arch>`,
`LIBCS=<one>`, via `ci qemu`, no Docker; the stage3 sysroot cache is target-side
and shared across hosts); `qemu` alone is the full local matrix:

| Preset | `MCC_QEMU_ARCHS` | Common |
|---|---|---|
| `qemu` | all (`x86_64;i386;arm;arm64;riscv64`) | Debug, `MCC_ENABLE_CROSS=ON`, `MCC_QEMU_TESTS=ON`, `MCC_QEMU_LIBCS=glibc;musl` |
| `qemu-x86_64` … `qemu-riscv64` | that one arch | inherit `qemu` |

**dist presets** — release artifacts (the shared `dist.yml` pipeline, called
by both `ci.yml` and `release.yml`; cells from `ci plan --job dist-unix` /
`--job dist-windows`). Every dist
build produces *all* permutations: Release, `MCC_BUILD_TESTS=OFF`,
`MCC_SINGLE_SOURCE=OFF` (so `mcc-dynamic` builds), `MCC_BUILD_STATIC_LIB=ON` **and**
`MCC_BUILD_DYNAMIC_LIB=ON` (both `libmcc-static.a` + `libmcc-dynamic.so`),
`MCC_ENABLE_CROSS=ON` (all cross compilers), `CMAKE_INSTALL_PREFIX=${sourceDir}/dist`:

| Preset | Compiler / profile | Extra |
|---|---|---|
| `dist-linux-gcc`, `dist-linux-clang` | `gcc` / `clang` | static exe, stripped, musl |
| `dist-macos` | `clang` | dynamic, no musl (post-`strip -x`), no static exe |
| `dist-mingw` | mingw profile | static exe, stripped (PE) |
| `dist-msvc` | MSVC profile | static exe, stripped (PE) |

`package-dist` writes into `dist/` (`.tar.xz` on Unix/macOS, `.zip` on Windows)
three component archives — `mcc-<ver>-<plat>` (compiler + variants + runtime),
`libmcc-<ver>-<plat>` (headers, libs, CMake package), `mcc-cross-<ver>-<plat>`
(cross compilers + per-target `libmccrt.a`) — plus `bundle-<ver>-<plat>`, an
all-in-one archive whose contents are those three (the `bundle-` prefix signals
"contains the other archives"), and a `checksums-<plat>.txt` covering all four.
Because the install prefix and the packaging output are both `dist/`, `ci pkg`
stages into a private `.pkg/` scratch tree (inside `dist/`) and never wipes `dist/`.

**Benchmark (`MCC_BENCH`, default OFF; CI turns it on).** Builds the `mccbench`
host tool and a `bench` target that races mcc against the host compilers
(gcc/clang/mingw/msvc) over three workloads — the portable `tests/bench/corpus.c`,
`tests/diff/full_language.c`, and mcc's own single-source TU — measuring wall
time, CPU, peak RSS, object size, and functions/second (function counts come
from mcc's `-bench`). It writes `dist/bench-<plat>.txt`: system info, a
per-workload compiler table, and a test-results table parsed from a ctest
`--output-junit` file (pass `--junit`; skipped if absent). `ci pkg` folds the
report into the release `bundle-`; CI also uploads it and posts it to the job
summary.

**Parity is machine-checked**: `ci parity` (run in every build as the
`preset-parity-invariant` ctest) diffs the non-hidden configure presets against
workflow coverage (the presets scheduled by each `ci plan --job <x>` a workflow
references, plus literal preset mentions, across `ci.yml`/`release.yml`/
`dist.yml`) and against the ledger tables (`tools/ci.c` `PS_*`/`PLAN_*`),
failing on any preset without both. The curated exemptions are `local-ci`, the
`qemu` umbrella, and the five alias dev presets; and
`diff <(ci matrix | sort -u) <(ci parity --list)` is empty by construction.

Build presets exist for every configure preset; test presets exist for all
except the build-only (`mingw`), test-less (`dist-*`), superbuild
(`matrix`), and `local-ci` (whose own build `test` target reproduces CI) ones —
the superbuild registers no top-level tests and runs
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
| `MCC_BENCH` | BOOL | OFF | | always | Build the `mccbench` tool and the `bench` target that races `mcc` against the host compilers and writes `dist/bench-<plat>.txt`. CI/release turn it on (see §benchmark). |
| `MCC_MCCRT_USE_HOSTCC` | BOOL | OFF | | always | Build native `mccrt` with the host CC instead of `mcc` (faster bcheck). Auto-forced ON when no emulator / asm disabled. |
| `MCC_EMBED_MCCRT` | BOOL | **ON** | | always (ELF/Mach-O; forced OFF on WIN32) | Bake `libmccrt.a` into the `mcc` binary (self-contained; no sidecar `.a` needed at link time). The embedded loader streams it through a temp fd to the ordinary alacarte archive loader. Forces the native `mccrt` to be host-CC built (like `MCC_MCCRT_USE_HOSTCC`) to break the mcc→archive→mcc build cycle. Only the primary `mcc` target embeds; static/musl/cross variants keep the sidecar. |
| `MCC_CONFIG_AUTOCORRECT` | BOOL | OFF | | always (advanced) | Non-strict: auto-correct inert/non-runnable combos instead of only warning. |
| `MCC_MINGW_SOURCE` | STRING | `winlibs` | winlibs, multilib | always (advanced) | Source for the `mingw-toolchain` download. |

---

## 4. Diagnostics / instrumentation (`GROUP "Diagnostics"`)

| Value | Type | Default | Gate | Purpose |
|---|---|---|---|---|
| `MCC_ALL_DIAGNOSTICS` | BOOL | OFF | GNU/Clang host (advanced) | Everything-on: verbose warnings + debug info + build `mcc_s`/`mcc_p`/`mcc_c`. |
| `MCC_BUILD_SANITIZE` | BOOL | OFF | GNU/Clang or MSVC host | Build `mcc_s` (sanitized): GCC/Clang → ASan+UBSan; MSVC → ASan (`/fsanitize=address`); mingw/PE with no libasan → trap-mode UBSan. Alignment excluded (mcc's intentional unaligned access). Exercised by the `sanitize-smoke` ctest. |
| `MCC_BUILD_PROFILE` | BOOL | OFF | GNU/Clang host; **not** Darwin | Build `mcc_p` (`-pg -static`). Fatal on Darwin (no static crt0). |
| `MCC_BUILD_COVERAGE` | BOOL | OFF | GNU/Clang host; needs runnable mcc | Build `mcc_c` (coverage instrumentation). |

`MCC_BUILD_PROFILE` requires a GCC/Clang **host** compiler (fatal otherwise);
`MCC_BUILD_SANITIZE` is broader — it accepts a GCC/Clang, **MSVC**, or **mingw**
host (only a non-GCC/Clang/MSVC host is fatal). Presets: `sanitize` (native
GCC/Clang host → ASan+UBSan) and `sanitize-msvc` (Windows/MSVC → AddressSanitizer,
`/fsanitize=address`). There is no dedicated mingw-sanitize preset: mingw/PE
trap-mode UBSan (no libasan/libubsan runtime needed) is reached by adding
`MCC_BUILD_SANITIZE=ON` to a `mingw`-profile build.

---

## 5. mcc feature toggles (`GROUP "mcc features"` / `"Darwin"`)

These bake `CONFIG_*` values into the compiler and change its runtime behavior.

| Value | Type | Default | Choices | Gate | Purpose |
|---|---|---|---|---|---|
| `MCC_CONFIG_MINGW` | BOOL | OFF | | always | Build a WIN32/mingw target (forces `MCC_TARGETOS=WIN32`). |
| `MCC_CONFIG_BACKTRACE` | BOOL | **ON** (Debug) / OFF | | always | Stack backtraces (`-bt` / `-run`). Debugging aid: defaults ON only for Debug builds. |
| `MCC_CONFIG_BCHECK` | BOOL | **ON** (Debug) / OFF | | **MCC_CONFIG_BACKTRACE** | Bounds checker (`-b`). Requires backtrace to link; defaults ON only for Debug builds. |
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
| `MCC_CST` | BOOL | **ON** | | always (`GROUP "Advanced"`) | Build the CST database subsystem (side-recorded concrete syntax tree; LSP/`-g`/opt substrate) → `CONFIG_MCC_CST=1`. On by default; codegen is byte-identical either way (guarded by the codegen-identity gate). The `cst` preset builds/runs the `tests/cst` suite explicitly. |
| `MCC_AST` | BOOL | **ON** | | always (`GROUP "Advanced"`) | Build the AST intention-IR subsystem (docs/AST.md) → `CONFIG_AST=1`. A pure side-channel like the CST: `-O0` never builds it and stays byte-identical; `-O1` lowers it and replays through the vstack API. The `ast` preset builds/runs the `tests/exec` `-O1-replay` column + the `asttool` pure-lib suite. |
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

Every automated download / vendored toolchain lands under one root,
`MCC_VENDOR_DIR` (`<src>/vendor`, gitignored). Each subdir carries a fully
specific `brand-version-arch` name so nothing collides and a version bump lands
in a fresh directory:

| Subdir | What |
|---|---|
| `winlibs-mingw-w64-<ver>-ucrt-x86_64` / `-i686` | WinLibs mingw-w64 toolchain (dual-arch) |
| `mingw-w64-multilib` | user-supplied single-gcc multilib mingw |
| `llvm-clang/<clang+llvm-ver-arch-os>` | fetched LLVM/clang release |
| `musl-src` | musl libc git source checkout |
| `musl-sysroot` | musl sysroot built from `musl-src` |
| `gnu-gcc` | optional drop-in native GNU gcc (`gnu-gcc/bin`) |
| `gentoo-stage3-<arch>-<libc>` | qemu-user cross-conformance rootfs |

The compiler-resolution functions check `vendor/` **first** (vendor-first), so a
fetched toolchain is auto-detected on the next configure with no extra flags —
download once, share across build dirs (and, via a bind mount, into the docker
runners).

Consumed by the `mingw-toolchain` / `clang-toolchain` / `vendor-musl` /
`musl-sysroot` fetch targets and the `MCC_QEMU_TESTS` rootfs fetch.

| Value | Type | Default | Purpose |
|---|---|---|---|
| `MCC_VENDOR_DIR` | PATH | `<src>/vendor` | **Input** root for all downloaded/vendored toolchains (gcc/clang/mingw/musl) + qemu rootfs. Checked first by autodetect. |
| `MCC_DIST_DIR` | PATH | `<src>/dist` | **Output** root: default `CMAKE_INSTALL_PREFIX` (install staging) + `package-dist` bundle output. Gitignored; docker-mounted at `/dist` so CI/host share one artifact tree. |
| `MCC_MINGW_DIR` | PATH | `vendor` | Parent dir for the `winlibs-mingw-w64-*` / `mingw-w64-multilib` trees. |
| `MCC_MINGW_WINLIBS_VER` | STRING | `16.1.0-ucrt` | Version tag in the WinLibs vendor dir name (keep in sync with the URLs). |
| `MCC_MINGW_WINLIBS_X86_64_URL` / `_SHA256` | STRING | pinned | WinLibs x86_64 zip + hash. |
| `MCC_MINGW_WINLIBS_I686_URL` / `_SHA256` | STRING | pinned | WinLibs i686 zip + hash. |
| `MCC_MINGW_MULTILIB_URL` / `_SHA256` / `_SUBDIR` | STRING | `''` / `''` / `mingw64` | Single-gcc multilib archive (required when `MCC_MINGW_SOURCE=multilib`). |
| `MCC_CLANG_DIR` | PATH | `vendor` | Parent dir for the `llvm-clang` tree. |
| `MCC_CLANG_URL` / `_SHA256` / `_SUBDIR` | STRING | host-dependent | LLVM release archive, hash, and top-level dir. |
| `MCC_CONFIG_EXTRA` | FILEPATH | `config-extra.cmake` | Optional extra CMake config included before options. |

---

## 11. Test-suite knobs (only when `MCC_BUILD_TESTS=ON`)

| Value | Type | Default | Purpose |
|---|---|---|---|
| `MCC_REF_CC` | FILEPATH | host cc | GCC-compatible reference compiler for differential tests (mcctest/-bcheck). |
| `MCC_DIFF3_GCC` / `MCC_DIFF3_CLANG` | FILEPATH | `''` | gcc/clang reference compilers for the three-way `diff3` differential. On Windows the `msvc`/`sanitize-msvc` presets pass these from `$env{}` (there is no host gcc/clang otherwise). |
| `MCC_GCCTESTSUITE_PATH` | PATH | `''` | `gcc.c-torture` dir for the `gcctestsuite` target. |
| `MCC_ARM_CROSS_COMPILE` | STRING | `''` | binutils prefix (e.g. `arm-linux-gnueabi-`) for `arm-asm-testsuite`. |
| `MCC_DARWIN_HOST` | BOOL | OFF | Tests run on a macOS/darling host with real libSystem (enables `macho-libsystem-kernel-fused`). The `macos` preset sets it (the CI runner is real Darwin). |
| `MCC_CROSS_DIR` | PATH | `''` | The cross-compiler **build** dir the cross-consuming tests read `mcc-<arch>-{osx,win32}` from — every `macho-*` cross test and `pe-wine-conformance`. The `qemu`/`qemu-*` presets set it; without it those tests self-skip. |
| `MCC_WINE` | FILEPATH | auto | wine/wine64 runner for `pe-wine-conformance` (installed by the `setup-wine` target). Empty ⇒ the wine PE test skips. |
| `MCC_QEMU_TESTS` | BOOL | OFF | Master on/off for the qemu-user cross-conformance matrix below (the `qemu` presets set it). |
| `MCC_QEMU_MIRROR` | STRING | Gentoo releases | Base URL for qemu-user rootfs downloads. |
| `MCC_QEMU_ARCHS` | STRING (list) | `x86_64;i386;arm;arm64;riscv64` | Architectures to exercise under qemu-user. |
| `MCC_QEMU_LIBCS` | STRING (list) | `glibc;musl` | C libraries to exercise. |
| `MCC_QEMU_DLDIR` | PATH | `vendor` | Parent dir for the Gentoo stage3 rootfs trees (`vendor/gentoo-stage3-<arch>-<libc>`). |
| `MCC_QEMU_DOCKER_ARCHS` / `_LIBCS` | STRING | `''` | Passed to the `qemu-docker` matrix (empty = image default). |

**Platform test families & host tooling.**

- **Darwin / Mach-O** (label `macho`, filtered out of the `msvc`/`sanitize-msvc` test
  presets). Cross-consuming drivers `macho-structural`, `macho-codegen-run`,
  `macho-image-run`, `macho-apple-libc` read the `mcc-x86_64-osx` cross from
  `MCC_CROSS_DIR` (else self-skip → "requires the (x86_64-)osx cross compilers"). Native
  self-skipping drivers: `macho-conformance-native`, `macho-stack-protector`,
  `macho-universal` (the last exercises the `machofat` tool — a self-contained universal/fat
  Mach-O combiner + ad-hoc `codesign`, built only when `MCC_TARGETOS=Darwin`; its 2-slice case
  shells out to `xcrun --show-sdk-path` for the SDK). `macho-libsystem-kernel-fused` needs
  `MCC_DARWIN_HOST=ON`. `qemu-arm64-osx` (labels `qemu;macho`) covers arm64-Darwin codegen
  under qemu.
- **Windows / PE** (label `wine`). `pe-wine-conformance` runs mcc's PE output under
  `MCC_WINE` using the `mcc-x86_64-win32` cross from `MCC_CROSS_DIR` (`setup-wine` installs
  wine); `pe-native-conformance` runs only on a native WIN32 host. mcc's PE output links the
  legacy `msvcrt.dll` import set (`msvcrt`,`kernel32`,`user32`,`gdi32`), so `-b` bounds
  checking, the `parts-suite`, and (on arm64) the `mcctest` differential self-skip on PE.
  `compile.win32` runs only for a WIN32 target. `setup-mingw` installs the mingw toolchain
  (or the `mingw` preset's superbuild fetches WinLibs GCC `16.1.0-ucrt`).

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

- **`MCC_BUILD_PROFILE` needs a GCC/Clang host** → fatal on MSVC host.
- **`MCC_BUILD_SANITIZE`** → works on GCC/Clang (ASan+UBSan), MSVC (ASan), and
  mingw/PE (trap-mode UBSan; no libasan ships). Fatal only on a non-GCC/Clang/MSVC host.
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
7. `MCC_CONFIG_BACKTRACE=OFF` (implies bcheck off) vs the Debug default on —
   the Release presets cover this slice for free (backtrace/bcheck default OFF
   outside Debug builds).

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
13. WIN32 target: `MCC_CONFIG_MINGW=ON`; and `MCC_BUILD_SANITIZE=ON` on Windows —
    verify it is **accepted** now (MSVC → ASan `/fsanitize=address`; mingw/PE → trap-mode
    UBSan, no runtime lib), not rejected. The `sanitize` configure-fatal fires only on a
    non-GCC/Clang/MSVC host.
14. A real cross target with `CMAKE_TOOLCHAIN_FILE` + `CMAKE_CROSSCOMPILING_EMULATOR`
    (e.g. qemu-arm / wine), and the same without an emulator to hit the fatal /
    autocorrect path.

**Runtime (opt-in, heavy):**

15. `MCC_QEMU_ARCHS` × `MCC_QEMU_LIBCS` under qemu-user; `qemu-docker` matrix.

Run `ctest --output-on-failure` after each (or rely on `MCC_SUPERBUILD_TEST=ON`
for matrix builds).
