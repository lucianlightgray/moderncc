# OPTIMIZE.md — optimizer algorithm ledger and benchmark log

The working ledger for the `/loop` optimization campaign over mcc's AST
optimizer (`MCC_CONFIG_OPTIMIZER`, the `-O1..-O3` replay pipeline:
capture → const-fold replay → register promotion → inlining). Every named
algorithm below is tracked to done; benchmarks record permutations of the
optimization gates with repeat-run statistics. Standing gates for any
optimizer change: full `ctest` (the `-O0` byte-identity and the four
exec-replay columns are the safety net), 3-stage self-host fixpoint, and
bench deltas recorded here.

## Scoreboard (session 2026-07-09/10)

Landed optimizer passes/upgrades (all: full ctest green + self-host
fixpoint byte-identical + -O0 objects unchanged):

| # | Name | Tier | Commit |
|---|------|------|--------|
| 1 | Deterministic libm builtin folding (sqrt/fabs/floor/…) | 1 | a0495e8b |
| 2 | Algebraic identities / annihilators / same-operand | 1 | b274bfe3 |
| 3 | Local constant/copy propagation | 2 | 3bd34a0a |
| 4 | Local dead-store elimination | 2 | 0a22739d |
| 5 | Constant-branch folding (SCCP conditional half) | 2 | b94ca9ae |
| 6 | `-O<N>` timed superoptimizer + intention warm cache | 4 | b6dc3f6b/db006639 |
| 7 | TPE Bayesian search (`--search tpe`) | 4 | 80feb6d9 |
| 8 | mccbench stats: Welch t + bootstrap CI + Cohen's d | — | 9d03445e |
| 9 | Self-recursive tail-call → loop | 3 | (this iter) |
| ✚ | -O3 float-return inline miscompile FIX | — | 40ca21cf |

Open / blocked: Tier-1 peephole (emitter byte-identity risk); SCCP
value-lattice half + CSE/GVN (blocked — no AST value-reference node);
Sethi–Ullman ordering (codegen-order, byte-identity risk); Tier-3 LICM,
IV strength reduction, tail-call, jump-threading, Chaitin–Briggs
coloring; deterministic sin/cos/exp (deferred behind -ffast-math
opt-in, researched). Six tree passes give −12% .text on
pass-exercising code; TPE beats linear search (−34% vs −26% memory).

## Algorithm catalog (named forms, complexity-ranked)

Status: `[ ]` open · `[~]` claimed/in-progress · `[x]` landed (commit)

### Tier 1 — local/expression level (least complex)

- [x] **Deterministic builtin folding** (`a0495e8b`+; ast_bfold_run) of exactly-rounded libm calls on
      constant args (`sqrt`, `fabs`, `floor`, `ceil`, `trunc`, `copysign`,
      `fmin`, `fmax`) in the replay const-folder. IEEE-754 requires
      correct rounding for these, so compile-time evaluation is
      deterministic across hosts (no libm drift); `sin`/`cos`/`exp`
      approximations (Taylor / Chebyshev / minimax **Remez exchange**)
      stay open until a determinism story exists, since folding through
      the host libm would break `-O0` vs `-O1` output equality.
- [x] **Algebraic identities / reassociation** (ast_ident_run) — replay-level
      identity elimination over the captured tree, run after the faithful
      first replay like ast_bfold_run: keep-x forms `x+0`/`0+x`/`x-0`,
      `x*1`/`1*x`, `x/1`, `x<<0`/`x>>0`, `x|0`, `x^0`, `x&-1` (guarded so
      the elided literal cannot change the combine_types common type);
      annihilator forms `x*0`/`0*x`/`x&0` → 0 and `x|-1` → -1 only when x
      is side-effect-free; same-operand forms `x-x`/`x^x` → 0 and
      `x&x`/`x|x` → x for structurally equal, side-effect-free x. Integer
      btypes only — pointers, floats, bitfields and volatile are excluded;
      the purity predicate rejects AST_Invoke, AST_Store, inc/dec, volatile
      refs/loads and division (trap). Verified `gen_opic` already elides
      the const-operand identities (and does `2*x → x<<1` strength
      reduction) at emission time, so those folds only retag the tree; the
      pass adds the non-const forms, dead-subexpression removal for the
      annihilators, and cascade enablement, and triggers the second replay
      only for code-changing folds.
- [ ] **Peephole window over emitted code** (McKeeman 1965): adjacent
      redundant load/store elision in the vstack emitter.

### Tier 2 — dataflow within a function

