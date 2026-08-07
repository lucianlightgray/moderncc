# TODO

> History, landed-fix write-ups, superseded boards, and the "landmine" warnings that
> used to live here were moved to [`docs/ARCHIVED.md`](ARCHIVED.md) on 2026-08-05 for
> later validation. This file now holds only the configuration instructions below and
> present-tense, open items. File:line anchors are omitted on purpose — the archived
> ones had drifted 1000–1900 lines after merges; find code by symbol.

## Where RIR replay accuracy stands — 2026-08-06

RIR is **two layers**, and conflating them is what made the older boards unreadable. The
op-stream capture is the ground truth; the arena is a second derivation built from it, and
only the arena has ever had a gap.

| layer | coverage, by emitted body bytes |
| --- | --- |
| capture (`rir_verify`, `MCC_REPLAY_IR=3`) | **100.000%**, `fallback=0`, on both corpora |
| arena (production, `MCC_RIR_PROD`) | **100.000%** self at `-O0`/`-O1`, **99.965%** at `-O2`/`-O3`, residual 0 |

Measure it with `ctest -R rir-coverage`; the numbers are banked in
`tests/rir/coverage-bank.json` and ratcheted. **The ratchet gates percentages and the
residual, never body totals** — the census body total is not stable run-to-run at fixed
HEAD (two consecutive full-suite runs gave `used=95823` and `used=95971` while every class
count was byte-identical), so compare class counts, never totals.

Refusal classes closed on 2026-08-06: `asm`, `regdangle`, `bail`, `mismatch` — all report
`used` now. `noops` is 338 provably empty bodies (`len=0 rirn=0 capn=0`), which is not a
gap; `revargs` is one body, assessed and deliberately left.

**The `abort` class is closed for the lost-cast family — 2026-08-06.** A cast that changed
the static type but emitted no machine code left no trace in the arena, so a chain like
`((Sym *)(uintptr_t)x)->v` handed the replay's `->` an integer and it died with
`pointer expected`. `rir_hook_cast_type()` now records the post-cast `CType` (`t`, `ref`,
`bp`/`bs`) as a `RIR_M_CASTT` mark at every explicit cast, and `rir_to_arena` turns it into
an `AST_Convert` carrying the *final* static type of the chain, not the type of the last
code-emitting cast. Self corpus: gap 5411 B / 5 bodies → **0 B at `-O0`/`-O1`**, 5834 B / 6
bodies → **467 B / 1 body at `-O2`/`-O3`**, and `kept` rose 89.8% → 96.0% because the
restored pointer type also makes the replay pick the same code paths.

**What is left is one body, 467 B, and it is not a type.** `host_runmem_alloc` in
`src/mcchost.c` aborts with `ast-replay: storeval-arg stack underflow`: `ast_storeval_*`
marks its `AST_Invoke` `AST_FB_CALL_STOREVAL_ARG`, and at replay the vstack holds fewer
live entries than the `vrotb(3)`/`vswap()` rotation needs. That AST pass only runs from
`-O2`, which is why the class vanishes at `-O0`/`-O1` and why `tests/rir/gap/abort.c`
carries a `rir-gap-levels: O2,O3` marker. Reduced reproducer: `p = f(n += g());`.

Fallback rows in the census now carry `first`/`end`/`blen`/`nlen`, which localises a
divergence to a byte span within the body — the standing "fix at the USE site" item needs
exactly that. Example row: a 479-byte `host_runmem_alloc` diverging only at bytes 428–457.

**Assembly is modelled, not opaque.** `asm goto` labels, `volatile`, and the
`"memory"`/`"cc"`/`"flags"` clobbers were all being discarded and are now captured; the
effect set (`reads`/`writes`/`clobbers`/`mem`/`pins`) is decoded onto the node and eight
strategies are re-admitted to asm-bearing bodies. The right mental model is a **pin**, not
a barrier: `subst_asm_operands` bakes concrete registers in at parse time, so `narrow`,
`ltemp`, `sethi`, `reg-color` and promotion break an asm without moving anything across it.

## The slice equivalence oracle is an exhaustive width ladder — 2026-08-06

`ast_eval_slice_equiv` in `src/ast_eval_slice.h` used to sample eight arbitrary points
(`{0, 1, -1, 2, 7, -3, 100, 12345}`, rotated across the live-ins) and return
`evaluated > 0` — it certified equivalence off **one** successful evaluation while the
other seven may have bailed. Behind `MCC_AST_EVAL_LADDER=1` (default OFF) it is now an
exhaustive ladder. The old sampler is still there as `ast_eval_slice_equiv_seed` and is
what runs with the toggle off; `src/mcc.c` objects are byte-identical to the unpatched
compiler at `-O0`/`-O1`/`-O2`/`-O3` with the toggle off, proven with `cmp`.

**The ladder.** Live-ins are collected with their declared type. Rungs, climbed in order:

| rung | domain | certifies? |
| --- | --- | --- |
| `const` | all live-ins zero, 1 evaluation | only when there are no live-ins |
| `w1`..`w32` | every tuple of `e_i = min(w, typewidth_i)`-bit patterns | yes, if the rung completes |
| `corners` | `{0, 1, -1, TYPE_MIN, TYPE_MAX, TYPE_MAX-1}` per live-in | no — falsifier only |
| `observed` | real argument tuples recorded by the JIT | yes, but never `exact` |

A `w`-bit pattern is **sign-extended then fitted to the live-in's type**, so an unsigned
live-in gets both the zero neighbourhood and the `2^n - 1` neighbourhood at every rung —
the carry boundaries `seed[8]` never reached. The rungs nest, so the first rung that
disagrees is the *smallest* width at which the two forms differ, and that is what the
oracle reports. When `e_i == typewidth_i` for every live-in the rung is exhaustive over
the true input space and the verdict is marked `exact`; the ladder then stops early.

**A rung completes or nothing is certified.** A point where the *source* form is
undefined (signed overflow, division by zero, out-of-range shift) is vacuous — undefined
behaviour constrains nothing — but a point where the source is defined and the
replacement is not counts as a difference. A rung with zero non-vacuous points refuses
(`all-undefined`). A rung that does not fit the budget is not partially run.

**Budget.** `MCC_AST_EVAL_LADDER_BUDGET` (default `1<<20` points) caps a rung. The
2^32 rung is a GPU rung: on this CPU a single `n=1`, `w=32` sweep of `int x*2+1` against
`x+x+1` is 4,295,033,110 evaluations in **6m03s** (~85 ns/point, two tree walks each).
At the default budget an `n=1` slice therefore stops at `w16` and an `n=2` slice at `w8`.
Nothing above the budget is guessed: the fallback is `ast_slice_ladder_observed_source()`,
wired in `mccjit_kernel_search_from_blob` to `MccjitIntent.param_off[]` +
`MccjitCounterState.sample[]`/`argmin`/`argmax` — the JIT's own recorded argument tuples.
With no source registered the verdict is `refused over-budget`, never a certification.

**Measured over a full `src/mcc.c` self-compile** (`MCC_AST_EVAL_LADDER_CENSUS=1`, `-O2`,
default budget; 19,206 slice-vs-copy pairs and 81,361 cross pairs between distinct slices
of the same body):

- certifiable, self pairs: seed **19,193 / 19,206 (99.93%)**, ladder **18,940 (98.62%)**.
  Decided at `const` 2,442, `w2` 1, `w4` 259, `w8` 2,723, `w16` 13,515, `w32` 0 — the
  13,515 at `w16` stop there only because 2^32 exceeds the CPU budget. 2,616 are `exact`.
- **the old oracle was certifying non-equivalent forms.** Of the 16,122 cross pairs
  `seed[8]` called equivalent, the ladder refutes **1,449 (9.0%)** with a counterexample:
  smallest differing width `w1` 203, `w2` 6, `w4` 273, `w8` 674, `w16` 230, corners 63.
  The 3 is the tell — `seed[8]` contains no 3, so `x == 3` versus `0` certifies clean and
  the ladder refutes it at `w4`. That case is `jit/selftest-sliceladder`.
- refused for want of a type: **252 self pairs (1.31%)** and 1,794 cross pairs (2.20%)
  have a leaf — live-in, literal or convert — with no static type. Under
  `MCC_AST_EVAL_LADDER_STRICT_TYPE=1`, which also rejects an interior node whose
  arithmetic width had to be inferred from a child, that becomes **896 (4.67%)** and
  6,263 (7.70%). Those are the numbers the arena `CType` stamping should drive to zero.
  Today the width is inferred for 2,088 interior nodes across 896 self pairs.
- cost: at the default budget the oracle is **1.5 ms per self pair / 199 us per cross
  pair**, 45.3 s of oracle time on a self-compile whose baseline is 0.37 s. At
  `MCC_AST_EVAL_LADDER_BUDGET=4096` it is **13.2 us / 1.7 us** and adds 0.43 s to the
  whole self-compile for 100,564 pairs.

**No GPU path is wired.** `tests/gpu/` has a fixed-shader Vulkan harness, not a general
AST interpreter; emitting a compute shader per slice at compile time is the open work.
Everything above runs on the CPU and the default suite is green with no GPU present.

## Slices between anonymous invokes — the A1 census, 2026-08-06

The GPU-offload question is "what is the largest chunk of a body that runs without ever
calling out". `MCC_SLICE_CENSUS=<path|->` makes the compiler answer it, one line per
slice and one per body, and `tools/slice-census.py` aggregates it.
`ctest -R slice-enum` verifies the enumerator (0.1 s); `ctest -L census -R slice-census`
runs the full sweep.

**The boundary.** A slice is a *maximal run of consecutive statements inside one statement
list whose subtrees contain no opaque `AST_Invoke`*. Maximal in both directions: the
neighbouring statement has a call, and the parent statement has a call. A call-free loop
is not split out — it is an ordinary statement, so a call-free loop nest comes through as
one slice. **Statement-granular, not expression-granular**: the call-free half of
`a[i] + h(s,i)` is not a slice. That case is exactly what the pre-existing machinery in
`src/mccast.c` already does — `ast_slice_win_root_ok` roots an expression *window* at a
Binary/Unary/Convert/Load, `ast_slice_ident_hash` gives it an alpha-equivalent identity,
and `-fopt-slice` (rung 10) memoises those windows to a disk cache for JIT specialisation.
Windows are 3..65536 nodes of pure integer arithmetic and are a different object from a
slice; nothing here duplicates them.

**Anonymity is measured, not assumed.** An `AST_Invoke` carries the callee as child 0.
Four classes, counted per body: `indirect` (child 0 is not a `Ref` to a `VT_FUNC` `Sym`),
`opaque-direct` (a `Sym` with no body retained in this TU), `retained` (in the graft pool,
not graftable) and `graftable`. The first two are the truly anonymous ones. Two partitions
are reported side by side: `t=0` treats every invoke as a boundary, `t=1` lets graftable
invokes be transparent, because from `-O2` the reemit path really does graft them (checked:
`-O2` and `-O3` both emit zero `call` for a static callee in the pool). `t=1 - t=0` is the
inlining effect and nothing else.

**Byte extents are measured, not modelled.** `ast_replay_bb` brackets each statement's
replay with the code index `ind`, so a slice's byte extent is the sum of what its
statements actually emitted. It reconciles to 99.995% of body bytes on `src/mcc.c` at
`-O1..-O3` and 100.053% at `-O0`; the drift is peepholes that rewind `ind` and so
double-count a retracted byte. The gate allows `max(32, bytes/50)` of slack per body.
The measured replay is the faithfulness replay, where `ast_inline_active` is 0, so both
partitions are attributed against the *un-grafted* emission: `t=1` says how many of the
caller's own bytes stop being cut by a boundary, not what the post-inline body would
measure.

**Headline, `src/mcc.c` amalgamated, x86_64.** 2589 modelled bodies, 1,323,733 body bytes.

| | `-O0` | `-O1` | `-O2` | `-O3` |
| --- | --- | --- | --- | --- |
| anonymous invokes | 100.0% | 100.0% | 84.6% | 84.6% |
| slices (`t=0`) | 11847 | 11847 | 11847 | 11847 |
| body bytes in a slice (`t=0`) | 41.42% | 41.44% | 41.44% | 41.44% |
| body bytes in a slice (`t=1`) | 41.42% | 41.44% | **44.68%** | **44.68%** |
| slices (`t=1`) | 11847 | 11847 | 11628 | 11628 |

**`-O` barely moves this.** The production arena handed to `ast_func_end` is the same at
`-O1`, `-O2` and `-O3` — the strategy passes run after it — so the `t=0` partition is
identical at all three. The only thing `-O` changes is graftability, and grafting every
graftable callee buys **3.2 points of body bytes** (548,599 → 591,386 B) while *reducing*
the slice count by 219. That is the whole inlining effect, and it is small because 84.6%
of invokes remain anonymous after it: mcc calls out to libc and to non-static helpers far
more than it calls static leaves.

**The shape is the result, and it is discouraging.** Of 11847 slices at `-O2`: 46% are
≤4 nodes and carry 9.6% of slice bytes; 65% are ≤8 nodes. The tail is thin — 292 slices
(2.5%) are ≥65 nodes, and they carry 26.0% of slice bytes. Only **512 slices (4.3%)
contain a loop at all**, and those hold **9.58% of body bytes**. Slices at loop depth ≥1
are 3728 of 11847. The largest are real but few: `cplx_const_int_div` (956 nodes, 8 loops),
`ast_opt_defaults` (803), `gen_opq_fold` (560), `ast_eval_binop` (495). 19.2% of bodies are
wholly call-free but they are small — 5.47% of body bytes.

`--corpus exec` (301 files, 1249 bodies) is *worse* and should not be read as a second
opinion: 96.7% of its slices are 3–4 nodes, because the corpus is assert-and-`printf`
straight-line code. Its one big slice, `classify` at 5128 nodes / 27,339 B, is a generated
table.

**Read this as a negative result with a narrow positive.** Straight-line call-free code is
plentiful (41% of bytes) but atomised into fragments too small for a kernel launch. The
prospects for a shader live entirely in the ~500 loop-bearing slices holding ~9.6% of body
bytes, and the next question for anyone continuing is whether those 500 are
*data-parallel*, not merely call-free — `ast_loop_analyzable` / `ast_loopdep_dump` already
answer that per loop and were not consulted here.

**Known limits of the enumerator.** A loop's *condition* is not a statement list, so a
call-free condition inside a call-containing loop is never a slice. Bodies the arena never
modelled emit no record, so `fn_n` is the modelled population, not everything compiled
(at `-O0` the arena is off entirely unless `MCC_FORCE_REPLAY=1`, which the tool sets). A
statement list wider than 512 statements is flushed in chunks and flagged `ovf=1`; that
never fires on either corpus.
## A slice actually ran on the GPU — 2026-08-06

`src/mccspv.h` emits a SPIR-V 1.3 compute module from an arena subtree plus its
live-in slot offsets; `tools/spvgate.c` dispatches it and compares against the
real `ast_eval_slice`. The node set is exactly `ast_eval_slice_rec`'s — Literal,
Ref, Load, Convert, Unary, Binary, If — and integer widths ≤32, which is what the
width ladder certifies. `ast_eval_slice_is64` types are refused, not approximated.

RTX 5070 Ti, 11 cases, 44 modules, 723,932 lanes: **666,450 compared points,
0 mismatches**, 44/44 pass `spirv-val`. Sweeps are exhaustive per ladder rung
(1/2/4/8/16 bits). Points where `ast_eval_binop` reports the source undefined
(signed overflow, `/0`, `INT_MIN/-1`, out-of-range shift) are *vacuous* and
excluded — 57,482 of them — so the comparison covers only what C defines.

**Short-circuit is control flow, not `OpSelect`.** `a ? b/c : d` and n-ary
`&&`/`||` emit `OpSelectionMerge`/`OpBranchConditional`/`OpPhi`, phi predecessors
taken from the *last* block of each arm. Relying on an untaken arm's division
being discarded is not the same as not computing it.

**Undefinedness is modelled, not taken on the CPU's word.** The shader mirrors
`ast_eval_binop`: signed overflow on `+`/`-` via `((a^r) & (b^r)) < 0`, on `*`
via `r/a != b` guarded at `a == 0`, `/0`, `INT_MIN/-1`, and shift counts outside
`[0,32)`. The flag rides in `SpvMod.def`, is phi'd through both arms of every
`If` and `&&`/`||` link, and lands in `out[2*lane+1]` beside the value. The gate
requires it to agree with `ast_eval_slice`'s verdict, so the 3.7M vacuous points
are *verified* rather than skipped.

**A flag computed beside a poisoned invocation is worthless.** The first version
computed the flag correctly and still performed the undefined division in the
same invocation; SPIR-V leaves that undefined, and the driver returned
`defined=1` for `0/0` while the disassembly plainly showed `defined=0`. Divisors
and shift counts are now guarded through `OpSelect` so the kernel never executes
UB at all. `divraw`/`remraw`/`shiftraw`/`ovf`/`ovfadd` exist to exercise this;
the older synthetic set had *no* undefined points once `cmp` stopped being a
shift (vacuous 57482 → 0), so it could not have caught any of it.

Three defects the differential caught, none of which a passing test would show:

- the entry point listed its StorageBuffer variables, legal only from SPIR-V 1.4;
  at 1.3 the module is invalid and the driver may do anything — this produced the
  first wrong answers;
- `OpUDiv`/`OpUMod` require an unsigned result type, and emitting them with `%int`
  **still returned correct answers** — only `spirv-val` caught it;
- signed `%` is synthesised as `a - b*(a/b)`, mirroring `ast_eval_binop`, rather
  than trusting `OpSRem` at `1 % -1` and `-8 % 3`.

**Real slices, not hand-built ones.** `MCC_ARENA_DUMP=<path>` serialises every
modelled body's production arena at the same hook `MCC_SLICE_CENSUS` uses;
`spvgate --arenas` rebuilds them and walks top-down for maximal subtrees the
emitter accepts with ≤4 live-ins. The hook is inert when unset — 0 per-test
verdict changes over 4907 cross-oracle cases.

