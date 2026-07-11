# AST optimization substrate

Status: design finalized. The staged rollout at the end is the implementation
order; step 1 is the first PR. Two items remain **OPEN** (worst-case scoring
axis; the naming authority) but neither blocks steps 1-4. Open work is tracked in
`docs/TODO.md`.

## Purpose

Unify the three optimization drivers that exist today into one substrate that
both the AOT backend and a live JIT query through a single API:

1. the fixed in-process pass pipeline in `ast_func_end` (bfold -> ident ->
   narrow -> cprop -> cse -> dse -> sccp -> jt -> bf -> sethi -> tco -> ltemp ->
   ivsr -> pre), run once per function, gated by faithful replay;
2. the in-process per-function inline trial (the `AST_PF_EMIT` loop, "§22");
3. the out-of-process superoptimizer search (`mcc_superopt_search` /
   `mcc_superopt_perfn` in `src/mcc.c`) that spawns worker subprocesses, tries
   env-var config vectors, scores by size or JIT runtime, and caches per
   function by intention hash.

These do overlapping work with three different mechanisms (env config vectors,
byte-size measurement, disk cache). The substrate collapses them to one engine,
one memo, one scoring model.

## Governing principle: measure effectiveness, gate correctness

Zero assumptions about which transform is *best* — effectiveness is measured and
sorted, never hardcoded. Correctness is the opposite: it is a hard gate, not a
measured quantity. We do not measure our way to a correct binary; we measure our
way to an effective one, and every candidate must clear soundness before it can
be scored. The two are separate subsystems and must stay separate.

## Assumptions this design is built on

1. No anonymous code or data — every node, slot, and value has a stable name.
   Slot-aliasing across scopes is dissolved; def/use edges are exact, not may.
2. Any slice/window is re-contextualizable — given a chosen entry and exit, the
   entry facts and exit obligations are reconstructible on demand. Faithfulness
   becomes slice-local plus reconstructed context, not whole-function byte
   identity.
3. A bidirectional, incremental tree/stack hash indexes both look-back (reaching
   context) and look-ahead (downstream uses) from one structure.
4. Speculation is memoized: every transform explored, valid or invalid, writes a
   verdict to the side-car keyed by `slice-hash (X) context-hash (X) rule`.

## Side-car substrate

A set of parallel indices over the existing struct-of-arrays arena
(`AstArena`, `src/mccast.c:21`). Side-car only: it may inform pass-2 rewrites but
must never perturb the pass-1 faithful-replay bytes (no reach into
`cur_text_section`).

- **Structural Merkle hash** `h[n] = H(kind,op,type,sym,ival,fbits,
  combine(child h))`. Answers equality / occurrence / change. Serves the hottest
  query, `ast_ident_same` (~15 call sites), turning lockstep tree walks into O(1)
  compares. Patched up the parent chain on edit (O(depth)).
- **Def/use projection** — one per-slot side-table `slot -> {written?, escaped?,
  defs, uses}`. Collapses the four whole-arena rescans
  (`ast_local_is_readonly`, `ast_licm_written`, `ast_cprop_escapes`,
  `ast_ivsr_count_writes`) into one O(n) sweep; all become O(1). `cprop_escapes`
  (11 call sites) is the cheapest first win.
- **Lazy property memos** — per-node bit/byte arrays for the monotone subtree
  predicates (`pure`, `has_label`, `regpure`, `cprop_safe`), filled bottom-up,
  O(1) on re-ask.
- **Predicate-vector projection** — the semantic sibling of the structural hash:
  a packed bitset of tested-predicate truths over <=8 named slots in a window,
  for branch coalescing (generalizes `ast_bf_run` + §30).

## The single API

Warm = after the relevant hash/memo exists.

```
cursor(point, dir)                 -> handle           O(1)
hash(slice)                        -> fingerprint      O(1)
context_in(point) / context_out    -> facts + hash     O(1) warm / O(fixpoint) first
slice(cursor, predicate)           -> min closure      O(1) warm / O(|closure|) first
reaches(def,use) / writes_between  -> bool             O(1)
predicate_vector(cursor, keys<=8)  -> bitset           O(1)
transform(slice, context, rule)    -> result_hash | _  O(1) memo hit / else explore+memo
```

