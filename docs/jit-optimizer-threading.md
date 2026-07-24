# Item 1 — Multi-threaded optimizer via the JIT pool: design & data-race audit

Status: **design / pre-implementation**. This is the plan the "5→4→3, then stage item 1"
sequence asked for. No optimizer threading code is written until this is reviewed.

## Goal

> All optimizations run in the multi-threaded JIT; an AOT `-O` compile simply triggers
> the JIT to optimize the AST across threads, then joins back to the main thread.

Concretely: generalize today's **fork-based** per-function gate search into a
**pthread** fan-out over the shared `mccjit_pool` worker pool, used as the optimizer's
committed path, with the join happening before emit.

## The invariant that must survive

The `-O4` search today is safe *because the parallel part only explores*: each candidate
gate config is scored on an isolated clone, and the winner is chosen **deterministically**
(lowest cost; ties → lowest index) and then applied by the **normal single-threaded emit**
on the untouched captured tree (`mccast.c:11783` block comment). Threads must preserve
exactly this: **parallel score → deterministic select → serial emit**. If selection stays
deterministic and emit stays single-threaded, byte-golden output is unchanged regardless of
thread scheduling. Any design that lets a thread's nondeterministic timing influence *which*
config wins, or that emits from a worker, breaks reproducibility and is rejected.

## Why fork today, and what threads cost

`ast_search_pool` (`mccast.c:12856`) forks up to `nproc-1` score-only workers. The code
comment (`mccast.c:12839`) is explicit:

> "A fork gives each worker its own copy of every optimizer global (COW) … the fork isolation
> replaces the whole per-context state refactor for the scoring step."

Threads share one address space, so the COW isolation is gone. Every optimizer global the
scoring path touches becomes a data race. **Item 1's real prerequisite is that per-context
state refactor.** The good news: the *static-score* path touches far less than the emit path.

## The scored path (what must become thread-safe)

`ast_search_score_one` (`mccast.c`, the no-emit scorer):

```
saved_cur = ast_cur;
trial     = ast_arena_clone(pristine);   // calloc'd, per-candidate — already isolated
ast_search_gates_set(gates);             // <-- mutates ~31 global flags
ast_cur   = trial;                       // <-- pervasive global (251 refs)
hits      = ast_run_strat_cycle(trial, sym, faithful, ast_strat_order, n, NULL);
sc        = ast_cost_score(trial);
ast_cur   = saved_cur;
ast_arena_free(trial);
```

## Data-race audit — the shared mutable surface

Classification of every global class the scored path can reach. **The full per-symbol
inventory is stage-0's deliverable**; this is the framework and the known entries.

| Class | Examples (file:line) | Verdict |
|---|---|---|
| **Per-candidate isolated** | `trial` arena — `ast_arena_clone` uses `calloc` per arena | ✅ already safe (malloc is MT-safe) |
| **Pervasive current-arena ptr** | `ast_cur` (251 refs) | ⛔ **thread-local** |
| **Gate state** | `ast_search_gates_set` writes ~31 `ast_*_env` flags (`mccast.c:12493`) | ⛔ **thread-local**, or refactor strategies to read a per-context mask |
| **Per-pass fold counters** | `ast_tmpl_folds`, `ast_ident_folds`, `ast_cprop_folds`, `ast_dse_folds`, `ast_sccp_folds`, `ast_jt_folds`, `ast_cse_folds`, `ast_licm_folds`, `ast_bf_folds`, `ast_range_folds`, `ast_divmagic_folds`, `ast_abs_folds`, `ast_select_folds`, `ast_reassoc_folds`, `ast_sethi_folds`, `ast_tco_folds` (~16) | ⛔ **thread-local** |
| **Strategy scratch pools + `_n`** | `ast_strpool_n`, `ast_ltemp_n`, `ast_cse_n`, `ast_vlat_n`, `ast_fconst_n`, `ast_du_n`, `ast_argsub_n` and their backing arrays | ⛔ **thread-local** (or prove emit-only, see below) |
| **Module accumulators** | `ast_graft_total`, `ast_promo_total`, `ast_opt_total`, `ast_data_total_ro/rw` | ⛔ thread-local during scoring (already save/restored) |
| **Emit-only pools** | `ast_inline_pool`, `ast_reemit_pool`, `ast_promo_n/save_n`, emit cursors | ❓ **must confirm NOT reached by the no-emit cycle** (`mccast.c:11783` claims the score path avoids emit-cursor/promo hazards) |
| **Read-only shared** | `ast_strat_order[]`, `ast_strat_order_n`, `ast_strategies[]`, env config read once | ✅ safe if frozen before fan-out |

