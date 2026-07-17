# TODO

## Working conventions

Mark an item in progress, then commit and push to main, before starting work on
it. Completed items are pruned once verified; the detail lives in git history.

## Runtime JIT: `--jit` / `MCC_JIT` / `MCC_CONFIG_JIT`

mcc activates the runtime JIT through a layered model: the `--jit`/`--no-jit`
CLI flags, a runtime `MCC_JIT` env var (0/1) read by the baked `.init_array`
constructor, and the CMake `MCC_CONFIG_JIT` build default (ON) → `MCC_JIT_DEFAULT`.
`--embed-jit` bakes the machinery into output. Precedence is env > flag (for
`-run`) > CMake default; `MCC_JIT=0` runs pure AOT.

JIT eligibility covers int/ptr-argument functions, string-using functions
(`MCCJIT_ROLE_DATA` captures anonymous rodata string literals as raw bytes and
rematerializes them in `mccjit_build_rec`, guarded to in-bounds rodata elfsyms
under `MCCJIT_DATA_MAX` with no relocations in range), pointer-returning
functions (the intent serializes the return pointee via `MccjitIntent.ret_type_ref`),
and enum-typed functions (`mccjit_strip_enum` recompiles enums as their integer
base). Switch functions and any unsupported shape stay on the AOT baseline.
Internal JIT-helper compiles set `mccjit_internal_compile`, so they bypass the
process-global embed registry. Runtime JIT runs on ELF, Mach-O, and PE/Windows
targets (`ctest -R jit/` green on each).

## Windows JIT-embed

Windows runtime JIT works: `ctest -R jit/` = 32/32 on the x86_64 mingw host.
`src/mccjit_win32.h` shims the POSIX layer (VirtualAlloc exec pages ·
CreateFileMapping KGC store · SRWLOCK + CONDITION_VARIABLE + InitOnce +
`_beginthreadex` · QPC clock · Interlocked atomics); the x86_64 stubs carry
Microsoft-x64-ABI branches; the `-run`/`--embed-jit` pipeline fires on PE.
`mccjit_make_kgc_stub_mixed` returns NULL on WIN32 (mixed int+FP → AOT baseline,
`jit/selftest-mixed` skips). The KGC verify-stub/dispatch tail is x86_64/arm64-only;
i686 promotion-dependent selftests skip via `MCCJIT_HAVE_STUB_TAIL` (a real i386
KGC/FP(x87)/mixed stub tail is the future work to make i386 JIT-promote).

The prior Windows/PE CI failures are resolved (detail in git history): the
`exec-replay/{run_atexit,errors_and_warnings}` mismatch was cross-CRT stdout
buffering — a winlibs/msvc mcc links UCRT while `-dt -run` snippets use msvcrt, so
snippet stdout flushed only at process exit (reordered); fixed by `fflush(0)` in
`runmain.c`'s `_runmain`/`exit`. The i686 `jit/selftest-*` failures skip on arches
without the stub tail. The ckconfig i686 `.rsrc` was a transient host-binutils
manifest quirk that no longer reproduces (toolchain-side, not mcc).

### Windows embed-blob (`--embed-jit` standalone exe)

`--embed-jit` now **links** the JIT engine end-to-end on Windows (exe ~3 KB →
~1 MB); the one remaining blocker is that it doesn't yet **run** (below). Landed
(detail in git history):
- **COFF/PE object + archive reader** — `coff_load_object_file`/`coff_object_type`/
  `coff_map_reloc` in `src/objfmt/mccpe.c` (sections/symbols/relocs → internal ELF;
  `.text$mn`→`.text` merge; COMDAT/link-once dedup), wired into `mcc_load_archive`
  (whole-archive + à-la-carte) and the bare-object dispatch, `#ifdef MCC_TARGET_PE`.
  `libmcc_jitengine` ungated on WIN32. Validated: bare object / archive / whole-
  archive links run correct; `ctest -R jit/` = 32/32, no regressions.
