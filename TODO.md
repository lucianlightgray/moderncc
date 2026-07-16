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

## JIT trigger refactor: `--jit` / runtime `MCC_JIT` / CMake `MCC_CONFIG_JIT` (WIP)

Replaces the compile-time `MCC_JIT`/`MCC_AST_JIT` env gate with a runtime model:
`--jit`/`--no-jit` CLI flags + runtime `MCC_JIT` env (0/1) read by the baked
`.init_array` constructor + CMake `MCC_CONFIG_JIT` (default ON) → `MCC_JIT_DEFAULT`.
`--embed-jit` alone now bakes the machinery. Precedence: env > flag (for -run) > CMake default.

### Done + validated
- Flags parse (`--jit`/`--no-jit`), bogus rejected, no collision with `--jit-max-duration`.
- Runtime gate works: `MCC_JIT=0` cleanly runs pure AOT (correct output); default/on activates.
- Fixed the eager-recompile **segfault** (was `unresolved`/NULL `sv->sym` deref at
  `x86_64-gen.c:375`): the intent serializer could not rebuild anonymous symbols
  (string literals) — NAMED handles with no name → `mccjit_build_rec` returns NULL.
  Fix in `mccjit_intent_serialize` (`src/mccjit_intent.c`): refuse to serialize any
  function whose handle table contains a NAMED handle with an anonymous token
  (`!(tv>=TOK_IDENT && tv<SYM_FIRST_ANOM)`) → function stays AOT baseline.
- Differential sweep: 254/254 exec tests, JIT-on == `MCC_JIT=0`, 0 crashes, 0 mismatches.
  `busy()` still recompiles (perf-map); string-using `main` safely falls back.
- CLI tests `embed_jit_manifest`, `clear_cache_and_jit_flags` pass.

### BLOCKER — FIXED (28/28 jit selftests green, cmake-linux-gcc under amd64 docker)
`jit/selftest-{lazy,pool,fork,observability,vrange,fparg,mixed}` regressed to
`mcc: error: unresolved reference to '__mccjit_slot_f'` → "baseline recompile returned NULL".

The original diagnosis (dispatch-stub transform re-applying during the recompile's own
`ast_reemit_extern`) was **wrong**: `ast_reemit_extern` → `ast_reemit` → `ast_replay_body`,
which never reaches the `mccast.c:12742` transform (confirmed by instrumentation — the transform
fires exactly once, during the stash, never during recompile).

Actual root cause: `mccjit_embed_fns` is a **process-global** registry. With the WIP making
`ast_jit_env` true for any `MCC_OUTPUT_MEMORY` compile, every internal `mccjit_stash_one`
(all callers are selftests) now runs the dispatch transform, whose `mccjit_embed_note`
(`mccast.c:12778`) appends the stashed fn to that global list. Nothing consumes/clears it (the
stash never relocates), so entries leak. On the next `mcc_relocate` of an internal state
(`MCC_OUTPUT_MEMORY`), `mccjit_embed_finalize` emits `extern void *__mccjit_slot_<fn>` + a
registry for every stale entry, but the slot is only *defined* by the transform when the fn's
body is compiled in that state → unresolved. This bit both the recompile (stale `f`) and the
end-to-end embed tests (stale `q`/`g` from earlier stashes in the same test).

Fix (`src/mccjit_embed.c` only, 9 lines): a file-static `mccjit_internal_compile` set around
the compile in `mccjit_stash_one` and around the reemit+relocate in `mccjit_recompile_common`.
It early-returns `mccjit_embed_note` (producer) and `mccjit_embed_finalize` (consumer) so
internal JIT-helper compiles never touch the embed registry. Real `mcc -run`/`--embed-jit`
programs (flag stays 0) build the registry exactly as before. Note: gating on `embed_jit`
alone would have broken real `-run` JIT, which is `MCC_OUTPUT_MEMORY` with `embed_jit==0`.

Validated: `ctest -R jit/` = 28/28. Full suite shows only pre-existing env/emulation failures
(macho-*, asan_shadow_native_*, config-defines, run_atexit, errors_and_warnings) that fail
identically on clean HEAD under qemu amd64; no regressions from this change.

### Remaining decision already made by user
Keep `MCC_CONFIG_JIT` default ON; user chose to fix the recompile crash rather than flip OFF.

### DONE — JIT string-using functions via `MCCJIT_ROLE_DATA` (commit d099aa73)
Serialize-bail replaced by a new `MCCJIT_ROLE_DATA`: anonymous rodata symbols (string
literals) are captured as raw bytes and rematerialized as a fresh 16-byte-aligned rodata
symbol in `mccjit_build_rec`. Bumped `MCCJIT_INTENT_FORMAT` 4→5.

