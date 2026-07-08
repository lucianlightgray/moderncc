# CONFIG.md — the `CONFIG_MCC_*` in-code configuration surface

This is the source of truth for the **code-facing** build configuration: the
`CONFIG_MCC_*` preprocessor macros the compiler branches on, and how they are
supplied by the build. For the full CMake **node** table (all 55
`mcc_config_node`s, presets, cache flags — many of which only steer the build and
never reach the code) see [BUILD.md](BUILD.md). This file covers only the subset
that becomes a `-DCONFIG_MCC_*` the source reads.

A `tools/ckconfig` checker (ctest `config-drift-invariant`) enforces everything
below so it cannot rot — see [The checker](#the-checker).

## The flow

```
CMake option / mccbuild flag ──emits──▶  -DCONFIG_MCC_X=…  ──▶  #if CONFIG_MCC_X
   (MCC_CONFIG_X or MCC_X)                                        in the source
                                   header default (mcc.h etc.):
                                     #ifndef CONFIG_MCC_X
                                     #define CONFIG_MCC_X <default>
```

Every code-facing flag has three ends that must agree:

1. **Emitter** — `CMakeLists.txt` (and the parallel `mccbuild` in
   `tools/build.c`) turn a build option into a `-DCONFIG_MCC_X=value`.
2. **Default** — a header (`src/mcc.h`, …) provides an `#ifndef`-guarded default
   (or an unconditional constant) so the source still compiles with no `-D`.
3. **Reader** — the source uses `#if CONFIG_MCC_X` / `#ifdef` / a value reference.

## Naming convention (drift class *c*)

The emitted macro is always `CONFIG_MCC_<THING>`. The CMake option that drives it
is **not** spelled the same — two systematic transforms apply:

| CMake option        | emitted define            |
|---------------------|---------------------------|
| `MCC_CONFIG_<THING>` | `CONFIG_MCC_<THING>` (swap `MCC_CONFIG`↔`CONFIG_MCC`) |
| `MCC_<THING>`        | `CONFIG_MCC_<THING>` (prefix `CONFIG_`) |

Examples: `MCC_CONFIG_BCHECK`→`CONFIG_MCC_BCHECK`, `MCC_CST`→`CONFIG_MCC_CST`,
`MCC_SYSINCLUDEPATHS`→`CONFIG_MCC_SYSINCLUDEPATHS`. Boolean flags emit `=0/=1`;
string flags go through `mcc_def_str(_mccdefs CONFIG_MCC_X "…")`.

## The current surface

Boolean/value flags emitted by the build and read by the compiler (regenerate
with `ckconfig --list src CMakeLists.txt`):

`CONFIG_MCC_ASM` · `AUTO_MCCDIR` · `BACKTRACE` · `BCHECK` · `CPUVER` ·
`CROSSPREFIX` · `CRTPREFIX` · `CST` · `ELFINTERP` · `LIBPATHS` · `MUSL` · `PIC` ·
`PIE` · `PREDEFS` · `SEMLOCK` · `SWITCHES` · `SYSINCLUDEPATHS`.

### Intentional exceptions

These deliberately break the "emitted ⇔ read" symmetry; `ckconfig` allowlists
them (with the rationale in `tools/ckconfig.c`):

| Macro | Class | Why |
|---|---|---|
| `CONFIG_MCC_UCLIBC` | emitted, **not read** (dead) | legacy uClibc marker, kept for provenance; "no path effect on modern hosts" |
| `CONFIG_MCC_BACKTRACE_ONLY` | **read**, no CMake node | opt-in "backtrace-only" runtime build variant; `#ifndef`-graceful, set outside the config surface |
| `CONFIG_MCC_ELFINTERP_ARMHF` | **read**, header `#define` | code-internal constant (`src/mcc.h`), not a build option |
| `CONFIG_MCC_TOOLHOST` | **read**, header `#define` | set by `tools/toolhost.h` when mcc source is built as a host tool |
| `CONFIG_MCC_STATIC` | emitted **per-target** + read | driven by `MCC_BUILD_STATIC_EXE` / the `mcc_p` target (a `MCC_BUILD_*` option, not a `MCC_CONFIG_*`/`MCC_*` one), emitted with `target_compile_definitions` rather than the `_mccdefs` list — so it is outside the `ckconfig --list` surface above yet is genuinely read (`src/mcchost.{c,h}`, `src/libmcc.c`: static `-run` symbols from a built-in table). **Set only on Linux** (`NOT MSVC AND NOT APPLE AND NOT WIN32`) — inert/undefined on **Darwin** and **WIN32**. |

**Platform note.** The Darwin/Win32-specific *build* knobs — `MCC_CONFIG_NEW_MACHO`,
`MCC_CONFIG_CODESIGN` (Darwin only) and `MCC_CONFIG_MINGW` (Win32), plus `MCC_CONFIG_DWARF`
— steer the build only and emit **no** `-DCONFIG_MCC_*` the source reads, so they are
catalogued in [BUILD.md](BUILD.md), not here. `CONFIG_MCC_STATIC` (above) is the one
`MCC_BUILD_*`-driven macro the code actually branches on.

## The checker

`tools/ckconfig.c` (ctest `config-drift-invariant`, built next to `hostgate`)
cross-checks the three ends and **fails the build** on drift:

- **DRIFT(a)** — the code reads a `CONFIG_MCC_X` that no emitter mentions and no
  header `#define`s (an implicit/undefined config).
- **DRIFT(b)** — an emitter emits `-DCONFIG_MCC_X` the code never reads (a dead
  define).

It scans `src/` for reads + header `#define`s, `tools/` for code-internal
`#define`s, and treats both `CMakeLists.txt` and `tools/build.c` (the `mccbuild`
emitter) as providers. When you add or rename a code-facing flag, update **all**
of: the emitter(s), the header default, the reader, this file, and — if it is a
deliberate asymmetry — the `ALLOW_*` lists in `tools/ckconfig.c`.

Run manually:

```sh
tools/ckconfig --list src CMakeLists.txt   # inventory + classification
tools/ckconfig src CMakeLists.txt          # just the pass/fail gate
```

See also: [BUILD.md](BUILD.md) (full CMake node table), the host-gate invariant
(`tools/hostgate.c`, `MCC_HOST_*` macros — the analogous rule for host macros).
