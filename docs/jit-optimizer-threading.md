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

### Empirical audit result (resolved via `mcc_t` + the pthread scorer)

Running `MCC_AST_SEARCH_PTHREADS=1` under ThreadSanitizer on a cold search of a unique
function reported **~47k races across 64 distinct globals**. The dominant surface was **not**
the statically-obvious `ast_cur`/gate flags — it was the strategies' **lazily-built analysis
arenas**, which the static read missed entirely. The full thread-local surface:

| Group | Globals (mccast.c) | ~races | Note |
|---|---|---|---|
| **Hash-cons cache** | `ast_hash`, `ast_hash_arena`, `ast_hash_cap`, `ast_hash_done`, `ast_hash_epoch` | ~2090 | epoch-invalidated; dominant |
| **Def-use analysis** | `ast_du_arena`, `ast_du_epoch`, `ast_du_flags`, `ast_du_n`, `ast_du_off`, `ast_du_state` | ~640 | rebuilt per function |
| **Predicate memo** | `ast_memo`, `ast_memo_arena`, `ast_memo_cap`, `ast_memo_epoch` | ~240 | epoch-invalidated |
| **Gate flags** | 35 × `ast_*_env` | ~35 | set per-candidate by `ast_search_gates_set` — safe once TLS (score path always sets before read) |
| **Fold counters** | `ast_{abs,divmagic,licm,range,reassoc,select,sethi}_folds` | ~20 | pass hit counts |
| **Loop-temp scratch** | `ast_ltemp_n`, `ast_ltemp_cur`, `ast_ltemp_cand` | ~10 | |
| **Misc** | `ast_cur`, `ast_graft_budget`, `ast_inline_depth`, `ast_ivsr_target` | ~10 | |

**Implication:** the epoch-keyed analysis arenas (`ast_hash_*`, `ast_du_*`, `ast_memo_*`) are
the real cost. `_Thread_local` gives each worker its own arena — correct, but each worker
leaks its arena at thread exit (C `_Thread_local` pointers have no destructor); the pool must
either reuse a fixed worker set or add explicit per-thread teardown. This is the single most
important design consequence the empirical audit surfaced.

Audit tooling (committed, default-off): `ast_search_pool_pthreads` +
`MCC_AST_SEARCH_PTHREADS` (mccast.c), driven under the `mcc_t` TSan build. The race list is
reproducible with a unique never-compiled function (the search memo is disk-backed on a fixed
path, so a repeated function is a memo hit and never re-searches).

## ⛔ Blocker found: `_Thread_local` is unavailable under the 10.6 deployment target

Marking the 64 audited globals `_Thread_local` (approach A) **does not compile**:

```
src/mccast.c:762: error: thread-local storage is not supported for the current target
```

The build pins `CMAKE_OSX_DEPLOYMENT_TARGET "10.6"` (CMakeLists.txt:1561, FORCE) for broad
macOS compatibility. macOS 10.6 predates the TLS runtime (`__tlv_bootstrap`, 10.7+), so Apple
clang rejects `_Thread_local` — and `-femulated-tls` does **not** rescue it on Apple either.
Plain `cc` with no deployment flag accepts TLS fine, confirming the target is the cause.

**This is almost certainly the real reason the search uses `fork`:** the code comment frames
fork as avoiding "the whole per-context state refactor", but the deeper truth is that
thread-local storage — the mechanical way to do that refactor — is simply not on the menu
under the shipped deployment target. There is also a second-order problem even if it were:
`_Thread_local` in `mccast.c` must be codegen'd by **mcc itself** during self-host, requiring
macho-arm64 TLS lowering in mcc's own backend.

Approach A is therefore off the table unless the deployment-target policy changes. The
revised, portable options:

## ✅ Both approach-A blockers are now resolved