Two open questions stage 0 must answer with certainty:
1. **Which scratch pools does `ast_run_strat_cycle` (no emit) actually touch** vs. which are
   emit-only? This sizes the thread-local surface (memory = surface × nthreads).
2. **Any hidden statics inside individual strategies** (static scratch buffers, memo tables,
   `ast_now_ms` forecasting window `ast_search_durwin_push`)? A TSan run over the threaded
   scorer is the ground truth here.

## Approach options

- **A. `_Thread_local` the scored-path globals.** Mechanical, localized. Each worker thread
  gets its own `ast_cur`, gate flags, counters, scratch. Lowest code churn; cost is memory ×
  nthreads for the pools, and every `_Thread_local` access is slightly slower on the hot main
  path (measure). **Recommended** for stage 1a — smallest diff that preserves the single
  invariant.
- **B. Per-context struct threaded through the scorer.** Cleaner long-term (an explicit
  `AstOptCtx*` argument), but touches the 251 `ast_cur` sites and every strategy signature —
  a very large diff. Defer; A can evolve into B incrementally.
- **C. Keep fork, unify the API only.** No thread-safety work: wrap the fork pool behind a
  `trigger→join` API and route the funnel stats through it. Delivers the *shape* (and item 2's
  stats) without the "multi-threaded" substance. Fallback if A proves too invasive.

## Staging (each stage gated, independently revertible)

- **Stage 0 — audit + harness (no behavior change).**
  - Add a **TSan build** (`-fsanitize=thread`); the existing `cmake-build-sanitize` is
    ASan+UBSan only, which does **not** detect data races.
  - Produce the complete per-symbol classification table (resolve the two open questions).
  - Land a determinism harness: N repeated threaded self-host compiles must be byte-identical
    to the AOT reference (extends `tools/selfhost-jit.py`).
- **Stage 1a — thread-safe scorer, still serial.** Apply approach A (`_Thread_local`) to the
  audited surface. Run the *existing* serial search; assert byte-identical + TSan-clean. This
  is the risky refactor, verified with zero concurrency first.
- **Stage 1b — fan-out on `mccjit_pool`.** Replace `ast_search_pool`'s fork with pthread jobs
  on the existing pool (`mccjit_embed.c:960`), behind `MCC_AST_SEARCH_THREADS`. Deterministic
  select unchanged. Gate: TSan-clean under load + byte-identical self-host over many runs.
- **Stage 1c — default + stats (item 2).** Make the threaded path the default optimizer route
  ("AOT triggers the JIT to optimize, then joins") and wire the capture→score→select funnel
  counters through the joined workers. Gate: full ctest + self-host byte-identical.

## Verification gates (every stage)

1. **Byte-identical**: `cmp` AOT object vs. threaded object, and `selfhost-jit.py` (in-memory
   compiler output == AOT) — run **repeatedly** to catch scheduling-dependent divergence.
2. **TSan-clean** under the new thread build, over the self-host workload.
3. **Full ctest** (all 47 JIT selftests + the exec/replay/search matrices).
4. **No hot-path regression**: `_Thread_local` cost on the single-threaded main compile is
   measured (the common case must not regress).

## Risks

- **Silent miscompile** from a missed racy static → mitigated by TSan + repeated byte-identical
  self-host (a race that changes output shows as a diff).
- **`_Thread_local` hot-path cost** on the dominant single-thread compile → measure in stage 1a;
  if material, fall back to option B for the hottest globals only.
- **Scope creep of the per-context refactor** → strictly bound to the *scored* path; the emit
  path stays single-threaded and untouched (that is also what protects byte-goldens).
- **Non-POSIX / fork-only assumptions** elsewhere → the pool already has `pthread_atfork`
  handling (`mccjit_embed.c`); Win32 keeps the serial fallback.