| corpus | slices | compared | clean | mutated |
| --- | --- | --- | --- | --- |
| gcc c-torture, **all 1693 files** | 25,877 (12,157/16,208 bodies) | **1,695,607,075** | 0 | — |
| gcc c-torture, 300 files | 858 (460/1910 bodies) | 53,678,914 | 0 | **all** |
| compiler-rt builtins, 96 | 401 (73/192 bodies) | 23,174,609 | 0 | **all** |
| synthetic, 13 cases | — | 855,556 | 0 | **all** |

The full sweep is 127,355 GPU dispatches over 1,699,304,114 lanes, 0 mismatches
and 0 rejected modules. **75% of modelled bodies (12,157 of 16,208) contain at
least one slice the emitter lowers** — far above the 24% the first 300-file
sample suggested, so the alphabetical head of that suite is not representative.

**`TOK_SHL` is `'<'` (60) and `TOK_SAR` is `'>'` (62); comparisons are
`TOK_LT`=156, `TOK_GT`=159.** Real slices produced 131,627 mismatches on first
run, all one shape: `TOK_LT`/`TOK_GT` returned signed comparisons unconditionally
while `TOK_LE`/`TOK_GE` respected `uns`. `ast_eval_binop` compares *after*
`ast_eval_narrow`, which zero-extends unsigned types, so an unsigned `TOK_LT`
carries unsigned semantics. **The synthetic `cmp` case never caught this because
it was written with `'<'` and had therefore been exercising a shift, not a
comparison.** That is the argument for real corpora in one sentence.

**`gpu/spv-slice-known-positive` and `gpu/spv-slice-real` are the cells that
matter.** They require the mutated build to FAIL, reject exit 77 (a skipped
known-positive is not a pass), and `-real` additionally fails when *zero* slices
lowered — a differential over an empty set is green for the same reason a blind
one is.

**The first known-positive was worse than useless and cost two defects to
discover.** Rewriting every `OpIAdd`→`OpISub` also rewrote `spv_load_live`'s
addressing, so indices went negative, the shader read far out of bounds, the
device faulted, and every later allocation returned
`VK_ERROR_OUT_OF_DEVICE_MEMORY`. It was testing error handling, not detection —
and found `gpu_run` leaking buffers/memory/descriptor pools on both early-return
paths, and a rejected module being a silent `continue`, so a mutation producing
invalid SPIR-V would have been reported OK. Rejects now count as failures, every
path frees, and the mutation is `OpBitwiseXor val, 1` on the result before the
store: touches no addressing, cannot fault, perturbs every point. Detection went
from 6 of 11 cases to total.

**The trap for anyone extending this.** `mccast.c`'s fallback `ast_eval_slice`
stub returns 1 *without writing `*out`*, so the obvious build compares
uninitialised memory and passes everything. `spvgate` hard-errors unless
`AST_EVAL_SLICE_PROVIDED` is set. Two TUs are needed because `mccast.c` compiles
standalone only *without* `mcc.h` while the evaluator needs it for `VT_*`.

## The ladder oracle replays on the GPU — 2026-08-07

`ast_eval_ladder_rung()` delegates its replay to the device when `MCC_GPU` is
compiled in (CMake option, **OFF** by default — it puts Vulkan in the compiler's
link) and `MCC_AST_EVAL_LADDER_GPU=1` is set. Both arenas are lowered and
dispatched over the rung's whole tuple space; the verdict is rebuilt on the host
from the returned `(value, defined)` pairs. **Only the replay moves — the verdict
logic is not reimplemented on the device.** The rung falls back to the scalar
loop whenever either arena will not lower, the live-in count is out of range, or
the space exceeds 2^20, so the GPU path is an accelerator, never a coverage
change. `gpu/ladder-gpu-parity` pins it: verdicts identical GPU-vs-CPU, 1404
dispatches, 0 differing files, and **zero dispatches is a hard failure** because
identical verdicts prove nothing if the GPU never ran.

**Cross-oracle with the GPU live during every compilation.** 4907 cases, gcc
judged by clang and clang by gcc, census mode on so the GPU actually replays:
**0 behavioural changes** against the CPU baseline, `DIFF_EXIT` 53 and
`DIFF_STDOUT` 19 identical. The only 5 transitions are `PASS -> MCC_NOBUILD`,
all compile `timeout` on the huge generated `memclr`/`memcpy-a*` bodies, which
census makes slow. Nothing miscompiled.

**The JIT's own path reaches the device, not just the census.** Every production
caller of `ast_slice_equiv` is in `mccjit_embed.c`, which links `libmcc`, so
`MCC_GPU` applies there too. `jit/selftest-sliceladder` — a JIT code path, not a
diagnostic — replays on the GPU: 30 dispatches, 263,312 lanes, and its output is
**byte-identical** to the CPU run.

That byte-for-byte diff caught what the cell could not, because the cell pins
verdicts and not values. The GPU path stored the raw `int32` into
`res->diff_a`/`diff_b` where the CPU stores the value fitted to the source type;
for an unsigned root that is a zero-extension, so one counterexample printed
`a=4294967295` on the CPU and `a=-1` on the GPU. **The verdict was right and the
diagnostic was wrong**, which is the worse way round — a counterexample is what a
human reads when the oracle refutes something. Both are now fitted through
`ast_eval_slice_fit` with the root's own width type, and the rung refuses the GPU
path when either root has no static type rather than guessing one.

## The runtime JIT never consults the oracle — measured, 2026-08-07

**`mcc` cannot currently be shown to run RIR replays on the GPU during ordinary
compilation or ordinary JIT execution, and the reason is structural rather than
configurational.** This was chased all the way down; the negative result is worth
more than the knobs tried.

`ast_slice_equiv()` — the only entry to the ladder, hence the only route to the
GPU — is reachable in production from exactly one place: `mccjit_slice_search()`
→ `mccjit_kernel_search_from_blob()`. Everything else calling it is a selftest.
And `mccjit_slice_search` opens with

    if (!async || mccjit_last_allfp || mccjit_last_ret_wide || np < 1 || np > 3
        || st->nsample <= 0 || !st->blob) return NULL;

where `np` is `mccjit_last_nparam`, a **global set by a previous build**. On a
slot's first promotion nothing has populated it, so the guard rejects before
`mccjit_kernel_search_from_blob` is ever called. The runtime swap path instead
verifies with the KGC scalar/purity check, which never asks the ladder anything.

Confirmed end to end on a program built `--embed-jit --jit-threads 2 -lvulkan`,
with a `noinline` hot pure scalar function so calls actually reach the slot:
pool starts (`live=2`), stub installs, threshold trips, `promote-async` fires,
the variant is promoted `route=kgc` and the answer is correct — and
`[ladder-gpu]` prints **zero** times. The same binary prints 30 dispatches when
the oracle *is* called, so the backend is live and the absence is real, not a
misconfiguration. Knobs tried without effect: `MCC_JIT_LAZY`,
`MCC_JIT_SEARCH_SLICE`, `MCC_JIT_SEARCH_MS`, `MCC_JIT_HOT_CALLS`,
`--jit-threads`.

Two consequences, and neither is a measurement problem:

1. **The cross-oracle run needs `MCC_AST_EVAL_LADDER_CENSUS=1` for the device to
   do any work at all.** Without it the same green board appears with the GPU
   idle, so census is what makes that run mean anything — and census is a
   diagnostic, not production.
2. **Making the claim true is a JIT design change**, not further instrumentation:
   the promotion path would have to verify candidates through the ladder instead
   of, or in addition to, the KGC check. That is a behavioural change to how the
   JIT admits variants and is deliberately not attempted here.

`-O2` and `-fopt-slice` dispatch nothing either, for the same reason: neither
consults the equivalence oracle.

**Two emitter defects surfaced only here, and `spirv-val` accepted both.** They
crashed NVIDIA's SPIR-V compiler (`libnvidia-glvkspirv`) with SIGILL:
`OpBitwiseOr %int` fed a `%uint` operand to convert the invocation id (legal,
but `OpBitcast` is the idiom), and the signed multiply-overflow check emitted
`r / select(a == 0, 1, a)`, which becomes a wholly constant-foldable division
when `a` is a literal — `9 * x`, a shape the synthetic cases never produced
because they all put the constant second. It is now `OpSMulExtended` with a
high-word test and emits no division at all.

**A dispatch cap was added as a "mitigation" and then reverted, and that is the
lesson.** The crash correlated with dispatch count on the first file, so a cap
looked principled; `MCC_AST_EVAL_LADDER_GPU_MAX=1` still crashed on
`vla-stexp-9.c`, refuting it in one command. Shipping the cap would have buried
two real emitter bugs behind a magic number.

**Still not claimed.** Everything in the section above runs through `spvgate`
offline; the ladder path is the only thing the compiler itself dispatches.

### The runtime JIT cannot carry the backend yet — 2026-08-07

Wiring `MCC_GPU` into `libmcc_jitengine` does make a compiled program's runtime
JIT replay on the device, and it works: `--embed-jit --jit-threads 2` on a
`noinline` hot pure function gives 5 rungs / 10 dispatches / 131,628 lanes with
the answer matching gcc, clang and the JIT-off build. **It is off anyway,
because it corrupts memory.** `pr50729.c` segfaults **4 of 40 runs** with the
backend present and **0 of 40** without; `pr100499-1.c` did too until the mutex
and exit-quiesce landed, and is now 0/40.

The fault, caught under gdb: `cmp 0x10(%r8),%rdi` inside `libnvidia-gpucomp.so`
with `si_addr = 0x0` — the driver dereferences a **NULL context pointer**, which
is per-thread/per-instance driver state being absent rather than anything wrong
with the shader or our own allocations.

Eliminated, each with a decisive test, so nobody repeats them:

| hypothesis | test | result |
| --- | --- | --- |
| invalid shader | `spirv-val` + standalone dispatch of all 10 modules | all valid, all `rc=0` |
| data race | mutex + exit quiesce | fixed `pr100499-1`, not `pr50729` |
| stack exhaustion | 64MB worker stack | no change |
| Vulkan-in-thread under mcc | threaded probe, gcc vs mcc | both clean |
| lazy init inside worker | eager main-thread init | no change |
| static TLS surplus | `GLIBC_TUNABLES` | no change |
| dispatch count / module order | cap sweep | independent |
| JIT-worker environment | dedicated GPU dispatch thread | 6/40, no change |

**Narrowed to one call.** Instrumenting every step of the dispatch puts the
fault exactly at `vkCreateShaderModule` -- the driver compiling the SPIR-V,
which is what the `libnvidia-gpucomp` backtrace already said. The bytes are
*verified intact at that call site*: magic `07230203`, 223 words, hash printed
immediately before the call. The identical module compiles and dispatches fine
from a standalone gcc-built process (`spvgate --spv`). So the driver faults
compiling a valid module, and only inside an mcc-produced executable.

Two further eliminations from that round: the SPIR-V buffer is **not** corrupted
by using the compiler's allocator off-thread -- switching the GPU path to plain
`malloc` changes nothing (40/40) -- and `dlopen`ing libvulkan instead of linking
it makes the crash *deterministic* (40/40 vs 4/40) under both `RTLD_LOCAL` and
`RTLD_GLOBAL`. The deterministic `dlopen` build is therefore the reproducer to
debug with; it is far easier to work with than the 10% linked case.

**Nor is it threading.** The same mcc-built probe, moved into a spawned pthread
so it matches the failing context, still makes 50 clean `vkCreateShaderModule`
calls on the crashing module -- as does the gcc build. Combined with the result
below, the isolation is complete: not the module, not the executable, not the
thread. The only thing left that differs is that **the JIT is running in the
process**, and it crashes before it has reemitted anything.

**It is not mcc's code generation.** The obvious suspicion -- that mcc-produced
executables are somehow malformed in a way the driver trips over -- is wrong. A
small program compiled *by mcc* that loads the exact crashing module and calls
`vkCreateShaderModule` on it 50 times completes cleanly, as does the same
program built by gcc. So the executable is fine and the module is fine; what
differs in the failing case is that **the JIT has been running in the process**.
That points the remaining investigation at what the JIT does to process state --
`host_runmem_alloc` and the `mprotect` of code pages, the hot-patch, the fork
handlers -- and away from codegen and from the shader, which is where the first
several rounds of this were wasted.

**Tooling limits, so nobody wastes time on them.** glibc's heap checkers
(`MALLOC_CHECK_=3`, `MALLOC_PERTURB_`) report nothing and the crash is unchanged,
so this is not heap-metadata corruption. **Valgrind cannot run mcc-produced
executables at all** -- it dies with SIGILL in `_dl_start` before `main`, so the
obvious instrumentation is unavailable here and that is worth knowing before
reaching for it.

At the crash point the JIT has done: lazy stub install (an RWX mmap with
hand-assembled machine code), counter ticks, blob deserialize and arena build,
then the ladder. It has *not* yet reemitted a kernel -- `ast_slice_ladder_explain`
runs inside `mccjit_kernel_search_from_blob`, before `mccjit_slice_search`
reemits. So the suspect set is small: the stub's RWX mapping, `host_runmem_alloc`,
and the arena work.

**Measure with 40 runs, not 5.** Two separate times a stale embedded engine blob
made an intermittent crash look deterministic, and once made a broken build look
clean; `cmake --build` does not reliably regenerate the blob after a CMake-level
change to the engine target. Rebuild the whole project and re-measure before
believing any result in this area.

Second, independent reason it is off: linking Vulkan into the engine makes
**every** `--embed-jit` program need `-lvulkan` to link at all.

**The clean integration point is `ast_eval_ladder_rung()`, and its prerequisite
is now met.** That function is an embarrassingly-parallel loop over `space`
codes, each replaying both arenas at one point, and the JIT already invokes it
through `mccjit_kernel_search_from_blob`. It *is* bulk RIR replay, so moving it
to the GPU makes the claim literally true while doing work the JIT already wants
done — unlike speculative offload, where the census says the payoff is thin
(1.91% loop-bearing slices). It could not have been attempted before the
definedness model landed: a rung must decide vacuity itself, and until the
shader modelled UB the CPU had to visit every point anyway, which is the whole
cost the GPU was meant to remove. What remains is plumbing, not semantics —
emit both arenas, compare on device, and reduce. The obstacle is that it puts
Vulkan in the compiler's link, so it wants a `dlopen` loader behind a build gate
rather than `-lvulkan`.

Two smaller debts: `ast_bad_type` and `is_float` are duplicated in the gate
because they are static/inline inside `mccast.c`'s `MCC_INTERNAL` half, and the
gate needs two TUs because `mccast.c` compiles standalone only *without*
`mcc.h`.

## Each suite is now scored by the other vendor's compiler — 2026-08-06

`tools/xoracle.py` judges gcc's tests with clang and clang's with gcc, on exit
status and stdout bytes, never on a suite's own directives. `qualify` builds each
run-mode test with the oracle at `-O0` *and* `-O2`, runs both, and runs the `-O0`
binary twice; a test joins the oracle set only if all three agree, which is what
keeps UB-sensitive and nondeterministic programs from later surfacing as phantom
miscompiles (23 of 5352 excluded on exactly that basis). `check` replays the
cached set against one mcc configuration, so configurations diff test-by-test.

**The llvm direction needed rescuing.** clang's suite is compile-and-FileCheck:
of 10861 C files just 14 are execution tests. The real execution corpus is
`compiler-rt/test/builtins/Unit`, which `xsuite.py` skips wholesale as
`runtime-lib-suite`. Pairing `X_test.c` with `lib/builtins/X.c` and compiling both
with the compiler under test lifts that direction 7 → 139 cases.

`LINK_POLICY` is kept distinct from `MCC_NOBUILD`: mcc embeds `libmccrt.a`, which
already defines `__udivmodti4`, so linking compiler-rt's copy collides where
gcc's lazily-resolved libgcc does not. Folding that into a failure count would
overstate the defect rate.

4907 qualified cases: **-O0 96.70%, -O2 96.56%**, 72 behavioural divergences at
both levels with identical membership — so front-end/semantic, not optimizer. The
12 on the builtins side are all `tf`/`ti` (`__float128`, `long double`,
`__int128`), which `ast_bad_type()` already rejects. Re-run after the CType
stamping, width ladder and SPIR-V commits: **0 per-test verdict changes** at
either level.

## Byte coverage is a proxy. The real metric is the lowerable census — 2026-08-06

Byte coverage only proves the arena can regenerate **the same target's** code. The goal is
retargeting slices between anonymous `invoke` nodes to GPU compute shaders, which is
strictly stronger. The **lowerable census** measures that directly and lives beside
`modelled`/`gap`, not instead of it — byte coverage is still the single-target regression
detector and is untouched.

The predicate is **one function**, `ast_low_node()` in `src/mccast.c`. Tightening or
loosening it is a one-place change. Every arena node gets exactly one class:

| class | meaning |
| --- | --- |
| `ok` | none of the below |
| `asm` | an inline-asm node (`AST_OP_ASM`/`ASMGEN`/`ASMOPS`) |
| `reg` | a `Ref` naming host machine state: a hardware register (`valmask < VT_CONST`), or `VT_CMP`/`VT_JMP`/`VT_JMPI` |
| `opaque` | `AST_Poison`, or an op with no portable meaning: VLA, VLA_RESTORE, VAARG, VASTART, GGOTO, the atomics, the complex builders |
| `call` | `AST_Invoke` — a slice is the code *between* anonymous invokes, so an interior invoke ends the region by definition |
| `type` | not type-complete: `ast_bad_type()`, a `Convert` with no operand, or a dereference (`Load`/`Store`/MEMBER/MEMBER_ARROW) whose address operand derives from no node carrying a pointer/array/struct type |
| `frame` | the value's meaning is a host frame offset: `AST_OP_ADDR`, a `VT_LLOCAL` `Ref`, or a `VT_LOCAL` `Ref` (see levels) |
| `global` | a `Ref` carrying `VT_SYM` — a host link-time address |

A node is **clean** when its whole subtree is `ok`, i.e. it roots a maximal lowerable
region. A body is lowerable when every node in its arena is `ok`.

The `type` clause independently rediscovered the lost-cast defect from the arena alone,
with no replay, and `rir_hook_cast_type()` closed it on 2026-08-06. What that bought the
lowerable census was almost nothing, and **that null result is the most useful thing this
census has said so far**. Splitting `type` into its three sub-clauses over the self corpus
at `-O2` (temporarily relabelling them, then reverting):

