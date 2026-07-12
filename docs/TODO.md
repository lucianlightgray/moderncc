# TODO

Sorted by number of open questions/ambiguities (first-round unknowns + the
sub-questions immediately following them), most-open first.

## AST substrate + unified optimizer — see `docs/AST.md`, plan in `docs/SUBSTRATE-PLAN.md`

Collapse the three optimization drivers (the `ast_func_end` pipeline, the §22
`AST_PF_EMIT` trial, the `mcc.c` out-of-process search) into one side-car
substrate + one memo + one strategy engine, shared by the AOT backend and a live
JIT. This reframes/subsumes several items below (§21 cache key, §22 emit
isolation, §28 rewrite IR, §33b/e seam+window keys, §30 predicate bitset, H_e
epoch hash, the time-budgeted engine, per-function `-O1`, PP-as-executable JIT).

**The `docs/AST.md` staged rollout (steps 1–5) is COMPLETE.** Step 1 def/use projection,
step 2 property memos, step 3 structural hash, step 4 strategy engine (now the sole
pipeline), and **step 5 "Worker pool + live search at -O4+, warming the shared memo"** —
the fork process pool (`MCC_AST_SEARCH_THREADS`), the live -O4+ search (`MCC_AST_SEARCH`,
subset-lattice candidates, static + emitted-size scoring), and the warmed memo (in-memory
+ disk-backed `mcc-search.memo`) — are all on `main`, each gated by `-O6` differentials +
`1905/1905` ctest + the shadow build, default builds byte-neutral throughout. Runtime
JIT + guarded deopt is **not** a rollout step (AST.md lists it as "rollout steps 3-5 **+
the JIT**"); it is the separate post-rollout item below, gated by the design on the §26
recompiler.

**LANDED — the read-only side-car (rollout steps 1–3).** All four PRs shipped as one
change; pure accelerators, no emitted-byte changes. Validated by the compile-time
shadow-assert build (`MCC_CONFIG_AST_SHADOW`, default-off, opt-in `cmake-shadow` dir +
CI) that runs each O(1) side-car answer against the legacy scan and aborts on any
divergence: 1904/1904 ctest pass under shadow with zero divergences, including
`fixpoint-invariant` and `fuzz/{smoke,matrix}`; the production (non-shadow) build is
byte-neutral by the same evidence. See `docs/SUBSTRATE-PLAN.md` for the as-built notes.

- [x] **PR-1 — `(tag,id)` naming partition** — `src/mccname.h` (`mcc_name`/
  `mcc_name_tag`/`mcc_name_id` + `MCC_NS_{AST_SLOT,CST_BRANCH}`); CST field
  `slot_key -> branch_tag` renamed (source-only, serialized format unchanged).
- [x] **PR-2 — Step 1: def/use projection** — per-slot `{written,escaped}` side-car
  (`ast_du_*`) rebuilt on `AstArena.epoch` change; `ast_cprop_escapes` (11 sites) and
  `ast_local_is_readonly` (1) now answer from it (originals kept as `_scan` oracles).
  Scope note: `ast_licm_written`/`ast_ivsr_count_writes` were left as-is — they are
  subtree-scoped (not whole-arena rescans), so the whole-function table does not
  subsume them; a descendant-indexed extension is future work.
- [x] **PR-3 — Step 2: property memos** — per-node tri-state memos (`ast_memo_*`) for
  `ast_ident_pure`/`ast_cprop_safe`/`ast_sccp_has_label`/`ast_cse_regpure`, cleared on
  epoch change.
- [x] **PR-4 — Step 3: structural hash** — lazy side-car hash (`ast_hash_*`) folding
  the exact `ast_ident_same` tuple; used as a collision-proof O(1) fast-reject, hash
  matches fall through to `ast_ident_same_scan` (confirm-on-fire). Design note: realized
  as an epoch-rebuilt side-car array, NOT the incremental parent-chain-patched SoA
  column — same correctness, and it sidesteps the invalidation risk that killed the
  earlier prototype. The incremental SoA `h[]` column stays available if a future
  hot path needs O(depth) updates instead of O(n) rebuilds.

**LANDED — Step 4: the strategy engine (now the sole pipeline).** Each fixed-pipeline
pass is wrapped as an `AstStrategy {name, gate, apply}` and the pipeline order is a frozen
data table (`ast_strategies[]`) consumed by `ast_func_end`. It is the **only** engine —
the legacy inline block and its `MCC_AST_ENGINE` toggle have been removed. The table
order/gates/args reproduced the legacy block byte-for-byte (verified before removal:
900/900 object comparisons across 300 generated programs × `-O1/-O2/-O3` were
byte-identical), so making it the sole engine changed no emitted bytes: the full ctest
stays 1904/1904 (incl. fixpoint-invariant + fuzz/matrix + the exact-byte goldens) and
shadow is 1904/1904 with zero side-car divergences. `match` = the gate; `est_cost_delta`
(the search's ranking key) is deferred to Step 5.

**LANDED — Step 5 core: the live -O4+ search.** At -O4+ (`optimize_search_seconds > 0`)
opt-in via `MCC_AST_SEARCH`, `ast_func_end` runs a per-function search over the four
toggleable fold gates (templates, narrow, bitflag, sethi) instead of the single frozen
order (`ast_search_select`). Execution model matches the intended runtime JIT: each
candidate gate config is a stackless coroutine whose one *tick* applies exactly one
strategy pass to its own isolated `ast_arena_clone`; the scheduler advances the
least-total-time-consumed live candidate (running sum of tick durations, ties → baseline
first — fair, no starvation), and a rolling 10-sample duration window makes the search
stop once the predicted next tick would overrun the remaining budget. The next-tick
prediction is a forecasting ensemble (`src/mccforecast.h`): thirteen self-contained
one-step predictors (random walk, SES, AR(1), linear/Bayesian-ridge regression, penalized
spline, GAM, local-level Kalman/BSTS, Gaussian-process, gradient-boosted stumps, Holt,
Theil-Sen, moving median) are scored on their online accuracy over the window; the three
most accurate vote and the one closest to their median (least distance to consensus) is
used. The same module is exposed to `-ffold-math` (constant-arg `mcc_fc_*` builtins fold
to a compile-time prediction) — one implementation, two consumers. The budget is the
`-ON` seconds, **absolute** from the first tick; `ast_search_abort` is a forced-abort
hook (pool/JIT/deadline) checked at every tick boundary — because each candidate mutates
only a throwaway clone, abort discards in-progress work safely (the pool's "kill+restart
worker" reduces to this discard). Candidates are scored by the static cost model
(`ast_cost_score`); the search only *selects* a gate config — the winner is emitted by
the normal unmodified pipeline+emit path on the untouched captured tree, so a search bug
(or a time-truncated / aborted search) can only pick a larger-but-correct config, never
a miscompile. Winners are memoized by `ast_intention_hash`. Single-threaded; -O4+ output
is timing-bounded and non-reproducible by design (quarantined there — `-O1..-O3` carry
no budget, never search, stay byte-reproducible). Opt-in: `MCC_AST_SEARCH=1` +
`-O<n≥4>` — a dedicated flag, kept off the default -O4+ path so it does not perturb the
out-of-process superopt's per-worker gate measurements. Validated: default `-O1..-O3`
unchanged (1904/1904 ctest, search never engages); `MCC_AST_SEARCH -O5/-O6` differential
correct vs gcc/clang (200/200 + 90/90 across scheduler revisions); shadow zero side-car
divergences; the `-ON` budget is an absolute cap (verified no hang, finishes early when
candidates complete); asttool 55/55 with the portable `clock()` timer.

**LANDED — Step 5+: emitted-byte-size scoring** (`MCC_AST_SEARCH_EMITSIZE`). Instead of
the static `ast_cost_score` proxy, each candidate is folded to completion and replayed
into the live text section (the proven in-place emit-and-rewind of the inline on/off
trial) with inline + promotion **off** (so the promotion save/restore desync never
arises); the emitted byte length is the score and every emit cursor is rewound. The
interleave-thrash problem (see below) is sidestepped by running this path
**run-to-completion** per candidate (the tick scheduler's fair interleave is kept for the
default static-cost path). Opt-in within the search; the winner is still emitted by the
normal pipeline on the untouched captured tree, so a mis-emit reverts through
`ast_func_end`'s faithful revert. Validated: `MCC_AST_SEARCH_EMITSIZE -O6` differential
135/135 correct vs gcc, zero crashes/miscompiles; default unaffected (1905/1905). Still
open: JIT-runtime scoring (`MCC_AST_JITSCORE`) and emit-size under the *tick* scheduler
(needs the per-context state below).

**LANDED — Step 5+: NCores-1 process pool** (`MCC_AST_SEARCH_THREADS`). `ast_search_pool`
forks up to `host_nproc()-1` score-only workers over the candidate set. A **fork gives
each worker its own copy of every optimizer global (COW)**, so a worker folds+scores its
candidates with zero shared-state contention and **no `_Thread_local` marking** — the
fork isolation replaces the whole per-context-state refactor for the scoring step.
Workers only read the shared `pristine` tree (COW) and write `{index, score}` records to
a pipe, then `_exit` without flushing; the parent (which never mutates shared state
during the fork window) collects them and applies the winner single-threaded. Handles
both static-cost and emit-size scoring. POSIX only (`#if MCC_HOST_POSIX`; elsewhere falls
back to serial). Validated: `MCC_AST_SEARCH_THREADS -O6` differential correct vs gcc
(16/16, incl. +emit-size), zero crashes/hangs; default unaffected (1905/1905); asttool
excludes it. Still open: the design's *C11-thread* pool with `_Thread_local` per-context
state (only needed for interior/tick-mode parallelism, not candidate scoring — the fork
pool covers that); the per-context refactor is the remaining substrate item if that is
ever wanted, its own gated change (side-car shadow + fixpoint + fuzz).
- [ ] **Step 5+ — widen the search space** — the fold-gate candidate set is the full
  **subset lattice** of the enabled gates (now on `combo_run`, see M1). **LANDED — opt-in
  enablement knobs (the search ADDS, not just drops).** The candidate space is now the
  subset lattice of `searchable = base | opt-in-knobs`, not just `subsets(base)`: knobs that
  are **off in every -O baseline** (so the subset-of-base lattice can never reach them) are
  offered to the search to turn ON. Two knobs land, both **gated on a gate reliably in `base`
  in BOTH superopt modes** (`base` is `narrow|sethi`=10 in search-mode workers, `15` under
  `MCC_AST_PERFN` — so only narrow/sethi-gated knobs fire; templates/bitflag-gated ones are
  shadowed by the superopt's own axes, the M1/M3 duplication in vivo). **The strategy bitmask is
  now `AstGateMask` = `uint64_t`** (was `unsigned` in memory but hard-capped at **8 bits** on disk
  via `gates & 0xff` + `MAGIC<<8`): it starts using only the low bits and scales to the host's max
  width — 64 knobs in memory, the low **48** persisted (`AST_GATE_BITS`, MAGIC moved to the high 16
  of the on-disk word), so an added knob past bit 7 is no longer silently truncated on
  persist/reload. Old 8-bit-format records fail the per-record MAGIC check and are skipped (cache
  rebuilds; already version/triplet-salted). Validated: ctest 1905/1905, 48-bit disk round-trip,
  exec 12/12. The two landed knobs:
  **Regression coverage (NEW — the whole search subsystem had zero ctest coverage before):**
  two exec-corpus ctest variants over all ~291 exec goldens, total ctest **1906 → 2488**, all pass.
  (i) `exec-narrowfix/*` forces both knobs ON at -O2 (`MCC_AST_NARROW_FIX=1 MCC_AST_SETHI_LEAF=1`),
  cheap, covering the V-narrow(a)/V-sethi(a) codegen. (ii) `exec-search/*` runs the live -O4 combo
  search (`MCC_AST_SEARCH=1 MCC_SEARCH_WORKER=1` — the worker flag runs the in-process search
  directly and suppresses the out-of-process superopt spawn, whose fork+exec argv-rewrite otherwise
  collides with the exec runner's `-o`; diagnosed via the correct sorted output proving no
  miscompile), covering M1 enumeration + the memo/5-word disk record + scoring + knobs-via-`searchable`.
  (iii) `exec-search-emitsize/*` and `exec-search-threads/*` cover the two other scoring paths —
  the emit-size scorer (`ast_search_emit_size`: fold-to-completion + in-place emit-and-rewind) and
  the NCores-1 fork pool (`ast_search_pool`) — across the whole corpus. (iv) Both knobs are
  registered in the differential miscompile fuzzer's `GATES[]` table (`tests/fuzz/runner.c` —
  `NARROW_FIX`/`SETHI_LEAF`), so `fuzz/matrix` now stress-tests their codegen against the gcc+clang
  consensus on random programs; a 110-seed campaign ran 0 miscompiles. **Total ctest now 3070**
  (was 1906 before this coverage) — the entire search subsystem is CI-covered end-to-end.
  (v) **Self-host validated:** mcc compiled its own ~100K-line source with all three knobs
  (`NARROW_FIX`+`SETHI_LEAF`+`SCCP_FIX`) forced on → clean object; linked (mcc as linker, its runtime
  supplies `__floatundixf`/`__fixxfdi`); the resulting knobs-on self-hosted mcc correctly compiles
  and runs `fib(10)=55` and the exec-corpus quicksort. Full M8 validation for the landed transforms:
  exec-corpus (1164 tests) + differential fuzz (~410 seeds, 0 miscompiles) + self-host.
  a) **`AST_SG_NARROWFIX`** (narrow iterate-to-fixpoint, `MCC_AST_NARROW_FIX`, gated on
  `AST_SG_NARROW`) — V-narrow(a); b) **`AST_SG_SETHILEAF`** (leaf-aware Sethi-Ullman
  register-need: a literal leaf needs 0 registers, `MCC_AST_SETHI_LEAF`, gated on
  `AST_SG_SETHI`) — V-sethi(a), correct-by-construction since sethi only reorders commutative
  operands. The memo-hit intersection was widened `& base → & searchable` so a cached winner
  keeps its opt-in knobs; the fork-pool `gatelist` grew 16→64 (2^6). Validated: default
  `-O1..-O3` byte-identical (**1905/1905**, asttool 130/0); `MCC_AST_SEARCH -O6` enumerates the
  full 2^4 lattice over `{narrow,sethi,narrowfix,sethileaf}` (TRACE: `searchable=58`, candidate
  masks 2..58 incl. the 16/32 knob bits) and stays exec-identical to baseline (14/14 + `w()`
  cascade). A tried droppable CPROPJOIN knob was removed — droppable knobs need the gate in
  `base`, which the superopt strips, so only the **addable** pattern reliably fires. This is
  the template for the remaining opt-in axes.
  **Budget-scaling the candidate count — LANDED:** `combo_run`'s `spec.budget` is now
  `AST_SEARCH_MAX_CAND` (128) instead of 0/unbounded, and the fork pool's `gatelist` is sized to it
  with an explicit (TRACE-logged, non-silent) cap on the submask enumeration — so the subset lattice
  of `searchable` can grow to 2^9 (4 fold gates + the 5 opt-in knobs) without pathological
  enumeration or `gatelist` overflow. This unblocked wiring `LTEMP`/`IVSR`/`PRE` into `searchable`
  (V-licm(d), above). Validated: default byte-identical (search-mode base=10 → 15 candidates, well
  under the cap; ctest 3070), perfn-mode 12/12 exec + self-host. **Best-first frontier (est_cost_delta seed) — LANDED:** when
  the vocabulary is large enough that the `AST_SEARCH_MAX_CAND` budget truncates the enumeration,
  base's single-toggle neighbours (drop one enabled gate / add one opt-in knob — the highest-value
  nearby configs) are scored explicitly *before* the general combo enumeration, so the cap can't
  crowd them out. **Data-driven ordering (est_cost_delta) — LANDED:** each frontier probe's measured
  marginal delta then REORDERS `items[]` (insertion sort, most-improving first), so combo_run's
  budget-capped ascending-mask enumeration spends its candidates on the most-promising gate
  combinations first — measured deltas are a stronger ordering signal than an `ast_fc_forecast`
  prediction would be, since the frontier already evaluates them directly. Scheduling-only
  (correct-by-construction, any order → correct winner) and gated on `(1<<nitems) > cap`, so
  small-vocabulary searches (default search-mode) are byte-identical. Validated: ctest 3070, perfn
  14/14 exec, self-host. **Catalog knobs wired to `AST_SG_*` bits — 7 opt-in knobs now
  search-selectable:** `NARROWFIX`(16), `SETHILEAF`(32), then templates-gated `LTEMP`(2048)/
  `IVSR`(4096)/`PRE`(8192)/`DSECALL`(16384)/`TCOPTR`(32768). perfn-mode `searchable=0xf83f` = the
  full 11-bit vocabulary (4 fold gates + 7 knobs), budget-capped at 128 so 2^11 doesn't explode
  (validated: default byte-identical ctest 3070, perfn 12/12 exec, self-host). Still open: the
  **inline/promote axes** (want emitted-size scoring, since inline/promote effects are emit-time),
  and the search-mode superopt shadowing (templates-gated knobs only fire in perfn mode → M3 wiring).
