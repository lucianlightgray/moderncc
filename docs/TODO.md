# TODO

Sorted by number of open questions/ambiguities (first-round unknowns + the
sub-questions immediately following them), most-open first.

## AST substrate + unified optimizer

Collapse the three optimization drivers (the `ast_func_end` pipeline, the §22
`AST_PF_EMIT` trial, the `mcc.c` out-of-process search) into one side-car
substrate + one memo + one strategy engine, shared by the AOT backend and a live
JIT. This reframes/subsumes several items below (§21 cache key, §22 emit
isolation, §28 rewrite IR, §33b/e seam+window keys, §30 predicate bitset, H_e
epoch hash, the time-budgeted engine, per-function `-O1`, PP-as-executable JIT).

**The staged AST-optimizer rollout (steps 1–5) is COMPLETE.** On `main`, default byte-neutral, each gated by `-O6`
differentials + full ctest + the `MCC_CONFIG_AST_SHADOW` shadow build (zero side-car divergence):
the `(tag,id)` naming partition (`src/mccname.h`); the read-only side-car (step-1 `ast_du_*` def/use,
step-2 `ast_memo_*` property memos, step-3 `ast_hash_*` structural hash); the step-4 strategy engine
(the sole `ast_strategies[]` pipeline; the legacy inline block + `MCC_AST_ENGINE` removed); and the
step-5 live -O4+ search (`ast_search_select`/`MCC_AST_SEARCH` over the fold-gate lattice, with
static-cost, emit-size `MCC_AST_SEARCH_EMITSIZE`, and NCores-1 fork-pool `MCC_AST_SEARCH_THREADS`
scoring; next-tick forecast in `src/mccforecast.h`; in-memory + disk memo `mcc-search.memo` keyed by
`ast_intention_hash`). The search only *selects* a gate config — the normal pipeline emits the winner
on the untouched tree, so a search bug can only pick a larger-but-correct config, never a miscompile;
-O4+ output is timing-bounded / non-reproducible by design (quarantined there — `-O1..-O3` never
search, stay byte-reproducible). Runtime JIT + guarded deopt is **not** a rollout step (the design
lists it as "steps 3-5 **+ the JIT**") — it is the separate post-rollout milestone below, gated by
the §26 recompiler design.

The Step-5+ scoring/parallelism continuations still open (were inline in the removed LANDED prose):

- [ ] **Step 5+ — emit-size scoring under the *tick* scheduler + JIT-runtime scoring** — emit-size
  scoring is run-to-completion per candidate today because the fair-interleave tick scheduler
  thrashes the shared ltemp/fconst emit state across candidates; making it tick-interleavable needs
  the per-context emit state (the C11-thread item below). JIT-runtime scoring — wiring the shipped
  `MCC_AST_JITSCORE` runtime measurement into the search's ranking key — is the other half. (needs
  §22 scratch-`Section` emit isolation)
- [ ] **Step 5+ — C11-thread pool with `_Thread_local` per-context state** — the fork pool (COW
  isolation) already covers candidate *scoring* with no thread-local marking, so this is only needed
  for interior / tick-mode parallelism. The per-context-state refactor is the remaining substrate
  item if ever wanted — its own gated change (side-car shadow + fixpoint + fuzz).
- [ ] **Step 5+ — widen the search space** — the candidate set is the subset lattice of
  `searchable = base | opt-in-knobs` on `combo_run` (see M1). **LANDED:** the search ADDS knobs that
  are off in every -O baseline (not just drops base gates); `AstGateMask` is a `uint64_t` (low 48 bits
  persisted — `AST_GATE_BITS`; MAGIC in the high 16, old 8-bit records rejected); budget-scaled to
  `AST_SEARCH_MAX_CAND`=128 with a best-first `est_cost_delta` frontier that scores base's
  single-toggle neighbours first and reorders `items[]` most-improving-first (non-silent when the cap
  truncates). 13 opt-in knobs are wired to `AST_SG_*` bits and added to `searchable`: NARROWFIX,
  SETHILEAF, LTEMP, IVSR, PRE, DSECALL, TCOPTR, CSECOMM, RANGE, DIVMAGIC, ABS, REASSOC, SCCPFIX (the
  narrow/sethi/templates-modifier knobs are base-gated; RANGE/DIVMAGIC/ABS/REASSOC/SCCPFIX standalone).
  CI coverage: the `exec-{narrowfix,search,search-emitsize,search-threads}/*` ctest variants + the
  fuzzer `GATES[]` table cover the whole search subsystem end-to-end; default `-O1..-O3` byte-identical.
  **Still open:** the **inline/promote axes** (want emit-size scoring — inline/promote effects are
  emit-time), and the search-mode superopt shadowing (templates-gated knobs only fire in perfn mode,
  so the search-mode workers never exercise them → M3 wiring).
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