AOT and JIT are the same API with two settings — budget and entry policy. The
opt level selects the setting, and critically selects **whether the live pool
search runs at all**:

|                | -O1 .. -O3 (AOT)          | -O4+ (AOT tail / JIT)     |
|----------------|---------------------------|---------------------------|
| strategy order | frozen best-known from memo| live pool search          |
| determinism    | byte-reproducible (sealed) | non-reproducible by design|
| budget         | apply frozen order, no search | flat time/agent bound  |
| entry policy   | whole-TU roots            | hot slice from profile     |
| memo tier      | cold, on-disk (§21), read | warm, read-write + hot-swap|
| verbs called   | identical                 | identical                  |

The JIT is the backend at -O4+ with a finite budget and a profile-chosen entry
point. There is one code path and one memo; the opt level is a dial on it, not a
fork in it.

## Memo model

- Key = `hash(slice) (X) hash(context_in) (X) rule`. Context is a first-class
  key because a transform valid in one calling context can be invalid in
  another; the bidirectional index already produces `hash(context_in)` at the
  boundary.
- Monotone and append-only: invalid explorations record `-> _` (with reason),
  valid ones record `-> result_hash`. Both are reusable.
- **Confirm-on-fire**: queries may hit the memo probabilistically (O(1)); a hit
  that *commits a rewrite* first confirms by comparing the actual subtree
  (O(size)) so a hash collision can never miscompile. Cost is paid only when
  firing, never on read-only queries.

## Soundness (the correctness gate)

Effectiveness is measured; soundness is **proven by exhaustive equivalence**,
never measured or sampled for the deterministic tiers. Soundness is established
once per (strategy, slice, context), cached, and fires forever after.

- A speculative strategy earns soundness by an **exhaustive equivalence check**:
  under the reconstructed context, run the original slice and the rewritten slice
  over *every* valid input assignment and require agreement on every input where
  the original is well-defined. Equivalence is conditional on definedness — on
  inputs where the original has UB the rewrite may differ, so **the checker must
  model UB** or it certifies wrong.
- Passing lets the rewrite **claim the original's cache key** (be stored as a
  sound substitution for that `slice (X) context`). This runs inside the standard
  round-robin loop; the **sanity-check time is tracked separately** from
  find/apply compute time — verification is one-time and amortized, apply cost is
  per-compile, and the effectiveness score must reflect steady-state apply cost.
- `-O1..-O3` use **only already-proven strategies** — soundness must already be
  known. They never run a check; they test a cheap applicability precondition (a
  substrate query) and apply. This is what keeps them deterministic.
- The **runtime JIT** runs the same exhaustive check, **unbounded unless told
  otherwise**.
- **INVALID** (disagreement on a defined input, or emit fault) is authoritative
  and monotone; that (strategy, key) can never fire.

### Tractability: context restricts the input domain

Exhaustive enumeration terminates only for a bounded input domain, and
`context_in` is what bounds it: if the context proves a 64-bit variable is one of
four values here, the checker enumerates four, not 2^64. Re-contextualization
does double duty — cache key *and* proof-domain restrictor — so most branch-heavy
code (booleans, small enums, flag clusters) is enumerable once context-restricted.
A slice whose context-restricted live-in domain is still too large to enumerate
cannot be proven; it stays JIT-speculative (validated against observed inputs
only) and never enters `-O1..-O3`.

### Structural vs semantic (both required)

Confirm-on-fire guards structural identity (collision cannot miscompile); the
proven verdict guards semantic equivalence. Guard both.

### Baseline, versioning, fixpoint

Faithful replay stays the entry gate (a slice is optimizable only if its
unoptimized replay is byte-faithful). Memo keys include a rule version, so
tightening a rule invalidates exactly its cached verdicts (H_e epoch). The
3-stage self-host fixpoint runs against a **sealed proven-strategy set** — the
proven set grows across builds, so byte-identical self-host requires pinning it.

## The strategy engine (shared by all tiers)