| sub-clause | share of nodes | blocks | SOLE blocker of | after the cast fix |
| --- | --- | --- | --- | --- |
| `ast_bad_type()` — struct, bitfield, `long double`, `__int128` | 0.823% | 42.597% of body bytes | 0.127% | 0.822% / 42.576% / 0.127% |
| deref whose address operand derives from no typed node | 0.706% | 50.959% of body bytes | 0.237% | 0.705% / 50.840% / 0.233% |
| `Convert` with no operand | ~0 | ~0 | ~0 | unchanged |

The deref clause barely moved even though **every** `abort` in the corpus was fixed,
because the lost cast was never what dominated it. The real cause is that the arena is
**type-sparse**: gate `ast_low_node()` on the node's static type alone and **35.6% of
arena nodes carry no static type at all**, in 100% of bodies. `ast_low_base_ptr()` is a
depth-8 search *around* that hole.

### Closing the hole — `MCC_RIR_STAMP`, 2026-08-06

`rir_to_arena` now stamps every node with the static `CType` of the value it produces.
Untyped share on the self corpus, `-O0`/`-O1`/`-O2`/`-O3`, 407,913 nodes:

| | unknown | explicitly void |
| --- | --- | --- |
| `MCC_RIR_STAMP=0` (default) | 35.570% / 35.598% | 0 |
| `MCC_RIR_STAMP=1` observed only | 17.618% | 3.574% |
| `MCC_RIR_STAMP=2` observed + derived | **0.010%** (42 nodes) | 14.95% |

`=1` alone leaves every value-producing node typed but nothing else: what remains at that
level is `Store` 21,044, `BasicBlock` 19,127, `If` 12,625, `Invoke` 8,809, `Jump` 5,392,
`Return` 4,707 — statements and void calls, all of which `=2` resolves structurally.
Value nodes are already done at `=1`: `Binary` 88, `Load` 67, `Unary` 8, `Ref`/`Literal`/
`Convert`/`StoreVal` 0.

Three channels, all in `src/mccrir.c`:

1. **Observed** — `rir_stamp_deep()` runs at the head of `rir_stamp_sv()` and, for every
   shadow-stack slot holding a still-untyped node, defers `(node, kind, CType)` from the
   parser's own `SValue`. `rir_stamp_flush()` replays the log at the end of
   `rir_to_arena()`, first observation wins. This is the parser's answer, not a re-derivation.
2. **Leaves** — `rir_leaf_slot()` stamps at creation, which is the only way to reach the
   14,485 dead unparented `VT_CONST` `Literal`s (3.6% of the whole arena!) that the vstack
   refill in `rir_reconcile_sv()` creates at slot 0 and nothing ever consumes.
3. **Derived** (`=2` only) — `rir_stamp_derive()`: pointer arithmetic (`Binary +`/`-` with a
   pointer operand), dereference (`Load` of a `VT_PTR`), `Store`/`StoreVal`/`Return` from
   their operand, `Invoke` from the callee `Sym`'s return type, and control nodes
   (`BasicBlock`, `If`, `Jump`) as explicitly void.

**`VT_VOID` is `0`, so "void" and "unset" are the same bit pattern** in `type_t`. A shader
generator must tell "this node yields nothing" from "we do not know what this node yields",
so the stamp carries its own `st_have` bit. 14.95% of the arena is genuinely void:
19,127 `BasicBlock` + 12,626 `If` + 5,392 `Jump` + 14,485 dead literals + 8,732 `void`
calls + 562 `void` returns.

**What is still unknown is 42 nodes**: 34 `Binary` whose result the vstack never witnessed
(folded address arithmetic retained as a child) and 8 `Unary` inline-asm ops, which have no
C type by construction.

The wide corpus agrees and is a wider sample of C: 1,052,863 nodes at `-O2`, **29.442% →
0.014%** unknown (151 nodes — 107 inline-asm `Unary`, 44 `Binary`), 12.890% explicitly void.

**The stamp does NOT live in `type_t`, and that is the load-bearing finding.** The first
implementation wrote the observed types straight into `type_t`/`type_ref`/`type_bp`/`type_bs`
— the columns that were "already there and simply unset". With the toggle on that
**miscompiled**: `exec/cast_operator`, `exec/complex`, `exec/integer_promotion`,
`exec/lex_extras`, `fuzz/smoke` and both `selfhost-output-parity` cells failed, `__va_arg_inline`
started aborting the replay with `invalid operand types for binary operation`, and `kept`
fell 96.0% → 93.0%. `type_t` is not a *description* of the node, it is the **emitter's own
input**: `ast_replay_*` and every AST strategy pass read it, and they were all written
against an arena where `t == 0` means "leave this alone". Adding true facts to that column
changes what the compiler emits.

So the static type is a **second, parallel view**: `st_have`/`st_t`/`st_ref`/`st_bp`/`st_bs`,
lazily allocated exactly like `wide_hi`/`wide_r2`, read through `ast_stype_t()` /
`ast_stype_ref()` / `ast_stype_bp()` / `ast_stype_bs()` / `ast_stype_known()`, which fall
back to `type_t` when it is set. Nothing in the emitter reads them. `ast_low_node()` and
`ast_low_base_ptr()` do. Result: **`src/mcc.c` objects are byte-identical with the toggle
on and off at all four levels**, and the whole 8845-cell suite is green with
`MCC_RIR_STAMP=2` except the two census ratchets, which move by design.

### What type-completeness costs the lowerable census — it is worse, and that is the point

Same binary, same corpus (2631 bodies, 407,913 nodes), level-1 (default) locals:

| | `-O0` | `-O1` | `-O2`/`-O3` |
| --- | --- | --- | --- |
| whole bodies lowerable | 9.205% → **7.684%** | 9.236% → **7.678%** | 9.236% → **7.678%** |
| of modelled body bytes | 2.257% → **1.572%** | 2.278% → **1.559%** | 2.186% → **1.528%** |
| nodes in a maximal region | 41.755% → 41.432% | 41.747% → 41.424% | 41.747% → 41.424% |
| nodes in regions ≥16 | 5.014% → 4.075% | 5.018% → 4.078% | 5.018% → 4.078% |
| `type` nodes_pct | 1.518% → **2.313%** | 1.519% → **2.315%** | 1.519% → **2.315%** |
| `type` sole_bytes_pct | 0.377% → **1.062%** | 0.416% → **1.135%** | 0.367% → **1.025%** |
| `type` bytes blocked | 62.234% → 74.537% | 62.178% → 74.538% | 61.297% → 73.717% |
| `frame` sole_bytes_pct | 0.256% → 0.025% | 0.257% → 0.025% | 0.249% → 0.026% |
| `global` sole_bytes_pct | 1.224% → 0.799% | 1.226% → 0.801% | 1.242% → 0.805% |

**The census was flattered by the hole.** `ast_bad_type(0)` is false, so an untyped node
scored `ok` — the predicate said "lowerable" about values whose width and signedness it did
not know. With the hole closed, 0.80% of all arena nodes turn out to carry a type no shader
can represent (struct, bitfield, `long double`, `__int128`), `type` overtakes `frame` and
`global` as a sole blocker, and 1.56 points of "lowerable bodies" evaporate. The equivalence
oracle could never have run on the 9.2% figure; 7.68% is the number that was always true.

Byte coverage is unmoved by the toggle at every level (100.000% / 100.000% / 99.965% /
99.965%, gap 0 / 0 / 467 B / 467 B), which is the proof that the two views are independent.

**Cost.** Compile time for `src/mcc.c`, best of 3, two independent runs: `-O0` −1.8%..−1.0%,
`-O1` +1.7%..+2.6%, `-O2` +2.0%, `-O3` +0.4%..+1.8% — at the noise floor. Peak RSS: `-O0`
+0.6%/−0.2%, `-O2` +2.0%/+1.6% at `=1`/`=2`. The five shadow columns are 15 B/node and are
`calloc`ed only on the first `ast_set_stype()`, so with the toggle off they cost one NULL
test per query and nothing else.

**Next.** The `type` class is now dominated by `ast_bad_type()` — aggregates and the
non-portable scalars — not by missing information. Splitting a struct-typed value into its
scalar fields, or admitting fixed-layout aggregates into the shader ABI, is what moves it
now. `ast_low_base_ptr()`'s depth-8 search is dead weight under `MCC_RIR_STAMP=2` and can be
deleted once the toggle becomes the default.

