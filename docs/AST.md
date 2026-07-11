# AST optimization substrate

Status: design finalized. The staged rollout at the end is the implementation
order; step 1 is the first PR. The two previously-**OPEN** items (worst-case
scoring axis; the naming authority) and the three research/investigative designs
(equivalence checker, bidirectional hash + context, guarded deopt) are now
settled — see "Settled research questions" below. Implementation of each stays
future work tracked in `docs/TODO.md`.

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

## Settled research questions

The five research/investigative items from `docs/TODO.md` are decided below.
Each is a settled *design*; the implementation of each stays future work
(rollout steps 3-5 + the JIT). Code anchors are the grounding evidence.

### Exhaustive-equivalence checker — a second, independent AST evaluator

Decision: the checker is a **standalone AST-over-values interpreter**
`eval_slice(arena, slice, env) -> {value, defined}` over the 15-kind IR, **not** a
reuse of faithful replay. Replay (`ast_replay_value`, `mccast.c:2747+`) only
re-emits machine code — it never computes a result — so the checker is a fresh
evaluator, and original-vs-rewrite becomes a *differential* between two unrelated
implementations, which is the point.

- **UB-definedness oracle.** `eval` returns `defined = false` on any UB; agreement
  is required only where the *original* is defined (per Soundness above). The UB
  catalog is seeded from the constant folder `gen_opic` (`mccgen.c:2314-2455`):
  signed `+ - *` overflow (`pp_signed_ovf`), div/mod by zero, and — critically —
  **shift ≥ width must be modeled as UB**, because `gen_opic` currently *masks* the
  shift as a platform behavior (`l2 & 63|31`), which would certify a wrong rewrite.
  Extend with the UBSan hook classes (null deref, etc.).
- **Domain from `context_in`.** Live-in slots are enumerated by the existing pattern
  in `ast_tco_run` (`mccast.c:4836-4857`): `VT_LOCAL` refs filtered through
  `ast_cprop_escapes` / `ast_local_is_readonly`, already restricted to the
  enumerable (integer, non-escaping) domain. The context-restricted domain bounds
  the enumeration; a slice whose domain exceeds a fixed cap is **refused** — it
  stays JIT-speculative and never enters `-O1..-O3` (the tractability rule).
- Sanity-check time is tracked separately from apply time (already required).

Depends on rollout step 1 (def/use bitmap) for O(1) live-in enumeration.

### Bidirectional incremental tree/stack hash + `context_in`/`context_out`

Decision: add one **per-node `uint64_t h[]` column** to the arena SoA
(`mccast.c:21`) folding the **exact `ast_ident_same` tuple** — `kind, op, type_t,
type_ref, ival, fbits, sym, nchild, combine(child h)`. This is deliberately *not*
the current `ast_intention_hash` tuple, which omits `type_ref`, interns `sym`, and
skips a Ref's `ival` (`mccast.c:392-397`); folding the equality tuple is what makes
`h` a sound O(1) stand-in for `ast_ident_same` (~15 sites), with confirm-on-fire
covering collisions.

- **Incremental via the existing `parent[]` spine.** `parent` is already a column
  (`mccast.c:23`), maintained even across splices (`ast_ident_adopt` re-parents,
  `mccast.c:3828`). On edit: recompute `h[n]`, then re-fold up the parent chain —
  O(depth). The ~10 mutators (`ast_set_{kind,op,type,ival,fbits,sym}`,
  `ast_add_child`, `ast_clear_children`, `ast_ident_adopt`, `ast_ident_setlit`, the
  Poison retag) must be wrapped so no edit path bypasses the patch. `adopt` and
  Poison are the sharp cases — they change `nchild`/child-set, so `combine()` must
  be re-derived, not leaf-patched.
- **Bidirectional = the structured tree is its own stack.** Because control flow is
  the `AST_If`/`AST_BasicBlock` nesting (no arbitrary CFG edges),
  `context_in(point)` is the hash-fold of the reaching prefix (entry→point in
  structured order) and `context_out` the fold of the continuation — both walked
  along the ancestor chain + left-siblings. That structural nesting is what makes it
  O(depth) incremental instead of a full CFG dataflow fixpoint.
- **Facts, first cut.** `context_in` carries (a) the structural prefix hash (the
  memo key) and (b) the value-domain restriction on live-in slots (the checker's
  enumeration bound). (b) comes from a bounded backward walk collecting the
  equality/range predicates of dominating `AST_If` conditions — O(fixpoint) first,
  O(1) warm — reusing the transient `ast_cprop_{koff,ktt,kval}` set shape
  (`mccast.c:4199-4204`) as the fact representation. No persistent lattice exists
  today; this is the net-new half.

Ships as rollout step 3 (structural hash) first; the `context_in` domain half
layers in alongside the checker.

### Runtime guarded deopt (OSR) — entry-guarded dispatch first, interior OSR deferred

