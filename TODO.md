# TODO

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

### BUG 1 — JIT recompile of a pointer-*returning* function segfaults in `ast_reemit`
Discovered 2026-07-14 while validating the `MCCJIT_ROLE_DATA` feature above. Independent of
that feature (reproduces with no string literal involved).
**Pointer-return MITIGATED (commit 407df3a8):** `ast_jit_eligible` rejects a `VT_PTR` return.
**Enum subclass FIXED (commit 20ce0a42):** BUG 1 was broader than pointer returns — ANY enum
type (param, return, local, or an enum-typed callee) crashed the same way. Root cause: an enum
is `VT_INT` + `VT_ENUM` (struct-mask) + a `type.ref` to the enum sym; `mccjit_role_for_base`
keys off `VT_BTYPE` (=`VT_INT`) so the ref is classified PLAIN and never rebuilt (NULL), while
`mccjit_rebuild_sym` sets `type.ref=NULL` on the enum-flagged type → any `IS_ENUM` path
dereferences NULL and crashes in `rebuild_sym`/`ast_reemit`. Fix: `mccjit_strip_enum()` clears
the enum struct-mask bits at rebuild (return, params, every AST node) so enums recompile as
their integer base. The pointer-return `ast_reemit` fault is likely the same family (NULL
`type.ref`) and remains latent behind the eligibility gate.

Repro (in-process, e.g. from a selftest calling the internal API in `src/mccjit_embed.c`):
```
char *h(char *p, int i){ return p + i; }        // pointer arithmetic + pointer return
// stash: mccjit_stash_one(src, "h", 1, &len, &state)  -> blob is produced OK
// recompile: mcc_jit_recompile_blob(blob, len)        -> SIGSEGV
```
Also crashes with a string base (`char *h(int i){ return "world" + i; }`).

What is known:
- The function *stashes* fine (`mccjit_intent_serialize` succeeds, blob non-NULL) and
  `mccjit_rebuild_sym` returns a non-NULL sym.
- The crash is inside `ast_reemit_extern` → `ast_reemit` → `ast_replay_body`
  (`src/mccast.c`): instrumentation printed "after rebuild_sym" but never "after reemit".
  So it faults while replaying the body of a function whose return type is a pointer and
  whose body is pointer arithmetic.
- NOT hit by the exec suite: `exec-*/function_pointer`, `func_pointers`,
  `func_arg_struct_compare` all pass under JIT. Likely the runtime promotion/eligibility gate
  does not recompile pointer-returning functions, OR they never get hot; the selftests hit it
  only because `mcc_jit_recompile_blob` bypasses that gate.
- Latent-severity note: the `MCCJIT_ROLE_DATA` feature widened what serializes, so a
  string-using function that *also* returns a pointer now serializes (instead of bailing) and
  would hit this crash on recompile rather than falling back to AOT. Not observed in the exec
  suite, but a reason to fix this rather than leave it latent.

Next steps to fix:
- Get the exact fault: build the `linux-gcc-sanitize` preset (ASan) under amd64 docker and run
  a minimal harness that recompiles `char *h(char*,int){return p+i;}`; gdb does NOT work under
  qemu emulation (exits 127), so ASan is the way to get a stack. Or add fine-grained markers
  through `ast_replay_body`'s `AST_Ref`/`AST_Literal`/binary-op cases (`src/mccast.c` ~4170).
- Suspect the replayed node's type/`type.ref` for the returned pointer, or the sret/return
  handling in the reemit epilogue path, not the DATA symbol (the DATA sym is only a reloc
  target). Compare the AST-node stream for a pointer-return vs an int-return leaf.

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

**Remaining (OPEN, cosmetic): JIT-on speculative-recompile error leak.** Recompiling an mcc
function that calls a *static* (local-linkage, non-`dlsym`-able) callee — e.g. `read32le`,
`host_clock_ms`, `so_jit_env` — fails at `mcc_relocate(js)` with "unresolved reference"; the
recompile correctly bails to AOT (output stays correct), but the diagnostics print and leak
into the exit code (`rc=1`). The right fix is to bail these at SERIALIZE (refuse to serialize a
function whose intent references a static/local-linkage function, since it can never be
`dlsym`-resolved at recompile) — NOT to suppress errors at runtime (tried gating `error1` on
`mccjit_is_internal_compile()`; the early return skips error1's cleanup and made it worse —
reverted). Detect via the callee sym's linkage in the serialize handle loop
(`src/mccjit_intent.c`). Impact: only full embed-all self-JIT of mcc; targeted
`--jit-functions <leaf fn>` and `-run` are clean.