**The `VT_LOCAL` rule is the one genuinely open question** (a local's `ival` is the
parser's own frame offset), so it is a level and all three are measured in one walk:

- **0 strict** — every `VT_LOCAL` `Ref` is frame-dependent.
- **1 default** — a `VT_LOCAL` `Ref` is frame-*in*dependent iff no node anywhere in the same
  arena takes the address of a local (no `AST_OP_ADDR`, no `VT_LLOCAL` `Ref`) **and** the
  `Ref`'s own type is a scalar the arena models. With no address-of-local in the body no
  slot's address is observable, so every slot is a pure value cell and its offset is only a
  name.
- **2 loose** — a `VT_LOCAL` `Ref` never disqualifies.

Pointer *values* are allowed at every level: a pointer reaching a region is a buffer
binding to resolve at the region boundary, not a frame dependency.

Numbers at `2aeb2989`, identical across `-O0..-O3` to within 0.1pp and byte-identical
across six consecutive runs:

| | self | wide |
| --- | --- | --- |
| whole bodies lowerable | 9.15–9.18% of bodies, 2.17–2.27% of body bytes | 15.62–15.63% of bodies, 1.19–1.22% of body bytes |
| nodes inside a maximal lowerable region (strict / default / loose) | 26.26 / 41.61 / 66.12% | 27.12 / 34.89 / 64.25% |
| nodes in regions ≥3 nodes (default) | 16.71–16.73% | 9.74–9.75% |
| nodes in regions ≥16 nodes (default) | 4.97% | 2.92% |

**What dominates.** By nodes rejected, `frame` is far and away the largest disqualifier —
13.7% of self nodes and **25.1% of wide nodes**, roughly triple the next class. `global`
(9.1% self / 10.9% wide) and `call` (5.1% / 8.7%) follow; `type` is only 1.5% / 0.9% of
nodes but blocks 62% / 80% of body *bytes* because it lands in the big bodies. `asm` and
`opaque` are noise (<0.02%). So the single most valuable thing to settle is whether the
arena needs a value layer above `AST_Ref`'s frame offsets: moving `frame` from level 0 to
level 2 alone lifts region coverage from 26% to 66% on self and 27% to 64% on wide.

Provisional, in order of how much they would move the number:

1. The **region boundary** is "maximal clean subtree". The real definition is maximal
   call-free slices between anonymous invokes; when that lands, only the maximality test in
   `ast_low_census()` changes.
2. The **locals verdict** is level 1 by assertion, not by an alias audit. An audit that
   distinguishes address-taken from genuinely-value `Ref`s replaces the whole-body `pinned`
   flag with a per-slot one.
3. `global` is rejected outright. A `Ref` to a read-only global array is exactly a shader
   buffer binding, so some of that 9–11% is recoverable, not a real blocker.
4. Region size thresholds are 3 and 16 nodes (`AST_LOW_MIN_REGION`/`AST_LOW_BIG_REGION` in
   `src/mccrir.h`), picked to match the existing slice window and a plausible kernel.

Run it with `ctest -R rir-coverage` (self, gated) or `ctest -L census` (wide). The predicate
itself is pinned per class by `ctest -R rir-lowerable-classes` against `tests/rir/low/*.c`,
one file per class — that cell is what catches a predicate that silently stops firing.
`MCC_RIR_LOW_DUMP=<func>` prints the per-node classification of one body.
**Only percentages are banked, never totals.**
## The lowerable ratchet is self-referential — read before re-banking

`tools/rir-coverage.py`'s `lowerable` floors are measured over `src/mcc.c`, which
amalgamates the compiler's own source. **Any code added to `src/` moves the number**, and
diagnostic code moves it the wrong way: the `MCC_AST_REFCENSUS`/`MCC_AST_FRAMEPERT`
instrumentation added 207 lines of env parsing, file I/O and classification to
`src/mccast.c`, all of it inherently non-lowerable, and `bodies_pct` fell 9.1821% ->
9.1048% on the self corpus at every level. The ratchet fired correctly; nothing regressed
in capability. Instrumenting for lowerability made lowerability measure worse.

Consequences, in order:
- **A `lowerable` regression is not evidence of a defect until you have checked whether
  `src/` grew.** Diff the corpus first, then investigate.
- Re-bank with `python3 tools/rir-coverage.py <build-dir> --update-bank-low`. It prints
  the pre-bank check's FAIL lines and then banks, which reads alarmingly; the banked value
  afterwards is what counts.
- The metric needs one of: a tolerance band, exclusion of diagnostic-only code from the
  corpus, or a corpus that is not the compiler itself. Until then this will recur on every
  substantial `src/` change, and the temptation each time is to re-bank without checking
  which of the two causes applies.

The byte-coverage floors do not have this problem to the same degree — they are a ratio of
modelled to emitted bytes, so new code lands on both sides.

## How frame-bound is the arena? — measured 2026-08-06

> **Framing correction, same day.** This section was written to test whether frame-bound
> Refs obstruct lowering slices to GPU compute shaders. **They do not, and the question is
> retired.** An address is a 64-bit integer and a shader computes it exactly: `tests/gpu/
> rev64.comp` already takes a base address as `base_lo`/`base_hi` push constants and does
> `add64`/`mul64` over `uvec2` with `umulExtended`, on hardware with no native 64-bit int.
> A frame offset is an operand, not an obstacle; the memory it reaches is a buffer
> binding. The `frame`, `global` and `laddr` classes that the lowerable census counts as
> disqualifiers are all just integers, and counting them as blockers understates the
> lowerable fraction badly. What genuinely cannot lower is what is not computation over
> buffers: an opaque `invoke`, host machine code (`asm`, <0.02% of nodes), and — the real
> one — **a lost static type**, because exact arithmetic requires knowing the width and
> signedness of what is being computed. The measurements below stand as measurements; the
> conclusion drawn from them does not.


A local is not named in the arena. `rir_leaf_slot` writes the parser's `sv->c.i` into the
node's `ival` and that integer **is** the variable's identity: `ast_du_build` hashes slots
by `int off`, `ast_cprop_escapes`/`ast_local_is_readonly`/`ast_promo_off`/`ast_argsub_off`
and `ast_eval_slice_livein` all key on it, and `gfunc_epilog` derives the frame size from
`loc` alone — nothing in the arena states it, which is why `ast_reemit_with_gates` has to
rescan every Ref for the minimum offset before replaying.

Two diagnostics, both off by default, quantify this.

`MCC_AST_REFCENSUS=<path>` dumps one line per `AST_Ref` at `ast_func_end`: class, offset,
`op`, `type_t`, the size of the maximal call-free subtree containing it, and a slice id.
Over amalgamated `src/mcc.c` (2307 functions, 356941 nodes, 101011 Refs) and `tests/exec`
(1285 functions, 25238 Refs, excluding the `translation_limits.c` outlier):

| class | mcc.c | tests/exec | share of Refs in call-free slices >= 32 nodes |
| --- | --- | --- | --- |
| `lval` scalar, non-escaping — SSA-renamable | 46.85% | 41.19% | 70.30% / 85.77% |
| `laddr` address-taken slot | 17.70% | 9.56% | 16.06% / 8.54% |
| `lagg` struct/array/VLA | 3.64% | 7.27% | 4.48% / 1.85% |
| `sym` symbol-relative — already frame-free | 31.77% | 40.87% | 9.00% / 3.39% |

66.5% of all Refs are frame-resident, but only 286 of 214889 sit above the frame pointer.
The bigger a call-free slice gets, the purer it gets: 53% of slices of >= 8 nodes and
47% of >= 32 nodes are *Ref-closed* (every Ref is `lval`/`sym`/`reg`). The blocker in the
rest is `laddr` first (2115 slices), `lagg` second (848). Slot reuse is rare and bounded:
in `src/mcc.c` only 6.90% of distinct offsets ever carry two different `VT_BTYPE`s, and
the median function has none.

`MCC_AST_FRAMEPERT=<base>[:<scale>[:<pad>]]` re-lays-out the frame at replay time: every
own-frame Ref offset below the post-prolog watermark `rir_body_loc_sv` maps to
`floor + base + (off - floor) * scale`, `ast_alloc_loc`'s recorded slots are reallocated
(with `pad` extra bytes) and bound into a per-function map, and `loc` is re-derived. Under
`-O2 -fno-replay-fallback`, over 8819 ctest cells:

| perturbation | cells failed | pass |
| --- | --- | --- |
| none | 0 | 100% |
| `-64:1` rigid 64-byte shift | 156 | 98.2% |
| `-256:1` rigid 256-byte shift | 159 | 98.2% |
| `0:2` every gap doubled | 201 | 97.7% |
| `-64:4` shifted and gaps x4 | 186 | 97.9% |

**The count is flat in the magnitude and the kind of the perturbation.** Dilating the
whole layout 4x costs 30 more cells than nudging it 64 bytes. So the arena is not a script
for one layout — it is layout-agnostic except for a fixed set of constructs that stash a
frame offset in a *second* channel the Refs do not own. 140 cells fail under all four:
111 of them assert an implementation outcome (`ast/replay-*` demands byte-faithful replay,
`optfire/*` demands a specific pass fires) and must fail under any perturbation. The 26
behavioural ones name the second channels exactly: aggregate ABI slots (`struct_byval`,
`struct_return`, `union_byval`, `return_struct_in_reg`, `aggregate_perm`, `alignas_over`),
`r2` pair slots (`complex`, `complex_annexg`, `c11_imaginary_suffix`, `float16`), VLA
bookkeeping (`vla/basic`, `vla_empty_init`), `__attribute__((cleanup))`, atomics — and the
entire JIT slice path (9 `jit/selftest-*`, `run-opt/native`, `jit-submit-aot-diff`).

That last cluster is the one that matters for GPU lowering. `ast_slice_ident_hash` already
canonicalises offsets to dense indices via `ast_sid_off`, so a slice's *identity* is
frame-free — but `ast_slice_splice` copies `ast_ival` verbatim and `ast_slice_live_ins`
hands back raw `int32_t *offs`, so a kernel can only be installed into a site whose frame
happens to match. Making Refs frame-independent is the smaller job; it is the four
side-channels and the slice ABI that need the work.

## The configuration surface moved: read this before running any recipe below

**Every `MCC_AST_*` and `MCC_RIR_*` gate is now a `-f` flag.** 113 of them, generated
from one list in `src/mccopt.h`, which also generates the driver's option table so the
two cannot drift. `-fno-<name>` turns a knob off, `-f<name>` on.

This matters for reading the rest of this file, because **a retired variable in the
environment is ignored silently** — no warning, no error, exit 0. A recipe that still
spells one will run, print a plausible board, and measure nothing. That failure mode
cost three separate defects during the conversion: the superopt search explored a
space where nothing changed and passed; `runtime-bench`'s ratchet read its win as
+0.0% instead of erroring; `optfire`'s extra-gate column stopped firing. The runnable
recipes in this file were repointed; prose that merely *names* a retired variable was
left as history.

Translation for the ones this file uses most:

| was | now |
| --- | --- |
| `MCC_RIR_NOFB=1` | `-fno-replay-fallback` |
| `MCC_AST_PROMOTE=0` | `-fno-promote-locals` |
| `MCC_AST_TEMPLATES=0` | `-fno-reemit-templates` |
| `MCC_AST_INLINE=0` | `-fno-inline` |
| `MCC_AST_SEARCH=1` | `-fopt-search` |
| `MCC_AST_NARROW_FIX=1` | `-fnarrow-fix` |
| `MCC_AST_BITFLAG=1` | `-ftree-switch-conversion` |

**Still environment variables, deliberately.** The numeric tuning knobs
(`MCC_AST_CSE_WINDOW`, `MCC_AST_TILE_SIZE`, `MCC_AST_INLINE_DEPTH`, …) and the RIR
measurement handles this file's censuses are built on — `MCC_REPLAY_IR`,
`MCC_RIR_PROD`, `MCC_RIR_FORCE`, `MCC_FORCE_REPLAY` — are unchanged, so every
`[rir-prod]` / `[rir-total]` recipe still works verbatim. So are the `MCC_JIT_*`
knobs: the embed JIT runs inside programs mcc *compiled*, where a compiler flag
cannot reach it, which is the same reason `OMP_NUM_THREADS` is an environment
variable.

**`-O` is a ladder now, not a dial.** 1–3 are the settled levels; **4–12 are one
in-development optimizer each**, cumulative; **13 and up run the strategy search with
the level as its budget in seconds**. Anything in this file that says `-O4` meaning
"search for 4s" now means "`-O3` plus one x86-only pass" — the search entry is
`-O13`. `MCC_OPT_SEARCH_LEVEL` in `mccopt.h` is the single definition, and
`ast_opt_defaults` refuses to build if a knob is ever placed at or past it.

A pass that is merely unfinished belongs on the ladder; one that is **known to
miscompile does not**. `-fjit-splice` was off-by-default, exercised by nothing, and
segfaults a threads program; putting it on rung 12 made every level above it emit
wrong code, and the suite caught it. It is back to opt-in.

**Compile-time is down to four switches** outside target/host: `MCC_DIAG`
(instruments mcc as it runs — allocator/Sym/optrace, plus the warning set and
`mcc_s`/`mcc_c`), `MCC_DEV` (differential self-checking that aborts on divergence —
the AST side-car coherence oracle, and the JIT fault injectors, which therefore
cannot be switched on in a shipped compiler), `MCC_CONFIG_TRACE` (`MCC_TRACE` alone,
~12,600 call sites), and `MCC_EMBED_JIT`. Deleted outright, now always compiled in:
`MCC_CONFIG_OPTIMIZER`, `MCC_CONFIG_DIAG_RT`, `MCC_CONFIG_LSP`, `MCC_CONFIG_ASM`,
`MCC_REPLAY_IR` (the compile-time half), `MCC_IR_CAPTURE`, `MCC_PROFILE` with
`mcc_p`. Their capabilities are reachable at runtime instead: `-O0`, `-b`/`-bt`/
`-fsanitize=bounds`, `--lsp`, `-fasm`/`-fno-asm`.

**Coverage, as wired.** `tests/optfire/flagsweep.sh` is three ctest surfaces, all under
the `flagsweep` label. `flagsweep/accept` checks every flag in the table accepts both
spellings (113/113). `flagsweep-exec/<flag>` — **113 cells, one per `MCC_OPT_ROW`** —
turns the flag on and off at `-O2` and checks twelve exec goldens still compute the
`-O0` answer. `flagsweep-cover/<row>` runs the same corpus under a **3-way covering
array**, opt-in behind `-DMCC_FLAGSWEEP_FULL=ON` and the `flagsweep-full` label.
Together they cost **4.2s wall at `-j32`** and the covering array another 6.6s.

The table has **113 rows**, not 115: `grep -c 'MCC_OPT_ROW('` counts the doc comment at
`mccopt.h:63` and the `#define` at `:181`. **34** flags were referenced nowhere in
`tests/`/`tools/`/`CMakeLists.txt` before this, counting a reference as the name on a
token boundary anywhere — `-f<name>`, `-fno-<name>`, or a bare `|`-field in the optfire
data files. Reading it strictly as "something passes this spelling" (`-f`/`-fno-` only)
the figure is **58**. The recorded 43 does not reproduce under either rule. Every one of
them owns a cell now — this is the population `-fjit-splice` came from. The first version of that harness used two synthetic programs and
*passed* `-fjit-splice`; it uses real goldens now and fails it. A sweep that misses the
bug you already have is worse than no sweep, because it reads as coverage.

**The covering array is a 3-wise guarantee, not enumeration.** Exhaustive three-deep is
C(113,3) × 2³ = 1,872,568 configurations. `tests/optfire/cover3.txt` is **74 rows** such
that every one of the 1,587,880 three-flag settings of the 107 varying flags appears in at
least one row — the identical guarantee for any bug needing three flags or fewer, and no
guarantee at all for one needing four. **107, not 113: six flags are pinned, so a bug that
needs one of them at its non-default setting is outside the guarantee by construction.** It is built by deterministic IPOG (`cover3.py gen`, no RNG, byte-identical
on any host) and **`flagsweep/cover3-verify` re-proves the property on every ctest run**
rather than trusting the generator: it re-derives the flag list from `mccopt.h`, so a new
`MCC_OPT_ROW` that nobody regenerated for fails the cell.

**Six flags are pinned out of the array**, each a debt, not a decision:
- `jit-splice` at 0 — the known miscompile. It is also the one `KNOWN_RED` entry in
  `flagsweep.sh` (`jit-splice:on:random_stuff`), so `flagsweep-exec/jit-splice` is XFAIL
  rather than red, and self-cleaning: if it starts passing the cell fails and says so.
- `replay-cmp-materialize`, `replay-landor-invert`, `storeval-call`, `replay-while-comma`
  and `replay-loopcond-store`, all at their `MCC_OPTD_ALWAYS` default of 1. These are
  bisection handles and replay-fidelity knobs, not alternative lowerings: their off-state
  is a deliberately incomplete path kept so a suspected arena defect can be confirmed.
  Sweeping them off measures the handle, not the compiler.

**`replay-fallback` is no longer pinned.** The array now varies it, which is what the
byte-gate removal needs evidence about. What that exposed, delta-debugged to minimal flag
sets over all 281 runnable `tests/exec` programs at `-O1/-O2/-O3/-Os`:

| minimal set | subjects | ships today? |
|---|---|---|
| `-fno-replay-fallback` + `inline=1, inline-functions=0` | `transparent_union`, `union_byval` | masked by the gate — **fixed** |
| `-fno-replay-fallback -fno-chain-store-live` | `chained_assign` | masked by the gate — **fixed** |
| `-fbuiltin-math-errno` | `libm_builtin_fold` | **shipped — fixed** |
| `-fno-reg-disp` + `inline=1, inline-functions=0` | `const_member_copy` | **shipped — fixed** |

All four are closed. The two masked ones were the same class as `union_cast` — real replay
defects the byte compare hid — and they were the last known blockers on flipping the
default. `flagsweep-cover` rows 17, 26, 28, 41 and 42 are **green** now; the earlier claim
that they are red described the tree before `a4d28f03`. Plain `-O0`…`-O3`/`-Os`, with and
without `-fno-replay-fallback`, are clean on all 281 subjects.

~~Before the default is flipped, one backstop is still owed.~~ **Landed in `705f0b0f`.**
`ast_func_end` computes `keep = faithful || (ast_rir_nofb_env && ast_replay_completed)`,
and `ast_replay_completed` was set after the *first* replay and never cleared by the
`posterr` arm that catches a longjmp out of the optimizer emit — so with the fallback off,
a body that aborted mid-emit was kept anyway. That is exactly how the `transparent_union`
defect shipped a body truncated at 52 of 92 bytes instead of falling back. It was
deliberately left open while that defect was live, because adding it then would have turned
the defect green by hiding it. No effect on a default build: with the fallback on, the same
arm already clears `faithful` and `keep` is `faithful` alone.

### Flipping `MCC_OPT_REPLAY_FALLBACK` off by default — the decision, not a task

**No known defect blocks it.** Everything below is measured on `main`, not argued:

- The whole suite is green under `MCC_TEST_OPT="-O2 -fno-replay-fallback"`, and under
  `-O1 … -finline` and `-O3 … -fno-inline-functions`, the two states that reach
  `inline=1, inline-functions=0` and that no single flip reaches at `-O2`.
- The per-body benignity probe (`rir-nofb-probe`, opt-in `-L census`) keeps one divergent
  body at a time and diffs the program: **zero miscompiles** at any level. The banked
  `nofb_miscompiles` list is empty for O0/O1/O2/O3. It used to hold `union_cast::main`.
- The 3-way covering array varies `replay-fallback` across 74 rows over 107 flags and
  finds nothing.

**What the gate costs while it stays on.** `ast_run_strat_seq` gates every one of the 22
strategies on `faithful`, so a byte-divergent body receives *no* optimization at all. At
`-O1` that is 2.0% of bodies but **10.2% of body bytes**, because a discarded body averages
2585 bytes against 470 for a kept one — the gate is withholding the optimizer from the
largest functions in the program. That is a capability cost, not a correctness one.

**What it still buys.** It converts a replay defect into a missed optimization rather than
a wrong program, and it is nearly free (the `orig` buffer is already copied for the restore
path). It cannot be kept *and* the bytes optimized for the same body — those are mutually
exclusive by construction.

The residual risk is not a known bug but the shape of the evidence: a 3-way array cannot
see a 4-flag interaction, and the suite is the only oracle. Recommendation is to keep
computing `faithful` permanently regardless, and — whichever way the default goes — make
the divergence **visible**, which it is not today: `rir_prod_note` only reports at
`MCC_RIR_PROD>=2`, so in a default build a fallback is silent. `union_cast` was wrong for
as long as it existed and nobody knew, because the gate suppressed the symptom. That is
worth fixing under either decision.

`inline=1, inline-functions=0` is worth naming as a state rather than a flag: `inline`
defaults off at `-O1` and on at `-O3`, `inline-functions` off at `-O1` and on at `-O2`, so
that state is reached by `-finline` at `-O1` and by `-fno-inline-functions` at `-O3` and by
**no single flip at `-O2`**. A sweep at one `-O` level calls two of those three green,
which is why `flagsweep.sh` now runs `-O1 -O2 -O3` rather than `-O2` alone.

## Branches deleted 2026-08-06 — all seven were already landed

All non-`main` branches and every agent worktree were removed. An earlier version of this
section listed seven commits as "not on `main` in any form". **That was wrong, and the
method is the lesson:** it used `git rev-list --count main..<branch>`, which counts by SHA
reachability, and a cherry-pick lands the same change under a new SHA. Every one of the
seven has a payload-identical twin already on `main`, confirmed by comparing added and
removed lines with context stripped:

| orphan | already on `main` as |
| --- | --- |
| `a4217c24` gnu89 extern-inline redefinition | `5d52753d` |
| `2bcd21d9` width-64 bitfields in packed contexts | `1b78d132` |
| `b06dcf9d` its TODO update | `5a2f8970` |
| `c3ed8b2a` VLA parameter dimension token ownership | `6cbbbc65` |
| `1e10dd1a` C23 `u8` character constants | `a170a134` |
| `22575f40` `__has_attribute` as a builtin macro | `1df8f3b8` |
| `6a6fe8f2` invalid `##` paste is an error | `97164575` |

`a3c51e8d`, the held `fix-imaginary` branch, is the one case where no single twin exists —
and it is still redundant. `main` carries all three of its hunks across `1d3f2719` (float
`iF`/`iL` order) and `38508d54` (the three early suffix sites, needed for the integer cases
`9iu`/`11il`, plus the `CONST_WANTED` rodata fold). **Applying it now would regress:** its
fold has no `CONST_WANTED` guard and no `gen_cast` on either part, so an integer imaginary
literal would push raw integer bits through a `VT_DOUBLE` `init_putv`, and its third hunk
is a workaround for a cache-aliasing bug `38508d54` deleted outright. The branch is
therefore closed, not pending.

`38508d54` did ship with **zero tests**, and no fixture in the tree used a reversed-order
imaginary suffix, so `a3c51e8d`'s test was salvaged and landed on its own. It is
load-bearing against the code as it sits on `main`: removing the early suffix sites gives
`invalid number` on `9iu`, and disabling the `CONST_WANTED` fold gives `initializer element
is not constant` on a file-scope `1.0i`.

The three commits from this session's research that were genuinely unlanded are now all in:
`ea67df7d` (byte-level RIR coverage census and ratchet), `d2e4c162` (the strategy-registry
sweep) and `934b692e` (the region-granularity research that closed F3). **Neither of the
last two was a straight cherry-pick** — re-measurement contradicted several of their own
claims, so budget that rather than a replay for anything else recovered this way. See
"Strategy-registry sweep" below for what changed.

### Open: long-double complex static initializer

`double _Complex z = 1.0iL;` at file scope dies with `internal error: static initializer
for unknown base type 7`. So do `0x1p4iL`, `1.0Li`, `float _Complex` targets, and the
underlying `double _Complex z = __builtin_complex(0.0L, 1.0L);`. The imaginary suffix is
incidental — the `CONST_WANTED` fold emits a `VT_LDOUBLE`-based rodata complex and the
static-init path has no complex-to-complex narrowing for it. `double`-to-`float` narrowing
works, so it is long-double-specific; function scope is fine at every spelling; gcc accepts
all of them.

## Invariants the code no longer states — all comments were removed 2026-08-06

Every `/* */` and `//` was stripped from `src/`, `include/`, `runtime/`, `tools/` and
`tests/` on 2026-08-06. There were **no** `TODO`/`FIXME`/`XXX` markers among them — the
only such hits were references pointing here — so no open work was lost. What was lost is
a set of prohibitions, and violating four of them caused real bugs during that day's work.
They are recorded here because nothing else records them.

- **An asm node's `sym` is not a `Sym`.** `rir_to_arena` packs `(is_output, out_reg)` into
  it, so `out_reg == -1` reads as `0xffffffff00000000`, and `ival` is an offset into the
  per-function `ir_cap_raw` pool. So an arena carrying `AST_OP_ASM`/`ASMGEN`/`ASMOPS` can
  neither be walked as if every `sym` were a `Sym` pointer, nor outlive the function that
  built it. `mccjit_intent_serialize` walks every node and interns `ast_sym`; that is the
  fourth path by which an arena escapes `ast_func_end`, beside the three retain paths, and
  missing it segfaulted the compiler under `-run` at `-O1`+.
- **`ast_loc_low` is not a usable frame floor for the inline graft.** It drifts down as
  each graft runs and never covers the graft return slot. Use `ast_graft_base`, set once
  per emit to the parser's frame bottom. Using `ast_loc_low` broke sibling reuse and put
  graft #2's parameter on graft #1's result (`exec/struct_packed_indirect`).
- **During replay `ast_alloc_loc` *assigns* `loc` from the recorded list rather than
  decrementing it**, so `loc` can come back shallower than the caller's own declared
  locals. The parser's frame bottom is `saved_loc`, not the last replayed temp.
- **A record/replay side channel must be skipped, not consumed, on a key mismatch.** There
  are four (`rir_locrec`, `rir_slotrec`, `rir_tvrec`, `rir_fcrec`), each a body-global
  array with one monotonic cursor keyed by absolute code offset. A consumer that takes an
  entry another will ask for desynchronizes every later lookup. `rir_hook_fconst_reuse`
  matched only on kind and not on value, and handed a `0.5` multiply the label belonging
  to `1.0f`.
- **A body that longjmp'd out of the emit must never be kept** — see the `posterr` item
  under the C2 gap section.
- **Syms allocated under the debug allocator belong to the slab and must not be freed.**
- **`-fno-asm` stops the `asm` keyword being recognised, as in gcc. It does not stop
  inline assembly reaching the backend by other spellings.**
- **Building an inner mcc for the JIT self-host wants a small `-DMCC_JIT_TLS_MAX`** — the
  inner compiler never hosts the slab.

## Present-tense open items — validated 2026-08-05 at `9b83dc05`

Only open, actionable work is listed. Directly-relevant "do not" caveats are kept with
their task; the broader landmine set is in [`ARCHIVED.md`](ARCHIVED.md).

### Windows / macOS host items
- **W2** — `arm64-win32`, `arm-win32`, `arm-wince` execution. Compiled and byte-compared
  only; needs Windows-on-ARM hardware (wine runs x86 PE only; qemu-user cannot load PE).
  `arm-win32` and `arm-wince` must read identically on every counter — a differential
  where they disagree is a harness bug, not a codegen one.
- **W5** — mcc cannot self-host on Windows arm64: stage1 takes `0xC0000005` on
  `lib/atomic.c`, `lib/alloca.S`, `lib/alloca-bt.S`, `lib/builtin.c` (host ABI: varargs,
  alloca, stack probe). Needs Windows-on-ARM hardware.
- **W8** — fix the `selfhost-jit` heap corruption. Root-caused (2026-08-05) to a
  heap-use-after-free of a `Sym` in the AST forward-inline re-emit path: the sym is
  retired by `ast_func_end` and then read by `ast_reemit_forward_inlines` →
  `ast_inline_graft` → `ast_replay_value` → `gaddrof`. `ast_reemit_retain` /
  `ast_inline_retain` under-count cross-function graft references, so a retained
  forward-inline body's syms get dropped. Release builds recycle via `sym_free_first`
  (hence the nondeterministic `0xC0000374`/`0xC0000005`); the MSVC-ASan `mcc_s` binary
  makes it deterministic — `mcc_s` + `tools/selfhost-jit.py` is the verification
  harness. The fix needs the full suite as a regression gate, not just the ASan oracle.
- ~~**W3 residual** — the 3-way-concurrent stress re-run.~~ **Closed 2026-08-05 on a
  macOS arm64 host, with a negative control.** 399 chains at the fix, 0 non-identical,
  over default `-O`, `-O1`, `-Os`, `-O3`, the 12-flag gates set and
  `memmodel-{O2,O3,Os}`, at concurrency 3, 6 and 10. A zero alone would prove nothing if
  the stress never reached the condition here, so it was paired with a twin built from
  `git archive HEAD` with `5aa66d88`'s `src/mccast.c` hunk reverse-applied — only the
  `ast_divmagic_invalidate` hook differing. **The defect reproduces on macOS arm64**:
  5 non-identical in 150 chains, 2 of them in 30 at exactly the Windows-matching cell,
  strictly bimodal at a constant −336, never serial. Rule of three bounds the fixed side
  at 0.75% (1 in 133); Fisher exact one-sided p = 0.0015. The claim is that the original
  failure mode at its actual rate is gone, not that the compiler is proven deterministic.
- **One-off, unexplained**: `selfhost-fixpoint-memmodel-{O3,Os}` SIGSEGV'd once in stage2
  (mcc1 compiling `src/mcc.c`) during a `macos-cross` run, mid-compile at ~2.5 s against
  a normal ~5.5 s. It survived 38 clean re-runs (4/4, then until-fail:10 over both cells,
  then a 28-minute load-reproduction at `-j8`). Two unconfirmed causes: memory/CPU
  pressure, or a mid-edit source tree — `src/mccgen.c`, which `src/mcc.c` amalgamates,
  was being written while that suite ran, and the re-runs all recompile from the tree and
  pass, which favours the second. **Durable lesson: separate build directories are not
  isolation, because `selfhost-*` and `mcctest` compile the live `src/` tree.** If it
  recurs on an idle machine with a clean tree it is real and wants a core dump.

### Closed 2026-08-05
- **Subscripting a pointer to function was accepted silently.** `f[0]` and `0[f]` are a
  constraint violation at every pedantry level; mcc emitted nothing at all, so
  `gcc.dg/pointer-arith-{1,2,3}.c` all compiled clean where gcc and clang reject. The
  check goes in the subscript path after `gen_op('+')` — which is where `f[0]` and
  `0[f]` converge — and not in `indir()`, because dereferencing a function pointer is
  legal (`(*f)()` is just `f()`) and `indir()` returning early on `VT_FUNC` is correct.
  An array *of* function pointers is unaffected: `a[0]` yields `VT_PTR`, not `VT_FUNC`.
  Regression cell `diag.dg-error.function_pointer_subscript` covers both the rejection
  and the legal neighbours. This closes 3 of the 288 missing-diagnostic files; the
  `void *` arithmetic *warnings* those same tests also expect are still not emitted.
- **`__builtin_powi` / `powif` / `powil` did not exist.** Inline definitions in
  `mccdefs.h`, since the generic `__builtin_` → libm alias path cannot express them:
  it types every argument alike and forwards to the same-named libm symbol, but powi
  takes an `int` exponent and libm has no `powi` — gcc lowers to libgcc `__powidf2`.
- **`__builtin_expect` discarded its second operand's side effects.** Both it and
  `__builtin_expect_with_probability` parsed the operand under `nocode_wanted++`, so
  `__builtin_expect(c, z++)` emitted no increment: `gcc.c-torture/execute/pr85156.c`
  returned 10 where 11 is required. Silent wrong code with no diagnostic — the second
  operand is an ordinary argument that gcc and clang both evaluate, and only the
  *probability* operand of the `_with_probability` form has to be a constant, so that
  one keeps its `nocode_wanted` guard. Found by the three-compiler board below.
  Full suite after the change: **8576 cells, the 4 documented reds, nothing new** —
  `run-tier/{x86_64,i386}-win32` (`tls`, `tls_threads`) and
  `selfhost-qemu-{arm,i386}-O2` (`MCC_MAX_ALIGN`), all four with their recorded causes.
- **`stage2 / macos / macos-arm64-clang / pe` was red in CI**, and would have been on any
  host with wine. `stage3 --consume test` runs `ctest` with **no label filter**, so the
  `wine`-labelled `run-tier/x86_64-win32` and `run-tier/i386-win32` cells run — and both
  are documented deliberate reds (`tls` and `tls_threads`, the open `-run` TLS defect).
  `tools/run-tier.sh` had no expected-failure mechanism: any bad program failed the whole
  cell, so a known-red cell could only ever be a red CI job. Adding the macOS `pe` gate
  cell is what surfaced it.

  Fixed with a `KNOWN_RED` list in `tools/run-tier.sh` — `<triple>:<program>` pairs, at
  present the four `{x86_64-win32, i386-win32} × {tls, tls_threads}`. A listed program
  that fails reports `XFAIL` and does not fail the cell; **anything unlisted still fails
  exactly as before**, which is this file's own rule that any further failure is a
  regression. The list is self-cleaning: a listed program that starts *passing* fails the
  cell with *"N KNOWN_RED program(s) now pass — drop them from KNOWN_RED"*, so it cannot
  rot into silent coverage loss. Verified both directions. The PE cells now read
  `12/14 OK under both JIT tiers, 2 known-red`.

  **Delete the four entries when the `-run` TLS defect is fixed** — the cell will tell
  you to. **Done:** the defect is fixed (next item) and `KNOWN_RED` is now empty; both
  PE cells read `14/14 programs OK under both JIT tiers`.
- **The PE `-run` TLS defect is fixed** — `{x86_64-win32, i386-win32} × {tls, tls_threads}`
  all pass under both JIT tiers. Windows has no loader step during `-run` to lay out a
  guest's implicit TLS, so the guest read whatever `gs:[0x58][_tls_index]` happened to
  hold and every `__thread` with a non-zero initializer came back 0. It now borrows mcc's
  own implicit-TLS block, mirroring the Linux slab model: `tls_setup_pe` (mccrun.c) flags
  the run and records the slab's bias inside mcc's block via `host_run_tls_slab_tpoff`
  (mcchost.c reads it off the live TEB), the x86 `TPOFF32` relocations fold every guest
  TLS offset into `mcc_jit_tls_slab`, and `tls_seed_pe` fills the slab from the guest
  template and repoints the guest's `_tls_index` at mcc's own. Threads the guest starts
  get a fresh zeroed block, so their `_beginthreadex` import is bound to a wrapper
  (`mcc_run_beginthreadex`) that reseeds the slab on entry. All Windows-only, no macros
  gating it behind the POSIX paths. Native x86_64 Linux `-run` tier still `14/14`.
- **`__has_builtin` lied about the fourteen `__builtin___*_chk` builtins.**
  `pp_has_builtin_arg` answered true only for predefined builtin tokens or `#define`d
  macros, but `runtime/include/mccdefs.h` supplies the `_chk` family as `static __inline`
  *functions*, so `__has_builtin(__builtin___sprintf_chk)` reported **0**. They are now
  in that function's `untokenized[]` list, beside the `va_*` entries already there for
  the same reason.

  This was deferred once because `__has_builtin(__builtin___*_chk)` is what gates the
  FORTIFY blocks in libc headers, so answering true could reroute every
  `str*`/`mem*`/`*printf` call through the `_chk` inlines. **Measured, that does not
  happen on Darwin**: Apple additionally gates those blocks on `_USE_FORTIFY_LEVEL > 0`,
  which mcc does not satisfy, so no call site is rewritten and the objects are
  **byte-identical** before and after on a `sprintf`+`memcpy`+`strcpy` program. Full
  suite after the change: **8498/8498, exit 0.** The correctness fix lands; the fortify
  question is untouched and stays open for a libc that gates only on `__has_builtin`.

### Flag-sweep coverage
- ~~Wire `tests/optfire/flagsweep.sh`'s exec half as ctest cells.~~ **Closed** — 113
  `flagsweep-exec/<flag>` cells, one per `MCC_OPT_ROW`, plus a 74-row 3-way covering
  array behind `-DMCC_FLAGSWEEP_FULL=ON`. See "Coverage, as wired" above.
- ~~Unpin `replay-fallback` from `tests/optfire/cover3.py`.~~ **Closed** — the array
  varies it and the five bisection-only knobs are pinned in its place. Delete the
  `jit-splice` pin and the one `KNOWN_RED` entry when that pass is fixed.
- ~~Root-cause the two masked-by-the-gate replay defects.~~ **Closed in `a4d28f03`.**
  `transparent_union`/`union_byval`: `ast_inline_graft` bound by-value parameters with a
  plain `vstore()`, whose assignment conversion does not exist for a `transparent_union`
  parameter — the parser never performs it either, since `gfunc_param_typed` consults
  `transparent_union_member` and casts to the *member* type. The graft raised
  `cannot convert 'struct A *' to 'union <anonymous>'` and longjmp'd out of the
  post-optimization emit. Fixing the cast then exposed a frame overlap underneath it,
  unreachable while the error aborted the graft first: the graft takes its frame base from
  `loc`, but during replay `ast_alloc_loc` *assigns* `loc` from the recorded list rather
  than decrementing, so `loc` can come back shallower than the caller's own locals. Hence
  `ast_graft_base`, set once per emit to the parser's frame bottom. `ast_loc_low` is not a
  usable floor — it drifts down per graft and broke `exec/struct_packed_indirect`.
  `chained_assign`: `rir_hook_fconst_reuse` matched only on the complex/scalar kind and
  never on the value, unlike its ast-side twin `ast_fconst_reuse`, so a drifted cursor
  handed the `0.5` multiply the label belonging to `1.0f`.
- **The per-flag cells cannot see a two-flag bug and the array cannot see a four-flag
  one.** `flagsweep-exec/inline` was green on the `const_member_copy` miscompile because
  that one needs `-fno-reg-disp` too; only `flagsweep-cover` found it. When a fix lands
  for a multi-flag defect, add a subject that makes *one* of its flags sufficient —
  `structs_unions/inline_sret_locrec` is that subject for the `ast_locrec` desync, and it
  is red under `flagsweep-exec/inline` at `-O1` and `flagsweep-exec/inline-functions` at
  `-O3` if the fix is reverted.
- The corpus has a hole worth closing: an injected `x + 1 → x` fold in the ident-arith
  pass was invisible to all twelve goldens when gated on a `LEVEL(4)` flag until it was
  moved from `-` to `+`. `x - 1` in a returned expression is not exercised.
- The array pins two flags, so it is a 3-wise guarantee over **111** of the 113, and a
  bug needing four specific flags is out of reach by construction. Raising strength to 4
  is ~4× the rows; it has not been measured.

### Strategy-registry sweep — `tests/optfire/stratsweep.sh`

The `-f` surface above is one axis; the *order* of `ast_strategies[]` is another, and
`MCC_AST_STRAT_ORDER` is what makes it testable from outside the compiler. The sweep asks
the two questions nothing else does: does a row hold up when it is the only row that runs,
and do two rows commute. Both green on this tree at every depth measured.

- The registry has **22** rows and all 22 are covered. An earlier cut of this sweep tested
  rows 4..21 only, on the theory that `bfold`/`ident`/`narrow`/`cprop` are always-on
  folders not worth isolating; they isolate fine — each is green alone over the whole
  admitted corpus — so "always on" was never a reason to leave a quarter of the table
  unmeasured. The triple space is therefore 22·21·20 = **9240**, not 18·17·16 = 4896.
- `stratsweep/check` is the thing that keeps the sweep honest: `STRAT_NAMES` must equal
  `ast_strategies[]` name for name and in order, and `STRAT_NONE` must still be an empty
  slot below `AST_STRAT_COUNT_MAX`. A row added to the registry and not to the sweep is a
  row the sweep silently skips while still printing PASS, so both are hard failures.
- **Admission is the load-bearing part.** A `tests/exec` program is used only if its `-O0`
  build runs, two `-O0` runs agree, *and* `-O2` with the whole registry disabled
  (`MCC_AST_STRAT_ORDER=23`) still agrees with `-O0`. Without the third condition every
  `-O2` defect outside the registry, and every program that prints a stack address, lands
  on whichever strategy happens to be under test. 282 of 300 admitted; the 18 drops are
  all at the `-O0` build step (they want `-trigraphs`, a `-std=` gate, an arm64 target, or
  a second TU). `-O2` minus the whole registry matching `-O0` on all 282 is itself a
  result.
- **Two tiers, split by corpus and by how much of the triple space is walked — not by
  which rows are looked at.** Default: all 22 rows in isolation plus six 1/1024
  permutation shards over the 31-program subject list, 29 cells, 49–145 sec\*proc
  depending on machine load, no cell over 20s. Opt-in (`-DMCC_STRATSWEEP_FULL=ON`, label `stratsweep-full`): the
  same 22 rows over the whole admitted corpus plus all 9240 triples bare and all 9240
  with rows 0..3 in front — 86 cells, 6440 sec\*proc, 7 min at `-j16`, **all green
  2026-08-06**, and the shards partition the space exactly (24 of 289 + 8 of 288 = 9240).
  A full-corpus isolation cell cannot be in the default gate —
  admission alone builds 282 programs twice, so none of them finishes under a minute.
  The shard divisor is the knob that holds the default tier down: these cells are
  registered last and are the tail of the whole `ctest` run, so the slowest one sets the
  suite's wall time. At 1/512 they were 31s each.
- **Subjects run with cwd inside the cell's own workdir.** `tests/exec/programs/stdio.c`
  writes and re-reads `fred.txt` in the current directory, so any two cells running it
  from a shared cwd race. Measured: of four full-corpus isolation runs started together
  from one directory, one dropped `programs/stdio` as nondeterministic and three admitted
  it; with per-cell cwd, four of four admit all 282. This is the likeliest explanation
  for the one unreproducible red an earlier cut of this harness produced, which landed on
  `iso-bf` for no reason connected to the bitflag pass. `tests/runner.c` already does the
  same thing — any new harness that runs `tests/exec` programs must too.
- The subject list is the flagsweep twelve **plus the strategy-named optimizer goldens**,
  and the second block is not decoration. A planted `ast_cse_kill` bug (deleting the
  read-invalidation half, so CSE reuses a cached expression across a store to one of its
  operands) is completely invisible to the twelve; it is caught by `optimizer/cse` and
  `types/int_conversion`. Same lesson as `flagsweep.sh`'s `-fjit-splice` subject, arriving
  a second time. Re-run 2026-08-06 against the shipped harness: `iso-cse` and all four
  `perm3-*` go red naming exactly those two subjects, while the other 21 isolation cells
  stay green — the signal is specific, not blanket.
- **Prefixing rows 0..3 masks bugs, so the bare mode is the sensitive one.** Under the
  planted bug both `perm3-based-*` stay green while all four `perm3-*` go red. Both modes
  are registered because they are not the same test. Keep it that way.
- Subject binaries run pinned to one CPU. `atomic_counter` is 16 threads on one cache
  line: 2.0s unpinned, 0.05s pinned, and unpinned it alone was most of a pass. Every
  atomic instruction still executes and `tests/exec` still runs these goldens unpinned;
  what is given up is true simultaneity.
- A mismatch is re-run four times before it is called a failure, and a non-recurring one
  is printed on stderr and counted in the PASS line rather than swallowed. That is for
  fork/exec losing under load, not for miscompiles — any recurrence is a hard failure.
- Not covered: one host triple (x86_64 Linux), `-O2` only, and triples — not 4-deep, and
  not the full 22-row order.

### C2 gap — remaining Replay_IR fidelity work

#### Byte-level coverage board, both RIR layers (2026-08-06, x86_64 Linux, gcc-built mcc)

Body counts overweight tiny functions: at `-O1` on `src/mcc.c` the arena falls back on
**2.0% of bodies** (52 of 2581) but **10.2% of body bytes**, because the average
fallback body is 2585 B and the average kept body is 470 B — 5.5x. Bytes are the honest
denominator and the census counts them: `tools/rir-coverage.py`, gated by
`ctest -R rir-coverage`.

**RIR is two layers and only the second has a gap.** The op-stream capture
(`rir_verify` under `MCC_REPLAY_IR=3`) is the 100% baseline; the arena
(`rir_to_arena` → `rir_prod_take` → `ast_replay_body`) is what production ships and
what loses bodies. Everything below is the arena unless it says capture.

The split that matters is semantic, not byte-exact:

| bucket | meaning |
| --- | --- |
| **kept** | modelled, replayed, byte-identical to the parser — shipped from the arena |
| **discarded** | modelled, replay ran to completion, the memcmp against the parser's second derivation disagreed (`len`/`bytes`/`rellen`/`relcontent`/`posterr`) so the parser's bytes were restored. Coverage exists; the byte gate threw it away |
| **gap** | never modelled (`skip:*`) or modelled but the replay did not complete (`abort`). The only true coverage gap |

`src/mcc.c` compiled by the built `mcc`, `-O0` forced with `MCC_FORCE_REPLAY=1`,
percentages of **body bytes** (`.text` minus prologue/epilogue):

| level | capture | arena modelled | of which kept | discarded | **gap** |
| --- | --- | --- | --- | --- | --- |
| `-O0` (forced) | **100.000%** | **100.000%** | 81.676% | 18.324% | **0.000%** (0 B, 0 bodies) |
| `-O1` | **100.000%** | **100.000%** | 95.981% | 4.019% | **0.000%** (0 B, 0 bodies) |
| `-O2` | **100.000%** | 99.965% | 95.951% | 4.014% | **0.035%** (467 B, 1 body) |
| `-O3` | **100.000%** | 99.965% | 95.951% | 4.014% | **0.035%** (467 B, 1 body) |

Prologues and epilogues are modelled by neither layer, by construction, and are now by far
the largest non-covered slice of `.text` — 5.5%, two orders of magnitude bigger than
the gap.

The wide corpus (`src/mcc.c` + every `tests/{exec,behavior,ast,asm,runtime,static}/**.c`
+ `examples`, 363 files, 9 of which are negative tests that do not compile) agrees:
modelled 100.000% / 100.000% / 99.986% / 99.986%, gap 0 B at `-O0`/`-O1` and the same
467 B at `-O2`/`-O3` — **the same body as the self corpus**, so what remains lives in
`src/`.

#### The gap, enumerated — three classes, each with a minimal reproducer

`tests/rir/gap/<class>.c` is one file per class and `ctest -R rir-gap-classes` compiles
each at all four levels and fails if a class stops reproducing. A fixture whose class is
only reachable under an optimization that does not run at every level declares that with a
`/* rir-gap-levels: O2,O3 */` comment on the first line.

| class | what RIR cannot model | reproducer | bytes in the gap |
| --- | --- | --- | --- |
| `abort` | the replay **re-runs the emitter's own invariants** and the arena does not carry enough to satisfy them. `MCC_RIR_ABORTWHY=1` prints the message `ast_error_sink` swallows. One body left, `host_runmem_alloc`: `ast-replay: storeval-arg stack underflow`, an `AST_FB_CALL_STOREVAL_ARG` invoke whose vstack rotation finds fewer live entries than it needs | `abort.c` (`-O2`,`-O3` only) | 467 B / 1 body at `-O2`,`-O3`; **0 B** at `-O0`,`-O1` |
| `skip:noops` | a body with no ops at all (an empty function) | `noops.c` | 0 B by definition |
| `skip:replayok` | the arena was built and taken, but `ast_replay_ok()` refuses it at `ast_func_end` — previously invisible, it fell out of the census entirely until `rir_prod_why_set("replayok")` was added | `replayok.c` | 0 B on both corpora |

`capbad`, `unbal`, `ovf`, `invalid`, `unsafe` and `revargs` are **never hit** on either
corpus (`revargs` needs `-freverse-funcargs`, off by default). `asm`, `regdangle`,
`bail` and `mismatch` were live classes with reproducers when this board was first
drafted; **all four have since been closed** — their reproducers now report `used` at
every level and the files are gone.

**So the arena gap is now one body, 467 B, and only from `-O2`.** Closing it means
recording how many vstack entries the store-value rotation expects, the same shape of fix
as `RIR_M_CASTT`: a missing fact at the capture site, not a divergence at the use site.

#### Do not read `discarded` as either a defect or as covered

`ast_run_strat_seq` gates **every** optimizer strategy on `faithful`
(`if (faithful && ast_strategies[si].gate())`), so a discarded body receives zero
optimization — the discarded bucket is a *capability* cost, not just bookkeeping. To
tell whether it is also a correctness cost, `tools/rir-coverage.py --nofb-probe` keeps
one divergent body at a time (`-fno-replay-fallback` plus `MCC_RIR_NOFB_SKIP` for its
siblings) and diffs the program against the shipped compiler's output. On `tests/exec`:
9 divergent bodies at `-O0` and 2 at `-O1`/`-O2`/`-O3`, **all benign, no miscompiles**.
The bank therefore holds an *empty* miscompile set per level and the cell fails on the
first new one. (`structs_unions/union_cast.c::main` was a real miscompile when this was
first measured; it no longer reproduces.)

#### The capture layer measures 100.000% on both corpora

`MCC_REPLAY_IR=3` over `src/mcc.c` and over the wide corpus: `fallback` 0, `rerror` 0,
**100.000% of body bytes faithful at all four levels**. So the claim "RIR covers all
machine code generation" is *true of the op stream* and false only of the arena.

- **The `IR_OP_RAW` escape hatch does not explain the arena's gap.** RAW is the
  capture's memcpy-the-bytes fallback and the arena has no equivalent, so it was the
  obvious suspect. Measured: **0 bodies** use it in `src/mcc.c` or anywhere in the wide
  corpus, and **0.00% of the arena's lost bytes** are in a body that used it.
- **RIR does not run at all under `-g` or on `extern inline` bodies**
  (`rir_started = (rir_env || rir_prod_env) && !debug_modes && !cur_func_inline_extern`).
  On the wide corpus that is 43 functions no layer ever sees. Reported as `unnoted` and
  never counted as covered.

#### What reconciles

The harness checks its own arithmetic: `.text == Σ per-function bytes + Σ re-emitted
bytes + residual`, and `Σ body bytes == used + fallback + skip`. Both hold exactly
(residual **0**, delta **0**) at all four levels on the self corpus, and the census's
own invariant `Σ[rir-prod-unfaithful] == fallback` holds on every row.

On the wide corpus the residual is **−119 bytes over 3.5 MB (0.003%)** in both layers,
and every byte of it is named, per file: `+1/+6/+14/+14` in `asm_outside_function.c`,
`al_ax_extend.c` and `asm_c_connect/part{1,2}.c` — file-scope `asm` emits into `.text`
outside any function, so no body owns those bytes; `−78/−38/−38` in `inline.c` and
`c99inline_{a,b}.c` — `extern inline` bodies are generated (and counted by the
per-function hook) and then not placed in `.text` at all. Reported, not hidden.

**It only reconciles because `-O3` re-emission is accounted for, and that uncovered a
real defect.** At `-O3` — and only at `-O3`, because `MCC_OPT_INLINE` is
`optimize >= 3` — `ast_reemit_forward_inlines()` re-emits 27 functions / 52,022 B at the
end of `src/mcc.c` so they can inline a callee defined later. The *first* emission is
left in `.text` with no symbol pointing at it. Not a correctness bug — nothing branches
there — but `-O3` ships ~3.6% more `.text` than it needs to, and any per-body byte
accounting that does not know about re-emission silently attributes the pre-inline copy.

#### The ratchet

**Re-banked 2026-08-06 for the width-ladder oracle.** `src/ast_eval_slice.h` and the
census block in `src/mccast.c` add ~700 lines of new compiler source, so every `lowerable`
percentage is measured over a different denominator. `bodies_pct` fell 9.1328% -> 9.0189%
(`-O0`) because the added bodies are `fprintf`/`snprintf` reporting code, which `call` and
`global` block; in the same move `nodes_pct` rose 41.6543% -> 41.8098% and
`region_nodes_pct` 16.7579% -> 16.8908%, because the ladder's arithmetic *is* lowerable at
region granularity. Re-banked with `tools/rir-coverage.py <bdir> --update-bank-low`. No
lowering capability regressed; only the mix of the compiler's own source changed.


`tests/rir/coverage-bank.json` banks, per level and per layer, the **modelled**
percentage — kept + discarded, i.e. everything that is *not* the gap. `ctest -R
rir-coverage` fails if it regresses by more than 0.05 pp, or if the *magnitude* of the
byte-accounting residual grows — a residual that moves toward 0 is better accounting
and must not fail the gate. **Deliberately not banked: byte faithfulness.** A ratchet on `kept`
would lock in the wrong invariant and fight every future improvement that makes the
optimizer *change* bytes. Banked at HEAD on the self corpus: modelled **99.59%** at
`-O0`/`-O1` and **99.56%** at `-O2`/`-O3`, capture **100.00%** at every level,
residual **0**.

`ctest -R rir-gap-classes` is the other half: it fails when a gap class stops
reproducing, so closing one is a deliberate act with a bank update rather than a silent
no-op. Four classes were closed that way between the first draft of this board and
this one.

**Bank classes and percentages, never totals.** The body *total* of a census taken over
a whole `ctest` run is not stable at fixed HEAD (two consecutive runs gave `used=95823`
and `used=95971` with every class count byte-identical), so nothing derived from a
varying denominator is banked. The fixed-file-list censuses used here *are*
deterministic — two consecutive wide runs were byte-identical.

Default cost is `rir-coverage` 4.8 s + `rir-gap-classes` 0.05 s on an idle box. The
wide-corpus census and the per-body benignity probe are opt-in:
`MCC_RIR_CENSUS=1 ctest -L census` (`rir-coverage-census` 17 s, `rir-nofb-probe` 3.6 s);
they skip with 77 otherwise so the default suite stays fast.

- Close the open per-body byte divergences (the "largest first" list in the archive).
  **Fix at the USE site, never the CAPTURE site.**
- `full_language.c` still diverges at `-O0` on x86_64/i386 — an `AST_OP_ASM` replay
  defect (P4 defect 4), contained not closed.
- **Do not turn `-fno-replay-fallback` (`MCC_RIR_NOFB`) on by default until the
  byte-faithful step is green.** `keep = faithful || (nofb && replay_completed)` is
  load-bearing — ≥4 of the fallback bodies are genuinely wrong, not benign.
- `-fno-replay-fallback` + selfhost-jit-with-mcc blows RSS ~6.7×; bisect via
  `MCC_RIR_NOFB_SKIP`. Measure on the full suite, not the 317 exec goldens.
- **`exec/union_cast` is closed** — it was the sole consistent native failure under
  `-fno-replay-fallback` at every `-O` level, and the whole remaining correctness
  justification for the byte gate. `rir_stmt` appended a statement straight into the
  basic block whenever `rir_hold_inline` declined to hold it, even with an inline-held
  `Invoke` still queued from earlier in the op stream, so the held call was re-emitted
  behind it by the next `rir_ihold_bind`. `take((union U) 13)` is the two-line
  reproducer: `gen_union_cast` lays the temp down as `memset(tmp,0,n)` then
  `tmp.i = 13`, the `memset` is held because `printf`'s callee is already on the
  shadow stack, and the store lands at a shallower shadow depth than the hold, so the
  arena read `tmp.i = 13; memset(tmp,0,n)` and the callee saw 0. Whole suite under
  `MCC_TEST_OPT="-O2 -fno-replay-fallback"`: zero replay failures. The body is now
  byte-faithful too, so it stops falling back and reaches the 22 optimizer passes.
- `run-tier/x86_64` fails `tls_threads` whenever `MCC_JIT=1` meets an active AST
  replay (`MCC_FORCE_REPLAY=1` at `-O0`, or plain `-O1`/`-O2`); `MCC_JIT=0` is green
  at every level and `MCC_RIR_PROD=0` does not help, so this is **not** an arena
  defect and not a `-fno-replay-fallback` one. The child thread reads every
  non-zero-initialised `__thread` as 0: the `-run` TLS slab is a `__thread` array
  inside mcc (`mcc_jit_tls_slab`, src/mcchost.c:1450) that glibc zeroes per thread,
  and the re-seeding wrapper is installed by binding the program's `pthread_create`
  to `mcc_run_pthread_create` (src/objfmt/mccelf.c:974) under `s1->run_tls_active`,
  which `tls_setup_linux` (src/mccrun.c:451) only sets on the interpreter relocate
  path. `--no-jit` does not suppress it — `MCC_JIT=1` still wins.

#### The refusal census bottoms out at 359, and 338 of those are `{ }` (2026-08-06)

`MCC_RIR_PROD=2` + `MCC_RIR_PROD_OUT` over a full `ctest -j32` at ambient
`MCC_TEST_OPT`, `\r` stripped. Before this day's work the classes were
`noops`=338, `mismatch`=42, `revargs`=21. After: `noops`=338, `revargs`=21,
**`mismatch`=0**. Gate: `ctest -j32` green and the whole suite green under
`MCC_TEST_OPT="-O2 -fno-replay-fallback"`.

- **`mismatch` was two sites, both only reachable from the self-hosting cells.**
  A previous instrumentation pass over `src/` + `tests/` at `-O1`/`-O2`/`-O3`
  found nothing because the trigger is `src/mcc.c` compiled *amalgamated* by a
  built mcc, which is what `tools/selfhost-smoke.py` does and no other cell does.
  Reproduce by hand: take the `-D`/`-I` flags out of
  `cmake-release/compile_commands.json` for `mcc.c` and run
  `mcc <flags> -O2 -c src/mcc.c`. Both sites are fixed; see the two commits.
  One was a fixed `AstLocal args[32]` in the arena's `IR_OP_CALL` (mcc's own
  `rir_report` calls `fprintf` with 33 arguments); the other was region marks
  recorded under `DATA_ONLY_WANTED` — a function-scope
  `static const … off[] = { offsetof(…), … }` — surviving into the arena when
  the ops from the same window are already dropped by `rir_op_effect`.