Decision: deopt lands as **entry-guarded variant dispatch**, not interior state
transfer. At a JIT-optimized region's entry, check the live-in domain bound (the
guard *is* the proof's domain restriction — no separate synthesis); on failure,
dispatch to a proven variant matching the new `slice (X) context` (memo lookup), or
fall through to the **static byte-faithful baseline** — which already exists as the
preserved original bytes in `ast_func_end` (`mccast.c:6422-6423, 6618-6619`). An
entry guard keeps the OSR state map trivial (just the incoming live-ins),
sidestepping mid-function live-state transfer.

- **Interior OSR is deferred** as the highest-risk piece, behind the same
  domain-restriction guard, until a runtime recompiler + hot-swap exist.
- **Hard prerequisite:** there is *no* runtime recompiler today — `mcc_relocate`
  (`mccrun.c:80-99`) is a one-shot `-run` loader, `--embed-jit` only prints a
  manifest (`mcc.c:1339-1342`), and `so_jitscore` is a build-time autotuner
  (`mcc.c:321-1236`). Deopt is therefore gated behind §26 (embedded recompile +
  atomic-pointer hot-swap). Confined to -O4+/JIT throughout; `-O1..-O3` never deopt.

### Global naming authority — disjoint `(tag,id)` partition, activated at H_e

**Principle: the CST and AST stay independent.** They keep their own separate local
id spaces (`CstLocal`, `AstLocal`) and never share a mutable id counter or a
back-pointer. The `(tag,id)` scheme is *not* a merged namespace that couples the two
arenas — it is only the disjoint encoding used at the H_e boundary, where a name
must be globally unambiguous. The sole cross-link is the existing **one-way,
explicit AST→CST reference**: the `cst` column (`mccast.c:35`) holding an opaque
`CstId`. That is the only "reliable/safe" sharing channel; nothing reaches the other
way, and neither arena's local ids leak into the other's.

Decision: the global name = **`(tag, id)`**, `tag` = namespace discriminant, `id` =
per-namespace ordinal, packed exactly like the existing `CstId = (file<<32)|local`
(`mcccst.h:57-66`). The AST per-slot key (built in rollout step 1) is minted in an
`AST_SLOT` tag range with `id` = the `VT_LOCAL` slot offset; the CST PP-branch tag
lives in a `CST_BRANCH` range with `id` = `branch_ord`. Disjoint by construction —
the tag partition is what lets both name spaces coexist in one H_e hash *without*
either arena having to know the other's ids.

- **Correction of record:** there is no AST `slot_key` today. The only `slot_key` is
  the CST array (`mcccst.c:35`), used *solely* to hold PP-conditional branch
  ordinals via `cst_mark_branch` (`mcccst.c:544-555`, `+1`-biased, 0 = "not a
  branch"). The bridge between the id spaces is the AST `cst` column (`mccast.c:35`)
  holding a `CstId`.
- **Do now (cheap), activate later.** Fix the tag *partition* immediately — a header
  enum + `name(tag,id)` / `tag_of` / `id_of` macros mirroring `cst_id*` — so step 1
  mints AST slot keys in the reserved range from day one and no migration is needed
  when H_e lands. Rename the CST field `slot_key -> branch_tag` to kill the
  misleading dual-use name. The authority only *activates* at the H_e accumulator
  (step 5+); the partition is defined up front to avoid rework.

### Worst-case vs average scoring axis — worst-case

Decision: score the runtime-delta axis by **worst-case path cost**, not
profile-weighted average.

- Average-case needs per-branch probabilities the compiler **has at no tier**:
  `__builtin_expect`'s weight is discarded (`parse_builtin_params(0,"ee"); vpop();`,
  `mccgen.c:8678-8681`), and both would-be profile sources (§24 `-g`
  entry-frequency, §25 hot-value branch/switch cache) are unbuilt.
- `-O1..-O3` take **no** runtime measurement (only static `ast_fn_cost` and byte
  size); `-O4+` `MCC_AST_JITSCORE` measures a **single concrete input**, best-of-3
  (`mcc.c:756-770, 1095-1113`) — one path, not a distribution, so it cannot validate
  an average either.
- Worst-case is monotone and deterministic — compatible with the `-O1..-O3`
  byte-reproducibility invariant; profile weighting would inject input-dependent
  nondeterminism into the frozen order. The landed model is already worst-case
  (`ast_fn_cost = nodes*(maxdepth+1)*(calls+1)`, `mccast.c:5145-5163`; promotion
  weighting `1<<depth`, `mccast.c:2327`), with static loop-depth as the frequency
  proxy.
- **Revisit** only when §25 records real branch/switch frequencies — the first point
  at which an average is computable.

## Staged rollout

1. Def/use projection + `cprop_escapes` bitmap (pure query, no mutation risk).
2. Per-node property memos (monotone, trivially correct).
3. Structural Merkle hash for `ast_ident_same` (needs invalidation discipline).
4. `Strategy` objects wrapping existing passes; frozen table consumed
   deterministically at `-O1..-O3` behind `MCC_AST_ENGINE=strategy`.
5. Worker pool + live search at -O4+, warming the shared memo.
