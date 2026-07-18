# NOTES

Design rationale that does not belong in code comments (project style: no code
comments) or in TODO.md (open work). Newest first.

## AST-replay: separate frontier for replay-time scratch (ast_alloc_temp_loc)

`ast_alloc_loc` records each AST-local's stack offset during the pre-transform
record pass and stores it *in the AST node*, so replay must hand back the exact
same offset (`ast_locrec[ast_locrec_i++]`). But AST transforms that run between
record and replay can *add* stack allocations that the record never saw:

- register promotion (`ast_promo_entry_init`) reserves a callee-save spill area
  for callful functions;
- backend spills (`get_temp_local_var`) can differ in count/size/order because
  the post-transform body emits different code (e.g. DIVMAGIC's 64-bit MULHU).

The pre-fix code allocated both of those from the shared `loc` frontier, which
at replay start sits just under the frame base — exactly where the recorded
AST-local offsets live. Result: a replay-time scratch slot (or the promo save
area) overlapped a recorded 8-byte local, and a later narrow store clobbered its
high dword. Concretely this corrupted the `AstArena*` arg of `ast_divmagic_try*`
with a stray bit-34, crashing in `ast_node` ~3300 candidates into an
`MCC_AST_SEARCH` self-host run. Layout-sensitive (only SEGVs when the corrupt
pointer hits an unmapped page), invisible to ASan (wrong-width access of valid
memory), and hidden by `-g` (which disables AST-replay).

Fix: replay-time scratch gets its own frontier, `ast_temp_frontier`, seeded at
each replay entry to `ast_locrec_min` (the lowest recorded AST offset) and only
ever decreasing. `ast_alloc_temp_loc` serves both the promo save area and
`get_temp_local_var`, so all replay-added slots live strictly below every
recorded AST slot and can never overlap them regardless of order/size drift.
`ast_alloc_loc` (AST slots) is unchanged except to track `ast_locrec_min`.
Ordering matters: the frontier reset + `ast_loc_low = loc` seed must run *before*
`ast_promo_entry_init` in the replay macro, else the promo allocation is wiped.
Validated: repro no longer SEGVs, self-host search evaluates ~26k like gcc-mcc,
`fixpoint-invariant` byte-identical, `ast/` + `jit/` ctests green.

## Step 2 const-eval / const-fold (ast_jit_const_fn, ast_jit_fold_consts)

`ast_jit_const_fn` reduces a single-pure-return function whose return has no free
variables to a constant (empty-env `ast_eval_slice`). `ast_jit_fold_consts` walks
the arena and rewrites free-var-free integer Binary/Unary nodes to literals
(op=VT_CONST, keep type). Convert nodes are excluded — a void-typed conversion
"evaluates" but folding it to a VT_CONST literal yields "operation on void value"
at codegen. The compiler already const-folds plain source during compile, so a
fresh deserialized intent has nothing to fold; `ast_jit_fold_consts` earns its
keep only on ASTs where a later transform materializes new constants (e.g.
spec-fold turning a param into a const, so `param*2+1` becomes `3*2+1`). Its
end-to-end correctness is covered by jit/selftest-fold-consts, which forces the
fold through every recompile (including the vspec/spec-fold path) and requires all
selftests to stay correct; the consteval unit test only asserts it is a safe
no-op on an already-folded intent.

## Runtime search loop (mccjit_search_masks)

The heart of "the JIT running perm x combo x strategy optimizations until its
timeout": recompile the hot blob under each candidate gate mask
(mcc_jit_recompile_blob_gated -> ast_reemit_with_gates), bounded by a wall-clock
budget (CLOCK_MONOTONIC via mccjit_elapsed). jit/selftest-search validates the
loop: 128 masks all built with no budget; a 3ms budget stops early (~13 built);
every winner still computes f(x)=x*2+1. SELECTION is currently first-valid — a
placeholder — because the two natural fitnesses aren't cleanly available in a
standalone helper: emit size needs mccjit_last_state->text_section but
`text_section` is a state-macro (MCC_STATE_VAR) that can't deref an arbitrary
state; runtime bench (mccjit_bench_admit) needs a live MccjitCounterState. Both
ARE available at the promote seam (mccjit_counter_tick, st in hand), so the
fitness-driven selection lands when the loop is wired there (next), reusing the
existing bench_admit as the fitness and jit_max_duration as budget_s.

## Runtime search path is ASan+UBSan clean

The live perm x combo x strategy search (combinatorial recompiles, per-improvement
mcc_jit_publish incremental hot-swap, mccjit_bench_admit selection, variant
lifetime) was validated memory-safe + UB-free. MCC_BUILD_SANITIZE only builds a
separate mcc_s, NOT the selftests, so a fully-instrumented build is needed:

  cmake -S . -B <dir> -G Ninja -DMCC_EMBED_JIT=ON -DMCC_CONFIG_JIT=ON \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g"
  cmake --build <dir> --target jit_selftest jit_selftest_kgc jit_selftest_pool \
    jit_selftest_struct jit_selftest_purity
  ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
    MCC_JIT_SEARCH=1 MCC_JIT_HOT_THRESHOLD=2 MCC_JIT_SEARCH_MS=25 ./jit_selftest_...

detect_leaks=0 is required: the search leaks losing variants by design
(pointer-swap-and-cap; QSBR never frees a possibly-live variant). Result: 0
AddressSanitizer/UBSan reports across int/kgc/struct/purity/async-pool shapes,
including the incremental publish issued from the pool worker thread. ASan does not
instrument the JIT-emitted machine code itself (not compiled with ASan) — it covers
the engine's orchestration (deserialize, recompile, KGC stub build, bench, publish,
variant bookkeeping), which is exactly the new search code.