- **All 338 `noops` are provably empty.** Instrumented `rir_prod_take`'s `!nops`
  arm to log `ind - rir_body_ind_sv`, `rir_n` and `ir_cap_n` over a full suite:
  338 rows, every one `len=0 rirn=0 capn=0`. 37 distinct names, all `{ }` bodies
  (`void_foo` in `exec/features_c99_c11/generic.c`, `func_ull_ull` in
  `exec/bounds/stack_safe.c`, the whole `inline`/`inline2` linkage matrix, …).
  There is no arena to build and no bytes to lose, so counting them in the
  coverage gap overstates it by 94% of the gap. `rir_prod_report` now also prints
  a **derived** `[rir-prod-cov] nonempty= modelled= refused= empty=` line;
  `used`/`fallback`/`skip` and every `[rir-prod-why]` count are untouched, so
  every board above stays comparable. Today's suite reads
  `nonempty=98727 modelled=98706 refused=21 empty=338` against
  `used=95971 fallback=2735 skip=359`. Note the body **total** moves a little
  between runs at fixed HEAD — two consecutive censuses gave `used`=95823 and
  95971 — while `fallback`, `skip` and every `[rir-prod-why]` count were
  identical. Compare the class counts across runs, not the totals.
- **`revargs` (21) is one body, and it is not worth closing.** Measured by
  lifting the refusal behind an env probe: under `-O2 -fno-replay-fallback` the
  *only* thing the whole suite notices is `exec/errors_and_warnings` printing
  `122333` where the golden says `333221` — and the ` 1 2 3` beside it is already
  right, so the Invoke's children, types and values all survive; the gap is
  purely evaluation order. The bit is cheap (`fbits` is a `uint64_t`, bit 22 is
  the highest one taken, and the arena is never serialized — there is **no**
  schema to revise, contrary to the older note). The emitter is not: `AST_Invoke`
  already carries the sret pre-alloc, the storeval-arg rotation, the
  indirect-call type fixup and the noreturn `CODE_OFF`, and a reverse child order
  multiplies the states each of those must be right in, while every pass that
  reads an Invoke's children as evaluation order (inline grafting, the call
  window, the argument analyses) becomes conditionally correct. Against that: one
  distinct body in the entire corpus (the 21 rows are that one `main` recompiled
  by 21 cells), a refusal that is safe by construction, a flag that is off by
  default, and a single golden line as the only coverage the new path would get.
  If it is ever picked up, the cheap half is narrowing the refusal from
  whole-translation-unit to bodies that actually contain a call — it does not
  help this corpus, where the one body does contain calls.

