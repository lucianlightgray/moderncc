# WIN.md — Windows / PE conditional-compilation map

This document catalogs every place the **moderncc (mcc)** source is gated on
Windows, and explains *why*. It is a reference for anyone touching the Windows
port, cross-compilation to Windows, or the PE/COFF backend.

> **HOST rows centralized (see HOST.md).** Every HOST-axis gate below now
> lives in `src/mcchost.h` / `src/mcchost.c`; the original sites call a
> `host_*` function or test a normalized `MCC_HOST_*` predicate instead of
> raw host macros. Raw host-macro tests outside `mcchost.{h,c}` are rejected
> by the `host-gate-invariant` ctest. The HOST rows in the tables below
> describe the *behavior* that moved; their file:line references are
> historical. TARGET rows are unaffected.


## The two gating axes

mcc separates *where it runs* from *what it emits*. Almost every Windows
`#if` falls into exactly one of these two buckets — keeping them straight is the
key to reading the code.

| Axis       | Macros                                              | Meaning                                                                                                                                                                     | Cross-platform?                                                                                  |
|------------|-----------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------|
| **HOST**   | `_WIN32`, `_WIN64`, `_MSC_VER`, `__MINGW*`          | The compiler *binary* is being built for / running on Windows. Governs OS-API glue: paths, process spawning, memory protection, timers, semaphores, exception registration. | No — decided by the compiler that builds mcc.                                                    |
| **TARGET** | `MCC_TARGET_PE` (build flag `MCC_TARGETOS="WIN32"`) | mcc *emits* Windows PE/COFF output. Governs the code generator, linker, symbol model, ABI, and runtime.                                                                     | **Yes** — a Linux-hosted mcc can cross-compile to PE, and a Windows-hosted mcc could target ELF. |

> **Rule of thumb:** `_WIN32`/`_WIN64` = "the machine mcc runs on."
> `MCC_TARGET_PE` = "the machine the output runs on." They are independent; a
> native Windows build happens to have both set (see `MCC_IS_NATIVE` below).

---

## 1. Central configuration — `src/mcc.h`

The umbrella header wires up both axes.

| Lines     | Condition                                   | Axis        | Why                                                                                                                                                                                                                                                                         |
|-----------|---------------------------------------------|-------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 25–33     | `#ifndef _WIN32`                            | HOST        | Pull in POSIX headers (`unistd.h`, `sys/time.h`, `dlfcn.h`) and `strtof`/`strtold` prototypes that don't exist / differ on Windows.                                                                                                                                         |
| 35–81     | `#ifdef _WIN32`                             | HOST        | Windows host glue: `<windows.h>`, `<io.h>`, `<direct.h>`, `<malloc.h>`; map `inline`→`__inline`, `snprintf`→`_snprintf`; MSVC `strtoll`/`strtof` shims; `LIBMCCAPI` dllexport; MSVC warning-disables; map `_M_ARM64`→`__aarch64__`, `_X86_`→`__i386__`; `va_copy` fallback. |
| 99–107    | `#ifdef _MSC_VER`                           | HOST        | `NORETURN`/`ALIGNED`/`PRINTF_LIKE` via `__declspec` instead of GCC `__attribute__`.                                                                                                                                                                                         |
| 117–127   | `#ifdef _WIN32`                             | HOST        | Path semantics: `IS_DIRSEP` accepts `\`, `IS_ABSPATH` handles drive letters (`C:\`), `PATHCMP`=`stricmp` (case-insensitive FS), `PATHSEP`=`;`.                                                                                                                              |
| 149–151   | `#ifdef _WIN32` (inside target auto-detect) | HOST→TARGET | When no target is forced, a Windows host defaults its **target** to `MCC_TARGET_PE`. This is the bridge that makes a native build produce PE.                                                                                                                               |
| 157–170   | `defined _WIN32 == defined MCC_TARGET_PE …` | BOTH        | `MCC_IS_NATIVE` is defined only when host and target agree (Windows host + PE target, and matching arch). Native mode enables `-run`, JIT, backtrace.                                                                                                                       |
| 228–230   | `!defined _WIN32`                           | HOST        | `CONFIG_MCCDIR` default (`/usr/local/lib/mcc`) is Unix-only; Windows locates its lib dir relative to the exe.                                                                                                                                                               |
| 271–288   | `defined MCC_TARGET_PE \|\| defined _WIN32` | TARGET+HOST | Default **system include** and **library search paths**: PE/Windows use `{B}/include`, `{B}/include/winapi`, `{B}/lib`; ELF uses the `/usr` triplet tree.                                                                                                                   |
| 293–294   | `#elif defined(MCC_TARGET_PE)`              | TARGET      | PE has no ELF interpreter — `CONFIG_MCC_ELFINTERP` is `"-"`.                                                                                                                                                                                                                |
| 967–969   | `#ifdef _WIN64`                             | HOST        | `MCCState` carries `run_function_table` — the x64 SEH unwind table registered for `-run` mode only on 64-bit Windows.                                                                                                                                                       |
| 1286–1291 | `#ifdef _WIN32`                             | HOST        | Declares `normalize_slashes`, `mcc_fopen`/`mcc_fclose` (binary-mode, slash-normalizing) on Windows; elsewhere `mcc_fopen`≡`fopen`.                                                                                                                                          |
| 1860–1866 | `#if CONFIG_MCC_SEMLOCK` / `_WIN32`         | HOST        | Multithread lock (`MCCSem`) uses a Win32 `CRITICAL_SECTION` on Windows vs pthreads/POSIX elsewhere.                                                                                                                                                                         |