**LANDED — compression codecs + the `mcccombo` substrate.** `src/algorithms/{rle,lzss,lzw}.h`
(header-only PackBits-RLE / LZSS / LZW, selftest 18/18) and `src/mcccombo.h` (`combo_run`
subset×ordering enumerator with `ComboScoreFn` lower-is-better, `combo_pipeline_search` codec-chain
search, `ComboMemo` refcount + LFU-evicted key->value cache; selftest 20/20). The -O4 disk memo
(`mcc-search.memo`) stores a compressed "MSZ1" best-of-3 (rle/lzss/lzw) container. **M1 wired
`ast_search` onto `combo_run`**; the remaining call-site migration (the memo struct, the superopt,
the formula families) is tracked as M2/M3/M7 below.

### Substrate indices/analyses designed but not built (from the retired substrate design doc)

The rollout built three of the four planned side-car indices (`ast_hash_*` structural hash, `ast_du_*`
def/use, `ast_memo_*` property memos) plus the strategy engine and search. These designed pieces were
never rollout steps and have no symbol in `src/` today:

- [ ] **Predicate-vector projection — the 4th side-car index** — a packed bitset of tested-predicate
  truths over ≤8 named slots in a window (the `predicate_vector(cursor, keys≤8) -> bitset` verb), the
  semantic sibling of the structural hash, for **branch coalescing** — generalizes `ast_bf_run` (V-bf)
  + the §30 value-table/bit-flag dispatch. No `predvec`/`predicate_vector` symbol exists (only the
  three other indices were built). Distinct from the §30 *transform*: this is the index it would read.
- [ ] **`context_in` / `context_out` value-domain fact lattice (the "net-new half")** — the value-domain
  restriction on live-in slots: a bounded backward walk collecting the equality/range predicates of
  dominating `AST_If` conditions (reuse the `ast_cprop_{koff,ktt,kval}` set shape), O(fixpoint) first /
  O(1) warm. It is the checker's enumeration bound (see `eval_slice`, §26 Stage 4) and the memo's
  *context* key. Step 3 shipped only the structural-hash half (`ast_hash_*`); no `context_in`/
  `context_out` symbol exists. Overlaps but is not §29 (that lattice is scoped to narrowing residue,
  not the reaching-context proof-domain / memo key).
- [ ] **Descendant-indexed (DFS enter/exit) def/use extension** — so the two *subtree-scoped* write
  queries `ast_licm_written` (`mccast.c`, called from cse/licm) and `ast_ivsr_count_writes` (`mccast.c`,
  ivsr) become O(1) table lookups. PR-2's whole-function `ast_du_*` table subsumes only the two
  whole-arena scanners (`ast_cprop_escapes`, `ast_local_is_readonly`); "written under node n" needs a
  descendant range index. Both remain recursive subtree walks today.

### Macro roadmap — collapse both searches + const-data onto one substrate

Grounded by two audits: (i) the out-of-process superopt duplicates **every** concern of the
in-process `ast_search` on a second substrate (process-spawn + ELF-file measurement vs.
arena-clone + cost model); (ii) the substrate target (`src/mcccombo.h`) and its four migration
call-sites already exist. Numbered macro steps; lettered sub-features inline; `(A)` nested
subgroups; each names the concrete hook and the double-checked blocker. Order is dependency order
(M4 before M6; M5 before M6).