#### The fallback census was unreadable until 2026-08-06 — six defects

All six are fixed; every fallback board taken before this date is suspect, and the
`[rir-prod-why]` column of one is worthless by construction. The board below is the
first that **reconciles** — `sum([rir-prod-unfaithful]) == fallback` at all four levels.
Keep that identity as the check on any future census: it was 48-vs-49 at `-O2` before
the last two fixes, and that one missing body turned out to be a real defect class.

- **`[rir-prod-why]` could never print a line.** `rir_prod_why_n[]` was incremented
  only on the `fallback` verdict, but `rir_prod_take()` sets `rir_prod_why = ""` on
  entry and every path that reaches a `used`/`fallback` verdict returned non-NULL — so
  the histogram was fed exclusively with the empty string and `rir_prod_report` printed
  nothing, every run. The codes it names (`asm`, `regdangle`, `mismatch`, …) only ever
  live on the `nomodel` → `skip` path, which was not counted. This is the mechanical
  reason behind the archive's "`rir_prod_why` is empty in every fallback observed" —
  that was an artefact of where the counter sat, not a fact about the bodies. Now the
  skip path feeds the why-histogram and fallbacks feed a new
  `[rir-prod-unfaithful]` one.
- **`rir_unfaithful_why` was never reset per body.** It is assigned in exactly one
  place, at the `faithful` compare in `ast_func_end`. A body whose replay longjmp'd out
  before reaching that compare inherited the *previous* body's reason, so an aborted
  replay was silently attributed to `len` or `bytes`.
- **A global could not carry the reason at all — `ast_func_end` nests.** Resetting the
  global per body is not enough: nested bodies run their own compare and overwrite it,
  and at `-O2`/`-O3` the JIT-dispatch path runs *between* the outer body's compare and
  its census note. The reason is now a per-body `volatile` local (`ast_unf_why`) that is
  published to the global only in the statement before `rir_prod_note`, so nothing
  between the two can clobber it. It is `volatile` for the same reason `faithful` is —
  it is written inside a `setjmp` region.
- **A byte-faithful replay could still fall back, with no bucket for it.** The last
  unreconciled body at `-O2`/`-O3` (`host_runmem_alloc`) completes its replay *and
  passes the byte compare*, then a later stage inside the same protected region raises
  an error; the `else` arm clears `faithful` and the body is restored. `abort` is the
  wrong name for it — the replay was fine, the **post-replay** work failed — so it now
  has its own bucket, `posterr`. Distinguishing the two matters: `abort` is a
  replay-fidelity defect, `posterr` is not, and lumping them hides both.
- **`MCC_RIR_PROD_OUT` dropped the reason column.** The file sink wrote four fields
  ending in `rir_prod_why`; only the stderr sink wrote `rir_unfaithful_why`. Since
  `rir_prod_why` is always empty for a fallback (above), every fallback in a
  `MCC_RIR_PROD_OUT` census carried *no* attribution at all — and that file is the only
  sink that survives a full-suite run. Both sinks now emit the same five columns.
- **The stderr line glued the two reasons together** (`"%s%s"`), so a non-empty pair
  printed as `bailbytes`. They are separate tab-delimited columns now.

#### Full-suite board with the stage-2 compiler (2026-08-06)

`ctest -j32` in a `MCC_TOOLCHAIN_PROFILE=mcc` build dir, ambient `MCC_TEST_OPT` set per
level. 8577 cells, 403 skipped, ~225 s per level. Identical at all four levels:
**8172 pass, 2 fail** — `selfhost-qemu-arm-O2` and `selfhost-qemu-i386-O2`, the
`-fif-conversion` defect recorded under "Open codegen / front-end defects". Nothing else
is red, and no cell is level-dependent.