### BUG 2 — `MCC_EMBED_JIT=1` build segfaults compiling `vla/basic.c` at `-O1` (arm64/macOS)
Pre-existing, config-specific; documented in memory [[embedjit-arm64-vla-o1-crash]].
An `MCC_EMBED_JIT=1` build of `mcc` on arm64/macOS **deterministically SIGSEGVs (rc=139)**
compiling `tests/exec/vla/basic.c` at `-O1` — a plain compile, no `--embed-jit` flag. Surfaces
as **14 `exec-*/basic` ctest failures** (replay, replay-tmpl, replay-promote, narrowfix, vlat,
zerobss, interchange, fusion, tile, mergestrings, search, search-emitsize, search-emitiso,
search-threads) because every `*/basic` runner batches a 295-case set including that VLA source.
- Reproduced 3/3 on a pristine pre-change embed `mcc` (not from any recent JIT work).
- Config-specific: the embed-**off** build compiles the same file fine (rc=0). So it is the
  `MCC_EMBED_JIT=1` build config interacting with the VLA path at `-O1`, not the front-end in
  general, and not the emitiso `-dt` scratch-section UAF (that was fixed in 7ee2207e; crash
  persists after).
Next steps: bisect what the embed build config changes (blob embedding / static-engine link /
extra defines) that perturbs `-O1` VLA codegen; reproduce under ASan on an embed build.

### BUG 3 (hardening/audit) — brace-initialized automatic `Operand` unions leave `e.v` garbage
Documented in memory [[union-init-partial-zero]]. The per-arch assembler `Operand` structs
(e.g. `src/arch/riscv64/riscv64-asm.c`) wrap a union whose first member is a small `uint8_t
reg` with `ExprValue e` behind it. `Operand x = { OP_..., { 0 } }` only zeroes the first union
member; homebrew gcc-16 leaves `x.e.v` upper bytes as stack garbage (clang zeroes the whole
union) — a host-compiler-dependent codegen bug (caused the 2026-07-03 dash-s-bytes-riscv64 CI
failure via `mv` emitting a garbage `addi` immediate). The specific `mv` instance was fixed;
this is an **audit item** for other pseudo-instruction cases.
Next steps: grep the arch `*-asm.c` files for automatic `Operand`/union brace-inits whose `e`/
`imm`/`e.v` is read without an explicit prior assignment; assign `e.v` explicitly or use the
file-static fully-zeroed `zimm`/`zero` consts.

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

### DONE (gate) / TODO (port) — Windows support for the new JIT embed
`MCC_EMBED_JIT` default flipped OFF→ON in `e0fca0a5`, so `src/mccjit_embed.c` now compiles on
every target — but it is POSIX/glibc-only: `#include <pthread.h> <sys/mman.h> <sys/wait.h>
<unistd.h>` and uses `fork`/`waitpid`/`mmap`/`munmap`/`pthread_*`/`pthread_atfork`. None of
those exist under MSVC (`error C1083: 'pthread.h'`), and mingw lacks `sys/mman.h`/`fork`/
`sys/wait.h` too, so both WIN32 toolchains break. Stopgap to unbreak CI: gate `MCC_EMBED_JIT`
OFF for `MCC_TARGETOS STREQUAL "WIN32"` in `CMakeLists.txt` (~line 1662 / 1805) so the embed
stays glibc-only. **APPLIED** (CMakeLists ~line 1662, mirroring the `MCC_EMBED_MCCRT` WIN32
gate); the canonical Windows debug + msvc builds are green again.

To actually support Windows, port the embed's OS primitives:
- executable memory: `VirtualAlloc(MEM_COMMIT, PAGE_EXECUTE_READWRITE)` / `VirtualFree` in
  place of `mmap`/`munmap` (all ~15 `PROT_READ|WRITE|EXEC` sites).
- threading/locks: Win32 `CRITICAL_SECTION` + `CONDITION_VARIABLE` + `InitOnceExecuteOnce`
  (or `<threads.h>` on ucrt) for the `pthread_mutex`/`pthread_cond`/`pthread_once` pool.
- the `fork`/`waitpid` selftest path (`mccjit_selftest_fork`) and `pthread_atfork` handlers
  have no Win32 equivalent — either `#ifdef` them out on WIN32 or reimplement over
  `CreateProcess`.
- the shared-file KGC map (`open`/`ftruncate`/`MAP_SHARED`) needs `CreateFileMapping`/
  `MapViewOfFile`.
- the embed-blob mechanism (`bin2c` of `libmcc_jitengine.a` → `MCC_EMBED_JIT_BLOB`) assumes an
  ELF static lib; the PE equivalent is unbuilt.
The WIN32 CMake gate is in place; the items above remain for a real Windows JIT port.