- [ ] **Step 5+ — disk-backed cross-build memo (refcounted, LFU-evicted, compressed)** —
  the per-function winner persists across builds as a **refcounted permutation**:
  `<cachedir>/mcc-search.memo`, records `{intention-hash, gates|MAGIC<<8, refcount}`,
  loaded into `ast_search_memo`; a hit applies `cached & base` (a winner cached under a
  different -O base never enables a gate this build disabled), **bumps the permutation's
  refcount**, and persists. The on-disk form is a **compressed whole-file container**
  ("MSZ1" magic + best-of-3 rle/lzss/lzw over the serialized record image; a rewrite
  replaces the old raw append-log, so a hit rewrites only when the working set changed).
  Every accessor (load / store / hit) checks the **shared cache-dir disk usage**; at
  **10 GiB** it evicts the lowest-refcount quarter of the working set and rewrites the file
  (temp + rename), keeping the most-reused permutations. `mcc --clear-cache` wipes it.
  Validated: cross-invocation refcount accumulation, -O6 differential correct, the eviction
  path (lowered-cap test: file grows then compacts, output preserved), and the compressed
  round-trip (real -O4 populate+reload). Still open (now tracked as **M2/M3** in the macro
  roadmap below): **unify with the out-of-process `pf-*.ck` format** (`so_pf_key`) so the
  in-process search fully subsumes `mcc_superopt_perfn`; salt the key with version/triplet;
  raise `AST_SEARCH_MEMO_CAP` if the 4096-entry hot set proves too small; throttle the
  per-accessor dir-walk if it shows up on very large caches.

**LANDED — compression codecs + the `mcccombo` substrate wrapper (not yet wired).**
`src/algorithms/{rle,lzss,lzw}.h` — header-only static-inline PackBits-RLE / LZSS / LZW,
round-trip selftest `src/algorithms/selftest.c` (18/18). `src/mcccombo.h` — a header-only
wrapper for the recurring "try permutations of formulaic combinations, score, keep the best,
memoize by key->value" pattern: `combo_run` (subset × ordering enumerator, `COMBO_MAX 16`,
`ComboScoreFn` lower-is-better), `combo_codecs`/`combo_pipeline_search` (codecs as a permutable
formula family — best codec *chain*), `ComboMemo` (key->value cache, values best-of-3 compressed,
refcount + LFU byte-cap eviction); selftest `src/algorithms/combo_selftest.c` (20/20). The disk
memo now stores a **compressed container** ("MSZ1" magic, best-of-3 rle/lzss/lzw — a real -O4 run
packs 216->111 bytes) in place of the raw 24-byte append-log. `mcccombo.h` is written but **used
only by its selftest** — the roadmap below wires the real call-sites onto it. Validated:
1905/1905, real -O4 populate+reload round-trip.

### Macro roadmap — collapse both searches + const-data onto one substrate

Grounded by two audits: (i) the out-of-process superopt duplicates **every** concern of the
in-process `ast_search` on a second substrate (process-spawn + ELF-file measurement vs.
arena-clone + cost model); (ii) the substrate target (`src/mcccombo.h`) and its four migration
call-sites already exist. Numbered macro steps; lettered sub-features inline; `(A)` nested
subgroups; each names the concrete hook and the double-checked blocker. Order is dependency order
(M4 before M6; M5 before M6).

