# tcc preprocessor feature matrix

Every `#if`/`#ifdef`/`#ifndef`/`#elif` condition in the compiler sources
(`*.c`/`*.h`, excluding the `elf.h`/`dwarf.h`/`coff.h`/`stab.h` system tables)
reduces to the macros below — 1,100 directives, 360 distinct conditions,
driven by ~70 identifiers. They are grouped by **what controls them**:
user-settable build knobs, target-derived, or host-autodetected.

Regenerate the raw frequency list with:

```sh
files=$(ls *.c *.h | grep -vE '^(elf|dwarf|coff|stab)\.h$')
grep -hE '^\s*#\s*(if|ifdef|ifndef|elif)\b' $files \
  | grep -oE '[A-Za-z_][A-Za-z0-9_]*' \
  | grep -vwE 'defined|sizeof' | sort | uniq -c | sort -rn
```

The CMake option ↔ macro mapping lives in `CMakeLists.txt` section 1 (options)
and section 3a (the dynamic preprocessor flag catalog).

---

## 1. Target backend — CPU (mutually exclusive, exactly one defined)

| Macro | Set by | Effect when defined |
|---|---|---|
| `TCC_TARGET_I386` | `-DTCC_CPU=i386` | 32-bit x86 codegen/linker; `PTR_SIZE=4 LONG_SIZE=4 LDOUBLE_SIZE=12` |
| `TCC_TARGET_X86_64` | x86_64 (host default) | 64-bit x86 codegen; `PTR_SIZE=8 LDOUBLE_SIZE=16` |
| `TCC_TARGET_ARM` | arm | 32-bit ARM codegen; pulls in `TCC_ARM_*` ABI sub-flags |
| `TCC_TARGET_ARM64` | arm64 | AArch64 codegen; `LDOUBLE_SIZE=16` |
| `TCC_TARGET_RISCV64` | riscv64 | RISC-V 64 codegen |
| `TCC_TARGET_C67` | cross only | TI C67xx codegen (uses COFF; no runtime lib) |

## 2. Target object format / OS (layered on the CPU)

| Macro | Set by | Effect |
|---|---|---|
| `TCC_TARGET_PE` | WIN32 target / `--config-mingw32` | PE/COFF output, win32 paths, `.def` import libs, SEH |
| `TCC_TARGET_MACHO` | Darwin target | Mach-O output, leading underscore, codesign/version-min |
| `TCC_TARGET_COFF` | C67 | COFF output |
| `TCC_TARGET_UNIX` | non-PE/Mach-O | ELF output path |
| `TARGETOS_FreeBSD` / `OpenBSD` / `NetBSD` / `_FreeBSD_kernel` / `BSD` | OS detect | Per-BSD ELF interp path + syscall/`SIGSTKSZ` quirks |
| `TARGETOS_ANDROID` | Android | Bionic libc paths, PIE default |
| `CONFIG_NEW_MACHO` | `--config-new_macho=no` (default auto/1) | `LC_DYLD_CHAINED_FIXUPS` vs legacy Mach-O linkedit |

## 3. Feature toggles (user-settable; default ON unless noted) — the main "settings"

| Macro | CMake option | Default | Effect when 0 / disabled |
|---|---|---|---|
| `CONFIG_TCC_BCHECK` | `TCC_CONFIG_BCHECK` | 1 | Drops bounds-checker (`-b`); ~91 guarded blocks compile out |
| `CONFIG_TCC_BACKTRACE` | `TCC_CONFIG_BACKTRACE` | 1 | No `-bt`/`rt_printline` stack traces (also gates libtcc_test_mt) |
| `CONFIG_TCC_BACKTRACE_ONLY` | (derived) | — | Backtrace without full bcheck runtime |
| `CONFIG_TCC_ASM` | always (non-C67) | on | Inline/`.S` assembler support |
| `CONFIG_TCC_PREDEFS` | `TCC_CONFIG_PREDEFS` | 1 | 0 = load `<tccdefs.h>` at runtime instead of compiled-in `tccdefs_.h` |
| `CONFIG_TCC_PIE` | `TCC_CONFIG_PIE` | off | Generate position-independent **executables** |
| `CONFIG_TCC_PIC` | `TCC_CONFIG_PIC` | off | Position-independent **code** (auto-on with PIE) |
| `CONFIG_SELINUX` | `TCC_WITH_SELINUX` | off | `mmap` executable memory for `-run` |
| `CONFIG_TCC_SEMLOCK` | `TCC_CONFIG_SEMLOCK` | 1 | 0 = no thread lock around shared state |
| `CONFIG_NEW_DTAGS` | `TCC_CONFIG_NEW_DTAGS` | off | `DT_RUNPATH` instead of `DT_RPATH` |
| `CONFIG_CODESIGN` | `TCC_CONFIG_CODESIGN` | auto (Darwin) | ad-hoc codesign emitted binaries |
| `CONFIG_DWARF_VERSION` | `TCC_CONFIG_DWARF` | 0 | 0 = stabs debug; 2–5 = DWARF version |
| `CONFIG_RUNMEM_RO` | (source) | — | Make JIT code pages read-only after reloc |
| `TCC_EH_FRAME` | (derived: ELF & !ARM & !BSD) | on | Emit `.eh_frame` unwind tables |

