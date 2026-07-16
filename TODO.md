# TODO

## Working conventions (status markers)

Always mark TODO items as in progress, and commit and push to main, before doing any work.

Each slice/phase of work carries a status marker so the board stays legible when
picked up cold. Mark a slice with exactly one of:

- **🚧 IN PROGRESS** — actively being worked *right now*. Add this marker (with a
  one-line note on what's underway + who/when) the moment you start a slice, and
  remove it when you land it (→ DONE/FIXED) or park it (→ WIP/BLOCKER). At most a
  few of these should be live at once. Never leave a slice IN PROGRESS across a
  hand-off without a note on where it stands.
- **WIP** — started but parked between sessions; not actively being worked.
- **BLOCKER** — cannot proceed until the noted dependency/decision is resolved.
- **Still open** — scoped but not started.
- **DONE / FIXED / RESOLVED / AUDITED** — landed and validated; keep the
  validation note (what was run, what passed) with it.

Completed items are pruned once verified; the detail lives in git history.

## Documentation gaps (open)

- **docs/TODO.md full reconciliation.** Only the head (System matrix / gating ledger / strategic path) and the
  JIT/§26 sections were reconciled this session; the 1554-line reference-library tail below §26 was only
  spot-checked and may carry more pre-unification staleness. A full line-by-line pass against `src/` is
  deferred (low urgency — it's an internal strategic doc, not user-facing).
- **Stale prebuilt binary (not a doc bug).** `cmake-release/mcc` (Jul 14) predates `--jit`/`--no-jit` and
  rejects them; rebuild before using it to sanity-check documented JIT flags.

## Known bugs (to fix)

*No open bugs.*

### Not bugs — local qemu-amd64 emulation noise (do NOT chase as compiler defects)
When running the full ctest suite in an amd64 Ubuntu container under qemu on Apple Silicon, a
recurring set fails that also fails identically on clean HEAD and/or passes when run serially in
isolation — these are environment/emulation artifacts, not compiler bugs:
- `macho-*` (Mach-O = macOS output format; cannot run on Linux),
- `asan_shadow_native_*`, `cli/bcheck_exe_static_bounds` (ASan/bounds runtime under emulation),
- `config-defines` (host triplet/OS-release specific), `run_atexit`,
  `exec-search*/errors_and_warnings` (diagnostic-text/atexit env deltas),
- `git-stamp` (fails only when the working tree is dirty),
- sporadic `function_pointer`/`func_pointers`/`func_arg_struct_compare`/`complex` (qemu
  parallelism flakes — pass in isolation; also seen as spurious gcc SIGSEGVs mid-build).
Real regressions show up as a *different* failing test; validate JIT/codegen work with
`ctest -R jit/` + `ctest -R ast/` and per-test serial reruns, not the full parallel sweep.

## Windows JIT-embed port — embed-blob standalone exe (WIP, parked 2026-07-16)

The runtime JIT itself is DONE on WIN32 (Win64-ABI stubs + OS-primitive shim `src/mccjit_win32.h` +
`-run`/`--embed-jit` PE pipeline; `ctest -R jit/` = 32/32 on the mingw PE host; only mixed-sig stubs defer
to the AOT baseline). Sole remaining open slice: the standalone `--embed-jit` blob.

**Direction chosen (user, 2026-07-16): path (a) — teach mcc's linker to read COFF/PE object archives** (a
real COFF object reader mapping COFF sections/symbols/relocs into mcc's internal ELF structures, wired into
`mcc_load_archive`). Parked before implementation at user request; no code written yet. Validation is
WIN32-only and this host is macOS/arm64, so the reader must be compile-guarded and final validation deferred
to a mingw/MSVC box.

Open COFF items (path a work breakdown):
- [ ] **COFF-object magic dispatch.** In the `mcc_add_file`/`mcc_load_file` sniff
  (`src/libmcc.c` ~1150-1240, alongside the ELF/`!<arch>`/`pe_load_file` checks) detect a native
  COFF object by `IMAGE_FILE_HEADER.Machine` (AMD64/I386/ARM64) and route it to a new loader.
- [ ] **COFF object loader** (new `coff_load_object_file`, parallel to `mcc_load_object_file`):
  read `IMAGE_FILE_HEADER` + `IMAGE_SECTION_HEADER[]`, create internal `Section`s, copy section
  data, honor `IMAGE_SCN_*` characteristics → mcc section flags/align.
- [ ] **COFF symbol table → internal `Sym`.** Parse the 18-byte `IMAGE_SYMBOL` records + aux
  records + the COFF string table; map storage classes (`IMAGE_SYM_CLASS_EXTERNAL`/`STATIC`/…),
  section numbers (incl. `IMAGE_SYM_UNDEFINED`/`ABSOLUTE`/`DEBUG`), and value→ `set_elf_sym`.
- [ ] **COFF relocation mapping.** Translate `IMAGE_RELOCATION` entries + `IMAGE_REL_AMD64_*` /
  `IMAGE_REL_I386_*` / `IMAGE_REL_ARM64_*` types into mcc's internal `R_*` reloc constants per
  arch (esp. AMD64 REL32/ADDR64/ADDR32NB and the ARM64 branch/page/pageoff family).
- [ ] **COFF archive loader.** Teach `mcc_load_archive` (`src/libmcc.c` ~1166) to parse a COFF
  `!<arch>` — the first/second linker members (symbol directory) and the longnames (`//`) member
  — and dispatch each COFF member to the new object loader (whole-archive + on-demand pull).
- [ ] **Reuse audit.** Reuse the COFF/PE structs/constants already in `src/objfmt/mccpe.c`
  (`IMAGE_FILE_HEADER` et al.); define only the missing `IMAGE_REL_*`/`IMAGE_SCN_*`/
  `IMAGE_SYM_*` constants. (Two Explore agents were mapping the ELF target side and the existing
  mccpe COFF defs when this was parked — re-run that mapping before coding.)
- [ ] **Ungate the build.** Once the reader works, drop the WIN32 guard on `libmcc_jitengine`
  (`CMakeLists.txt` ~1956) so the embed-blob builds on Windows.
- [ ] **Validate on a WIN32 box** (mingw + MSVC): build, `ctest -R jit/` = 32/32, and a
  standalone `mcc --embed-jit hello.c` → run the emitted exe with `MCC_JIT=1` (self-recompile).

Prior status: `bin2c` of `libmcc_jitengine.a` → `MCC_EMBED_JIT_BLOB` writes the archive to a temp file and
links it via mcc's OWN linker (`AFF_WHOLE_ARCHIVE`, `libmcc.c:mcc_add_jit_engine_embedded`), which is
**ELF-only**. On Windows the host CC produces a COFF/PE archive mcc's ELF linker can't consume, so the blob
is left unbuilt on WIN32 (`CMakeLists.txt` ~1951 now guards `libmcc_jitengine` off on WIN32). mcc's own
`-run` JIT and the selftests don't need the blob (the engine is compiled into mcc); only standalone
`--embed-jit` output would. Needs either mcc's linker to read PE/COFF archives, or a self-hosted ELF build
of the engine.
