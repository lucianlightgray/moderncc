# §26 JIT runtime — finish plan

Singularly-focused, dependency-ordered plan to complete the entire runtime JIT + guarded deopt.
Settled decisions: D1=B embedded · D2=A recompile=re-invoke engine · D3=A entry dispatcher ·
D4=A runtime-observed live-in range · **D5=both (.init_array ctor + jit-profile hot counter)** ·
**D6=deopt-first, eval_slice as later hardening** · **D7=ELF x86_64 first** · **D8=pthread pool via
`runtime/include/threads.h`**.

Global gate: `MCC_AST_JIT` (default off) until the full validation bar passes, then P0-style flip.
Every milestone must clear the repo bar before the next: default byte-identical (ctest object-diff 0),
self-host 3-stage fixpoint byte-identical, -O6 differential == gcc == clang, cross-arch i386/arm64,
differential fuzz 0 miscompiles, shadow zero-divergence, UBSan trap clean.

## Key grounding (verified)

- `ast_strategies[]` has 4 free slots (20/24, `mccast.c:817,8872`) — exactly the 4 jit rows.
- Entry-prepend prior art: `ast_tco_run` tail rebuilds the root child list (`mccast.c:6068-6079`).
  Guard = `AST_If` (op-encoded); label/goto = `AST_Jump` op 4/5. No new node kind needed.
- Baseline fallback already 90% present: `orig`/`orig_rel` snapshot at `mccast.c:10246-10252`,
  freed at `10512-10513`; `ast_cur` already survives via `keep_inline`/`keep_reemit`
  (`10515-10519`). Add a `keep_baseline` sibling pool keyed by `sym`.
- D3=A dispatcher sidesteps the static-`E8 rel32` problem (`x86_64-gen.c:588`): call sites unchanged,
  dispatcher reads a swappable data pointer. One aligned 8-byte atomic store; no GOT/i-cache trick.
- Recompile into a fresh `run_ptr` (`mcc_relocate` rejects double-relocate, `mccrun.c:83`); W^X dual
  map + `host_runmem_protect` + `host_icache_flush` all exist (`mcchost.c:1085,1141,1130`).
- `.init_array` emission wired (`add_array`, `mccelf.c:1375`); C11 threads shim real
  (`runtime/include/threads.h:59` `thrd_create`). The design's "threads don't exist" note is stale.
- eval_slice must be independent: `ast_fold_eval`/`gen_opic` both mask `shift & 63/31`
  (`mccast.c:4102`, `mccgen.c:2347`). Domain from `AstVLat`+`ast_vlat_context` (`7189`), live-ins from
  `ast_tco_run` param walk (`5944`), harness modeled on `ast_vlat_check_sound` recompute-then-abort.

## Milestones

### M1 — Baseline retention (W1)
Stop freeing `orig`/`orig_rel`; stash `{orig, orig_rel, ast_body_ind_sv, ast_reloc0_sv, body_len,
rel_len, orig_ind, orig_rsym}` + retained `ast_cur` in a new `ast_baseline_pool[]` keyed by `sym`,
via a `keep_baseline` flag beside `keep_inline`/`keep_reemit` (`mccast.c:10515`). Gated `MCC_AST_JIT`.
Validate: pool only allocates when the gate is on → default path byte-identical; no leak regression
(reuse the shadow build). No behavior change yet.

### M2 — Stage 1 strategies `jit-guard` + `jit-dispatch` (W2, ships a complete guarded-deopt)
Add 2 rows to `ast_strategies[]` + `AST_STRAT_*` enum; 2 gate bits `AST_SG_JIT_DISPATCH/_GUARD`
(bits 40/41, `mccgate.h`); mirror in `ast_search_gates_now`/`_set` (`9393/9431`) and the searchable
set (`10009-10017`). Emit entirely at compile time: prepend an `AST_If` guard on the static
`context_in` live-in domain; then-arm = the optimized variant (existing fold strategies on a
domain-restricted clone); else-arm = the retained faithful baseline re-emitted inline via
`ast_replay_body` (`4070`) / the revert `memcpy` (`10469`). Reuse the `ast_tco_run` root-rebuild
idiom to prepend. This is real, differential-validatable guarded deopt with **no runtime recompiler**.
Validate: full repo bar with `MCC_AST_JIT=1`.

### M3 — Wire `--jit-functions` selection (D8 partial)
Make the parsed-but-inert `--jit-functions` (`libmcc.c:2152`) actually select which `sym`s get the
M2 dispatcher (default `main`, sites at common ancestor per `mcc.c:96`). `--embed-jit` currently only
prints the manifest (`mcc.c:1344`) — keep the manifest, add the selection wiring.

### M4 — Embed the libmcc engine slice into `-O4+` output (W4 prerequisite, heaviest piece)
Embed the ~800 KB strategy-engine + per-function intention-tree slice into the output so the runtime
can re-invoke it (D2=A). Dominant size/build cost — isolate behind `--embed-jit`. This is the gating
dependency for M5.

### M5 — Stage 2 runtime recompiler + hot-swap (W4)
Thin driver: dispatcher reads a global variant-pointer slot (init = baseline). Driver re-invokes the
embedded engine on a hot `sym` into a fresh `MCCState`/`run_ptr` via the `mcc_relocate` path
(`mccrun.c`), then one aligned 8-byte atomic store publishes the new pointer; triple-buffer/RCU
reclamation of the old region (`mcc_run_free`, `mccrun.c:101`). x86_64 ELF only (D7).
Validate: JIT differential — variant output == baseline output over the guarded domain.

### M6 — Stage 3 triggers + threads + budget (W5, D5=both, D8 pthread pool)
Add `jit-profile` strategy (live-in range capture + hot counter, the 3rd jit row). Emit a synthesized
`.init_array` ctor (`add_array`, `mccelf.c:1375`) that spawns the `--jit-threads` pthread pool via the
`threads.h` shim; the pool eagerly warms `--jit-functions` AND services counter-triggered lazy
recompiles. Enforce `--jit-max-duration` as the runtime budget bound (`mcc.c:1345` manifest → real
deadline). Link libpthread into `--embed-jit` output.

### M7 — `jit-patchpoint` strategy (D3B, optional)
4th jit row: nop-padded patchable prologue for in-place code-patch hot-swap. Lower priority; M5's
pointer-swap dispatcher is the primary mechanism. Deferrable.

### M8 — Stage 4 eval_slice soundness gate (W3, hardening — D6 places it here)
Independent AST-over-values interpreter `eval_slice(arena, slice, env) -> {value, defined}`. UB oracle:
`defined=0` on div/mod-by-0, `INT_MIN/-1`, shift `>= width` or `< 0`, and signed +/−/* overflow
(NOT the masking/​wrapping `ast_fold_eval`). Domain enumeration from `ast_vlat_context` (`7189`) via the
`ast_tco_run` live-in walk (`5944`); refuse any slice whose domain exceeds a fixed cap. Harness after
`ast_vlat_check_sound` under `MCC_CONFIG_AST_SHADOW`, then promote to a hard per-strategy gate. Also:
the static `context_in` domain that replaces the observed range in the guard.

## Critical path
M1 → M2 → (M3) → M4 → M5 → M6. M7 and M8 attach independently after M2. M2 alone is a shippable,
complete guarded-deopt JIT; M4 is the size/build gate for everything runtime.