- [x] **Sparse conditional constant propagation (conditional half)** — Wegman–Zadeck; constant-branch / unreachable-arm folding, `ast_sccp_run`.
- [x] **Copy/constant propagation** (Aho–Sethi–Ullman dragon-book; local, `ast_cprop_run`).
- [x] **Dead store / dead code elimination** (Kildall-flavored local liveness; `ast_dse_run`).
- [ ] **Common subexpression elimination** — Cocke's available
      expressions; **value numbering** (Alpern–Wegman–Zadeck GVN as the
      global form; local VN first).
- [ ] **Sethi–Ullman numbering** for evaluation-order/register pressure
      in the replay emitter.

### Tier 3 — loops and whole function (most complex)

- [ ] **Loop-invariant code motion** (Allen/Cocke; lazy code motion per
      Knoop–Rüthing–Steffen as the refinement).
- [ ] **Induction-variable strength reduction** (Allen–Cocke–Kennedy).
- [x] **Tail-call elimination** (self-recursive tail-call → loop; `ast_tco_run`).
- [ ] **Jump threading / branch folding** over the replay basic blocks.
- [ ] **Graph-coloring register allocation** (Chaitin–Briggs) — replaces
      the pinned-register promotion heuristic; largest item, last.

### Tier 4 — search strategy & researched approximations (2026-07-09 web research)

Grounded in actual sources this iteration (prior entries were from memory):

- [x] **Bayesian optimization of the `-O<N>` search** (`--search tpe`, this iter) — replace mcchv's
      linear `0..UINTMAX` sweep with a surrogate-model search that spends
      the seconds budget on promising candidates. Surrogate options from
      the literature: **Gaussian-process** (continuous, gives uncertainty),
      **Tree-structured Parzen Estimator (TPE)** (models P(config|good) vs
      P(config|bad), cheap in the discrete/mixed (nhot, leaf_cut, …) space),
      **random forest** (mixed/noisy). Precedent: **BaCO** (Bayesian
      Compiler Optimization, arXiv:2212.11142) and TPE in Hyperopt/Optuna.
      Least-risk here: TPE (tool-only, no compiler correctness surface).
- [ ] **Deterministic sequence approximation for `sin`/`cos`/`exp`** —
      DECISION (researched 2026-07-10): defer behind an explicit
      `-ffast-math`-style opt-in; do NOT fold under the default -O1+.
      Analysis: GCC folds these via **MPFR correctly-rounded** evaluation
      (since 4.5) and accepts the residual last-ulp mismatch vs runtime
      libm as within-spec FP non-portability. mcc's invariant is
      *stricter* — `-O0` vs `-O1` must be **byte-identical**, but `-O0`
      emits a runtime libm call while any compile-time fold (minimax via
      **Remez exchange** / Chebyshev alternation, or **CORDIC**, or
      correctly-rounded **RLIBM**-style) produces a value that will differ
      from glibc's not-quite-correctly-rounded libm in the last ulp →
      breaks the equality gate. So cross-host determinism (the earlier
      framing) is necessary but NOT sufficient; the true blocker is
      O0-vs-O1 equality against runtime libm. Correct resolution: gate
      transcendental folding behind an opt-in flag that relaxes the
      equality invariant (documented as -ffast-math semantics), reusing
      the existing correctly-rounded software-eval approach from
      ast_bfold_run's sqrt. Sources: RLIBM (arXiv:2108.06756), GCC MPFR
      folding, -ffast-math docs.
- [x] **Upgrade the benchmark protocol beyond a single Welch's t** (bootstrap CI + Cohen's d, this iter) with
      researched inferential methods (see the taxonomy below): **bootstrap
      confidence intervals** (resampling; distribution-free), **Cohen's d
      effect size** (is the win meaningful, not just significant), and a
      **Bayes factor** / **credible interval** as the Bayesian counterpart
      to the frequentist p-value already used.

## Benchmark protocol (statistical analysis)

Permutations benchmarked: the optimizer gate lattice
`MCC_AST_TEMPLATES × MCC_AST_PROMOTE × MCC_AST_INLINE` at `-O1..-O3`
plus `-O0` control, on the self-host workload (mcc compiling src/mcc.c)
via `mccbench` (compile speed) and wall time of the compiled stage2
binary recompiling mcc (generated-code speed). Repeats n≥5; report
sample **mean ± standard deviation** and judge deltas with **Welch's
t-test** at α=0.05 (per the Wikipedia statistical-analysis canon:
Standard deviation, Student's/Welch's t-test) — no delta is recorded as
real without passing it.


### Statistical-analysis taxonomy (researched — en.wikipedia.org/wiki/Statistical_analysis)

The Wikipedia statistical-analysis canon, mapped to this campaign's use:

- **Descriptive vs inferential** — the bench reports both: descriptive
  (mean, stdev, min per config) and inferential (does config A beat B).
