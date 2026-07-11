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

- [ ] **Step 5+ — scoring fidelity: emitted-size / JIT-runtime** — replace the static
  `ast_cost_score` proxy with true emitted-byte size, and at the JIT tier measured
  runtime (`MCC_AST_JITSCORE`). **Deeper blocker than first thought:** `AST_PF_EMIT`
  reads per-candidate global emit state (`ast_ltemp_n`/`ast_ltemp_cur`, the
  `ast_fconst_i`/`ast_locrec_i` cursors, `ast_promo_*`), which the *interleaved tick
  scheduler* overwrites across candidates — so emit-measuring a candidate is only valid
  immediately after that candidate is folded-to-completion, not under the fair
  interleave. Emitted-size therefore needs the per-context fold/emit state below, not
  just §22 scratch-`Section` isolation.

- [ ] **Step 5+ PREREQUISITE — per-context fold/emit state (the common blocker)** — the
  three items below all converge here. The optimizer keeps its working state in ~global
  singletons (`ast_cur`; the `ast_du_*`/`ast_memo_*`/`ast_hash_*` side-cars keyed by one
  arena pointer; `ast_ltemp_*`, `ast_fconst_*`/`ast_locrec_*` cursors; `ast_promo_*`;
  `*_total` counters). Bundling these into a per-context object (or `_Thread_local`
  under C11) is the single change that unblocks: **(1)** emit-size scoring (each
  candidate folds+emits in its own context, no interleave thrash), **(2)** the NCores-1
  thread pool (parallel candidates on independent contexts), and **(3)** any interior
  parallelism. Milestone-scale substrate refactor; do it as its own gated change (the
  side-car shadow build + fixpoint + fuzz are the gates), not bolted onto the search.
- [ ] **Step 5+ — widen the search space** — the fold-gate candidate set is now the
  full **subset lattice** of the enabled gates (submask enumeration, ≤16, base first),
  not just leave-one-out (validated: -O6 differential 80/80, default 1905/1905). Still
  open: the **inline/promote axes** and **budget-scaling the candidate count** (both
  want the emitted-size scoring above, since inline/promote effects are emit-time), and
  a real `est_cost_delta` best-first frontier ordering beyond the current base-first +
  fair-time schedule.
- [ ] **Step 5+ — disk-backed cross-build memo** — the per-function winner now
  persists across builds: `<cachedir>/mcc-search.memo`, append-only records
  `{intention-hash, gates|MAGIC<<8}`, loaded once into `ast_search_memo`, a hit applies
  `cached & base` so a winner cached under a different -O base never enables a gate this
  build disabled; `mcc --clear-cache` wipes it (validated: cross-invocation memo hit +
  -O6 differential correct). Still open: **unify with the out-of-process `pf-*.ck`
  format** (`so_pf_key`) so the in-process search fully subsumes `mcc_superopt_perfn`,
  and compact the append-only file instead of the current ~64K-record size cap.
- [ ] **Step 5+ — NCores-1 coroutine thread pool** — stackless `step()` strategies on a
  C11 pool (optional, confined to -O4+/JIT; disabled under `__STDC_NO_THREADS__`).
  Blocked on the per-context fold/emit state prerequisite above (parallel candidates need
  independent contexts, not one global set of side-cars/cursors).
- [ ] **Step 5+ — runtime JIT + guarded deopt** — the -O4+/JIT tier (per-tick drive,
  entry-guarded variant dispatch); depends on §26 embedded recompiler + hot-swap.
  **Blocked:** there is no runtime recompiler (`mcc_relocate` is a one-shot `-run`
  loader; `--embed-jit` only prints a manifest) — §26 must land first.

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
- [ ] **Widen the §23 inliner budgets** — bigger node/graft/depth (current
  `ast_graft_budget_max = 2048`, `AST_INLINE_MAX_DEPTH 8`). Byte-identity-gated,
  then registered as a §22 search knob. (§23 step 1)
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
  noise) — add `MCC_TRACE` at points of interest as needed; (c) wiring `MCC_CONFIG_TRACE`
  into a preset is deliberately skipped (the release-inherits-debug caveat that applies
  to `MCC_CONFIG_AST_SHADOW`); (d) migrate ad-hoc `if (verbose) fprintf(stderr,...)`
  sites to the tagged `mcc_logf`/`MCC_DEBUG` categories.
- [ ] **Ungate the `i386-fastcall-abi` test** — registered but `mcc_skip_test`'d;
  needs an i386 cross + an ELF-32 reference.
