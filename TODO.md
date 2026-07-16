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
fall to the AOT baseline and `jit/selftest-mixed` skips.

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