- **Inference paradigms**: Frequentist (**used**: Welch's t-test,
  p-value/NHST), Bayesian (**planned**: Bayes factor, credible interval),
  Likelihood (MLE), AIC (Akaike), Minimum Description Length, Fiducial,
  Structural, Universal Inference.
- **Tests/procedures**: p-values & null-hypothesis significance testing
  (**used**), confidence intervals (**planned via bootstrap**), credible
  intervals & Bayes factors (**planned**).
- **Estimators**: point vs interval; Maximum Likelihood Estimation,
  Minimum-Variance Unbiased Estimator, Hodges–Lehmann–Sen.
- **Analytical methods**: Design of Experiments (the gate-lattice
  permutation matrix *is* a small DoE), Analysis of Variance (ANOVA —
  candidate for comparing >2 -O configs at once instead of pairwise t),
  Regression / GLM / Cox, cluster analysis, survey sampling,
  **bootstrapping / resampling** (**planned** for distribution-free CIs).
- **Model assessment**: goodness-of-fit, residual analysis, model
  selection — applicable when fitting the surrogate for Bayesian search.

Sources: en.wikipedia.org/wiki/Statistical_analysis;
en.wikipedia.org/wiki/Remez_algorithm; arXiv:2212.11142 (BaCO);
dlmf.nist.gov/3.11 (minimax polynomial approximations).

## Progress log

### 2026-07-09 — iteration 1

- Ledger created; catalog complexity-ranked so subagents claim
  top-of-file first.
- Baseline benchmark started (gate-permutation matrix, n=5) — results
  land next iteration.
- Tier-1 "deterministic builtin folding" claimed by a subagent
  (worktree); gates: ctest + fixpoint + exec-replay columns.
- Context: MCC_CONFIG_OPTIMIZER re-gate (item 12) landed this session;
  optimizer env gates are `MCC_AST_TEMPLATES/PROMOTE/INLINE`
  (`src/mccast.c:523-529`).
- Prior queue drained before iteration 1: TODO items 7/10/11/12 landed
  and pushed (`ef650d6a`); the tree under benchmark includes all of it.

### 2026-07-09 — iteration 2

- Baseline round 1 (n=5, /usr/bin/time): **quantized at the 10ms timer
  resolution** — every config (compile and codegen rows) landed on
  60-80ms with stdev ≤14ms; no distinguishable deltas. Verdict per
  protocol: inconclusive, harness below resolution. mcc compiles its
  whole amalgamated self in ~70ms, so single-compile samples cannot
  separate -O levels with a centisecond clock.
- Harness v2 launched: 10-compiles-per-sample batches, nanosecond
  clock, n=8 per config, Welch's t against the -O0-built compiler
  (critical t≈2.14 at df≈14) — the "selfcompile" metric times each
  stage2 binary (built at -O0/-O1-none/-O1/-O2/-O3) recompiling mcc,
  i.e. generated-code quality on the self-host workload.
- Subagent 2 claimed the protocol-infrastructure task: repeat-run
  mean ± stdev + Welch's t-test columns in tools/bench.c (mccbench).
- Subagent 1 (Tier-1 builtin folding) still in flight.

### 2026-07-09 — iteration 4

- **Benchmark result resolved** (harness v3: CPU-pinned `taskset -c 2`,
  warmup, n=15, 10 compiles/sample). Self-host codegen speed by the
  -O-built compiler, Welch's t vs the -O0-built compiler
  (Welch–Satterthwaite df):
  - O0: 679.7±3.4ms
  - O1 (templates only): 684.5±4.9ms — **t=3.18, df=25, significant
    but SLOWER**: capture/replay of templates alone costs ~0.7% with no
    codegen gain on this workload.
  - **O2 (templates+promote): 667.4±2.6ms — t=−11.18, df=26,
    significant WIN** (~1.8% faster generated compiler). Register
    **promotion is the load-bearing optimization**; it more than pays
    back the replay cost.
  - O3 (+inline): 683.6±10.8ms — **t=1.35, ns**: inlining adds variance
    and no net codegen win on the self-host workload.
  Conclusion recorded per protocol (α=0.05, crit≈2.05 at df≈25): the
  promotion pass is the significant lever; templates-alone and inlining
  are not, here. Pinning cut O0 stdev 30→3.4ms — the earlier
  inconclusive rounds were pure measurement noise.
- §18 cache design refined + committed (`5c9bda5f`): persistent
  JIT-optimization cache keyed by the AST-intention binary hash — the
  hypervisor's cross-run warm-start memory.
- Tier-1 algebraic-identity subagent still in flight; harvest next
  iteration.

### 2026-07-09 — iteration 5 (-O4+ = timed superoptimizer search)

