# OPTIMIZE.md — optimizer algorithm ledger and benchmark log

The working ledger for the `/loop` optimization campaign over mcc's AST
optimizer (`MCC_CONFIG_OPTIMIZER`, the `-O1..-O3` replay pipeline:
capture → const-fold replay → register promotion → inlining). Every named
algorithm below is tracked to done; benchmarks record permutations of the
optimization gates with repeat-run statistics. Standing gates for any
optimizer change: full `ctest` (the `-O0` byte-identity and the four
exec-replay columns are the safety net), 3-stage self-host fixpoint, and
bench deltas recorded here.

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

- [ ] **Sparse conditional constant propagation** — Wegman–Zadeck SCCP.
- [ ] **Copy propagation** (Aho–Sethi–Ullman dragon-book dataflow).
- [ ] **Dead code elimination** (liveness via **Kildall's algorithm**
      worklist dataflow).
- [ ] **Common subexpression elimination** — Cocke's available
      expressions; **value numbering** (Alpern–Wegman–Zadeck GVN as the
      global form; local VN first).
- [ ] **Sethi–Ullman numbering** for evaluation-order/register pressure
      in the replay emitter.

### Tier 3 — loops and whole function (most complex)

- [ ] **Loop-invariant code motion** (Allen/Cocke; lazy code motion per
      Knoop–Rüthing–Steffen as the refinement).
- [ ] **Induction-variable strength reduction** (Allen–Cocke–Kennedy).
- [ ] **Tail-call elimination**.
- [ ] **Jump threading / branch folding** over the replay basic blocks.
- [ ] **Graph-coloring register allocation** (Chaitin–Briggs) — replaces
      the pinned-register promotion heuristic; largest item, last.

### Tier 4 — search strategy & researched approximations (2026-07-09 web research)

Grounded in actual sources this iteration (prior entries were from memory):

- [ ] **Bayesian optimization of the `-O<N>` search** — replace mcchv's
      linear `0..UINTMAX` sweep with a surrogate-model search that spends
      the seconds budget on promising candidates. Surrogate options from
      the literature: **Gaussian-process** (continuous, gives uncertainty),
      **Tree-structured Parzen Estimator (TPE)** (models P(config|good) vs
      P(config|bad), cheap in the discrete/mixed (nhot, leaf_cut, …) space),
      **random forest** (mixed/noisy). Precedent: **BaCO** (Bayesian
      Compiler Optimization, arXiv:2212.11142) and TPE in Hyperopt/Optuna.
      Least-risk here: TPE (tool-only, no compiler correctness surface).
- [ ] **Deterministic sequence approximation for `sin`/`cos`/`exp`** —
      the missing determinism story for the parked libm-fold item.
      Compile-time evaluate with a **minimax polynomial** whose
      coefficients come from the **Remez exchange algorithm** (Remez 1934;
      optimal in the uniform/L∞ norm via the **Chebyshev alternation
      theorem**), or **CORDIC** for a shift-add integer form. Evaluated in
      the compiler's own fixed integer arithmetic (not host libm) so the
      result is bit-identical across hosts — that is what unblocks folding
      these without breaking `-O0` vs `-O1` output equality. Correctly
      rounded is not required (unlike sqrt); a fixed, documented
      max-ulp minimax poly is deterministic, which is the actual gate.
- [ ] **Upgrade the benchmark protocol beyond a single Welch's t** with
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
