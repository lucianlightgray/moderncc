# Design notes

Rationale that does not belong in code comments or the TODO checklist. Newest first.

## JIT/AST `/goal` campaign — CHECKPOINT (resume here)

Autonomous campaign to implement all JIT/AST TODO items. **All landed work is on `main` (through 98959ace);
every landing held default `3968/3968` byte-identity.** Pattern for each item: a worktree agent implements
it **gated behind a new env, default OFF → default byte-identical**; validate the gated-ON path to the full
bar (differential fuzz vs gcc+clang + self-host 3-stage fixpoint); integrate + independently re-verify
firing + correctness; commit; update TODO.

**Landed:** entire §26 runtime JIT (complete — live self-hot-swap, known-good-cache soundness, purity
gate, oracle). §27+ optimizations, each gated: §29 outer-narrow `MCC_AST_NARROW_ELIM`; §29 right-shift
narrowing `MCC_AST_VLAT`; §33c arg-forwarding `MCC_AST_ARGFWD`; §35 n-ary Sethi–Ullman `MCC_AST_SETHI_NARY`;
§36 spill-slot sharing `MCC_AST_SPILL_SHARE`. §27 loop foundation: loop-nest model + dependence test +
`ast_loop_interchange_legal`/`ast_loop_fusion_legal` (analysis-only, inert; dumps `MCC_AST_LOOPNEST_DUMP`/
`MCC_AST_LOOPDEP_DUMP`). Fixed a real bug: `MCC_AST_COLOR` self-host miscompile (unsound cdeffirst interval
heuristic). Pruned stale TODO items (§31 registry already done, etc.).

**RESUME HERE (next items, in order):**
1. **§27 loop interchange** — the first transform on the legality gate. New env e.g. `MCC_AST_INTERCHANGE`
   (default off). Swap two adjacent PERFECTLY-nested `for` loops (`AST_If` op 3, children [cond,incr,body])
   when `ast_loop_interchange_legal(outer,inner)` AND a locality heuristic helps; pure header exchange.
   Full-bar; RESULT-identity is the correctness gate (reordered loop must give identical results).
2. **§27 loop fusion** (`ast_loop_fusion_legal`), then **§27 tiling**.
3. Then: §24 hot-slice ranking (uses `ast_loop_depth`); §32a widening dataflow; §30 (needs the `.rodata`
   data-emission project first); FLOAT combo M2/M3 (search-infra, lower risk); V-* strategy decomposition;
   §26 marginal tail (float/struct KGC args, static-link E1a, bitfields, N-worker queue, M7 patchpoint).
4. **Endgame:** after broad exposure/soak, flip the validated gates default-on (P0-style); fix the
   pre-existing `MCC_AST_COLOR=1` interaction so COLOR can flip too.

**Validation recipe (per default-codegen item):** default full `ctest --test-dir cmake-debug` (3968,
object-diff 0) is MANDATORY (gate off = byte-identical); gate-on `tests/fuzz/fuzz_runner … --ref gcc … --ref
clang …` ~100 seeds 0 miscompile; gate-on self-host 3-stage fixpoint byte-identical (link stage objects via
mcc's OWN linker + the `mccrt_blob` object — GNU ld hits overlapping-FDE/`__va_arg`; stage2 needs the
embedded runtime, so the ad-hoc header recipe is fiddly — a targeted reproducer + objdump beats fighting it).
Independent-verify tip: a quick throwaway test often DOESN'T fire the pass (const-folds / wrong shape) —
confirm firing via `-v128` TRACE or an object-diff, then correctness vs gcc.

## §26 runtime JIT — 2026-07-12 parallel implementation push

Landed: M3 selection, search-bits 40/41, W2.3 modes 4/5, M8 oracle (e6e18acd); M6 plumbing
(362fe0d5); M4 scaffold (e47d6509). M5 recompiler+hot-swap is the remaining greenfield lane.

Decisions taken this push (A1a-indexed matrix), with the non-obvious "why":

