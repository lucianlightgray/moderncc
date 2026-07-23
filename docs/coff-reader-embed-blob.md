---
name: coff-reader-embed-blob
description: "mcc's linker reads COFF objects/archives + short-import (MSVC/LLVM/SDK) members (Windows embed-blob); embed fn-selection works on all PE arches; embed-jit blocker is i386 stdcall __imp_ + mingw-leaf, NOT short-import (winlibs mingw is 100% long-import)"
metadata: 
  node_type: memory
  type: project
  originSessionId: 5702afd7-2f7e-4d7f-a2c4-45b9f7aa328b
---

**DONE (this host is native x86_64 mingw — build+validate happen here, not the
macOS/arm64 box the older TODO assumed).** mcc's own linker reads native COFF
objects and COFF `!<arch>` archives, so `--embed-jit` can whole-archive-link the
host-CC-produced JIT engine archive on Windows.

- `coff_load_object_file` / `coff_object_type` / `coff_map_reloc` live in
  `src/objfmt/mccpe.c` (PE-only), wired into `mcc_load_archive` (whole-archive +
  à-la-carte) in `mccelf.c` and the bare-object dispatch in `libmcc.c`
  `mcc_add_binary` — all `#ifdef MCC_TARGET_PE`. Declarations in `mcc.h`.
- Records use UNIQUE tags (`MccCoffSym`/`MccCoffRel`) and `#ifndef`-guarded
  constants because mingw's `winnt.h` IS included in mccpe.c and already defines
  `IMAGE_SYMBOL`/`IMAGE_RELOCATION`/`IMAGE_REL_*`/`IMAGE_SYM_*` — don't redefine.
- CMake: `libmcc_jitengine` ungated on WIN32 (`CMakeLists.txt` ~1956).
- Validated: bare object, à-la-carte, whole-archive all run correct; whole-archive
  of the real 2MB `libmcc-static.a` parses every member cleanly. `ctest -R jit/`
  32/32; cli/exec-basic/asm 276/276. Commit ~`115d16ec`.

**Blocker #1 RESOLVED; one blocker (#2) remains for i386/arm64-PE embed exes:**
1. ~~Embed function-selection inert on WIN32~~ — RESOLVED. x86_64-PE embed already
   ran end-to-end (see [[pe-import-library-support]]: exe self-recompiles under
   MCC_JIT=0/1). 2026-07-21 extended it to i386/arm64-PE: the root gate was that
   the §26 mode-6 entry dispatcher in `mccast.c` had no i386 branch (i386 fell
   through the generic dispatcher, never recorded the embed fn → `have_fns()`
   false); added the i386 embed-slot codegen (`ff 25 abs32` + `R_386_32` slot/body,
   commits `bd837321`/`4f091469`) + i386-PE COFF undecoration (mccpe.c
   `coff_load_object_file` strips a leading `_`; mccelf.c `mcc_load_alacarte`
   fallback) so the whole-archive engine members resolve (0 unresolved with a
   hand-built engine archive).
2. PARTLY MIS-DIAGNOSED (corrected 2026-07-21, commit `ffb4ad7d`). The claim
   "kernel32/msvcrt use short-import members mcc rejects" is **FALSE for the
   vendored winlibs mingw**: I scanned every `.a` under both mingw sysroots —
   `libkernel32.a`/`libmsvcrt.a`/`libucrt.a`/… are **100% long-import, ZERO
   short-import members** on all arches. So the i386/arm64-PE embed unresolved
   (~38 `__imp_` + ~13 mingw-leaf `strtoull`/`__mingw_printf`/`ldexpl`) come from
   (a) the i386 stdcall `__imp__NAME@N` decoration matching in `pe_check_symbols`
   and (b) linking the mingw-leaf *code* members — NOT an unparsed short-import
   format. That is the REAL remaining embed-jit blocker (still open).
   - Short-import support was still ADDED (it's the MSVC/LLVM/Windows-SDK import
     format: `00 00 FF FF` IMPORT_OBJECT_HEADER; MSVC SDK `kernel32.Lib` is
     ~1396/1399 short members). `coff_short_import_info` (mccpe.c) parses the
     20-byte header + Name\0 DLL\0, derives the export name per NameType
     (NAME/NO_PREFIX/UNDECORATE/EXPORTAS/ORDINAL); `mcc_load_alacarte` (mccelf.c)
     registers it via `pe_putimport`+`pe_import_set_alias` just like long-import,
     so `pe_check_symbols`' `__imp_`/stdcall stripping binds it. Validated on
     x86_64-PE: mcc links+runs vs the SDK `AdvAPI32.Lib` (GetUserNameA). Regression
     test `pe/short-import` (tools/mkshortimp.c synthesizes a short lib). This does
     NOT unblock the mingw embed exe (which uses long-import) — different feature.

Build here: PowerShell, prepend `C:\Users\llg\scoop\...\CLion\bin\mingw\bin` to
PATH (else gcc can't find `as`). See [[moderncc-windows-build]], [[windows-jit-embed-port]].
Git Bash mangles Windows `C:/...` PATH entries (`:` collides with separator) — use
PowerShell for builds.
