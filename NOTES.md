# NOTES

Design rationale that does not belong in code comments (project style: no code
comments) or in TODO.md (open work). Newest first.

## Escape-aware purity (ast_fn_purity_noescape) — why a separate function

`ast_fn_purity` returns IMPURE on the first `AST_Store`, which is correct for its
job (gating KGC memoization: a memoized variant must have no writes the caller
could observe) but too strict for the const-fold pillar — a loop that accumulates
into a plain local (`for(...) s+=i;`) is a pure function of its inputs, yet every
`s=…`/`i++` is an `AST_Store`. `ast_fn_purity_noescape` makes the one distinction
that matters: a store is a non-escaping side effect iff its target is a *direct
local scalar lvalue* — an `AST_Ref` with `(r & VT_VALMASK)==VT_LOCAL`, `VT_LVAL`,
and `!VT_SYM`. A store through a pointer (target isn't that shape), a store to a
global (`VT_SYM`), any *address-of-local* anywhere in the body (`VT_LOCAL`,
`!VT_LVAL`, `!VT_SYM` — the address escaped, so an alias could store), an
`AST_Invoke`, or a `VT_VOLATILE` type all forfeit the guarantee and return
IMPURE. Loads still demote to TIER1 exactly as `ast_fn_purity` does.

It is deliberately a **new** function, not a change to `ast_fn_purity`: touching
the latter would move KGC-memoization eligibility and break fixpoint. Additive
⇒ default path byte-identical. This is the analysis half of the AOT const-fold
pillar (STEP 2(i)); the loop interpreter that turns a certified no-escape slice
into a constant is the remaining subproject — `ast_eval_slice` evaluates no loops
yet, and a compile-time loop evaluator must be bounded + UB-sound to avoid the
historical fold-miscompile failure mode.

## STEP 3 live slice hot path (mccjit_slice_search) — two non-obvious traps

The partial-slice pillar's primitives were all built + selftested in isolation;
wiring them into the live promote seam surfaced two traps that the isolated
selftests never hit because they call reemitted kernels *directly*.

1. **Never direct-call the shipped baseline to build a correctness oracle.**
   The first cut verified the slice kernel by comparing it against
   `st->baseline(args)`. `st->baseline` is the value returned by
   `mcc_jit_recompile_blob`, which for a routed (int-arg) function is a
   *dispatch-only KGC stub* — its entry begins with a frame-unwinding sequence
   and is only valid when entered via `jmp *slot` from the counter stub, never
   via a plain `call` (see [[mcc-jit-kgc-stub-dispatch-only]]). Direct-calling
   it corrupted the stack and the process later jumped to a stack address
   (`0x7fffffff…`). Fix: the oracle is a *faithful whole-function reemit* of the
   same intent (`mccjit_reemit_arena_blob(blob,len,NULL,&keep)`), which returns a
   raw `mcc_get_symbol` entry that IS directly callable; compare kernel-vs-
   faithful over the captured live-ins. No live baseline call, so no stub hazard
   and no re-entrancy/deadlock on the slot's counter.

2. **A variant published to a slot must carry the leading-`leave` trampoline.**
   `mccjit_dispatch_entry` (and the real counter stub) enter a slot's variant via
   `push rbp; mov rbp,rsp; jmp *slot` — so the variant must begin with `leave`
   (`0xc9`) to rebalance that frame. `mccjit_make_trampoline` prepends exactly
   that; the whole-function search wraps its winner with it. A raw
   `mcc_get_symbol` kernel has no leading `leave`, so publishing it verbatim
   imbalanced the stack. Fix: verify on the *raw* kernel (direct calls are
   correct), but `return mccjit_make_trampoline(kern)` so the published entry is
   slot-enterable, identical in shape to the whole-function path.

Soundness rests on gating the slice path to `AST_PURITY_TIER0` (no
load/store/invoke/volatile): there the function body IS `return <pure expr>`, so
the wrapped return slice reemit equals the whole function by construction, and
the live-in differential is defense-in-depth. Anything else falls back to the
whole-function search. Default-off (`MCC_JIT_SLICE_SEARCH` unset) ⇒ both seams
behave exactly as the prior inline ternary ⇒ fixpoint byte-identical.

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

## Concurrent hot-swap is ThreadSanitizer-clean

The live search's async path publishes an improving variant to the dispatch slot
from the pool WORKER thread (mcc_jit_publish) while the main thread dispatches
through that same slot (jmp *slot) — a genuine write-vs-read concurrency point.
Validated race-free under TSan (separate build, -fsanitize=thread; ASan and TSan
are mutually exclusive): jit_selftest_pool + jit_selftest_lazy under
MCC_JIT_SEARCH=1, 0 data races across 6 repeated runs (races are timing-dependent,
hence the repeats). The pointer-swap publish + the pool qlock/swap_lock ordering
hold up. Repro mirrors the ASan recipe above but with
-DCMAKE_C_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g".

## Two-part -O4 regression (regression/o4-aot-jit)

Proves the AOT optimizer and the embedded JIT are the same optimization engine seen
from two sides. tests/ci/regression_o4_aot_jit.sh:
 - Part 1 (JIT off, MCC_JIT=0): `mcc -O4 -c src/mcc.c` with MCC_AST_SEARCH engages the
   AOT combo/strategy search for ~4s (the -O4 => optimize_search_seconds=4 budget;
   libmcc.c:2680), evaluating ~29k candidates with the RANGE gate (`●range`, the
   const-guided value ranges) active. Asserts wall in 3..8s + evaluated>1000 + range
   gate shown. (MCC_JIT=0 makes it AOT-only; the search code is JIT-independent. A
   genuine -DMCC_CONFIG_JIT=OFF -DMCC_EMBED_JIT=OFF build — zero mccjit_* symbols —
   was separately confirmed to engage the same 4s AOT search: wall=4.03s, evaluated
   59614, range=75, proving the AOT optimizer is not JIT-dependent.)
 - Part 2 (JIT on, MCC_JIT=1 MCC_JIT_SUBMIT_AOT=1 + -O4 -run): the backend hands its
   live compiled AST for the hot function to the engine via mcc_jit_submit_ast (the
   backend-override API), and the JIT recompiles the hot function FROM THAT submitted
   AST — not from the shipped MccjitIntent. Asserts busy is submitted
   (mccjit-aot-submit[busy]) AND the override path fires (mccjit-override[busy]: using
   backend-submitted AST) AND the JIT output equals the MCC_JIT=0 reference.
Registered under MCC_EMBED_JIT with TIMEOUT 90.

## Backend override API (mcc_jit_submit_ast) — why it stores a serialized blob

The backend hands a dynamically-compiled AST to the engine so the runtime JIT
optimizes/hot-swaps from the backend's AST instead of the baked MccjitIntent. The
override wins over the shipped intent per-symbol (keyed by anchor_sym_v; B1a keeps the
deserialize path for un-submitted syms). CRITICAL: a raw AstArena clone is NOT portable
across MCCState contexts. Every function references its PARAM syms (and callees/globals)
as live Sym* POINTERS; those are freed by the time the runtime hot-recompile builds a
fresh MCCState, so reemitting a cloned arena dereferences dead pointers and segfaults
(observed: full-mask reemit of glibc stdio inline stubs crashed). The Sym*->name-handle
remap that makes an arena portable is exactly what mccjit_intent_serialize/_deserialize
already perform. So mcc_jit_submit_ast SERIALIZES the arena at submit time (while the
Sym*s are still live) into the override table (mccjit_override_put), and
mccjit_recompile_common re-deserializes the override blob in place of the shipped intent
on a match. A true zero-serialization live-AST hand-off would require recompiling in the
origin MCCState or replicating the remap on the live arena — deferred; serialize-on-
submit is the sound path and reuses the ship-to-disk machinery. Default OFF
(MCC_JIT_SUBMIT_AOT unset) => no submissions => fixpoint byte-identical.