- [x] **M1 — wire the live -O4 search onto `combo_run`** (behavior-preserving first). **LANDED
  (subset mode, byte-identical).** `ast_search_select` now drives the serial static-cost and
  emit-size searches through `combo_run` (`src/mcccombo.h`): `items[]` = the baseline-enabled
  `AST_SG_*` bits, the `ComboScoreFn` (`ast_search_combo_score`) wraps `ast_search_score_one`
  (owning the `ast_cur` + 4-gate global save/restore that keeps `combo_run` a pure enumerator),
  budget/abort enforced by `ast_search_should_stop` → `COMBO_REJECT`. base is scored first and
  wins ties (safe-fallback); the empty (all-off) config competes too, so the candidate SET equals
  the old submask lattice `{base .. 0}`. The fork pool keeps its own submask `gatelist[]`.
  Validated: **byte-identical to HEAD** across default `-O2`, `MCC_AST_SEARCH`, `+EMITSIZE`,
  `+THREADS` (diff of every emitted `.o`); full ctest **1905/1905**; asttool 130/0.
  c) **ordered enumeration** (`MCC_AST_SEARCH_ORDERED`) is plumbed to `combo_run`'s permutation
  mode but is a **no-op over the current 4-gate vocabulary** — gates combine as a bitmask
  (`gates |= items[sel[i]]`) so orderings collapse to the same mask/score (verified byte-identical
  to subset mode). Making it meaningful (strategy *pipelines*) needs (i) the 12-entry
  `ast_strategies[]` as the combo vocabulary with order-respecting application, AND (ii) the
  **emit path** (`ast_func_end`'s frozen `ast_strategies[]` loop) to honor the discovered per-fn
  order — otherwise the winning order is selected but never applied. That emit-path change is the
  remaining M1(c) work (tracked in the variation catalog below). *Synergy still open:*
  `ast_fc_forecast` best-first candidate ordering.

  **LANDED (on top of M1) — hit-count tie-break (`ast_search_pack_score`).** Each candidate now
  sums the fold-count every applied strategy reports (`apply()` returns a hit count) and packs it
  into the score: `(primary << 12) + (HITMAX - clamp(hits))`, so ranking is cost-first and, WITHIN
  an equal cost, favors the config whose algorithms actually **fired more on this AST slice** (a
  gate that lands many folds beats one that does nothing; base still wins its own ties as the
  fallback). Applies to both static-cost and emit-size scoring and the fork pool. Deliberately
  changes -O4+ winner selection (quarantined non-reproducible tier); default `-O1..-O3` untouched
  (**1905/1905**), `MCC_AST_SEARCH -O6` exec-identical to gcc. Verified via `-v128` TRACE: on a
  slice where narrow lands 0 folds and sethi lands 1 at equal cost, sethi now outranks narrow.

- [ ] **M2 — unify the memo on `ComboMemo` + disk backing.** a) key = `ast_intention_hash`
  (`mccast.c:435`, already the memo key, stable across builds); b) value = winner record (gates +
  score/size) stored best-of-3 compressed (the "MSZ1" container logic moves into `ComboMemo`);
  c) refcount + LFU eviction under the shared 10 GiB cap (already present). **(A)** extend
  `ComboMemo` with disk load/store. **(B) Blocker — LANDED:** the version/triplet salt is now
  applied. `ast_search_key_salt` (mccast.c, FNV over `MCC_VERSION_STR` + `MCC_CONFIG_TRIPLET`,
  mirroring `so_pf_key`, `#ifdef`-guarded so the standalone asttool TU still builds) salts the
  search-memo key at computation in `ast_search_select`, partitioning the shared cache dir so a
  winner cached by an incompatible build/target is never silently reused. Validated: ctest
  1905/1905, asttool 130/0, cross-invocation cache-hit still preserved (run 1 searches, run 2 is a
  full memo hit with identical output). Still open: the `ComboMemo`-struct migration (a)+(c) — the
  current disk memo is the hand-rolled `AstSearchMemo`/MSZ1 path, not yet the `ComboMemo` type.
  *Synergy:* the shadow oracle `MCC_CONFIG_AST_SHADOW` (`mccast.c:2485`) validates a
  cache hit == recompute, reusing the per-node memo's existing shadow-assert harness.

- [ ] **M3 — subsume the out-of-process superopt** (`mcc_superopt_perfn`/`mcc_superopt_search`,
  `mcc.c:922/1053`) onto the substrate. a) map perfn `{1,3,7}` config bits and the search 3-axis
  int product into the `sel[]`/gate vocabulary; b) fold `pf-*.ck`/`so-*.ck` (`mcc.c:817-896`) into
  the compressed container; c) reconcile concurrency — per-key `flock` + claim-cursor work-stealing
  (`so_claim`, `mcc.c:447`) vs the memo's whole-file rewrite. **(A) Blocker — LANDED (record
  fields):** `AstSearchMemo` + the on-disk record now carry `int64 score` (the winning config's
  search score, populated from the live `best_score`) and `uint64 tried` (a measured-config bitmask
  — one bit per candidate the combo scorer actually measured before the budget ran out, so a
  truncated search records partial progress; `memo_add` accumulates it across builds) — the fields
  `SoPfCkpt` keeps (`best_size`/`tried`) so one record serves both searches. The disk record grew
  3→5 u64 words (`AST_MEMO_RECWORDS`); old 3-word records fail the per-record MAGIC check and are
  skipped (cache rebuilds; version/triplet-salted). Validated: ctest 1906/1906, 5-word round-trip
  (280B → 7 entries = 40B/rec), score + tried persist across runs (`-v128` winner trace shows
  `tried=7fff` = all 15 of the 2^4 lattice measured). Remaining: expose `tried` in the superopt's
  own config-index ordering (needs the unified search to define that ordering). **(B) Blocker — LANDED (vocabulary bridge):** the lossless config↔gate mapping is
  defined in **`src/mccgate.h`** (`ast_gate_from_so`/`ast_gate_to_so` for the superopt-search 4-bit
  gate, `ast_gate_from_perfn`/`ast_gate_to_perfn` for the perfn `{1,3,7}` cfg) with the two
  vocabularies laid out on one 64-bit `AstGateMask` — fold gates + knobs in the low bits, the
  superopt-only axes (`AST_SG_{PROMOTE,INLINE,NOCALLFUL,CPROPJOIN,CSEJOIN}`) in bits 6.. so they
  never collide. Selftested by `tools/asttool.c` `suite_gatemap` (483 round-trip checks: full
  16-value so-gate space both directions, the 8 perfn cfgs, exact bit correspondences, disjointness)
  — asttool 613/0. Header-only + dependency-free so the harness tests it without the `MCC_INTERNAL`
  search body (the `mcccombo.h` "selftested before wired" pattern); NOT yet wired into a unified
  search — that wiring + the `budget` int-axes (node/graft/bitflag levels, which carry no gate bit)
  are the remaining M3 work. *Synergy:* the fork score-only pool (`ast_search_pool`) replaces
  fork+exec child compilers for scoring; one cache dir, one eviction, one key scheme retires the
  second substrate.

- [ ] **M4 — extend scoring to data/rodata** (prerequisite for const-data compression to *compete*).
  a) snapshot `data_section->data_offset` + `rodata_section->data_offset` before replay and diff
  after — mirrors the reloc-cursor save/restore `ast_search_emit_size` already does (`mccast.c:7247`);
  b) combined score = text delta + data/rodata delta; c) add a data-size term to `ast_cost_score`
  (`mccast.c:5497`, today text/AST only). **(A) Audited fact:** emit-size scoring is **text-delta
  only** — a data-shrinking transform currently scores as a net regression, so without M4 datacomp
  can never be chosen. *Synergy:* makes M6 visible to the search; no separate scoring path.

- [ ] **M5 — const-data emission foundation** (the existing `.rodata data-emission` item above).
  Add an `AstKind` array/global/static-data node + a table-symbol+initializer emitter wired into the
  replay/rewrite lifecycle. **(A) Audited blocker:** const data is `memcpy`'d into `Section->data` at
  **parse time, outside the AST capture window** (`init_putv`, `mccgen.c`) — this step is what brings
  it under the substrate so a pass can rewrite it. **First step LANDED — const-data visibility
  side-car (byte-neutral):** `ast_hook_data(sec, off, size, is_ro)` (`mccast.c`, mirrors the def/use
  `ast_du_*` side-car) records one entry per initialized static/global object, hooked read-only at
  `decl_initializer_alloc`'s `put_extern_sym`/`vpush_ref` (`mccgen.c`, `#if MCC_CONFIG_OPTIMIZER`),
  keeping running .rodata/.data byte totals. Const data is now *visible* to the substrate (queryable:
  `MCC_AST_DATA_REPORT` dumps per-object `{section, offset, size}`, or `-v128` TRACE `ast_hook_data`)
  — the prerequisite for M4 data-size scoring + M6 datacomp. Verified: `static const int[8]`→rodata
  32B, string literal→rodata 13B, `static int[4]`→data 16B, `const char *`→data 8B. Validated:
  byte-neutral (ctest 3070, changes no emitted bytes), self-host, i386 cross-arch exec 292/292.
  **Second step LANDED — the hook now fires AFTER `decl_initializer` writes the bytes** (moved from
  `put_extern_sym` to after the `decl_initializer(&p,...)` write in `decl_initializer_alloc`), so the
  side-car can read each object's emitted bytes and feed the M6 candidate-ID estimator below.
  **Remaining (non-neutral):** the `AstKind` data node + a re-emit pass so a pass can *rewrite* the
  bytes (not just observe them). *Synergy:* also unblocks §30 value-table dispatch.

- [ ] **M6 — datacomp: const-data compression pass** (codegen-layer, opt-in; **not** an AST
  strategy — audited infeasible as one: data absent from the AST, text-only score, needs a
  synthesized ctor). **(A) Target:** a) string literals ・ b) `static const` arrays ・ c) both;
  threshold by size×entropy. **(B) Codec:** per-blob best via `combo_pack`, or `combo_pipeline_search`
  for a chain. **(C) Decompression:** a) eager `.init_array` ctor (`add_array`) ・ b) lazy first-use
  guard ・ c) both. **(D) Runtime:** new `__mcc_decompress` in `runtime/`, call emitted via
  `vpush_helper_func`+`gfunc_call`. **Blockers (audited):** breaks link-time-constant consumers;
  `const`→writable `.bss`; multi-backend ctor synthesis (x86_64/arm64/riscv64/i386/arm32). **Gate:**
  off by default; fires only when M4 scoring says it net-shrinks.
  **Candidate-ID step LANDED (byte-neutral, rides the M5 visibility side-car):** `ast_data_estimate`
  (`mccast.c`) runs `combo_pipeline_search` (mcccombo.h — the SAME `combo_run` permutation engine the
  -O4 gate search rides) over each just-emitted const object's bytes to find the best codec CHAIN
  (depth ≤3 over RLE/LZSS/LZW), flags objects that shrink >50% as M6 datacomp candidates, and
  accumulates an estimated-bytes-saved total plus the winning chain (the exact recipe (B)+(C) would
  emit). A chain beats any single codec, so the estimate is tighter than a best-of-3 `combo_pack`. This
  is analysis (B) "which blobs are worth compressing, with what chain" without yet performing (C)/(D).
  Read-only (size 32..8192 window, ping-pong-buffer bounded); changes no emitted bytes. **Round-trip
  correctness gate LANDED:** before a chain is counted, `ast_data_roundtrips` (`mccast.c`) runs
  `combo_pipe_apply`→`combo_pipe_unapply` and `memcmp`s against the source — a candidate is only
  accepted if compress→decompress is bit-exact (a datacomp rewrite trusting a lossy chain would silently
  miscompile). Doubles as a decoder bug-hunt: every const object in every compiled TU exercises the
  chosen `dec()` path against a known original. Verified: `const int[256]={0}`→`rle+lzss` 1024→5B flagged
  (vs 16B single-codec), 53×`'a'`→`rle` 4B, `{1,2,3,4}`/varied text NOT flagged; src/mcc.c → 3 candidates,
  0 lossy; ctest 3070 byte-identical, decoders run corpus-wide with no crash/mismatch. `MCC_AST_DATA_REPORT`
  prints `^ compressible A->B chain=rle+lzss ... round-trip OK`; `-v128` TRACE `data pack ... k=N` /
  `LOSSY chain`. **Remaining:** the actual (C) ctor + (D) runtime, which need M5's non-neutral rewrite step.