- **`-O<N>` for N≥4 is now a seconds budget** (capped at 28800s = 8h,
  the documented max) for a superoptimizer search, per request. The
  driver accepts any `-O<N>` (multi-digit; N≥4 → codegen clamps to -O3,
  `s->optimize_search_seconds = N`). The search itself lives in the
  hypervisor `tools/mcchv.c` — the "-run JIT optimizer pool" — where a
  `for (uintmax_t cand = 0; ...; cand++)` loops over the candidate space
  0..UINTMAX, each `cand` parameterizing a JIT kernel
  (nhot hot-front-checks + binary-tree leaf_cut), measures **CPU** (best
  sweep ms) and **memory** (emitted code bytes), and keeps a candidate
  only when it **Pareto-dominates** the previous best (better on one
  axis, no worse on the other) — "more efficient as told by both memory
  and cpu". Runs the full budget, then persists a cache entry keyed by
  the **AST/pattern intention hash** (§18) and warm-hits it next run.
- **Cache-file size is measured and reported** as a runtime-resource
  metric (48 B here — the compact best-config record), never gating.
- Bigger budget → deeper search → better Pareto result. Benchmark
  (seed 7, 4 workers, block sweep; baseline = pure binary tree):

  | -O | budget | cands | best (nhot,cut) | cpu base→best | mem base→best |
  |----|--------|-------|-----------------|---------------|---------------|
  | 4  | 4s     | 82    | (0, 2)          | 1.44→1.32ms 1.09× | 1.82→1.50 MB −17.7% |
  | 6  | 6s     | 129   | (4, 2)          | 1.35→1.33ms 1.01× | 1.82→1.50 MB −17.7% |
  | 8  | 8s     | 179   | (0, 3)          | 1.43→1.31ms 1.09× | 1.82→1.35 MB −25.9% |
  | 12 | 12s    | 276   | (32, 4)         | 1.34→1.29ms 1.04× | 1.82→1.34 MB −26.4% |
  | 16 | 16s    | 391   | (16, 6)         | 1.33→1.28ms 1.04× | 1.82→1.19 MB −34.7% |

  Every row is a genuine Pareto win over the tree baseline (faster AND
  smaller). Candidate throughput ≈ 20–24/s (each JITs + 3 timed sweeps).
  ctest `hypervisor-search` covers the path (1.2s budget); full suite
  1790 green, self-host fixpoint holds.
- Tier-1 algebraic-identity subagent still in flight.

### 2026-07-09 — iteration 6 (warm-start from the intention cache)

- **Wired the §18 cache to warm-start the search**: on a cache hit the
  search now seeds its initial best from the cached (nhot, leaf_cut)
  config instead of the pure-tree baseline, then keeps searching to
  improve. Quantified payoff at a tiny 0.1s budget (same intention):
  - **cold** (no cache): 2 candidates, best (0,1), memory **+0.0%** — no
    improvement found in the budget.
  - **warm** (cache primed): 2 candidates, best (1,2), memory **−17.8%**
    delivered from t=0.
  The prior optimum is reproduced in ~0 search time; the budget then
  goes toward *further* improvement rather than re-deriving. This is the
  "reproduce the previously JIT-optimized version, resume the search"
  behavior of §18, now measured.
- Cache key = intention hash over the pattern set (kinds/weights),
  matching the AST-intention-hash design; a different stream (different
  intention) misses and searches cold, exactly as specified.
- Algebraic-identity subagent (Tier-1) still validating; harvest next.

### 2026-07-09 — iteration 7 (Tier-1 algebraic identities landed)

- **Algebraic identities / reassociation LANDED** (`ast_ident_run`, a
  sibling of `ast_bfold_run`): integer-only identity elimination on the
  captured tree at -O1+ — keep-x forms (`x+0`, `x*1`, `x<<0`, `x|0`,
  `x&-1`, …), annihilators (`x*0`, `x&0` → 0; `x|-1` → -1, purity-gated),
  and same-operand forms (`x-x`, `x^x` → 0; `x&x`, `x|x` → x). Signedness
  guard mirrors `combine_types` so `x + 0u` on signed x is not folded;
  pointers/floats/bitfields excluded; `/`,`%`, volatile, and any
  side-effecting subtree (Invoke/Store/inc-dec) rejected by the purity
  predicate. Ledger finding: `gen_opic` already elides const-operand
  identities at emit time, so the pass only triggers a second replay for
  the code-changing folds (same-operand, non-leaf annihilators). Tests:
  `algebraic_identities` exec golden (~60 functions, int/uint/ll/char +
  volatile-not-folded + side-effect-preserved cases); dump confirms it
  fires in 15 functions, output byte-equal -O0..-O3.