- **`run-tier/{x86_64,i386}-win32` were flaky under load, not level-dependent. Fixed.**
  They came up red in the `-O2` sweep and green at the other three; re-run in isolation
  they passed 6/6 at both `-O2` and `-O3`. It was never codegen and never wine emulation:
  the corpus always passed and the cell died in its own teardown. A red run says

      [i386-win32] -run tier: 15/15 programs OK under both JIT tiers
      rm: cannot remove '/tmp/tmp.W5WwhnLAbB/.wineprefix': Directory not empty

  The wineserver outlives its last client and flushes `user.reg` and `userdef.reg` back
  into `WINEPREFIX` as it exits. `trap 'rm -rf "$work"' EXIT` fired the instant the
  corpus loop ended, emptied `.wineprefix`, the departing server recreated a registry
  file inside it, and the closing `rmdir` failed with `ENOTEMPTY`. That `rm` was the last
  command the shell ran, so under `set -e` its status became the cell's status. Load
  stretches the server's shutdown, which is why only a full parallel suite tripped it.
  `tools/run-tier.sh` now shuts the prefix's own wineserver down and waits for it before
  removing anything, and routes teardown through a `cleanup()` that restores the body's
  status and cannot fail. Measured with both cells run concurrently under a 30-way CPU
  burn, 20 iterations each: 7 of 40 red before, 0 of 40 after, and 0 red across seven
  consecutive full `ctest` sweeps. Every abandoned `/tmp/tmp.*/.wineprefix` on a host is
  one lost race, so `ls -d /tmp/tmp.*/.wineprefix` dates the recurrences directly.

- **A shared build dir is not a place to measure a flake.** Several concurrent
  `ctest` runs in one binary dir overwrite each other's `Testing/Temporary/LastTest.log`
  and `LastTestsFailed.log`, so a cell can appear in `LastTestsFailed.log` for a run that
  exited 0 — the entry belongs to somebody else's run. Read a cell's own stdout, not the
  summary files, and give each concurrent sweep its own build dir. The `-run` PE cells
  are unaffected by the sibling hazard in `work=$(mktemp -d "$bdir/run-tier.XXXXXX")`:
  that branch is taken only when `DOCKERPLAT` is set, which the `win32` triples never do.

#### A stage-2 build dir does not rebuild when a header changes

`MCC_TOOLCHAIN_PROFILE=mcc` build dirs do not track the amalgamated headers: after
editing `src/mccast.c` and `src/mccrir.c`, `cmake --build cmake-selfhost` reported
`ninja: no work to do` while `CMakeFiles/mcc.dir/src/mcc.c.o` was eight minutes older
than both sources. Under `MCC_SINGLE_SOURCE` only `src/mcc.c` is a ninja input and every
other `.c` arrives through it, so the header dependency has to come from a compiler
depfile — which the gcc-built dir gets and the mcc-built dir does not. The stale binary
is silent and plausible: it runs, it self-hosts, it passes. **Any measurement taken from
a stage-2 dir without first forcing the object out is suspect.** Workaround is
`rm cmake-<dir>/CMakeFiles/mcc.dir/src/mcc.c.o` before building; the fix is to make mcc
emit a depfile CMake's `CMAKE_DEPFILE_FLAGS_C` can consume for this profile.

#### Self-host fallback board at HEAD (2026-08-06, x86_64 Linux, mcc-built mcc)

`mcc.c` compiled by the stage-2 compiler; `-O0` needs `MCC_FORCE_REPLAY=1` because
`ast_replay_env` is `optimize >= 1 || embed_jit || forced`, so the census is otherwise
empty there. Bodies: 2538.

| level | used | fallback | skip | `len` | `bytes` | `abort` | `posterr` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `-O0` (forced) | 2426 | 120 | 12 | 97 | 18 | 5 | 0 |
| `-O1` | 2500 | 48 | 12 | 23 | 20 | 5 | 0 |
| `-O2` | 2499 | 49 | 12 | 23 | 20 | 5 | 1 |
| `-O3` | 2499 | 49 | 12 | 23 | 20 | 5 | 1 |

The `abort` column here and in the whole-suite board below is stale: `RIR_M_CASTT` took
this one to 0 / 0 / 1 / 1 later the same day. The rest of both boards still holds and the
reasoning below them is unaffected.

- **The `-O0` fallback set is a strict superset of the `-O1` set**, and the extra is
  one single failure mode. 120 ⊃ 48; exactly 0 bodies fall back at `-O1` that do not
  also fall back at forced `-O0`; and all **72** of the `-O0`-only bodies are `len` —
  not "mostly", all of them. So the `-O1` ladder does not introduce replay divergence,
  it *removes* 72 length divergences that the `-O0` emit path has on its own. That is
  the opposite of the intuition the census was built on, and it relocates the defect:
  the residual divergence is not the optimizer, it is the replay's length accounting on
  the unoptimized emit path, which some `-O1`-gated pass happens to paper over.
- **Next step on the census, and it is a cheap one:** find which `-O1` gate erases the
  72. Re-run the forced-`-O0` census adding one `MCC_OPTD_LEVEL(1)` row at a time
  (`MCC_FORCE_REPLAY=1 MCC_RIR_FORCE=1 … -f<name>`) and watch `len` fall from 97. Since
  the 72 are a clean set difference with a single reason code, whichever gate collapses
  them is pointing straight at the length mismatch's cause — and unlike the `bytes`
  bodies, this one does not need a per-body byte diff to make progress on.
- `-O2` and `-O3` differ from `-O1` by exactly one body, `host_runmem_alloc`, and are
  identical to each other — and that one body is the `posterr` above, i.e. **not** a
  replay divergence. On the fidelity axis the ladder above `-O1` is completely flat: the
  same 48 bodies, in the same three buckets, at all three levels. Any future claim that
  an `-O2`/`-O3` pass improved or regressed replay fidelity has to beat that flatness
  first.
- The 12 `skip`s are constant at every level: `regdangle`=8, `asm`=2, `mismatch`=2.

#### Whole-suite fallback board (same day, same compiler, all 8577 ctest cells)

`MCC_RIR_PROD=2` + `MCC_RIR_PROD_OUT` over a full `ctest -j32`, ambient `MCC_TEST_OPT`
per level, `MCC_FORCE_REPLAY=1` on the `-O0` row. Every row reconciles.

| level | bodies | used | fallback | rate | skip | `len` | `bytes` | `relcontent` | `abort` | `posterr` |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `-O0` (forced) | 254430 | 242724 | 5909 | 2.32% | 5797 | 4020 | 1614 | 15 | 235 | 25 |
| `-O1` | 223195 | 213371 | 4586 | 2.05% | 5238 | 2897 | 1447 | 15 | 202 | 25 |
| `-O2` | 223077 | 213301 | 4538 | 2.03% | 5238 | 2898 | 1398 | 15 | 202 | 25 |
| `-O3` | 222733 | 213035 | 4460 | 2.00% | 5238 | 2820 | 1398 | 15 | 202 | 25 |

- The self-compile board's shape holds at suite scale: `len` is the **majority bucket at
  every level** (68% of `-O0` fallbacks, 63% at `-O1`+), and forced `-O0` carries ~1100
  more `len` bodies than any optimized level. Whatever the length defect is, it is one
  population, it is the biggest one, and it is worst on the unoptimized emit path.
- The skip census is *byte-identical* across `-O1`/`-O2`/`-O3` (5238, and every
  sub-code equal). `relcontent` (15) and `posterr` (25) are constant on all four rows.
  The ladder moves `len`/`bytes` only, and only slightly.
- `-O0` skips 5797 vs 5238 — the extra is mostly `noops` (1729 vs 1470) and `asm`
  (908 vs 721), i.e. bodies the pre-flight declines to model at all.
- Reading the raw `MCC_RIR_PROD_OUT` file: the wine `mcc.exe` cells write CRLF, so 16
  `-O0` fallback rows arrive as `len\r`. Strip `\r` before tallying or the biggest
  bucket splits in two. The file is otherwise intact under 32-way concurrent append —
  0 malformed rows in 254430.
- `keep = faithful || (nofb && replay_completed)` reduces to `keep = faithful` in every
  default build: `MCC_OPT_REPLAY_FALLBACK` is `MCC_OPTD_ALWAYS` in `mccopt.h`, so
  `ast_rir_nofb_env` is always 0 unless `-fno-replay-fallback` is passed. The comment
  above that line in `mccast.c` claims the no-fallback path is the default — it is not,
  and the TODO entry above (do not turn it on) is the accurate one.
- Re-bank `o0-baseline` at HEAD on an x86_64 Linux host. **Not a macOS item — a Mac
  cannot do this, by construction.** The five ELF glibc keys need the Gentoo stage3
  sysroots under `vendor/`, and the `<arch>-fetch` cells that download them are gated on
  `if(NOT _QEMU_${arch}) continue()` — i.e. on a qemu-user binary being present, which is
  Linux-only. So on a Mac those cells never register and the sysroots cannot be fetched
  through the supported path at all. The narrower true constraint is *a host with the
  sysroots*, not x86_64 specifically: cross output is host-independent (proven below),
  so an arm64 Linux host with the sysroots would bank the cross keys identically. What
  genuinely wants x86_64 Linux is the `x86_64` key itself. (Verified 2026-08-05 that the
  Darwin side *reproduces*: all five PE keys are byte-identical to a Linux run at the
  same HEAD, and the two `*-osx` keys are measurable only on Darwin. Also fixed there:
  `o0_ab.sh`'s `x86_64` key used `$BUILD/mcc`, the build's own compiler, so on a
  non-x86_64-Linux host it measured the host's native target under the `x86_64` label and
  reported a plausible board of 276 objects for a key it never touched; it now checks the
  `mcc -v` target string and falls back to `mcc-x86_64` plus the usual sysroot
  requirement.)
- **`C2_FORCE=1` is dead, and was failing silently until 2026-08-05.** Both
  `tools/c2_sweep.sh` and `tools/c2_equiv.sh` derive the forced-`-O0` gate list by regex
  for `ast_env_gate("MCC_AST_…", o4 || s1->optimize >= 1)`; `ast_env_gate` no longer
  exists anywhere in `src/` after the `-f` conversion, so the regex matches zero. The
  authors' guard — refuse rather than "measure `-O0` with every pass off and call it
  parity" — **could never fire**: `ngate=$(… | grep -c .)` exits 1 on zero and `set -e`
  killed the script at that assignment, before the diagnostic printed (`exit 1`, zero
  bytes of stdout *and* stderr). `|| true` on both counts is fixed; **the derivation is
  still dead**, so it now fails loudly. Any forced-`-O0` row taken today is worthless.
  To restore it: scrape every `MCC_OPTD_LEVEL(1)` row of `src/mccopt.h` (21) and add the
  `MCC_OPT_SPECIAL` rows whose expression is `o4 || optimize >= 1` — **those are
  per-target**. `reg-disp` is `optimize >= 1` only under `MCC_TARGET_X86_64`; forcing it
  on arm64 moves `const_member_copy.c::main` by one `len` unit and fabricates a
  divergence. Correct sets: arm64 = 21 + `ivopts` + `builtin-minmax` + `builtin-fma`;
  x86_64 = 21 + `ivopts` + `reg-disp`. Pass as `-f<name>` on the compile line, not as
  env, with `MCC_FORCE_REPLAY=1 MCC_RIR_FORCE=1`. `MCC_AST_INT128=1` no longer exists
  either. Reproduce single files under `sh`, not `zsh` — zsh does not word-split
  unquoted `$FLAGS`, so mcc silently ignores it and you measure plain `-O0`.
- **Settled 2026-08-05, both were open questions in the archive.** (i) `arm64-osx`'s
  forced-`-O0` board *is* identical to its `-O1` board, per file as well as on all four
  sub-counters — but the control no longer reproduces: `x86_64-osx` is now also
  identical, so the equality is a property of the **tree**, not of the key, which is the
  opposite of the inference drawn from it. Likely `a55c0a07`, which moved six
  replay-fidelity knobs to always-on and removed the `-O0`-only gap. (ii) The
  `fuzz/runner.c` live-on-Linux / fallback-on-Mach-O split was **code drift, not a
  per-key pre-flight difference**, bisected to `c5db86ae` (`AST_FB_LOAD_LVAL`); all three
  bodies are live on every measurable key at HEAD. The axis was also misread — at
  `b89723f9` the fallback set is arm64/arm/i386 against x86_64, on **both sides** of the
  ELF/PE/Mach-O line. And the "pre-flight" framing had a wrong premise: `rir_prod_why` is
  empty in every fallback observed, so nothing declined; the fallback is the post-replay
  byte compare.
- `ptr_unlink` for-condition-store segfault: root-caused, fix open (the `rir_cf_cond` /
  `rir_docond` machinery; a 5-fix / 34-break discriminator).

#### Per-region keep/restore — measured, and refused (2026-08-06)

Keep/restore in `ast_func_end` is whole-body: one divergent byte discards the entire
replay. At ~50 regions per body the obvious idea is to make the unit a region instead.
It was measured at suite scale and it does not work. Two independent full `ctest` runs
(98930 bodies, 2669 fallbacks) agree to the row. **Do not build this**, and do not
re-open it without first reading the last bullet.

- **The regions are not a cover of the function body.** `RIR_R_BODY` is a *loop* body
  (`rir_hook_do_body_begin` / `for_body_begin` / `while`), not the function's. Nothing
  wraps a whole function, so straight-line top-level statement code sits inside zero
  regions. Measured consequence: for **1340 of 2566** fallbacks with a byte span (52.2%)
  there is no enclosing region at all, so there is no region to swap. Deduped to the 147
  distinct fallback signatures it is still 35.4%.
- **The byte disagreement is positionally local but structurally not containable.**
  Median divergent span is 6.7% of the body and touches 4 of ~36 regions; but the median
  *smallest enclosing region* is 100% of the body. Split by reason, the 584 `bytes`
  (same-length) fallbacks are genuinely tight — median span 2.1% of the body, 84.6%
  within 10% — while the 1964 `len` fallbacks are not, because a length change at offset
  X invalidates every PC-relative displacement crossing X. `len` is 76.5% of the
  population, which is the same thing the boards above say from the other direction.
- **Boundary state, over 129861 regions of one self-compile.** Only **4.0%** of regions
  (5256) have an empty value stack at *both* boundaries, no open return chain, and no
  break/continue/goto/label crossing them. 53.4% have a live hard register at a
  boundary, 29.2% carry a `VT_CMP`/`VT_JMP`/`VT_JMPI` (pending flags, or another jump
  chain embedded in the value stack), 48.0% have `rsym != 0`. The clean ones are exactly
  the statement-shaped kinds — `then` 32%, `else` 38%, `incr` 49%, `body` 29%, `for`
  24%, `do` 76%, `synth` 2% — and zero of `if`/`while`/`switch`/`ternary`/`landor`/
  `call`/`cond`/`inc`/`member`/`tarm`/`lsup`/`lopnd`/`vstore`/`cvt` qualify.
  Cross-referenced against where divergences land, only 22.9% of fallbacks are even
  enclosed by a *kind* that can qualify, before the ~30% per-instance filter.
- **What is region-local: the value stack, and only at statement boundaries.
  Everything else in the save/restore set is body-global.** `rsym` is the head of a
  linked list threaded through the text itself — `gjmp` stores the predecessor's text
  offset in the jump's own disp32 and `gsym_addr` walks it — and it is resolved by
  `gsym(rsym)` in `gen_function` *after* `ast_func_end` returns, so at the decision
  point the chain is live and physically interleaved with the code of every region.
  Break/continue are the same construction (`*cur_scope->bsym = gjmp(*cur_scope->bsym)`),
  as is the alloca chain in `mcc_state->cg_func_alloca`. `loc`, `anon_sym`,
  `nb_temp_local_vars` and `arr_temp_local_vars` are monotonic per-body accumulators —
  `get_temp_local_var` reuses slots by scanning the live value stack, so the table at a
  region's entry is a function of every preceding region. The relocation section is an
  append-only cursor.
- **The decisive obstacle: the replay is not a pure function of the arena.** It consumes
  four record/replay side channels — `rir_locrec`, `rir_slotrec`, `rir_tvrec`,
  `rir_fcrec` — each a single body-global array with one monotonic cursor advanced by
  comparing recorded positions against the *current* `ind`. Replaying one region in
  isolation would have to seek four cursors, and the keying is by absolute code offset,
  which any splice changes. This is not a per-region-kind soundness question; it is that
  **`ast_replay_body` has no entry point below the body at all.** Anything that revisits
  this has to make those four cursors seekable first, and that is the whole cost before
  any of the coverage arithmetic above starts to matter.
- `ast_baseline_splice` is what the splice primitive would have to look like, and shows
  the cost from the other end: memcpy, re-issue every relocation rebased off
  `src_base`, then walk the saved chain head and re-link every node into the current
  `rsym`. It handles exactly one chain and is only ever used for a whole body emitted at
  a fresh `ind`, with all other chains already resolved.
- **Arithmetic of the verdict.** Sound for at most 7 of 21 region kinds
  (`then`/`else`/`incr`/`body`/`for`/`do`/`synth`), covering 4.0% of regions, reaching
  maybe 7% of divergences, and only for the 23.5% of same-length fallbacks that are
  region-enclosed at all. The salvage argument is also weaker than it looks: the oracle
  that matters is the goldens, not the memcmp, and a divergent region is usually correct
  code that merely encodes differently. **If the byte compare is ever replaced by the
  goldens as the gate, this direction loses its remaining motivation entirely** — there
  is nothing left to restore.
- What survives in the tree: `rir_prod_span` appends `first`/`end`/`blen`/`nlen` to the
  `fallback` rows of an `MCC_RIR_PROD_OUT` census — where in the body the replay first
  disagrees and how far the disagreement runs. That is kept because the standing "close
  the open per-body byte divergences, fix at the USE site" item needs it, not because of
  this question. The region-pairing columns (`rall`/`rhit`/`rmin`/`rminkind`/`rdepth`)
  and the `MCC_RIR_BND=1` per-kind boundary table produced the numbers above and are
  **not** in the tree; they answered a closed question. Recover them from `934b692e` if
  the numbers ever need retaking.