The scoping worries turned out not to apply: replay takes the SValue type from the AST node
(`ast_type_t`/`ast_type_ref`) and uses the rebuilt symbol only as the relocation target
(`sv.sym`), and rebuilt `Sym*` are stored directly into the arena — so no type/array-count
reconstruction and no anon-token remapping are needed. A `char*` `get_sym_ref` over the copied
bytes suffices. Safety guards in `mccjit_data_sym_info`: rodata only (immutable → copy-safe,
never mutable `data_section`), valid in-bounds elfsym, size-capped (`MCCJIT_DATA_MAX`), and no
relocations in the byte range (a raw-byte copy would drop pointer relocs → miscompile). Any
unhandled shape still bails to AOT.

Validated: new `jit/selftest-strlit` (indexed load, two strings, mixed-string arithmetic);
`ctest -R jit/` = 29/29; full suite only pre-existing env/qemu-flake failures (all pass in
isolation). Note: a pre-existing, unrelated limitation remains — recompiling a
pointer-*returning* function (`char *h(char*,int){return p+i;}`, no string involved) segfaults
in `ast_reemit`; not exercised by the runtime eligibility gate or the exec suite.

Memory: [[mcc-jit-unification]] updated with all of the above.

## Known bugs (to fix)

### BUG 1 — JIT recompile of a pointer-*returning* function segfaults in `ast_reemit` — FIXED (2026-07-15)
Discovered 2026-07-14 while validating the `MCCJIT_ROLE_DATA` feature above. Independent of
that feature (reproduces with no string literal involved).
**FIXED (2026-07-15):** Root cause = `mccjit_rebuild_sym` (`src/mccjit_intent.c`) hardcoding the
rebuilt signature's return `type.ref = NULL`. The intent carried only the return *base*
`ret_type_t`, no pointee, so a `VT_PTR` return rebuilt as `{VT_PTR, ref=NULL}`; the return cast
`verify_assign_cast`'s `VT_PTR` case then deref'd it via `pointed_type` (`&NULL->type`) →
SIGSEGV in `ast_replay_body`'s `AST_Return`. (Pointer arithmetic in the body already replayed
correctly — it uses the AST node's rebuilt ref, not the signature.) Fix: serialize the return
pointee as a handle-table entry (intern `sig->type.ref` with `mccjit_role_for_base`; new
`MccjitIntent.ret_type_ref`; `MCCJIT_INTENT_FORMAT` 5→6) and rebuild it via `mccjit_build_rec`
in `mccjit_rebuild_sym`. The now-unnecessary `VT_PTR`-return rejection in `ast_jit_eligible`
(`src/mccast.c`) is removed, so pointer-returning functions JIT correctly instead of always
deopting. New `jit/selftest-ptrret`: `char*`/`int*` pointer-arithmetic returns + the
string-literal (`MCCJIT_ROLE_DATA`) + pointer-return combo. Validated: `ctest -R jit/` 36/36;
exec suite 4396/4396 excluding the documented run_atexit/errors_and_warnings env noise. The
`ast_reemit` pointer-return fault BUG 1b/1c speculated might share this family is now resolved
for the return path.
**Prior mitigation (commit 407df3a8):** `ast_jit_eligible` rejected a `VT_PTR` return.
**Enum subclass FIXED (commit 20ce0a42):** BUG 1 was broader than pointer returns — ANY enum
type (param, return, local, or an enum-typed callee) crashed the same way. Root cause: an enum
is `VT_INT` + `VT_ENUM` (struct-mask) + a `type.ref` to the enum sym; `mccjit_role_for_base`
keys off `VT_BTYPE` (=`VT_INT`) so the ref is classified PLAIN and never rebuilt (NULL), while
`mccjit_rebuild_sym` sets `type.ref=NULL` on the enum-flagged type → any `IS_ENUM` path
dereferences NULL and crashes in `rebuild_sym`/`ast_reemit`. Fix: `mccjit_strip_enum()` clears
the enum struct-mask bits at rebuild (return, params, every AST node) so enums recompile as
their integer base. Both the enum family and the pointer-return `ast_reemit` fault (NULL
`type.ref`) are now fixed per the FIXED note above.

