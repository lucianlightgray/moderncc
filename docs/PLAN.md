# PLAN — a second executor for the AST/RIR machine

> Status: **proposal, decision table resolved 2026-08-08, reframed 2026-08-08,
> adoption not yet taken.**
> Every lettered row now carries a recommendation resting on a measurement. Fourteen
> rows that read as open design questions were investigated on 2026-08-08 and closed —
> see [Decisions resolved](#decisions-resolved--2026-08-08) at the bottom, which also
> records the four that remain genuinely open and the bugs the investigation found.
> When the chosen rows are adopted they become the top of [`docs/TODO.md`](TODO.md) and
> this file becomes the standing design reference.
>
> **Amended 2026-08-08 — cluster L added.** The device is no longer a per-dispatch
> resource. It is brought up **once, with the JIT**, in the emitted `.init_array`
> constructor; stays warm and resident for the process; is dispatched by the JIT's own
> pool; is routed to per function through the JIT's own hot-patch slots; and is torn
> down **once, with the JIT**. That last clause is a construction rather than a move —
> the JIT has no teardown today — so L2′ is a prerequisite for the rest of Phase 1.
> Rows N1 and N6 are superseded by it, and the Phase-1 `mcc_relocate_ex` claim is
> corrected there.
>
> **Reframed 2026-08-08 — this is not an exclusive move; cluster S added.** The GPU
> does not replace the CPU and does not need to. It becomes a **second executor behind
> the scheduler the JIT already has**, and it earns each piece of work by measurement.
> The machine to plug into is already built: `mccjit_pool` (`src/mccjit_embed.c:1302-1401`)
> runs off-thread work items; `mccjit_lazy_entry` (`:8968`) already picks between three
> implementations of the same function, with an `async` flag that already restricts a
> strategy to pool workers; `mccjit_bench_pair` (`:3343`) already races a candidate
> against the incumbent on recorded argument tuples and admits it only on a **6% margin,
> best of 3 rounds, majority vote across up to 16 cores**; `mccjit_kgc_calln` (`:3409`)
> already runs both and poisons the candidate on sustained disagreement; and
> `mcc_jit_publish` (`:9414`) already installs the winner with one release store into a
> hot-patch slot the compiler emits per function (`src/mccast.c:18085-18120`).
> **A device kernel is a fourth candidate in that machine, not a parallel universe.**
> Every row below that reads "move X to the device" now means "make X a candidate the
> benchmark is allowed to promote, and let it lose". See
> [cluster S](#s-the-scheduler--one-pool-two-executors), which also carries the
> single-threaded-coroutine task (S7).
>
> **Strategy set by the user, 2026-08-08 — and it reorders the D cluster.**
> **First, full coverage of all non-`Invoke` AST/RIR on the device. Then map our own
> libc as `Invoke`/call nodes to extend coverage as far as it will reach.** That is a
> different plan from D4b's general host/device call boundary, and a better-founded one:
> the measured blocking callees on the compiler's own source are `memcpy`, `snprintf`,
> `realloc`, `malloc` and `free`, so a device libc reaches them with **no crossing
> protocol at all**, while D4b's ceiling argument rests on internal calls of which only
> ~16% are graftable. Phase 3's ordering ("D1a, then D4b, then D2b") is inverted by this:
> **D2b-as-device-libc comes first and D4b may not be needed for the frame path at all.**
>
> Where that leaves the work, measured over 947 non-empty `AST_BasicBlock`s:
>
> | | blocks | disposition |
> | --- | ---: | --- |
> | eligible today | **392 (41.4%)** | landed |
> | blocked by `Invoke` | **407** | deferred to the device-libc phase |
> | **blocked by everything else** | **148** | the non-`Invoke` completion target |
>
> And the 148, ranked — this is the actual remaining work list:
>
> | blocker | blocks | needs |
> | --- | ---: | --- |
> | store via a runtime address | 35 | B1 runtime index (**prerequisite landed**: the extent column) |
> | store to a global | 16 | B1 globals |
> | store destination is a `Unary` | 4 | B1 |
> | `Jump` / `goto` | 20 | the control-flow machine (cluster C) |
> | a nested expression in a store value | 16 | case-by-case |
> | `stmt-if` / `for` failing on a nested statement | 26 | case-by-case |
> | `++`/`--` on a non-local | 10 | B1 |
> | `Binary` as a statement | 8 | discarded pure expression, same shape as op 7 |
> | return value expression | 6 | case-by-case |
> | VLA | 3 | out of scope |
> | `switch` | 3 | `OpSwitch`, none emitted today |
>
> **55 of the 148 (37%) are cluster B1**, which is now the single largest non-`Invoke`
> item and whose blocking prerequisite — object extents in the arena dump — landed today.
>
> **Reading note on hosts.** Everything dated 2026-08-07 was measured on an Apple M1
> Pro through MoltenVK. Everything dated 2026-08-08 was measured on a Linux x86_64 host
> with a **discrete NVIDIA RTX 5070 Ti**, validation layers, `spirv-val` and `glslc`.
> Where the two disagree, both numbers are kept and the host is named.

## The goal, stated precisely

Today the GPU is an **oracle**: it answers "are these two pure integer expressions
equal over this input rung?" for the width ladder. It never executes user program
state and never changes a program's result.

The goal is to make the GPU a **second executor under the JIT's existing scheduler**.
A program compiled by mcc runs as a stream of RIR/AST slices between calls, and each
slice runs on whichever executor was *measured* cheaper for that slice:

```
  the JIT pool: one queue, N workers, one hot-patch slot per function
        ↓
  a work item is a slice — the maximal call-free run of statements,
  plus its live-in offsets, plus a batch of argument tuples
        ↓
  CPU executor: ast_eval_slice / the native variant — the default, always available,
                needs no evidence to run
  GPU executor: an emitted (A1a) or interpreted (A1b) kernel — a *candidate*,
                which must beat the CPU on the bench before it runs anything real
        ↓
  mccjit_bench_pair races them on the recorded tuples, with upload, dispatch,
  download and readback inside the timed region
        ↓
  winner published into the slot; loser discarded; KGC keeps running both until
  agreement is established and poisons the candidate back to CPU on disagreement
        ↓
  ... and the link-time INVOKE boundary is where the batch is refilled, not where
  a separate machine takes over
```

The end state is **not** "the only AST nodes that execute on the CPU are `AST_Invoke`
nodes whose callee is a link-time symbol". That was the target; it is now the
**eligibility ceiling** — the fraction of work the device is *permitted* to bid on
(99.4% static / 99.8% dynamic under D4b, measured below, and unchanged by the reframe).
The end state is:

**every slice runs on the executor a benchmark proved faster for that slice, on this
machine, at this batch size — and the CPU/GPU split is an output of the measurement,
not an input to the design.**

Four consequences, and they change the shape of the work more than the wording suggests:

1. **Nothing has to be total before anything ships.** A device executor that can run 3%
   of slices is useful the day it can run 3%, because the other 97% never leave the CPU
   path they are already on. Totality — the old item 4, and the reason the old phasing
   needed six phases before a number — stops being a precondition and becomes a slow
   ratchet on the eligibility set. **This is the single largest change the reframe makes
   to the schedule.**
2. **The interpreter stops being the critical path.** A1b's device interpreter is one way
   to widen eligibility; A1a's emitter is another, and it is the one that already exists.
   Under the symbiotic frame they are two candidate producers competing on the same bench,
   and the emitter bids first because it can bid today. A1c's "both, entered via b" was an
   ordering claim about a substrate; there is no substrate to order any more.
3. **The measurement is the deliverable, not the reporting.** There is today **no timer
   anywhere around `ast_eval_slice`, and none anywhere in `src/mccgpu.c`** — not one
   `clock_gettime`, `clock()` or `gettimeofday` in the entire device layer. The only
   timing near this code brackets `ast_eval_slice_ladder` inside `ast_ladder_census`
   (`src/mccast.c:16419-16432`) and silently conflates CPU and GPU work into one scalar.
   Until both executors are instrumented, **no promotion decision can be made at all**,
   which puts instrumentation ahead of everything else.
4. **A losing bid is a result, not a failure.** The old plan had no way to express "the
   device tried and was slower"; every refusal lowered the headline (F3a). Under the
   reframe the bench's rejections *are* the partitioning policy, and F3a's "always
   offload, in census mode" survives only as a separate census mode.

The benchmark program is still **mcc itself**, and the headline number changes with the
frame: it is the **wall clock of a self-host recompile of `src/mcc.c`**, with the CPU/GPU
node split reported beside it as a diagnostic rather than as the result.

## Where we actually are — 2026-08-07

Grounding facts, so the plan is measured against the code and not against ambition.

| thing | state |
| --- | --- |
| AST kinds | **14** in `AstKind`, `AST_KIND_COUNT` (`src/mccast.h:9-26`, `kind_names[]` at `src/mccast.c:640-654`) |
| kinds the **CPU** slice evaluator handles (`ast_eval_slice_rec`) | 7 — `Literal`, `Ref`, `Load`(of a local only), `Convert`, `Unary`, `Binary`, `If`(as ternary) |
| kinds the **GPU** emitters handle (`msl_expr`, `spv_expr` in `src/mccgpu.h`) | the same 7 |
| kinds neither handles | `BasicBlock`, `Jump`, `Return`, `Store`, `Invoke`, `Poison`, `StoreVal` — **7 of 14**, and they are exactly the ones that make a program a program |
| what `AST_If` support actually means | the emitter requires `nchild == 3` (`src/mccgpu.h:1526`), and 65.6% of `AST_If` nodes are two-child — but that gate is **correct**, not a gap. By `op`: statement-`if` (`op == 0`) is 8755 two-child + 1887 three-child, loop regions another 2180, and **only `op == 5 && nchild == 3` — the ternary, 1678 nodes, 11.5% of `AST_If` — produces a value** an expression emitter could return. The real reading: **73% of `AST_If` is statement control flow, reachable only via the control-flow machine (C), not a wider expression emitter** |
| device ABI | `mcc_gpu_dispatch(code, n, in, ntuple, nlive, out)` — `int32` live-ins in, one `int32` + a defined-flag out, per lane |
| memory | none. There is no device-visible program address space at all |
| control flow | **branches and joins already exist** — `spv_branch_pair` (`src/mccgpu.h:1252-1309`) emits a real `OpSelectionMerge` + `OpBranchConditional` + two `OpPhi`s, `msl_branch_pair` (`:419-451`) a real `if`/`else` with a join variable, and `spv_logical` (`:1311`) the same for `&&`/`\|\|`. **What is missing is loops**: zero `OpLoopMerge` and zero `OpSwitch` in the file |
| widths | `int32` only. The ladder hook explicitly refuses `is64` and `is_float` live-ins and return types |
| backends | Metal (MSL text, 65536-byte cap) and Vulkan (SPIR-V, 8192-word cap), `dlopen`ed, selected by `MCC_GPU_BACKEND` — a **build-time** CMake option (`CMakeLists.txt:489-517`) setting `MCC_GPU_LANG_MSL`, not a runtime selector |
| the cap that actually binds first | not size — **`SPV_MAX_CONST`/`MSL_MAX_CONST` = 512 distinct constants** (`src/mccgpu.h:764`, `:105`); overflow sets `m->failed`. A 2049-node arithmetic chain fails on the constant cache, not on module size |
| lanes | one lane per **input tuple** of a sweep — data-parallel over the oracle's search space, not over the program |
| tests | **8 `gpu/*` cells** after `f716cf8d` made `spvgate` dual-backend. Total tree: **8,892 ctest cells from 290 `add_test` sites**, and **138 `SKIP_RETURN_CODE 77` registrations with nothing anywhere asserting that any of them must fire** (see N13) |
| local device reality — 2026-08-07, M1 | **MoltenVK 1.4.2** (`/opt/homebrew/lib/libMoltenVK.dylib`) and `cmake-stage2-gpu-vulkan/mcc` really dispatches emitted SPIR-V to "Apple M1 Pro" — `dispatches=1 lanes=64`; only the CMake-time `find_package(Vulkan)` SDK was missing |
| local device reality — **2026-08-08, Linux** | **A discrete `NVIDIA GeForce RTX 5070 Ti Laptop GPU`**, driver 595.84, Vulkan 1.4.329, plus `VK_LAYER_KHRONOS_validation`, `spirv-val` and `glslc` in `$PATH`. `maxStorageBufferRange = 4294967295` (32× the Vulkan floor), `shaderInt64` and `shaderFloat64` both true. **Three of the four "genuinely unmeasured" items are measurable here.** But see N6: `MCC_AST_EVAL_LADDER_GPU=1` **aborts `mcc`** at HEAD, so the Vulkan arm currently dispatches nothing on this host |
| measured device work | **zero beyond a one-per-process warm-up across 600 programs** (TODO.md audit, 2026-08-07). Rungs fire and fall back |
| RIR opcode coverage | `src/mccircap.c` lists **67** opcodes; `src/mccrir.c` has a `case` for **42**. The remaining 25 hit a bare `default: break;` and are **silently skipped** — no `rir_arena_mismatch++`, no diagnostic |

### And the half of the table the old framing left out — mapped 2026-08-08

The scheduler this plan needs is largely already written, on the CPU side, for a
different purpose. It is worth stating as grounding facts too, because it is the
difference between building a machine and extending one.

| thing | state |
| --- | --- |
| the pool | `mccjit_pool` — one global intrusive FIFO, one `pthread_mutex_t` + `pthread_cond_t`, N `pthread_detach`ed workers (`src/mccjit_embed.c:1302-1401`). **Started only when `-jit-threads N > 0`, and `jit_threads` defaults to 0** (`src/libmcc.c:1215`), so the shipped default never starts it and takes a synchronous fallback (`:1922-1928`) |
| its jobs | `MccjitSwapJob` (`:1288-1298`): one `void (*run)(job)`, two implementations, **run-to-completion — no yield point, no resume state, no partial progress**. The worker holds the process-global `mccjit_swap_lock` **across the entire job body** (`:1353-1355`), so N workers compile strictly one at a time |
| its producers | exactly three: eager boot recompile (`:1911`), lazy tier-up at `MCC_JIT_HOT_CALLS` (default 1000, `:1462`), and the selftests. KGC flush is not on it; the bench spawns its own joined threads (`:3346-3384`); GPU dispatch is not on it |
| the strategy selector | `mccjit_lazy_entry` (`:8968`) — three implementations tried in fixed order, first non-NULL wins: slice kernel (async-only), gate-mask search, plain rebuild. **`async` is already a parameter on all three**, which is the closest thing to a tier concept in the tree |
| the promotion benchmark | `mccjit_bench_pair` (`:3343`) — `CLOCK_MONOTONIC`, `MCC_JIT_BENCH_ITERS` default 100,000 invocations, best of `MCC_JIT_BENCH_ROUNDS` = 3, admitted on `cb * (100 + margin) < ib * 100` with `MCC_JIT_BENCH_MARGIN_PCT` = **6** (`:3335`), then strict majority across up to 16 sibling threads (`:3390`) |
| **and the benchmark is compiled out of shipped builds** | `mccjit_bench_enabled()` (`:1018`) reads `MCC_DEV_ENV`, which expands to `((const char *)0)` unless `MCC_DEV` is defined (`src/mccdev.h:13`). So in production `mccjit_bench_admit` (`:1135`) **returns 1 — admit, unmeasured**. Fail-open. Under the old plan that was harmless; under this one it is the entire mechanism. See S4 |
| the correctness tier | `mccjit_kgc_calln` (`:3409`) — runs variant *and* baseline on every call, returns the baseline's answer, memoizes agreeing tuples when purity is TIER0, and sets `k->poisoned` at ≥8 calls with ≥50% disagreement (`:3461-3466`). A poisoned function routes to baseline forever, through a mutex, and is never unpublished |
| the argument samples the bench replays | `mccjit_counter_capture` (`:1236`) — **8 tuples × 6 integer registers**, captured by the counter stub before promotion. There is no XMM save, so **float arguments are never sampled**; `bench_admit` bails on `allfp` (`:1140`) but not on mixed int/FP, which is a live soundness hole in the bench inputs |
| the publish path | `mcc_jit_publish` (`:9414`): i-cache flush of one page, then `__atomic_store_n(slot, variant, __ATOMIC_RELEASE)`. The slot is a real data-section cell the compiler emits per function, initialized to the AOT body and reached by a 6-byte indirect jump spliced over the entry (`src/mccast.c:18085-18120`) |
| what connects the JIT to the GPU today | **two calls to `ast_ladder_gpu_setup()`** (`src/mccjit_embed.c:1893`, `:1906`) and nothing else. The file contains no `mcc_gpu_*` symbol at all, and `ast_ladder_gpu_setup` only arms the *equivalence oracle* |
| timing on either executor | **none.** `src/mccgpu.c` has zero timing calls of any kind; nothing times `ast_eval_slice_rec`; `ast_ladder_gpu_run`'s budget is a **dispatch count**, deliberately not a duration (`src/mccast.c:15776`). The only relevant number is per-ladder-pair and conflates both sides (`:16419-16432`) |
| the slice partitioner | `ast_slc_walk` (`src/mccast.c:13810-13831`) already splits a `AST_BasicBlock` into maximal call-free runs at invoke boundaries — **exactly the unit this plan schedules** — and then **throws it away**: `ast_slc_flush` (`:13775`) prints one census line and reuses the stack array. No struct, no span, no arena marker survives |

Read the two tables together as the size and the *shape* of the gap. On the device side
we have a pure expression compiler for two shading languages, a working device layer and
a differential harness — **no** state, **no** control flow, **no** widths beyond 32 bits,
**no** call boundary. On the host side we have a real hot-patch scheduler with a real
measured-admission gate and a real differential correctness tier — none of which has ever
been pointed at the device, and whose benchmark is `#ifdef`-ed out of the builds that
ship. The plan is to join them.

## The six things that must be built

0. **A scheduler with two executors, and a bench that decides between them.** One work
   queue, one work-item type, a CPU executor that is always available and a device
   executor that is a candidate. The pool, the selector, the margin gate, the
   differential correctness tier and the publish path all exist
   (`src/mccjit_embed.c`); what does not exist is a slice as a first-class object, a
   timer on either side, a batched bench, and a benchmark that is compiled into shipped
   builds. **This is new, it is first, and it subsumes what used to be item 4.**
   See [cluster S](#s-the-scheduler--one-pool-two-executors).
1. **A device address space.** One buffer that holds the program's globals, heap and
   stacks. Pointers become offsets into it. `AST_Load`/`AST_Store` become indexed
   accesses. Without this, nothing past a scalar expression can run — so this is what
   bounds the *eligibility set*, not what bounds shipping.
2. **A device control-flow machine.** Loops, `goto`, `switch`, computed goto,
   early return. SPIR-V demands a *structured* CFG; C does not supply one.
3. **A call boundary.** A protocol for "GPU wants `write(2)` called" that does not
   cost a full dispatch teardown per call.
4. **Coverage, ratcheted — not totality, up front.** *Restated by the reframe.* Every
   node kind and type width that the device cannot run is a slice the CPU keeps, at no
   correctness cost and no schedule cost. The old item read "a path by which *every*
   node kind executes on the device, so the residual CPU set really is link-time
   invokes"; that is now the ceiling the eligibility ratchet climbs toward, and the
   thing that must be **total from day one is the escape census (H3)**, not the device.
5. **A device lifetime.** The backend is brought up **once**, with the JIT, before
   `main`; stays warm and resident for the process; is dispatched by the JIT's own
   pool; is routed to per function through the JIT's own hot-patch slots; and is torn
   down **once**, with the JIT. Today it is a per-dispatch resource with a lazy sticky
   init and a teardown reachable only under one env var — and the JIT has no teardown
   at all. See [cluster L](#l-device-lifetime--residency--the-device-is-a-jit-lifetime-resource).
   **The reframe raises this from a prerequisite to the load-bearing one**: without
   residency every bid pays a 117 µs cold pipeline build and every bid loses.

---

# The decision table

**UPPERCASE** = macro strategy. **NUMBER** = micro-strategy within it.
**lowercase** = the options to choose between. `★` marks the recommendation.

**Cluster S is new (2026-08-08) and comes first**, because under the reframe every other
cluster answers "what can the device bid on?" and S answers "how does a bid get made,
measured, won and revoked?". The letter is `S`, not `M` — `M5` is already an experiment
id (D1e's latency measurement) and `N` is already the no-letter findings list.

## S. THE SCHEDULER — one pool, two executors

The device is a candidate implementation competing for work against the CPU, arbitrated
by the machinery `src/mccjit_embed.c` already contains. Rows here are about the seam.

| | question | options |
| --- | --- | --- |
| **S1** | The unit the scheduler moves | **a.** an AST node — far too fine; at 117 µs fixed cost a node is 4 orders of magnitude below break-even<br>**b.** ★ **the slice: the maximal run of consecutive statements with no opaque `AST_Invoke` in their subtrees** — "RIR between calls", which is precisely what `ast_slc_walk` (`src/mccast.c:13810-13831`) already computes and precisely what it already discards. The boundary predicate `ast_slc_hascall` (`:13664`) is memoised and already has a `transparent` mode that lets graftable callees through, which is D4b's internal/external split arriving a phase early. **The work is to give the run an identity**: today `run[AST_SLC_RUNMAX]` is a stack array consumed inline by `ast_slc_flush` (`:13775`) and nothing survives the call<br>**c.** a whole function — F1c's unit, and correct for deciding *where the external-call boundary is*. Too coarse for deciding *which executor runs this*: a function is a mix of slices with wildly different batch shapes |
| **S2** | Who owns the queue | **a.** a second, GPU-specific pool<br>**b.** ★ **the JIT pool, extended** — one queue, work items tagged with a target executor, the device thread being one more worker identity. **Hard prerequisite, and it is a real defect today:** `mccjit_pool_worker` holds the process-global `mccjit_swap_lock` across the entire `job->run(job)` (`:1353-1355`), so N workers already compile strictly one at a time; putting a shader build or a dispatch on the pool as-is would serialize **every JIT compile in the process behind one pipeline build**. The lock exists because the compile path runs on process-global state (`mccjit_last_*`, `cur_text_section`, `funcname`) — so the fix is to narrow it around the codegen, not to remove it<br>**c.** two pools — rejected: two pools cannot arbitrate one hot-patch slot, and `mcc_jit_publish` has no CAS, no versioning and no ABA guard (`:9414`), so two publishers racing one slot is last-writer-wins |
| **S3** | How a slice earns the device | **a.** a static cost model — this is what exists: `ast_slice_promote_static` (`src/mccast.c:1314`) compares two `ast_cost_score` values, and `ast_cost_score` (`:9823`) is `nodes * (loopdepth+1) * (calls+1)`. **It cannot see a 117 µs dispatch, and no weighting of node counts will teach it to**<br>**b.** ★ **`mccjit_bench_pair`, extended to batch-vs-batch.** Same primitive, same `MCC_JIT_BENCH_MARGIN_PCT` = 6 margin, same best-of-3, same majority vote. Two changes are forced: (i) `mccjit_bench_run_pair`'s per-tuple scalar loop (`:3261-3269`) would time a batched kernel as N separate dispatches and misrepresent it by orders of magnitude, so the device side must be timed as **one dispatch of N tuples against N CPU evaluations**; (ii) **upload, dispatch, download and the readback memcpy must be inside the timed region**, because that is the cost the caller actually pays and excluding it is how this decision gets made wrong<br>**c.** compile-time verdict recorded on the node (F2a) — ★ **keep, demoted to a filter**: it decides what may be *benched*, never what runs |
| **S4** | The prerequisite the reframe exposes | ★ **`mccjit_bench_enabled()` must graduate out of `MCC_DEV_ENV`.** It reads `MCC_DEV_ENV("MCC_JIT_BENCH")` (`:1018`), which is `((const char *)0)` unless `MCC_DEV` is defined (`src/mccdev.h:13`), so in every shipped build `mccjit_bench_admit` **returns 1 without measuring** (`:1138`). Every bench knob below it — iters, margin, rounds, cores, poison thresholds — is the same. **And the fail direction must flip.** Today the bailouts are default-admit: no bench ⇒ admit (`:1138`), not routed ⇒ admit (`:1140`), no samples ⇒ admit (`mccjit_promote_by_profile:3397`). That is defensible for a CPU variant that KGC will check anyway. It is **not** defensible for a device variant, where "unmeasured" plus "batched" plus "117 µs" is a reliable slowdown. Rule: **no measurement ⇒ the CPU keeps the slice.** The CPU is the default precisely because it needs no evidence |
| **S5** | Batch size — and why the naive promotion always loses | The device interface is batched: `ntuple` lanes rounded up to `MCC_GPU_LOCAL_SIZE = 64` (`src/mccgpu.h:38`; `cap` at `src/mccgpu.c:1671`). A slice promoted on a hot *scalar* path has **one** tuple per call and pays the full fixed cost plus 64 lanes of zero-fill for one value. Break-even against a CPU slice at `C` ns/tuple: `ntuple > fixed / (C − per_lane)`. **MEASURED 2026-08-08 and the estimate below was two orders out.** With cluster L3's residency in place the fixed cost is **~20 µs, not 117 µs**, and the sweep gives break-even at **104 lanes for a 15-node slice, 32 at 31 nodes, 12 at 127, and 5 at 255+** — CPU cost is linear in node count at ~14 ns/node while device per-lane cost is flat at ~207 ns, so the device crosses over at about 15 nodes. *Superseded estimate, kept because it drove the row's conclusion:* at 117 µs fixed and a 100 ns CPU slice this read **1,177 lanes**, and below `C` ≈ 72 ns no batch size wins. The direction of S5c survives; the magnitude does not.<br>**a.** the existing loop-nest analyses (`ast_loopnest_build`/`ast_loopdep`) supply the batch from a data-parallel loop<br>**b.** a deferred-call ring that accumulates invocations and flushes — needs a call protocol the trampoline ABI cannot express today (`mccjit_make_trampoline`, `:874`, is a synchronous scalar tail-jump)<br>**c.** ★ **a only, for v1: bid only on slices already inside a loop over independent tuples, and never on scalar call sites.** This is the honest reading of the arithmetic, it makes S3's bench meaningful (there is something to batch), and it does not require inventing a deferred-call protocol before the first measurement. **Note this reopens J4a** — lane parallelism is the *whole* source of the win, so "single-lane until Phase 7" cannot also be true. J4a is amended below |
| **S6** | What must be cached before any bid can win | Vulkan rebuilds **ten object classes per dispatch** and passes `VK_NULL_HANDLE` for the pipeline cache (`src/mccgpu.c:1752`), so the driver recompiles the SPIR-V every call; Metal caches PSOs on an FNV-1a of the MSL source text (`:510-583`) but not buffers. Everything except the two buffer maps, the memcpy, the command recording and the submit is a **pure function of `(code, nlive)`**.<br>**a.** ★ a persistent handle — `mcc_gpu_prepare(code, n, nlive) → MccGpuKernel *`, dispatched by handle, freed with the kernel. The current signature takes raw `(code, n)` bytes (`src/mccgpu.h:67`) so there is nowhere to hang a pipeline<br>**b.** ★ **the cache key, which does not exist today.** `MccGpuCode` is `{void *p; int n;}` (`src/mccgpu.h:53`) and remembers nothing about what it was emitted from. It cannot be keyed on the code bytes — that is what Metal already does, and it only dedups *after* the emit is paid. The key must be `(ast_slice_ident_hash(a, root), nlive, off[0..nlive) in order, wtype)`: the `off` **ordering is part of the ABI** (`msl_env_index`, `src/mccgpu.h:770`; the SPIR-V twin at `:2310`), so the same subtree with a permuted live-in vector is a different kernel. None of those four is recorded anywhere |
| **S7** | The task representation | **a.** run-to-completion, as today (`:1341-1359`)<br>**b.** ★ **a single-threaded coroutine that ticks, and is the one task type both pools schedule.** Four separate open problems collapse into it: (i) **L2′** — the JIT has no shutdown because its workers sit in an unbounded `pthread_cond_wait` (`:1347`) and are `pthread_detach`ed at `:1375`; a quit flag between ticks is trivial, a quit flag inside an opaque `job->run(job)` holding a global lock is not; (ii) **D1e is already a coroutine on the device side** — a pre-enqueued self-skipping resume chain where each command buffer loads the state vector, checks for a host reply and exits immediately if absent **is** a tick, measured at 12.4 µs; (iii) **C3b's** suspend-at-loop-top step budget is the same object a third time; (iv) mcc ships a C11 `<threads.h>` over pthreads (`runtime/include/threads.h`, 217 lines) whose only in-tree consumer is `tools/mcchv.c` and whose `mtx_timedlock` (`:144`) is a 1 ms `nanosleep` poll. **One representation serves all four, and the tree has no coroutine, fiber, `ucontext` or state-machine task abstraction of any kind today** — the census found zero. Task and detail: [`docs/TODO.md`](TODO.md)<br>**c.** an OS thread per slice — rejected: a 117 µs dispatch does not need a thread, and it multiplies the S2 lock problem |
| **S8** | Where the verdict lives | `MccjitCounterState.promoted` is a bare `void *` (`:1101`) and `mcc_jit_publish` can only store something directly callable under the C ABI. **A device kernel is not** — `mcc_gpu_run` is a batched call over `MCC_GPU_IN_SLOTS`/`OUT_SLOTS`.<br>**a.** ★ `promoted` keeps its type and holds a **host-side marshalling trampoline**, the shape `mccjit_make_trampoline` (`:874`) and `mccjit_make_kgc_stub_n` (`:3932`) already build; the discriminant and the `MccGpuKernel *` hang off a widened `MccjitCounterState`<br>**b.** ★ **and the differential pair is `MccjitKgc.mx_variant`/`mx_baseline` (`:2517-2518`), so a device variant inherits the `vval == bval` gate (`:3447`) for free.** This is the most valuable single consequence of the reframe: **poisoning already means "route to the CPU forever"**, which is exactly the per-slice fallback J3 had to delete at region granularity — and it is sound here for the reason it was unsound there, because a *candidate* slice's stores have not yet been trusted |

### Findings — the scheduler, mapped 2026-08-08

- **There are three unrelated things called "slice" in this tree**, and the plan has been
  sliding between them. (1) The **eval slice** — a pure-integer expression subtree the
  oracle interprets (`src/ast_eval_slice.h`), an `(AstArena *, AstLocal)` pair, not
  persisted. (2) The **opt window** — a 3..65536-node expression window rooted at
  Binary/Unary/Convert/Load, alpha-hashed and memoised to disk for `-fopt-slice`
  (`src/mccast.c:1015-1063`). (3) The **census slice** — the maximal call-free run of
  *statements* (`:13800-13882`), persisted nowhere, existing only as a printed line.
  **S1b is the third one.** Every "slice" in cluster S means the third one.
- **`mcc_gpu_emit` is not reachable from the JIT.** It is `static` inside
  `#if defined(MCC_GPU_ORACLE)` (`src/mccgpu.h:2551`), and `MCC_GPU_ORACLE` is defined in
  exactly one place — `src/mccast.c:15745`. In the amalgamated build it happens to be in
  scope because `mccast.c` precedes `mccjit_embed.c` in `src/libmcc.c`; **in the
  `MCC_AMALGAMATED=0` build (`CMakeLists.txt:2032`) `mccjit_embed.c` is a separate TU and
  cannot see it at all.** A non-static entry point is a prerequisite for any bid.
- **The pool is off in the shipped default.** `async = s1->jit_threads > 0` is decided at
  *compile* time (`:1965`) and `jit_threads` defaults to 0 (`src/libmcc.c:1215`), so the
  generated constructor emits the synchronous `mccjit_boot_swap` form and the pool never
  starts. Anything that depends on `async` — including `mccjit_slice_search`, which is
  gated to pool workers at `:8916` — is dead by default. **The scheduler this plan
  extends is, today, not running.**
- **A rejected candidate is rejected permanently, and leaks.** `st->promoted` is latched
  on the losing path too (`:1419`, `:1484`), and `mccjit_counter_tick` short-circuits on
  it (`:1452`), so the slot never re-enters promotion for the process lifetime and
  sample capture stops. The rejected `mmap`'d pages and the `MccjitKgc` are never freed.
  For a device candidate this is the wrong default twice over: the right answer depends
  on batch size, which changes at runtime, so **a device rejection should be recorded
  per `(slice, batch bucket)` and be re-triable**, not latched per slot.
- **Only one thing persists across runs, and it is not the verdict.** `gs_bench_won`
  drives `mccjit_graduate_slices_blob` (`:1229`) → `ast_slice_graduate_arena`
  (`src/mccast.c:15649`) → a salted disk cache `sl-%016llx.ck` (`:15493`). What is *not*
  persisted: the winning gate mask per slot, the `promoted` pointer, the bench verdict
  itself, and **every poison decision** — a function poisoned in run N re-runs the whole
  differential gauntlet in run N+1. A device verdict is exactly the kind of thing that
  should persist (it is a property of this slice on this machine), and there is a
  working disk-cache idiom to copy.
- **The bench's inputs are narrower than they look.** 8 tuples × 6 GP registers, captured
  by the counter stub's reverse push order (`:1239`). Pointer arguments are replayed
  verbatim as `intptr_t` (`:3154`), so **the benchmark re-invokes the function against
  heap addresses that were valid at sample time**. For a CPU variant that is a
  known-imperfect but bounded risk; for a device variant under B1b, where a pointer is a
  buffer offset, it is a different value entirely and the marshalling layer must convert,
  not reinterpret.
- **`mccjit_bench_pair`'s sibling threads all hammer the same two functions
  concurrently** (`:3360-3390`), so the number is throughput under self-contention. A
  device candidate has a **global** lock (`MCC_GPU_LOCK`, `src/mccgpu.c:18-30`) and one
  queue, so 16 siblings would measure 16-way contention on a single device queue against
  16-way parallel CPU execution — **a systematically pessimistic comparison for the
  device**. Either the device arm runs single-sibling or the CPU arm does; they cannot
  both be right at `cores = 16`.
- **Nothing anywhere times either executor**, restated here because it is the day-one
  work: zero timing calls in `src/mccgpu.c`; nothing around `ast_eval_slice_rec`;
  `ast_ladder_gpu_run` budgets by dispatch *count* (`src/mccast.c:15776`);
  `AstLadderStat.secs` (`:16204`) is per ladder-pair and mixes both sides into one
  scalar, and `cmake/ladder_gpu_parity.cmake:36-37` strips it before comparing.

## A. EXECUTION SUBSTRATE — what runs a region on the device

| | question | options |
| --- | --- | --- |
| **A1** | What form does device-side code take? | **a.** translate each region to a shader (extends today's `msl_expr`/`spv_expr`)<br>**b.** upload the **arena itself** as data and run an interpreter kernel over it<br>**c.** ★ **both — but entered via a, reversed 2026-08-08.** They are two candidate producers bidding into the same S3b bench, not two layers of one substrate. The emitter bids first because it can bid today; the interpreter is what widens eligibility once the H6 table shows there is something to widen *into*. See A3 |
| **A2** | If interpreting, what is the node encoding? | **a.** the live `AstArena` layout, uploaded verbatim — **89 B/node across 21 descriptor bindings, and still not pointer-free**; the 21 bindings, not the size, are the disqualifier<br>**b.** ★ **RESOLVED 2026-08-08 — two layers, and the host layer already exists.** Layer 1 is `mccjit_intent` (`src/mccjit_intent.c`, 972 lines, `MCCJIT_INTENT_MAGIC`/`FORMAT`) bumped to format 14: versioned, pointer-free, round-trip-tested, and its `ROLE_FUNC` arm **is** D3's typed argument descriptor while its `ROLE_STRUCT` arm **is** E4's aggregate layout, both already written. Layer 2 is a **32 B/node fixed-stride device projection** with sparse `FBITS`/`WIDE`/`IVAL64` overlays<br>**c.** a bytecode/stack machine — a third IR to keep faithful; note that designing a twelfth field set from scratch would be the same objection |
| **A3** | Do we start with A1a or A1b? | **a.** ★ **REVERSED 2026-08-08 by the reframe — emitter first.** The case for the interpreter was **totality**, and totality is exactly what stopped being a precondition: an executor that covers 7 of 14 kinds is useful the day it ships, because the other 7 never leave the CPU. The emitter exists, `mcc_gpu_emit` works today over real harvested arenas, and it can therefore produce the H6 cost table — the artifact that decides whether the interpreter is worth writing at all. **Writing a 15–25k-word interpreter kernel before knowing a single break-even number is the one sequencing error this plan can still make**<br>**b.** ~~interpreter first — for totality and to be the emitter's differential oracle~~ — the oracle half survives and is unaffected: G2b's reference tier is the **CPU** interpreter over the same lowered array (Phase 0), which is not the device interpreter and never needed to be. The totality half is superseded<br>*Unchanged either way:* the size-cap argument for A1b was wrong (see findings), and N2's cap raise is still a prerequisite **for A1b specifically**, not for the emitter path |
| **A4** | Where does the interpreter's source live? | **a.** ★ **hand-written twice**, MSL and SPIR-V — ~1500–2500 lines each, at the 1.45:1 ratio the existing emitters run at (976 MSL lines vs 1417 SPIR-V). **"Twice" never doubles any single binary**: `src/mccgpu.h:155`/`:1131`/`:2548` already `#if`-wrap each arm entirely and exactly one compiles, which is the whole of K1's answer<br>**b.** ~~one C-subset source lowered by mcc's own emitters~~ — **a genuine bootstrap paradox, rejected as a mechanism**; restated below as the Phase-6 *acceptance criterion*<br>**c.** GLSL + SPIRV-Cross — adds a build dependency `mccgpu.c` deliberately has none of. Measured cost of the `glslc` half on the 2026-08-08 host: **~5 ms per 1000 words** (320 switch arms → 32,815 words in 0.181 s), i.e. front-end compile is a non-issue; it is the *dependency* that disqualifies it |

### Findings — measured 2026-08-07

**Emitter cost per AST node** (standalone harness over `tools/spvgate_arena.c`, which is literally `#include "mccast.c"`):

| shape | SPIR-V words/node | MSL bytes/node |
| --- | --- | --- |
| add/mul/and/xor chain | 11.7 | 71 |
| signed div chain | 16.3 | 79 |
| shift chain | 13.8 | 63 |
| balanced `AST_If` tree | 20.4 | 96 |

Fixed floor ≈ 200 SPIR-V words / ≈ 1.5 KB MSL, so **8192 words ≈ 400–700 nodes and 65536 bytes ≈ 680–1050 nodes** — the two caps are roughly equivalent, not wildly different as the draft implied.

**Self-compile arena census** (`MCC_ARENA_DUMP`, `mcc -O2 -c src/mcc.c`, 2452 bodies / 374,310 nodes):

- nodes per body: p50 = **61**, p90 = 310, p95 = 539, p99 = 1677, max = **7019** (`unary_nested`).
- kind mix (%): `Ref` 29.3, `Literal` 16.3, `Convert` 10.5, `Binary` 10.1, `Unary` 6.4, `Store` 5.0, `StoreVal` 5.0, `Invoke` 5.0, `BasicBlock` 4.7, `If` 3.5, `Load` 1.6, `Jump` 1.4, `Return` 1.2, `Poison` 0.0.
- **maximal invoke-free regions (F1b): mean 24.8 nodes, p90 = 48, p99 = 185, max = 1114.**
- a real emitter run over all 324k reconstructed nodes: **max module 1618 words, p99 = 313, zero modules over 8192**.

**Three corrections to the draft this forced:**

1. **"An interpreter kernel is a single fixed binary, which sidesteps the cap" was false.** `MCC_GPU_CODE_MAX` is used in exactly two places, both emitter-side sanity checks (`src/mccgpu.h:1587`, `:1614`); the device layer takes pointer+length with no fixed buffer (`src/mccgpu.c:1417`, `:367`, `:1504`). An over-cap interpreter is rejected by the *same* check. Nothing is sidestepped — the constant is simply raised, and **raising it is a hard prerequisite** for A1b: a realistic interpreter is ≈15–25k words at glslc-class density (≈50–100k naive), i.e. **2–12× over the current cap** and far below any real limit (Vulkan sets no module-size bound; Metal handles multi-MB shaders). The binding constraints for an interpreter are register pressure and one-time compile time, not bytes.
2. **The size cap does not bind at region granularity anyway.** Max real invoke-free region is 1114 nodes ≈ 18k words, and the measured max over a whole self-compile was 1618 words. Whole-*function* emission is where it binds: at a 500-node budget it covers 94.5% of bodies but only **56.4% of nodes**, and the largest body is ~112k words, 14× over cap.
3. **A4b is a bootstrap paradox, confirmed by enumeration.** The emitters today have **no function definitions** (single hard-coded entry, `mccgpu.h:683`, `:1092`), **no loops** (zero `OpLoopMerge` in the file), **no local variables** (`SpvStorageFunction` is declared at `mccgpu.h:758` and never used — no `Function`-storage `OpVariable` is emitted anywhere), **no calls** (zero `OpFunctionCall`), no arrays beyond the two fixed storage buffers, int32 only, a 512-constant ceiling, and `AST_If` only in ternary form. Compiling an interpreter needs loops + switch + locals + arrays + calls — *which is the entire project*. There is no staging that escapes it: any subset compilable today is an expression, and an interpreter that is one expression is not an interpreter.

**A4b survives as an acceptance criterion, not a mechanism.** The emitters are *done* when they can recompile the hand-written interpreter's own C source and the result matches the hand-written pair bit-for-bit. That keeps the ambition and inverts nothing.

**`tools/spvgate.c` is already ~80% of the G6 harness** (1021 lines): its own Vulkan device, 18 synthetic cases (`:533-551`), a `--mutate` known-positive mode that corrupts a word and requires failure, and an `--arenas` mode (`:744`) that replays `MCC_ARENA_DUMP` files and differentials every lowerable subtree against `ast_eval_slice_rec`. It forces `MCC_GPU_LANG_MSL 0`, so it gates SPIR-V on every host. **What it lacks is a memory buffer and a region (rather than expression) entry point** — exactly the G6 delta.

## B. ADDRESS SPACE & STATE — where program memory lives

| | question | options |
| --- | --- | --- |
| **B1** | What is a pointer? | **a.** a host virtual address, translated per access<br>**b.** ★ a **byte offset into one device buffer**; the whole program address space is that buffer<br>**c.** a tagged (region, offset) pair — safer, but every pointer arithmetic node pays for the tag against 77.8% str/mem traffic |
| **B2** | How is that buffer allocated? | **a.** fixed size at startup<br>**b.** ★ **RESOLVED 2026-08-08 — 64 MiB fixed reservation, device bump + 8-class free list, and NO growth protocol in v1.** Growth **cannot be serviced mid-kernel**, for a reason beyond coherency: a larger buffer is a *different* `VkBuffer`/`MTLBuffer` and the descriptor binding is recorded into the command buffer at encode time. Exhaustion writes a record and aborts loudly. 64 MiB is 1.5× the measured 42 MB peak live and exactly half the 128 MiB Vulkan floor<br>**c.** paged, faulted in on demand — unjustified, nothing here needs it |
| **B3** | Where do `malloc`/`free` run? | **a.** proxied to the host like any other invoke — every allocation is a round-trip<br>**b.** ★ **emulated on device** against B2's arena, installed at the one `mcc_set_realloc` hook. **AMENDED 2026-08-08: real `realloc` grow-in-place is NOT semantically required.** All 93 raw-libc sites assign the result back and none holds an interior pointer across a grow, so copy-on-grow is correct everywhere; in-place is a *space* optimization (83.3 MB → 53.0 MB), and glibc only achieves it 4.5% of the time today. The actual work is **fusing the 14 SoA arrays** and **routing through a hook** — see the B3 findings |
| **B4** | Host/device coherence | **a.** explicit upload/download per boundary<br>**b.** ★ **host-visible coherent** memory — already in effect on both backends, **but only at command-buffer granularity.** Mid-kernel it is **one-directional: GPU→host works, host→GPU does not** (see the D findings). Anything in B, C or D that assumed two-way mid-kernel coherency is unfounded<br>**c.** dirty-range tracking for discrete GPUs — **deferred, unverifiable in this project's CI** |
| **B5** | The C stack | **a.** one stack per lane, in the same buffer<br>**b.** ★ **RESOLVED 2026-08-08 — variable frames with a real device stack pointer, 256 KiB per lane**, single-lane until the globals are privatized. **Fixed frame slots are not merely wasteful (499–1366× against a 72 B median frame) — they cannot express the `alloca`/VLA that `decl`, `ast_func_end` and 25 other functions use on the parse path.** Cost of variable frames over fixed: one extra word of resume state<br>**c.** ~~registerize frames~~ — **impossible as a strategy**, demoted to an optimization (see findings) |
| **B6** | Seeding: files, argv, env | **a.** every `read()` is a host round-trip<br>**b.** ★ pre-stage whole files at open, serve `read`/`lseek`/`fstat` on device<br>**c.** ★ **b, amended and mandatory**: also pre-stage the *resolved include set and its path-search table*, because 71% of file syscalls are failed `open` probes, not reads |

### Findings — measured 2026-08-07, Apple M1 Pro, `cmake-release/mcc` compiling `src/mcc.c`

Baseline: **0.093 s wall, 23.4 MB peak RSS, 21.7 MB peak footprint, 3,673,174-byte `.o`.**

**The external-call census (DYLD interposer; lower bounds, since clang inlines fixed-size `mem*`/`str*`):**

| class | calls | share |
| --- | --- | --- |
| `memcmp` 323,058 · `memcpy` 219,989 · `strcmp` 137,330 · `strlen` 53,250 · other 611 | **734,238** | **77.8%** |
| `realloc` 104,037 · `free` 96,168 · `calloc` 167 · **`malloc` 46** | **200,418** | **21.2%** |
| `open` 7,304 (**6,820 failed probes**, 484 ok) · `read` 1,766 (7.82 MB) · `close` 484 · `fwrite` 28 · `stat` 14 · `fstat` 3 | **9,600** | **1.02%** |
| **total** | **944,327** | |

**This census is the most important number in the plan**, and it is now paired with a
*measured* round-trip rather than an assumed one. At the measured **150 µs** for D1a,
944k crossings → **141.7 s, 1523× slower than the 0.093 s baseline**. Even with device
`str*`/`mem*` **and** a device allocator, 9,671 crossings → **1.45 s = 16×**. See the D
findings for the full table: **all four of D2b, B3b, B6c and D1b are required** before
boundary cost drops below the current compile time. None is an optimization.

- **B3's premise was wrong in a way that helps.** "mcc allocates constantly" implied
  libc `malloc`; the real count is **46 mallocs against 104,037 reallocs**, because
  everything routes through one swappable hook — `default_reallocator`
  (`src/libmcc.c:166-179`) behind the `reallocator` pointer (`libmcc.c:188`, public
  `mcc_set_realloc`), with bare `malloc`/`free`/`realloc` poisoned into link errors at
  `src/mcc.h:1334-1339`. 807 of 907 allocation sites go through it, so a device
  allocator installs at **one function pointer**. The hot classes are already pooled
  (TinyAlloc 256 KiB chunks `src/mccpp.c:112,130-131`; `Sym` 8 KiB slabs with a
  free-list `src/mccgen.c:1252-1291`; AST/CST bump indices into doubling SoA
  `src/mccast.c:123-170`). The residual is the ~96 raw-libc sites in `mccast.c`/
  `mcccst.c` that deliberately `#pragma push_macro` past the poison
  (`src/mccast.c:45-46`) — **those need real `realloc` grow-in-place**, and that, not
  growth-on-exhaustion, is the actual hard part of B2/B3.
- **B4b is already implemented, not proposed.** Metal: `newBufferWithLength:options:`
  with `options = 0` = `StorageModeShared` (`src/mccgpu.c:289`), one physical
  allocation on Apple Silicon. Vulkan: `mcc_gpu_mem_index` *hard-requires*
  `HOST_VISIBLE|HOST_COHERENT` and fails init otherwise (`src/mccgpu.c:1272-1285`),
  mapped persistently at `:1314`. `grep` for `didModifyRange|blitCommandEncoder|
  vkCmdCopyBuffer|vkFlushMappedMemoryRanges|StorageModePrivate|DEVICE_LOCAL` over
  `src/mccgpu.c` → **no hits**. The device layer has no copy or sync primitive at all.
  Cost of B4 on this host: **zero**. On a discrete Vulkan device it still works (BAR
  memory is coherent on essentially all desktop GPUs) but is uncached write-combined
  over PCIe and nothing requests `HOST_CACHED`, so host *reads* of program state would
  be uncached at every boundary crossing.
- **B5 is forced, not chosen — and the margin is far worse than first measured.**
  Per-thread storage cannot hold a C stack: `maxThreadgroupMemoryLength` is **32,768 B
  for an entire threadgroup**, and at `MCC_GPU_LOCAL_SIZE = 64` (`src/mccgpu.h:38`)
  that is **512 B per lane**. The Vulkan floor for `maxComputeSharedMemorySize` is
  16 KiB → 256 B/lane.

  > **Correction.** The B track first reported a 33 KiB peak stack. That was measured
  > on a self-compile invoked **without the build's own `-D`/`-I` set**, which does not
  > engage the real code paths — the same defect that made an early arena dump come back
  > empty. Re-measured with the real flags and independently confirmed by `ulimit -s`
  > bisection on the release binary: **fails at 896 KiB, succeeds at 960 KiB.**

  **The real peak is 930 KiB — 28× the first figure**, so the shortfall against
  512 B/lane is **1860×, not 66×**, and 64 lanes × 930 KiB = **58 MiB**, which is *not*
  negligible: it collides with I2's 128 MiB `maxStorageBufferRange` Vulkan floor. The
  conclusion (the stack must be an explicit array in the B1 buffer) is unchanged and
  strengthened; the *sizing* consequence is new and constrains D-parallelism directly.

  **And the deepest recursion is not the parser.** Parser depth peaks at **11** over
  241,380 `unary()` calls. **STALE — the budget that figure was taken against is gone.**
  It was `MCC_MAX_UNARY_DEPTH 2048`, one guard on `unary()`; `wt/unarydepth` replaced it
  with `MCC_MAX_PARSE_DEPTH 512` (`src/mcc.h:1625`), one budget charged and released at
  eight parser entry points through `mcc_parse_depth_enter` (`src/mccgen.c:243`). The
  peak of 11 has not been re-taken against the new budget and must not be quoted of it.
  The real consumer is **`ast_replay_bb` (`src/mccast.c:5742`), max
  depth 35 across 33,681 calls, with a 27,424-byte frame** (`sub sp,#0x6ac0`) — 35 levels
  ≈ 936 KiB, essentially the whole stack. **Implied device frame array:** a 4× headroom
  cap of 128 frames ≈ **3.5 MiB per lane**; a cap of 64 ≈ 1.75 MiB/lane. Single-lane is
  trivial; 64 lanes is 112–224 MiB and breaches the 128 MiB floor. That coupling between
  C4's depth cap and I2's buffer limit was not previously in the plan.
- **B1 has a large existing asset, and a trap.** `mcc_relocate_ex`
  (`src/mccrun.c:664-813`) already lays every `SHF_ALLOC` section into **one contiguous
  blob**, sized in a null pass then filled, on a single monotonic cursor
  (`:711-716,749`) — and `src/mccrun.c:751` reads `s->sh_addr = mem ? addr + offset : 0`,
  so **passing `mem = 0` already yields pure image offsets**. But the blob is *not
  relocatable*: `relocate_syms` bakes live host VAs (`src/objfmt/mccelf.c:1007`),
  undefined symbols resolve through `host_dlsym_process` (`mccelf.c:970-985`),
  `R_AARCH64_ABS64` does `add64le(ptr, val)` (`src/arch/arm64/arm64-link.c:253,272`),
  `R_*_RELATIVE` never fires for `MCC_OUTPUT_MEMORY`, and `cleanup_sections`
  (`mccrun.c:185-201`) then **destroys the reloc records** so nothing can be re-based
  afterwards. Stack, heap and TLS are outside the blob entirely. The plan must say
  *"reuse the `mem = 0` layout pass, replace absolute relocation with an in-blob import
  table"* — not *"reuse the `-run` image"*.
- **B6b as drafted captures only 25% of what it claims.** The dominant file syscall is
  not `read` — it is **6,820 failed `open`s, 71% of all file I/O**, from the
  include-path probe loop calling `mcc_open` per candidate directory
  (`src/mccpp.c:1489-1538`). Staging bytes at open leaves every probe a round-trip.
  Hence the amended B6c: stage the *resolution decision* too.
- **A hard part no row owned.** Buffers are created **and destroyed per dispatch**
  (`src/mccgpu.c:315,318` → `357,358`). A persistent address space requires a lifetime
  redesign of the device layer. Added to the hard-parts list.
- **No limit is ever checked.** `VkPhysicalDeviceLimits` is fully transcribed
  (`src/mccgpu.c:569-676`, including `maxStorageBufferRange:577`) and
  `vkGetPhysicalDeviceProperties` is called (`:1225`) — but **only `deviceName` is
  read**. I2's "declare a minimum feature set" has zero foundation in code today.

## C. CONTROL FLOW — arbitrary C on a structured device

| | question | options |
| --- | --- | --- |
| **C1** | Interpreter control flow | **a.** ★ a **dispatch loop over a node/block index** — **CONFIRMED ON DEVICE.** A hand-assembled 352-word SPIR-V module (1 `OpLoopMerge` + 1 `OpSelectionMerge`/`OpSwitch`, pc in a `Function` var) runs on the M1 Pro through MoltenVK, all 64 lanes bit-correct |
| **C2** | Emitter control flow | **a.** ★ **structurize the reducible, refuse the rest** — far stronger than the draft implied: **mcc is 99.92% reducible (2360/2362 functions); exactly 2 are irreducible** (`decl_designator`, `gen_cast`, both `src/mccgen.c`). C2a as the Phase-6 fast path escapes on **0.08%** of functions<br>**b.** ★ **"relooper" only in its loop-with-switch form — which *is* C1a**, so it is the universal floor, not an independent option. A classical structurizer emitting `break label`/`continue label` does **not** map cleanly: SPIR-V's construct-exit rules are written around the innermost construct<br>**c.** full unrolling with trip caps — unnecessary |
| **C3** | Unbounded loops and watchdogs | **a.** trust the program<br>**b.** ★ **MANDATORY, demonstrated, and SIZED 2026-08-08.** A resumable SPIR-V dispatch loop that writes `(pc, acc, n)` on budget exhaustion cost **4 extra words**; budgets 4/16/64/unbounded → 48/12/3/2 rounds with **zero mismatches across all 64 lanes at every budget** — which is a proof that the state vector fully determines continuation. **Unit: dispatch-loop steps, gas-surcharged for the variable-cost D2b primitives. Suspension only at the top of the loop. Budget `1<<20`, matching `AST_EVAL_LADDER_DEFAULT_BUDGET`.** What is banked is the *rule* — median round ≥100× dispatch latency and ≤2 s/20 — not the number; the window between those bounds is **33× wide**, so this row was never tight. Needed for occupancy, Windows TDR, device errors that are currently invisible, and — newly — because it shrinks J3's fault space to two external classes |
| **C4** | Recursion | **a.** device call stack in B1's buffer<br>**b.** ★ same **with a depth cap**, and the case is worse than "SPIR-V forbids recursion". **MSL *compiles* recursion and then hangs the GPU**: `fib(n)=fib(n-1)+fib(n-2)` with runtime `n` compiles cleanly and hangs at `n=5` (`…ErrorHang`), while accumulator recursion is silently linearized to depth 65536. **The device call stack must never be left to the MSL compiler.**<br>**AMENDED 2026-08-08 — this row is the same object as D4b's missing statement guard.** The cap is a **byte budget** checked at function entry (`sp + frame_size > limit`), with a 128-level equivalent for the diagnostic wording: 3.7× the measured depth and above C99's 127-level compound-statement floor. **The identical cap must be added to the CPU front end first** (`block`, `decl`, `decl_initializer`↔`decl_designator`) and the same N reused on device — a cap that fires only on device *is* a J1 divergence, and J3a cannot rescue it because a mid-region overflow has already mutated the B1 buffer |

### Findings — control flow, measured on the M1 Pro through MoltenVK 1.4.2

Hand-assembled SPIR-V dispatched through `src/mccgpu.c` built standalone with `-DMCC_GPU_LANG_MSL=0`:

| shape | words | result |
| --- | ---: | --- |
| C1a dispatch loop, pc in a `Function` var | 352 | **runs, all 64 lanes correct** |
| same, case branching to the loop merge (break out) | 352 | runs, correct |
| `OpSwitch` **as the loop header's terminator** (negative control) | 343 | **REJECTED** — `SPIR-V to MSL conversion error` |
| `OpSwitch` with **20,000 case arms** | 180,352 | runs, correct |
| **irreducible** two-entry loop, no merge instructions | 232 | **SIGSEGV** — infinite recursion in MoltenVK's `emit_block_chain`, host stack blown |
| multi-level break (inner block → *outer* loop's merge) | 333 | accepted by MoltenVK — **treat as unportable**, no `spirv-val` here |

- **The binding rule, confirmed by the negative control and by spec:** `OpLoopMerge` must be second-to-last in its block and must immediately precede `OpBranch`/`OpBranchConditional` — **never `OpSwitch`**. So the switch cannot be the loop header; it must be a *selection* construct in a block the header dominates. That is precisely the C1a shape, which is why C2b collapses into C1a.
- **C1a scales and has no uniformity requirement.** `OpSwitch` reaches 20k arms (hard ceiling ≈32,766 — the 16-bit instruction word-count field, not a driver limit). Divergence on the selector is legal, merely slow. Constraints: no `OpControlBarrier` inside the dispatch loop, and all `Function`-storage `OpVariable`s first in the entry block.
- **The backends are asymmetric in the *opposite* direction to my brief.** **MSL has no `goto` at all** — `error: 'goto' is not supported in Metal`, and labeled break is likewise rejected. MSL is *also* structured-only; both backends need the same lowering. There is no "MSL is unconstrained" shortcut. What MSL uniquely has is function pointers and recursion — and per C4, that is a trap.
- **There is no wall-clock watchdog on Apple Silicon; there is an *interactivity* watchdog.** At 1 threadgroup × 64 lanes a kernel ran **122.6 s to completion, `status=4`, no error**. ≤512 threadgroups completed at 50 s. **≥1024 threadgroups was killed after 0.55–5.5 s.** The kill is `MTLCommandBufferStatusError` with `MTLCommandBufferErrorInternal`, and the real cause appears only in `localizedDescription`: `Impacting Interactivity (kIOGPUCommandBufferCallbackErrorImpactingInteractivity)`. **The threshold is occupancy, not duration.** Other surfaces seen: `kIOGPUCommandBufferCallbackErrorPageFault` (OOB device write), `…ErrorHang`, and `…ErrorInnocentVictim` — an *unrelated* command buffer discarded because another faulted.
- **Vulkan/Linux** (spec, not measured): no mandated timeout; loss surfaces as `VK_ERROR_DEVICE_LOST`. Drivers impose their own — `amdgpu.lockup_timeout` ~10 s, i915 hangcheck ~10–15 s, Windows WDDM TDR 2 s. **Note for I1c: lavapipe has no watchdog at all** (it is CPU threads), so the proposed Linux cell *cannot* regression-test C3b.
- **The interpreter/shader asymmetry is real and is an argument the plan was not making.** The resumable module's entire live state is three integers plus a budget. A *compiled* region has no such handle — its state is SSA values with no addressable names, so a mid-region checkpoint means materializing every live value at every suspension point, i.e. re-inventing the interpreter's state vector, worse. **This is an independent argument for interpreter-first.**
- **mcc's own CFG shapes** (dominator + retreating-edge analysis over the full `src/mcc.c` TU at `-O0`, 2362 functions): **99.92% reducible**, 2 irreducible; 823 contain loops, 1539 are acyclic and structurize trivially; **`indirectbr` count is 0** — mcc's own source contains **zero computed goto**, so `ast_func_has_labeladdr` (set only at `src/mccgen.c:12815`) can never fire on mcc itself. 739 `goto`s, 314 backward, overwhelmingly `goto again:` at a block head.

### Two real bugs this exposed — one fixed, one live

**Metal — FIXED by `c6814625`.** The path used to call `waitUntilCompleted` and then
**unconditionally** `memcpy` the output and set `rc = 1`, never reading `[cb status]` or
`[cb error]`, so a watchdog kill, page fault or hang was reported as a successful
dispatch with garbage output. `src/mccgpu.c:381-387` now checks the status and reports
through `mtl_report_err`. **Residual defect:** `mtl_report_err` prints only under
`MCC_AST_EVAL_LADDER_GPU_DIAG`, so the *reason* for a kill is invisible by default —
the exec path must classify unconditionally into H3, not print conditionally. To
distinguish "we faulted" from `…ErrorInnocentVictim` programmatically the command buffer
must be created with `MTLCommandBufferDescriptor.errorOptions =
MTLCommandBufferErrorOptionEncoderExecutionStatus`; untested, no Darwin host available.

**Vulkan — LIVE, and worse than first described.** `src/mccgpu.c:1507-1509` waits 30 s
and on any non-`VK_SUCCESS` falls to `done:` at `:1515-1535`, destroying fence, command
pool, pipeline, pipeline layout, shader module, descriptor pool, descriptor set layout,
mappings, memory and buffers — **while the command buffer may still be executing.** On
`VK_TIMEOUT` the CB is still *pending*, and because buffers are created fresh per
dispatch the driver recycles the freed allocation into the next dispatch's `bin`/`bout`
while a zombie kernel writes into it. **A timeout in dispatch N silently corrupts
dispatch N+1.** Two further defects found alongside: every one of the ~13 Vulkan failure
exits is **diagnostically mute** (unlike the Metal path), and **`mcc_gpu.ok` is never
cleared** after `VK_ERROR_DEVICE_LOST`, so `mcc_gpu_quiesce`'s unbounded
`vkDeviceWaitIdle` from `atexit` deadlocks the process after a hang.

**Neither is reachable by any existing test.** `src/mccgpu.c`'s Vulkan dispatch path has
**zero direct coverage** — the `gpu/spv-slice-*` cells use `spvgate`'s own duplicated
Vulkan implementation, not the library's. The cheapest regression test is to make the
hardcoded 30 s fence a named tunable and run one cell at 1 ns with validation layers on;
that works on this host and on lavapipe and needs no fault. A fault-injection shim
(`MCC_GPU_FAULT_INJECT=timeout|lost|error|partial`) is the only fault test that runs on a
device-less host, and it tests the *policy* — which is what J3a′ needs gated.

## D. THE CPU BOUNDARY — the INVOKE protocol

| | question | options |
| --- | --- | --- |
| **D1** | Boundary mechanism | **a.** ★ **dispatch-per-region** as the TDD bootstrap only — **measured 144–180 µs median** on M1, **117 µs fixed + 72 ns/lane** on the 2026-08-08 Linux/NVIDIA host<br>**b.** **DEMOTED 2026-08-08 to Phase 7+.** The doorbell's 24 µs is **100% cache sweep**: the four cost points fit **0.78 µs/KB with a zero intercept**, so there is no fixed doorbell cost and *any* safety margin destroys the win (2× → 3.0×, 4× → 1.5×, 8× → worse than D1a). And 3000/3000 clean rounds bounds the per-round hang rate only at 1e-3, which against 2,851 residual crossings predicts **~2.9 silent hangs per self-compile**. Its real justification is **lane-parallelism, not latency**, which places it after Phase 7<br>**c.** graduate a→b — superseded by e<br>**d.** ★ **asymmetric posting.** GPU→host is free and immediate; host→GPU mid-kernel is impossible. Calls whose result is unused need **no doorbell and no stall** — the GPU posts to a ring and continues. That is `free` (96,168), `write`/`fwrite` and `close`, **~10.3% of crossings**, independent of the other options. **Couples to J3a′:** the ring must carry no externally visible effect, or the restart precondition breaks<br>**e.** ★ **NEW — speculative pre-enqueued resume chain.** A resume **is** a command-buffer boundary, and a CB boundary is exactly what invalidates the GPU L1 — so resumption buys the doorbell's visibility *by spec*, with no sweep, no per-device constant and no hang mode. Each CB loads the state vector, checks for a host reply, and exits immediately if absent, so the chain is safe to enqueue speculatively. Projected **~30 µs** from the measured 19.5 µs/CB pipelined throughput. **This projection is arithmetic, not measurement, and it is the deciding experiment for the whole D1 row** |
| **D2** | Which externals are proxied vs emulated | **a.** proxy everything<br>**b.** ★ **emulate the hot pure ones on device — the highest-value row in the cluster.** The top **four** functions are 733,627 calls = **77.7% of all crossings**, and `memcmp`/`memcpy`/`strcmp`/`strlen` are each ~10-line device loops. **The monsters are not on the hot path at all**: `qsort` is **0 calls** (6 static sites, all cold), `vsnprintf` 44, `printf`/`vfprintf` ~0, and ctype is already inlined by clang and never crosses |
| **D3** | Call record format | **a.** a fixed struct with N slots<br>**b.** ★ a **typed argument descriptor** from the `AST_Invoke` node's type info — but at 2,851 residual crossings the encode/decode cost is invisible next to a 24 µs boundary, so this row is cheap either way. Dependency: `MCC_ARENA_DUMP` drops `type_ref` and `sym`, which is exactly what the descriptor needs |
| **D4** | Internal calls | **a.** proxy them — **impossible**, not merely slow: at 150 µs each the compile would take hours<br>**b.** ★ **stay on device. A precondition, not a lever.** Concrete requirement: **~130 mutually-recursive functions in one SCC** (68 in `mccgen.c`, 29 in `mccpp.c`) — **there is no partial port, the whole front end goes at once.** Statement nesting (`block`/`decl`/`decl_initializer`↔`decl_designator`) has **no depth guard at all**, so C4's cap is load-bearing for *safety*, not just sizing |
| **D5** | Function pointers / indirect calls | **a.** refuse to the CPU<br>**b.** ★ **device jump table — cheaper than feared: ~80 slots covers every internal indirect call.** ~75 address-taken functions in the compiler proper, dominated by two static tables (`ast_strategies[22]`×`{gate,apply}` = 44 entries, `src/mccast.c:15036`; `ast_fc_models[13]`, `src/mccforecast.h:407`). Genuinely-varying sites are three: `error_func` (3 targets), the strategy table (data-driven index), and `expr_fn` (3 targets, `src/mccgen.c:8940`, and hot). Escapes are all off the self-compile path |

### Findings — the boundary, measured on Apple M1 Pro

Every variant run in a **separate process**, fresh buffers, per-iteration magic tokens (a stale read aborts the run), N=2000–3000 after 200 warm-up, warm queue, AC power.

| variant | median | p90 | p99 |
| --- | ---: | ---: | ---: |
| **1 CB, 1 dispatch, persistent buffer, `waitUntilCompleted`** — the D1a number | **144–180 µs** | 197–204 | 224–240 |
| same, spin-poll shared memory instead of waiting | **101–105 µs** | 114–124 | 159–169 |
| same, spin-poll `[cb status]` | 183 µs | 211 | 266 |
| **fresh 4 KB `MTLBuffer` per dispatch** | 200 µs | 231 | 288 |
| 2 dispatches / 1 encoder | 221 µs | — | — |
| 16 / 256 dispatches / 1 encoder | 276 / 1066 µs | marginal settles to **3.6–8.8 µs** | |
| pipelined submit, never waiting | **19.5 µs per CB** (throughput, not latency) | | |
| `MTLSharedEvent`, CPU signals a pre-enqueued GPU-waiting CB | 125 µs | 180 | deadlocks past ~64 queued CBs |
| **persistent kernel + doorbell, 32 KB sweep per poll** | **24 µs** | **48** | **56** (max 120) |

**The plan's 20 µs assumption is refuted for D1a — it is off by 7.5×.** By coincidence it is roughly right for D1b (24 µs).

**Wall-clock, boundary cost only** (excludes all device compute; baseline 0.093 s, 944,327 crossings):

| scenario | crossings | @150 µs (D1a) | @101 µs (poll) | @24 µs (D1b) |
| --- | ---: | ---: | ---: | ---: |
| naive, every external call | 944,327 | **141.7 s = 1523×** | 95.4 s = 1026× | 22.7 s = 244× |
| + D2b (device `str*`/`mem*`, −77.8%) | 210,089 | 31.5 s = 339× | 21.2 s = 228× | 5.0 s = 54× |
| + B3b (device allocator, −21.2%) | 9,671 | **1.45 s = 16×** | 0.98 s = 11× | 0.23 s = 2.5× |
| + B6c (stage the 6,820 failed probes) | 2,851 | 0.43 s = 4.6× | 0.29 s = 3.1× | **0.068 s = 0.73×** |

**All four are required before boundary cost drops below the current compile time.** Note the ordering that falls out: crossing *reduction* is a ~100× lever and D1b is a 6× lever — **D2b/B3b/B6c must land first; D1b cannot rescue an unreduced census.**

**D1b feasibility: QUALIFIED YES, with a hazard the draft did not mention.**

1. **Host sees device writes without CB completion — yes, unambiguously.** 104 distinct pre-completion observations of a progress counter; every doorbell echo was seen mid-kernel. GPU→host is free and immediate.
2. **Device sees *host* writes mid-kernel — NO, by any qualifier.** Relaxed `atomic_load`, `atomic_fetch_or` RMW, `threadgroup_barrier(mem_device)`, `atomic_thread_fence(mem_device, thread_scope_device)`, `volatile device uint*` (serviced *zero* rounds), and a ring over distinct cache lines at 128 B and 4 KB strides — **every one saw exactly one host write and then went permanently blind.** MSL has no primitive for it: `thread_scope` is `{thread, simdgroup, threadgroup, device}` — **there is no system scope** — and `METAL_VALID_*_ORDER` restricts every device atomic to `memory_order_relaxed`. The GPU's L1 is not coherent with the CPU during a dispatch; it is invalidated only at command-buffer boundaries. Vulkan/SPIR-V inherits this, and MoltenVK lowers to MSL anyway.
3. **What works is forcing the eviction**, and **the threshold is sharp with a silent-hang failure mode**: 32 KB (the GPU L1 size) gives 3000/3000 at 24 µs; **12 KB ran 516 rounds then hung**; ≤8 KB fails immediately. Cost scales with the sweep: 32 KB→24 µs, 128 KB→100 µs, 512 KB→400 µs, 1 MB→800 µs.

So D1b is buildable, but only with a **compiler-mandated ≥32 KB cache-thrashing sweep in the poll loop** — a hardware-specific, spec-unsanctioned constant. It must be a named, tested, tunable design element. **One shortcut to forbid explicitly:** "the real interpreter loop touches enough memory anyway, so drop the sweep" — sometimes true, undefined always, and it fails as a hang.

**Two free fixes this measured, both in `src/mccgpu.c`:** buffers are destroyed per dispatch (`:315,318` → `:357,358`) costing **+56 µs, 39% of D1a's latency**; and `waitUntilCompleted` costs ~45 µs more than spin-polling a result word.

## E. TYPE & OP COVERAGE — the ladder to totality

> **Three width divergences found and fixed, 2026-08-08 — two of them in shipped code.**
> Running the real-corpus differential with the runner's strict-width workaround off, and
> dumping the first divergent tree each time, took mismatches from 60 to 55 to 2 to 0:
> (1) `ast_eval_slice.h`'s `AST_Ref`/`AST_Load` local arms returned the raw environment
> word instead of narrowing it to the type of the Ref reading it, while the `AST_Literal`
> arm beside them already did — so an `unsigned int` live-in holding `-12345` read as
> `-12345` on the CPU and `4294954951` on the device, **and the device was right**;
> (2) both emitters' Ref/Load arms could not narrow below 32 bits, because
> `spv_load_live_v`/`msl_load_live_v` take a width flag rather than a type, so a
> `signed char` live-in holding 1000 arrived as 1000 rather than `-24`; (3) the runner
> fitted the device result to the declared type rather than applying `ast_eval_narrow`,
> which over-narrows — `(unsigned char)255 + 1` is 256 under C's promotions and under
> `ast_eval_binop`, and fitting that back to `unsigned char` gives 0.
>
> **Bug 1 is the interesting one for this plan.** The ladder never saw it because
> `ast_ladder_gpu_run` pre-fits every input to its live-in's type before handing it to
> either side. That is a real precondition of the CPU evaluator, it was nowhere stated,
> nothing enforced it, and the moment a second caller appeared it produced wrong answers.
> Detail and the corpus numbers are in [`docs/TODO.md`](TODO.md).
>
> **E2b is verified at full width, 2026-08-08.** The `uint2` pair representation was
> implemented and covered, but only ever exercised by `spvgate`'s 20 `ll-` cases, whose
> inputs are all built by `mk_up` — a 1..16-bit rung value shifted up by a constant — so
> the high word only held patterns that one construction produces. `slice/wide64` now
> crosses a 16-value full-width hard corpus with itself (the 2^32 boundary both ways, a
> low word of all ones, `INT64_MIN`/`INT64_MAX`, halves with nothing in common) across
> 28 signed and unsigned ops plus both conversion directions: **143 checks, 0 failures,
> 31 dispatches**, device against CPU on value *and* definedness, with a mutation cell
> proving the comparison is not blind. **E2b's representation is sound at full width; the
> refusal sites are the only thing keeping int64 out.**
>
> It also found a shipped SIGFPE and a **J1 host divergence**: `ast_eval_slice.h:117`
> evaluates `r / a != b` before the two `INT64_MIN`/`-1` guards that exist beside it, so
> `-1 * INT64_MIN` traps on x86 and refuses cleanly on arm64. Fixed by reordering; 114
> objects byte-identical. Detail in [`docs/TODO.md`](TODO.md).

| | question | options |
| --- | --- | --- |
| **E1** | Width order | **a.** ★ `int32` → `int64` → `float`/`double` → aggregates/bitfields → `long double`. Corroborated: FP is **0.02%** of mcc's op stream, so int64 is where the benchmark lives. **But the cost was understated** — 16 refusal sites, not 8, plus a SPIR-V type/ABI change |
| **E2** | `int64` on the device — **IMPLEMENTED `989e4b3b`, E2b as planned** | **a.** native only<br>**b.** ★ **emulated `uint2` pair everywhere for the first rung** — no capability query, no `I2` plumbing, works on both backends today unmodified, and matches the 32-bit buffer the ABI already has<br>**c.** ~~native-when-advertised + emulated fallback~~ — **premature**: four code paths (2 emitters × 2 strategies) to keep differentially green. Graduate to c only if H5 shows int64 arithmetic is hot |
| **E3** | Floating point | **a.** ★ pinned rounding, no fast-math, bit-exact; `long double` escapes — **and that escape costs ~0%** (see findings)<br>**b.** fast-math — rejected, but **for J1, not for `selfhost-fixpoint`**: that parenthetical in the draft was unsupported and is probably false |
| **E4** | Aggregates, bitfields, unions, volatile | **a.** ★ all become **byte operations on the B1 buffer** — the bitfield `bp`/`bs` already on the node drive shift/mask; `volatile` forces a coherent access and disables caching |
| **E5** | Inline `asm` | **a.** ★ always escapes and is counted honestly — **and it is free: the measured asm floor on the benchmark is 0%** (see findings). Say so, because the census names the blockers that *are* real |
| **E6** | **CLOSED 2026-08-09 — REFUSE permanently; `double` on Metal** | mcc's `CValue` (`src/mcc.h:218-227`) carries `double d`, and constant folding, `ast_fc_*` forecasting and `gen_op` all operate on it. **MSL has no `double` type at all**, so the Metal arm can never be bit-exact for `double` natively and would need a **software f64 exactly as E2b did for int64**. Vulkan's `shaderFloat64` is optional but present on both the NVIDIA host (measured, see "local device reality") and lavapipe (**read from Mesa 26.0.8 source, not from a device**: `src/gallium/frontends/lavapipe/lvp_device.c:454` `.shaderFloat64 = (pdevice->pscreen->caps.doubles == 1)` → `src/gallium/drivers/llvmpipe/lp_screen.c:301` `caps->doubles = true;`, both unconditional; full record in [`docs/TODO.md`](TODO.md), "The lavapipe citation, now sourced"). **This is an E-track rung, not an I2 refusal**, and no row owned it, which is how it sat marked OPEN here while the closure below and `docs/TODO.md` both recorded it closed. Mitigating: FP is 0.02% of mcc's op stream. **Verdict and evidence are in the closure below** — 0 of 7,971 ladder type-rejections are float and 0 slices are blocked solely by float; the blocker is E4 |

### Landed 2026-08-08

- **E1 rung 2 (`int64`) is done, both backends, emulated pair.** ABI widened to 2 in /
  3 out slots. `spvgate` 38 cases / 2.5M points / 0 mismatches; 889 real slices at
  58.2M points; ladder parity 1767 dispatches on Metal *and* Vulkan. `MCC_GPU_CODE_MAX`
  raised 8192 → 16384 words for SPIR-V (the udiv64 helper is a real `OpFunction` with a
  structured `OpLoopMerge` loop, ~500 words, emitted unconditionally).
- **J1 gained a real defect and lost it again.** The CPU reference evaluator compared
  `TOK_LT/LE/GT/GE` signed regardless of signedness — sound at 32 bits, wrong at 64,
  and reachable because the arena records the pre-`gen_op` token. 1,382,356 mismatching
  points over the `gpu/spv-slice-real` corpus; 0 after `ee1fa9e0`. Emitted code is
  unchanged, so it was latent. **The lesson for G2: the synthetic suite could not see
  this — only real arenas discriminated.** A hand-written matrix is necessary and not
  sufficient; G4's harvested-arena stage is what caught it.
- **F5 is closed and its payoff was zero.** All three dropped opcodes are now modelled
  and the emitted object is byte-identical. The reconcile/stamp machinery already
  reconstructed their effect. `IR_OP_LOAD` is provably not unblockable.
- **I1 gained teeth and reach.** `MCC_GPU_REQUIRED` is on for the CI `gpu-vulkan` cell,
  and a MoltenVK fallback in `CMakeLists.txt` means Darwin now registers all four
  `gpu/*` cells instead of one. **Corrected 2026-08-08 on the M1 Pro: it is seven, not
  four** — `ladder-gpu-parity` plus an `spv` and an `msl` trio, the count "four" having
  predated `f716cf8d`. A `vulkan`-backend build on the same host registers four, because
  `MCC_GPU_LANG_MSL` is derived from the backend and `-DMCC_GPU_LANG_MSL=1` is silently
  discarded there. With `-DMCC_GPU_REQUIRED=ON` all seven still pass, which converts no
  skip into a failure and so proves only that nothing was skipping.
- **CLOSED 2026-08-08 — Metal now has a per-value differential, and it is clean.** The
  dual-backend `tools/spvgate.c` from `f716cf8d` ran on a real `Apple M1 Pro`.
  Bit-exact per-value replay against the CPU oracle over harvested arenas:
  **153,158,012 points, 151,870,645 compared, 1,287,367 vacuous, 0 mismatches, 0
  rejected modules** across 1,325 arenas / 2,359 slices / 11,129 dispatches from all 304
  `tests/exec` files. Definedness agreed on all 153.2 M points, not only on the compared
  ones. `--mutate` reports every compared point as a mismatch, so the gate is not blind.
  **`spvgate` over the identical dump returns every counter identical**, giving the
  project its first direct Metal-vs-MoltenVK comparison with **no divergence to report**;
  `ladder-gpu-parity` agrees at 1767 dispatches on both arms. `nm -u` confirms the arms
  are disjoint code — 0 Vulkan symbols in `mslgate`, 39 in `spvgate`.
- **New, and free:** `spirv-val` and `glslc` (v2026.3) are installed on that host, yet
  neither appears anywhere in `CMakeLists.txt`, `cmake/` or `tools/spvgate.c`, so **no
  cell validates SPIR-V on any host**. Run by hand over the 152 modules dumped with
  `SPVGATE_DUMP`: **152/152 valid at `--target-env vulkan1.1`.** Wiring that into the
  `spv` cells is a one-line `find_program` and is not done.

### Findings — types, ops, and the opcode gap

**The 25-opcode audit: counts confirmed, causal claim refuted, and F5a mechanically wrong.**

`src/mccircap.c:7-74` = **67** opcodes; unique `case IR_OP_` in `src/mccrir.c` = **42**; difference **25**, matching the TODO list name for name. The silent `default: break;` is at `src/mccrir.c:3264-3265`, closing the `switch (o->kind)` opened at `:2481` inside `rir_op_effect()`. Confirmed: no `rir_arena_mismatch++`, though that counter is used at 30 other sites.

**But "42 handled" is the wrong measurement.** Five opcodes counted as *handled* — `JMP`, `JMPCOND`, `JMPADDR`, `JMPAPPEND`, `GSYMADDR` — have no case in `rir_op_effect` at all; their cases live in *other* switches (`src/mccrir.c:965,969,987,1013,1029` and `:4711,4717,4723,4739,4749`). They reach the `default:` arm legitimately. There is also **a second silent-skip path the audit missed**: `src/mccrir.c:4678-4681` `continue`s the whole op whenever any of nine depth/pending flags is set — which is why `IR_OP_LOAD` never reaches the switch.

**Frequency, measured on an instrumented copy compiling `src/mcc.c` (`-O2`; `-O1` identical; `-O0` zero, the arena path is off there):**

| opcode | captured | reaches switch | hits `default:` | in the 25? |
| --- | --- | --- | --- | --- |
| `jmp` | 12336 | 9578 | 9078 | no — handled at `:965` |
| `gsymaddr` | 24175 | 18458 | 6804 | no — handled at `:1013` |
| **`retval`** | 4006 | 4006 | **4001** | **yes** |
| **`mkptr`** | 3603 | 3596 | **3590** | **yes** |
| **`vpushsym`** | 3757 | 3468 | **3059** | **yes** |
| `jmpaddr` / `jmpappend` / `jmpcond` | 2984 / 24780 / 20622 | 2984 / 2102 / 2093 | 2966 / 2100 / 2092 | no |
| **`load`** | 2237 | **0** | 0 | yes — killed by the `:4678` `continue` |

**Of the 25, exactly four occur at all: `RETVAL`, `MKPTR`, `VPUSHSYM`, `LOAD`. The other 21 fire zero times.** A naive `default:` counter would report 33,690 hits of which **23,040 (68%) are benign**. On arm64, `X87POP`, `XFERRET`, `VLA_RESULT`, `REGADDI` and `STRUCTCOPY` are `#ifdef`-ed out and *cannot* occur (`src/mccgen.c:7-43`); `MULH`/`ROUND`/`COPYSIGN`/`SQRT` are compiled in but never fired; `ASAN_*`/`UBSAN_NULLPTR`/`TCOV` need `-fsanitize`/`-ftest-coverage`; `RAW` is handled outside the switch (`:4943,5309,5471,5782`) and belongs on no gap list.

**The TODO audit's causal chain is refuted.** Per-body correlation, measured: `bodies_with_drop=1823`, `bodies_nodrop=633`, `unf_with_drop=81`, `unf_nodrop=14`. So 74.2% of bodies contain a dropped opcode and **95.6% of those still replay byte-identically and are used**. Unfaithful rate is 4.4% with drops vs 2.2% without — a 2× relative risk, not a mechanism. `RETVAL`/`MKPTR`/`VPUSHSYM` are compensated by the region/reconcile machinery. Both the TODO's "silently skipped → unfaithful → every AST optimization skipped" chain and this plan's earlier "**caps GPU coverage before any GPU work starts**" were overstated.

**So F5 becomes four features, not 25 and not zero:** a per-opcode histogram *excluding* the five CFG opcodes, then handlers for `RETVAL`, `MKPTR`, `VPUSHSYM`, and unblocking `LOAD` from the `:4678` `continue`.

**F4's headline metric is the wrong one.** `tools/rir-coverage.py:924` computes `modelled = used + (fallback_bytes − abort_bytes)` — **a body that replayed to *different bytes* still counts as "modelled".** The honest sibling `kept_coverage` sits at `:925`. Scale, measured two ways — **and the single-file figure is an outlier that must not be generalized**: `src/mcc.c` alone gives kept **81.70%** (`-O2`) / **81.42%** (`-O1`), but the **self corpus** gives kept **97.784%** measured on this host at `-O1`, against banked 96.156% (`-O1`) / 96.127% (`-O2`). So the real whole-corpus gap is **2–4% of body bytes**, not 18%.

**The reporting was already honest; the ratchet was not.** `rir-coverage.py:943-945` has always printed `MODELLED … (kept … + discarded by byte compare …)`. But the regression check gated only `modelled_coverage` and `residual` — **`kept_coverage` was banked and never enforced.** A gate has now been added beside the modelled one, verified to fire on a doctored bank and stay silent against real values. Note it is enforced only on elf/pe: `rir-coverage` returns 77 on Darwin (`:836-848`, no banked lowerable floors for macho), so both coverage cells skip on this host.

- **The CPU slice evaluator already does int64.** `src/ast_eval_slice.h:382,422` use `is64` to *select* width and pass it to `ast_eval_binop`; nothing in `ast_eval_slice_rec` refuses it. Only the two emitters and the ladder gate do. The refusal sites are **7 in MSL + 7 in SPIR-V + 2 in the ladder hook = 16**, not 8 (`src/mccgpu.h:500,512,522,539,552,564,601,1382,1394,1404,1419,1432,1444,1479`; `src/mccast.c:15759,15766`).
- **int64 costs an ABI change on either path.** The SPIR-V module declares **only `SpvCapShader`** (`src/mccgpu.h:943-944`) — no `Int64`, no `Float64` — and the storage buffer is an `OpTypeRuntimeArray` of 32-bit ints with `ArrayStride 4`. Native and emulated both need the same two-slot buffer layout, which is why E2b (pair everywhere) is the cheaper first rung.
- **FP is negligible in mcc: 106 FP ops out of ~496,000 captured ≈ 0.02%** (`opf=32`, `cvt_itof=34`, `cvt_ftof=24`, `cvt_ftoi=16`). `long double` is 29 textual occurrences, essentially all literal parsing in `src/mccpp.c:3359-3490,3683`; on AArch64 Darwin `long double == double` (`src/mccast.c:2576`) and `ast_bad_type()` already rejects it. **E3's permanent escape costs <0.02% of dynamic nodes, plausibly 0 on this host.**
- **E4 confirmed.** `ast_set_type_bf` (`src/mccast.h:54`), readers `ast_type_bp`/`ast_type_bs` (`:77-78`); the RIR builder populates them from `SValue.type.bp/.bs` at 17 sites and reads them back at `src/mccrir.c:1900-1901,2512`. The information is on the node and survives capture→arena.
- **E5 is free, and the real floor is elsewhere.** On `src/mcc.c` at `-O2` the `[rir-low-why]` census printed **no `asm=` line at all** — 0 of 2456 bodies, 0 of 374,527 nodes. The 9 `__asm__` sites in `src/` are top-level naked asm or per-arch `#if` arms dead on arm64. The blockers that *are* real: `global` (2131 bodies / 33,301 nodes), `call` (1968 / 18,564), `frame` (843 / 48,387), `type` (749 / 5,620), `reg` (246 / 2,374).

## F. PARTITIONING POLICY — what goes to the GPU, decided when

> **Amended 2026-08-08 by the reframe.** This whole cluster used to answer "what do we
> move?". Under cluster S it answers a smaller question: **"what may bid?"** The verdict
> itself is S3b's measurement, taken at runtime, per slice, per batch size. F rows that
> read as *decisions* are now *filters*, and F3's conflict with H5 dissolves — the
> census mode and the performance mode stop being the same knob.

| | question | options |
| --- | --- | --- |
| **F0** | *(new)* who decides | **a.** ~~the partitioner, at compile time~~ — the frame this cluster was written in<br>**b.** ★ **the bench decides; the partitioner nominates.** Compile time produces a *candidate set* (F2a's per-node verdict array, demoted to a filter) and a batch-shape estimate; runtime produces the verdict via S3b. The two must not be conflated in the reporting either: H3's escape census now needs a class for **`bid-lost`** — a slice the device *could* have run and was measured slower — distinct from `unsupported-kind`, which it could not run at all. **These two numbers have opposite meanings and the old census had one bucket for both** |
| **F1** | Unit of offload | **a.** a slice/expression (today)<br>**b.** ~~maximal invoke-free region first, then whole functions~~ — **WRONG as an ordering**: region dispatch never breaks even (see the break-even table below). Useful only as a Phase-2 *correctness bring-up* granularity<br>**c.** ★ **whole function from the moment D4b exists**, whole program for the headline. **1860 of 2452 bodies (75.9%) contain zero external invokes**, carrying 52.9% of static nodes and 52.6% of dynamic weight — three-quarters of all functions are offloadable with a single entry crossing |
| **F1′** | the break-even that decides it | Latency at which crossings × latency = the 0.093 s baseline:<br>**D4a (every invoke)** ~241 M crossings → **0.39 ns**<br>**D4b (external only)** 944,327 measured → **98 ns**<br>**D4b + D2b** (device `str*`/`mem*`) 210,018 → **443 ns**<br>**D4b + D2b + B3b** (syscalls only) 9,600 → **9.7 µs**<br>**Only the last row is inside any plausible dispatch latency.** At an optimistic 1 µs, D4b alone is 10.2× baseline and D4a is 2,589×. The D-track measurement decides *how far* the last row clears — it cannot rescue the rows above it |
| **F2** | When is the decision made? | **a.** ★ **compile time**, recorded on the node — **but in a NEW SoA array, not the 41 spare `fbits` bits.** `fbits` is mixed into the node hash (`src/mccast.c:4030`) and compared in `ast_ident_same_scan` (`:7479`), so a verdict stored there would perturb CSE/dedup and put K3 byte-identity at risk. Cost of a new array: +1 byte/node, and arenas are per-body and freed (`:18485-18486`), so peak is the largest body (7019 nodes → 8 KB). Touch points: the `AST_REGROW` block (`:122-145`), `ast_arena_new`/`_free`, and **`ast_slice_graft_rec` (`:1244-1252`), which copies fields one by one and would silently drop the verdict on clone/splice** |
| **F3** | Cost model | **a.** ★ **always offload, in census/conformance mode only** — `MCC_GPU_EXEC_CENSUS`, whose purpose is to measure the eligibility ceiling and to force every kind through the device for conformance. It is not a performance mode and never was<br>**b.** ~~offload unless provably tiny and invoke-adjacent~~ — **superseded by S3b, not kept.** A static "provably tiny" predicate is the wrong instrument: the quantity that decides is batch size at the call site, which is not a property of the slice. The old note that this is "near-worthless anyway — 500 bodies account for 99.8% of dynamic weight" cuts the other way now: **it means the bench has to adjudicate very few slices**, so per-slice measurement is affordable<br>**c.** ★ **the measured verdict (S3b), cached per `(slice ident hash, batch bucket)` and persisted** the way `ast_slice_graduate_arena` already persists gate masks (`src/mccast.c:15649`) |
| **F4** | RIR's role | **a.** ★ the production RIR arena (`MCC_RIR_PROD`) is the source of truth — **but quote `kept_coverage`, not the `modelled` 100.000%/99.965%**, which by construction counts bodies that replayed to *different bytes*.<br>**RESOLVED 2026-08-08 — do NOT bank macho lowerable floors; narrow the skip instead.** `arena.residual` is **not** per-format in the schema while `lowerable` is, and on Darwin `residual` is legitimately **120** (`mcc_tlv_thunk` is raw asm with no C body), so `--update-bank-low` on a Darwin host would convert an honest 77 into a **hard failure on a correct number**. The right fix is to split `tools/rir-coverage.py`'s whole-run `return 77` so a missing *lowerable* floor skips only the lowerable comparison — `kept_coverage` and `capture` are target-derived, not host-format-derived, and there is no reason they are unreachable on Darwin. Two live bugs found alongside: **`wide`'s `lowerable` is still the legacy flat form, so `rir-coverage-census` silently 77-skips on PE as well as macho**; and the `kept_coverage` gate added by `78d4856f` **has never been run against a clean HEAD build** — the bank is 29 commits stale |
| **F5** | close the opcode gap how? | **a.** ★ **amended: a per-opcode histogram excluding the five CFG opcodes**, then handlers for the **four opcodes that actually fire** — `RETVAL`, `MKPTR`, `VPUSHSYM` — plus unblocking `LOAD` from the `src/mccrir.c:4678` `continue`<br>**b.** ~~write all 25 handlers~~ — 21 of them fire zero times, 5 are `#ifdef`-ed out on arm64<br>**c.** ~~bare `rir_arena_mismatch++` in `default:`~~ — **would break the compiler**: 68% of its hits are benign CFG opcodes handled in other switches, and wiring it to the mismatch counter would refuse ~74% of all bodies |

## G. CONFORMANCE & TDD — how each node kind gets proven

| | question | options |
| --- | --- | --- |
| **G1** | The unit of test | **a.** ★ one directed test per `AstKind` × permutation, **amended: table-driven and machine-generated**, one ctest per row, with a per-kind hit bitmap asserted `== (1<<AST_KIND_COUNT)-1`. ~2,180 leaf cases makes hand-writing impossible |
| **G2** | The oracle | **a.** golden output files — rejected, brittle<br>**b.** differential against a CPU reference interpreter over the **same** lowered array — catches **device** bugs<br>**c.** differential against native codegen — catches **lowering** bugs<br>**d.** ★ **b and c as two tiers.** They are not substitutes (see findings) |
| **G3** | Order of work | **a.** breadth: all 14 kinds at `int32` before any widening<br>**b.** ~~depth-first per kind~~ — **WRONG as stated**: you cannot finish `AST_Binary` on three backends before `AST_BasicBlock`/`AST_Return` exist, since there is no way to get a value out<br>**c.** ★ **hybrid — breadth-first across all 14 kinds on the reference interpreter at int32 (= Phase 0), then depth-first per kind across the three backends** |
| **G4** | Fuzzing | **a.** ★ **staged — REORDERED 2026-08-08, and G1 now comes first.** Stage 1, free today: harvest real arenas via `MCC_ARENA_DUMP`; `cmake/spvgate_real.cmake` already does this. **But the plan's argument against synthesis does not survive tracing.** Of four historically discovered defects, three were *hand-authoring errors in a 13–38 case set* — the `TOK_LT` case was written with `'<'` (`TOK_SHL` is 60, `TOK_LT` is 0x9c); the `9 * x` crash was "they all put the constant second", which is literally G1's operand-const axis; and the int64 case *does* build the discriminating shape, so "0 mismatches" is indistinguishable from "0 points compared" because `spvgate` prints `OK` for a case that lowered nothing. **The real axis is enumerated vs hand-written, not synthetic vs real.** So: G1's table first (it closes three of the four classes by enumeration), then a **hybrid mutator over harvested arenas** (~40 lines on `arena_mode`: retag ops, types and kinds, which reaches `Poison` and `GGOTO` while keeping real tree shapes), then a free-standing AST generator **last or never** — real corpora already contain `Binary` with 1 and 3 children, `Return` with 0, `BasicBlock` with 93. Stage 1's ceiling is measured: **13/14 kinds, 6/20 `Ref` forms, 18/26 binary ops, 0 `Poison`, 0 computed goto** |
| **G5** | Reuse of existing corpora | **a.** ★ **`tests/exec` is the vehicle** — 343 golden rows / 294 executable, and CMake already regex-scrapes `goldens.h` into 4 test loops (`CMakeLists.txt:3781-3822`). A 5th `exec-gpu/` loop with `MCC_GPU_EXEC=2` is **~6 lines of CMake** for 343 byte-exact whole-program comparisons. Use `tests/run` (15 programs: setjmp, TLS, varargs, long double, inline asm) as the **escape-census** corpus, not conformance. Skip `tests/behavior` (4 cells) |
| **G6** | The `spvgate` gate | **a.** ★ extend `tools/spvgate.c` to **whole-region differentials** — content correct, ~~rationale deleted~~ (it does *not* work without a device)<br>**b.** ★ **prerequisite: make the SPIR-V gate live locally.** CI already does this with `brew install molten-vk` (`.github/workflows/ci.yml:182-185`); the runtime dylib is present but `find_package(Vulkan)` needs the SDK headers. One install turns 3 dead ctest names live |

### Findings — conformance

**The permutation matrix is ~2,180 leaf cases, so generation is mandatory.** Axes: 10 integer type shapes; 20 `AST_Ref` forms (`VT_VALMASK`'s 5 values × `VT_LVAL` × `VT_SYM`, `src/mcc.h:1072-1121`); 32 binary ops (26 token ops in `ast_eval_ladder_binop_ok` + ~6 `AST_OP_*`); ~15 integer unary ops of the ~31 at `src/mccast.c:2074-2122`.

| kind | cases (full width) | at the int32 rung |
| --- | --- | --- |
| `Binary` | 32 ops × 10 types × 4 operand-const patterns = **1280** | 256 |
| `Ref` | 20 forms × 10 types = **200** | 40 |
| `Unary` | 15 × 10 = **150** | 30 |
| `Load`/`Store`/`StoreVal` | 3 × 5 addr forms × 10 types × volatile = **300** | 60 |
| `Convert` | 10 × 10 = **100** | 8 |
| `Literal` | 10 × 7 value classes = **70** | 14 |
| `Invoke` | internal/external/indirect × varargs × 0–8 args × struct-by-value/ret ≈ **40** | 40 |
| `If`/`Jump`/`Return`/`BasicBlock`/`Poison` | ≈ **39** | 39 |
| **total, no interaction terms** | **≈ 2,180** | ≈ 490 |

× 3 backends = ~6,500 executions; × 5 E1 rungs = **~33,000 cell-executions**. The right shape is the repo's own idiom: a checked-in table (like `goldens.h`, or optfire's `counters.txt`) that a generator emits and CMake expands one-ctest-per-row.

- **G2's two oracles catch disjoint bug classes, and the draft blurred them.** G2b (CPU vs GPU over the *same* lowered array) catches device bugs — MSL/SPIR-V op mismatch, sign-extension, definedness. It **structurally cannot** catch a lowering bug: a wrong arena→node-array lowering is wrong *identically on both sides* and passes green. G2c (vs native codegen) catches lowering bugs. The draft's own Phase 0 already says "prove it runs `tests/exec` identically to native codegen" — that *is* G2c, so the table and the phasing contradicted each other. Hence G2d.
- **`ast_eval_binop` (`src/ast_eval_slice.h:70-230`) is the crown jewel and is directly reusable — ~230 lines.** UB is modelled as *refusal* (`return 0`), not as a value: division by zero, `INT_MIN/-1`, shift `<0 || >=width`, and signed `+ - *` overflow are each checked separately at 32 **and** 64 bits. `is64` is already threaded through every arm — so **the E1 int64 rung is deleting refusal predicates, not writing new semantics.** `ast_eval_slice_fit` (`:255`) is likewise reusable, and its value+definedness pair already matches the device ABI's `out[2*t+1]` defined flag.
- **A2b is ~70% specified already** — and **this bullet is now stale in three ways** (superseded 2026-08-08, kept for the record). (1) `354e96f6` widened `MCC_ARENA_DUMP` from 7 fields to **12** — `id kind op type_t ival first_child next_sib type_ref bp bs sym fbits` — plus an `[inv] <node> <callee>` line per `AST_Invoke`, so only `wide hi/r2` is still genuinely absent. (2) **The consumer was not widened**: `rebuild_arena` still does `sscanf(...) != 7` (`tools/spvgate.c:1083`) and the five new fields have **zero consumers**, so the commit is half-landed. (3) The untyped-readback diagnosis was wrong: the type is not in a side table, it is **absent**, because `st_*` is written only from `src/mccrir.c` and only when `rir_stamp_env` is set — `MCC_RIR_STAMP`, **off by default** (`src/mccrir.c:5949`). And the artifact is far broader than `Load`: **39,640 of 39,643 `Binary` nodes (9.2% of the arena, the most important kind for the emitters) read back untyped**, against 7,195 `Load` nodes at 1.7%. Turning the recovery on is free — `MCC_RIR_STAMP=0`, `=1` and `=2` produce **byte-identical objects**, and `=2` takes typed-node coverage from **65.8% to 100.0%**.
- **There is no AST generator anywhere in the tree.** `tests/fuzz/gen.h` generates C *source text* (seeded splitmix64, reproducible) and `runner.c` has a real line-granular ddmin shrinker — but zero RNG exists in `src/`, `ast_slice_search` (`src/mccast.c:16079`) is a *subset selector* over existing subtrees, and `ast_jit_search_vocab` enumerates optimization-gate bitmasks. The text shrinker does not transfer to ASTs.
- **The no-device idiom is mature and has both halves.** `SKIP_RETURN_CODE 77` on 60+ tests, `VK_HOST` exiting 77 — *and* the inverse guard against silent no-op passes: `cmake/ladder_gpu_parity.cmake` `FATAL_ERROR`s on `_disp EQUAL 0` ("zero GPU dispatches, so identical verdicts prove nothing"), `spvgate_real.cmake` on `slices=0`. **The conformance suite must inherit both halves**, and the reference-interpreter tier must be *non-skippable* — it has no device dependency, so it runs unconditionally everywhere; only the Metal and SPIR-V tiers carry 77.

### The day-one failing test

New `suite_noderun()` in `tools/asttool.c`, dispatched from `main()` and appended to the existing `foreach(_an arena clone wide … slice_locate)` list at `CMakeLists.txt:3497`. ctest name **`ast/noderun`**.

```c
static void suite_noderun(void) {
  AstArena *a = ast_arena_new();
  AstLocal lit = ast_node(a, AST_Literal), ret = ast_node(a, AST_Return),
           bb  = ast_node(a, AST_BasicBlock);
  int64_t out = 0;
  ast_set_op(a, lit, VT_CONST); ast_set_type(a, lit, VT_INT, 0);
  ast_set_ival(a, lit, 42);
  ast_add_child(a, ret, lit); ast_add_child(a, bb, ret);
  CHECK(ast_node_exec(a, bb, NULL, NULL, 0, &out) == 1, "BasicBlock{Return{42}} executes");
  CHECK(out == 42, "BasicBlock{Return{42}} yields 42");
  CHECK(ast_exec_kinds_covered() == AST_KIND_COUNT, "every AstKind has an exec case");
  ast_arena_free(a);
}
```

It is red today in the strongest possible way: **`ast_node_exec` does not exist, so the file does not compile.** `ast_eval_slice_rec` has no case for `AST_BasicBlock` or `AST_Return` — both fall to `default: return 0`. Making assertions 1–2 green is ~25 lines: declare `ast_node_exec()` in `mccast.h`, implement `ast_exec_rec` as `ast_eval_slice_rec` plus two cases — `AST_BasicBlock` (children in order, propagate a returned-flag) and `AST_Return` (evaluate child, set flag). **Assertion 3 stays red for the whole project** — that is G1's ratchet, one `uint32_t` bitmap set in the switch, reading 9/14 after the first commit and gating every kind after. Each new kind is then one `case` + one table row + one bit.

## H. MEASUREMENT & RATCHET — the headline number

| | question | options |
| --- | --- | --- |
| **H1** | What is counted? | **a.** static: nodes lowered / total — available today from `MCC_ARENA_DUMP`<br>**b.** dynamic: node *executions* — **measurable TODAY, and measured.** `mcc -ftest-coverage` self-build (0.5 s) then a self-compile (0.93 s, 10× baseline) yields a 4.7 MB gcov-format `.tcov` (`src/objfmt/mccelf.c:1614-1646`): **789,238,394 block executions** over 48,909 blocks, joined to **2380/2452 bodies (98.9% of nodes)**. My earlier "not measurable until Phase 0" was **wrong**<br>**c.** ★ **both, side by side** — and (b) moves to Phase −1/0, because it changes what the headline *means* |
| **H2** | How is it collected? | **a.** ★ a **device-side per-node-kind counter array** in the B1 buffer, plus a host-side counter for escaped nodes; dumped at exit like `ast_ladder_gpu_report` |
| **H3** | The escape census | **a.** ★ every CPU-executed node **classified by reason** (`link-invoke`, `asm`, `longdouble`, `indirect-miss`, `unsupported-kind`, `budget`, `alloc-fail`), plus an **`unmatched-body`** class. **Measured prior:** of 2,290 non-internal invokes, **2,248 (98.2%) are `link-invoke`**, 42 are indirect, and **`asm` is 0**. So the census will be dominated by one class, which is the good case — it means the escape set is a single well-understood thing |
| **H4** | Ratchet | **a.** ★ a **`tests/gpu/exec-bank.json`** modelled on `tests/rir/coverage-bank.json` — good model, **with three fixes**: (i) gate the **escape-class map**, which `coverage-bank` records but never gates (only 11 of 304 leaves are gated); (ii) pair it with I1b's dispatch-count teeth, or it inherits the original's **three silent-pass paths**; (iii) **drop "never totals"** — over-general, see below<br>**b.** ★ **bank the number that means what it says.** `rir-coverage.py:924` defines `modelled = used + (fallback − abort)`, counting bodies that replayed to *different bytes*; `kept_coverage` sits at `:925`. That metric was banked but **never ratcheted** until this study added a gate for it |
| **H4′** | may the bank gate absolute totals? | **Yes, for a fixed input list** — but **the arena-dump half of the evidence is now FALSE.** The `MCC_RIR_PROD=2` results stand: three runs on fixed input gave byte-identical `prod.tsv` and `.o`, and a fixed 120-file census run twice was byte-identical. **The `MCC_ARENA_DUMP` byte-identity no longer holds**: `354e96f6` added `sym` and `type_ref` as raw `(uintptr_t)Sym*` columns, so two identical self-compiles now differ under ASLR (identical under `setarch -R`; the 7-field prefix is still identical). **Any bank keyed on the dump is unbankable until A2's interning lands** — which is the strongest single argument that the encoding must intern pointers rather than emit them |
| **H5** | The final experiment | **a.** ~~`mcc` recompiling `src/mcc.c` with GPU execution on, reporting the H1c pair, the H3 class breakdown, and wall-clock against the CPU baseline~~ — right content, **wrong headline under the reframe**<br>**b.** ★ **the same experiment, with wall clock promoted to the result and the split demoted to a diagnostic.** `mcc` recompiles `src/mcc.c` with the scheduler on; the number is **wall clock against the CPU-only baseline (0.093 s)**; the H1c static/dynamic pair, the H3 class breakdown and the new F0b `bid-lost` count are reported beside it to explain the wall clock, not to stand in for it. A run where 2% of dynamic nodes went to the device and the compile got 15% faster is a **success**; a run where 99% went to the device and it got slower is a failure, and the old headline could not tell them apart. Success criterion still to be set once the first honest number exists |
| **H6** | *(new)* the number that gates day one | ★ **a per-slice CPU/GPU cost table, before any promotion exists.** Instrument both executors (S3b's timed region on the device side, a `clock_gettime` bracket on `ast_eval_slice` on the CPU side), run the existing corpora, and emit `slice-ident, nlive, nodes, cpu ns/tuple, gpu fixed ns, gpu ns/lane, break-even ntuple`. **This is buildable now, needs no interpreter, no address space and no control-flow machine, and it is the only artifact that can tell us whether any of the rest is worth building.** If the break-even column is uniformly above the batch sizes the loop-nest analyses can supply, that is the project's answer and it arrives in Phase 0 instead of Phase 7 |

## I. PORTABILITY & BACKENDS

| | question | options |
| --- | --- | --- |
| **I1** | Backend parity | **a.** three implementations must agree — right in substance, **but "CI already has a cell" does not mean CI has a device**<br>**b.** ★ **a, plus teeth**: every exec parity test emits a dispatch count and its cell `FATAL_ERROR`s at zero (the `ladder_gpu_parity.cmake:47-50` pattern), and the "no usable device" early-return becomes a *loud* skip a cell can be configured to reject<br>**c.** ★ **plus a Linux exec cell with `mesa-vulkan-drivers` (lavapipe)** — one apt package, and it makes the Vulkan arm tested on the only OS where Vulkan is the default backend |
| **I2** | Device feature floor | **a.** ★ declare a minimum feature set and refuse cleanly below it. **RESOLVED 2026-08-08 — ~120 lines, all at init, all zero runtime cost**, of which ~60 is transcribing `VkPhysicalDeviceFeatures`, which today is a **forward `typedef` with no body** while `vkGetPhysicalDeviceFeatures` **is not in the loader table at all**. The list: **(A)** scored device *selection* by `deviceType` with tie-break on limits, never `devs[0]`, plus `MCC_GPU_DEVICE` override and `MCC_GPU_ALLOW_CPU` so lavapipe can be opted into for I1c; **(B)** limits — `maxStorageBufferRange ≥ computed requirement` (not a constant), the four compute-workgroup limits, and **`minStorageBufferOffsetAlignment` honoured at `max(256, reported)`** because this host reports 16 and the spec permits 256, so a hardcoded layout works here and breaks on a conformant-minimum device; **(C)** features — `shaderFloat64 = TRUE` (see E6, and note the floor is safe on lavapipe on a *source* reading of Mesa 26.0.8, not a device reading — no lavapipe ICD exists on this host), `shaderInt64 = FALSE` (E2b emulates), zero extensions, and 32-bit `StorageBuffer` atomics need no bit; **(D) memory-type preference, the highest value per line in the row** — `mcc_gpu_mem_index` takes the *first* `HOST_VISIBLE\|HOST_COHERENT` type, which on the NVIDIA host is system RAM while a ReBAR `DEVICE_LOCAL\|HOST_VISIBLE\|HOST_COHERENT` type and a `HOST_CACHED` type both exist; under B1 that makes every interpreted load and store a PCIe transaction; **(E)** Metal — query `maxBufferLength`, and use `MTLCopyAllDevices` rather than `MTLCreateSystemDefaultDevice`, which is exactly the iGPU-first case this row names.<br>**Do not declare a `maxComputeSharedMemorySize` floor** — B5's conclusion is that mcc uses no shared memory at all, so a floor there re-imports the 512 B/lane trap into the refusal logic for nothing |
| **I3** | Third backend | **a.** ★ not now — reworded to "no third **device** backend"; the CPU reference interpreter is de facto a third implementation and I1 already names it |

## J. FAILURE, DETERMINISM & UB

| | question | options |
| --- | --- | --- |
| **J1** | Equivalence standard | **a.** ★ bit-exact with CPU execution for defined behaviour — **but the enforcement clause was aspirational**: `selfhost-fixpoint`/output-parity test the *compiler's output*, not GPU execution, and only enforce J1 once run with the switch on (that is G5's job) |
| **J2** | UB | **a.** ~~points where the source form is undefined are **vacuous**~~ — **UNSOUND for an execution engine, rejected** (see findings)<br>**b.** ★ **UB must be made deterministic and identical, not vacuous**: every UB-capable operation (signed overflow, shift ≥ width, div by zero and `INT_MIN/-1`, out-of-range float→int, unaligned/OOB access) is pinned on device to bit-match the CPU reference, that behaviour becomes part of the spec, and it is fuzz-tested like anything else. The ladder's vacuity rule stays in the *oracle* and does not move |
| **J3** | Device fault handling | **a.** ~~fault → CPU fallback **for that region**~~ — **DELETED 2026-08-08.** Its hidden precondition ("a region's device writes must be recoverable-or-uncommitted at fault time") is **false by default against B1b+B4b, not merely unproven**: the buffer *is* the address space, it is host-coherent, and `src/mccgpu.c` contains no copy or sync primitive, so every device store is already committed. Nine partial-commitment modes were enumerated; the host can detect *that* a fault occurred in all of them and *what was written* in none. Mode 2 is decisive — E2b makes every `int64` two stores and E4 makes aggregates N byte-ops, so a fault leaves values **no correct execution ever produces**. Region-granularity fallback would then run the CPU correctly on corrupt inputs and emit a plausible wrong object: J3b's own "catastrophic case" wearing a reassuring name<br>**a′.** ★ **NEW — whole-run abandon-and-restart.** On any device fault, discard the B1 buffer **unread**, re-run the compilation from argv with the device off, count one `device-fault` escape in H3. mcc is a deterministic batch compiler, so the whole-run pre-state is free and permanent. Zero device-store cost, zero buffer-byte cost. **Precondition, checkable and written down: no externally visible side effect yet committed** — which couples this row to D1d, the only design element that would violate it<br>**b.** ★ **fail loudly** for: a second fault in the restarted run, a fault past the side-effect watermark, any `PageFault` (that is our own bug), and any unclassifiable fault. `spvgate`'s exit-77 and `ladder_gpu_parity`'s `FATAL_ERROR` are the precedent.<br>**Mechanisms evaluated and rejected, recorded so the question is not reopened:** an **undo log** is a durability technique on a substrate with neither durability nor ordering — the log lives in the same buffer that just became untrustworthy, and a GPU reset need not flush L1/L2; **COW/shadow write sets** cost a probe on every load as well as every store and are unbounded under F1c; **whole-buffer snapshots** are ~80 MB per region ≈ 4 ms, 26× D1a's latency |
| **J4** | Reproducibility | **a.** ~~single-lane until D-parallelism~~ — **AMENDED 2026-08-08 by S5c, and this is a real collision, not a wording fix.** S5's arithmetic says lane parallelism is not one benefit among several; it is the *only* source of a win — below ~72 ns/tuple of CPU work **no batch size breaks even**, and at 100 ns break-even is 1,177 lanes. A single-lane rule therefore guarantees every bid loses, and the two rows cannot both hold<br>**b.** ★ **determinism by construction over the batch, not by serializing it.** Lanes must be *independent by the same analysis that formed the batch* (`ast_loopdep`), so lane order cannot affect the result and the batch is reproducible without being sequential. Single-lane survives only as the **debug and differential mode** — `spvgate`, the G1 conformance matrix and any J1 divergence hunt — where reproducibility is the point and throughput is not. Keep: seeded deterministic fuzzer replay, and **strip timing from any compared output**, as `cmake/ladder_gpu_parity.cmake:36-37` already does with `secs=`<br>**The cost of b:** J1 divergence hunting gets harder, because a wrong answer now has a lane index. The mitigation is that `mcc_gpu_dispatch`'s out slots already carry a per-lane defined flag, so a divergent lane is identifiable rather than merely present |

## K. DELIVERY & GATING

| | question | options |
| --- | --- | --- |
| **K1** | User-facing switch | **a.** `MCC_GPU_EXEC=0/1/2` **plus a CMake option** — **CLOSED as never, 2026-08-08.** Trigger, stated as a number: add the option when the interpreter's measured delta to *one configured binary* exceeds **256 KB, or 10% of the compiler-proper footprint (1,841,954 B → 184 KB today), whichever is smaller.** Estimated actual, from measured densities (16.13 B of `.text` per line of C; 42.9 B/line for MSL-as-embedded-text): **24–127 KB — 1.4× to 7.7× below the trigger**, in a 4,942,064 B binary whose `mccjit_blob.c.o` alone is **2,970,706 B (60.1%)**. And the exclusion mechanism K1a would provide **already exists**: `MCC_GPU_LANG_MSL` `#if`s out an entire emitter arm, so A4a's "hand-written twice" never doubles one binary. Corroboration: `989e4b3b` added +1,319/−235 lines to `src/mccgpu.h` — a half-interpreter-sized change in the exact file — with no size discussion and no option<br>**b.** ★ **env var only, permanently.** K3 stays structurally trivial: the object cannot change. Read via `mcc_env_num`, **never `mcc_env_on`**, which collapses 1 and 2. Freeze the tri-state at `{0 off, 1 on/fallback, 2 on/required}` and give every orthogonal axis its own variable (`MCC_GPU_EXEC_UNIT`, `MCC_GPU_EXEC_CENSUS`), the way the RIR board already splits `MCC_RIR_PROD`/`MCC_RIR_LOW`/`MCC_RIR_STAMP`<br>**c.** ★ **plus the standing rule that every phase lands with ≥1 ctest cell setting `MCC_GPU_EXEC=1`** — and the cell must emit an exec count that `FATAL`s at zero, or the rule buys a green cell that proves nothing. Measured bit-rot ratio: **68 `rir` cells against exactly 1** that forces `MCC_AST_EVAL_LADDER`. "≥1 per phase" is a floor against zero, not a target |
| **K2** | Landing shape | **a.** one big branch<br>**b.** ★ **incremental on `main`, gated off** — the RIR board did exactly this and is the healthiest subsystem in the tree |
| **K3** | Risk to existing behaviour | **a.** ~~proven with `cmp`, "the way the ladder toggle was"~~ — **the cited precedent is not an enforcement mechanism**: it was a one-time manual check<br>**b.** ★ **an automated ctest**, `gpu-exec-byte-identity`, compiling `src/mcc.c` at `-O0/-O1/-O2/-O3` with the switch unset vs set-to-0 and `cmp`ing the objects. Model it on `tests/ci/target-link-gate.sh`, which already compiles `src/mcc.c` standalone across eight configurations |

### Findings — portability, determinism, delivery

**No CI cell is proven to have a real GPU, and the design makes device loss invisible.**
Only `.github/workflows/ci.yml` mentions GPU at all. Linux stage2 (`:115`) installs
`libvulkan-dev` — **loader and headers only, no ICD**; no lavapipe, no SwiftShader.
Windows (`:223`) installs `vulkan-headers` via vcpkg, explicitly "so rir-coverage
matches the bank". The `gpu-vulkan` cell (`ci.yml:182`, `tools/ci.c:668,696`) is
**macOS-only** and installs MoltenVK, so its device is whatever the GitHub `macos-15`
VM exposes to Metal — and **the repo contains no assertion that this is a real GPU**.
Meanwhile every GPU test degrades to a *green pass* with no device:
`cmake/ladder_gpu_parity.cmake:27-30` matches `available=0` and `return()`s with exit 0;
`spvgate_real.cmake:27` and `spvgate_mutate.cmake:4` print "no usable device, skipping"
and succeed. **CI would stay green if both device backends were deleted.** The one
real tooth is `ladder_gpu_parity.cmake`'s `FATAL_ERROR` on `_disp EQUAL 0`.
→ I1 needs amending to **(b): every exec parity test emits a dispatch count and the
cell fails at zero**, plus a Linux exec cell with `mesa-vulkan-drivers` (lavapipe) —
one apt package that converts the Vulkan arm from untested to tested on the only OS
where Vulkan is the default backend.

**Byte-identity was a one-time manual `cmp`; there is no automated enforcement.**
`MCC_AST_EVAL_LADDER` appears in five files and has **zero hits in `CMakeLists.txt`,
`tests/`, `tools/`** — it is not a CMake option and never was, just a runtime env var
read by `mcc_env_on()` at `src/ast_eval_slice.h:757`. The TODO's "proven with `cmp`"
is a historical observation with no regression test. Note the asymmetry: the ladder's
guarantee is *structurally trivial* because the toggle is runtime and the object
cannot change. **K1a's CMake option would forfeit that**, which is why K1b now leads.

**On bit-rot, the evidence is unambiguous and it is not about the default.**
`MCC_RIR_PROD` is default-off (`src/mccrir.c:5892`) and is thoroughly maintained —
`rir-coverage`, `rir-gap-classes`, `rir-lowerable-classes`, `rir-nofb-probe`,
`ast/rir-parity-{O0..O3}`, `ast/rir-c2-*` all force it on. `MCC_AST_EVAL_LADDER` is
also default-off, has one-and-a-half cells, and is the one that rotted — "zero device
work beyond a one-per-process warm-up across 600 programs". **Default-off features
stay alive exactly in proportion to the number of CI cells that force them on.**

**J2 was unsound, on three independent grounds.** The vacuity rule is real and is at
`src/ast_eval_slice.h:983-985` (`ast_eval_ladder_point` discards a point when the
source form fails), and J2a described it correctly *as a property of the ladder*. It
does not transfer:
1. **An oracle can skip a point; an engine cannot skip an execution.** When the ladder
   hits signed overflow it deletes that input tuple from the comparison. When a running
   program hits it, the device produces a value, the CPU produces a value, they may
   differ, and **the program continues from that value** — divergence propagates into
   subsequent *defined* computation, and then output-parity fails on a difference J2a
   said to ignore. "Vacuous" has no meaning once state is threaded.
2. **The CPU side is no longer a reference, it is a participant.** Under J3 the same
   run may execute region R on device and R′ on CPU; if R's UB result feeds R′, the
   *composition* is what is observed, and there is no source-form/replacement-form pair
   left to compare.
3. **mcc's own self-compile almost certainly contains UB** — TCC-derived C, decades of
   pointer punning, `-w` throughout the corpus. The benchmark program is the one where
   "don't chase UB divergence" is least affordable.

**The feature floor has no foundation in code today.** Current actual floor
(`src/mccgpu.c:1180-1300`): Vulkan 1.1, `pEnabledFeatures = NULL`, **zero device
extensions**, physical device **`devs[0]` with no scoring**, first compute queue,
`HOST_VISIBLE|HOST_COHERENT`, `STORAGE_BUFFER`. Metal assumes
`MTLCreateSystemDefaultDevice` and `options = 0`. What exec adds: `Int64` (optional,
avoidable via E2b), host-coherent (already required), storage buffers (universal), and
**atomics — where the news is good: `SpvCapShader` already grants `OpAtomic*` on
`StorageBuffer` for 32-bit ints, so the D1b doorbell needs no new capability at 32
bits.** I2 must also add **device selection** (`devs[0]` is a bug waiting for a machine
that lists an iGPU first) and a `maxStorageBufferRange` check — the B1 buffer is the
whole address space and the Vulkan floor for that limit is 128 MiB, which will bind.

**What the existing guarantees actually prove:** `selfhost-fixpoint*` is 3-stage
(o1==o2, o2==o3) and catches nondeterminism but **not a *stable* miscompile**
(`tools/selfhost-fixpoint.py:1-12`); `selfhost-output-parity-{O2,O3}` compiles and runs
all of `tests/exec` under both stages and compares stdout — **that is the one that
catches stable miscompiles**; `jit/selftest-stage2` covers call-bearing JIT recompile.

## L. DEVICE LIFETIME & RESIDENCY — the device is a JIT-lifetime resource

**The decision, stated once.** The device stops being a per-dispatch resource and
becomes a **process-lifetime resource owned by the JIT**: brought up once by the same
`.init_array` constructor that boots the JIT, held warm and resident for the life of
the process, dispatched by the JIT's own worker pool, routed to per function through
the JIT's own indirect-jump slots, and torn down exactly once by a JIT shutdown path
**that does not exist today and is created by this cluster**.

Grounding, verified at `b740ae46`:

| thing | state |
| --- | --- |
| JIT boot | an emitted ELF constructor `__mccjit_boot_all` (`src/mccjit_embed.c:2013-2031`), `.init_array`, **before `main`**, iterating `__mccjit_registry[]` (`:2000-2006`) and calling `mccjit_boot_swap` (`:1889`) or `mccjit_boot_swap_async` (`:1899`) per baked function. Kill switch `MCC_JIT=0` |
| **the GPU seam already exists, in the wrong shape** | `mccjit_boot_swap` calls `ast_ladder_gpu_setup()` at `src/mccjit_embed.c:1893` and `_async` at `:1906` — but **once per registry entry**, and what it does is emit a literal-7 kernel and *dispatch* it (`src/mccast.c:15891-15900`). That is a warm-up, not a bring-up |
| GPU init | lazy on first dispatch, `static` per backend (`src/mccgpu.c:204` Metal, `:1199` Vulkan), reached only from `mcc_gpu_dispatch_locked` (`:336`, `:1390`). The guard is `mcc_gpu.tried`, set **before** the work (`:209-211`, `:1212-1214`), so a failed init is sticky and never retried |
| GPU teardown | `mcc_gpu_quiesce` (`:241` Metal, `:1299` Vulkan) has **exactly one caller**: `ast_ladder_gpu_report` (`src/mccast.c:15909`), registered `atexit` at `src/mccast.c:15881` **only when `MCC_AST_EVAL_LADDER_GPU` is set**. In a normal build nothing ever quiesces |
| **JIT teardown** | **there is none.** The engine's only exit hook is `atexit(mccjit_kgc_flush_all)` (`src/mccjit_embed.c:2810`) plus `mcc_stats_set_flush_hook` (`:2811`). Pool workers are `pthread_detach`ed (`:1375`) into an infinite `cond`-wait loop (`:1341`) and are never signalled, joined or drained — a process can exit with a build in flight |
| what is resident today | Metal: `dev`, `queue`, and a 64-entry PSO cache (`src/mccgpu.c:98`, `MCC_MTL_CACHE_MAX 64` at `:59`). Vulkan: `inst`, `phys`, `dev`, `q` — **and nothing else**. `vkCreateComputePipelines` is called with `VK_NULL_HANDLE` for the pipeline cache (`:1471`), so even the shader compile is repeated every dispatch |
| the per-dispatch teardown | Vulkan destroys ten object classes every dispatch (`:1515-1535`); Metal releases both buffers and drains an autorelease pool (`:392-396`) |
| the lock | `mcc_gpu_lock`, a real `pthread_mutex_t` on POSIX (`src/mccgpu.c:24`) and **`((void)0)` on Win32** (`:28-29`). `mcc_gpu_stats` (`:1545`) reads the counters without it |
| the hot-patch slot | every JIT-targeted function begins with an indirect jump through `__mccjit_slot_<fn>`: `ff 25 disp32` on x86-64 (`src/mccast.c:18106-18111`), `adrp/add/ldr/br x16` on arm64 (`:18194-18199`), with the AOT body spliced immediately behind it (`ast_baseline_splice`, `src/mccast.c:3552`). Takeover is a single release store, `mcc_jit_publish` (`src/mccjit_embed.c:9414-9421`) |
| what the current shape costs | **130 ms one-time init against 117 µs per dispatch** on the Linux/NVIDIA host, with 24 ioctl / 3.9 openat / 2.3 mmap / 2.1 munmap **per dispatch** |

| | question | options |
| --- | --- | --- |
| **L1** | When is the device brought up? | **a.** lazy on first dispatch (today) — the 130 ms lands inside whichever dispatch happens to be first, and "is there a device" is unanswerable until after the first offload decision has already been made<br>**b.** ★ **once from `__mccjit_boot_all`**, beside `mccjit_feasible()` (`src/mccjit_embed.c:1891`), under a real `pthread_once` rather than a pre-set `tried` flag. Replace `ast_ladder_gpu_setup`'s literal-7 *dispatch* with a `mcc_gpu_boot()` that creates instance/device/queue/pipeline-cache/resident buffer and returns a verdict; keep the dispatch only as an opt-in self-test. **Move the call out of `mccjit_boot_swap` and into the constructor**, so it fires once rather than once per baked function<br>**c.** explicitly from `mcc_new()` — wrong altitude: the compiler creates many states per process and the device outlives all of them |
| **L2** | What owns teardown? | **a.** today's env-gated `atexit(ast_ladder_gpu_report)` — in a normal build, nothing<br>**b.** ★ **a new `mccjit_shutdown()`**, registered `atexit` exactly once, which **drains the pool, then quiesces and destroys the device, then flushes KGC**, ordered ahead of `mcc_stats_finish`'s hook (`src/mccstats.c:541-546`). **This row is a construction, not a move** — see the three preconditions below<br>**c.** leave it to process exit — forfeits the leak-clean exit an `--embed-jit` program needs and leaves a live command buffer racing process teardown |
| **L2′** | preconditions L2b must fix first | **(i)** pool workers are detached and un-drainable (`src/mccjit_embed.c:1375`, `:1341`); a quit flag + `pthread_cond_broadcast` + join, or a counted in-flight barrier, has to exist before "drain" means anything. **(ii)** `mcc_gpu.ok` is never cleared after `VK_ERROR_DEVICE_LOST`, so the unbounded `vkDeviceWaitIdle` in `mcc_gpu_quiesce` (`src/mccgpu.c:1303`) **deadlocks the process from `atexit`** after a hang — today that is unreachable because nothing registers the hook; L2b makes it reachable on every run. **(iii)** Vulkan quiesce destroys **nothing** (`:1299-1305`): no `vkDestroyDevice`, no `vkDestroyInstance`, and the `dlopen` handle is never closed anywhere in `src/mccgpu.c`. A JIT-lifetime device must tear those down or the coupling buys nothing over process exit |
| **L3** | What becomes resident? | ★ the entire non-per-dispatch object set: instance / physical device / device / queue, **a real `VkPipelineCache`** (currently `VK_NULL_HANDLE`, `:1471`), the descriptor set layout, the pipeline layout, the interpreter pipeline, one command pool, a ring of command buffers, the fences, and **the B1 address-space buffer**. Metal: device, queue, the existing PSO cache extended to hold the interpreter PSO, and the resident buffers.<br>**Note what this row is and is not.** The held-buffer experiment was implemented, A/B'd and reverted on a refuted rationale — fresh vs held was within noise at every size from 64 to 262,144 tuples. That measurement stands and is **not** an argument against L3: residency here is a **correctness precondition for B1** (the address space must survive a dispatch, and a resized buffer is a different `VkBuffer` whose binding is recorded at encode time), and the object that actually costs something is the **pipeline**, which Vulkan rebuilds every dispatch with no cache at all |
| **L4** | Who dispatches? | **a.** the calling thread under one global mutex (today)<br>**b.** ★ **the JIT's existing pool** — `mccjit_pool` (`src/mccjit_embed.c:1302-1309`), same queue, same condvar, same `pthread_atfork` discipline — plus one **device thread** that owns queue submission, so command-buffer ordering is single-writer and D1e's pre-enqueued resume chain has an owner<br>**c.** a separate GPU-only pool — a second lifetime to shut down, for no gain<br>**Hard constraint on b:** a device dispatch or pipeline build must **never** hold `mccjit_swap_lock` (`:1300`), which serializes every JIT compile process-wide. A 15–25k-word module build is plausibly seconds on MoltenVK (SPIRV-Cross → MSL → Metal); holding that lock would stall all JIT compilation behind one shader |
| **L5** | How does a function get routed to the device? | **a.** a dispatch decision inside the interpreter<br>**b.** ★ **the existing publish.** A GPU-resident variant is stored into `__mccjit_slot_<fn>` by `mcc_jit_publish` exactly as a JIT variant is — release store plus `host_icache_flush`. The slot is already the process's single switch point per function, the AOT body already sits behind it as a permanent fallback, and the whole-function offload unit F1c demands is precisely the unit the slot addresses. **Consequence for J3a′:** a device fault becomes a **slot re-publish back to the AOT body** for the per-function case, and whole-run abandon-and-restart is needed only when the fault happened past the side-effect watermark<br>**c.** patch call sites — rejected; the indirect slot exists so nothing has to |
| **L6** | Which functions are eligible? | **This is where the coupling costs something and it must be decided explicitly.** `ast_jit_want` (`src/mccast.c:1776`) requires `ast_jit_eligible` (`:1753`): no varargs, no `switch` in the body, ≤6 params, **≥1 param**, every parameter and the return in the scalar GP/double set — plus no VLA in the body (`:1779`). Those are **host-ABI trampoline constraints, not device constraints**, and they are far narrower than the 99.4% ceiling.<br>**a.** reuse `ast_jit_eligible` unchanged — cheapest, and forfeits the headline<br>**b.** ★ **a second predicate `ast_gpu_want`** that shares the slot machinery and the `-jit-functions` selection (`ast_jit_selected`, `:1720`, where an empty list already means *every* function) but carries **device** constraints: no `long double`, no inline `asm`, no computed goto. Eligibility must not be inherited from a trampoline's register budget<br>**c.** widen `ast_jit_eligible` itself — couples two unrelated things and puts the JIT's own admission at risk |
| **L7** | fork, threads, Windows | ★ three hazards this coupling **creates**, none of which bites today because exactly one thread ever dispatches and it dispatches almost never:<br>**(i) `fork`.** `pthread_atfork` (`src/mccjit_embed.c:1336`) resets the pool in the child (`:1324`). Vulkan and Metal handles are **not** valid across `fork`; the child handler must mark the device dead (`ok = 0`, `closing = 1`) rather than inherit it.<br>**(ii) Windows.** `MCC_GPU_LOCK`/`UNLOCK` are `((void)0)` on non-POSIX (`src/mccgpu.c:28-29`). With a pool dispatching, that is a live data race on `mcc_gpu.dispatches`/`lanes`, on the Metal PSO cache and on the resident buffer. Needs a real critical section before L4b lands.<br>**(iii) unlocked stats.** `mcc_gpu_stats` (`:1545`) reads the counters outside the lock — harmless today, torn once the pool dispatches |
| **L8** | `--embed-jit` | ★ **the device is `dlopen`ed at boot, never linked.** The precedent is in the tree and was established for exactly this reason: `objc_getClass`/`sel_registerName`/`objc_msgSend` are resolved through `host_dlsym_process` at Metal-load time rather than linked, because *"an `--embed-jit` program has no business linking libm"* (`src/mccgpu.c:32-33`). Same rule here. `MCC_JIT=0` must leave the program with **no device bring-up at all**, and a missing ICD must be a clean refusal at boot, not a boot failure |

### Findings — lifetime, verified 2026-08-08 at `b740ae46`

- **The seam is already wired, which makes L1 small and L2 large.** `mccjit_boot_swap`
  already calls `ast_ladder_gpu_setup()` (`src/mccjit_embed.c:1893`, `:1906`), so
  "the GPU comes up with the JIT" is a two-line relocation into the constructor plus a
  change of what the callee *does*. There is no matching seam on the way out: the JIT
  has no shutdown function, no drain, and no ordered exit beyond one `atexit` KGC
  flush. **"Torn down once, with the JIT" therefore means building the JIT's teardown
  as a prerequisite**, and that is the larger half of this cluster.
- **The 130 ms is currently paid in the worst possible place.** Init is lazy and its
  guard is set before the work succeeds, so the first dispatch on a machine with a real
  ICD absorbs a 130 ms stall and a machine without one absorbs it as a sticky failure
  that is never retried. Under L1b the cost lands in `.init_array`, concurrent with the
  JIT's own boot, and the verdict is available *before* the first offload decision.
- **Vulkan rebuilds the shader every dispatch.** `VK_NULL_HANDLE` at
  `src/mccgpu.c:1471` means no pipeline cache exists at all, and the shader module,
  pipeline layout and descriptor set layout are created and destroyed alongside it
  (`:1456`, `:1462`, `:1417` → `:1525`, `:1523`, `:1529`). Metal is better only by
  accident — it has a 64-entry PSO cache keyed on an FNV-1a of the MSL text
  (`:98`, `:251`, `:266-268`). Under an interpreter kernel there is **one** module for
  the life of the process, so this row converts a per-dispatch driver compile into a
  single boot-time one. That, not buffer reuse, is where the residency win is.
- **L5 makes the offload decision use machinery that is already load-bearing.** The
  slot is not a convenience: it is how the JIT already achieves atomic, lock-free,
  per-function takeover with a permanent fallback body sitting six (or sixteen) bytes
  behind the entry. Reusing it means the GPU path inherits `mcc_jit_publish`'s release
  store and icache flush, the AOT baseline, and `-jit-functions` selection, and it means
  device-vs-host is decided at exactly the granularity F1c argues for.
- **But L6 is a real narrowing and the plan must not paper over it.** `ast_jit_eligible`
  refuses varargs, `switch`, >6 params, zero params, and any non-scalar parameter or
  return. A GPU path that inherits that predicate cannot reach 99.4% of nodes — it
  cannot even reach the front end, since the parse path is full of `switch`. `ast_gpu_want`
  has to be its own predicate from the start.
- **Correction to N6.** The `int32_t pin[64], pout[128]` stack overflow at the warm-up
  site is **fixed at `b740ae46`**: the buffers are heap-allocated and slot-sized at
  `src/mccast.c:15794-15798`, with frees on both the `out:` and `bail:` paths
  (`:15859-15861`, `:15866-15868`). The row below is retained for the record only. The
  warm-up *dispatch* it lived in is what L1b deletes.
- **Correction to the phase-1 buffer note.** The claim that a persistent address space
  is blocked by per-dispatch buffer lifetime is right; the claim that fixing it is also
  free performance is not — the held-buffer A/B found no win at any size and was
  reverted. L3's justification is correctness and the pipeline cache, not latency.

---

# Decisions resolved — 2026-08-08

Eight parallel investigations, one per coupled group of open rows, each grounded in the
code and measuring where it could. The first pass over this document classified 52 micro
rows as 4 landed / 33 ratify-only / **14 genuinely open** / 2 deferred. All fourteen now
carry a recommendation. **The shape inverted: most of them turned out to have answers
already sitting in the tree** — the serializer, the emitter-arm exclusion, the frame
hoist, the stype recovery. What remains is one experiment, one unowned sizing problem,
and a list of bugs.

| row | resolution | the reason it turned |
| --- | --- | --- |
| **A2** | `mccjit_intent` format 14 as host wire format + a 32 B/node device projection | the serializer exists — `ROLE_FUNC` *is* D3's descriptor, `ROLE_STRUCT` *is* E4's layout |
| **B2** | 64 MiB fixed, no growth protocol in v1 | growth cannot be serviced mid-kernel: a larger buffer is a *different* `VkBuffer`, bindings are recorded at encode time |
| **B3** | fuse the SoA, funnel through the hook, 8-class free list | grow-in-place is not semantically required anywhere; `free` reclaims 63–73%, so a pure bump is unaffordable |
| **B5** | variable frames, one device `sp`, 256 KiB/lane | fixed slots cannot express the `alloca`/VLA that `decl` uses on the parse path |
| **C3** | step budget `1<<20`, suspend at loop top, bank the rule | overhead depends only on round *duration*; total work cancels; the window is 33× wide |
| **C4** | dynamic byte cap, 128-level equivalent, merged with D4b's guard, CPU first | a cap firing only on device *is* a J1 divergence |
| **F4** | narrow the skip; do **not** bank macho floors | `arena.residual` is not per-format and is legitimately 120 on Darwin |
| **G4** | G1's table first, hybrid mutator second, generator last or never | 3 of 4 historical misses were hand-authoring errors, not synthesis limits |
| **I2** | ~120 lines at init; the list is in the I2 row | `VkPhysicalDeviceFeatures` has no body and its getter is not in the loader table |
| **J3** | delete region fallback; J3a′ whole-run abandon-and-restart | "uncommitted" is false by default on a coherent buffer |
| **K1** | never add the CMake option | 24–127 KB against a 184 KB trigger; the exclusion mechanism already exists |
| **N1** | ~~buffer cache now, ctx struct before Phase 1~~ — **SUPERSEDED by cluster L.** The device is a JIT-lifetime resource: up once with the JIT, resident, torn down once with the JIT | the buffer-cache half was A/B'd and reverted (no win at any size); the real lifetime question is ownership, and the JIT already owns an equivalent one |
| **N2** | raise **both** caps to `1<<20` words / 4 MB + an incremental budget | the MSL cap must move too; the 512-constant ceiling is irrelevant to a hand-written interpreter |
| **N3** | stay build-time; add a Linux ICD cell | a runtime selector is impossible on Linux — the Metal arm needs `<objc/message.h>` |

### And the rows the 2026-08-08 reframe turned

| row | resolution | the reason it turned |
| --- | --- | --- |
| **A1/A3** | **emitter first, interpreter conditional** — reversed | the interpreter's case was totality; the reframe deletes totality as a precondition, and the emitter can produce the deciding measurement today |
| **F0/F3** | the bench decides, the partitioner nominates; F3b superseded | "provably tiny" is a property of the slice, but the quantity that decides is batch size at the call site |
| **H5** | wall clock is the headline; the node split is a diagnostic | the old headline cannot distinguish "2% offloaded, 15% faster" from "99% offloaded, slower" |
| **H6** | *(new)* a per-slice CPU/GPU cost table, bankable now | no promotion decision exists without it, and it needs nothing else built |
| **J4** | determinism by construction over an independent batch; single-lane demoted to debug mode | S5's arithmetic makes lane parallelism the only source of a win, so single-lane and the plan cannot both hold |
| **S1–S8** | *(new cluster)* the device is a candidate in the JIT's existing promotion machine | the pool, selector, margin gate, differential tier and publish path are all already written and none of them has ever been pointed at the device |

## Still genuinely open

| | question | why it cannot be closed from here |
| --- | --- | --- |
| **N13** | the must-run manifest | 138 `SKIP_RETURN_CODE 77` sites and nothing asserts any of them must fire |
| **N14** | per-lane writable globals | 5.82 MiB × 64 lanes = **373 MiB, 4× the floor before a single stack frame exists** |
| **S3′** | *(new)* what a fair batched bench looks like | `mccjit_bench_pair` runs up to 16 sibling threads against the same pair (`:3360-3390`). A device candidate serializes on one global lock and one queue (`src/mccgpu.c:18-30`) while the CPU arm runs 16-way parallel — **a systematically pessimistic comparison that no choice of margin fixes.** Either arm can be made single-sibling; which one is correct depends on whether the real workload has one hot slice or many, and that is not knowable from here. **Needs the H6 table first** |
| **S5′** | *(new)* can `ast_loopdep` actually supply 1,177 lanes? | S5's break-even is arithmetic from measured constants and is not in doubt. What is unknown is the **distribution of independent-iteration counts** in mcc's own hot loops. `ast_loopnest_build`/`ast_loopdep` exist and are used for other purposes; nobody has ever asked them this question. **This is the single measurement that decides whether the project has a subject** — if the answer is "hot loops are 8–40 iterations", the emitter path wins nothing at any level of eligibility and only D-track crossing reduction remains. Measurable today, needs no device |

**M5 / D1e and E6 were both closed on 2026-08-08**, measured on an Apple M1 Pro; the
full numbers are in [`docs/TODO.md`](TODO.md).

- **M5 / D1e — YES, and better than projected. D1e is the recommendation.** Measured
  **12.4 µs median / 19.1 µs mean**, against the ~30 µs projection and D1b's 24 µs. The
  projection was one CB period pessimistic: the measured mean *is* one pipelined CB
  period. Note this was run **without** cluster L — the harness stands up its own chain,
  so the row's "blocked on a resident command-buffer ring" amendment was true of the
  compiler and not of the experiment. The load-bearing visibility claim holds with **no
  sweep**: ~800,000 host→device posts, 0 stale reads, per-crossing stale rate bounded at
  **< 4 × 10⁻⁶**. A 32 KB sweep costs 6.5× and buys nothing, so **D1e does not collapse
  into D1b and N5's sweep constant is obsolete**. The **~64-CB deadlock is not an event
  bug** — it is `MTLCommandQueue`'s default `maxCommandBufferCount`, removable with one
  argument, and it does not apply to non-blocking chains, which are flat from depth 16 to
  1024. D1b additionally shows **silent wrong answers** below its sweep threshold, not
  merely the documented hang. D1b's one surviving advantage is the tail above ~p85.
  Carried forward: **lane-parallelism — D1b's actual justification — is untouched by this
  result**, the MoltenVK and Vulkan arms were not exercised, and the "by spec" half of the
  claim is evidenced at 4 × 10⁻⁶ on one device and OS build, not proven.
- **E6 — REFUSE permanently; `is_float` escapes to the CPU.** 0 of 7,971 ladder
  type-rejections are float, and **0 slices are blocked solely by float** across 104,237
  slices from 189 files; on a deliberately 27.5% float-carrying corpus the payoff is 2
  slices, both sign-bit negation. Float is not rare (1.67% of arena nodes touch it, 2.95%
  of `Binary`) and that makes it worse — every float slice is co-blocked by
  `STORE`/`RETURN`/`BB`/`INVOKE`/loops, so **the blocker is E4, not E6**. The cheap
  native `float` rung does not exist either: Apple Silicon flushes subnormals
  unconditionally in every math mode, so E3's bit-exactness would need soft-float32, not
  native MSL `float`. Revisit only when E4 lands.

## Rows no letter owns

| | item | status |
| --- | --- | --- |
| **N1** | device-layer lifetime redesign — buffers created *and destroyed* per dispatch | **now owned by [cluster L](#l-device-lifetime--residency--the-device-is-a-jit-lifetime-resource).** Measured on Linux/NVIDIA: **130 ms one-time init, 117 µs per dispatch, 72 ns per lane**, with 24 ioctl / 3.9 openat / 2.3 mmap / 2.1 munmap **per dispatch**. The lifetime is tied to the JIT's, and the JIT's teardown has to be built first |
| **N2** | `MCC_GPU_CODE_MAX` must be raised before an interpreter can exist | resolved above. Enforcement moved to `src/mccgpu.h:2574` (MSL, **bytes**) and `:2601` (SPIR-V, words) |
| **N3** | `MCC_GPU_BACKEND` is build-time, not runtime | resolved above |
| **N4** | dispatch status checking | **CLOSED 2026-08-08.** Metal half landed at `c6814625`; the Vulkan `VK_TIMEOUT` use-after-free is fixed — a failed fence wait now strands (destroys nothing, clears `mcc_gpu.ok`, counts) instead of freeing objects a pending command buffer still references. Testable because the hardcoded 30 s timeout is now `MCC_GPU_FENCE_NS`; `slice/fault` forces 1 ns on a real device. Verified as a known-positive: with the strand reverted the cell fails and the post-timeout dispatch *succeeds* on recycled memory |
| **N5** | the ≥32 KB doorbell sweep constant | **OBSOLETE, 2026-08-08.** M5 ran and D1b did not survive it: D1e is 1.47× better on the mean with **no sweep at all**, and adding a 32 KB sweep to the resume chain costs 6.5× for nothing. None of the mitigations this row specified is needed — no `#error` floor, no per-device qualification table, no bounded poll loop, no soak, no 4 KB known-positive. Kept as a record of what the doorbell would have cost. Measuring it also found that D1b's sub-threshold failure mode is worse than documented: at 8 KB it **serves with a correct flag and a stale payload** (3 token mismatches at 8 KB, 13 at 4 KB), so below threshold the doorbell produces silent wrong answers, not only the recorded silent hang |
| **N6** | ~~`MCC_AST_EVAL_LADDER_GPU=1` overflows two stack buffers~~ | **FIXED at `b740ae46`** — the buffers are heap-allocated and slot-sized (`src/mccast.c:15794-15798`) and freed on both the `out:` and `bail:` paths (`:15859-15861`, `:15866-15868`). Retained for the record; the warm-up *dispatch* it lived in is what L1b deletes. **Was:** `src/mccast.c:15892` declared `int32_t pin[64], pout[128]` — sized for the pre-`989e4b3b` ABI — while the device layer reads `ntuple*nlive*MCC_GPU_IN_SLOTS*4` = **512 B from a 256 B buffer** and writes `ntuple*MCC_GPU_OUT_SLOTS*4` = **768 B into a 512 B buffer**. Hard SIGABRT under glibc's stack protector, so the Vulkan arm dispatches nothing on Linux; a silent 256 B stack overwrite on Darwin. **CI cannot see it** — the Linux cell installs `libvulkan-dev` (loader and headers, no ICD), so the cell green-skips. One-line fix |
| **N7** | **arena dump reproducibility regression** | **CLOSED 2026-08-08.** `sym` and `type_ref` are now interned by first-encounter order — deterministic for a deterministic compile, and dense small ids rather than addresses, so they are finally usable by a consumer at all (which unblocks N12). Verified byte-identical across two runs and between `setarch -R` and normal ASLR. H4′'s banked evidence is valid again |
| **N8** | **CLOSED 2026-08-08** — the ASMGEN arm is a `noinline` callee, so the frame is `sub $0x198` (408 B, was 4096 with the 32 KB array beyond) and self-compile peak stack is **≤64 KiB, was 1024** — 16×, better than the predicted 9.1×. Byte-neutrality checked the only way that means anything, *the same input compiled by both compilers*: 132 objects across `-O0..-O3`, identical. Deliberately not a file-scope buffer, per the row's own warning. Was: | `SValue sv_stack[VSTACK_SIZE + 1]` (`src/mccast.c:5823`) is 32,832 of the 35,424 B frame, declared unconditionally in a recursive prologue for an inline-asm arm. Hoisting it: frame **35,424 → 2,592 B**, self-compile peak stack **1024 → 112 KiB**, objects **byte-identical** at `-O0`…`-O3`. The shipping form must not be `static` (it has to survive `mcc_error`'s `longjmp`) — an arena save-area or a `noinline` helper |
| **N9** | **CLOSED 2026-08-08** — `slice/ops` is a 52-row op matrix at 32 and 64 bits over a full cross product of hard values: 162 checks, 12,249 defined tuples, 53 dispatches, and a per-opcode bitmap that asserts each of the six individually, so a row that stops lowering fails rather than quietly narrowing the matrix. `slice/ops-known-positive` proves it can fail. Was: | `TOK_UDIV`, `TOK_UMOD`, `TOK_PDIV`, `TOK_UGE`, `TOK_ULE`, `TOK_UGT` each have an MSL arm, a SPIR-V arm and a CPU-reference arm at 32 **and** 64 bits, and are exercised by nothing. **Structurally unreachable from harvested arenas** — `gen_op` rewrites `TOK_GE`→`TOK_UGE` at `src/mccgen.c:4455` *after* the arena records the token, the same mechanism behind `ee1fa9e0`. Measured 0 occurrences across 24,562 harvested nodes. Findable by enumeration alone |
| **N10** | **memory-type selection picks the worst type** | **CLOSED 2026-08-08.** `mcc_gpu_mem_index` now scores instead of taking the first match — `DEVICE_LOCAL` then `HOST_CACHED`, first-match order as tie-break so a single-candidate device is unchanged. On this host it selects `memoryTypes[4]`, `flags=0x7`, the ReBAR heap, where it previously took system RAM. The `devs[0]` half of I2(D) is still open |
| **N11** | **PARTLY CLOSED 2026-08-08** — the dead `memset(pout, …)` is gone and `memset(pin, …)` now clears only the `[ntuple, cap)` padding tail. The duplicated upload in `ast_ladder_gpu_run` is untouched. Was: | `memset(pout, …)` is 100% dead (every lane writes all out slots unconditionally; out is read only for `t < ntuple`); `memset(pin, …)` is needed only for the discarded tail; and `ast_ladder_gpu_run` uploads the **identical** input twice per rung. 28 of 56 B/lane is dead. The existing cells are **lane-bound** (23.1M lanes for `spv-slice-real`), so this is a bigger lever than N1's fixed cost |
| **N12** | **`rebuild_arena` reads 7 of 12 fields** | **CLOSED 2026-08-08, and the payoff claim was wrong.** All 12 fields are now consumed, which N7 made possible (they are interned ids, not addresses). But **no** field among `type_ref`/`bp`/`bs`/`sym`/`fbits` is read by `ast_eval_slice.h` or by either emitter — zero uses of any of them — so none can change lowerability. Measured before and after on one corpus: 783 slices, 6264 tuples, 0 mismatches, identical. The change is correct; it raises no bound |
| **N13** | **CLOSED 2026-08-08** — `tests/must-run.txt` (18 rows), `tools/must-run.py`, the `ci/must-run-registered` cell, and the `--results` half wired into `ci.yml`. Separates *not registered* (the matrix.yml bug) from *skipped*; never exits 77, because a manifest that cannot be checked is a failure. Was: | the GPU cells *lie* (exit 0 after zero work, ctest prints PASS); `rir-coverage` *tells the truth to a listener who is not there* (exit 77, reason named). Per-cell teeth is the special case; the general fix is a checked-in table of `test-name: hosts-where-SKIP-is-a-failure`, consumed by one post-ctest cell. `tools/ci.c`'s `FEATURES[]`/`GATE_CELLS[]` is the same idea one altitude too high |
| **N14** | **per-lane writable globals cap lanes at 15, not 64** | `image_ro` 1.62 MiB is shared, but `globals_rw` = `.data`+`.bss`+`.tbss` = **5.82 MiB is per lane**. Max legal lanes under the 128 MiB floor is 15 with variable frames — **for any depth cap, including zero**. The plan attributed the 64-lane collision to C4's cap; the binding term is globals. The lever is the **4.25 MiB of read-mostly `.bss` caches** (`rir_xt` 1.31 MiB, `ast_memo_try`/`ast_memo_pk` 448 KiB each, six 320 KiB tables) — 73% of `globals_rw`; sharing or shrinking them raises the ceiling to **51 lanes** |

## Corrections the investigation forced

1. **The measurement host changed, and three "unmeasurable" items are now measurable.**
   See the reading note at the top. The `docs/PLAN.md:650-651` claim that no discrete
   device exists on this host or in CI is false as of 2026-08-08.
2. **The census is 2.45× the plan's on this host.** `-O3`: **255,209 reallocs**
   (vs 104,037), 230,050 frees, **112.6 MB ever requested**, 47.6 MB peak RSS,
   **992 KiB peak stack** (`ulimit -s` bisection: fails 896, OK 1024 — replicating the
   arm64 bracket).
3. **Every B/C/D figure is an `-O1`+ figure.** At `-O0`: 30.8 MB total-ever, 63,499
   reallocs, and **20,672 B of stack — 49× less**, because `ast_replay_bb` never runs
   there (`ast_replay_env` requires `optimize >= 1`). Not previously recorded.
4. **The raw-libc sites are not "the residual".** They are **46% of realloc calls and
   74% of bytes** — `AST_REGROW` alone is 8,308 grows × 14 arrays = 116,312 calls and
   83.3 MB. And essentially all of it is `MCC_EMBED_JIT` intent capture
   (`mccjit_intent_deserialize`), not the core compiler: 5,532 arenas averaging 67.2
   nodes, 861 concurrently live, **859 never freed**.
5. **The 14 SoA arrays sum to exactly 64 B/node with zero padding** — one cache line.
   Fusing them takes raw reallocs 116,312 → 8,308 and frees ~98,000 → 4,673, makes each
   arena **one contiguous device offset**, and makes grow-in-place physically possible
   (one block can top a bump allocator; fourteen cannot). Separately, `ast_grow`'s
   initial cap of 64 against a 67.2-node average costs **22.66 of 52.99 MB**; dropping
   it to 16 saves ~17 MB for one token.
6. **B1b makes buffer growth free of relocation.** Pointers are offsets, so growth is
   `memcpy` with no fixup. This is the largest concrete payoff of B1b over B1a and it
   was not written down.
7. **`spvgate` has the `dispatches == 0` tooth at run granularity but not per case.**
   `tools/spvgate.c:1250-1254` prints `SKIP (not lowerable)` and continues, then `:1335`
   prints `OK` because `case_bad` is still 0 — **a case that lowered nothing at every
   rung reports OK**. This is the `ladder_gpu_parity.cmake` failure mode un-closed one
   level down, and it is why "the synthetic suite showed 0 mismatches" cannot currently
   be distinguished from "0 points compared". ~10 lines to fix, and it must be fixed
   before any generator's yield number is trusted.
8. **`spirv-val` conformance, discrete-GPU Vulkan behaviour and a Linux Vulkan cell are
   all now testable**, which retires three of the four items under *Measurement status*.

# Proposed phasing (if the ★ column is taken)

> **Reordered 2026-08-08 by the reframe.** The old order was a *construction* order: build
> the substrate, then the address space, then control flow, then the boundary, then
> widths, then measure. It had to be, because nothing produced a result until everything
> existed. Under cluster S the order is an **evidence** order: instrument, bid, measure,
> and widen eligibility only where a measurement says widening pays. Three phases move,
> and one of them moves six places.
>
> | phase | was | now | why |
> | --- | --- | --- | --- |
> | instrumentation (H6) | inside Phase 7 | ★ **Phase 0a, first** | no promotion decision is possible without it, and it needs nothing else to exist |
> | the first real bid (S) | did not exist | ★ **Phase 0b** | the emitter can bid today over the existing corpora |
> | device interpreter (A1b) | Phase 2 | **Phase 4+, conditional** | A3 reversed; it is now an eligibility widener that must be justified by the H6 table |
> | cluster L residency | Phase 1 | ★ **Phase 1, unchanged and now load-bearing** | without it every bid pays a cold pipeline build and every bid loses |
> | widths (E1) | Phase 4 | **unchanged in position, changed in meaning** | each rung is an eligibility increment that can be shipped and measured on its own |
> | the number (H5) | Phase 7 | ★ **continuous from Phase 0b** | there is a wall-clock number after every phase, not one at the end |
>
> Phases −1 and −1b are unchanged and still worth doing regardless of adoption.

**Phase −1 — close the four real opcode gaps and widen the oracle.** The measurement
this phase was going to buy has already been made (see the E findings), so the phase
shrinks and sharpens: add a **per-opcode histogram** at `src/mccrir.c:3264` excluding
`JMP`/`JMPCOND`/`JMPADDR`/`JMPAPPEND`/`GSYMADDR`, then handle `RETVAL`, `MKPTR`,
`VPUSHSYM`, and unblock `LOAD` from the `:4678` `continue`. Separately, widen the
existing oracle to `int64` across the 16 refusal sites (E2b, `uint2` pair). Neither
depends on anything else here, and both are worth doing whether or not the rest is
approved.

> **The original Phase −1 said "one line: `rir_arena_mismatch++` in the `default:`
> arm". That would have broken the compiler** — 68% of that arm's traffic is CFG
> opcodes correctly handled in other switches, and routing it to the mismatch counter
> would have marked ~74% of all bodies unfaithful. Kept here as a record of what the
> measurement caught.

**Phase −1b — bank the baseline census now.** The dynamic census does not need the
interpreter: `mcc -ftest-coverage` + a self-compile gives execution weights today, at
10× wall. Bank the static and dynamic node histograms, the internal/external invoke
split, and the dead-node fraction **before** any device work, so every later claim has
a fixed reference. This is one afternoon and it makes H5 interpretable.

**Phase 0a — instrument both executors. DONE 2026-08-08.** Both runners are timed, the
device figure spanning pack, dispatch, readback and unpack; `slicerun --cost` emits the
table over real arenas and `--cost-synth` sweeps synthetic slices, gated as `slice/cost`.
**The first table read "never" for every row and was measuring an artifact** — ~640 µs of
fixed cost, because every dispatch still rebuilt all ten Vulkan object classes. Cluster
L3 landed as a result (resident descriptor set, command pool, command buffer, fence, a
real `VkPipelineCache`, a 64-entry pipeline cache keyed on `(code, nlive)`, and growable
persistent buffers), taking fixed cost to **~20 µs** and making the number mean something.
Original statement of the phase follows. *New, and first, 2026-08-08.* Put a `clock_gettime` bracket around
`ast_eval_slice` on the CPU side and a timed region around `mcc_gpu_dispatch` on the
device side that **includes upload, dispatch, download and readback** — the whole cost a
caller pays, not the kernel time. Both are currently untimed: `src/mccgpu.c` contains no
timing call of any kind, and `AstLadderStat.secs` conflates both sides into one scalar.
Then emit the **H6 cost table** over the existing harvested corpora (`MCC_ARENA_DUMP` +
`spvgate --arenas` already supply the slices): `slice-ident, nlive, nodes, cpu ns/tuple,
gpu fixed ns, gpu ns/lane, break-even ntuple`. Bank it. This needs no interpreter, no
address space, no control-flow machine and no scheduler work, and **its break-even column
is the go/no-go for everything after Phase 1.** Cheap enough to do before adoption is
taken; do it first anyway if adoption is refused.

**Phase 0b — the first bid, end to end, on the narrowest possible path.** Make one slice
run on the device because a benchmark said so. Concretely: give `mcc_gpu_emit` a
non-static entry point so `mccjit_embed.c` can reach it in the non-amalgamated build
(finding above); give the census slice an identity so `ast_slc_walk`'s run survives
`ast_slc_flush` (S1b); add `mcc_gpu_prepare` so a kernel and its Vulkan objects outlive
one dispatch (S6a) with the four-part key of S6b; graduate `mccjit_bench_enabled` out of
`MCC_DEV_ENV` and **flip the fail direction to fail-closed for device candidates** (S4);
extend `mccjit_bench_run_pair` to time batch-vs-batch (S3b); add the fourth branch to
`mccjit_lazy_entry` returning the S8a marshalling trampoline. The correctness gate is
free — the device variant goes in as `MccjitKgc.mx_variant` and inherits `vval == bval`
(S8b). **Acceptance: one ctest where a slice is measured, promoted, executed on the
device, and produces a byte-identical program**, plus one where it is measured, *loses*,
and the CPU keeps it — the second cell matters as much as the first and is the one the
old plan had no way to write.

**Phase 0c — the reference interpreter.** *Was Phase 0; unchanged in content, now third
because 0a and 0b do not depend on it.* **Restated 2026-08-08**, because the record
question resolved differently than drafted. Do *not* widen the text dump further —
revert it to its 7-field reproducible form (N7) and add a **binary sidecar emitting
`mccjit_intent` format 14** (A2 layer 1), which is already versioned, pointer-free and
round-trip-tested. Finish the half-landed consumer: teach `rebuild_arena`
(`tools/spvgate.c:1083`) to read it (N12). Make the stamped type view unconditional on
the dump path so `Binary` stops reading back untyped, gated by a `cmp` test — the
objects are already known byte-identical across `MCC_RIR_STAMP` settings. Then write the
CPU reference interpreter over the 32 B/node projection, reusing `ast_eval_binop`
wholesale. Breadth-first across **all 14 kinds at int32** (G3c), day one being the
`ast/noderun` test above. The codec's total correctness check is
`ast_hash_of(rebuilt) == ast_hash_of(original)` over every body of a self-compile —
not a proxy, since `ast_hash_of` *is* the identity relation the compiler uses for CSE.
Prove it runs `tests/exec` identically to native codegen (that is G2c). *No GPU yet.*

**Phase 1 — the device lifetime, then the device address space.** Cluster L comes
first and its two halves are not symmetric. **L2′ before L2:** give the JIT a real
shutdown — a quit flag, a `pthread_cond_broadcast`, and a join or in-flight barrier
over the detached pool workers (`src/mccjit_embed.c:1341`, `:1375`). **S7b is the
recommended shape for that shutdown and lands here, not later**: a quit flag checked
between coroutine ticks is a few lines, while a quit flag against an opaque
`job->run(job)` holding `mccjit_swap_lock` across a whole compile is a redesign wearing
a smaller name. The same tick representation is what D1e's resume chain and C3b's step
budget both want, so building it once here pays for itself three times. Then clear
`mcc_gpu.ok` on `VK_ERROR_DEVICE_LOST` so `vkDeviceWaitIdle` cannot deadlock from
`atexit`, then make Vulkan quiesce actually destroy the device and instance. Only then
L1b (bring-up in `__mccjit_boot_all`, once, not once per registry entry) and L3
(residency, including the `VkPipelineCache` that does not exist today). L7's three
hazards land with L4b, not after it.

Then the address space: B1b/B2b/B4b. **Correction to the draft:** `mcc_relocate_ex`'s
`mem = 0` pass does **not** yield image offsets — `src/mccrun.c:750` reads
`s->sh_addr = mem ? addr + offset : 0`, so a null pass sets every `sh_addr` to zero and
only the total padded size survives. Either patch that line to assign unconditionally
(`addr` is already 0 on that pass, `:748`) or write a separate layout pass; the
monotonic single-cursor property the plan wanted is real (`:708-751`), the offsets are
not. Replace absolute relocation with an in-blob import table — `relocate_syms` bakes
live host VAs (`src/objfmt/mccelf.c:1007`) and `cleanup_sections` (`src/mccrun.c:198`)
then frees the reloc sections, so nothing can be re-based afterwards. `Load`/`Store`/
`StoreVal` against the buffer. Still CPU-executed.

**Phase 2 — widen eligibility through the emitter, not the interpreter.** *Rewritten
2026-08-08; the old Phase 2 was "the interpreter on the device" and is now Phase 4+.*
With Phase 0b's bid path live and Phase 1's residency in place, the cheapest eligibility
increments are all emitter-side and each one ships and is measured on its own: the
control-flow machine (cluster C — loops are the whole gap, zero `OpLoopMerge` and zero
`OpSwitch` exist today), then `Store`/`StoreVal` against Phase 1's buffer, then the
remaining kinds. **Every increment is gated by the same question**: does the H6 table
show slices that this increment makes eligible *and* whose break-even lands inside a
batch size `ast_loopdep` can supply? An increment that widens eligibility into slices
that always lose the bid is measurable, publishable, and not worth shipping. Conformance
matrix per G1/G3c across reference + Metal + SPIR-V as each kind lands.

**Phase 3 — the call boundary.** **D4b is not a later refinement; it is the phase.**
The static ceiling without it is 95.0% and with it 99.4%, and the break-even latency
without it is 0.39 ns — unachievable on any hardware. Order: D1a for bring-up, then
**D4b (internal calls on device)** with F1c whole-function offload from that moment
(1860 of 2452 bodies have zero external invokes), then D2b (device `str*`/`mem*` —
77.8% of crossings), then B3b (device allocator installed at `mcc_set_realloc` — a
further 21.2%), then B6c (file staging *including the path-probe table*). Only after
all four does the break-even reach 9.7 µs and the design become viable at all.

**Phase 4 — widths, and the interpreter if it is earned.** E1's ladder: int64 as a
`uint2` pair (E2b), then FP, then aggregates/bitfields — each rung an eligibility
increment, shipped and measured on its own. **And this is where A1b is decided, not
assumed.** *Was Phase 2.* The device interpreter is a 15–25k-word kernel, hand-written
twice (A4a), needing `MCC_GPU_CODE_MAX` raised first (N2) and priced by MoltenVK at
seconds of driver compile time without a persisted `VkPipelineCache`. Build it when — and
only when — the H6 table shows a class of slices the emitter cannot reach whose
break-even lands inside an achievable batch. **Under the old plan this was the substrate
and its cost was unavoidable; under the reframe it is a widener that must clear the same
bar as every other widener.**

**Phase 5 — the boundary mechanism, no longer "the doorbell".** Ship **D1e** (speculative
pre-enqueued self-skipping resume chain) if the M5 experiment confirms ~30 µs: it reuses
C3b's state vector, gets its cache visibility from the command-buffer boundary by spec,
and carries no hardware constant and no hang mode. **Cluster L is what makes D1e
buildable at all** — a pre-enqueued chain needs a resident command pool, a resident
command-buffer ring and a single owning submitter, which is exactly L3 plus L4b's
device thread. Under the old per-dispatch lifetime there is nothing to pre-enqueue
*into*. **D1b is demoted to Phase 7+** — its
24 µs is 100% sweep, any safety margin erases the win, 3000 clean rounds bound the hang
rate only at ~2.9 per self-compile, and its genuine advantage (other lanes keep running
through one lane's host call) is **no longer worth nothing** — J4a's single-lane rule was
amended by S5c, and under lane-parallelism that advantage is real. It stays demoted on
the other three grounds, but the reason list is now three items, not four. Either way
this phase stays *after* Phase 3: crossing reduction is a ~100× lever and the boundary
mechanism is a 6× lever. **Implementation note from S7b:** D1e's self-skipping resume
chain and the coroutine tick are the same object, so if Phase 1 built the tick, this
phase is wiring rather than design.

**Phase 6 — the emitter path, specialized.** A1c: specialize hot regions. *Amended* —
the old acceptance criterion was "the emitters can recompile the interpreter's own C
source and match the hand-written pair bit-for-bit", which presupposes an interpreter
Phase 4 may correctly decline to build. It survives **conditionally**: if A1b was built,
that criterion stands as the strongest available proof of emitter totality. If A1b was
not built, the criterion becomes the H6 table itself — every slice class the emitter
claims, it must also win or honestly lose a bid on.

**Phase 7 — the number.** H5b. Self-host recompile, wall clock against the 0.093 s
CPU-only baseline as the headline, with the static and dynamic censuses, the H3 class
breakdown and the F0b `bid-lost` count reported beside it as explanation. Ratchet banked.
**Under the reframe there is a wall-clock number after every phase from 0b onward**, so
this phase reports the final one rather than the first one.

# Measurement status

**All eight research tracks are complete, and a second round of eight decision
investigations completed 2026-08-08** (see [Decisions resolved](#decisions-resolved--2026-08-08)).
Three of the first round died on a session limit and were
re-run; every row in the tables above now rests on a measurement or a cited line of
code, not on reasoning. The corrections the measurements forced are recorded inline
rather than quietly folded in, because several of them reversed a ★.

**Cross-target verification actually performed** (2026-08-07, after enabling
`MCC_ENABLE_CROSS=ON`): the `cross/*` suite (26 cells), `pe-wine-conformance` under
Wine 11.0, and the docker matrix across linux/386, linux/amd64, linux/arm64 and
linux/riscv64 — `i386-fastcall-abi`, `i386-tls`, `i386-codegen-diff`,
`i386-divmagic-soak`, `i386-inline`, `dwarf-gdb`, `eh-unwind-{arm64,riscv64,amd64}`,
`extlink-{x86_64,arm64}`, `abi-diff-amd64`, `arm64-inline`. **QEMU user-mode is not
available on this host** (only `qemu-system-x86_64`), so the `selfhost-qemu-*` cells
cannot run here; Docker's binfmt emulation covers the same targets instead.

**What remains genuinely unmeasured** — revised 2026-08-08, since the dev host changed:

- **Windows TDR** (~2 s default) — still untested, still the one real portability risk
  in C3. macOS was measured and does *not* kill long compute kernels on duration.
- **Register pressure in the interpreter kernel** — no `nvdisasm` here, AIR is opaque.
  Weak negative evidence added 2026-08-08: `maxTotalThreadsPerThreadgroup` stayed at 1024
  for every generated module up to 99,373 words on the M1 Pro, which is consistent with
  no register-pressure cliff and is nothing more than that.
- **Driver-side compile time on NVIDIA/AMD/Intel.** The MoltenVK and native-Metal halves
  are now measured (below); no such device exists on the Darwin host, so that half stands.
- **A real interpreter's per-step cost.** The step rate below was measured with arms of 3
  ALU ops or one load-plus-store; a real AST arm also does operand decode, stack traffic,
  type dispatch, gas accounting and bounds checks. **Every measured ns/step is a lower
  bound**, and the `1<<16` budget recommendation errs generous accordingly.
- **Realistic multi-lane divergence.** Only the two extremes were run — all lanes
  converged and all lanes at distinct pcs, which differ by ~10×. Real behaviour is between.

**Measured on 2026-08-08 and removed from this list** (Apple M1 Pro, under contention;
full numbers in [`docs/TODO.md`](TODO.md)):

- **M5 / D1e latency — RUN. 12.4 µs median / 19.1 µs mean, no sweep**, beating both the
  ~30 µs projection and D1b. The ~64-CB deadlock is `maxCommandBufferCount`, not an event
  bug, and does not apply to non-blocking chains. See the closed row above.
- **The real device step rate — RUN, and C3's bracket does not survive it.** ~1 µs/step
  single-lane at an interpreter-width switch (120 ns absolute floor with no device memory
  at all), so **the 20–100 ns/step bracket is optimistic by 7–50× and its lower bound is
  unreachable on this hardware in any constructible configuration**. Per-lane latency is
  flat across a 16,384× occupancy change, so all the device's power is width: 64
  *converged* lanes cost one lane's wall clock. **C3's `1<<20` budget is 16× too large;
  `1<<16` is the only power of two meeting both bounds at every switch width.** Switch
  width is the dominant controllable cost at ~2.5 ns per arm per step.
- **MoltenVK back-end compile time — RUN, and the SPIRV-Cross hypothesis is refuted.**
  MoltenVK is 1.93× native Metal at 1649 words falling to **1.05× at 99k**: translation is
  5–20% and shrinking, and the seconds are in the Metal back end, which native Metal pays
  too. Cost is linear (~14 µs/word) below ~25k words and quadratic above ~50k, so A4's
  density estimate is load-bearing. A persisted `VkPipelineCache` collapses every size to
  1.6–3.1 ms. **This prices A1a out on compile time alone** — ~15,100 regions inside a
  0.093 s compile demands ~162,000 regions/s against a driver supply of ~52/s.

**Retired from this list on 2026-08-08**, because the current host can measure them:
discrete-GPU Vulkan behaviour and the uncached-host-read concern (B4c); `spirv-val`
conformance of the multi-level-break module MoltenVK accepted; a Linux Vulkan device
cell; and whether the ≥32 KB doorbell sweep transfers off Apple silicon. **None of the
four has actually been run yet** — they moved from *unmeasurable* to *unmeasured*.

# The ceiling — measured, and it is high

**Method.** An instrumented `ast_adump_body` emitted one `[inv]` record per
`AST_Invoke` naming the callee `Sym`, classified the way `ast_bfold_run`
(`src/mccast.c:6920-6926`) already does. Reproduced the known census exactly: 2452
bodies / 374,310 nodes.

| class | sites | % of invokes |
| --- | ---: | ---: |
| **INTERNAL** (defined in the TU) | **16,260** | **87.65%** |
| **EXTERNAL** (undefined function symbol) | **2,248** | **12.12%** |
| indirect via function-pointer object | 20 | 0.11% |
| indirect, callee has no sym | 22 | 0.12% |
| total `AST_Invoke` | 18,550 | **4.956% of all nodes** |

135 distinct external callees; the top are `strcmp` 294, `snprintf` 249, `memcpy` 246,
`fprintf` 246, `memset` 153, `strlen` 75, `free` 64, `getenv` 62.

| design | static ceiling | dynamic (avg-block) | dynamic (entry) | measured-crossings anchor |
| --- | ---: | ---: | ---: | ---: |
| **D4a** — all invokes on CPU | 95.044% | 95.974% | 94.974% | — |
| **D4b** — only *external* invokes on CPU | **99.399%** | **99.795%** | **99.825%** | **99.984%** |

The anchor column is model-independent: the B-track's measured 944,327 external calls
divided by the tcov-derived ~5.97e9 node executions. Both weighting models *over*-count
external invokes because they sit in cold error paths, so **99.8% is the conservative
figure**. The static number is also a lower bound — 294 clang-defined non-`.cold`
functions have no arena body and are miscounted as external, conservatively.

**The ceiling is robustly above 99% under D4b by every method, and D4b is what earns
it** — the gap between D4a and D4b is 4.4 static points.

**But the reframe changes what this number is for.** It was "the plan's strongest
justification". It is now the **eligibility ceiling**: the fraction of work the device is
*permitted to bid on*, and an upper bound on nothing else. A 99.4% ceiling with a
break-even of 1,177 lanes and hot loops of 30 iterations is a 0% result. The number that
justifies the plan is H6's break-even column crossed with S5′'s iteration-count
distribution, and **neither has been measured.** This section is kept in full because the
ceiling is still a genuine and hard-won bound — it just answers a different question than
it was written to answer.

**But the same census demolishes any static-only headline.** **58.4% of bodies (50.3%
of static nodes) never execute at all during a self-compile**, and the top 250 bodies
— 27% of static nodes — carry **97% of dynamic weight**. A "99.4% of nodes on the GPU"
claim would be half-vacuous on its own. H1c's pair is not a nicety, and any H5 report
must state the dead-node fraction or the number cannot be interpreted.

# Known hard parts, named up front

- **The bench that the whole reframe rests on is `#ifdef`-ed out of shipped builds, and
  it fails open.** `mccjit_bench_enabled()` reads `MCC_DEV_ENV`
  (`src/mccjit_embed.c:1018`, `src/mccdev.h:13`), so `mccjit_bench_admit` returns 1 —
  *admit, unmeasured* — in every build a user runs, as do the no-samples and not-routed
  bailouts. For a CPU variant that KGC will check anyway this is defensible. For a
  batched device candidate at 117 µs it is a reliable slowdown shipped by default. S4.
- **The device cannot win a scalar bid, ever.** Break-even against a CPU slice at `C`
  ns/tuple is `ntuple > 117000/(C − 0.072)`; below `C` ≈ 72 ns **no batch size wins at
  all**, and at 100 ns it is 1,177 lanes. The unit that can win is a slice inside a loop
  over independent iterations — which makes **lane parallelism the whole thesis** and
  collides head-on with J4a's single-lane determinism rule. J4 is amended; the collision
  was real, not a wording problem.
- **Nothing in this tree times anything on either side of the comparison the plan now
  depends on.** Zero timing calls in `src/mccgpu.c`; nothing around `ast_eval_slice_rec`;
  the ladder's only budget is a dispatch *count*; `AstLadderStat.secs` mixes both sides
  into one scalar and `ladder_gpu_parity.cmake` strips it before comparing.
- **The pool the plan extends does not run in the default build.** `jit_threads`
  defaults to 0 (`src/libmcc.c:1215`), `async` is decided from it at compile time
  (`src/mccjit_embed.c:1965`), and the generated constructor therefore takes the
  synchronous path. Every `async`-gated strategy, including the existing
  `mccjit_slice_search`, is dead by default.
- **One global lock serializes the pool it is supposed to parallelize.** The worker holds
  `mccjit_swap_lock` across the entire `job->run(job)` (`:1353-1355`) because the compile
  path runs on process-global state. Adding device work without narrowing that lock puts
  every JIT compile in the process behind one pipeline build.

- **The four opcode gaps that actually fire**, not 25: `RETVAL`, `MKPTR`, `VPUSHSYM`,
  and `LOAD` (blocked earlier, at `src/mccrir.c:4678`). The other 21 fire zero times on
  this target and five are `#ifdef`-ed out on arm64. The earlier framing of this as
  "the real ceiling" was wrong: 95.6% of bodies containing a dropped opcode still
  replay byte-identically.
- **The device layer has no persistent state, and no owner for one.** Vulkan destroys
  ten object classes every dispatch (`src/mccgpu.c:1515-1535`) and creates its compute
  pipeline with `VK_NULL_HANDLE` for the cache (`:1471`), so the shader is recompiled
  every time; Metal releases both buffers per dispatch (`:392-393`) and caches only
  PSOs. A persistent address space is therefore a lifetime redesign, not a parameter
  change — that is cluster L. **The `+56 µs, 39% of latency` figure this bullet used to
  carry is withdrawn**: held-vs-fresh buffers were A/B'd in one binary and were within
  noise at every size from 64 to 262,144 tuples. Residency is justified by correctness
  (B1's address space must survive a dispatch) and by the missing pipeline cache, not
  by buffer allocation.
- **The JIT has no teardown to hang the device's off.** Its only exit hook is
  `atexit(mccjit_kgc_flush_all)` (`src/mccjit_embed.c:2810`); pool workers are detached
  into an infinite loop and are never signalled or joined (`:1341`, `:1375`), and
  `mcc_gpu_quiesce`'s single caller is registered only under
  `MCC_AST_EVAL_LADDER_GPU` (`src/mccast.c:15881`). "Torn down once, with the JIT"
  requires **building the JIT's shutdown first** (L2′), and doing so makes an existing
  latent deadlock reachable: `mcc_gpu.ok` is never cleared after
  `VK_ERROR_DEVICE_LOST`, so an unbounded `vkDeviceWaitIdle` would run from `atexit`
  on every exit rather than never.
- **`MCC_GPU_CODE_MAX` must be raised before an interpreter can exist** — a realistic
  interpreter kernel is 15–25k words against a cap of 8192, and the cap is enforced on
  the interpreter by the very same check it was supposed to sidestep.
- **The C stack cannot live in device-private memory.** 32,768 B of threadgroup memory
  ÷ 64 lanes = 512 B/lane against a measured **930 KiB** (M1) / **992 KiB** (Linux)
  requirement — **~1900× short**. It must be an array in the B1 buffer. Forced, not
  chosen. **Amended 2026-08-08 on two counts:** the requirement is 9.1× smaller after
  N8's one-line frame hoist (992 → 112 KiB), and **the 64-lane collision is not the
  stack** — see N14. Per-lane writable globals are 5.82 MiB against a per-lane stack of
  256 KiB, so lanes cap at 15 under the Vulkan floor regardless of the depth cap.
- **Host→device coherency does not exist mid-kernel.** GPU→host writes are visible
  immediately; **host→GPU writes are invisible by every available qualifier.** Any
  design that assumes a two-way mailbox is unfounded. **Amended 2026-08-08:** the
  doorbell's ≥32 KB sweep is a hardware-specific emulation of the cache invalidation a
  **command-buffer boundary provides by spec** — so a resumable dispatch (C3b) obtains
  the same visibility with no constant and no hang mode, which is what D1e proposes and
  what demotes D1b.
- **A device fault leaves no trustworthy byte.** Under B1b + B4b the buffer *is* the
  address space and is host-coherent, so every device store is already committed; a
  faulted kernel can leave torn `uint2` int64 pairs and half-written aggregates that no
  correct execution ever produces, and a GPU reset need not flush L1/L2 even for stores
  the kernel retired. **There is no in-buffer mechanism that survives this** — an undo
  log has neither durability nor ordering to rely on. Hence J3a′: abandon the whole run.
- **Dispatch latency dominates everything** until crossings are reduced. Measured
  **150 µs** for D1a, not the 20 µs first assumed — 1523× the baseline at full census.
  D1b's 24 µs is a 6× lever; crossing reduction is a 100× lever. **Reduction first.**
- **`long double`.** x87 80-bit has no device equivalent. It escapes, permanently,
  and must be visible in the census rather than quietly rounded to `double`.
- **Recursion depth.** mcc's recursive-descent parser will hit C4b's cap. The cap has
  to be generous and the escape path has to be correct, not merely present.
- **Watchdogs — the draft was wrong about macOS.** There is **no wall-clock watchdog**
  for Metal compute: a single kernel ran **61.3 s** to `Completed` with no error, and
  another **122.6 s** at 1 threadgroup. What exists is an **interactivity/occupancy**
  watchdog — ≥1024 resident threadgroups is killed after 0.55–5.5 s with
  `kIOGPUCommandBufferCallbackErrorImpactingInteractivity`, hidden inside
  `localizedDescription` behind a generic `MTLCommandBufferErrorInternal`. C3b remains
  mandatory, but for **occupancy, Windows TDR (~2 s, untested), and device errors that
  are currently invisible** — not for duration on macOS.
- **Determinism vs. parallelism.** Single-lane execution is deterministic and slow.
  Any lane-parallel scheme (data-parallel loop nests via the existing
  `ast_loopnest_build`/`ast_loopdep` analyses) reopens J1 and should not be attempted
  before Phase 7 has a baseline.
