---
name: pe-import-library-support
description: mcc consumes mingw PE import libraries + fixes COFF .bss/reloc bugs; --embed-jit exe RUNS on x86_64-PE under BOTH MCC_JIT=0 and MCC_JIT=1 (self-recompile). Also fixed CRLF trace-gate CI failure.
metadata: 
  node_type: memory
  type: project
  originSessionId: 145abde2-0efb-4fe4-a643-6df995637add
---

**DONE 2026-07-18 (commit 9ee3670b): `--embed-jit` standalone exe RUNS on Windows
under `MCC_JIT=0`** (was the flagship remaining Windows blocker in TODO.md). Output
byte-matches gcc; ~1 MB exe. The startup SIGSEGV (IAT never bound → thunk
`jmp *[IAT]` jumped into the `IMAGE_IMPORT_BY_NAME` string) is fixed by teaching
mcc's linker to consume mingw **long-import** COFF members instead of loading their
`.idata` as inert data.

Key facts (all `#ifdef MCC_TARGET_PE`):
- winlibs mingw import libs (`libucrt.a`/`libmsvcrt.a`/`libkernel32.a`) use the
  LONG-import format ONLY (regular COFF member: `.text` `jmp *__imp_X` thunk +
  `.idata$4/$5/$6/$7`). The short-import `IMPORT_OBJECT_HEADER` path was NOT needed.
- `coff_import_func_info` (mccpe.c): detects a member defining external `__imp_<X>`
  (SectionNumber>0) + referencing undefined `_head_<X>`; returns `impname`
  (`__imp_` stripped = match name), `expname` (`.idata$6` hint/name = real DLL
  export), `headsym`.
- `coff_resolve_import_dll` (mccelf.c): DLL name is INDIRECT — a fn member's
  `.idata$7` only relocs to `_head_<X>`; the DLL string lives in the archive's
  `__<X>_iname` member's `.idata$7`. Derive iname from head (`_head_<X>` ↔
  `__<X>_iname`), find via the armap, read `.idata$7` (`coff_import_dllname`).
  Wired into `mcc_load_alacarte` (import libs load à-la-carte) — that's the path
  that matters; whole-archive just skips import members.
- Registration = `mcc_add_dllref` + `pe_putimport(impname)` (NOT loading .idata),
  skipped if impname already in dynsymtab (e.g. from a .def). Doesn't `++bound`.
- **Two decoupling fixes were the hard part** (see NOTES.md "PE import libraries"):
  1. `pe_imp_alias` table (MCCState) + `pe_import_set_alias`/`pe_import_bindname`:
     a member's linker symbol ≠ DLL export (`_setjmp`→`__intrinsic_setjmp`,
     `__msvcrt_assert`→`_assert`). `pe_build_imports` emits `expname`, `pe_check`
     matches `impname`. `pe_putimport` RESETS a stale alias so a later `.def`
     re-registration wins (mcc's msvcrt.def also has `_setjmp`→msvcrt.dll, which
     DOES export it; without the reset the stale `__intrinsic_setjmp` alias +
     msvcrt.dll → STATUS_ENTRYPOINT_NOT_FOUND 0xC0000139).
  2. `pe_check_symbols` chain fix: a symbol imported both as `X` (thunk) and
     `__imp_X` (IAT) — the foreign UCRT-compiled engine does — had its `__imp_X`
     chain link clobbered by the thunk branch (`is->iat_index` overwritten by the
     fresh `IAT.<name>` sym). Now the new IAT sym threads onto the existing chain
     (`st_value = prev_chain`). Pre-existing linker code; only foreign objects hit it.

Debugging method that worked: dump `objdump -p h.exe` imports, `comm -23` each
DLL's imported names against real exports (`objdump -p /c/Windows/System32/*.dll`)
to find the ONE invalid (dll,name) pair. gdb only gives 0xC0000139 with no name.
`_fdopen`→math apiset and `_assert`/`__intrinsic_setjmp` aliases all match gcc's
own output (verified by linking the same refs with winlibs gcc) — apisets forward.

**MCC_JIT=1 embedded self-recompile — FIXED** (commit after import-lib work). It
crashed at JIT boot from TWO more `coff_load_object_file` bugs exposed by the
engine's ~1.9 MB `.bss` (mcc's own ELF compiler never emits such objects, so
untested until the embed engine):
1. NOBITS `.bss` size was read from `Misc.VirtualSize` (0 in a COFF *object*; the
   size is in `SizeOfRawData` — VirtualSize is a PE-image field). Engine `.bss` got
   0 bytes → globals past SizeOfImage. Fix: use SizeOfRawData (max with VirtualSize).
2. Section-relative relocs DOUBLE-COUNTED the in-field offset. mcc's `relocate()`
   applies R_X86_64_PC32/32/64 with add32le/add64le (field is an implicit addend
   ADDED to S+r_addend), but `coff_map_reloc` captured the field as the addend
   WITHOUT zeroing it. For a reloc vs a SECTION symbol (gcc puts the in-section
   offset in the field, e.g. `mov %eax,.bss+0x1e2024(%rip)`) the offset counted
   twice. Named-symbol relocs have field 0 → MCC_JIT=0 + all prior COFF tests fine;
   only the engine's static-global stores (JIT boot) hit it. Fix: memset the 4/8-byte
   field after capturing addend (x86_64/arm64 ADDR/REL cases). i386 REL path as-is.
Debug method: `-d pe` doesn't parse; instrument pe_assign_addresses/pass-3 with
fprintf. gdb gives `mov %eax,disp(%rip) # <VA>`; compare VA to `objdump -h` sections
+ SizeOfImage. Result: `mcc --embed-jit hello.c` correct under MCC_JIT=0 AND =1.

**BONUS FIX (same session): trace-gate-invariant Windows CI failure.** Failed on
every Windows job, passed Linux. Cause: CRLF checkout (no `*.c eol=lf` in
.gitattributes) + tracegate's `strip()` only matched `\`+`\n` continuations, not
`\`+`\r\n`, so multi-line #define macro bodies weren't blanked → 16 false-positive
"branch/function does not open with MCC_TRACE" in macros (CHECK/AST_MEMO_QUERY/
dwarf_ignore_type/…). Fix in tools/tracegate.c: handle the CR before the LF.

Build/repro: see [[moderncc-windows-build]], [[coff-reader-embed-blob]],
[[windows-jit-embed-port]]. cmake-winlibs, winlibs gcc on PATH.
`mcc --embed-jit hello.c -o h.exe` (hello has `int busy(int)` hot loop).
