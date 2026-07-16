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

- [ ] **`exec-replay/run_atexit` + `exec-replay/errors_and_warnings` — all 4 PE
  jobs** (mingw i686, mingw x86_64, msvc arm64, sanitize-msvc arm64). Symptoms:
  `errors_and_warnings` emits only ~125 of the ~297 expected diagnostic lines —
  mcc aborts partway through diagnostics (output is *truncated*, not extended);
  `run_atexit` produces empty output where the atexit-handler text is expected.
  Root cause: JIT-default-on (`MCC_CONFIG_JIT`/`MCC_JIT_DEFAULT`) drives the
  embed-JIT recompile during these compiles. `f5cdcce9` suppressed the same
  best-effort-recompile diagnostic leak on ELF via `mccjit_error_quiet`
  (`error1`), but the PE path still diverges. Leads: `errors_and_warnings` has
  hard errors + unresolvable in-program symbols the JIT tries to recompile —
  check that `mccjit_error_quiet` actually wraps the PE recompile call
  (`mccjit_recompile_common`) and that `mccjit_embed_note`/`mccjit_embed_finalize`
  tolerate a translation unit that errored; the truncation suggests an abort/
  longjmp during compile, not just extra stderr. Repro:
  `ctest -R "exec-replay/(run_atexit|errors_and_warnings)$"` on each PE host,
  then diff `MCC_JIT=0` vs default output.
- [ ] **`jit/selftest-{lazy,pool,eligibility,liverun,fparg}` — mingw i686 only**
  (x86_64 mingw passes all 32). Symptom (`selftest-lazy`): the 7 cold/baseline
  calls return correct values, then `PROMOTE at call 8 promoted=00000000
  slot=01d60000 FAIL` — promotion hands back a NULL code pointer and every hot
  call stays `stable=no`. So the i386/PE JIT-promotion path (KGC stub build +
  dispatch-slot patch) yields no promoted body. This is specific to the 32-bit
  x86 Microsoft ABI stub path (`mccjit_make_kgc_stub` / i386 slot install),
  distinct from the x86_64 stubs that work. Repro: `ctest -R jit/selftest-lazy`
  on the i686 host; trace why `promoted` is NULL at the promotion threshold.
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