### RIR cut — remaining cleanup
- Deletion residue, keep-or-delete each: `ast_verify_diff` (+ `_match` / `_dump_diff`),
  `ast_treechk` / `MCC_AST_TREECHK`, `ast_jit_guard_env` (declared, never assigned),
  `ast_rir_arena` (dead local), `tools/tracediff.sh`, the recorder half of
  `gate-ledger.sh`.
- **P6** — split the monolithic `src/mccast.c` (~17k lines) and rename `ast_*` → `ir_*`.
  Precondition still false: the `ir_` namespace already collides (~850 occurrences) and
  `targetgate` still whitelists `mccast.c`.
- ~~Land the held `fix-imaginary` branch.~~ **Closed** — redundant with `38508d54`; see the branch section above.

### Open codegen / front-end defects
- `__bf16`: finish encode/decode + ABI now that `VT_BTYPE` is 5 bits. **Do not alias
  `__bf16` onto `_Float16`** — distinct `c.i` storage, `is_float_abi`, libgcc name.
- 32-byte vectors are laid at 16-byte alignment (`MCC_MAX_ALIGN` cap) — open ABI
  decision; cross-TU to gcc is currently incompatible (struct-ABI, not SysV vector).
- `aligned(N)` bitfields: ~139 survivors.
- ~~`expr_type()`'s unconditional `nocode_wanted++`.~~ Already fixed: `expr_type_vm`
  re-parses and evaluates a variably-modified operand. Of the 8 tests once attributed to
  it only `vla-14`, `vla-24` and `vla-stexp-1` still fail, each for a different reason.
- ~~`selfhost-qemu-{i386,arm}-O2`~~ **Fixed 2026-08-06.** Both targets now reach a
  byte-identical `s2 == s3` fixpoint at `-O1` and `-O2`. Two things are worth keeping:
  - **The `MCC_MAX_ALIGN` diagnosis was wrong.** The `alignment of 16 is larger than
    implemented` error at `mccdefs.h:528` was a *symptom*; `MCC_MAX_ALIGN` is not on
    that path at all. `parse_one_attribute` errors when `n != 1 << (ad->a.aligned - 1)`,
    i.e. when the `aligned : 5` bitfield fails to round-trip — and it failed because
    **every** bitfield store in the stage-2 compiler was writing 0.
  - **Root cause: `-fif-conversion` marked ternaries whose arms are two-word types.**
    `ast_sel_gpr` accepted `VT_LLONG`, which is a register *pair* on a 32-bit target —
    which is exactly why only the two 32-bit ELF targets failed. The marked body in
    `mcc.c` was `vstore`'s bitfield mask, `bits >= 64 ? ~0ULL : (1ULL << bits) - 1`. The
    replay path materialises the condition into a GPR *before* evaluating both arms;
    the arms need `r0:r1` and call `__aeabi_llsl`, so the condition is spilled and then
    reloaded into `r0` — on top of the mask's low word. The mask came out 0, so every
    bitfield store wrote 0. `gen_select` already refuses two-word types and falls back
    to `gen_select_branch`, so marking them bought nothing and only took the riskier
    both-arms-first path; the fix is one guard in `ast_sel_gpr` and is a no-op on
    64-bit, where `long long` is a single word.
  - Regression test: `tests/exec/codegen/select_two_word.c`. Verified to fail (`mask 62`
    and `mask 63` come back with a corrupted low word) on both arm and i386 at `-O2`
    with the guard disabled, and to pass at `-O0`..`-O3` on x86_64, arm and i386 with it.
    It only bites at `-O2`+, so keep it in a corpus the optfire/exec-search cells run.
- 32-byte vectors are laid at 16-byte alignment (`MCC_MAX_ALIGN` cap on i386/arm is 8,
  16 elsewhere) — open ABI decision, and a real constraint, but it was **not** the cause
  of the self-host failure above.
- Register-array decay; const-parameter assignment; `_Atomic` complex.
- Varargs pr92904 (32-byte-aligned param when caller align == 16); `__int128` old
  signedness coercion.
- `__builtin_object_size` subobject-from-declared-array-type: reverted (regressed 5
  tests); `-1` is the safe answer — **do not re-introduce**.
- Nested member designator `{.a.a=1, .a.b=2}`.
- **D6** — scalar_storage_order / ms_abi is the most dangerous open item: mcc objects
  link against gcc's, so a mismatch is *silent* wrong codegen.
- Declined upstream `7f7845cd` (VT_VOID); i386 `R_386_TLS_GOTIE` gap.
- Raise arena fidelity / finish the capture path (Phase F).
- Four hand-reproduced wrong-answer defects from the three-compiler board are written up
  under "External suites" below, where the board that found them is. (A fifth,
  `__builtin_expect` dropping side effects, is fixed and closed above.)

### Intermittent / to-confirm

**The orchestration hazard is confirmed, not a suspicion — and it manufactured three
separate false results on 2026-08-06.** Parallel agents were each given the *same absolute
path* to the main checkout's build dir. Consequences, all observed:
- Concurrent `ctest` runs race on `Testing/Temporary/LastTest.log` and
  `LastTestsFailed.log`, so one agent read another agent's failure as its own.
- A rebuild swaps `mcc` out mid-run, so a census can silently mix two trees into one board.
- `tests/exec/programs/stdio.c` writes `fred.txt` into the **current directory**; 18
  full-corpus cells sharing one cwd made `programs/stdio` look nondeterministic, which is
  what the "`iso-bf` flake" actually was. Each sweep cell gets its own cwd now.

Give every parallel agent its own worktree **and its own build directory**, and never a
path into the shared checkout. Related, and the same root cause in miniature: `cd` persists
between shell invocations, so a build launched from the wrong directory fails while
`grep -c "error:"` on its log prints 0 — check the build's exit status, not a grep.

- `superopt/promote-floor` failed once in eleven full-gate runs and passes in isolation.
  Not chased. May be the hazard above rather than a real intermittent, since those runs
  were concurrent with other agents.
- `selfhost-fixpoint-memmodel-{O3,Os}` SIGSEGV'd once under heavy parallel load and
  never reproduced. Same suspicion, now much better founded.
- ~~The two win32 `-run` cells are an intermittent wine flake.~~ **Not wine, and not a
  flake in the tree — fixed in `36e20b30`.** `tools/run-tier.sh` tore its work tree down
  with `trap 'rm -rf "$work"' EXIT`. The wineserver outlives its last client and flushes
  `user.reg`/`userdef.reg` back into the prefix as it exits, so `rm` emptied
  `.wineprefix`, the departing server recreated a registry file in it, and the closing
  `rmdir` failed `ENOTEMPTY`. That `rm` was the last command the shell ran, so under
  `set -e` **its status became the cell's status** — the cells passed 15/15 programs and
  then died in their own cleanup. ~17% per cell run under load, 7 red in 40 before, 0 in
  40 after, and 0 red across 11 full sweeps. Every lost race abandoned a
  `/tmp/tmp.*/.wineprefix`; 47 had accumulated at one or two per hour, matching "red in
  nearly every gate all day", and post-fix sweeps create none. Teardown now shuts down
  that prefix's own wineserver and can no longer decide the cell's exit status.
- ~~Reconcile the deliberate-red count.~~ **Settled 2026-08-05 by running them.** It was
  7; then **2**, both PE (`run-tier/{x86_64-win32,i386-win32}`, `tls` and `tls_threads`
  each); it is now **0** — the PE `-run` TLS defect is fixed (see the fix write-up above),
  so all four cells pass and `KNOWN_RED` is empty. x86_64-native, i386, arm and riscv64
  were closed earlier by the archive's `-run` TLS fixes. Any deliberate-red `-run` TLS
  cell reappearing is a regression. Two *further* cells became red only because the qemu
  sysroots now exist, `selfhost-qemu-{i386,arm}-O2` — **both fixed 2026-08-06**; the
  `MCC_MAX_ALIGN` attribution was wrong and the cause was `-fif-conversion` selecting
  two-word arms on 32-bit targets. See "Open codegen / front-end defects".

### External suites — three-compiler board taken 2026-08-05 at `030fb4aa`

The cross-oracle run the old entry here asked for is **done**, and it replaces the
`b8f1a80b` numbers that used to be quoted in this slot. Method: render the harness plan
once, then run it three times — clang, gcc, mcc — over both trees, and keep only the
cells where **clang and gcc both PASS**. That set is *agnostic*: portable C two
independent compilers agree on, under a plan the harness demonstrably renders correctly.
mcc's misses on it are gaps. Its misses anywhere else are somebody's extension and are
deliberately not counted.

21,638 runnable tests × {`-O0`,`-O2`} = **43,318 cells** common to all three boards.
**37,885 (87.5%) are agnostic; mcc passes 36,430 of them — 96.2%.** The 1,455 misses:

| cells | files | bucket |
| ---: | ---: | --- |
| 574 | 288 | mcc **accepts** what both reject — missing diagnostics |
| 400 | 203 | long tail, one to six cells each |
| 135 | 76 | dead call survives to link — a fold mcc does not do |
| 66 | 33 | `__bf16` unsupported (already open above) |
| 66 | 33 | missing `__builtin_*` |
| 61 | 32 | wrong or missing runtime answer |
| 52 | 26 | `__m512` / `__m256h` / `__m128h` |
| 47 | 25 | other unresolved reference |
| 28 | 14 | `__float128` / `_Float128` |
| 9 | 5 | `_Complex _Float16` |
| 8 | 4 | compound-literal initializer (already open above) |
| 4 | 2 | `_BitInt` |
| 4 | 2 | `__seg_fs` / `__seg_gs` (already open above) |
| 1 | 1 | ICE |

Reproduce with `tools/xsuite.py --mcc <cc> --out <dir> --gcc <gcctree> --llvm <llvmtree>
--opt=-O0 --opt=-O2`, once per compiler, then intersect on `(file, opt)`.

**Caveats that must travel with this board.** Samples were re-verified by replaying the
exact per-test command line against all three compilers: 12/12 confirmed for the XPASS
bucket, 11/12 for FAIL — budget ~8% replay noise on run-mode cells. `ET_FALSE` hides
whole feature families (`__int128`, vectors, LTO, profiling) from *all three* boards
equally, so "87.5% agnostic" describes the tests the harness admits, not the trees:
26,281 of 47,919 files are skipped by directives before anything runs. `KEEP_OPT_RE` does
still drop `-idirafter` and `${srcdir}` — but it does **not** drop `-Werror`, contrary to
the note this entry replaces.

#### Missing diagnostics — 574 cells / 288 files, the largest single bucket
mcc compiles these clean; gcc and clang both reject. No dominant cause — the top
families by file count are attribute-argument validation (`access`, `assume`,
`counted_by`, `malloc`, `format`, ~20 files), universal-character-name validation
(out-of-UCS-codespace and malformed `\u`), `void *` and function-pointer arithmetic
pedwarns, C++ raw strings not rejected in C, C90 non-lvalue-array rules, and asm operand
addressability. Each is a handful of files; there is no one fix.

#### Folds mcc does not do, surfacing as link failures — 135 cells / 76 files
The gcc torture suite guards `extern void link_error(void)` behind a condition the
optimizer must prove false; when the fold happens the symbol is never referenced and the
link succeeds. **58 of these fail at `-O0`**, where gcc and clang still fold them in the
front end — so this is not an `-O2` pipeline gap. Recurring shapes: `fabs(x) < 0.0`,
`__builtin_constant_p` chains, and signed-overflow reasoning.

#### Newly-confirmed wrong-answer defects, each reproduced by hand at `-O2`
- ~~`__builtin_expect` discards side effects in its second operand.~~ **Fixed** — see
  the closed list above.
- **`-fno-wrapv` does not enable signed-overflow simplification.** `fwrapv-2.c`:
  `(2*x)/2` must fold to `x`; mcc computes the wrapped value and the test aborts.
- **Builtin math folding loses to a local definition.** `20021127-1.c` defines an
  aborting `llabs` and requires the compiler to fold `llabs(-1)` to 1 regardless.
- **GNU range designators in nested initializers produce wrong data.** `gnu99-init-1.c`
  mixes `[2 ... 4][0 ... 1][2 ... 3] = 1` with later overriding designators. Adjacent to
  the nested-member-designator item above, but a distinct bug.
- **C90 `if`-controlling-expression scope.** `c90-scope-1.c`: under `-std=iso9899:1990` a
  struct declared in an `if` condition belongs to the enclosing scope, not to the `if`.
  mcc applies C99 scoping in both modes.

Do **not** refile the rest of that bucket: the `builtin-object-size` /
`builtin-dynamic-object-size` cells are the deliberately-declined item above, and
`vla-14` / `vla-24` are already open.

#### Types the front end cannot parse
`__m512` / `__m256h` / `__m128h` (52 cells), `__float128` / `_Float128` (28),
`_Complex _Float16` (9), `_BitInt` (4). `_BitInt` is C23-mandatory; the rest gate the
x86 ABI tests, which is why `gcc.target` sits ~2pp below the other suites.

#### Missing `__builtin_*` — 66 cells / 33 files
`__builtin_cpu_supports` and `__builtin_cpu_init`, `__builtin_setjmp` and
`__builtin_longjmp`, `__builtin_addc` and the `subc` family, `__builtin_powi` /
`__builtin_powif`, `__atomic_thread_fence`, `__builtin_eh_return_data_regno`,
`__builtin___fprintf_chk`, `__builtin_vprintf`.

#### Confirmations for the clusters the archive had ranked
Still open, now measured on the agnostic set: `__seg_fs`/`__seg_gs` (4 cells),
`alias("x")` resolved by C identifier instead of asm name (10), compound literal as an
initializer without braces (8, all `gnu89-init-*` and `pedwarn-init`). The std-gated
pedwarn batch and `#pragma GCC system_header` are inside the 400-cell long tail, whose
own top signatures are `initializer element is not constant` (20), `invalid array size`
(18), `constant expression expected` (16) and `string constant expected` (14).

#### Optimizer ladder board, `-O1`/`-O2`/`-O3`, same method

Re-run with the ladder instead of `-O0`/`-O2`, agnosticism decided **per level** so a
test is judged against oracles running the same optimizer level. 64,977 common cells,
56,808 agnostic.

| suite | cells | agnostic | `-O1` | `-O2` | `-O3` |
| --- | ---: | ---: | ---: | ---: | ---: |
| gcc:c-c++-common | 3162 | 83.9% | 833/884 | 833/884 | 833/884 |
| gcc:c-torture/compile | 5514 | 93.6% | 1711/1721 | 1711/1721 | 1711/1721 |
| gcc:c-torture/execute | 5127 | 93.8% | 1585/1603 | 1584/1602 | 1584/1602 |
| gcc:gcc.dg | 34818 | 88.0% | 9826/10217 | 9823/10213 | 9821/10211 |
| gcc:gcc.misc-tests | 96 | 78.1% | 24/25 | 24/25 | 24/25 |
| gcc:gcc.target | 8166 | 92.1% | 2362/2508 | 2362/2508 | 2362/2508 |
| llvm:clang | 8046 | 73.6% | 1856/1975 | 1856/1975 | 1856/1975 |
| llvm:compiler-rt | 45 | 46.7% | 7/7 | 7/7 | 7/7 |
| **TOTAL** | | | **96.1%** | **96.1%** | **96.1%** |

**The optimizer introduces no failures.** Of 18,928 files agnostic at all three levels,
**exactly one** has an mcc verdict that changes with `-O`, and it moves the *good* way:
`gcc.dg/torture/20240517-1.c` is FAILEXE at `-O1` and passes at `-O2`/`-O3`. Nothing
regresses from `-O1` to `-O3`. That is the single most useful line on this board — it
says the remaining 736 misses per level are front-end and diagnostic work, not
optimizer bugs, and it is why the buckets above carry no `-O`-specific column.

This board also cross-validates the first one: the XPASS count fell by exactly the 3
files the function-pointer-subscript fix closed, and FAIL by the 3 the powi commit
closed, with everything else unmoved.

- **String literals are not merged until `-O2`.** The one level-dependent cell.
  `20240517-1.c` needs `"Hello"` and `"Hello" + 1` to denote the same object; gcc and
  clang merge identical string literals from `-O1` (`-fmerge-constants`), mcc only from
  `-O2`, so the test aborts at `-O0` and `-O1`. The test's own
  `-fmerge-all-constants` is dropped by `KEEP_OPT_RE`, so this is mcc's *default*
  behaviour at `-O1` being measured, which is the thing that differs.

#### Harness defect found and fixed while taking this board
`tools/xsuite.py` buffered the child's stdout in the parent for *every* mode, including
`-E`, where it is never read. One gcc.dg preprocessor test drove clang to emit multi-GB
of expansion; the parent held 7.4 GB resident, one worker went uninterruptible in
page-fault and the GIL starved the other nine. Throughput collapsed from ~240 results/s
to 8/s and the clang board could not finish. Preprocess stdout now goes to `DEVNULL`,
run-mode stdout likewise (nothing reads it — `dg-output` is ignored), and captured
stderr is capped at 1 MiB.

That fix was **incomplete and the ladder board hit the other half**: capping the string
after `capture_output` still buffers the whole stream first. `gcc.dg/builtin-stdc-bit-1.c`
is a *stderr* bomb under clang (it does not know `__builtin_stdc_bit_*`), and the parent
reached 12.7 GB with the same D-state/GIL-starvation collapse. Child output now goes to
a file that `RLIMIT_FSIZE` actually binds — lowered from 1 GiB to 64 MiB, which is ample
for these `.o`s — and only the first 1 MiB is read back. That test now records a clean
TIMEOUT, and the resumed board ran at 12,000 results/45 s with the parent at 3.7 MB.

**Two traps this cost a run each, worth knowing before quoting a board.** A compiler
copied out of the build tree cannot find its own freestanding headers: an `mcc` pinned
to `/tmp` failed 2,321 cells on `stddef.h not found` and scored 84.4% instead of 96.1%.
A smoke test using only `__builtin_*` will not catch it — include something. And do not
relink `mcc` while a board is running against it; pin by leaving the binary in place and
not building, not by copying it elsewhere.

Unfixed and still latent: `subprocess.run`'s timeout kills the driver but not the
`-cc1` grandchild, so orphaned compilers accumulate and keep burning CPU; `preexec_fn`
is also documented-unsafe under threads.
