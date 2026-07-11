# OPT.md — how `-O4` works, in plain English

This is a walkthrough of what happens when you run `mcc -O4 foo.c` (or any
`-O<N>` with `N >= 4`). It is written for someone who has never read the
code. It explains the moving parts, roughly in the order they run, and
points at the functions that do the work so you can go deeper.

For the current AST/optimizer behavior reference see [MCC.md](MCC.md) §9 and
[EXCESS.md](EXCESS.md); for remaining work see [TODO.md](TODO.md). This file is
the "how it works" narrative that ties those together.

The one-sentence version: at `-O4` and above the compiler does not just
compile your file once — it **compiles it many times with different
optimizer settings, keeps the best result, and remembers what it learned
on disk so the next run picks up where this one left off.**

Below `-O4` there is no search. `-O0`/`-O1` are minimal, and `-O2`/`-O3`
run a **fixed set of always-on optimizer passes** (described in §4) exactly
once, with no self-recompilation. As of the §41 "default-on sweep"
(2026-07-11) that fixed set is fairly rich — the join-dataflow, bit-flag,
cross-call, narrowing and operand-ordering passes now all run by default at
`-O2`. The `-O4+` search is a layer *on top of* that ordinary `-O2`/`-O3`
work: it re-tunes those passes and searches a few extra knobs, but it never
starts unless you actually ask for `N >= 4`.

---

## 1. What `-O<N>` for N≥4 triggers at the driver level

Normal optimization levels `-O0`, `-O1`, `-O2`, `-O3` each select a fixed
amount of work and produce one output. `-O4` and higher mean something
different. When the option parser (`libmcc.c`, `MCC_OPTION_O`) sees a
numeric level greater than 3 it does **not** invent a new tier of passes.
Instead it pins the internal optimization level to 3 (`s->optimize = 3`)
and records the number itself as a **time budget in seconds**:
`s->optimize_search_seconds = lvl`. So `-O4` means "spend about 4 seconds",
`-O8` means "spend about 8 seconds", and so on. The digit is a stopwatch,
not a knob.

After the front-end has finished its normal setup, the driver (`main` in
`mcc.c`) checks whether a search should run. It only kicks in when
`optimize_search_seconds` is set, the output is a real object or
executable (`-c` object or a linked exe), there is at least one input
file, and we are **not already inside a search worker** (the
`MCC_SEARCH_WORKER` environment guard — see below). If those hold, the
driver hands off to one of two search functions instead of compiling
normally:

- `mcc_superopt_search` — the default, whole-translation-unit search.
- `mcc_superopt_perfn` — a per-function search, selected by setting the
  `MCC_AST_PERFN` environment variable.

The search works by **re-running the compiler on itself**. It finds its
own executable path (`host_exe_path`) and repeatedly spawns child
`mcc` processes, each compiling the same input but with different optimizer
settings passed through environment variables. Each child sees
`MCC_SEARCH_WORKER=1`, which is exactly the flag that stops the driver from
recursively starting *another* search — the children just compile once and
exit. The parent measures each child's output and keeps the winner.

Because a search can run for many seconds, it is **interruptible**. Before
looping, the search installs handlers for `SIGTERM`, `SIGINT`, and `SIGHUP`
(`so_on_stop`) that set a `so_stop` flag. Every loop checks that flag, so a
Ctrl-C or a `kill` makes the search stop promptly, save its progress to the
on-disk checkpoint, write out the best result it has found so far, and
exit in a second or two rather than abandoning the work.

---

## 2. The search space

The thing being searched is a **configuration of optimizer passes**. The
whole-TU search (`mcc_superopt_search`) treats that configuration as three
independent dimensions and explores them as a portfolio of strategies.

**The three dimensions** (translated into environment variables for each
child by `so_setenv_cfg`):

- **Gate** — which passes are on and how hard the inliner tries. This is a
  small bit-field: bit 0 turns on the AST "template" passes
  (`MCC_AST_TEMPLATES`), bit 1 register promotion (`MCC_AST_PROMOTE`),
  bit 2 inlining (`MCC_AST_INLINE`), bit 3 the "no call-ful promotion"
  restriction (`MCC_AST_NO_CALLFUL`), and the upper bits (`gate >> 4`)
  carry the inline size limit, from 0 up to 160 (`SO_INLINE_LIMIT_MAX`).
  So the gate dimension has `16 × 161 = SO_GATE_SPACE` points. A helper
  `so_gate_dead` skips the meaningless combinations (an inline limit set
  while inlining is off).