---

## 2. Driver & core

### `src/libmcc.c` — main library / option parsing / library resolution
| Lines                 | Condition                                   | Axis   | Why                                                                                                          |
|-----------------------|---------------------------------------------|--------|--------------------------------------------------------------------------------------------------------------|
| 35–36                 | `#ifdef MCC_TARGET_PE`                      | TARGET | Compiles the entire PE backend into the library via `#include "mccpe.c"`.                                    |
| 52–…, 696–697         | `#ifdef _WIN32`                             | HOST   | `normalize_slashes()` and its use when recording file names.                                                 |
| 443–444               | `#ifdef _WIN32`                             | HOST   | `realpath()` → `_fullpath()`.                                                                                |
| 811                   | `CONFIG_MCC_AUTO_MCCDIR && !defined _WIN32` | HOST   | Auto-locate lib dir via `stat` is Unix-only.                                                                 |
| 990–991               | `#ifdef MCC_TARGET_PE`                      | TARGET | PE DLL loading path in the native loader.                                                                    |
| 1152                  | `#elif defined MCC_TARGET_PE`               | TARGET | Output-format dispatch default → PE.                                                                         |
| 1299, 1312–1313       | `#ifdef MCC_TARGET_PE`                      | TARGET | Library search tries `.def`/`.dll` name patterns; maps `-lm` (libm) specially since PE has no separate libm. |
| 1403–1404             | `#ifdef MCC_TARGET_PE`                      | TARGET | `mcc_pe_set_dll_characteristics()` for `-large-address-aware` etc.                                           |
| 1449–1450             | `#ifdef MCC_TARGET_PE`                      | TARGET | Accept `-Wl,--oformat=pe-*`.                                                                                 |
| 1481–1482, 1706, 2032 | `#ifdef MCC_TARGET_PE`                      | TARGET | PE-only CLI options: `-large-address-aware`, `-impdef`, `-g.pdb`.                                            |
| 1840–1841             | `#ifdef MCC_TARGET_PE`                      | TARGET | Default auto-linked lib list includes `mingw32`.                                                             |

### `src/mcc.c` — CLI front-end
Target-gated (`MCC_TARGET_PE`): PE-only help text (`-g.pdb`, `-impdef`,
`-file-alignment`/`-stack`/`-subsystem`/`-large-address-aware` linker options,
L48–145), `" Windows"` version suffix (L189), default `.exe`/`.dll`/`.o`
extensions (L256–261), and enabling the `-impdef` tool (L308).
Host-gated (`_WIN32`): millisecond timing via `GetTickCount()` vs
`gettimeofday()` (L269).

### `src/mcctools.c` — auxiliary tools (ar, impdef, cross-name)
- `MCC_TARGET_PE`: whole `mcc_tool_impdef()` (extract `.def` from a DLL, L299–401); `-win32` infix in cross-compiler exe names (L463).
- `_WIN32` (HOST): locate DLLs with Win32 `SearchPath()` (L356), `quote_win32()`/`execvp_win32()` command-line quoting + process spawn (L414–453), `.exe` suffix on cross names (L467), `_CRT_glob`/`_dowildcard` for shell-glob expansion (L480).

### `src/mccrun.c` — `-run` / JIT / in-memory execution (all HOST except two)
- `_WIN32`/`_WIN64` (HOST): `VirtualAlloc`/`VirtualProtect`/`FreeLibrary` in place of `mmap`/`mprotect`/`dlclose` (L49, 144, 363, 401); x64 SEH function-table register/unregister (`win64_add_function_table`, L60, 378, 430); reading fault registers out of a Windows `CONTEXT` per-arch (`Rip`/`Eip`/… L1129–1140); `SetUnhandledExceptionFilter`/`AddVectoredExceptionHandler` instead of POSIX signal handlers (L1217+).
- `MCC_TARGET_PE` (TARGET): relocate via `pe_output_file()` and `pe_imagebase` instead of ELF runtime/PLT (L279, 389).