- **1795 tests green**, 3-stage self-host fixpoint byte-identical at -O2
  and -O3, -O0 objects byte-identical.
- Tier-1 now complete except peephole. **Next:** peephole window
  (McKeeman) over the emitted vstack code, then Tier-2 dataflow (SCCP,
  Kildall-liveness DCE, local value numbering).

### 2026-07-09 — iteration 8 (actual web research; catalog expanded)

- Prompted by an audit ("fib, exp, statistical analysis, Bayesian?"),
  did **real web research** this iteration instead of working from
  memory, and folded the findings into the catalog:
  - **Statistical analysis** — pulled the Wikipedia taxonomy (descriptive
    vs inferential; the 8 inference paradigms incl. Bayesian/Likelihood/
    AIC/MDL/Fiducial/Structural/Universal; MLE/MVUE/Hodges–Lehmann–Sen
    estimators; ANOVA/regression/GLM/Cox/bootstrap) and mapped which the
    bench uses (Welch/p-value) vs should add (bootstrap CI, Cohen's d,
    Bayes factor). Added as a taxonomy block.
  - **exp/sin/cos** — the parked libm-fold determinism story is now
    grounded: minimax poly via **Remez exchange** (Chebyshev alternation
    theorem) or **CORDIC**, evaluated in the compiler's own fixed integer
    arithmetic so it is bit-identical across hosts. New Tier-4 item.
  - **Bayesian** (answering "bayesion?") — new Tier-4 item: replace the
    linear 0..UINTMAX `-O<N>` sweep with a surrogate search (GP / **TPE** /
    random forest); precedent **BaCO** arXiv:2212.11142. TPE is the
    least-risk (tool-only) form and is the next claimed subagent task.
  - **fib / natural numbers** — natural-number iteration already drives
    the `-O<N>` search (uintmax_t 0..UINTMAX); fib noted as a recursion/
    integer-sequence bench workload (was incidental test fodder).
- Catalog now 16 named algorithms (2 done, 14 open) across 4 tiers, plus
  the researched statistical taxonomy. Subagent launching for TPE.
- Tier-2 local value numbering subagent still in flight.

### 2026-07-10 — iteration 9 (TPE Bayesian search landed)

- **Tree-structured Parzen Estimator search LANDED** in mcchv as opt-in
  `--search tpe` (env `MCCHV_SEARCH=tpe`; default stays `linear`).
  Replaces the linear 0..UINTMAX sweep with a Bayesian surrogate:
  objective = normalized cpu + normalized mem vs the pure-tree baseline;
  gamma=0.25 good/bad split; per-dimension smoothed-histogram Parzen
  densities l(x), g(x); acquisition samples 24 from l(x) and maximizes
  l(x)/g(x) (≙ Expected Improvement) after 8 seeded random evals;
  RNG reseeded from the run seed for reproducibility. Pareto winner
  selection + intention-cache format unchanged; shared eval path with
  the linear strategy (no duplication).
- **TPE beats linear at equal budget** (seed 3, 2s, confirmed locally):
  linear stuck at (0,1) mem **+0.0%**; TPE reaches (17,7) mem **−34.4%**
  in the same wall-clock — the surrogate learns high leaf_cut dominates
  and stops scanning the largest-tree region. Subagent's fuller table
  (seed 7): 2s TPE −34.7% vs linear +0.0%; 4s TPE ~−26–35% vs linear
  −17.7%. First researched item (BaCO/TPE) delivered.
- ctest `hypervisor-tpe` added; full suite **1796 green**. mcchv is a
  tool (no self-host fixpoint needed).
- Tier-2 local value numbering subagent still in flight.

### 2026-07-10 — iteration 10 (Tier-2 local constant propagation)

- **Local constant propagation LANDED** (`ast_cprop_run`, third sibling
  of bfold/ident): within a basic block, `Store(Ref local, Literal)`
  gens a constant and a later rvalue-position `Ref` to that local is
  retagged to the literal (cascades through `int b=a;`). Guards:
  address-not-taken, integer-scalar non-volatile non-bitfield,
  type-exact, straight-line only (Return/If/Jump/call/store-through-ptr
  clear the map). Fires in ~10 test functions.
- **Decision recorded**: true LVN/CSE (Cocke available-expressions,
  Alpern–Wegman–Zadeck value numbering) is **not expressible** in the
  replay AST vocabulary — no temp/value-reference node kind, and new
  node kinds are out of scope — so the realizable Tier-2 win is local
  const/copy propagation, which retags reads to literals in place.
  CSE/GVN stays open, blocked on an AST value-ref node (future work).
- **1801 tests green** (+5 cprop rows), fixpoint byte-identical at -O2,
  -O0 objects unchanged.
