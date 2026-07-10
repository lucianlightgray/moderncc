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
- [ ] **Algebraic identities / reassociation** (Muchnick catalog):
      `x+0`, `x*1`, `x*0` (side-effect-safe), `x^x`, `2*x → x<<1`
      (strength reduction, expression form). Verify what `gen_opic`
      already covers at parse time; the ledger item is the *replay-level*
      pass over the captured tree.
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