### `src/mccpp.c` — preprocessor
- `_WIN32` (HOST): case-insensitive include-cache hashing (L1710); backslash normalization in `#line`/`__FILE__` (L1927).
- `MCC_TARGET_PE` (TARGET): wide-string literals encoded UTF-16 with surrogate pairs (L2316); predefine `_WIN32`/`_WIN64` macros for the *target* program (L3887).

### `src/mccgen.c` — semantic analysis / IR
Target-gated (`MCC_TARGET_PE`): `dllimport`/`dllexport` symbol attributes and
their validation (`pe_check_linkage`, L691, 1403, 10056, 10222); **stdcall name
decoration** `_func@N` (L739); `__builtin_va_start` vs `__builtin_va_arg_types`
selection (L7021); `wchar_t`/wide-literal type = `unsigned short` (UTF-16)
(L6498, 6571, 9289, 9536); `dllimport` indirection through the IAT (L7274) and
its rejection in constant expressions (L8830); 8-byte double/long-long alignment
on PE i386 (L3471); PE-specific return-reg transfer path (L7448, 7971); VLA
result handling on PE x64 (L9801). Non-PE only: `sigsetjmp`/`siglongjmp`
builtins (L1743).
Host-gated: `#if defined _MSC_VER && __x86_64__` marks `long double` volatile to
dodge an MSVC constant-folding bug (L2545).

### `src/mcctok.h` — builtin/token table
`MCC_TARGET_PE`: define `__builtin_va_start` for PE x64 variadics (L205);
define the `__chkstk` builtin used by stack probing (L326).

### `src/mccdbg.c` — DWARF / coverage
`_WIN32` (HOST): normalize `\`→`/` in the DWARF comp-dir and coverage paths
(L1078, 2448).

---

## 3. Object format & linking

### `src/objfmt/mccpe.c` — the PE/COFF writer (entire file is the PE backend)
Produces PE executables and DLLs: headers, sections, import/export tables,
relocations, resources, debug info, and x64 unwind data. Arch-specific PE
relocation types are defined for x86-64, x86, ARM, ARM64. Key entry points
(called from the target-gated sites above): `pe_output_file`, `pe_load_file`,
`pe_putimport`, `pe_add_unwind_data`.
Its own `#if`s are almost all **HOST** (`_WIN32`/`_WIN64`) glue so the PE writer
can run when cross-compiling from Unix:
- L6–12 `#ifndef _WIN32`: `stricmp`→`strcasecmp`, add `<sys/stat.h>`.
- L281 `#ifdef _WIN64`: use `IMAGE_OPTIONAL_HEADER32` from the host SDK vs the generic struct.
- L597 `#ifndef _WIN32`: `pe_shell_quote()` for Unix when invoking helper tools.
- L614 `#ifdef _WIN32`: run cv2pdb via `_spawnvp()` vs `system()`.
- L915 `#ifndef _WIN32`: `chmod()` the output executable (Windows has no exec bit).

### `src/objfmt/mccelf.c` — format dispatcher (ELF + PE + Mach-O)
`MCC_TARGET_PE` (TARGET) throughout: `.rdata` vs `.data.ro` section name (L24);
explicit entry-point symbol registration (L106); x64 PE unwind symbol setup
(L169); skip STT_NOTYPE re-export normalization (L196); don't `_`-prefix symbols
containing `@` (L484); route symbol add through `pe_putimport()` (L510); no
`dlsym()` dynamic resolution — PE uses static import tables (L1000); DLL bounds-
check init (L1520); skip `mcc_add_runtime()` — PE links no ELF runtime (L1637);
final dispatch to `pe_output_file()` (L2881). One HOST gate: slash normalization
when writing coverage (`_WIN32`, L1560).

### `src/arch/*/​*-link.c` — per-arch relocation processing (all TARGET)
Consistent PE-vs-ELF relocation differences:
- **x86_64** (L233 PC32 undefined-symbol tolerance for imports; L359/397 TLS offset from PE TLS base vs ELF TCB; L412 `RELATIVE` subtracts image base).
- **i386** (L219 `RELATIVE` image-base; L305 TLS offset).
- **arm64** (L238/308 emit NOP stub for out-of-range weak symbols instead of erroring; L368 TLS offset without the ELF 16-byte TCB; L386 `RELATIVE` image-base).
- **arm** (L395 `R_ARM_RELATIVE` image-base).

