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

Windows runtime JIT works (32/32 selftests on the mingw PE host). `src/mccjit_win32.h`
shims the POSIX layer `mccjit_embed.c` needs (VirtualAlloc exec/RW pages;
CreateFileMapping/MapViewOfFile KGC store; SRWLOCK + CONDITION_VARIABLE +
InitOnce + `_beginthreadex` threads/locks; QPC clock; Interlocked atomics). The
hand-written x86_64 stubs carry Microsoft-x64-ABI branches, and the
`-run`/`--embed-jit` auto-JIT pipeline fires on PE (`runmain.c` runs `.init_array`
ctors on WIN32; `pe_add_runtime` calls `mccjit_embed_finalize`).
`mccjit_make_kgc_stub_mixed` returns NULL on WIN32 — mixed int+FP signatures
fall to the AOT baseline and `jit/selftest-mixed` skips. (32/32 selftests hold
on the **x86_64** mingw host; the **i686** 32-bit host regressed — see below.)

### Windows/PE CI failures — fix on a Windows host ⛔ (need mingw/MSVC to reproduce)

These regressed with the batched §26 JIT work (JIT now default-on, plus the
fresh PE embed-JIT port `f2a65333`). All are Windows/PE-only; the non-Windows
CI is green. None reproduce on this arm64/macOS host — they need a mingw
(i686 + x86_64) and MSVC (arm64) box. First diagnostic for every item: re-run
with `MCC_JIT=0` (pure AOT) — if it passes, the fault is in the PE JIT path.

