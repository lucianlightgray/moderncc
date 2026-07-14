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

### Future work — JIT string-using functions (separate feature, NOT part of this WIP)
Scoped for a later session with its own fuzz/differential soak — this is a wire-format bump
with silent-miscompile risk, deliberately not shipped alongside the blocker fix above. The
current behaviour (serialize-bail → AOT fallback for string-using functions) is correct and
intentional; this only widens JIT coverage.

Sketch: extend the intent format (bump `MCCJIT_INTENT_FORMAT` 4→5) with a new
`MCCJIT_ROLE_DATA` for anonymous rodata/data symbols (string literals), and re-materialize
them as fresh rodata syms in `mccjit_build_rec`. Non-trivial sub-problems found while scoping:
- Anonymous string syms carry an unstable `anon_sym` token (`sym->v >= SYM_FIRST_ANOM`), so the
  AST-replay token→sym resolution needs a handle-indexed path, not name/token lookup.
- The array type must be rebuilt from `type.ref->c` (element count) — the current PLAIN type
  record does not capture an array-count sym.
- Serialize the section kind + byte length + bytes (`sec->data + off`, size from `elfsym`),
  rebuild by `section_ptr_add` into the recompile state's rodata + `get_sym_ref`.
- Keep the bail for every shape not positively handled (wide strings, struct/array-in-rodata,
  bss) so any uncertainty still falls back to AOT.
Acceptance gate: `ctest -R jit/` = 28/28 + full exec differential (JIT-on == `MCC_JIT=0`,
0 mismatches) + a fuzz soak, per the promotion-default-on precedent.

Memory: [[mcc-jit-unification]] updated with all of the above.