- **Budget** — the *shape* of the aggressive passes, packed into one
  index of `SO_BUDGET_SPACE = 144` points. `so_setenv_cfg` unpacks it into
  five sub-choices: inliner node budget (`{64,128,256}`), graft budget
  (`{2048,4096,8192}`), the §30 bit-flag cluster threshold
  (`{0,3,5,9}` via `MCC_AST_BITFLAG`), and two on/off join-dataflow
  switches — §32a structured-join const-prop (`MCC_AST_CPROP_JOIN`) and
  §32b cross-join CSE/LICM (`MCC_AST_CSE_JOIN`). That is
  `3 × 3 × 4 × 2 × 2 = 144`. Note that the bit-flag threshold and both join
  switches now have **default-on baselines at `-O2`** (see §4), so inside the
  search this dimension is mostly asking the opposite question: whether
  *disabling* or re-tuning a pass makes a particular file smaller.
- **Opt-limit** — a cap on how *many* functions get the expensive
  promotion/optimization work, one of `{-1, 64, 16, 4, 1}`
  (`SO_LIMIT_SPACE = 5`), driving `MCC_AST_PROMOTE_LIMIT` and
  `MCC_AST_OPT_LIMIT`. `-1` means unlimited; the small values let the
  search find out whether restricting the work to the few hottest
  functions is actually better.

**The 3-strategy portfolio and greedy composition.** The search does not
brute-force the full product of those dimensions (that would be enormous).
Instead each round it optimizes **one dimension at a time**, holding the
current best values of the other two fixed — a greedy hill-climb. It first
sweeps a chunk of the gate space, adopting any gate that shrinks the
result; then sweeps the budget dimension the same way; then the opt-limit
dimension. The best-so-far from each dimension feeds the next, so good
choices compose.

**Exponentially-doubling time slices.** Each dimension gets a time slice
per round, and the slice **doubles every round** (`slice = base_ms <<
round`, capped). The base slice is calibrated from how long the very first
compile took (`base_ms = first_eval_ms × SO_SLICE_FACTOR`, factor 8), so
the search adapts to the size of the program: a big TU gets proportionally
bigger slices. Early rounds are quick and broad; later rounds spend longer
per dimension refining the frontier. The whole thing stops when the total
wall-clock budget (`optimize_search_seconds`) is spent, the space is
exhausted, or a stop signal arrives.

**Concurrency.** Multiple `mcc -O4` processes on the same input cooperate
rather than duplicate work. The gate space is handed out in chunks of
`SO_CLAIM_CHUNK = 64` through an atomic **claim cursor** (`so_claim`)
advanced under a file lock, so two workers each cover half the space
instead of both covering all of it.

Each child compile also runs under a **watchdog** (`so_spawn_timeout`): if
a candidate compile hangs or runs away, it is killed after a timeout and
the search moves on, so one pathological configuration cannot stall the
whole run.

---

## 3. Scoring — how a candidate is judged

Every candidate compile is turned into a single number by `so_eval`, and
smaller is better. There are two scoring objectives.

**Default: `.text` size.** For most builds the score is the size of the
executable code. `so_textsize` parses the resulting ELF file and sums the
sizes of all sections flagged executable (`SHF_EXECINSTR`), falling back to
the plain file size for non-ELF outputs. This is a fast, deterministic
proxy for "tighter code".

**Optional: CPU + memory (the JIT scoring tier, §25).** When
`MCC_AST_JITSCORE` is set *and* the TU actually links to a runnable
executable, the score becomes **runtime performance** instead of size.
`so_run_score` runs the candidate binary up to three times (best-of-K),
each run via `so_spawn_run`, which forks the program, times it with
`gettimeofday`, and reaps it with `wait4` to collect `getrusage` peak
resident-set size. The candidate's score is the **best (smallest) wall-time
of the K runs**, and the peak RSS is reported alongside. Runs are also
watchdog-limited, so a candidate that loops forever is killed and simply
loses. On Apple silicon the runnable-candidate path goes through the
W^X / `MAP_JIT` runmem machinery so timed execution works under hardened
JIT rules.

**Non-runnable fall back to size.** If the TU can't be run — it's a `-c`
object, `-S`, `-E`, `-r`, or `-shared` output — the CPU tier is disabled
for that build and scoring falls back to `.text` size. To keep the two
kinds of measurement from contaminating each other, the JIT-scoring mode is
folded into the checkpoint **key** (`so_key` mixes in a `"jitscore"` tag),
so a size-scored checkpoint and a time-scored checkpoint never collide on
disk.