- [ ] **M6z — zero-init `.bss` placement** (a cheaper cousin of M6, surfaced by the M5 side-car; NOT
  compression — a free placement fix). **Finding:** mcc emits an all-zero-initialized writable static
  object (`static int x[256] = {0};`) into `.data` as `size` zero bytes on disk, when C11 6.7.9 makes
  `={0}` identical to no initializer → it belongs in `.bss` (NOBITS, zero disk bytes). `size -A`
  confirmed a 256-int `={0}` costs 1024 `.data` bytes under mcc (gcc puts it in `.bss`). The section is
  chosen in `decl_initializer_alloc` (`mccgen.c:12108`, `has_init → data_section`) **before** the
  initializer bytes exist, so the fix needs either an all-zero pre-scan of the initializer or
  post-emission section move (symbol re-bind + `.data` reclaim) — a non-neutral, cross-arch change.
  **Analysis step LANDED (byte-neutral):** `ast_data_zero_check` (`mccast.c`, on the visibility
  side-car) flags each all-zero writable object as `.bss`-movable and accumulates the reclaimable disk
  (`MCC_AST_DATA_REPORT` prints `^ all-zero .data ... belongs in .bss`; `-v128` TRACE `data zero`).
  Verified: `int[256]={0}`→flagged 1024B reclaimable, `int[256]={1}` correctly NOT flagged; src/mcc.c →
  0 (already clean); ctest 3070 byte-identical. `.rodata` all-zero excluded (const-placement is separate).
  **REWRITE LANDED (non-neutral, opt-in `MCC_ZERO_BSS`, validated to the full M8 bar).** Approach B
  (post-emission move) in `decl_initializer_alloc` (`mccgen.c`, right after `decl_initializer`):
  truncate `data_section->data_offset` back to the object's `addr`, re-allocate it in `bss_section`, and
  re-bind the symbol with `put_extern_sym(sym, bss_section, new_addr, size)`. Relocations are
  symbol-keyed (`greloca` stores `sym->c`), so re-homing the symbol fixes every reference automatically.
  Guarded to a provably-safe subset — `v && sym`, `sec==data_section`, `!bcheck && !asan_g &&
  !flexible_array && !TLS`, **last-allocation** (`addr+size == data_section->data_offset`, auto-excludes
  bcheck byte / asan redzone), and **initializer emitted no relocation** (`data_section->reloc->data_offset`
  unchanged across `decl_initializer`) — the last guard is the critical one: a pointer init (`={0,&g}`)
  leaves zero *bytes* but carries a reloc, and moving it to NOBITS would drop the pointer. Correctness is
  C11 6.7.9 (`={0}` ≡ no initializer). **Validated:** default byte-neutral (ctest 3070, gated off); with
  `MCC_ZERO_BSS=1` — exec 292/292 (x86_64), cross-arch exec i386 292/292 + arm64 292/292, self-host
  (mcc self-compiles + correct executables), differential fuzz vs gcc/clang 7/7; the pointer-init repro
  (`static int *pz[4]={0,0,0,&g}`) keeps `*pz[3]==42` (reloc preserved, object correctly NOT moved).
  Regression-locked by the `exec-zerobss/` ctest variant (CMakeLists, forces the move on across the corpus).
  `-v128` TRACE `zero-bss move v=.. size=.. data@.. -> bss@..`. **Remaining (deferred):** TLS `tdata`→`tbss`
  and the asan/bcheck cases (excluded by guards for now); flip to default-on after broader field exposure.

- [ ] **M6s — string-literal merging** (`-fmerge-constants`-style rodata pooling; opt-in `MCC_MERGE_STRINGS`).
  **LANDED, validated to the full M8 bar.** C11 6.4.5p7 leaves identical string literals' distinctness
  unspecified, so sharing storage is sound (unlike const *array* dedup, which C11 6.5.9 forbids — distinct
  named objects must have distinct addresses). mcc previously emitted a fresh rodata copy per literal
  (confirmed: two `const char*="dup"` gave two copies). A value-use string literal (`str_init`,
  `mccgen.c:8593`, `v==0`) is homed at an anonymous symbol via `vpush_ref`→`get_sym_ref`, and references
  are symbol-keyed (`greloca(sec, vtop->sym,…)`, `mccgen.c:11518`) — so merging is the SAME symbol-rebind
  as M6z, no reloc rewriting. In `decl_initializer_alloc` after `decl_initializer`: content-key the just-
  written bytes (`ast_strpool_find_or_add`, `mccast.c` — hash + exact memcmp, keyed on bytes+size+align so
  wide never aliases narrow; per-TU, reset in `ast_configure`); on a hit, **zero the reclaimed slot**,
  truncate `rodata_section->data_offset` to `addr`, and re-home the symbol to the shared offset. Guards:
  `v==0 && sym`, `sec==rodata_section`, last-allocation, `size>0`. `char arr[]="dup"` (copied into the
  object) is naturally excluded (`v!=0`). **Bug caught+fixed by the regimen:** truncating left the dup's
  bytes in the buffer, and the next literal's `decl_initializer` memcpys only content and relies on a
  pre-zeroed trailing NUL → a format string lost its NUL and read a stray `c` from the reclaimed slot; the
  `memset(rodata+addr, 0, size)` on truncation restores the "space at/after data_offset is always zero"
  invariant (M6z didn't need it — its reclaimed bytes were already zero). **Validated:** default byte-
  neutral (ctest 3361, gated off); `MCC_MERGE_STRINGS=1` — exec 292/292 (x86_64) + i386 292/292 + arm64
  292/292, self-host OK, differential fuzz vs gcc/clang 7/7; rodata 141→77B on the repro. Regression-locked
  by `exec-mergestrings/`. `-v128` TRACE `string merge`/`strpool exact`. **Suffix/tail sharing ALSO LANDED**
  (clang shares `"bar"` inside `"foobar"`): NO reloc-addend surgery needed after all — since references are
  `symbol + addend`, re-homing the literal's symbol to `A_addr + (A_size − size)` makes every reference
  resolve into the interior of the longer literal `A`; C11 6.5.2.5 footnote permits sharing *overlapping*
  representations. `ast_strpool_find_or_add` now matches this literal against the last `size` bytes of each
  pooled entry (guarded by `interior_off % align == 0` so a reused slot always meets alignment; exact match
  is the `A_size==size` case). Verified: `"foobar"`/`"bar"`/`"obar"` fold to one `foobar\0` (rodata 37→28B),
  output correct; revalidated full M8 (default byte-neutral 3652; `MCC_MERGE_STRINGS=1` exec 583 x86_64 +
  i386 292 + arm64 292, self-host OK, fuzz 7/7). `-v128` TRACE `strpool suffix`. **Remaining:** default-on.

- [ ] **M7 — formula-family unification** (the long tail). a) expose cost/ratio formulas as
  fold-math builtins (`mcc_cost_*`/`mcc_ratio_*`, copy-pasting the `foldfc_try` template,
  `mccgen.c:8402`); b) make the forecast ensemble a first-class `combo` formula family (pick
  codec/strategy/threshold, not just next-tick duration); c) one `-f` front — extend `fold-math`
  (`s->fold_math`, `libmcc.c:1902`), which **already** unifies libm + forecast folding, or add a new
  gate. *Synergy:* one enumerator over {strategies, predictors, codecs}; one flag family; the
  four `host_cache_dir` caches collapse to one `ComboMemo` store.

- [ ] **M8 — validation gates** (apply to *each* of M1–M7 as it lands). a) full ctest 1905/1905;
  b) `-O6` differentials vs gcc/clang; c) self-host 3-stage fixpoint; d) sanitizers (UBSan/ASan);
  e) cross-arch (i386/arm32/riscv64/arm64, qemu-docker); f) differential miscompile fuzz;
  g) `MCC_CONFIG_AST_SHADOW` zero-divergence. Behavior-preserving steps (M1 subset-mode, M2, M3)
  must stay byte-identical; M4–M7 are gated opt-in and may change emitted bytes only under their flag.

### Strategy-variation catalog — one algorithm-variant per pass today; widen the search vocabulary

Audit result: of the 12 `ast_strategies[]` passes, **9 implement a single algorithmic variation**
(bfold, ident, narrow, licm, dse, sccp, jt, sethi, tco) and 3 already carry one extra axis
(`cprop`/`cse` a per-block-vs-join env toggle, `bf` a `ast_bitflag_min` threshold). Every `apply`
already returns a hit count (now consumed by `ast_search_pack_score`). Each variation below is a
candidate **search knob** — a distinct `AstStrategy` row or a per-strategy parameter the M1
`combo_run` vocabulary enumerates over. The M1(c) precondition applies to any *ordering* or
*pipeline* variant: the emit path must honor the discovered per-fn order, not just the frozen table.

- [ ] **V-bfold** (`ast_bfold_run`, table `ast_bfold_tab`, 8 ops) — a) extend the table:
  `pow/exp/log/sin/cos/round/nearbyint/fmod/hypot/ldexp`; b) `fma` contraction (`a*b+c` all-literal);
  c) partial folds (`fmin(x,+inf)`, `copysign` with one literal arg — today all `nargs` must be
  literal); d) `FLT_ROUNDS`/errno-safe gate for `-frounding-math`.
- [ ] **V-ident** (`ast_ident_rec`, DFS post-order, iterated to fixpoint, integer-only) —
  a) strength reduction (`x*2^k → x<<k`) is **backend-redundant** (mcc already emits `shl` for
  `x*8`) — skip; b) fast-math-gated float identities (drop the `ast_ident_intt` block);
  **c) LANDED — comparison identities:** `x OP x` folds to a compile-time 0/1 for all ten relational
  ops (`== <= >=` → 1; `< > !=` → 0; signed + unsigned), for pure integer `x` (int-only gate → the
  float NaN idiom `x==x` is untouched). Result typed `VT_INT` (a relational node stores `type_t==0`;
  the C result is `int` — the bug that first made the fold a silent no-op, caught with `-v128` TRACE
  of `nodetype=0 ct=3`). **Default-on** (not backend-done: mcc otherwise emits `cmp;sete`), so it
  improves every -O1+ compile. Validated to the M8 bar: ctest 3070 (no golden regressions), 10-form
  exec check, differential fuzz (60 seeds 0 miscompiles — comparisons well-stressed by gen.h), and
  self-host. **c-extra) LANDED — unsigned range vs 0:** for any unsigned `u`, `u >= 0` / `0 <= u`
  fold to 1 and `u < 0` / `0 > u` to 0 (the discarded operand must be pure; signed `x >= 0` is left
  alone). The relational op is the SIGNED token `TOK_GE/LT/LE/GT` — unsignedness lives in the operand
  type (`tx & VT_UNSIGNED`), which `-v128` TRACE (`cmp2 op=157 tx=51(uns=1)`) caught after a first
  attempt wrongly keyed on `TOK_UGE` (never emitted at the AST level). Default-on, validated: ctest
  3070, exec, fuzz (80 seeds 0 miscompiles), self-host. d) a worklist/BFS ordering variant.