- [x] **M1 — wire the live -O4 search onto `combo_run`** — **LANDED (subset mode, byte-identical).**
  `ast_search_select` drives the serial static-cost and emit-size searches through `combo_run`
  (`ast_search_combo_score` → `ast_search_score_one`, which owns the `ast_cur` + gate save/restore so
  `combo_run` stays a pure enumerator; budget/abort via `COMBO_REJECT`); base is scored first and wins
  ties, and the candidate set equals the old submask lattice `{base .. 0}`. Byte-identical to HEAD
  across `-O2` / `MCC_AST_SEARCH` / `+EMITSIZE` / `+THREADS`. Also landed on top: the **hit-count
  tie-break** (`ast_search_pack_score` — within an equal cost, favor the config whose passes fired
  more folds; -O4+ only, default untouched). **Remaining — M1(c):** ordered enumeration
  (`MCC_AST_SEARCH_ORDERED`) is plumbed to `combo_run`'s permutation mode but a no-op over the bitmask
  vocabulary; making it meaningful needs `ast_strategies[]` as the ordered combo vocabulary AND
  `ast_func_end`'s frozen loop to honor the discovered per-fn order (tracked in the variation catalog
  below); `ast_fc_forecast` best-first ordering is the open synergy.

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
  **OBSERVABILITY FOUNDATION LANDED (byte-neutral).** `ast_search_emit_size` (`mccast.c`) now
  snapshots `data_section->data_offset` + `rodata_section->data_offset` before `ast_replay_body`
  and computes `ddelta`/`rodelta` after (M4a). The per-candidate delta is exposed via `-v128` TRACE
  (`ast_search_emit_size: emit-size data delta text=.. data=.. rodata=..`, greppable by fn+values),
  giving M6 the measurement hook. **M4(b)+(c) score-folding is DEFERRED, and the reason is now a
  measured finding, not a guess:** the replay re-emits `.rodata` **float constants** (verified —
  a `(int)x + (unsigned)(x*2.5)` candidate shows `rodata=8`), and those constants are emitted once
  and **shared/reused across candidate clones** (`ast_fconst_reuse`/`ast_fconst_record` no-op under
  replay), so the per-candidate rodata delta is **order-dependent noise** ("who measured first"), not
  a transform-attributed cost — folding it into `size` changes selection unfairly, and an earlier
  attempt that also *restored* the offset **miscompiled** (`exec-search-emitsize/{random_stuff,
  double_to_signed}`: truncating the shared rodata dropped reused constants → wrong values). So the
  score stays **text-only** (selection byte-identical, ctest 3653/3653) until M6's data-**rewrite**
  step provides a real, per-candidate, transform-attributed data delta to count. The snapshot must
  **not** rewind data/rodata (they are shared, deliberately grown by the original path).

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

- [ ] **M6z — zero-init `.bss` placement** (a free placement fix surfaced by the M5 side-car; NOT
  compression). mcc emitted `static int x[256]={0}` into `.data` as 1024 zero disk bytes; C11 6.7.9
  makes `={0}` ≡ no initializer, so it belongs in `.bss` (NOBITS). **LANDED (opt-in `MCC_ZERO_BSS`,
  full M8 bar).** Approach B (post-emission move) in `decl_initializer_alloc`: truncate
  `data_section->data_offset` back to the object's `addr`, re-allocate in `bss_section`, re-bind the
  symbol (`put_extern_sym`) — relocations are symbol-keyed so references fix automatically. Guarded to a
  provably-safe subset (`v && sym`, `sec==data_section`, `!bcheck && !asan_g && !flexible_array && !TLS`,
  last-allocation, and **initializer emitted no relocation** — the critical guard: a pointer init
  `={0,&g}` has zero bytes but a reloc, so it is correctly NOT moved). Validated default byte-neutral;
  `MCC_ZERO_BSS=1` exec 292/292 x86_64 + i386 + arm64, self-host, fuzz 7/7; `exec-zerobss/` ctest locks
  it. `-v128` TRACE `zero-bss move`. **Remaining:** TLS `tdata`→`tbss` and the asan/bcheck cases
  (excluded by guards); flip to default-on after broader field exposure.