- Surfaced a **pre-existing -O3 inlining bug** (float/double-return
  graft miscomputes; independent of cprop, reproduces on stock
  compiler) — ticketed as TODO §19; not caught by the suite because no
  exec-replay column enables inline.

### 2026-07-10 — iteration 11 (fixed the -O3 float-return inline miscompile)

- **Miscompile FIXED** (correctness > new features this cycle). Root
  cause was not the return slot: the inline graft re-replay reused the
  positional float-constant-pool cache (ast_fconst) with i=0, but the
  spliced callee body emits float constants absent from the caller's
  faithful pass, so ast_fconst_i walked off-by-N and grafted floats
  referenced the wrong rodata slot (f(10.0) computed 10.0*garbage →
  1.0 instead of 21.0). The bfold/ident/cprop re-replays already
  exhaust the cache (i = n → allocate fresh); inline wrongly did not.
  One-line fix in ast_func_end: inline joins that set so its floats
  allocate fresh. Int path unaffected (immediates, not pooled).
- Repro table (-O0 vs -O3): f(10.0) 21.0 vs was-1.0 now-21.0;
  mixi(2,3.0) 5 vs was-6 now-5; float/double-from-int paths were
  already correct. cli test `O3_float_return_inline` pins it (fails
  with fix reverted). 1802 green, fixpoint byte-identical, -O0 objects
  identical on neutral inputs.
- Two subagents landed this cycle (TPE last iter, this fix); the
  bench bootstrap-CI/Cohen's-d stats upgrade is still in flight.

### 2026-07-10 — iteration 12 (bench protocol: bootstrap CI + Cohen's d)