- [ ] **V-narrow** (`ast_narrow_rec`, single post-order pass, NOT iterated) — **a) LANDED:**
  `ast_narrow_run` iterates to a `do/while` fixpoint under `MCC_AST_NARROW_FIX` (default off →
  single pass, byte-identical), wired into the search as the opt-in `AST_SG_NARROWFIX` knob (see
  "widen the search space" above). b) replace the type-width heuristic (`ast_ii_width`) with a
  demanded-bits/known-bits analysis; c) extend `ast_narrow_binop` past `+ - * & | ^` to shifts and
  comparisons (mirrors §29 non-distributive item).
- [ ] **V-cprop** (`ast_cprop_run`, join-vs-per-block already env-gated) — a) promote the
  join/per-block choice from `MCC_AST_CPROP_JOIN` env to a first-class strategy pair; b) copy
  propagation (`local == local`, not just constants); c) known-bits/range lattice variant;
  **d) LANDED** — `AST_CPROP_MAX` split into an array cap (512) + runtime `ast_cprop_window`
  (`MCC_AST_CPROP_WINDOW`, default 128 → byte-identical), raisable so constant-tracking state is not
  silently dropped in large functions. Correct-by-construction (sound cprop; more tracking → more
  folds). Validated: ctest 3070, exec-corpus, fuzz `GATES[]` `CPROP_WINDOW` (60 seeds 0 miscompiles),
  self-host, i386 cross-arch (292/292).
- [ ] **V-cse** (`ast_cse_run`, O(n²) structural `ast_ident_same` match, cap 64) — a) hash-based
  value-numbering (LVN/GVN) alternative; **b) LANDED (commutative-aware match)** — `ast_cse_same`
  (`MCC_AST_CSE_COMM`, default off → exact match only, byte-identical; `AST_SG_CSECOMM` search knob)
  also matches `a OP b` against `b OP a` for commutative `+ * & | ^`, so the two share one cached
  result — correct-by-construction (commutative ops yield equal values incl. IEEE add/mul). Top-level
  pair only; deeper structure still exact. Validated to the **full M8 bar**: ctest 3070, exec-corpus
  291, fuzz `GATES[]` `CSE_COMM` (100 seeds 0 miscompiles), self-host, cross-arch i386/arm64/riscv64
  (292/292 each); verified it fires (a `csec.c` case drops 4→3 adds). c) redundant-load elimination as
  a distinct availability class (needs the §29 lattice — comparison-result cast elision looked cheap
  but the `movzbl` is normal materialization, not a redundant narrow); **d) LANDED (window cap knob)**
  — `ast_cse_window` (`MCC_AST_CSE_WINDOW`, default 64 → byte-identical) is the runtime availability
  cap, raisable to `AST_CSE_MAX`=256 to catch more common subexpressions in large functions.
  Correct-by-construction (sound CSE; more tracking → more folds, never unsound). Validated: ctest
  3070, exec-corpus, fuzz `GATES[]` `CSE_WINDOW` (60 seeds 0 miscompiles), self-host, i386 cross-arch.
  (join toggle is already `MCC_AST_CSE_JOIN`.)
- [ ] **V-licm** (hoisting inside `ast_cse_run` via `ast_licm_at_loop`; only exprs already in the
  CSE list) — a) discover loop-invariant subexprs directly (not gated on CSE presence); b) iterate
  to fixpoint + hoist to the outermost invariant level (nested loops); c) preheader creation +
  hoist invariant loads/stores; **d) PARTIAL — promoted `ast_ltemp_run`/`ast_ivsr_run`/`ast_pre_run`
  into the unified gate vocabulary:** `AST_SG_LTEMP`/`AST_SG_IVSR`/`AST_SG_PRE` (bits 11-13) wired
  into `ast_search_gates_now`/`_set` so the three CSE-embedded helpers are tracked/persisted/
  restorable gate state (default off → byte-identical, ctest 3070). Validated: exec-corpus
  (`exec-narrowfix/*` now forces all six knobs on across ~291 programs) + self-host (mcc self-compiles
  with them on → working compiler) — they previously had only differential-fuzz coverage.
  **d) NOW FULL — the search proactively enables them:** `searchable` adds `LTEMP|IVSR|PRE` when
  templates is in base, so the -O4 search explores them (verified perfn-mode `searchable=0x383f` =
  the 9-bit vocabulary; exec 12/12 vs baseline; self-host with all six on → working compiler). This
  was unblocked by **budget-scaling** (below). **Knob-boundary caveat:** `licm` folds are counted
  inside `cse`, so toggling `cse` off zeroes `ast_licm_folds`.
- [ ] **V-dse** (`ast_dse_block`, resets on any non-Store stmt → strictly intra-block) —
  **d) LANDED — see through bare calls** (`MCC_AST_DSE_CALL`, default off → full reset, byte-
  identical): instead of the conservative full reset on a non-store statement, an `AST_Invoke` (a
  bare call) only `kill_reads` the locals it reads via its args and keeps the rest of the dead-store
  tracking — so `x=1; foo(); x=2` drops the dead `x=1`. **Correct by DSE's existing escape analysis:**
  it tracks only NON-ESCAPING locals, whose address never escaped, so no call (via arg, global ptr,
  or otherwise) can *write* them — sidestepping the control-flow/asm/aliasing trap that makes general
  DSE-through-statements risky (only `AST_Invoke` is seen through; If/loops/asm still reset). This is
  a genuine **non-monotonic dataflow** transform validated to the **full M8 bar**: ctest 3070 (default
  byte-identical), exec-corpus 291 (forced on), differential fuzz `GATES[]` `DSE_CALL` (100-seed run 0
  miscompiles), self-host (mcc's call-heavy source), and **cross-arch i386/arm64/riscv64 (292/292
  each under qemu)**. a) global backward-liveness across blocks (subsumes the A1 item); b) partial-
  dead-store (dead on some paths); c) track stores across `AST_If`/loop children instead of bailing.
- [ ] **V-sccp** (`ast_sccp_run` — misnamed: only folds `AST_If` with an already-constant cond, no
  lattice) — a) implement **true** sparse-conditional constant propagation (constant lattice +
  CFG-edge worklist pruning unreachable edges); b) switch/computed-branch folding; **c) LANDED —
  fuse cprop+sccp into one fixpoint:** `ast_sccp_run` now loops `{ ast_cprop_run; ast_sccp_scan }`
  under `MCC_AST_SCCP_FIX` (default off → single scan, byte-identical) so a folded constant branch
  exposes constants that cprop propagates and sccp re-folds. Correct-by-construction and terminating
  — cprop only *adds* constants, sccp only *removes* dead branches (both monotonic, neither reverts
  the other → converges, bounded by node count). Validated: default byte-identical (ctest 3070);
  forced on across the whole exec corpus (`exec-narrowfix/*`) + differential fuzz `GATES[]`
  (`SCCP_FIX`, 100-seed campaign 0 miscompiles) + exec 14/14 vs default. Not yet an `AST_SG` search
  bit (env-forced + fuzz/exec-validated first, the narrowfix pattern; sccp is templates-gated, so
  search-mode workers would shadow it — perfn mode would exercise it).
- [ ] **V-jt** (`ast_jt_run` — not real threading: only empty-both-arms or identical-arms) —
  a) real jump threading through a predecessor that determines a later identical condition;
  b) duplicate-condition threading across straight-line blocks (`ast_ident_pure` proves re-eval
  safety); c) correlated-condition threading (`if(x>0)…if(x>=1)…`); d) hammock merge.
- [ ] **V-bf** (`ast_bf_run`, 4 flat scans, window ≤63) — **a) LANDED — range predicates `lo<=x &&
  x<=hi` → `(unsigned)(x-lo) <= (hi-lo)`** (`ast_range_run`, `MCC_AST_RANGE`, `AST_STRAT_RANGE`, default
  off → byte-identical). A new sibling pass modeled on `ast_bf_run`, reusing its builders
  (`ast_bf_keyexpr` = `(unsigned)x - base`, `ast_bf_lit`) and its purity/const readers. Matches a
  `TOK_LAND` `AST_Binary` (the `&&`-as-condition form; value-context `&&` desyncs the AST so isn't
  touched) whose two children are **signed** relationals (`TOK_LE/GE/LT/GT`, literal on either side,
  operand-mirroring handled) over the **same pure integer key** with one lower + one upper inclusive
  bound and `lo<=hi`. Correct-by-construction (standard unsigned-subtract range identity; `<`/`>`
  normalized to inclusive, INT64_MIN/MAX half-ranges rejected to avoid overflow; unsigned keys skipped
  since they use `TOK_ULE`). Closes a real gap: mcc emitted two signed compares + branches, gcc emits
  branchless `sub; cmp; setbe` — now mcc matches when on. Verified codegen (`sub $0xa; cmp $0xa`),
  reversed-operand (`100>=x && 50<=x`) + negative-lo (`x>=-5`) fold correctly, OFF==ON output. **The
  complementary out-of-range form ALSO landed** (`ast_range_try_lor`/`ast_range_bound_or`): `x < lo ||
  x > hi` (a `TOK_LOR` of two relationals — every bounds check `if (x<0 || x>=n)…`) → `(unsigned)(x-lo)
  > (hi-lo)` (`TOK_UGT`), the exact complement identity; each operand normalized to the KEPT-range bound
  (`x<C`→lo=C, `x<=C`→lo=C+1, `x>C`→hi=C, `x>=C`→hi=C-1, INT64 extremes rejected). Verified `x<10||x>20`,
  reversed `x>20||x<10`, and `c<'0'||c>'9'` fold to branchless `sub;cmp;ja`, OFF==ON. Both forms share
  the `MCC_AST_RANGE` knob + `AST_SG_RANGE` search bit. Validated to the **full M8 bar**: default
  byte-neutral (ctest 3652), `MCC_AST_RANGE=1` exec 292 (x86_64) + i386 292 + arm64 292, self-host OK,
  differential fuzz vs gcc/clang (`GATES[]` `RANGE` + env-forced). `-v128`
  TRACE `range fold key=.. lo=.. hi=.. span=..`. **Also wired into the -O4 search vocabulary** —
  `AST_SG_RANGE` (bit 17), reflected in `ast_search_gates_now`/`_set` and added to `searchable`
  unconditionally (standalone pass, no base-gate dependency), so the -O4 search can enable it per
  function; validated exec-search/-emitsize/-threads 874/874, asttool 57/57. b) windows >64 via
  multi-word masks; c) `switch` → jump-table/bitmask
  sibling transform (ties into §30 + the `switch`-arm item); d) perfect-hash lowering for sparse sets.
- [ ] **V-sethi** (`ast_sethi_run` — top-level commutative operand swap only, naive balance count)
  — a) **LANDED (partial):** leaf-aware register-need metric — `ast_sethi_num` returns 0 for an
  `AST_Literal` leaf (immediate, 0 registers) vs 1 for a ref, under `MCC_AST_SETHI_LEAF`, wired as
  the `AST_SG_SETHILEAF` search knob (default off → every leaf counts 1, byte-identical). Still
  open: extend the leaf metric to memory-vs-register refs (not just literals). b) full Sethi-Ullman
  labeling + evaluation schedule (not a single swap); c) reassociation to rebalance associative
  chains for register pressure (coordinate with V-cse canonicalization; §35 n-ary ordering);
  d) deterministic tie-break when `l == r`.
