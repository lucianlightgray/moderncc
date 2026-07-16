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

### Windows embed-blob (`--embed-jit` standalone exe) — 🚧 IN PROGRESS

Implementing the COFF/PE object + archive reader in mcc's linker (native Windows
mingw host, so build + validate happen here).

mcc's linker reads only ELF and `!<arch>` archives, so the JIT engine archive
(`libmcc_jitengine.a` — a COFF/PE archive from the host CC on Windows) does not
link into standalone `--embed-jit` output there. mcc's own `-run` JIT and the
selftests do not need the blob (the engine is compiled into mcc); only standalone
`--embed-jit` output does. The path is a COFF/PE object-archive reader in mcc's
linker that maps COFF sections, symbols, and relocs into mcc's internal ELF
structures, wired into `mcc_load_archive`. The reader is compile-guarded for
WIN32 and validated on a mingw/MSVC host.

- [ ] **COFF-object magic dispatch.** In the `mcc_add_file`/`mcc_load_file`
  sniff (`src/libmcc.c`, alongside the ELF/`!<arch>`/`pe_load_file` checks),
  detect a native COFF object by `IMAGE_FILE_HEADER.Machine` (AMD64/I386/ARM64)
  and route it to the new loader.
- [ ] **COFF object loader** (`coff_load_object_file`, parallel to
  `mcc_load_object_file`): read `IMAGE_FILE_HEADER` + `IMAGE_SECTION_HEADER[]`,
  create internal `Section`s, copy section data, map `IMAGE_SCN_*`
  characteristics to mcc section flags/align.
- [ ] **COFF symbol table → internal `Sym`.** Parse the 18-byte `IMAGE_SYMBOL`
  records + aux records + the COFF string table; map storage classes
  (`IMAGE_SYM_CLASS_EXTERNAL`/`STATIC`/…) and section numbers
  (`IMAGE_SYM_UNDEFINED`/`ABSOLUTE`/`DEBUG`) through `set_elf_sym`.
- [ ] **COFF relocation mapping.** Translate `IMAGE_RELOCATION` +
  `IMAGE_REL_AMD64_*` / `IMAGE_REL_I386_*` / `IMAGE_REL_ARM64_*` into mcc's
  internal `R_*` constants per arch (AMD64 REL32/ADDR64/ADDR32NB, the ARM64
  branch/page/pageoff family).
- [ ] **COFF archive loader.** Parse a COFF `!<arch>` in `mcc_load_archive` —
  the first/second linker members (symbol directory) and the `//` longnames
  member — and dispatch each member to the object loader (whole-archive +
  on-demand pull).
- [ ] **Reuse audit.** Reuse the COFF/PE structs/constants in
  `src/objfmt/mccpe.c` (`IMAGE_FILE_HEADER` et al.); define only the missing
  `IMAGE_REL_*`/`IMAGE_SCN_*`/`IMAGE_SYM_*` constants.
- [ ] **Ungate the build.** Drop the WIN32 guard on `libmcc_jitengine`
  (`CMakeLists.txt`) once the reader works.
- [ ] **Validate on a WIN32 box** (mingw + MSVC): `ctest -R jit/` = 32/32 and a
  standalone `mcc --embed-jit hello.c` running under `MCC_JIT=1` (self-recompile).

## Documentation gaps — 🚧 IN PROGRESS (2026-07-16)

Resolving both: reconciling docs/TODO.md's reference-library tail against `src/`,
and rebuilding the stale `cmake-release/mcc`.

- **docs/TODO.md reconciliation.** The reference-library tail below §26 needs a
  line-by-line pass against `src/`; only the head (System matrix / gating ledger
  / strategic path) and the JIT/§26 sections are current.
- **Stale prebuilt binary.** `cmake-release/mcc` predates `--jit`/`--no-jit` and
  rejects them; rebuild it before using it to check documented JIT flags.

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