- [ ] **M6s — string-literal merging** (`-fmerge-constants`-style rodata pooling; opt-in
  `MCC_MERGE_STRINGS`). **LANDED, full M8 bar.** C11 6.4.5p7 leaves identical string literals'
  distinctness unspecified, so sharing storage is sound (unlike const *array* dedup, which C11 6.5.9
  forbids). A value-use literal (`str_init`, `v==0`) is homed at an anonymous symbol; references are
  symbol-keyed, so merging is the same symbol-rebind as M6z. In `decl_initializer_alloc` after
  `decl_initializer`: content-key the just-written bytes (`ast_strpool_find_or_add` — hash + memcmp on
  bytes+size+align so wide never aliases narrow, per-TU); on a hit, zero the reclaimed slot (restores
  the "space at/after `data_offset` is zero" invariant — a bug the regimen caught), truncate
  `rodata_section->data_offset`, re-home the symbol. **Suffix/tail sharing also landed** (`"bar"` inside
  `"foobar"`): re-home to `A_addr + (A_size − size)` — references are `symbol + addend` so they resolve
  into the interior (C11 6.5.2.5 permits overlapping representations), guarded by `interior_off % align
  == 0`. Validated default byte-neutral; `MCC_MERGE_STRINGS=1` exec x86_64 + i386 + arm64, self-host,
  fuzz 7/7; `exec-mergestrings/` locks it. `-v128` TRACE `string merge`/`strpool suffix`.
  **Remaining:** default-on.

- [ ] **M7 — formula-family unification** (the long tail). a) expose cost/ratio formulas as
  fold-math builtins (`mcc_cost_*`/`mcc_ratio_*`, copy-pasting the `foldfc_try` template,
  `mccgen.c:8402`); b) make the forecast ensemble a first-class `combo` formula family (pick
  codec/strategy/threshold, not just next-tick duration); c) one `-f` front — extend `fold-math`
  (`s->fold_math`, `libmcc.c:1902`), which **already** unifies libm + forecast folding, or add a new
  gate. *Synergy:* one enumerator over {strategies, predictors, codecs}; one flag family; the
  four `host_cache_dir` caches collapse to one `ComboMemo` store.

- [ ] **M7b — graduate the disk search-memo into compiled-in strategies (`cache` -> `src/algorithms/jit.h`).**
  A new `tools/` C utility + a CMake target that reads the shared cache dir (`~/.cache/mcc/`, i.e. the
  `mcc-search.memo` MSZ1 container — records `{intention-hash, gates|MAGIC<<8, refcount, score, tried}`,
  see M2) and **materializes each hot (high-refcount) memoized winner as an entry in a generated header
  `src/algorithms/jit.h`**, then **registers those entries in the AST optimization-strategy list**
  (`ast_strategies[]`, `mccast.c`) so a discovered gate configuration ships compiled-in instead of being
  re-searched every build. Flow: (1) the tool emits, per graduated record, a `{intention-hash, gate-mask,
  score}` row into `jit.h` (dep-free, header-only, the "selftested-before-wired" `mcccombo.h`/`mccgate.h`
  pattern); (2) `ast_func_end`/`ast_search_select` consult the compiled-in `jit.h` table as a
  **zero-latency memo tier above the disk memo** — a keyed hit by `ast_intention_hash` applies
  `graduated & searchable` directly, no search, no disk read; (3) **once verified that the newly
  compiled-in strategy is matched by the optimizer to that cache key** — i.e. a build proves the live
  `ast_intention_hash` of some function equals the graduated key AND the compiled-in mask reproduces the
  cached winner (differential/byte gate) — **the tool removes that entry from `~/.cache/mcc/`** (the disk
  record has served its purpose; drop it to keep the cache to genuinely-live, not-yet-graduated shapes).
  **Open questions:** (a) is a graduated record a *gate-mask* replay (trivially a new `jit.h` row consumed
  by the existing pipeline) or does it need to synthesize a genuinely new *algorithm* shape (a real new
  `AstStrategy.apply`)? — the cache stores gate BITMASKS today (M3 vocabulary bridge, `mccgate.h`), so v1
  is mask-graduation; algorithm-synthesis is the harder follow-on. (b) key stability: `ast_intention_hash`
  is salted per version/triplet (`ast_search_key_salt`) — `jit.h` must carry the salt so a graduated entry
  is only consulted for a matching build/target (else a stale mask fires on an incompatible shape). (c) the
  removal step's verification gate (byte-diff vs re-search? shadow-oracle recompute==graduated?). (d) when
  does the tool run — a manual `--target jit-graduate`, or a build step that folds the hottest N records in?
  *Synergy:* this is the AOT dual of the §26 runtime JIT (NEXT MILESTONE below) — instead of recompiling hot
  functions at runtime, it bakes the search's disk-memoized winners into the compiler; rides M2's
  `ComboMemo`/MSZ1 format and M3's `AstGateMask` vocabulary directly. Gated by M8.

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