### BUG 1b — mode-6 dispatch orphans anon slot symbols — FIXED (switch + residual 8)
Found 2026-07-14 while implementing self-JIT of mcc's own functions (`--embed-jit` over
`src/mcc.c`). The mode-6 dispatch transform rewinds `ind` and raw-splices the saved AOT body
(`ast_baseline_splice`) without redefining a function's anonymous label symbols; modes 1-3
re-emit the body fresh and are unaffected. A switch's data-section jump-table then references
undefined case labels → `unresolved reference to 'L.N'` at link (`L.N` = anon sym named in
`mccpp.c:691`).
**Both subclasses now FIXED.** (1) Switch functions bail from JIT (`ast_fn_switch`, commit
407df3a8) — cut embed-all unresolved-label errors ~40→8. (2) The residual 8 (commit 68e6d384):
the mode-6 dispatch created the anon `slot_sym` (referenced by the baked `jmp *slot(%rip)`)
BEFORE running `set_global_sym`/`mccjit_embed_note`/the `ind` rewind; for 8 functions one of
those left `slot_sym` UND while the `__mccjit_slot_<fn>` alias stayed defined at the same offset.
Fix = mint `slot_sym` immediately before the `greloca` that uses it. **Confirmed our bug, not
qemu:** emitting `-r` and `readelf -s` showed 8 `GLOBAL UND` L.N symbols in mcc's own object —
deterministic output, qemu-independent. embed-all of mcc.c now LINKS with 0 unresolved labels.

### BUG 1c — embed-all self-host startup crash — FIXED; JIT-on leaves a benign error leak
Exposed once BUG 1b let embed-all link. **FIXED (commit cb16ed00):** the `MCC_JIT=0` startup
SIGSEGV was `body_sym` (the AOT-baseline entry the dispatch slot points at) having the SAME
early-creation bug as `slot_sym` — undefined by the intervening `set_global_sym`/
`mccjit_embed_note`; unlike `slot_sym` its `R_X86_64_64` .data slot-init reloc resolves SILENTLY
to 0, giving a NULL slot → `jmp *NULL` at the first call (found via the whole-tree `MCC_TRACE`
build: last successful entry then RIP=0). Fix = mint `body_sym` right before its use too.
Result: `mcc --embed-jit src/mcc.c` now RUNS as a pure-AOT compiler (`MCC_JIT=0`, rc correct).
With `MCC_JIT=1` it also no longer crashes (after the enum fix 20ce0a42) and produces correct
output, self-recompiling ~19 functions (731 KGC hits).

**JIT-on speculative-recompile error leak — FIXED (2026-07-15).** Recompiling an mcc function
that called a *static* (local-linkage, non-`dlsym`-able) callee — e.g. `read32le`,
`host_clock_ms`, `so_jit_env` — failed at `mcc_relocate(js)` with "unresolved reference"; the
recompile bailed to AOT (output stayed correct) but the diagnostics printed and leaked into the
exit code (`rc=1`). Fixed as specified — bail at SERIALIZE, not at runtime: the serialize handle
loop in `mccjit_intent_serialize` (`src/mccjit_intent.c`) now returns `-1` (→ silent AOT
fallback, no blob stashed, no recompile) when a NAMED handle references a `VT_FUNC` sym with
`VT_STATIC|VT_INLINE` linkage (the exact STB_LOCAL condition from `put_extern_sym2`), excluding
the function's own sym (`s != sym`) so a recursive static function still recompiles. No runtime
error-suppression. Validated: `ctest -R jit/` 36/36 (incl. `selftest-stage2`, whose `abs` callee
is extern/global → not bailed); no regressions.

### BUG 2 — `MCC_EMBED_JIT=1` build segfaults compiling `vla/basic.c` at `-O1` (arm64/macOS) — RESOLVED (no longer reproduces at HEAD)
Was: an `MCC_EMBED_JIT=1` build of `mcc` on arm64/macOS **deterministically SIGSEGV'd (rc=139)**
compiling `tests/exec/vla/basic.c` at `-O1` — a plain compile, no `--embed-jit` flag. Surfaced
as **14 `exec-*/basic` ctest failures** (replay, replay-tmpl, replay-promote, narrowfix, vlat,
zerobss, interchange, fusion, tile, mergestrings, search, search-emitsize, search-emitiso,
search-threads) because every `*/basic` runner batches a 295-case set including that VLA source.
**RESOLVED (verified 2026-07-15):** on a clean-rebuilt (`--clean-first`) `cmake-build-embedjit`
(`MCC_EMBED_JIT=ON`, clang) at HEAD, `mcc -O1 -c tests/exec/vla/basic.c` succeeds 5/5 (rc=0),
compile+run correct at `-O0..-O3`, and all 14 `exec-*/basic` variants PASS (`ctest -R basic$`
34/34). Not bisected to a single commit; the crash disappeared across the JIT embed unification /
SOC-split refactor window (e0fca0a5 + the `mccjit_embed.c` split). Memory
[[embedjit-arm64-vla-o1-crash]] updated: the 14 `*/basic` failures are no longer expected noise on
the arm64 embed host — if they reappear that's a real regression.