---

## 4. Architecture codegen & ABI (`MCC_TARGET_PE`, all TARGET)

This is where the Windows ABI diverges most from System V.

### `src/arch/x86_64/x86_64-gen.c`
- **Calling convention:** 4 arg registers RCX/RDX/R8/R9 (+32-byte shadow space) vs SysV's 6 RDI/RSI/RDX/RCX/R8/R9 (L717, 775).
- **Register preservation:** RSI/RDI are callee-saved on Win64, so struct-copy prologue/epilogue must save/restore them (L2255, 2274).
- **No GOT:** all addressing is RIP-relative; GOT relocs are an error on PE (L240, 363, 636).
- **TLS:** TEB access via `_tls_index` + `gen_pe_tls_base()` and `R_X86_64_TPOFF32`, not SysV FS/GS segment TLS (L305, 382, 482, 614).
- **Stack/VLA:** `alloca`/VLA always go through the helper (page-probing), never inline `sub rsp` (L2221, 2233); scratch/alloca tracking (L133).
- **Stack guard:** SysV stack-canary variable disabled on PE (L123).

### `src/arch/i386/i386-gen.c`
- **Struct return:** structs ≤8 bytes returned in EAX/EDX (PE/BSD) vs SysV field-classification (L528–538, 673, 723, 762).
- **Stack probing:** `__chkstk`/`__alloca` when a frame or struct ≥4096 bytes (L617, 797); prolog one byte larger (L685); VLA via helper (L1287).
- **TLS:** PE FS-based TEB access with `_tls_index` and `R_386_TLS_LE` (L234, 296, 348, 441).

### `src/arch/arm64/arm64-gen.c`
- **`char` signedness:** signed on PE/Mach-O; `CHAR_IS_UNSIGNED` only on ELF arm64 (L28).
- **X18 platform register** handled specially in reg allocation (L65).
- **Addressing:** ADRP/`ADR_PREL_PG_HI21` symbol addressing, no GOT (L439).
- **TLS:** `_tls_index` + TEB at `x18+0x58` via `arm64_tls_base_x30()` (L498–522).
- **Variadics:** named args in regs, all variadic on stack; PE `va_list` offset computed via `arm64_pe_param_off()` (L958, 1315–1361, 1451).
- **Stack probing:** `__chkstk` for frames ≥4096 (L1101).
- **Unwind:** `pe_add_unwind_data()` emits `.pdata`/`.xdata` at function end (L1682).

### `src/arch/{i386,arm64}/*-asm.c`
- **i386-asm.c** L1466: inline-asm clobber/save set includes RSI/RDI on PE (callee-saved).
- **arm64-asm.c** L806: PE limits allocatable integer regs to X0–X17 (no callee-saved X19–X28 for asm).

---

## 5. Runtime library — `runtime/lib/`

Mostly **HOST**-gated because these files are *compiled by mcc for the target*,
so `_WIN32`/`_WIN64` here means "this object will run on Windows."

| File | Gate | Why |
|------|------|-----|
| `bt-exe.c` | `_WIN64` | `bt_init_pe_prog_base()` finds the PE image base via `VirtualQuery()` for backtraces; `#ifndef _WIN32` stubs `__declspec`. |
| `bt-dll.c` | Windows-only file | Backtrace DLL shim: pulls function pointers from the host exe via `GetProcAddress`, uses SEH + `ExitProcess`. |
| `bt-log.c` | `_WIN32` | `DLL_EXPORT`=`__declspec(dllexport)`. |
| `bcheck.c` | `_WIN32` (many) | Bounds checker: `CRITICAL_SECTION` locks, `GetCurrentThreadId`, `TlsAlloc/TlsSetValue/TlsFree`, no mmap/fork/siglongjmp on Windows. |
| `tcov.c` | `_WIN32` | Coverage file locking via `LockFileEx`+`OVERLAPPED` (and `_get_osfhandle`) vs POSIX `fcntl`. |
| `runmain.c` | `_WIN32` | ELF `.init_array` iteration + `_runmain` only for non-Windows; PE uses `mainCRTStartup` from `win32/lib/crt1.c`. |
| `alloca.c`, `alloca-bt.c` | `_WIN32` | Guard-page stack probing (`cmp $4096` loop; ARM64 `cbz`/`cmp`) before `sub sp`, required by the Windows stack-growth model. |
| `atomic.c` | `__x86_64__ && _WIN32` | x64 atomics use the Win64 register convention (RCX/RDX) vs SysV (RDI/RSI). |