---

## 4. The optimizer the search is permuting — AST replay

Everything above is a *search harness*. The actual optimization it permutes
lives in `mccast.c` and is called the **AST-replay optimizer**. Here is
what that means and what each pass does.

**Replay / re-emit.** As the front-end compiles a function it records the
function body as a small tree (an `AstArena` — the function's "intention").
After the normal code for the body has been emitted, the optimizer can
**re-emit** the function from that captured tree. Before optimizing, it
first replays the tree *unchanged* and checks that the bytes it produces
are **identical** to what the front-end already emitted — the
**faithfulness gate** (`ast_fn_faithful`, computed by comparing the
re-emitted text and relocations against the saved originals in `mccast.c`
around line 5340). Only if replay is byte-for-byte faithful does the
optimizer trust the tree enough to transform it and ship the result. If
replay diverges for any reason, the function keeps its original code and no
pass runs. This gate is why an experimental pass can never silently
miscompile: an unfaithful body is simply not reoptimized.

The transform passes only run when the function is faithful **and** the
relevant environment gate is on. The umbrella gate for the "template"
family is `MCC_AST_TEMPLATES` (default on from `-O1`); the individual
passes are then attempted and each keeps its result only if it actually
fired and helped.

Two kinds of gate matter here, and the difference is the whole "always-on
vs searched" story. Some `MCC_AST_*` gates default **on** at a fixed `-O`
level because their pass is always beneficial or neutral — so a plain `-O2`
build just runs them, no search involved. As of the §41 default-on sweep
that always-on set is: the template family (`MCC_AST_TEMPLATES`, from
`-O1`), register promotion (`MCC_AST_PROMOTE`, `-O2` on x86_64), Sethi–Ullman
operand ordering (§35), the bit-flag transform (§30), both join-dataflow
passes (§32a/§32b), the cross-call availability window (§33a), and
truncation-sink narrowing (§29). The `-O4+` search does **not** turn these
on from scratch; it *tunes* them (inline/graft budgets, the bit-flag
threshold) and can toggle a couple back off just to check whether a given
file is smaller without them.

A few newer, more speculative passes stay default-**off** behind their own
gates and are reached only by setting the gate by hand — they are not (yet)
wired into the automatic search: fresh-temp loop-invariant hoisting
(§32c, `MCC_AST_LICM_TEMP`), induction-variable strength reduction
(`MCC_AST_IVSR`), post-join partial-redundancy elimination
(`MCC_AST_PRE`), and graph-coloring register allocation (§36,
`MCC_AST_COLOR`). These are the natural next search dimensions; for now they
exist as opt-in gates you can enable to measure them.

In plain English, those passes are:

- **Constant / template folding** (`ast_bfold_run`) — evaluate
  compile-time-constant subexpressions and collapse them to their result,
  so the emitter never computes them at run time.
- **Copy / constant propagation** (`ast_cprop_run`) — when a variable is
  known to equal a constant or another variable, substitute that value at
  its uses, exposing more folding and dead code.
- **Identity / redundant-cast elimination** (`ast_ident_run`) — drop
  no-op operations and casts that convert a value to a type it already has
  (or widen-then-narrow round-trips that provably change nothing).
- **Truncation-sink narrowing** (§29, `MCC_AST_NARROW`) — when a wide
  computation only ever flows into a narrower sink, so its top bits are
  thrown away by a truncating store or cast, redo the arithmetic at the
  narrow width. The emitter then never computes bits that are about to be
  discarded. Every candidate narrowing is checked for soundness first, so
  it only fires where the discarded bits provably don't matter.
- **Common-subexpression elimination** (`ast_cse_run`) — recognize that
  the same expression is computed twice and reuse the first result instead
  of recomputing it.
- **Loop-invariant code motion** (LICM, `ast_licm_folds`) — hoist a
  computation whose inputs don't change inside a loop out to before the
  loop, so it runs once instead of every iteration.
- **Dead-store elimination** (`ast_dse_run`) — remove writes to variables
  whose value is never read afterwards.
- **Sparse conditional constant propagation** (SCCP, `ast_sccp_run`) and
  **jump threading** (`ast_jt_run`) — propagate constants through branches,
  discovering that some conditions are always true or false and
  straightening the control flow accordingly.
- **Tail-call optimization** (`ast_tco_run`) — turn a function's tail call
  to itself into a back-edge loop, avoiding a stack frame per call. (When
  this fires it disables promotion and inlining for that function, because
  they would break the back-edge — see the note at `mccast.c` ~5370.)

Then three passes the search tunes more deliberately (each carries a budget
or threshold the search sweeps, even though the passes themselves default on
at their `-O` level):

- **Register promotion** (`MCC_AST_PROMOTE`, §22/§23 machinery via
  `ast_plan_promotion`) — pin a hot local or parameter into a
  callee-saved register for the life of the function instead of keeping it
  in a stack slot. The search bounds how many functions get this
  (`MCC_AST_PROMOTE_LIMIT`) and restricts it to the caller-saved pool when
  the function will also inline, so the two don't fight over the same
  registers.
- **Virtual inlining** (`MCC_AST_INLINE`, the §23 inliner
  `ast_inline_graft`) — splice a called function's body directly into the
  caller, materializing its return value into a freshly carved frame slot,
  so the call overhead disappears. It is deliberately conservative
  (static callees, bounded node count via `MCC_AST_INLINE_NODES`, bounded
  total graft work via `MCC_AST_GRAFT`, limited parameter shapes, excludes
  VLA/setjmp/complex). The search permutes those budgets and the inline
  size limit rather than hardcoding them.
- **The §30 bit-flag transform** (`MCC_AST_BITFLAG`, `ast_bf_run`) — when
  a chain of "is `key` equal to one of these several constants" tests is
  found (a flat n-ary `||` or an else-if chain), it is rewritten into a
  single branchless shift-and-mask test:
  `(int)((MASK >> ((unsigned)key & 63)) & 1) & ((unsigned)key < 64)`.
  The comparison must be the *last* operand evaluated (a subtle flags
  hazard — see below). It only fires above a cluster-size threshold,
  because the mask test has a fixed cost that only pays off past a few
  compares. It defaults **on at `-O2`** with a threshold of 5; the `-O4+`
  search additionally sweeps that threshold over `{0,3,5,9}`.

And one codegen-ordering pass:

- **§35 Sethi–Ullman operand ordering** (`MCC_AST_SETHI`,
  `ast_sethi_run`) — for a commutative, side-effect-free binary node
  (`+ * & | ^`), emit the operand that needs more registers *first*,
  which lowers peak register pressure. Commuting a side-effect-free pair
  is value-preserving for every type (IEEE `+`/`*` commute bit-exactly),
  so no dataflow proof is needed. It skips the swap when either operand is
  a comparison/logical root, because that would leave a pending
  comparison-flags value (`VT_CMP`) for the parent and reordering it
  clobbers the flags — the same hazard the bit-flag transform guards
  against.

**The §32a/§32b/§33a join dataflow** deserves a note because it is what
makes several of the passes above stronger. Ordinary const-prop and CSE reason
along straight-line code. The join passes carry dataflow facts **across
control-flow merges**: `MCC_AST_CPROP_JOIN` (§32a) forks the constant
lattice at each `if`, meets the two arms back together at the join, and
descends into loops with an invariant lattice; `MCC_AST_CSE_JOIN` (§32b)
carries the "which expressions are already available" table down the
dominator tree so an arm inherits what the block before the branch
computed, and runs LICM on that richer table. A third, related pass — the
**§33a cross-`Invoke` availability window** (`MCC_AST_CALL_WINDOW`) — pushes
the same idea across an *un-inlined* function call: normally the optimizer
must assume a call clobbers everything and resets its availability tables at
each `Invoke`, but where the callee is proven not to disturb the relevant
state, §33a keeps those facts live across the call, so a value computed
before a call need not be recomputed after it. All three are now **on by
default from `-O2`** (the §41 sweep); the two join passes stay toggle-able by
the search, and all three preserve the self-host fixpoint whether on or off.

---

## 5. Persistence — the cache under `host_cache_dir()`

A single `-O4` run only has a few seconds. The payoff comes from the search
being **resumable across runs**, and that lives in a per-user cache
directory resolved by `host_cache_dir()` (`$XDG_CACHE_HOME/mcc` or
`$HOME/.cache/mcc` on Linux/BSD, `$HOME/Library/Caches/mcc` on macOS,
`%LOCALAPPDATA%\mcc` on Windows). Three kinds of file live there, all
written as raw fixed-size structs (no parser — a layout or version change
just misses cleanly):

- **`so-<key>.ck`** — the whole-TU search checkpoint (`SoCkpt`). It stores
  the best gate/budget/opt-limit found so far, the best score, and the
  per-dimension search cursors (how far each sweep has progressed, and the
  round counter). The key (`so_key`) is an FNV hash of the input file's
  bytes, the target triplet, and the scoring-mode tag.
- **`pf-<key>.ck`** — the per-function checkpoint (`SoPfCkpt`) used by
  `mcc_superopt_perfn`. Each function is keyed by its §18 **intention
  hash** — a hash of the function's captured tree with all identifiers
  alpha-renamed to positional slots, so structurally identical functions
  share an entry and only an *edited* function misses. The record holds
  that function's best config and a `tried` bitmask of which configs were
  already probed, so a converged function is adopted instantly and an
  edited one re-opens only its own search.
- **`mcchv-<key>.cache`** — the runtime-hypervisor (`tools/mcchv.c`) cache
  entry (§18), keyed by `MCC_VERSION` + triplet + the intention hash of
  the canonical baseline kernel, storing the best params and the search's
  resume state.

**Resumable warm-start.** On startup the search reads its checkpoint and
seeds `best_gate/best_budget/best_limit` and the cursors from it, so run 2
does not restart cold — it continues from run 1's frontier and spends *its*
budget pushing further. The best result is monotonically non-worse across
runs until the space is exhausted, after which hits are instant.

**Durability.** Writes never corrupt a good cached result. Each write
(`so_ckpt_write`, `so_pf_write`) takes an advisory `flock` on a sibling
`.lock` file, re-reads the current record, and does an **A/B keep-best
merge** — it only overwrites the stored best if the new result is genuinely
better (and takes the max of the progress cursors), so a run that made less
progress can't clobber one that made more. The record is then committed
durably: write to a `.tmp` file, `fsync` it, and `rename` it over the
target, so a crash or power loss always leaves either the whole old record
or the whole new one, never a torn fragment. Checkpoints are also written
incrementally during the search, not only at the end, so an interrupt loses
at most the in-flight evaluation.

**Clearing it.** `mcc --clear-cache` removes the whole directory
(`host_rmrf`) and exits.

---

## 6. How it stays safe

The overriding invariant is that turning this machinery on must never
change the *default* compiler. Several things enforce that:

- **The search is opt-in; every pass is individually revertible.** The
  `-O4+` search machinery only starts for `N >= 4` and never runs during an
  ordinary compile. The optimizer passes it permutes each default on only at
  a specific `-O` level (see §4), and every one stays individually
  switchable via `MCC_AST_<name>=0`, so any single pass can be bisected out
  without touching the others. The most speculative passes (§32c/IVSR/PRE/§36)
  stay default-off entirely.
- **Lower levels are unchanged; `-O2+` changed on purpose and stays
  self-consistent.** `-O0` and `-O1` are byte-for-byte what they always
  were. When the §41 sweep turned the always-on passes on at `-O2`, it
  *intentionally* changed `-O2`/`-O3` output — but nothing regressed,
  because the goldens that guard this are **self-consistency** checks, not
  fixed byte sequences: the `dash-s-bytes` test compares a `-c` object
  against the same code assembled from `-S` (a real change moves both
  identically, so it still passes), and the 3-stage self-host fixpoint
  checks that stages 2, 3 and 4 agree (they converge to *new* bytes but
  still agree). The differential fuzzer independently found 0 miscompiles
  over ~900 random programs at the new default.
- **The faithfulness gate.** As in §4, a pass only reoptimizes a function
  whose unchanged replay is byte-for-byte identical to the front-end's
  output. An experimental pass that would diverge simply doesn't run on
  that function.
- **The 3-stage self-host fixpoint.** The compiler compiles itself, then
  that compiler compiles itself again, and again; stages 2, 3, and 4 must
  be byte-identical. Every change to this machinery is validated to hold
  that fixpoint (under the relevant passes both on and off). A pass that
  perturbed codegen non-deterministically would break it and be caught.

Together these mean the superoptimizer can be as aggressive as it likes
inside the search, because a bad candidate can only ever lose on score —
it can never become the shipped default.

---

## 7. Cross-references

- **MCC.md** §9 — the current AST/optimizer behavior reference: the replay
  pass suite, the per-`-O` gating table, and the user-facing flags and
  environment knobs.
- **EXCESS.md** — the deep AST intention-IR design detail (the two-tier IR,
  the pass mechanisms, the cache/search internals) behind §9's summary.
- **TODO.md** — the remaining optimizer-ladder work (the §22/§23/§24/§25/§26
  build-out, §27–§33 frontier, §35/§36 residue).
