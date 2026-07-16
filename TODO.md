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
- [ ] **`ckconfig.exe(.rsrc) is too large (0x200 bytes)` — mingw i686 link.**
  The winlibs i686 `ld.exe` rejects `ckconfig.exe`'s `.rsrc` section at link
  time. Likely a toolchain/manifest-embedding quirk of the 32-bit winlibs
  bundle rather than an mcc defect (ckconfig is built by the host CC), but it
  fails the mingw/i686 job's build step. Investigate on the i686 host: inspect
  the emitted `.rsrc`/manifest and whether a resource/manifest strip or a
  linker flag avoids it.

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

**Remaining for a self-recompiling `--embed-jit` exe on Windows (downstream of
the reader, not COFF):**
- [ ] **Embed function-selection is inert on WIN32.** `mccjit_embed_have_fns()`
  returns false after a normal `--embed-jit` compile (even `--jit-functions=busy`),
  so `mcc_add_jit_engine_embedded` is never called and the engine is not linked
  (the emitted exe is byte-for-byte a non-embed build). The manifest that marks
  functions for embedding (`mccjit_embed_manifest`) is only called inside the
  `#if MCC_HOST_POSIX` superopt-dispatch block in `mcc.c` (~1345); wire it (and
  the embed-stash path) for the non-POSIX host.
- [ ] **mingw runtime deps of the embedded engine.** The engine archive is
  compiled by the host mingw gcc, so it references mingw runtime + import symbols
  (`__mingw_vfprintf`, `ldexpl`, `strtold`, `__chkstk_ms`, `__imp___acrt_iob_func`,
  `__imp_InitializeSRWLock`, …) that mcc's default msvcrt PE link does not
  provide. Embedding must also pull the needed mingw support libs
  (`libmingwex.a` + the right import libs) so the whole-archive engine resolves.

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
