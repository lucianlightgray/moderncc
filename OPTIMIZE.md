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

- [~] **Deterministic builtin folding** of exactly-rounded libm calls on
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