- **A2a** — `MCC_AST_JIT`/`MCC_AST_JIT_DISPATCH` stay the master switches; `--jit-functions` only
  *narrows* which syms get the dispatcher. Bridging the CLI flag straight to the gate was rejected
  because `--embed-jit` defaults on, so gate-from-flag would enable the JIT by default and break the
  byte-identity invariant.
- **B1a** — the dispatcher is made search-selectable via `AstGateMask` bits 40/41, NOT as
  `ast_strategies[]` apply-rows. `ast_order_pack` packs row indices as 4-bit nibbles and rows already
  exceed index 15; adding apply-rows would alias them in the ordered-search memo. The deopt mechanism
  already ships as machine-byte splice, so no apply-fn is needed — bits suffice.
- **C3a** — the mode-5 range guard bound comes from the *static* `ast_vlat_context` fact, not a
  runtime-observed range (that needs the M6 instrumentation, not yet built). Entry params usually only
  carry the trivial type-full range, so mode 5 often emits a redundant `[INT_MIN,INT_MAX]` assertion
  whose spec arm equals baseline — sound and deopt-protected, but it only prunes when vlat proves a
  narrower fact.
- **D2a/M8** — `eval_slice` is wired shadow-only as a **baseline-vs-spec differential**, not a unary
  "is the spec defined?" check. The oracle's `defined=0` conflates genuine UB with "node not modeled",
  and the spec's function *root* is unmodeled → a unary check false-aborts every specialization. The
  differential aborts only when the baseline root is well-defined and the spec disagrees, so an
  unevaluable root is a no-op. Consequence (L1a): the oracle currently no-ops on whole-function roots;
  making it bite needs value-slice enumeration before promotion to a hard gate.
- **E1a/E3a** — embed Tier-B only (emit path) because Tier-A selection scores by a static cost model
  with no emit (`ast_cost_score`), so re-*selecting* a variant needs no backend. The real work of the
  intention-tree slice is the `sym`/`cst`/`type_ref` side-table closure (the arena's SoA arrays are
  index-based and trivially serializable); a salt witness (`ast_search_key_salt`) prevents a stale
  slice firing on an incompatible build/target.
- **F3d** — old-variant reclamation is **never-free first**: correct and bounded-leak. `mcc_run_free`
  munmaps immediately with no quiescence, so a real QSBR/epoch reclaimer is deferred, not day-one.
- **G2a** — the libpthread link for the JIT pool is gated on `jit_threads>0`, not `embed_jit`
  (default-on), to keep ordinary binaries pthread-free and byte-identical.
- **M5b runtime known-good cache (settled 2026-07-12)** — hot-swap soundness is a *runtime differential*,
  not just the static guard: per variant keep a sorted `mmap`'d file of live-in tuples proven
  optimized==baseline; the entry guard is a membership test; a miss runs the baseline (deopt) AND
  compares the optimized result, inserting only on match, permanently deopting (and flagging for
  recompile) on mismatch. The baseline execution IS the oracle, so the JIT stays correct even if the
  static guard/`eval_slice` oracle is imperfect — the static side just lets in-domain values skip the
  miss-path check. Key caveat: comparing by re-running both arms is only valid for PURE functions;
  impure functions need buffered/rolled-back effects or a pure-slice restriction (gates eligibility).
- **M5c pure/impure slicing (settled 2026-07-12)** — the answer to M5b's side-effect gate: slice a JIT'd
  function into pure computation kernels and impure boundary ops. Pure kernels get a bespoke off-C-ABI
  calling convention (register live-ins/outs, no ABI frame) and are freely re-runnable, so they are the
  only part M5b re-runs to compare against baseline; impure ops stay C-ABI "bound" calls executed once in
  order. The slice split is simultaneously the optimization win (custom ABI on the hot kernel) and the
  correctness gate (only pure slices are re-run/memoized).
- **P2a** — `MCC_AST_JIT` is targeted to flip default-on (P0-style) after the full validation bar
  (byte-identity, shadow, differential vs gcc/clang, self-host fixpoint, fuzz, cross-arch) plus a soak
  — it is not a permanent opt-in.
