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
**MITIGATED (commit 407df3a8):** `ast_jit_eligible` now rejects a `VT_PTR` return, so
pointer-returning functions bail to plain AOT and are never recompiled through the normal
path — the crash is no longer reachable via eligibility. The underlying `ast_reemit` fault
is still latent for a forced `mcc_jit_recompile_blob` on such a function; root-cause and fix
below still stand as the real bug.

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

### BUG 1b — mode-6 dispatch raw-splice orphans anon labels (switch case FIXED; ~8 residual)
Found 2026-07-14 while implementing self-JIT of mcc's own functions (`--embed-jit` over
`src/mcc.c`). The mode-6 dispatch transform rewinds `ind` and raw-splices the saved AOT body
(`ast_baseline_splice`) without redefining a function's anonymous label symbols; modes 1-3
re-emit the body fresh and are unaffected. A switch's data-section jump-table then references
undefined case labels → `unresolved reference to 'L.N'` at link (`L.N` = anon sym named in
`mccpp.c:691`).
**Switch subclass FIXED (commit 407df3a8):** switch functions bail from JIT (`ast_fn_switch`).
This cut embed-all-of-mcc.c unresolved-label errors from ~40 to **8 residual**.
The **residual 8 are a DISTINCT mechanism, still open**: verified that all 459 mode-6 functions
in the embed-all build have NO orphaned labels (a cross-section-reloc-to-anon-text-label scan
returns 0 for every one), and disabling the inline/reemit passes (`MCC_AST_INLINE_PASS=0`,
`MCC_AST_REEMIT=0`, `MCC_AST_INLINE=0`) does not change the count — so it is neither the mode-6
splice nor the forward-inline re-emission. Next: find which section references each residual
`L.N` and which pass drops its definition (fix `relocate_syms` in `src/objfmt/mccelf.c` to scan
`sh_type==SHT_RELX && link==symtab_section` sections — note `symtab` local there differs from the
`symtab_section` macro — and print the referencing section + defining function). Repro:
`mcc -B<build> --embed-jit <mcc defines/includes> -O1 src/mcc.c <blobs> -o mcc2` in amd64 docker.
Impact: only full embed-all; targeted `--jit-functions <non-switch fn>` and `-run` work.

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