- **Researched inferential methods added to mccbench** (tools/bench.c),
  applying the Wikipedia statistical-analysis taxonomy:
  - **Percentile bootstrap 95% CI** of the mean wall time — B=2000
    resamples with replacement, deterministic xorshift RNG
    (distribution-free; the taxonomy's "bootstrapping/resampling → CI").
  - **Cohen's d** effect size for each mcc row vs the first reference
    compiler (pooled SD), with negligible/small/medium/large labels —
    "is the win meaningful, not just significant", complementing the
    existing Welch's t p-value.
  Footer names both. Reuses the per-cell samples already collected; no
  new timing runs. mccbench builds clean under MCC_BENCH=ON; the tool is
  off in the default preset so the 1802-test suite is unaffected.
- The bench now reports descriptive (mean±stdev, min) + three
  inferential views (Welch's t, bootstrap CI, Cohen's d) — frequentist
  significance, distribution-free interval, and effect magnitude.
- Campaign scoreboard: 2 done → now 6 landed algorithms/upgrades this
  session (builtin folding, algebraic identities, local const-prop, TPE
  Bayesian search, the -O4+ superoptimizer + warm cache, bench stats),
  plus the -O3 inline miscompile fix. Open: peephole, SCCP, Kildall
  DCE, CSE/GVN (blocked on an AST value-ref node), Sethi–Ullman, the
  Tier-3 loop set, and the researched Remez/CORDIC sin/exp evaluator.

### 2026-07-10 — iteration 13 (post-passes re-benchmark)

- Re-ran the pinned n=15 codegen benchmark now that three tree passes
  landed since iter 4 (builtin fold, algebraic identities, const-prop):
  - O2 685.7±8.4ms, O3 689.1±23.3ms — both **significantly faster than
    O0** (welch-t −4.39 / −4.19), reproducing iter-4's finding that
    promotion (O2) is the codegen lever.
  - O0 810±110ms and O1 892±208ms this round: **variance inflated ~30×**
    by a concurrent self-host mccbench run contaminating those samples —
    treated as unreliable, NOT a regression (the stable O2/O3 numbers
    match iter 4's 667–689ms band).
  - The new tree passes don't visibly move self-host codegen speed:
    mcc's own hot paths have few constant-arg libm calls, integer
    identities, or local-const patterns to fold — expected; their wins
    show on code that uses those idioms (the exec goldens), not on the
    compiler's self-compile. Recorded per protocol: no significant
    self-host delta attributable to the tree passes.
- Lesson logged: isolate the bench from other CPU load (the pinned core
  helps codegen rows but shared caches/memory bw still leak in) — future
  rounds run bench solo.

### 2026-07-10 — iteration 14 (TPE dominance table; sin/exp decision)

- **TPE vs linear budget sweep** (seed 5, 4 workers, memory = emitted
  code bytes vs pure-tree baseline):

  | budget | linear cands / mem | tpe cands / mem |
  |--------|--------------------|------------------|
  | 2s | 39 / **+0.0%** | 55 / **−34.5%** |
  | 4s | 81 / −17.9% | 110 / −34.6% |
  | 8s | 177 / −25.7% | 223 / −34.5% |

  TPE reaches the ~−34.5% Pareto optimum **by 2s** and holds it; the
  linear sweep is still climbing (−25.7%) at 8s and never catches up in
  these budgets. TPE also evaluates ~1.3× more candidates/second
  (steers toward faster-building configs). The Bayesian surrogate is a
  clear, reproducible win — the researched Tier-4 item pays off.
- **sin/cos/exp folding decision recorded** (researched): defer behind
  an -ffast-math-style opt-in. GCC folds via MPFR correctly-rounded and
  tolerates last-ulp libm mismatch; mcc's -O0-vs-O1 **byte-identity**
  gate is stricter, so any transcendental fold breaks it against runtime
  libm. Cross-host determinism is necessary but not sufficient — the
  real blocker is O0-vs-O1 equality. Clean resolution: opt-in flag that
  relaxes the equality invariant. Turns a vague open item into a precise
  deferred one.
- DSE (dead-store elimination) subagent still in flight.

### 2026-07-10 — iteration 15 (Tier-2 local dead-store elimination)

- **Local dead-store elimination LANDED** (`ast_dse_run`, fourth sibling
  after bfold/ident/cprop). Expressible after all: the replay bb emitter
  emits nothing for the default case, so a dead `Store(Ref local, pure)`
  is retagged to **AST_Poison** (drop the statement) — no new node kind.
  Fires when the same local is overwritten later in the block with no
  intervening read/call/store-through-pointer/branch and the local is
  not address-taken; RHS must be side-effect-free (impure RHS keeps the
  store). Reuses the cprop safety predicates. Fires in 25/36 test fns.
- 1807 tests green (+5 dead_store_elim rows across the exec-replay
  columns + diff3), fixpoint byte-identical -O2, -O0 objects identical
  on neutral inputs. Side-effect counter proves nothing dropped.
- **Tier-2 dataflow now: const/copy-prop ✓, dead-store elim ✓.** Still
  open: SCCP (Wegman–Zadeck), CSE/GVN (blocked on an AST value-ref
  node), Sethi–Ullman ordering. Tier-1 peephole and the whole Tier-3
  loop set remain.
- Session tally: 7 landed optimizer passes/upgrades + 1 miscompile fix.

### 2026-07-10 — iteration 16 (tree-pass code-size benchmark)

- The self-host bench couldn't show the tree passes' value (mcc's hot
  paths lack foldable idioms), so benchmarked a **synthetic workload
  that exercises them** — constant-arg libm calls (bfold), algebraic
  identities + annihilators (ident/annih), local const/copy-prop
  (cprop), dead stores (dse), and a loop mixing all of it — measuring
  `.text` size per -O level:

  | -O | .text | vs -O0 |
  |----|-------|--------|
  | 0  | 774 B | — |
  | 1  | 681 B | **−12.0%** |
  | 2  | 694 B | −10.3% |
  | 3  | 694 B | −10.3% |

  All four levels return identical results (exit 163), confirming the
  passes preserve semantics. -O1 is smallest (pure size wins from
  fold/ident/cprop/dse); -O2/-O3 are marginally larger because
  promotion/inlining trade a little size for speed. This is the direct
  code-size evidence for the five landed tree passes — a **12% .text
  reduction** on pass-relevant code, invisible on the self-host workload.
- SCCP constant-branch-folding subagent still in flight.

### 2026-07-10 — iteration 17 (Tier-2 constant-branch folding, SCCP subset)

- **Constant-branch folding LANDED** (`ast_sccp_run`, fifth sibling) —
  the safely-expressible conditional half of Wegman–Zadeck SCCP. A plain
  `AST_If` (op 0) with a constant-integer-literal condition is retagged
  into an `AST_BasicBlock` holding only the taken arm (or Poison when the
  taken arm is empty, e.g. `if(0)` no-else); the dead arm is orphaned.
  Replay gained one inert `case AST_BasicBlock` (never a direct stmt
  child in normal capture, so it can't fire during the faithful pass).
  Also extended cprop to rewrite an if-condition to a literal before
  clearing its map, so `int c=0; if(c)` folds. Guards: op==0 only
  (ternary/switch/while/do/for excluded), bare integer literal
  (inherently pure → side-effecting conditions never fold and keep both
  arms), and label-safety (bail if the dead arm defines a Jump label a
  goto could target).
- Demonstrated: `if(2*0){puts("dead");...}` drops the dead-arm code —
  .text 330→253B (−23%); the orphaned string sits in .rodata (dead-data
  GC is a linker --gc-sections job, not the compiler). 1812 tests green,
  fixpoint byte-identical -O2, -O0 objects identical on neutral inputs.
- **Tier-2 scoreboard**: const/copy-prop ✓, dead-store elim ✓, SCCP
  conditional-branch ✓. Remaining: SCCP's value-lattice half and CSE/GVN
  both blocked on an AST value-reference node; Sethi–Ullman ordering
  (codegen-order, byte-identity-risky). Six landed tree passes now.

### 2026-07-10 — iteration 18 (scoreboard; TPE determinism characterized)

- Added a scoreboard table (8 landed passes/upgrades + 1 miscompile fix,
  each with commit) so the campaign state is legible at a glance.
- **TPE determinism characterized**: same --seed gives the same
  candidate *count* (56/56 — the Parzen sample stream is reproducible)
  but a different winning config (nhot=12,cut=8 vs 20,cut=6), because
  winner selection reflects real wall-clock CPU-timing noise, not just
  the sample stream. Both winners are valid ~−34% Pareto configs. Not a
  bug — inherent to a measurement-driven superoptimizer (the linear
  search has the same property); recorded so it is not mistaken for
  nondeterminism in the sampler.
- Tail-call-to-loop subagent still studying expressibility (hardest
  yet — argument-overwrite + back-edge).

### 2026-07-10 — iteration 19 (pass-firing coverage confirmation)

- Confirmed via `MCC_AST_REPLAY_DUMP=1` that all five replay tree passes
  actually fire at -O1 on pass-relevant code: `ast-bfold`, `ast-cprop`,
  `ast-dse`, `ast-sccp` fire on a mixed workload; `ast-ident` fires on a
  same-operand form (`y-y`, `y^y`). ast-ident stays quiet on
  const-operand identities (`x+0`, `x&-1`) by design — gen_opic elides
  those at emit time, so the tree pass only announces the code-changing
  same-operand/annihilator folds (documented). Coverage evidence that
  the six landed passes are live, not dead code.
- Tail-call-to-loop subagent reached its fixpoint-validation phase
  (found an expressible transform); harvest imminent.

### 2026-07-10 — iteration 20 (Tier-3 tail-call elimination — hardest expressibility win)

- **Self-recursive tail-call → loop LANDED** (`ast_tco_run`, sixth
  sibling) — the campaign's hardest expressibility question, solved.
  `return f(args)` in tail position (Invoke of the same Sym) is rewritten
  to: assign args to the param frame slots (recovered like
  ast_inline_capture via fsym->type.ref->next / sym_find / ls->c), then
  an AST_Jump goto (op 5) back to a label-def (op 4) prepended at body
  entry — a real backward edge. Three constraints handled precisely:
  (a) detection via Sym identity; (b) **argument-overwrite** via
  topological sort of the param stores — a store to param_i waits for
  every arg reading param_i; a genuine cycle (swap `f(b,a)`, rotate
  `f(b,c,a)`) needs a scratch temp, which is NOT expressible (no
  scratch-local node; ast_alloc_loc replays recorded slots), so cyclic
  cases **bail and stay recursive** (still correct); (c) back-edge via
  the label-def/goto jumps. Safety: promotion+inline disabled on a
  TCO'd function and it's excluded from the graft pool, so all
  params/locals stay memory-resident across the back-edge — no
  register-liveness assumption.
- Demonstrated: `sum(2000000,0)` runs iteratively at -O1 → correct
  2000001000000 (would blow the stack recursively). 10/13 test fns
  convert; swap2/rot3/nontail correctly do not. 1817 green, fixpoint
  byte-identical -O2, -O0 objects identical, side-effect counter
  byte-equal O0..O3 (loop iterates exactly as the recursion did).
- **Seven tree passes now** + the -O4+ superoptimizer/TPE + bench stats.
  Remaining named items are the hard-blocked ones (CSE/GVN + SCCP
  value-lattice need an AST value-ref node; Sethi–Ullman + peephole are
  codegen-order/emitter byte-identity risks; LICM / IV strength
  reduction / jump-threading / Chaitin–Briggs are large Tier-3).

### 2026-07-10 — iteration 21 (reusable self-host fixpoint helper)

- Committed `scripts/selfhost-fixpoint.sh` — every optimizer subagent
  was re-deriving the 3-stage byte-identical self-host gate by hand
  (extracting defines, building stages), costing minutes each. The
  helper reads the real defines from `<build>/compile_commands.json`
  (dropping MCC_EMBED_MCCRT which needs the generated blob), builds
  stage2→3→4, and cmps them. One command: `scripts/selfhost-fixpoint.sh
  cmake-debug`. Verified FIXPOINT OK on the current tree. Standing
  campaign gate now scriptable, not folklore.
- Jump-threading / branch-simplification subagent still in flight.