## 4. Build-mode / amalgamation knobs

| Macro | Set by | Effect |
|---|---|---|
| `ONE_SOURCE` | `TCC_ONE_SOURCE` / cross (=1); native libtcc & tcc (=0) | 1 = `libtcc.c` `#include`s every other `.c` into one TU |
| `TARGET_DEFS_ONLY` | internal (per `*-gen.c` include) | Pull only the backend's macro defs, not its code |
| `ELF_OBJ_ONLY` | internal (cross w/o native ELF exe) | Emit `.o` only; skip exe/dynlink emission (~15 blocks) |
| `LIBTCC_AS_DLL` | shared-lib build | `__declspec(dllexport)`; win32 tccdir from `tcc.dll` location |
| `CONFIG_TCC_STATIC` | tcc_s / static link | No `dlopen`; static symbol resolution only |
| `NEED_BUILD_GOT` | internal (i386/arm) | Manual GOT/PLT construction path |

## 5. Native / JIT

| Macro | Set when | Effect |
|---|---|---|
| `TCC_IS_NATIVE` | target == host | Enables `-run`/JIT, native bt/bcheck, `tcc_relocate` (~21 blocks) |
| `TCC_TARGET_NATIVE_STRUCT_COPY` | x86_64 native | Use memcpy-style struct copy matching host ABI |
| `TCC_CROSS_TEST` | test build | Force cross paths even when native (test harness) |

## 6. Runtime paths & identity (string macros; tested via `#ifdef`)

| Macro | Set by | Effect |
|---|---|---|
| `CONFIG_TCCDIR` | `TCC_INSTALL_TCCDIR` / prefix | Compiled-in default tccdir (libtcc1.a + headers) |
| `CONFIG_TCC_AUTO_TCCDIR` | `TCC_AUTO_TCCDIR` | Enables build-path-first discovery, falling back to `CONFIG_TCCDIR` |
| `CONFIG_SYSROOT` `CONFIG_TRIPLET` `CONFIG_TCC_SYSINCLUDEPATHS` `CONFIG_TCC_LIBPATHS` `CONFIG_TCC_CRTPREFIX` `CONFIG_TCC_ELFINTERP` | `--sysroot/--triplet/...` | Override system include/lib/crt/dynamic-linker search paths |
| `CONFIG_TCC_CROSSPREFIX` | cross | Prefixes `libtcc1.a`/tool names (e.g. `x86_64-win32-`) |
| `CONFIG_TCC_SWITCHES` | `--tcc-switches` | Options injected into every `tcc_new()` |
| `CONFIG_TCC_MUSL` / `CONFIG_TCC_UCLIBC` | `--config-musl/-uClibc` | libc-specific interp/paths |
| `CONFIG_TCC_LIBGCC` / `TCC_LIBTCC1` | — | Override support-lib names |
| `CONFIG_OS_RELEASE` | `--os-release` | Mach-O version-min |
| `TCC_GITHASH` | git | Version banner string |

## 7. ARM ABI sub-features (derived from `cc -dM` probe; ARM only)

| Macro | Meaning |
|---|---|
| `TCC_ARM_EABI` | EABI calling convention |
| `TCC_ARM_VFP` | Hardware VFP registers available |
| `TCC_ARM_HARDFLOAT` | Float args in VFP regs (vs soft-float) |
| `CONFIG_TCC_CPUVER` / `__ARM_FEATURE_IDIV` | ARM arch level / hardware divide |
| `TCC_USING_DOUBLE_FOR_LDOUBLE` | `long double` == `double` (arm/riscv/win) → `LDOUBLE_SIZE=8` |

## 8. Auto-detected (host compiler/platform — **not** settings)

`_WIN32 _WIN64 _MSC_VER __APPLE__ __linux__ __CYGWIN__ __FreeBSD__ __NetBSD__
__OpenBSD__ __DragonFly__ __ANDROID__ __GNUC__/__GNUC_MINOR__ __clang__
__TINYC__ __x86_64__ __i386__ __arm__ __aarch64__ __riscv __LP64__ __cplusplus
__dietlibc__ SIGSTKSZ` — picked up from the toolchain; gate host-specific
code, never set by you.

## 9. Developer/debug source toggles (off; edit source to enable)

`MEM_DEBUG TAL_DEBUG TAL_INFO PP_DEBUG INC_DEBUG BF_DEBUG SYM_DEBUG PARSE_DEBUG
ASM_DEBUG DEBUG_RELOC DEBUG_VERSION ASSEMBLY_LISTING_C67 PE_PRINT_SECTIONS` —
print/trace instrumentation; no build-system hook.

---

**Derived size macros** `PTR_SIZE` / `LONG_SIZE` / `LDOUBLE_SIZE` are computed
from the chosen `TCC_TARGET_*` (not set directly) and drive ~60 codegen blocks.

**Key rule:** pick exactly one CPU backend (§1), layer one OS format (§2), then
toggle features (§3) — everything in §7–8 is inferred from the target and
toolchain.