- **Function-selection** — `--embed-jit` at `-O0` now arms the AST recorder so the
  mode-6 stash fires (`ast_replay_env |= embed_jit`, `src/mccast.c`); previously the
  engine was never stashed/linked (silently a non-embed exe, on Linux too).
- **Portable temp file** — `host_temp_file` (`mcchost.c`, GetTempPath+O_BINARY on
  WIN32) replaces the hardcoded `/tmp` mkstemp.
- **mingw runtime deps** — after the engine link, the toolchain support archives
  (`libmingwex`/`libgcc`/`libucrt`/`libmsvcrt`/`libkernel32`) are added alacarte;
  lib dirs baked from `gcc -print-*` (`MCC_EMBED_JIT_{MINGW,GCC}_LIBDIR`).
- **kernel32.def** — added `InitializeSRWLock` + the SRW/InitOnce exports the JIT
  win32 shim uses.

- [ ] **RUNTIME: the embedded engine crashes at startup — mcc has no PE
  import-LIBRARY support.** The `--embed-jit` exe links but SIGSEGVs before `main`
  (both `MCC_JIT=0`/`=1`). Root-caused via gdb: a `memset(buf,0,4)` call goes to
  the import thunk `jmp *[IAT]`, but the IAT slot still holds the
  `IMAGE_IMPORT_BY_NAME` string pointer (the "memset" name, 16 bytes away) — the
  Windows loader **never bound the import**, so the thunk jumps into the name
  string. Deeper cause: mcc (like TinyCC) resolves DLL imports **only from `.def`
  files** (`pe_load_def`→`pe_putimport`); it has no import-library handling. The
  mingw import archives (`libmsvcrt.a`/`libucrt.a`/`libkernel32.a`) satisfy the
  engine's libc refs with import members that mcc mishandles: **short-import**
  members (`IMPORT_OBJECT_HEADER`, sig `00 00 FF FF`) are skipped by
  `coff_object_type`, and **long-import** members (regular COFF with `.idata$`
  sections) are loaded as inert data — neither assembles a bound PE import
  directory, so the IAT is never filled. (This is why the engine links but its
  imports are dead.) There is also a **UCRT-vs-msvcrt** dimension: the winlibs
  engine is UCRT-compiled, so `__imp___acrt_iob_func` et al. are not in mcc's
  msvcrt `.def` and carry an ABI mismatch.
  **Fix path (a real linker feature):** teach mcc to consume PE import libraries —
  parse short-import members (`IMPORT_OBJECT_HEADER` → `mcc_add_dllref` +
  `pe_putimport`, mirroring `pe_load_def`) and recognize long-import `.idata$`
  members (extract name/DLL, register via `pe_putimport` instead of loading the
  `.idata`), plus a UCRT export set. Non-COFF-reloc (reloc types verified:
  ADDR32NB is `.pdata`-only, SECREL is `.debug`-only).
  *Separate minor follow-up:* ADDR32NB is mapped to absolute `R_X86_64_32` (should
  be RVA); harmless today (`.pdata`-only) but wrong.

  **Implementation guide (everything mapped this session):**
  - *Repro/build (native Windows host).* winlibs x86_64 UCRT at
    `vendor/winlibs-mingw-w64-16.1.0-ucrt-x86_64/mingw64/bin` (prepend to PATH in
    PowerShell so gcc finds `as`; Git Bash mangles `C:/…` PATH). Build dir
    `cmake-winlibs` (configured `-DCMAKE_C_COMPILER=<winlibs gcc>`). Repro:
    `mcc --embed-jit hello.c -o h.exe` (hello with an eligible fn like
    `int busy(int)`), then `MCC_JIT=0 ./h.exe` → SIGSEGV. gdb: `jmp *[IAT]` thunk,
    IAT holds the `IMAGE_IMPORT_BY_NAME` string ptr. (i686 repro: `cmake-i686`,
    `-DMCC_TARGET_ARCH=i386`.)
  - *Pattern to mirror.* `pe_load_def` (`mccpe.c:1741`): `dllindex =
    mcc_add_dllref(s1, dllname, 0)->index;` then `pe_putimport(s1, dllindex, name,
    ord);` per export. mcc's `__imp_` handling already exists (`mccpe.c:1385`
    strips `__imp_`/`_imp__`), so one `pe_putimport(name)` serves both the thunk
    (`name`) and IAT (`__imp_name`) references.
  - *Short-import member* = 20-byte `IMPORT_OBJECT_HEADER`: `WORD Sig1(=0),
    Sig2(=0xFFFF), Version, Machine; DWORD TimeDateStamp, SizeOfData; WORD
    Ordinal/Hint, Type` (Type: bits 0-1 = CODE/DATA/CONST, bits 2-4 = NameType),
    then `name\0` then `dllname\0` (within `SizeOfData`). Add a
    `coff_load_short_import(s1,fd,off)` in `mccpe.c` + detect (`Sig1==0 &&
    Sig2==0xFFFF`) in `mcc_load_archive`'s member dispatch (`mccelf.c` — the same
    two spots as the COFF-object dispatch: whole-archive loop + `mcc_load_alacarte`
    member pull). NameType may prefix-strip (`?`/`@`/`_`) — handle IMPORT_OBJECT_NAME
    (as-is) first; ordinal-only imports are rare here.
  - *Long-import member* = regular COFF object (machine `64 86…`, so it currently
    loads via `coff_load_object_file`) carrying `.text` thunk + `.idata$2/$4/$5/$6`.
    Detect these (member defines `X` + `__imp_X` and has `.idata$` sections) and
    route to `pe_putimport` instead of loading the `.idata` as data. The DLL name is
    in `.idata$7` (or derivable). *Note the archives are MIXED:* `libmsvcrt.a`/
    `libucrt.a` also hold real code members (e.g. `lib64_libucrt_extra_a-*.o` math
    fns) that must still load normally — dispatch on member shape, not archive.
    `nm libmsvcrt.a` shows `T memset` (thunk) + `I __imp_memset` per import.
  - *UCRT export set.* `__imp___acrt_iob_func`/`__imp__open`/`_read`/`fstat64i32`/
    `strtoll`/`strtoull`/`__p__environ` are UCRT (not in mcc's `msvcrt.def`).
    Once import-lib parsing works they come from `libucrt.a` with the right DLL
    (`api-ms-win-crt-*` / `ucrtbase.dll`); no separate def needed. Beware UCRT vs
    msvcrt `FILE`/stdio ABI if any FILE-typed import is exercised.
  - *After it works:* re-check whether the `kernel32.def` SRW additions and the
    `mcc_add_jit_engine_embedded` static-lib list are still all needed (imports may
    then resolve straight from `libkernel32.a`). Validate: `mcc --embed-jit hello.c`
    runs correct under `MCC_JIT=0` and `=1` (self-recompile), and `ctest -R jit/`
    = 32/32 on winlibs.

## qemu-amd64 emulation noise (not compiler defects)

Under an amd64 Ubuntu container on qemu/Apple Silicon, a recurring set of ctest
failures are environment/emulation artifacts — they fail identically on clean
HEAD and/or pass when run serially in isolation:

- `macho-*` (Mach-O output cannot run on Linux),
- `asan_shadow_native_*`, `cli/bcheck_exe_static_bounds` (ASan/bounds runtime under emulation),
- `config-defines` (host triplet/OS-release specific), `run_atexit`,
  `exec-search*/errors_and_warnings` (diagnostic-text/atexit env deltas),
- `git-stamp` (fails when the working tree is dirty),
- sporadic `function_pointer`/`func_pointers`/`func_arg_struct_compare`/`complex`
  (qemu parallelism flakes; pass in isolation).

A real regression shows up as a *different* failing test. Validate JIT/codegen
work with `ctest -R jit/` + `ctest -R ast/` and per-test serial reruns, not the
full parallel sweep.