---

## 6. `runtime/win32/` — the PE target support tree

Used **only** for PE targets (staged by the build when `MCC_TARGETOS="WIN32"`).

- **`runtime/win32/lib/`** — Windows CRT startup emitted into PE output:
  - `crt1.c` / `wincrt1.c` (+ unicode `*w` variants): console / GUI entry points — parse args via `__getmainargs`, set FPU via `_controlfp`, run constructors, call `main`, exit.
  - `dllcrt1.c` / `dllmain.c`: DLL entry (`_dllstart`) dispatching `DLL_PROCESS_ATTACH`/`DETACH`.
  - `crtinit.c`: shared `.init_array`/`.fini_array` iteration.
  - `.def` files: module-definition exports for the PE DLLs.
- **`runtime/win32/include/`** — a bundled, MinGW-derived set of **Windows target
  headers** (`winapi/`, `sec_api/`, `windows.h`, `tchar.h`, `process.h`, plus
  Windows flavors of the standard headers). These are the target program's
  system headers when emitting PE; they are **not** conditionals in mcc itself,
  just the header set selected by the PE include-path config in §1.

---

## 7. Build system — `CMakeLists.txt`

Windows is selected by `MCC_TARGETOS STREQUAL "WIN32"` (set when host is `WIN32`
or a mingw profile is chosen).

| Lines                  | Purpose                                                                                                                                                                |
|------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| ~273, 299, 361         | Guard rails: no sanitizers on PE (no mingw libasan); require emulator/static for shared libmcc cross-builds; ELF-only knobs (PIE/PIC/NEW_DTAGS) inert on WIN32/Darwin. |
| ~1123–1148             | Detect Windows host/mingw target; set suffixes `.exe`/`.dll`/`.lib`.                                                                                                   |
| ~1426–1437             | `-static` link with GCC (bundle runtime); strip Release with `-s`.                                                                                                     |
| ~1504                  | Define `MCC_TARGET_PE=1` for the PE build.                                                                                                                             |
| ~1709                  | Select the PE backend (compile `mccpe.c` instead of ELF).                                                                                                              |
| ~2272, 2288, 2509–2528 | Windows install layout: `libmcc1.a`/objs under `lib/`, stage `win32/include` headers, `.def` files, example sources.                                                   |
| ~2331–2350             | Cross targets `i386-win32`, `x86_64-win32`, `arm64-win32` (`MCC_TARGET_<arch> MCC_TARGET_PE`).                                                                         |

---

## 8. CI & tests

- **`.github/workflows/ci.yml`** — a Windows MSVC job (`-LE "qemu|wine|macho"`) and a Windows MinGW job (`-DMCC_TOOLCHAIN_PROFILE=mingw`, winlibs GCC). Label exclusions keep wine/macho tests off the wrong runners.
- **`tests/qemu/run_pe_wine.cmake`** — runs cross-compiled `x86_64-win32`/`i386-win32` output under wine (wine32 for i386, wine64 for x64); skips (exit 77) if no wine; compiles with `-I runtime/win32/include`.
- **`tests/support/msvcrt_start.c`** — bare-msvcrt PE startup for tests: DLL import redirection, SEH `setjmp` intrinsics, ctor/dtor hooks for mingw-w64 compatibility.
- **`tests/{cli,exec}/runner.c`** — `_WIN32` HOST gates: `_mkdir`/`_popen` vs POSIX; shell emulation on Windows; `exec/runner.c` skips `elf`-labeled tests on Windows/Darwin.

---

## Cheat sheet: "why is this gated?"

- **Path has `\` or `C:\`, `stricmp`, `.exe`** → HOST `_WIN32`, filesystem semantics.
- **`VirtualAlloc`/`VirtualProtect`/`CONTEXT`/`AddVectoredExceptionHandler`** → HOST, `-run`/JIT OS glue in `mccrun.c`.
- **`dllimport`/`dllexport`, `_func@N`, IAT indirection, UTF-16 `wchar_t`** → TARGET `MCC_TARGET_PE`, symbol/type model in `mccgen.c`.
- **RCX/RDX/R8/R9, shadow space, `__chkstk`, callee-saved RSI/RDI, TEB TLS** → TARGET, Win ABI in `arch/*-gen.c`.
- **`.rdata`, import tables, `pe_output_file`, image-base RELATIVE relocs** → TARGET, PE object format in `mccpe.c` / `mccelf.c` / `*-link.c`.
- **`CRITICAL_SECTION`, `TlsAlloc`, guard-page probing in `runtime/lib/`** → HOST-of-the-*target* (runtime objects that will run on Windows).
