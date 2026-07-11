# Milestone plan — AST side-car substrate (rollout steps 1–3)

Design: `docs/AST.md`. This is the implementation plan for the **read-only side-car
substrate** — the first three staged-rollout steps. It deliberately stops short of
the `Strategy` engine (step 4) and the worker pool (step 5), which are a later
milestone that forks emission behaviour and reopens the determinism/`MCC_AST_ENGINE`
questions.

## Why this scope

Steps 1–3 are pure accelerators: a def/use side-table, monotone property memos, and
a structural hash. None of them changes an emitted byte — they only make the existing
queries (`ast_cprop_escapes` and friends, `ast_ident_same`) answer in O(1) instead of
re-walking the arena. That property is what makes them cheap to gate: each PR is
validated by the three existing gates with no new correctness surface.

- **Faithful replay** (`mccast.c:6480`) — per-function byte compare of text + reloc;
  unchanged, still the entry gate.
- **Corpus differential** — ctest `fuzz/{smoke,matrix,corpus}` (`tests/fuzz/runner.c`).
- **Self-host fixpoint** — ctest `fixpoint-invariant` (`tools/fixpointgate.c`), requires
  stage2 == stage3 == stage4 byte-for-byte.

Plus one substrate-local check (D3): a **shadow-assert build** that runs both the O(1)
side-table answer and the legacy O(N) scan and asserts they agree. Default-off,
compile-time `MCC_CONFIG_AST_SHADOW`. Not wired into a stock preset (the `debug`
preset is inherited by `release`, so forcing it there would leak into release, where
the O(N) cross-check + abort is unacceptable); it is opted into per-build
(`-DMCC_CONFIG_AST_SHADOW=ON`, e.g. a `cmake-shadow` dir) and in CI. This localizes a
substrate bug to the substrate instead of surfacing it as a codegen diff three gates
downstream.

## Decisions (settled)

| # | Decision | Choice |
|---|----------|--------|
| D1 | Milestone scope | Steps 1–3 (read-only side-car), shipped as 4 PRs |
| D2 | Step-1 update discipline | Build-once + dirty-flag lazy rebuild; graduate to incremental at Step 3 |
| D3 | Correctness proof | Shadow-assert build (`MCC_CONFIG_AST_SHADOW`) + the 3 gates |
| D4 | `(tag,id)` partition | Land now, paired with Step 1 |
| D4b | CST `slot_key` rename | Rename field → `branch_tag` (incl. `W/R` serialize + 3 accessors) |
| D5 | Engine gating flag | No `MCC_AST_ENGINE` yet — side-car always built (read-only); flag arrives at step 4 |
| D6 | Step-3 hash | New `uint64_t h[]` SoA column folding the exact `ast_ident_same` tuple, confirm-on-fire on collision |

## PR sequence

Each PR is independently revertible and gated by faithful-replay + `fuzz/corpus` +
`fixpoint-invariant`.

### PR-1 — `(tag,id)` naming partition (D4, D4b)

Pure refactor; no functional change; no new emitted bytes.

- **New header** mirroring the `cst_id` family (`mcccst.h:57-66`): a namespace enum
  and `name(tag,id)` / `tag_of(name)` / `id_of(name)` inlines over the same
  `(hi32<<32)|lo32` packing. Reserve two ranges: `AST_SLOT` (Step-1 keys) and
  `CST_BRANCH` (PP branch ordinals).
- **Rename CST `slot_key → branch_tag`** everywhere it appears in `src/mcccst.c`:
  the column decl (`:35`), `G(...)` alloc (`:93`), free (`:132`), per-node zero-init
  (`:158`), `W(...)`/`R(...)` serialize (`:824`, `:866`), and the three accessors
  `cst_mark_branch`/`cst_branch_ord`/`cst_is_branch` (`:544-555`) + the render reader
  (`:728-737`) + the writer at `:1112`. Field only — the `+1`-biased ordinal semantics
  stay identical, so the serialized format is unchanged (rename is source-only).
- **No AST column yet** — PR-1 only reserves the `AST_SLOT` tag range and the accessors;
  Step 1 (PR-2) is what mints keys into it.

Anchor: there is no `AstId`/`tag_of`/`id_of` today (confirmed); `CstId = (file<<32)|local`
is the only pack/unpack primitive. The `(tag,id)` scheme is a disjoint encoding for the
future H_e boundary, **not** a merged namespace — CST and AST keep separate local id
spaces; the sole cross-link stays the one-way AST `cst` column (`mccast.c:35`).

### PR-2 — Step 1: def/use projection + `cprop_escapes` bitmap (D2, D3)

The first proof of the side-car discipline: one pure-query side-table, no mutation.

- **Table shape** — one per-slot record `slot → {written?, escaped?, defs, uses}`,
  keyed by `int off = (int)(int64_t)ast_ival(a, ref)` of a `VT_LOCAL` non-`VT_SYM`
  `AST_Ref` (the key already used by `ast_cprop_koff[]`/`ast_cse_off[]`/`ast_promo_off[]`).
  Minted in the `AST_SLOT` tag range from PR-1.