- **Strategy Pattern**: each current pass becomes a `Strategy` object
  `{match, apply, est_cost_delta}` over a slice. No fixed pipeline order; order
  is data (the sorted strategy table), not code.
- **-O1 .. -O3 consume the table deterministically.** They apply the frozen
  best-known strategy order for each `slice-hash (X) context-hash` from the memo,
  confirm-on-fire, and emit. No workers, no timing dependence, byte-reproducible
  in sealed-cache mode. The refactor of the current `-O1` path is exactly this:
  re-express the 13 passes as `Strategy` objects over the substrate, consumed in
  a fixed order, sharing the one memo and index. This is what replaces the
  hand-ordered `ast_func_end` pipeline; it is not itself a search.
- **-O4+ runs the live pool search that warms the table.** NCores-1 workers
  explore permutation x combination x slice x strategy against the shared memo.
  Every worker's exploration (valid or invalid) writes to the same side-car, so
  work is never duplicated across workers or across builds. Because the search
  only writes memo entries, and lower tiers only read them, the search never
  makes `-O1..-O3` non-reproducible — it only makes their frozen order *better*
  on the next sealed rebuild.
- **Benchmarked, sorted effectiveness table** — the ranking that becomes the
  frozen order. Sort is **lexicographic**: compute-time-to-find/apply ascending,
  then original-vs-optimized runtime delta descending, then memory/register
  pressure ascending, then code size ascending. (Compute-time-first means the
  cheapest-to-apply strategy wins ties; expensive transforms rank only when they
  are the sole discriminator — appropriate for a continuously-running JIT.)
  Locality is **not** a score — locality effects (bit-packing many vals into one
  word, field reordering, hot/cold splitting) are `Strategy` objects and win or
  lose on the same measured runtime as any other transform. No proxy metrics.
  Soundness-confidence is not a score either; it is the verdict grade (see
  Soundness). Remaining candidate axis (**OPEN**): worst-case vs average for
  branch-heavy code.
- **Search budget (-O4+ single invocation)**: a flat wall-clock / agent bound.
  There is no tuned beam width — the frontier is a **best-first priority queue**
  ordered by `est_cost_delta`, expanded until the time slice is consumed, then
  the queue is checkpointed. Width is whatever fits the clock. The memo is both
  the result store and the search-resume point (§21); the next invocation resumes
  the widest-benefit candidates first.
- **Memo storage**: disk-backed and persistent; paged into memory on query and
  kept as a hot working set. TTL / sweep / eviction are deferred (disk is cheap;
  correctness never depends on a memo entry being present).

## Execution model: coroutine strategies on a C11 thread pool

- Every `Strategy` is a **stackless coroutine** — an explicit resumable state
  machine `step(ctx) -> YIELD | DONE | BLOCKED`, not a run-to-completion
  function. Suspension points are explicit (portable C11; no `ucontext` / stack
  switching). The deterministic apply path is the degenerate one-tick coroutine
  (apply, DONE) — uniform interface, zero added cost at `-O1..-O3`.
- Two drive modes, selected by the scheduler:
  - **over-time** (backend / -O4 AOT tail): resume, run until the time slice /
    the `N(-O)` budget is consumed, yield. Round-robin across strategies.
  - **per-tick** (runtime JIT): resume, do exactly one unit of work, yield.
    Round-robin, one tick each per round, continuously — optimization progresses
    interleaved with the running program and never stalls the mutator.
- `N(-O)` is the per-invocation search budget and scales with -O **only at and
  above -O4**; `N = 0` for `-O1..-O3`, which run no round-robin search and
  consume the frozen proven set directly.
- The **pool is NCores-1 C11 threads**, each a coroutine runner pulling
  steppable strategies from a work queue. This unifies the backend search pool
  and the JIT pool; they differ only in drive mode and lifetime.
- **Threads are optional and confined to -O4+/JIT.** If C11 threads are
  unavailable (`__STDC_NO_THREADS__`, or a target libc without them), -O4+/JIT is
  disabled and `-O1..-O3` still build and run single-threaded. The self-host
  fixpoint runs at a deterministic tier, so it never needs the pool.