1. **10.6 target** → bumped to 10.7 (host clang accepts `_Thread_local`).
2. **`-run`/JIT couldn't execute TLS** → **implemented** (commit `feat(run): cross-platform
   TLS in the -run/JIT engine`). `mcc -run` now runs `_Thread_local` correctly: macOS via a
   synthesized tlv descriptor + a `pthread_key`-backed register-preserving thunk trampoline
   (since `_tlv_bootstrap` is an abort-stub on modern macOS); Linux via a `static __thread`
   slab with the LE relocation retargeted into it. Verified on macOS arm64 (42 + true
   per-thread isolation, self-host byte-identical); Linux LE + x86_64 trampoline are
   build-clean, pending CI on those triples.

Approach A is therefore unblocked.

### ✅ Stages 1a + 1b done (commit `feat(optimizer): thread-safe concurrent gate-search scoring`)

The 64-global scored-path surface is now `_Thread_local` (via `MCC_OPT_TLS`), so the pthread
scorer (`ast_search_pool_pthreads`, `MCC_AST_SEARCH_PTHREADS`) scores candidates concurrently
**race-free**. The `ast_strategies[]` table's `&gate_flag` pointers became per-flag accessor
functions (a static table can't hold TLS addresses). Verified on macOS arm64:
- builds clean at 10.7 (debug/embedjit/tsan); non-TLS codegen **byte-identical** (TLS is a
  no-op single-threaded); self-host AOT + `mcc --jit -O4 -run src/mcc.c` byte-identical to AOT;
- the pthread scorer is **TSan-clean (0 races, down from ~47,281)** and produces
  **byte-identical output across runs** (deterministic select preserved reproducibility);
- 372/372 exec+optimizer, 47/47 JIT selftests.

Concurrent optimizer scoring works and is **opt-in** (`MCC_AST_SEARCH_PTHREADS`).

### Remaining (stage 1c, deferred)
- Route the workers through the shared `mccjit_pool` instead of per-search `pthread_create`
  (avoids thread-churn per function).
- Make the threaded path the **default** — only after Linux + x86_64 CI validates the LE-slab
  TLS path and the x86_64 trampoline (build-clean but not runtime-exercised on arm64 macOS).
- Wire the capture→score→select funnel stats (item 2) through the joined workers.

(Detail retained below for reference.)

## Second blocker (approach A) — RESOLVED: mcc's `-run`/JIT can now execute TLS

Even with the host deployment target bumped so clang accepts `_Thread_local`, a TLS-laden
`mccast.c` breaks the JIT path:

- `mcc file.c -o bin` (AOT + system linker + dyld): **works** — prints `42`.
- `mcc -run file.c` (in-memory JIT): **SIGBUS** (exit 138). Isolated: `-run` on a plain
  global works; only `_Thread_local` crashes.

Diagnosis: the `-run` engine relocates through the ELF path (`relocate_syms`, mccelf.c:956;
`R_AARCH64_TLSLE/TLSDESC`, arm64-link.c) and executes in mcc's own macOS process. The ELF TLS
model does not match the macOS host `tlv` runtime, and mcc's own lowering degrades the
thread-local to a plain `D` (data) symbol (`nm` shows `_tlsv` as `D`, not a thread var). So the
JIT neither sets up macho `tlv` descriptors nor computes a host-correct thread-pointer offset.
`_tlv_bootstrap` *is* resolvable in-process (`dlsym(RTLD_DEFAULT,"_tlv_bootstrap")` ≠ 0), so a
descriptor-bootstrap fix is feasible — but it is a real backend feature, not a config tweak.

**This matters because item 1's premise is "optimizations run in the JIT."** With `_Thread_local`
optimizer state, the JIT-hosted compiler (`selfhost-jit.py`: `mcc --jit -O4 -run src/mcc.c`)
would crash. Approach A therefore requires, as a prerequisite: **implement host-compatible TLS
in mcc's `-run`/runmem engine** (bootstrap macho `tlv` descriptors via the in-process
`_tlv_bootstrap`, or map ELF TLS onto the host thread pointer), then verify `mcc -run` on the
1-line `_Thread_local` repro prints `42`.

Decided path (user): **stay on A** — bump the deployment target (done: 10.6 → 10.7,
CMakeLists.txt) and build the JIT TLS support. Status: target bumped; JIT TLS backend feature
is the next work unit (scoped above); the 64-global `_Thread_local` marking and the threaded
scorer follow once `mcc -run` executes TLS correctly.

## Approach options (revised)

- **A. `_Thread_local`.** ⛔ **Blocked** by the 10.6 target (above). Only viable if the project
  bumps `CMAKE_OSX_DEPLOYMENT_TARGET` to 10.7+ *and* mcc gains macho-arm64 TLS codegen for
  self-host. Smallest diff if the policy changes — otherwise dead.
- **B. Per-*arena* state (portable, recommended if we thread at all).** The dominant races are
  the epoch-keyed analysis caches (`ast_hash_*`, `ast_du_*`, `ast_memo_*`) — which already key
  on "the current arena". Move them *into* the `AstArena` struct (`a->hash`, `a->du`, `a->memo`)
  so each trial clone owns its cache and concurrent scoring on distinct clones cannot race —
  **no TLS, portable to every target.** Gate flags / fold counters ride the same context. Large
  but mechanical, and it aligns with the isolation the clones already provide. This is the
  honest path to real multithreading here.
- **C. Keep fork, unify the API only (portable, low-risk).** Wrap the existing fork pool behind
  a `trigger→join` API and route the funnel stats (item 2) through it. Delivers the *shape* and
  the observability without any thread-safety refactor — but it stays multi-*process*, not
  multi-threaded. Fastest to ship; least matches the literal "threads" ask.

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