- **Build point** — one O(n) sweep in `ast_func_end` after capture, before the pass
  block (`mccast.c:6491`). Dirty-flag (D2-A): any structural mutation
  (`ast_add_child`, `ast_ident_adopt`, `ast_clear_children`) sets `dirty`; the next
  query lazily rebuilds. No incremental patching yet.
- **Reroute the 4 rescans** to O(1) table lookups, cheapest-win first:
  - `ast_cprop_escapes` — 11 sites (`mccast.c:4352, 4559, 4615, 4704, 4856, 5107,
    5632, 5928, 6128, 6224, 6262`), O(N) → O(1). Biggest win; do first.
  - `ast_local_is_readonly` — 1 site (`:2097`), O(N) → O(1).
  - `ast_licm_written` — 5 sites (`:4594, 4616, 5632, 6263, 6274`), subtree walk →
    table query (per-slot `written?` within the loop window).
  - `ast_ivsr_count_writes` — 1 site (`:5930`), subtree → table `defs` count.
- **Shadow-assert (D3)** — under `MCC_CONFIG_AST_SHADOW`, every rerouted query also
  runs the original scanner and asserts equality. The scanners stay in the tree,
  compiled out of release builds, used only as the oracle.

### PR-3 — Step 2: per-node property memos

Bottom-up bit arrays for the monotone subtree predicates. Monotone ⇒ trivially correct;
lowest risk of the three.

- Memoize `ast_ident_pure` (12 sites), `ast_sccp_has_label` (12), `ast_cse_regpure`
  (8), `ast_cprop_safe` (~13). Each is a recursive subtree walk of identical shape
  today; each becomes one bottom-up fill + O(1) re-ask.
- Same dirty-flag discipline as PR-2; same shadow-assert oracle.

### PR-4 — Step 3: structural Merkle hash for `ast_ident_same` (D6)

The gate-heavy PR — introduces the first mutation-invalidation discipline.