- [ ] **V-tco** (`ast_tco_run` — self-recursion only, int params ≤16, bails on param cycle) —
  **param-count cap LANDED as a knob:** `AST_TCO_MAXP` split into an array cap (64) + runtime
  `ast_tco_maxp` (`MCC_AST_TCO_MAXP`, default 16 → byte-identical), so functions with >16 params are
  now TCO-able. Real value: an 18-param 500k-deep tail recursion **stack-overflows** at default but
  TCO's to a loop (correct output) at maxp=32. Validated: ctest 3070, that exec test, fuzz `GATES[]`
  `TCO_MAXP` (60 seeds 0 miscompiles), self-host (maxp=32).
  a) break param cycles via temporaries instead of `if (cyc) continue`; b) general/sibling tail
  calls via a tail-call ABI; **c) LANDED (pointer params)** — `MCC_AST_TCO_PTR` (default off →
  int-only, byte-identical) also accepts `VT_PTR` params: the param store/reload is already
  type-generic (`ast_set_type` with the captured `ptt`/`pref`), so a pointer stores/reloads exactly
  like an integer of the same width; arrays/VLAs/volatile still excluded. Common in list/tree walkers
  (`sumlist(p+1,…)`). Validated to the **full M8 bar**: ctest 3070, exec-corpus 291, fuzz `GATES[]`
  `TCO_PTR` (80 seeds 0 miscompiles), **self-host** (mcc's own pointer-recursion-heavy source — e.g.
  `ast_ident_rec(a,n)` — self-compiles with pointer-TCO → working compiler, a strong test), and
  **cross-arch i386/arm64/riscv64 (292/292 each)**. Still open under c): float/struct params.
  d) tail-recursion-modulo-accumulator (`return n*fact(n-1)` → loop + accumulator).

### Confirmed backend codegen gaps vs gcc (measured via `-S`; NOT bounded AST folds — backend/
### target-specific dedicated sessions, unlike the range folds which were target-independent rewrites)

- [ ] **Branchless select for min/max/abs/sign** (`cmov`/`csel`). **Measured:** mcc emits a compare +
  conditional **branch** for `a<b?a:b`, `a>b?a:b`, `x<0?-x:x`; gcc emits branchless `cmovle`/`cmovge`/
  `neg;cmovs`. **mcc's code GENERATOR emits no `cmov` at all** — `cmov` appears only in the i386/x86_64
  **disassembler/assembler** (`src/arch/*-dis.c`, `-asm.h`), never in codegen. So this needs new
  conditional-move emission in each backend (x86 `cmov`, arm64 `csel`, riscv has none — needs a
  branchless-arith fallback or stays branched), plus a safe-to-cmov analysis (both ternary arms
  side-effect-free, no traps). Target-specific → a dedicated per-backend session, not an AST fold.
- [ ] **Branchless boolean-normalizing ternary `cond?1:0` / `cond?0:1`** (frontend codegen, NOT an AST
  fold). **Measured:** mcc emits a compare + a branch and 3 jumps for `x<y?1:0`, `x?1:0`, etc.; gcc emits a
  single `setl`/`setne`/`sete` (branchless). **Confirmed it CANNOT be an AST fold:** `expr_cond`
  (`mccgen.c:9932`) has an `is_cond_bool(vtop)&&is_cond_bool(&sv)` fast path for `cond?bool:bool` that
  lowers via `gvtst`/`gjmp` (branches) AND **returns early before `ast_hook_ternary_end`** — so these
  ternaries DESYNC and the AST optimizer never captures them (verified: `x<0?5:7` is captured but `x<y?1:0`
  is not; a landed `ast_bool_try` never fired and was reverted). The fix is to make that `is_cond_bool`
  path materialize the condition branchlessly (the backend already emits `setCC` — e.g. `!(a<b)→setge` —
  so route through that VT_CMP materialization instead of `gvtst`+`gjmp`). Core default-codegen change,
  target-sensitive (x86 `setCC`, arm `cset`), churns goldens → a focused frontend session, high value
  (very common pattern). Also incidentally fixes the AST-desync (calling `ast_hook_ternary_end` on that path).
  **`abs`/-abs branchless bit-trick LANDED** (`ast_abs_run`, `MCC_AST_ABS`, `AST_SG_ABS` bit19, default
  off, search-selectable): mcc branched (`sub;jge;sub`) on `x<0?-x:x`; now folds to `(x^(x>>31))-(x>>31)`
  (arith shifts, only cheap x-dups, no temp/cmov) → `sub;sar;xor;sar;sub` branchless. Matches `AST_If` op
  `5` (the ternary, children `[cond,tval,fval]`) where `cond = x REL 0` (signed pure int x, literal 0),
  `{tval,fval}={x,-x}`; unary `-x` is `AST_Binary('-',[0,x])` (confirmed: mccgen `vpushi(0);gen_op('-')`).
  Handles all orientations (LT/LE/GT/GE × operand order) → abs or -abs. Validated **with GENUINE cross-arch**:
  default byte-neutral (ctest 3653); all 5 forms (`x<0?-x:x`, `x>=0?x:-x`, `x>0?x:-x`, mirrored, -abs)
  correct over ±1000 on x86_64 **and real i386 and real arm64**; exec-narrowfix, self-host, fuzz (`GATES[]`
  `ABS`). gcc's `neg;cmovs` is marginally better (needs the cmov backend), but this beats the branch.
  min/max bit-tricks (`b ^ ((a^b) & -(a<b))`) are uglier (multi-operand) — still best via cmov.