- **Determinism invariant**: thread/coroutine scheduling may affect only *which
  strategies get proven* (cache contents), never a sealed build's emitted bytes.

## Runtime deoptimization (JIT, -O4+)

A proven transform is valid only within the **context-restricted domain** that
made its exhaustive proof tractable. That same restriction is the runtime
**guard** — bound the domain to prove it, check the bound at runtime; the guard
falls out of the proof, no separate synthesis.

- When **anonymous data** (a value whose bounds/identity are not yet established)
  reaches a JIT-optimized region, the guard may no longer hold. All downstream
  optimized coroutines **return** (yield BLOCKED); the scheduler dispatches to
  either a proven variant whose context matches the new data (memo lookup keyed
  by the new `slice (X) context`), or the **original static implementation** as
  the always-sound fallback (the byte-faithful baseline).
- Meanwhile the new data is **bounds-checked asynchronously** to re-establish a
  named context (rename the anonymous data by characterizing its range). Once
  named: if it fits a proven context, re-engage that variant; else keep running
  static while the pool works on proving one. Execution never stalls on
  optimization — static is always ready.
- This is the runtime consumer of the re-contextualization assumption:
  reconstructing the original context at a guard-fail boundary *is* the
  deoptimization (OSR-style state transition). Highest-risk component; gates on
  that assumption holding cheaply.

## What this subsumes

The strategy pool + memo is the shared engine. It replaces: the fixed
`ast_func_end` pipeline order, the `AST_PF_EMIT` §22 trial loop and its manual
state save/restore, and the `mcc.c` out-of-process env-config search. The
faithful-replay revert stays as the per-slice soundness gate; scratch-`Section`
isolation replaces the in-place save/restore desync.

## Resolved decisions

- **Determinism.** Live pool search is enabled only at **-O4+**. `-O1..-O3` stay
  deterministic and byte-reproducible (frozen memo order, sealed-cache mode), so
  the self-host byte-identity fixpoint gate is unaffected. Non-reproducibility is
  quarantined to -O4+, where it is intended.
- **Rollout.** Parallel path behind `MCC_AST_ENGINE` (`legacy` default, current
  13-pass pipeline; `strategy` = new engine). Flip only after the corpus
  differential passes byte-identical/better and the 3-stage self-host fixpoint
  holds. The legacy path stays as the fallback throughout the transition.
- **-O4+ search budget.** Flat wall-clock / agent bound, round-robin over slices,
  no tuned beam width (best-first frontier expanded until the clock, checkpointed
  to the disk-backed memo for resume), dedup by `slice (X) context`.
- **Scoring.** Lexicographic: compute asc, runtime-delta desc, mem/reg asc,
  size asc. Locality is a family of strategies, not a score.
- **Budget vs opt level.** `N(-O) = 0` for `-O1..-O3` (no round-robin search;
  frozen proven set consumed directly); N scales with -O only at and above -O4.
- **Execution.** Strategies are stackless coroutines (`step() -> YIELD/DONE/
  BLOCKED`), driven over-time (backend/-O4) or per-tick (JIT) by an NCores-1 C11
  thread pool that is optional and confined to -O4+/JIT; `-O1..-O3` and self-host
  run single-threaded. Scheduling nondeterminism touches only the cache, never a
  sealed build's bytes.

## Still OPEN

- Worst-case vs average scoring axis for branch-heavy code — confirm before
  wiring.
- The global naming authority for "no anonymous" (the `slot_key` vs
  `cst_mark_branch` PPConditional partition, `mcccst.c:544/1112`). Deferred by
  construction: steps 1-4 are pure-AST and never cross into PP tags; the
  reconciliation only bites when the H_e O(1) accumulator (step 5+) lands.

## Staged rollout

1. Def/use projection + `cprop_escapes` bitmap (pure query, no mutation risk).
2. Per-node property memos (monotone, trivially correct).
3. Structural Merkle hash for `ast_ident_same` (needs invalidation discipline).
4. `Strategy` objects wrapping existing passes; frozen table consumed
   deterministically at `-O1..-O3` behind `MCC_AST_ENGINE=strategy`.
5. Worker pool + live search at -O4+, warming the shared memo.