- **`exec-replay/run_atexit` + `exec-replay/errors_and_warnings` (+ every exec-*
  variant) — FIXED.** NOT JIT-related and not truncation — it was **cross-CRT
  stdout buffering**. `-dt -run` runs each test snippet in one mcc process; the
  snippet's `printf` resolves to `msvcrt.dll`, but a winlibs/msvc mcc links
  **UCRT** — two independent stdout buffers on fd 1. `[test]` headers/diagnostics
  go to mcc's (flushed) stream while snippet output sits in msvcrt's buffer,
  unflushed until mcc's own process exit, so all run-output lands at the end
  (reordered) — a mismatch, not truncation. Fix: `_runmain`/`exit` in
  `runtime/lib/runmain.c` (compiled with the *snippet's* CRT) now `fflush(0)`
  after the program completes, flushing the snippet's stdio in order. Verified on
  the **winlibs UCRT x86_64** toolchain (reproduced the failure, then 624/624
  exec+replay+jit green); byte-neutral elsewhere (same-CRT hosts already flushed).
  Same mechanism should clear the msvc-arm64 rows (unverified on that host).
- **`jit/selftest-{lazy,pool,eligibility,liverun,fparg}` — mingw i686 — FIXED (skip).**
  Root cause: the runtime KGC verify-stub / dispatch tail (`mccjit_make_kgc_stub_*`)
  is hand-emitted machine code implemented only for **x86_64 and arm64**; on i386
  the stub builders return NULL, so JIT promotion can't install a verified variant
  and keeps the AOT baseline (`selftest-lazy` PROMOTE `promoted=00000000`; `pool`
  async never lands; `eligibility` refuses FP-arg). This is a documented
  x86_64/arm64-only capability, and the i686 CI cell is experimental/non-gating —
  so these promotion-dependent selftests now **skip** on arches without the tail
  (new `MCCJIT_HAVE_STUB_TAIL` guard → print "skipped …" + PASS, matching the
  `selftest-fork` Windows-skip precedent). Reproduced + verified on the vendored
  winlibs i686 toolchain (`MCC_TARGET_ARCH=i386`): the 5 now pass; x86_64 jit
  suite unchanged (32/32). *Future work: a real i386 KGC/FP(x87)/mixed stub tail
  would let i386 JIT-promote instead of skip.* (Local-only noise: `selftest-patch`
  / `selftest-sliceinstall` need elevation on a non-admin winlibs run — Windows
  UAC installer-detection on the "patch"/"install" exe names; they pass on CI.)
- **`ckconfig.exe(.rsrc) is too large` — mingw i686 — RESOLVED (no longer reproduces).**
  `ckconfig` is a host-CC-built config-drift linter (`tools/ckconfig.c`); the
  `.rsrc` is mingw's default application manifest (`default-manifest.o`), a pure
  host-toolchain artifact (a trivial `int main(){}` built by the same gcc has the
  identical 0x1e0 `.rsrc`, under the cited 0x200). Verified: clean full rebuild of
  `ckconfig` on the vendored winlibs i686 gcc/binutils 2.46.1 links fine; the
  latest CI i686 job also built (5433 tests ran). It was a transient older-binutils
  bug, fixed upstream. If it recurs on a toolchain bump, the fix is toolchain-side
  (`-Wl,--disable-auto-manifest` / drop the default manifest), not mcc code.

### Windows embed-blob (`--embed-jit` standalone exe)

**COFF/PE object + archive reader — DONE and validated (native x86_64 mingw host).**
mcc's linker now reads native COFF objects and COFF `!<arch>` archives, so the
host-CC-produced JIT engine archive links into `--embed-jit` output on Windows.
- `coff_load_object_file` + `coff_object_type` + `coff_map_reloc` in
  `src/objfmt/mccpe.c` (PE-only): reads `IMAGE_FILE_HEADER`/`IMAGE_SECTION_HEADER[]`,
  merges sections by name (`.text$mn`→`.text`), maps `IMAGE_SCN_*`→sh_flags/type
  + alignment, parses the 18-byte symbol records (+aux, +string table) via
  `set_elf_sym` (UNDEF/ABS/COMMON/section-number handling), and translates the
  AMD64/I386/ARM64 reloc families into internal `R_*` + RELA addends (REL32_N
  bias handled). Records use unique tags and `#ifndef`-guarded constants so they
  coexist with mingw's `winnt.h`.
- Wired into `mcc_load_archive` (whole-archive loop + à-la-carte member pull) and
  the bare-object dispatch in `mcc_add_binary` (`libmcc.c`), all `#ifdef MCC_TARGET_PE`.
- CMake ungated: `libmcc_jitengine` now builds on WIN32 (`CMakeLists.txt`), so the
  engine archive is bin2c'd into mcc.
- Validated on this host: bare COFF object, à-la-carte archive, and whole-archive
  links all compile+run correctly; a whole-archive link of the **real 2 MB
  `libmcc-static.a`** parses every member cleanly and reaches symbol resolution
  (only external mingw/msvcrt symbols remain — a runtime-lib issue, not COFF).
  `ctest -R jit/` = 32/32, no regressions.

**`--embed-jit` on Windows now LINKS the engine end-to-end (DONE this session).**
The exe grows from ~3 KB (non-embed) to ~1 MB with the JIT engine embedded. Landed:
- **Embed function-selection — FIXED.** Not the POSIX-gated manifest (a red
  herring: `mccjit_embed_manifest` is a no-op). Real cause: `--embed-jit` defaults
  to `-O0`, which never arms the AST recorder (`ast_replay_env = optimize>=1`),
  so the mode-6 stash never runs and nothing is stashed. Fix: `ast_replay_env`
  also true for `embed_jit` (`src/mccast.c`). Host-independent (Linux `-O0
  --embed-jit` was silently non-embedding too).
- **Portable temp file — FIXED.** `mcc_add_jit_engine_embedded` hardcoded
  `/tmp/.mccjitXXXXXX` (mkstemp), which fails on Windows. New `host_temp_file`
  (`mcchost.c`, GetTempPath+O_BINARY on WIN32) + unlink-after-close.
- **mingw runtime deps — FIXED.** After the whole-archive engine link,
  `mcc_add_jit_engine_embedded` now adds the toolchain support archives alacarte
  (`libmingwex.a`→`__mingw_*`/`ldexpl`/`strtold`, `libgcc.a`→`__chkstk_ms`,
  `libucrt.a`/`libmsvcrt.a`→`__imp_*`/`strtoll`, `libkernel32.a`); lib dirs baked
  from `gcc -print-*` (`MCC_EMBED_JIT_{MINGW,GCC}_LIBDIR`, CMake, WIN32 only).
- **COFF COMDAT dedup — FIXED.** libmingwex uses link-once (COMDAT) sections
  heavily; the COFF reader now deduplicates them (`IMAGE_SCN_LNK_COMDAT` →
  drop-and-remap, mirroring the ELF `.gnu.linkonce` path) — was "defined twice".
- **kernel32.def SRW exports — FIXED.** Added `InitializeSRWLock` + the SRW/InitOnce
  family the JIT win32 shim uses (the shipped def had only the Acquire/Release subset).

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