- [ ] **Constant integer division/remainder strength reduction** (magic-number multiply). **Measured:**
  mcc only strength-reduces power-of-2 (`x/2^k→shr`, `x%2^k→and`); a general `x / 7` stays a hardware
  `div` (~20-40 cyc) where gcc/clang emit a multiply-shift (~5 cyc). **FOUNDATION LANDED (byte-neutral,
  selftested-before-wire):** `src/mccmagic.h` (dep-free, only `<stdint.h>`) — `mcc_magicu`/`mcc_magics`
  (Granlund-Montgomery / Hacker's Delight) compute the 32-bit unsigned/signed magic `(M, shift, add)`,
  and `mcc_divu_apply`/`mcc_divs_apply` are the reference sequences the AST fold will mirror
  (`(uint32)((uint64)x*M>>32)` + shift + the add/sign corrections). Proven by the new `ast/magic` ctest
  (`tools/asttool.c:suite_magic`): **exhaustive** — every divisor ±2..20000 × a dense boundary-heavy
  dividend set (0/1/near-multiples/`0x80000000`/`0xFFFFFFFF`/`INT_MIN`/`INT_MAX`) all match native `/`,
  millions of checks, 0 failures. So the arithmetic is now trustworthy; **remaining = the AST transform
  only** — match `x / C`/`x % C` (`AST_Binary` `'/'`/`'%'` with a const divisor, non-0/1/pow2), build the
  replacement mirroring `mcc_div*_apply`, gate a default-off knob, validate to full M8 (the risk that
  remains is AST-construction, not the magic algorithm). 64-bit division needs a 128-bit high-multiply
  (`mulh`) primitive → backend support; 32-bit is target-independent (uses `(int64_t)` intermediates).
  **TRANSFORM LANDED for the unsigned/no-add-correction subset** (`ast_divmagic_run`, `MCC_AST_DIVMAGIC`,
  `AST_SG_DIVMAGIC` bit18, default off, search-selectable): 32-bit unsigned `x/C`/`x%C` (C non-0/1/pow2, x
  pure, `mag.a==0`) fold to `(uint32)((uint64)x*M>>32)>>s` and `x-(x/C)*C`. Skips `mag.a==1` (needs a
  shared-quotient temp), power-of-2 (backend `shr` for UNSIGNED), and signed non-pow2 (deferred). Verified
  `x/3`,`x/10`,`x%10`→`imul;shr`; `x/7` (a==1) stays `div`. **Signed power-of-two `x/2^k`,`x%2^k` ALSO
  landed** (`ast_divmagic_try_spow2`): mcc otherwise emits `cltd;idiv` for signed pow2 (unlike unsigned,
  the backend does NOT reduce it); folds to the round-toward-zero identity `(x+((x>>31)&(2^k-1)))>>k` (arith
  shifts) and `x-((x/2^k)<<k)` — only pure x duplicated (cheap load, no multiply → no temp needed). Verified
  `x/8`,`x%8`→`sar;and;add;sar`, no idiv (**negative pow2 `x/-8` too**: `x/-2^k = -(x/2^k)`, `x%-2^k = x%2^k`
  — exhaustively validated incl. negatives on 3 real arches). Validated **with GENUINE cross-arch**: default byte-neutral (ctest
  3653); an **exhaustive differential test** (unsigned: 26 divisors × boundary dividends + 300k sweep; signed
  pow2: 10 powers × boundaries incl negatives + 1M sweep; folded `x/C` vs a volatile-divisor reference) = 0
  fails on x86_64 **and real i386 and real arm64**; self-host; differential fuzz (`GATES[]` `DIVMAGIC`).
  **Signed NON-pow2 DIVISION `x/C` ALSO landed** (`ast_divmagic_try_signed`, mirrors `mcc_divs_apply`):
  mulsh(M,x) + d/M-sign correction + `>>s` + sign-bit add. The sign-bit add reuses the shifted quotient so
  the mul-high is DUPLICATED once (2× `imul`; CSE runs earlier so it isn't merged) — suboptimal but a clear
  win over `idiv` (~8-12 vs ~26 cyc), correct; I was over-deferring by treating the 2× multiply as a hard
  blocker. Verified `x/10`,`x/-7`→2×`imul` no idiv; exhaustive differential (19 divisors incl NEGATIVES ×
  boundaries incl INT_MIN/MAX + 800k sweep vs volatile ref) = 0 fails on x86_64 **and real i386 and real
  arm64**; full M8. **Signed non-pow2 REMAINDER `x%C` ALSO landed** (same handler, `x-(x/C)*C` — the
  division subtree, 2× imul, is reused once → ~3× imul, still beats signed `idiv`): exhaustive diff (14
  divisors incl negatives × boundaries + 700k sweep) = 0 fails on x86_64 + real i386 + real arm64; full M8.
  **Reorder-before-CSE tried and REVERTED (doesn't work):** moving divmagic before CSE so CSE merges the
  duplicated mul-high does NOT reduce the imul count — even with templates/CSE on at -O2, CSE's structural
  match does not merge the two mul-high subtrees (verified: `x/10` stays 2 imul). So the optimal 1×-multiply
  form genuinely needs a real temp-materialization mechanism (a Store to a fresh local + Loads), not a
  pipeline reorder. The 2-3× imul cases are correct and beat `idiv`; the 1× refinement is deferred to a temp
  mechanism.
  **Unsigned `mag.a==1` add-correction ALSO landed** (`t=((x-q)>>1)+q; >>(s-1)`, dups the mul-high → 2×
  imul, still beats `div`): `x/7u`→2×imul, `x%7u`→3×imul, no div; exhaustive (18 divisors incl many a==1 ×
  1.5M dividends + boundaries) = 0 fails x86_64 + real i386 + real arm64. **So the 32-bit constant div/mod
  strength reduction is now COMPLETE — divmagic covers EVERY 32-bit case** (unsigned a==0 & a==1, signed
  pow2, signed non-pow2; both div and rem), all with genuine cross-arch validation. **Only 64-bit remains**
  (needs a `mulh` high-multiply backend primitive — genuine backend infra). The duplicated-mul cases emit 2-3×
  imul (CSE runs earlier); optimal 1× form = reorder divmagic before CSE OR a temp mechanism (follow-up).
  **64-bit confirmed genuinely blocked (not an over-deferral):** mcc has NO 128-bit integer type — `__int128`
  is a parse error and `mccgen.c:4121` errors that TI-mode "requires 128-bit integers". So unlike the 32-bit
  magic (which used the existing 64-bit multiply via `(int64)` casts), the 64-bit magic needs the HIGH 64
  bits of a 64×64→128 product, i.e. a `mulh` op the AST/type system can't express — a per-backend primitive
  (x86_64 `mulq`→rdx:rax, arm64 `umulh`/`smulh`, riscv `mulhu`, i386 has none for 64-bit → runtime helper).
  So 64-bit div/mod strength reduction is a genuine backend session, deferred with a real blocker. Optimal 1×-multiply form for the duplicated cases = reorder divmagic before CSE OR a
  temp mechanism — a documented follow-up.
  **⚠ CROSS-ARCH VALIDATION FIX:** `cmake-qemu-i386`/`cmake-qemu-arm64` currently emit **x86_64** (native,
  `MCC_TEST_EMU` empty) — their exec suite is NOT cross-arch. Real cross compilers: `cmake-cross/mcc-i386`
  (`-dumpmachine i386-pc-linux-gnu`; its ELF runs directly on an x86_64 host) and `cmake-cross/mcc-arm64`
  (`aarch64-linux-gnu`; run via `qemu-aarch64 -L vendor/gentoo-stage3-arm64-glibc`). Use those for real
  cross-arch checks.

## NEXT MILESTONE — runtime JIT + guarded deopt (§26) — plan in `docs/JIT-PLAN.md`

Not part of the completed Steps 1–5 rollout (AST.md: "rollout steps 3-5 **+ the JIT**").
The design's `-O4+`/JIT tier: entry-guarded variant dispatch with a runtime recompiler +
hot-swap. Reusable today: the `-run` compile-to-executable-memory + `mcc_relocate`
pipeline, GOT/PLT indirection, `.init_array` ctors, the replayable `ast_cur`. Missing:
the byte-faithful baseline is freed per function (retain it), calls are hard `rel32`
(make JIT'd functions entry dispatchers), `--jit-functions`/`--jit-max-duration` are
inert, no threads, no `eval_slice`. Staged: retain baseline → entry dispatcher →
runtime recompile via the codegen+relocate path → guard+deopt → wire the `--jit-*`
flags → soundness (`eval_slice` later). Open decisions D1–D8 are in `docs/JIT-PLAN.md`.

## Bugs — surfaced by the conformance-test expansion (concrete repros)

- [ ] **Honor auto over-alignment under `-fsanitize=address` / `-b`** — the
  over-align indirect path in `decl_initializer_alloc` is gated off when
  `asan_g`/`bcheck` is active (the native-shadow stack instrumentation and the
  bcheck redzone both assume an rbp-relative slot), so `alignas(32+)` autos are
  under-aligned in those modes (verified: `-O0` gives aligned, `-fsanitize=address`
  and `-b` give unaligned). Needs the shadow/redzone bookkeeping to follow the
  runtime-aligned pointer, or a separate over-aligned+instrumented slot scheme.
- [ ] **Extend auto over-alignment to the PE (Windows) targets** — x86_64/arm64/
  i386 PE are still gated off (`STACK_OVERALIGN_MAX` undefined) because PE routes
  VLA alloc through the `__chkstk`/alloca helper (align-16 only); needs the helper
  parameterized on alignment + a bare-`VT_LLOCAL` load case on the PE paths. No
  native Windows runner here, so validate on a Windows-arm64/x64 cell.

- [ ] **`-std=c89 -pedantic-errors` C99-feature gaps (batch 2c)** — remaining:
  `inline` and `restrict` (both carry a `-std=gnu89` false-positive risk plus a
  keyword-vs-identifier nuance in strict C89 — need a strict-vs-gnu gate), `//`
  line comments (gcc makes this a hard error even without `-pedantic-errors`), and
  non-ASCII/UCN identifiers. Same fix shape — a `mcc_pedantic(...)` at each site
  guarded on `cversion` (+ `!gnu_ext` for inline/restrict).

- [ ] **Research the §28 rewrite-rule IR** — match→rewrite templates over the
  captured arena that the §22/§24 search composes into compound transforms, scored
  by §25, cached by §21, each rule differential-tested against the faithful replay
  before it may fire. (IR form? how does the search compose rules? scoring hook?
  cache key? the per-rule soundness gate?)

## 5 — many open questions

- [ ] **Explore a link-time/ABI differential fuzzer** — mix mcc `.o` with gcc
  `.o`, cross-check struct-return/varargs/`long double`/bitfield layout (the
  current fuzzer is deliberately tools-only, single whole-program).
- [ ] **Build the §27 loop-nest analysis foundation** — a loop-nest model over the
  `AST_If` op 2..5 forms, a conservative dependence test (subscript direction
  vectors, bail-to-"no"), and a legality check. (no new node kind)

## 4 — several open questions

- [ ] **Decide the §33b post-graft window dataflow (the pivot)** —
  splice-then-reanalyze (A: copy the callee subtree into the caller arena so one
  join pass sees the merged window) vs two-pass hand-off (B: thread the caller's
  exit facts into `ast_inline_graft` as the callee replay's entry facts).
  Deliverable is the A-vs-B decision + arena/gate design.
- [ ] **Build scratch-`Section` emit isolation for §22** — redirect
  `cur_text_section` (+ reloc, `ind`, symbol scope) to a throwaway `Section` per
  measurement, measure, discard, emit the winner once. In-place save/restore was
  proven insufficient (`ast_promo_entry_init` desyncs). The real production
  consumer of `ast_arena_clone` (today only in `tools/asttool.c`); milestone-scale.
- [ ] **Explore EMI mutation (Orion/Athena/Hermes)** targeting optimizer
  miscompiles.
- [ ] **Design the broader template library** (algebraic/dead-branch/jump-table).

## 3 — a few open questions

- [ ] **Decide compiler-rt-interop vs `libmccsan`** — shapes recover-mode/ASan
  downstream.
- [ ] **Investigate the §33d seam peephole window / McKeeman peephole** — a
  store-to-slot immediately followed by a load-from-the-same-slot straddling the
  inline boundary. Resolve whether a bounded 2–3-op window elision preserves the
  pass-1 faithfulness contract, or must run only in pass-2 replay under a
  differential exec gate.
- [ ] **Revisit §32c genuinely-speculative arm insertion (deferred by design)** —
  inserting E into an arm where it is not guaranteed to reach a post-join use can
  pessimize cold paths and is the class that killed the earlier prototype (arm64
  self-host miscompile). Only revisit with the 3-stage self-host fixpoint as the
  gate. (PRE hoist-only ships: `MCC_AST_PRE`, default off)
- [ ] **Explore coverage-guided generation** — gcov / Intel-PT feedback into
  `tests/fuzz/gen.h` (today purely deterministic seed-driven).
- [ ] **Build the `.rodata` data-emission project** — the `AstKind` enum has no
  array/global/static-data kind and no pass emits initialized data; add a
  table-symbol+initializer emitter wired into the replay/rewrite lifecycle.
  Prerequisite for §30 value-table dispatch.
- [ ] **Close the riscv64 Tier-3 backend gap** that blocks full `src/mcc.c`
  self-host (real-program codegen is correct; the whole-compiler self-host is not).
- [ ] **Build a systematic negative/`dg-error` diagnostic tier** — gcc's C99/C11
  files are ~70% diagnostic.
- [ ] **Build the `H_e` epoch hash** — invertible slot-keyed O(1) edit patch;
  designed, not built. Must reconcile the `slot_key` dual-use with the
  `cst_mark_branch` PPConditional tags (`mcccst.c:544`, invoked at `mcccst.c:1112`).
- [ ] **Design cross-TU LTO.**
- [ ] **Design separate `-O2`/`-O3` SSA drivers.**
- [ ] **Design a full `-g` debugger + gdb test suite.**

## 2 — two open questions

- [ ] **Port native-shadow ASan (inline probe + `mccasan.c` runtime) to
  arm64/riscv64** — the native shadow is x86_64/ELF-only end-to-end; those arches
  only have the separate bcheck-based `-fsanitize=address` today.
- [ ] **Implement arm64/riscv64 native-shadow stack-redzone instrumentation** via
  the `gfunc_prolog`/`gfunc_epilog` hooks (x86_64/ELF-only today). (needs the
  native-shadow port)
- [ ] **Implement UBSan `-recover` mode** — `sanitize-recover=undefined` is parsed
  but silently ignored; no recover state var or codegen.
- [ ] **Explore a self-host differential** — compile `src/mcc.c` with mcc vs gcc
  and diff the two compilers' behavior over the corpus.
- [ ] **Explore a freestanding/KASAN-style sanitizer for the runtime itself.**
- [ ] **Inline cross-TU static callees.** (§23 step 3)
- [ ] **Explore heuristic non-static inlining** (optional). (§23 step 4)
- [ ] **Implement §24 hot-slice budget allocation** — use the landed
  `MCC_AST_COST` model to allocate `optimize_search_seconds` to the top functions
  first; rank by `-g` profile entry-frequency, else `node# × loop-nest-depth ×
  call-out-count`. (needs §22)
- [ ] **Implement the §25 `-g` hot-value cache** — log function-argument and
  branch/switch key values + frequencies beside the opt checkpoint cache; seed
  each strategy's `MIN..MAX` from the observed hot range. Feeds §29 + §30.
  (`MCC_AST_JITSCORE` already ships.)
- [ ] **Embed the §26 per-function intention trees + libmcc slice** into `-O4+`
  output — the ~800 KB slice is the dominant size/build-system cost.
- [ ] **Implement §26 hot-function recompile + hot-swap** — recompile via the
  embedded `mcc_relocate`, hot-swap through an atomic-pointer slot +
  triple-buffer/RCU reclamation.
- [ ] **Explore §28 instruction-level superoptimization** over a fixed emitted
  window (optional).
- [ ] **Build the §29 integer range/known-bits lattice** — shared prerequisite for
  the narrowing residue.
- [ ] **Implement §30 value-table dispatch** for bit-flag clusters with *differing*
  bodies. (needs `.rodata` data-emission)
- [ ] **Refactor the §31 scheduler to a static-vtable strategy registry** — passes
  are invoked by a hardcoded env-gated `if` chain today.
- [ ] **Build widening/fixpoint dataflow for §32a** cross-loop-iteration value
  merging (none present today).
- [ ] **Implement §33c argument de-spill / caller-value forwarding** — forward a
  caller's live single-use value directly into the callee's first param use (the
  non-const generalization of the const `ast_argsub` channel); legality = param
  read-once before any store, operands unclobbered. (needs §33b's seam; optionally
  §32c)
- [ ] **Design the §33e window-level cache key** — `ast_intention_hash` runs
  pre-graft over the caller arena, excluding the callee body, so a window transform
  needs a window-level key or an accepted first-graft cache miss.
- [ ] **Extend §35 to an n-ary reassociation-aware ordering** past top-level
  commutative pairs (reassociation itself stays out — not commutative-safe).
- [ ] **Implement §36 spill-slot sharing** — extend the `MCC_AST_COLOR` interval
  sharing to spilled ranges; subsumes the A1 backward-liveness item.
  Fixpoint-gated + native arm64/riscv64.
- [ ] **Normalize CMake incrementally** — autodetect + enable-what-the-host-
  supports, offload gating to `tools/`, fold `.cmake` files in — with a verifiable
  target, not a sweep (CI-breakage risk across ~35 presets/platforms).
- [ ] **Cut CI wall-clock — attack the long-pole jobs** (from `logs_78925034425`:
  run starts ~21:01, last job `dist / macos-x86_64-clang` finishes 21:24:54, so ~24
  min end-to-end). The critical path is macOS + Windows + matrix jobs; native Linux
  is already fast (ctest ~60s). Biggest per-job sinks, in priority order:
  - **The `bench` target (~500s)** runs on the macOS/dist jobs (e.g. macos-cross:
    build+ctest done 21:09:48, then `--target bench` alone runs to 21:18:21). Gate it
    to a single fast native runner or a nightly job instead of every macOS/dist cell.
  - **macOS ctest is ~7× native (~431s vs ~60s)** — slow runners / residual Rosetta
    emulation (a prior pass already removed some; see [[arm64-native-ci-failures]]).
    Shard the macOS ctest across more `-j`/jobs and shrink the emulated subset.
  - **Matrix jobs re-run the full ctest per config cell sequentially** (38_matrix:
    three ~146s passes ≈ 430s). Parallelize cells or prune redundant ones.
  - **Windows msvc / sanitize-msvc / mingw ~900-970s** — profile build-vs-test split
    and cache/prune. Measure each change against the same log-derived baseline.
- [ ] **Implement slice-G multi-file `#include` stitching** — currently main-file
  only (the one open CST slice).
- [ ] **Root-cause the named promote/inline gap tests.**
- [ ] **Revisit PP-as-executable-C JIT** (the broader form; `-fmacro-eval`
  shipped).
- [ ] **Design a time-budgeted engine.**
- [ ] **Design dependency-ordered `-O1`.**
- [ ] **Design `-g` from provenance.**
- [ ] **Design human-friendly diagnostics** tested against terminal geometry.
- [ ] **Design `--hotreload` from reconciled CST snapshots.**

## 1 — one open question

- [ ] **Preserve the faulting address to the asan-shadow trap** (found by the
  `[x]`-audit) — the `-fasan-shadow` SIGILL report has the class, pc, shadow byte,
  and granule offset but is missing the faulting data address, access type
  (READ/WRITE) and size, the region-relative locator ("N bytes after M-byte region
  [lo,hi)"), and the "Shadow bytes around the buggy address" hex dump that real
  ASan prints. Root cause: the codegen traps with only the shadow byte (rax) and
  granule offset (rdx) live — the fault address is not carried to the `ud2`.
  `on_sigill` in `runtime/lib/mccasan.c` can format the rest once the address is
  preserved.
- [ ] **Implement the clang-compatible `__ubsan_handle_*` diagnostic ABI** — trap
  mode ships (`ud2` on x86_64, `brk` on arm64/riscv64); no handler ABI exists.
- [ ] **Implement a PE/mingw trap-mode UBSan** — trap mode is gated ELF-only.
- [ ] **Explore `-fsanitize-coverage`** — feeds the coverage-guided fuzzer.
- [ ] **Explore `-fsanitize=cfi` hardening** (absent today).
- [ ] **Explore `_FORTIFY_SOURCE`-style hardening** (absent today;
  `-fstack-protector` already ships with real x86_64/arm64 canary codegen).
- [ ] **Add the §22 promotion re-emit axis** on top of emit isolation. (needs the
  scratch-`Section` isolation)
- [ ] **Add the §22 arena-mutating pass-subset re-emit axis** on top of emit
  isolation. (needs the scratch-`Section` isolation; inline-size axis
  `MCC_AST_PERFN_INPROC` already ships)
- [x] **Widen the §23 inliner budgets** — **LANDED (all three now runtime knobs).** graft
  (`MCC_AST_GRAFT`=2048) and node-limit (`MCC_AST_INLINE_NODES`=64) were already env-configurable;
  the depth cap `AST_INLINE_MAX_DEPTH`(8) is now split into an array cap (32) + runtime
  `ast_inline_depth_max` (`MCC_AST_INLINE_DEPTH`, default 8 → byte-identical), raisable for deeper
  inline chains. Byte-identity-gated (default preserved), validated: ctest 3070, deep-inline exec
  (10-level chain correct at depth 8 vs 16), fuzz `GATES[]` `INLINE_DEEP` (`MCC_AST_INLINE=1
  MCC_AST_INLINE_DEPTH=16`, 80 seeds 0 miscompiles), self-host with depth=16. Still to do: register
  as a §22 search knob (it's a value axis, not a bit — wants the emitted-size scoring since inline
  effects are emit-time). (§23 step 1)
- [ ] **Add more §23 param shapes.** (§23 step 2)
- [ ] **Add the `--jit-threads` flag** — does not exist yet (§26).
- [ ] **Build the §26 ELF `.init_array` ctor** spawning the `--jit-threads` pool.
- [ ] **Enforce the `--jit-max-duration` runtime bound** — parsed but not enforced
  (§26). (run §26 LAST; builds on §25 + §21)
- [ ] **Implement the §27 interchange rewrite** + re-run the §22 search after the
  nest changes. (needs the loop-nest analysis foundation)
- [ ] **Implement §27 loop fusion.** (needs the loop-nest analysis foundation)
- [ ] **Implement §27 loop tiling.** (needs the loop-nest analysis foundation)
- [ ] **Extend §29 narrowing to non-distributive `/ % << >>` + comparisons** —
  `ast_narrow_binop` handles only the distributive `+ - * & | ^` today. (needs the
  lattice; `MCC_AST_NARROW` truncation-sink narrowing ships default-on -O2)
- [ ] **Implement §29 outer-narrow elimination** — drop a cast when the value
  provably fits. (needs the lattice)
- [ ] **Add the §30 `switch`-arm detection form.**
- [ ] **Implement §31 adaptive beam width.**
- [ ] **Implement §31 per-function scoping.**
- [ ] **Wire §25 scoring of the §33e de-spill delta.**
- [ ] **Register the §35 Sethi–Ullman ordering as a §31 search strategy** —
  `MCC_AST_SETHI` is called inline in the emit loop today. (needs the §31 registry)
- [ ] **Replace the `ast_plan_promotion` heuristic with §36 coloring outright**
  (not just filter it). Fixpoint-gated + native arm64/riscv64.
- [ ] **Verify Tier-4 inline (`ast/replay-inline-spec`) on riscv64/other arches,
  then ungate** — registered on x86_64 + arm64; skip-gated elsewhere.
- [ ] **Extend the arm64 backend register model for Tier-3 register promotion** —
  `MCC_NB_REGS=28` doesn't expose x19–x28 — + qemu validation. (promotion analysis
  is arch-agnostic and reused)
- [ ] **Extend the riscv64 backend register model for Tier-3 register promotion**
  + qemu validation.
- [ ] **Test the i386 TLS `R_386_TLS_GD/LDM` paths** (`i386-link.c`; i386-gen.c
  only emits `R_386_TLS_LE`, so GD/LDM are untested) — needs an i386 cross + a
  32-bit sysroot.
- [ ] **Audit each `mcc_skip_test` for per-triple ungating** — i386-linux blocked
  (no 32-bit sysroot); aarch64/armv7-linux partial (qemu is x86-TSO — only the
  memory-model-independent subset). arm64-windows is **no longer blocked** — CI now
  runs a native `windows-11-arm64` cell (MSVC 2022 ARM64 as the ref cc) that passes the
  full 1809-test suite (`logs_78925034425`); revisit the arm64-windows `mcc_skip_test`s
  for ungating there.
- [ ] **Revisit the `Bind`-marker** — only if the CST can't answer a `-g`/LSP query.
- [ ] **Revisit the `k` always-inline depth policy.**
- [ ] **Revisit size-gated outline.**
- [ ] **Revisit store factoring** (shared render engine).
- [ ] **Revisit the template DSL past ~30 templates.**
- [ ] **Revisit per-function `-O1` mode.**

## 0 — fully specified or execution-blocked (no open design questions)

**LANDED — `MCC_TRACE` tracing.** The macro (`src/mcclog.h`) prints
`[TRACE] FILE:LINE func: ` + args, compiled out unless `MCC_CONFIG_TRACE` (CMake option,
default off), runtime-gated on the `[TRACE]` verbosity bit (`-v128` logs TRACE only). The
compiler pipeline is instrumented at its phase entry + a branch point — `mcc_compile`,
`mcc_preprocess`, `gen_function`, `ast_func_begin`, `ast_func_end`, and the inline/
promote/tco decision — all proven to fire (`mcc -v128 -O2 -c` shows the per-function
trace; default trace-off build is byte-neutral, 1905/1905 ctest).

- [ ] **`MCC_TRACE` follow-ups** — (a) `MCC_TRACE`/`mcc_logf` read the global
  `mcc_state->verbose`, so a trace fires only where `mcc_state` is the current
  verbose-carrying state (driver/link phases before `mcc_enter_state`, e.g.
  `mcc_output_file`, don't fire — either thread the state or add a state-taking variant);
  (b) blanket per-function instrumentation is intentionally *not* applied (it would be
  noise) — add `MCC_TRACE` at points of interest as needed. **Points added (search subsystem):**
  the combo candidate + winner (`gates`/`score`/`base`/`searchable`), the memo hit
  (`funcname`/hash/`gates&searchable`/refcount bump), the disk load (path/codec/raw-size/entry
  count), and the disk eviction (usage/dropped-count) — all greppable by exact function name and
  argument values, e.g. `-v128 ... | grep 'memo hit'`. (c) wiring `MCC_CONFIG_TRACE`
  into a preset is deliberately skipped (the release-inherits-debug caveat that applies
  to `MCC_CONFIG_AST_SHADOW`); (d) migrate ad-hoc `if (verbose) fprintf(stderr,...)`
  sites to the tagged `mcc_logf`/`MCC_DEBUG` categories.
- [ ] **Ungate the `i386-fastcall-abi` test** — registered but `mcc_skip_test`'d;
  needs an i386 cross + an ELF-32 reference.