- [ ] **V-bfold** (`ast_bfold_run`, table `ast_bfold_tab`, ~~8~~ 9 ops) — **a) PARTIAL — `round`/
  `roundf` LANDED (default-on).** New id-8 table rows + a bit-exact hand-rolled `ast_bfold_round`
  kernel (round-half-away-from-zero, built on the existing `ast_bfold_trunc` since `x - trunc(x)` is
  always exact in IEEE) + `case 8` in `ast_bfold_eval_f`/`_d`. Errno-free and rounding-mode-independent
  (unlike `nearbyint`/`rint`), so it joins the default-on errno/rounding-safe subset. `-v128` TRACE
  `ast_bfold_run: bfold round id=8 ... res=0x..`. Validated: **ctest 3653/3653** (no golden
  regression), native differential vs gcc **bit-exact** across `round(2.5)=3`/`round(-2.5)=-3`/
  `round(-0.5)=-1`/`roundf` (half-away + negatives + signed-zero), non-const `round(x)` **preserved**
  (still calls libm, 2 undef refs), **self-host fixpoint-invariant passes**. Cross-arch is
  correct-by-construction (a target-independent literal splice via the same double-literal backend path
  as the 7 already-cross-validated folds; the `cmake-cross/mcc-{i386,arm64}` builds don't run bfold at
  all — verified floor/trunc also don't fire there). **Still open under (a):** `fmod` needs a real
  exact-remainder kernel (NOT the lossy `x - trunc(x/y)*y`); `nearbyint`/`rint` need the (d) rounding-
  mode gate; `ldexp`'s `int` 2nd arg doesn't fit the same-btype `ab[]` loader; `pow/exp/log/sin/cos/
  hypot` already fold in the `-ffold-math` `foldmath_try` engine (mccgen), don't duplicate here.
  b) `fma` contraction (`a*b+c` all-literal);
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
  (`SCCP_FIX`, 100-seed campaign 0 miscompiles) + exec 14/14 vs default. **NOW an `AST_SG` search
  bit — LANDED:** `AST_SG_SCCPFIX` (bit 21, `mccgate.h`) wired into `ast_search_gates_now`/`_set` and
  added to `searchable` under the templates-gated group (like ltemp/ivsr/pre — sccp runs inside the
  templates block), so the -O4 search enumerates it. Scheduling-only (correct-by-construction: the
  search only selects among individually-sound configs), so default `-O1..-O3` stay byte-identical
  (ctest **3653/3653**). Verified perfn-mode `searchable=0x3ff83f` (bit 21 set, `nitems=17`) via
  `-v128` `combo winner` TRACE, perfn -O4 exec output matches gcc, shadow-build search/exec/ast subset
  937/937 zero-divergence. As predicted it is shadowed in search-mode (sccp templates-gated); **perfn
  mode is where it actually varies the winner.** Adding the bit pushed the perfn vocabulary to
  `nitems=17`, one past `COMBO_MAX=16`, so `combo_run`'s internal subset enumeration now clamps — but
  the best-first frontier scores **every** single-toggle `base ^ items[i]` (incl. SCCPFIX) before the
  capped combo pass, so no single knob is lost; only combinations of the least-improving knob are
  dropped. Made that clamp **non-silent** per the M1 "no silent caps" rule (`-v128` TRACE
  `combo enum clamped nitems=17 -> COMBO_MAX=16 ...`).
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
- [ ] **Constant integer division/remainder strength reduction** (magic-number multiply).
  **32-bit is COMPLETE** — `src/mccmagic.h` (`mcc_magicu`/`mcc_magics`, Granlund-Montgomery /
  Hacker's Delight, exhaustively selftested by the `ast/magic` ctest) + `ast_divmagic_run`
  (`MCC_AST_DIVMAGIC`, bit 18, default off, search-selectable) fold EVERY 32-bit case: unsigned
  `x/C`,`x%C` (both `a==0` and the `a==1` add-correction), signed pow2 (incl. negative), and signed
  non-pow2 — replacing hardware `div`/`idiv` with `imul;shr`/`sar` sequences. Validated to the full
  M8 bar with genuine cross-arch (exhaustive differential sweeps vs a volatile-divisor reference = 0
  fails on x86_64 + real i386 + real arm64; self-host; fuzz `GATES[]` `DIVMAGIC`); default byte-neutral.
  **Open:** (a) **64-bit** — genuinely backend-blocked: needs the HIGH 64 bits of a 64×64→128 product
  (`mulh`), which mcc's type system can't express (`__int128` is a parse error, `mccgen.c`) — a
  per-backend primitive (x86_64 `mulq`, arm64 `umulh`/`smulh`, riscv `mulhu`, i386→runtime helper).
  (b) the **optimal 1×-multiply form** for the signed / `a==1` cases (they emit 2-3× `imul` because CSE,
  which runs earlier, won't merge the duplicated mul-high — reorder-before-CSE was tried and does not
  merge them) → needs a real temp-materialization mechanism (Store to a fresh local + Loads).
  **⚠ Cross-arch validation caveat:** `cmake-qemu-i386`/`-arm64` emit **native x86_64** (not cross);
  use `cmake-cross/mcc-i386` (ELF32 runs on the host) and `cmake-cross/mcc-arm64` (via
  `qemu-aarch64 -L vendor/gentoo-stage3-arm64-glibc`) for real cross-arch checks.

## NEXT MILESTONE — runtime JIT + guarded deopt (§26)

The design's separate "+ the JIT" work (the "rollout steps 3-5 **+ the JIT**"), deferred behind the
§26 embedded recompiler — entry-guarded variant dispatch with a runtime recompiler + hot-swap.

**Reusable infra (survey).** The `-run` compile-to-executable-memory path (`mcc_run`, `mccrun.c`;
`host_runmem_alloc` RWX / W^X dual-map, `mcchost.c`) + `mcc_relocate` (`mccrun.c`), host==target on
ELF x86_64/arm64/riscv64/arm/i386 + PE + macho; GOT/PLT indirection (`build_got_entries`/
`put_got_entry`, `mccelf.c`) as the per-function hot-swap redirect; `.init_array` ctor emission for a
startup pool; the replayable `ast_cur` (survives the function under `keep_inline`/`keep_reemit`).

**The §26 gap (missing).** (i) the byte-faithful baseline (`orig`/`orig_rel`) is `mcc_free`'d per
function (`mccast.c`) — must be **retained** as the deopt fallback; (ii) intra-module calls are hard
`E8 rel32` (`x86_64-gen.c`) with no swappable slot → the JIT'd function must be an **entry dispatcher**
so call sites need no change; (iii) `--jit-functions`/`--jit-max-duration` are parsed but inert
(`libmcc.c`), `--embed-jit` only prints a manifest + gates the build-time superopt; (iv) `--jit-threads`
and C11 `<threads.h>` do not exist (concurrency is `fork`); (v) the `eval_slice` value-level
equivalence checker does not exist.

**Architecture — the JIT is mostly Strategy objects, not a separate subsystem** ("one code path and
one memo; the opt level is a dial, not a fork"). The compile-time pieces are new rows in the same
`ast_strategies[]` table the search already consumes + scores; only a thin runtime remains.

- [ ] **§26 `jit-dispatch` strategy** — rewrite a function to `{guard; call variant-ptr else baseline}`.
- [ ] **§26 `jit-guard` strategy** — insert the live-in domain check at entry.
- [ ] **§26 `jit-profile` strategy** — insert live-in range-capture instrumentation (also the D5
  hot-counter trigger source).
- [ ] **§26 `jit-patchpoint` strategy** — emit a nop-padded patchable prologue (the code-patch
  hot-swap is a strategy, the D3B variant — not a bespoke mechanism). The optimized variant itself is
  just the existing fold strategies applied.

**Thin runtime (not a compile-time transform):** recompile = re-invoke the strategy engine at runtime
on a hot function (D2A); hot-swap = one atomic pointer store; trigger = `.init_array` ctor or a
`jit-profile` counter threshold. `eval_slice` is the per-strategy soundness gate (Stage 4).

**Staged (each independently gated by a JIT differential):**

- [ ] **Stage 1 — guarded deopt as pure strategies, NO runtime recompiler** — add `jit-dispatch` +
  `jit-guard` to `ast_strategies[]`; together they emit `{guard; AOT-optimized-variant else baseline}`
  entirely at compile time (variant = the existing fold strategies; baseline = the retained faithful
  emit — stop freeing `orig`/`orig_rel`, or re-emit from the kept `ast_cur`). Real guarded deopt,
  differential-validatable, ships **without** §26. This is the key payoff: a first complete version
  needs no runtime recompiler, and §26 shrinks to a stage-2 driver.
- [ ] **Stage 2 — runtime recompile (§26 driver, thin)** — a minimal embedded runtime re-invokes the
  strategy engine (D2A) on a hot function into fresh executable memory via the `mcc_relocate` path, and
  atomically publishes the pointer the dispatcher reads. (implements "§26 hot-function recompile +
  hot-swap" in §2 below)
- [ ] **Stage 3 — profiling + trigger** — add the `jit-profile` strategy; drive recompilation from a
  startup `.init_array` ctor and/or a hot counter; wire `--jit-functions` (which functions get a
  dispatcher) and `--jit-max-duration` (budget). (implements the inert-`--jit-*`-flag items in §2/§1)
- [ ] **Stage 4 — soundness hardening: `eval_slice`** — build `eval_slice(arena, slice, env) ->
  {value, defined}` as a **second, independent AST-over-values interpreter** (NOT a reuse of faithful
  replay), exhaustively checking equivalence over the guarded domain as the per-strategy gate. UB-oracle
  catalog seeded from `gen_opic` (`mccgen.c`): signed +/−/* overflow, div/mod by zero, and critically
  **shift ≥ width modeled as UB** (`gen_opic` currently masks it `l2 & 63|31`, which would otherwise
  certify a wrong rewrite). Enumeration domain from `context_in` (the value-domain lattice above) via
  the `ast_tco_run` live-in pattern; refuse any slice whose context-restricted domain exceeds a fixed
  cap (stays JIT-speculative). Also: a static `context_in` domain to replace the observed range.

**Decisions (settled with the user):** **D1=B** (embedded), **D2=A** (recompile = re-invoke the engine),
**D3=A** (entry dispatcher; the code-patch D3B is the `jit-patchpoint` strategy), **D4=A**
(runtime-observed live-in range). **Remaining:** **D5** trigger (startup ctor vs hot counter — the
counter is `jit-profile`); **D6** `eval_slice` now vs trust-the-AOT-gate + differential; **D7** platform
(ELF x86_64 first); **D8** wire the inert `--jit-*` flags.

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
  designed, not built. (The `slot_key -> branch_tag` naming split it needed is already
  done — `src/mccname.h` `MCC_NS_{AST_SLOT,CST_BRANCH}`, `cst_mark_branch` uses
  `branch_tag`; only the H_e patch itself remains.)
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