### BUG 3 (hardening/audit) — brace-initialized automatic `Operand` unions leave `e.v` garbage — AUDITED CLEAN (2026-07-15)
Documented in memory [[union-init-partial-zero]]. The per-arch assembler `Operand` structs
(e.g. `src/arch/riscv64/riscv64-asm.c`) wrap a union whose first member is a small `uint8_t
reg` with `ExprValue e` behind it. `Operand x = { OP_..., { 0 } }` only zeroes the first union
member; homebrew gcc-16 leaves `x.e.v` upper bytes as stack garbage (clang zeroes the whole
union) — a host-compiler-dependent codegen bug (caused the 2026-07-03 dash-s-bytes-riscv64 CI
failure via `mv` emitting a garbage `addi` immediate). The specific `mv` instance was fixed.
**AUDITED 2026-07-15 — zero remaining vulnerable sites.** Only `riscv64-asm.c` and `arm-asm.c`
have the vulnerable union at all (i386/x86_64 and arm64 lay `e` as a plain trailing member, so
a partial brace-init well-definedly zeroes it — the class can't occur). The only two automatic
`Operand` brace-inits in the tree are both already safe: `riscv64-asm.c:448` (`Operand imm`)
assigns `imm.e.v` explicitly on every path that reads it, and the zero-immediate pseudo-ops
(`mv`/`sext_w`/`nop`/`ret`) already pass the file-static fully-zeroed `&zero`/`&zimm` consts;
`arm-asm.c:543` (`Operand shift`) only reads `.e` under `if (nb_shift)`, which is set only when
`asm_parse_optional_shift` has fully populated it. Nothing relies on the `{0}` brace-init for a
read. No code change needed. (If a future site appears, mirror the `mv` fix: pass `&zimm`/`&zero`,
or assign `e.v` explicitly.)

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

### Windows JIT-embed port — DONE (runtime JIT works on the PE target; 32/32 selftests green)
`MCC_EMBED_JIT` default flipped OFF→ON in `e0fca0a5`, so `src/mccjit_embed.c` must build on
every target — but it was POSIX/glibc-only (`<pthread.h> <sys/mman.h> <sys/wait.h> <unistd.h>`,
`fork`/`waitpid`/`mmap`/`munmap`/`pthread_*`/`pthread_atfork`), broken under both WIN32
toolchains, so it was gated OFF on WIN32. **Now fully ported and ungated** — `MCC_EMBED_JIT`
builds and runs on WIN32, `ctest -R jit/` = **32/32** on the mingw PE host. Three layers landed:

**(1) OS-primitive port** (`src/mccjit_win32.h`, included by
`mccjit_embed.c` under `#ifdef _WIN32`, POSIX path byte-unchanged):
- **exec/RW memory:** a `mmap`/`munmap`/`msync` shim → `VirtualAlloc(PAGE_EXECUTE_READWRITE)` for
  anon exec/RW pages; `MAP_FAILED`≡NULL so the `== MAP_FAILED` checks read as NULL checks.
- **KGC shared-file map:** the `MAP_SHARED` fd path → `CreateFileMapping`/`MapViewOfFile` with a
  base→handle registry so one `munmap` dispatches file-view vs VirtualAlloc; plus `open`/`close`/
  `ftruncate`/`pread`/`fstat`/`mkstemp` adapters.
- **threads/locks:** the `pthread_*` subset → SRWLOCK (mutex, static `SRWLOCK_INIT`) +
  CONDITION_VARIABLE + `InitOnceExecuteOnce` + `_beginthreadex` (detached/joinable), no
  winpthreads dependency; one path serves mingw and MSVC.
- **fork:** `mccjit_selftest_fork` + `pthread_atfork` `#ifdef`'d out on WIN32 (the pool's
  post-fork reset invariant has no meaning without `fork`); the selftest reports a Win32 skip.
