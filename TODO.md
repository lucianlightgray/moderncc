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

### BLOCKER — 7 JIT selftests regressed (must fix before commit is "green")
`jit/selftest-{lazy,pool,fork,observability,vrange,fparg,mixed}` fail:
`mcc: error: unresolved reference to '__mccjit_slot_f'` → "baseline recompile returned NULL".
All 7 PASS on clean HEAD (confirmed via stash) — caused by this change.

Root cause: `src/mccast.c:1245` now sets `ast_jit_env = s1 && (embed_jit || output_type==MCC_OUTPUT_MEMORY)`.
But `mccjit_recompile_common` (`src/mccjit_embed.c:136`) creates the recompile state with
`output_type = MCC_OUTPUT_MEMORY`, so during the **recompile's own** `ast_reemit_extern`,
`ast_jit_env` is true → the mode-6 dispatch-stub transform (`mccast.c:12742`) re-applies to
the function being recompiled, emitting a `jmp *__mccjit_slot_f` with no slot defined in the
fresh state → unresolved. Previously `ast_jit_env` came from the `MCC_JIT` env (unset during
selftests) so the re-emit stayed plain.

Fix next session: suppress the JIT transform while inside a recompile. Options:
1. Add a `mccjit_recompiling` flag (set around `ast_reemit_extern` in `mccjit_recompile_common`),
   and `&&` it into the `ast_jit_env` computation (or the 12742 gate).
2. Gate `ast_jit_env` also on `s1->optimize >= 1` — the recompile state sets `optimize=0`
   (`mccjit_embed.c:134`). Cheaper but coincidental; option 1 is cleaner/explicit.
After fixing, re-run: `ctest -R jit/` in cmake-linux-gcc (expect 28/28) + the exec differential.

### Remaining decision already made by user
Keep `MCC_CONFIG_JIT` default ON; user chose to fix the recompile crash rather than flip OFF.

### Follow-up (coverage, not blocking)
The serialize-bail keeps string-using functions on AOT. To actually JIT them, extend the
intent format to serialize anonymous data-symbol bytes (string literals) and re-materialize
them as fresh rodata syms in `mccjit_build_rec` (new `MCCJIT_ROLE_DATA`, bump `MCCJIT_INTENT_FORMAT`).

Memory: [[mcc-jit-unification]] updated with all of the above.
