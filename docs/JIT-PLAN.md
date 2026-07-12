# §26 JIT runtime — finish plan

Singularly-focused, dependency-ordered plan to complete the entire runtime JIT + guarded deopt.
Settled decisions: D1=B embedded · D2=A recompile=re-invoke engine · D3=A entry dispatcher ·
D4=A runtime-observed live-in range · **D5=both (.init_array ctor + jit-profile hot counter)** ·
**D6=deopt-first, eval_slice as later hardening** · **D7=ELF x86_64 first** · **D8=pthread pool via
`runtime/include/threads.h`**.

**Baseline & cache model.** The JIT *baseline* is the AOT-compiled, possibly-optimized function
that ships in the object (the final emit at the chosen `-O`), NOT the pre-fold parse-time body. At
runtime the JIT produces a *further*-optimized variant specialized to an observed context, keyed by a
hash of that context (§21 epoch/cache key + §25 hot-value key); the cache maps `key → best-known
variant` (the baseline itself, or a more-optimized version). The dispatcher runs the cached variant
for the live key and **deopts to the AOT baseline on guard-fail / key-miss**.

Global gate: `MCC_AST_JIT` (default off) until the full validation bar passes, then P0-style flip.
Every milestone must clear the repo bar before the next: default byte-identical (ctest object-diff 0),
self-host 3-stage fixpoint byte-identical, -O6 differential == gcc == clang, cross-arch i386/arm64,
differential fuzz 0 miscompiles, shadow zero-divergence, UBSan trap clean.

## Key grounding (verified)

- `ast_strategies[]` has 4 free slots (20/24, `mccast.c:817,8872`) — exactly the 4 jit rows.
- Entry-prepend prior art: `ast_tco_run` tail rebuilds the root child list (`mccast.c:6068-6079`).
  Guard = `AST_If` (op-encoded); label/goto = `AST_Jump` op 4/5. No new node kind needed.
- Baseline = the AOT final emit (post-strategy), snapshotted from the live text section at the tail of
  `ast_func_end` into a `keep_baseline` pool keyed by `sym` (`orig`/`orig_rel` at `mccast.c:10246` are
  the pre-fold body + revert buffer, NOT the baseline). `ast_cur` already survives via
  `keep_inline`/`keep_reemit` (`10515-10519`); `keep_baseline` joins that condition.
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

### M1 — Baseline retention (W1) — LANDED (10cc7f05, 7114e90c)
`ast_baseline_retain` snapshots the AOT final emit (code `[ast_body_ind_sv, ind)`, relocs past
`ast_reloc0_sv`, shipped-body return-chain head `rsym`) + retained `ast_cur` into `ast_baseline_pool[]`
keyed by `sym`, via a `keep_baseline` flag beside `keep_inline`/`keep_reemit`. Gated `MCC_AST_JIT`
(default off). Validated: emit-neutral, retention fires, ctest 3968/3968 default + JIT-on.

### M2 — Stage 1 entry-dispatcher (W2, machine-byte splice, x86_64 first) — ships a complete guarded-deopt
Mechanism B (settled): the deopt arm is the AOT baseline reinstalled as *machine bytes*, not an
AST_If. Emit at compile time, replacing the body with `[guard; jcc deopt] [speculative arm] [jmp
epilogue] deopt: [AOT-baseline splice]`; both arms share one prologue/epilogue (frame = max(loc)) and
one `rsym` return chain.

- **W2.1 — reloc-rebasing splice primitive — LANDED (f18031e5).** `ast_baseline_splice` copies retained
  bytes to the live `ind`, rebases each reloc by `r_offset` only (`r_info`+addend position-independent
  for every x86_64 body reloc kind), and re-threads the open return-jump chain into `rsym`. Validated
  by the default-off `MCC_AST_JIT_SPLICE` harness: bit-correct execution over returns/loops/recursion/
  PLT calls; corpus semantics-clean.
- **W2.2 — dispatcher control flow — IN PROGRESS.** Guard (synthetic `xor eax,eax; jcc` for now) +
  speculative arm (non-rewinding replay, since `AST_PF_EMIT` hard-rewinds `ind`) + jmp-over + AOT-
  baseline splice; validated in two runtime modes (`MCC_AST_JIT_DISPATCH=1` never-deopt / `=2`
  always-deopt) so both arms are exercised. No runtime recompiler.
- **W2.3 — real speculative guard.** Replace the synthetic guard with a live-in domain check
  (`AstVLat`/`context_in`) where the speculative arm is a domain-specialized further-optimization;
  under deopt-first this rides the AOT differential. Also register `jit-dispatch`/`jit-guard` as gate
  bits (40/41, `mccgate.h`) + `ast_strategies[]` rows if search-selection is wanted.

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