- **misc:** `clock_gettime`→QPC, `nanosleep`→spin-yield-then-`Sleep` (so a 1 ms nap isn't rounded
  up to the ~15 ms scheduler tick), `setenv`/`unsetenv`→`_putenv_s`, perf-map path→`GetTempPath`,
  MSVC `__atomic_*`→Interlocked-on-8-byte, `_M_X64`/`_M_ARM64`→internal `MCCJIT_X64`/`MCCJIT_ARM64`
  so an MSVC-built mcc still gets real JIT. Selftest entry points marked `PUB_FUNC` so the DLL
  exports them.
- Also fixed `host_icache_flush`/`host_runmem_protect` to `FlushInstructionCache` on
  arm/arm64-Windows (Windows doesn't flush on `VirtualProtect`).

**(2) Win64-ABI hand-written stubs.** The stubs were SysV-AMD64-ABI-hardcoded (push rdi/rsi/rdx/
rcx/r8/r9, call the C helper arg0-in-rdi with no shadow space; the C-side capture read that
layout), so they segfaulted/hung under the Windows x64 ABI. Ported to the Microsoft x64 convention
(`#ifdef _WIN32` branches, SysV path byte-unchanged): `mccjit_make_counter_stub` now spills
rcx/rdx/r8/r9 into the 6-slot regs array the capture expects (stack args zeroed) + 32-byte shadow
space; `mccjit_make_kgc_stub_n` spills rcx/rdx/r8/r9 (+caller stack for args 5-6, `movsxd` for
narrow) and passes `mccjit_kgc_calln`'s 5th/6th args (nargs,flagged) on the stack;
`mccjit_make_kgc_stub_fp` does the same with xmm0-3 `movsd` spills → `calln_fp`. The profile
selftest's fixtures were widened `long`→`long long` (LLP64 `long` is 32-bit, so a `long` arg left
the captured register's upper bits caller-defined). **`mccjit_make_kgc_stub_mixed` is the one
deferral:** its forwarding thunk (`mccjit_mixed_thunk_code`) rebuilds a SysV call *by class*
(gpv→rdi.., fpv→xmm..), but Windows is *positional* (arg N in rcx/rdx/r8/r9 OR xmm-N by position),
which needs a per-arg class vector the stub doesn't carry — so on WIN32 it returns NULL (mixed
sigs fall back to the AOT baseline, correct but unmemoized) and `jit/selftest-mixed` skips.

**(3) The `-run`/`--embed-jit` auto-JIT pipeline on PE.** Two PE-only gaps kept the baked JIT ctor
from ever firing: (a) `mcc -run` didn't run `.init_array` constructors on Windows —
`runtime/lib/runmain.c`'s `run_ctors`/`_runmain` were `#ifndef _WIN32` (the PE linker *does*
synthesize `__init_array_start/end`, per `runtime/win32/lib/crtinit.c`), now ungated; and (b)
`mccjit_embed_finalize` (emits the registry + `__attribute__((constructor)) __mccjit_boot_all`)
was only called from the ELF/Mach-O `mcc_add_runtime`, never from PE's `pe_add_runtime`
(`src/objfmt/mccpe.c`) — added there before the init-array bounds are taken. Plus the boot ctor's
`getenv("MCC_JIT")` is now bound as a host symbol for PE in-memory relocates (`getenv` was
otherwise unresolved for a bare `mcc_relocate` that isn't a full `mcc_run`).

**WIP (parked 2026-07-16) — the embed-blob (`--embed-jit` standalone exe) on Windows.**
Sole remaining open slice. **Direction chosen (user, 2026-07-16): path (a) — teach mcc's linker
to read COFF/PE object archives** (a real COFF object reader mapping COFF sections/symbols/relocs
into mcc's internal ELF structures, wired into `mcc_load_archive`). Parked before implementation
at user request; no code written yet. Validation is WIN32-only and this host is macOS/arm64, so
the reader must be compile-guarded and final validation deferred to a mingw/MSVC box.

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

Prior status: `bin2c` of
`libmcc_jitengine.a` → `MCC_EMBED_JIT_BLOB` writes the archive to a temp file and links it via
mcc's OWN linker (`AFF_WHOLE_ARCHIVE`, `libmcc.c:mcc_add_jit_engine_embedded`), which is
**ELF-only**. On Windows the host CC produces a COFF/PE archive mcc's ELF linker can't consume, so
the blob is left unbuilt on WIN32 (`CMakeLists.txt` ~1951 now guards `libmcc_jitengine` off on
WIN32). mcc's own `-run` JIT and the selftests don't need the blob (the engine is compiled into
mcc); only standalone `--embed-jit` output would. Needs either mcc's linker to read PE/COFF
archives, or a self-hosted ELF build of the engine.