- **New SoA column** `uint64_t *h` in `AstArena` (`mccast.c:21-39`, 14th array),
  folding the **exact** `ast_ident_same` tuple (`mccast.c:3779`):
  `kind, op, type_t, type_ref, ival, fbits, sym, nchild, combine(child h)`.
  Deliberately **not** the `ast_intention_hash` tuple (which omits `type_ref`, interns
  `sym`, skips a Ref's `ival`) — folding the equality tuple is what makes `h` a sound
  stand-in.
- **Incremental invalidation (D2 graduates to B)** — patch `h[n]` then re-fold up the
  existing `parent[]` spine (O(depth)). Wrap every edit path so none bypasses the
  patch: `ast_set_{kind,op,type,ival,fbits,sym}`, `ast_add_child`,
  `ast_clear_children`, `ast_ident_adopt`, `ast_ident_setlit`, and the Poison retag.
  `adopt` (`:3816-3833`) and Poison are the sharp cases — they change `nchild`/child
  set, so `combine()` must be re-derived, not leaf-patched.
- **Retire the 13 `ast_ident_same` walks** (`mccast.c:3932, 3968, 3996, 4024, 4798,
  5048, 5123, 5199, 5272, 5288, 5415, 5661, 6023`) to O(1) `h[x]==h[y]` compares with
  **confirm-on-fire**: a hash-equal that commits a rewrite first re-compares the actual
  subtree, so a collision can never miscompile.
- Extend `ast_arena_clone` (`:94-121`) and the growth/init paths to carry `h`.
- Shadow-assert: `h[x]==h[y]` must agree with `ast_ident_same(x,y)` on every compare.

## As-built (landed)

All four PRs shipped together. Implementation summary and the two deviations from the
plan above:

- **Invalidation is epoch-based, not dirty-flag/incremental.** `AstArena` carries a
  `uint64_t epoch` bumped by every mutator — the public setters, `ast_node`,
  `ast_add_child`, `ast_clear_children`, `ast_arena_reset`, and the three helpers that
  write columns directly (`ast_ident_adopt`, `ast_cse_setref`,
  `ast_ltemp_insert_before`; verified to be the *complete* direct-write set). Each
  side-car (`ast_du_*`, `ast_memo_*`, `ast_hash_*`) records the epoch it was built at
  and lazily rebuilds when it differs, so a query never sees a stale answer. This
  replaces both the per-mutator dirty flag (PR-2) and the parent-chain repatch (PR-4)
  with one mechanism that is trivially correct and was proven equal to the scanners
  across the whole corpus.
- **PR-2 scope.** The whole-function table subsumes the two whole-arena scanners
  (`ast_cprop_escapes`, `ast_local_is_readonly`) only. `ast_licm_written` and
  `ast_ivsr_count_writes` are *subtree-scoped* (they ask "written under node n", not
  "written anywhere"), so a whole-function `written?` bit cannot answer them; they were
  left as subtree walks. A descendant-indexed (DFS enter/exit) extension is future work.
- **PR-4 realization.** The structural hash is an epoch-rebuilt side-car array
  (`ast_hash` + `ast_hash_done`), not the incremental `uint64_t h[]` SoA column. It is
  used as a **collision-proof fast reject**: `ast_ident_same` returns 0 in O(1) when
  `h[x] != h[y]`, and falls through to `ast_ident_same_scan` on a hash match, so a
  collision can never produce a false equality. The only hash invariant that must hold
  — equal subtrees hash equal — is asserted on every reject in the shadow build.
- **The shadow oracle.** Under `MCC_CONFIG_AST_SHADOW`, every side-car query also runs
  the legacy scan and `abort()`s on divergence (`ast_du_diverge`, `ast_memo_diverge`,
  and the inline `ident_same` check). The `config-defines` catalog gained a matching
  `--ast-shadow` flag in `tools/build.c`.

**Validation.** `cmake-shadow` (`-DMCC_CONFIG_AST_SHADOW=ON`): 1904/1904 ctest pass,
zero divergences, including `fixpoint-invariant` (self-host byte-identity) and
`fuzz/{smoke,matrix}`. Production `cmake-debug`: 1904/1904 pass. Because shadow proved
every optimization decision is bit-identical to the scanners, the substrate is
byte-neutral.

## Step 4 (landed) — the strategy engine

Built on the proven substrate. Each fixed-pipeline pass is a `AstStrategy {name, gate,
apply}`; the pipeline order is the frozen table `ast_strategies[]`, consumed by
`ast_func_end`. It is the **sole** pipeline — the legacy inline block and its
`MCC_AST_ENGINE` toggle have been removed. Byte-identical to the former legacy path by
construction (same functions, order, gates, args) and by measurement before removal:
900/900 object diffs across 300 generated programs × `-O1/-O2/-O3` were identical;
making it the only engine changed no bytes — ctest stays 1904/1904 (incl. fixpoint +
fuzz + byte-exact goldens) and shadow is 1904/1904, zero divergences. `match` = the
gate; `est_cost_delta` is deferred to Step 5.

## Step 5 core (landed) — the live -O4+ search

At -O4+ (`optimize_search_seconds > 0`) with the strategy engine, `ast_func_end` runs a
per-function search (`ast_search_select`) over the four toggleable fold gates (templates,
narrow, bitflag, sethi) rather than applying the single frozen order. Candidates are
memoized by `ast_intention_hash`.

**Execution model (shared shape with the runtime JIT).** Each candidate gate config is a
stackless coroutine; one *tick* applies exactly one strategy pass to that candidate's own
`ast_arena_clone`. The scheduler is not round-robin — it advances the live candidate with
the **least total time consumed so far** (running sum of its tick durations; ties → the
baseline config, finished first as the safe fallback), which keeps the schedule fair (no
candidate starves or monopolizes the budget). A rolling window of the last 10 tick
durations predicts the next tick's cost; the search stops once that prediction would
overrun the remaining budget. The budget is the `-ON` seconds, **absolute** from the
first tick (measured with a portable `clock()` CPU-time proxy — accurate for the
single-threaded CPU-bound search, and available to the asttool unit harness which
excludes the mcchost timer). `ast_search_abort` is a forced-abort hook (pool / JIT /
deadline signal) checked at every tick boundary; since each candidate mutates only a
throwaway clone, an abort discards in-progress work safely — the pool's "kill and restart
the worker" reduces to this discard.

**Safety architecture — search selects, existing path emits.** The search only picks a
gate configuration; the winning config is produced by the *normal, unmodified*
pipeline+emit path on the *untouched* captured tree. Because every gate configuration
yields correct code (each pass is individually sound + faithful-gated), a search bug — or
a time-truncated / aborted search — can only pick a larger-but-correct config, never a
miscompile. Measurement runs the fold passes on clones and scores by static cost with
**no emit**, so none of the emit-cursor / promo-plan / `*_total`-counter hazards apply;
the module counters are saved/restored around the search regardless.

Single-threaded. -O4+ output is timing-bounded and non-reproducible by design
(quarantined there); `-O1..-O3` carry no budget, never search, stay byte-reproducible.
Opt-in (`MCC_AST_SEARCH=1` + `-O<n≥4>`) — a dedicated flag kept off the default -O4+ path
so it does not perturb the out-of-process superopt's per-worker gate measurements.
Validated: default path 1904/1904 ctest (search never engages); `MCC_AST_SEARCH -O5/-O6`
differential correct vs gcc/clang; shadow zero side-car divergences; the `-ON` budget is
an absolute cap (verified no hang, finishes early when candidates complete); asttool
55/55.

## Out of scope (Step 5+ continuations)

Tracked in `docs/TODO.md`: emitted-size / JIT-runtime scoring (needs §22 scratch-Section
emit isolation), a wider search space (inline/promote axes, permutations, budget-scaled
best-first frontier by `est_cost_delta`), the disk-backed cross-build memo (subsuming the
out-of-process `mcc_superopt_perfn`), the NCores-1 coroutine thread pool, and the runtime
JIT + guarded deopt (depends on §26). Non-determinism enters only at the JIT tier and
stays quarantined to -O4+.
