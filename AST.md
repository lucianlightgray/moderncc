# The AST / Optimization Pipeline

Status snapshot: 2026-07-28.

## The core idea

There is no separate IR and no separate optimize phase. `src/mccgen.c` parses and
emits machine code in one pass; while that happens, ~90 `ast_hook_*` callbacks
mirror the codegen events into a side arena. That arena is "the AST".
Optimization means: mutate the arena, rewind the text cursor, and re-drive the
*same* emitters from the arena.

**The safety invariant is one `memcmp`** — `src/mccast.c:16457-16465`. At the end
of every function body the parser's bytes are snapshotted, `ind` is rewound to
`ast_body_ind_sv`, and the arena is replayed via `ast_replay_body`
(`src/mccast.c:6219`). Only if the replay is byte-identical — and relocations are
structurally equal via `ast_reloc_range_equiv` (`src/mccast.c:13975`) — is the
function marked `faithful` and allowed to be optimized. Not faithful means
everything rolls back (`src/mccast.c:17076-17085`) and the -O0 emit ships.

A replay bug therefore costs coverage, never correctness. Replay bugs are safe;
model bugs are not.

## Pipeline order

| # | Function | Location |
|---|---|---|
| 1 | `main` | `src/mcc.c:1426` |
| 2 | `mcc_parse_args` | `src/mcc.c:1444` |
| 3 | superopt dispatch (`-O<N≥4>` only) | `src/mcc.c:1522-1548` |
| 4 | `mcc_add_file` → `mcc_compile` | `src/mcc.c:1553`, `src/libmcc.c:820` |
| 5 | `preprocess_start` | `src/mccpp.c:4239` |
| 6 | `mccgen_init` | `src/mccgen.c:567` |
| 7 | `mccgen_compile` | `src/mccgen.c:587` |
| 8 | `mcc_output_file` / `mcc_run` | `src/objfmt/mccelf.c:3031` / `src/mccrun.c:121` |

Inside `mccgen_compile`:

- `ast_configure(s1)` — `src/mccgen.c:597` → `src/mccast.c:1985`. Reads `-O` and
  every env knob, sets all optimizer gates.
- `next(); decl(VT_CONST);` — `src/mccgen.c:607-608`. The entire parse and emit.
- `ast_reemit_forward_inlines()` — `src/mccgen.c:609`. Second-chance re-emit of
  functions whose callee only became graftable later.

Per function, `gen_function` (`src/mccgen.c:13077`) brackets the body:

```
mccgen.c:13133   ast_func_begin(sym)     -> mccast.c:14024
mccgen.c:13134   block(0)
mccgen.c:13135   ast_func_end(sym)       -> mccast.c:16356   (the whole optimizer)
mccgen.c:13151   ast_func_epilog()       -> mccast.c:17201
```

The substrate is off entirely at -O0: `ast_replay_env = s1->optimize >= 1 ||
s1->embed_jit` (`src/mccast.c:1996`). It is also disabled under `-g` /
`-ftest-coverage` and for `inline extern`.

## Arena structure

`struct AstArena` — `src/mccast.c:48-67`. Struct-of-arrays: 13 parallel heap
arrays (`kind`, `parent`, `first_child`, `last_child`, `next_sib`, `nchild`,
`op`, `type_t`, `type_ref`, `ival`, `fbits`, `sym`, `cst`) plus `count`, `cap`,
`epoch`.

- A node is a `uint32_t` index (`AstLocal`, `src/mccast.h:38`); sentinel
  `AST_NONE = 0xffffffff`.
- 16 node kinds — `AstKind`, `src/mccast.h:9-36`. `AST_BasicBlock` is always
  index 0 (the root).
- Allocation is bump-append (`ast_node`, `src/mccast.c:249`); growth is doubling
  realloc of all 13 arrays (`AST_REGROW`, `:69-92`); clone is flat memcpy
  (`ast_arena_clone`, `:138`).
- **It must stay a tree.** `AST_StoreVal` exists to carry a producing Store's
  index in `ival` rather than a back-pointer, avoiding a DAG edge. `ast_treechk`
  (`:6138`, env `MCC_AST_TREECHK`) guards the hazard, backed by the
  `ast/treecheck` CTest.

### Recording

Roughly 90 `ast_hook_*` callbacks (declared `src/mccast.h:202-278`, ~91 call
sites in `mccgen.c`) observe the single pass without suppressing it. The
recorder mirrors mcc's `vstack` with a shadow stack `ast_vs[64]` /
`ast_vn` (`src/mccast.c:1761`); any index mismatch sets `ast_desync` via
`AST_SET_DESYNC` (`:1784`), which records `__LINE__` so verdicts read
`desync:<line>`.

Representative hooks: `ast_hook_vpush` (`:2295`), `ast_hook_genop` (`:2392`),
`ast_hook_call_begin/_end` (`:3331`), `ast_hook_if_begin/_else/_end` (`:2721`),
`ast_hook_vstore` (`:3445`). `ast_hook_bail` (`:2958`) marks a function
un-replayable outright (switch jump tables, `va_arg`).

### Replay

`ast_replay_body` → `ast_replay_bb` (`:5794`) and `ast_replay_value` (`:5436`).
Replay does **not** emit machine code directly — it drives the same backend
entry points the parser uses (`vpushv`, `gen_op`, `vstore`, `gfunc_call`, `gsym`,
`gjmp`), which is why an identical tree reproduces identical bytes.

Guard: `ast_replay_ok` (`:13928`) requires `!ast_bail && !ast_desync && ast_vn ==
0 && ast_cf_top == 0` and a non-empty root.

Other replay drivers: `ast_reemit` (`:17207`, whole function from a retained
arena), `ast_reemit_with_gates` (`:17319`, the JIT path), and the `AST_PF_EMIT`
macro (`:16650-16687`, the production optimized-emit path).

### Side-cars and epoch invalidation

`AstArena::epoch` is bumped by every mutator. Each side-car stores
`(arena_ptr, epoch)` and rebuilds lazily when either differs.

| Side-car | Sync | Purpose |
|---|---|---|
| def/use table | `src/mccast.c:4617` | per-slot WRITTEN/ESCAPED bits, cap 2048, `-1` = overflow → rescan |
| property memos | `:4668` | 4 monotone subtree predicates |
| structural hash | `:4746` | O(1) fast-reject for `ast_ident_same` |
| value lattice | `:10170` | range / known-bits per slot |
| loop nest | `:12417` | loop structure |

`MCC_CONFIG_AST_SHADOW` (CMake, default OFF) is a **differential-oracle build**:
every cached answer is recomputed the slow way and any disagreement prints
`mcc: AST side-car divergence:` and aborts.

### Frame slots

Frame layout is a recorded tape, not recomputed.

- `ast_alloc_loc` (`src/mccast.c:1831`) — during capture does the normal
  `loc = (loc - size) & -align` and appends to `ast_locrec[]`; during replay it
  *replays the tape by index*, so declared locals land at identical offsets.
- `ast_alloc_temp_loc` (`:1853`) — replay-time temporaries (spills, grafted
  inline frames, promo save slots) come from a separate monotone frontier seeded
  below `min(ast_locrec_min, loc)` so they cannot collide with tape slots.
- `ast_loc_low` is the low-water mark observed during replay; `AST_PF_EMIT` folds
  it back into `loc` to size the frame (`:16657-16683`).

Regression coverage: `tests/ast/frame_regress.cmake`.

## Optimization passes

### Strategy table — 21 entries

`ast_strategies[]` at `src/mccast.c:14152` (enum `:14126`, struct `AstStrategy`
`:14079`). Driven by `ast_run_strat_cycle` (`:16558`), which runs to a fixpoint
at -O3 (`MCC_AST_CYCLE`).

`bfold, ident, narrow, cprop, cse, ltemp, ivsr, pre, licm, dse, sccp, jt, bf,
range, divmagic, abs, select, reassoc, sethi, tco, inline`

Selected knobs and defaults:

| Knob | Transform | Default |
|---|---|---|
| `MCC_AST_TEMPLATES` | master gate for bfold/ident/cprop/cse/licm/dse/sccp/jt/tco | O1+ |
| `MCC_AST_NARROW` | width narrowing of int ops | O1+ |
| `MCC_AST_SETHI` | Sethi–Ullman commutative-operand reorder | O1+ |
| `MCC_AST_RANGE` | `lo<=x && x<=hi` → one unsigned compare | O1+ |
| `MCC_AST_PRE` | partial redundancy elimination | O1+ |
| `MCC_AST_IVSR` | induction-variable strength reduction | O1+, not `-Os` |
| `MCC_AST_BITFLAG` | if / `\|\|` cluster → bit-flag mask test | O2+ |
| `MCC_AST_DIVMAGIC` | `x/C`, `x%C` → high-mul + shift | O2+ |
| `MCC_AST_ABS` | `x<0?-x:x` → branchless abs | O2+ |
| `MCC_AST_SELECT` | ternary / if → select / cmov | O2+ |
| `MCC_AST_REASSOC` | `(x OP c1) OP c2` → `x OP combine(c1,c2)` | O2+ |
| `MCC_AST_INLINE_PASS` | AST-level inline | O2+ |
| `MCC_AST_CYCLE` | run the pipeline to a fixpoint | O3+ |
| `MCC_AST_INLINE` | superopt-style graft inliner | O3+, not `-Os` |

`licm`, `dse`, `sccp`, `jt`, `cprop`, `cse`, `bfold`, `ident`, `tco` have no
individual knob — they ride `MCC_AST_TEMPLATES`.

PRE (`ast_pre_run`, `:13623`) is real but narrow: only the `AST_If`-with-two-arms
diamond, hoisting a `Binary` RHS out of a store when it occurs in both arms,
capped at 32 temps, skipping arms containing labels.

### Loop transforms — 3, outside the table

| Knob | Impl | Default |
|---|---|---|
| `MCC_AST_INTERCHANGE` | `ast_interchange_run` `:13290` | O2+ |
| `MCC_AST_FUSION` | `ast_fusion_run` `:13410` | O2+ |
| `MCC_AST_TILE` | `ast_tile_run` `:13571` | O2+ |

These run **once as a pre-pass** at `src/mccast.c:16515-16533`, before the search
and the strategy cycle. They are not in `ast_strategies[]`, have no `AST_SG_*`
bit, and are therefore invisible to the -O4 gate search.

### Register allocation

Two allocators.

**Baseline (all -O levels).** A linear scan of the live vstack, not a graph
algorithm: `get_reg` (`src/mccgen.c:1608`) picks the first register in class `rc`
not referenced by any live `vstack` entry, spilling via `save_reg` (`:1477`) if
none. Spill slots come from `get_temp_local_var` (`:1652`) → `ast_alloc_temp_loc`.

**Promotion (-O2+, x86_64 / arm64).** `ast_plan_promotion` (`src/mccast.c:4889`)
picks scalar locals to live in registers. Live ranges are first/last `AST_Ref`
extended over enclosing loops; interference is overlapping `[lo,hi]`. Assignment
is a Chaitin/Briggs simplify-stack colorer, `ast_color_graph` (`:1168`, cap 64
nodes, `MCC_AST_COLOR`, O2+), falling back to greedy-by-weight.

Per-arch pools are split caller-saved / callee-saved / XMM (`:3628-3686`).
riscv64 deliberately has no caller pool — pinning a0-a7 makes `get_reg` return
-1 and crash `freg()`.

**The coupling is one global.** Promotion is not a lowering; it is expressed
purely as "hide these registers from `get_reg`":

- `AST_PF_EMIT` sets `ast_pinned_regs` per promoted local (`:16676`) and calls
  `ast_promo_entry_init` (`:5330`) to save incoming callee-saved values.
- The baseline allocator skips pinned registers in both the free scan and the
  spill-victim scan (`src/mccgen.c:1613-1615`, `1636-1645`, `1588-1591`).
- `ast_func_epilog` runs `ast_promo_exit_restore` (`:5388`) and clears the pins.

### Gate vocabulary

Bits are `#define AST_SG_*` in `src/mccgate.h:25-114` — a macro list, not a
table. Defaults are a straight-line block of `ast_env_gate("MCC_AST_X",
default_expr)` assignments in `ast_configure` (`src/mccast.c:1996-2255`), each
default an expression over `-O` level, `-Os`, and `int o4 = optimize_search_seconds > 0`.

Flags↔mask conversion is two hand-written 35-term chains, `ast_search_gates_now`
(`:15176`) and `ast_search_set_gates` (`:15215`), which must be kept in sync with
the macro list by hand.

Known gaps:

- `AST_SG_JIT_DISPATCH` / `AST_SG_JIT_GUARD` are defined but referenced nowhere.
- `AST_SG_PROMOTE/INLINE/NOCALLFUL/CPROPJOIN/CSEJOIN` (bits 6-10) are
  bridge-only — converted for the out-of-process superopt but not read by the
  in-process search.
- `MCC_AST_SELECT`, `MCC_AST_INLINE_PASS`, `MCC_AST_NARROW_ELIM`,
  `MCC_AST_SETHI_NARY`, `MCC_AST_CALL_WINDOW`, `MCC_AST_VLAT`,
  `MCC_AST_IVSR_PTR`, `MCC_AST_ARGFWD` have no gate bit at all.
- There is **no peephole optimizer**. The real split is AST-level rewrites
  (`ast_strategies[]` + loop transforms) vs replay/emit-level knobs
  (`PROMO*`/`COLOR`/`SPILL_SHARE`/`REGDISP`/`XMM_HI`/`FMOV_IMM`/`CHAINSTORE`),
  which change register allocation and addressing rather than the tree.
- Three recorder-fidelity knobs (`MCC_AST_CMP_INVERT`, `MCC_AST_MEMBER_AGG`,
  `MCC_AST_MEMBER_CONST`) are lazily-cached `getenv` statics outside
  `ast_configure`, so they are on at every -O level including -O0.

## Search (-O4+)

`-O<N>` with `N>3` sets `optimize = 3` and `optimize_search_seconds = N`
(`src/libmcc.c:2951-2961`). **N is literally the search budget in seconds** —
codegen stays at -O3. The `o4` term also flips ~50 defaults on.

### A. Out-of-process superopt driver

`mcc_superopt_search` (`src/mcc.c:1213`), per-function variant
`mcc_superopt_perfn` (`:1083`). Dispatched from `main` (`:1523`) only when
`MCC_SEARCH_WORKER` is unset; every spawned child gets `MCC_SEARCH_WORKER=1`,
which is the recursion guard.

Search space is a 3-axis product of environment configs, re-exec'ing the
compiler once per candidate: gate (2576 points), budget (144), limit (5).
Objective is linked `.text` bytes (`so_textsize`, `:637`), or measured `-run`
wall µs under `MCC_AST_JITSCORE`. Walk is coordinate descent in rounds with a
time slice per axis, capped at `remaining/3` to avoid starving later axes.

### B. In-process per-function gate search

`ast_search_select` (`src/mccast.c:16040`), entered from `ast_func_end` only for
byte-faithful functions.

Search space is the subset lattice of `ast_search_searchable` (`:15899`) — the
baseline gates plus the opt-in knobs the search may *add*. Objective is
`ast_cost_score` (`:9313`, `nodes * (maxdepth+1) * (calls+1)`), or emitted bytes
under `MCC_AST_SEARCH_EMITSIZE` by replaying into a private scratch section
(`:15258-15338`). Scores are packed so equal-cost configs tie-break toward the
one whose strategies fired more.

Walks live in `src/mcccombo.h`: LINEAR (default), DFS (`:130`), BFS (`:143`),
PRODUCT (`:166`); selected by `MCC_AST_SEARCH_WALK`.

**The search only selects a gate config** (`src/mccast.c:14247-14255`). The
winner is emitted by the normal pipeline on the untouched captured tree, so a
search bug can pick a worse config but never a miscompile.

### Parallelism

`ast_search_pool` (`:15594`) forks `nproc-1` children (min 2, max 64), each
taking candidates strided by worker index and writing `{idx, score}` to a pipe.
**Fork COW is the isolation mechanism** — `ast_search_score_one` mutates
process-global optimizer state.

`ast_search_pool_pthreads` (`:15680`, default off) is the same shape in a shared
address space and is documented as **known-incorrect**; it exists to enumerate
races under TSan before the state is made `_Thread_local`.

### Resumable cache

`$XDG_CACHE_HOME/mcc` (`host_cache_dir`, `src/mcchost.c:1033`); `--clear-cache`
at `src/mcc.c:1478`.

- `mcc-search.memo` — compressed container (best-of-3 rle/lzss/lzw), 7-word
  records. Key is `ast_intention_hash` of the pristine arena, FNV-folded with
  `MCC_VERSION_STR` and `MCC_CONFIG_TRIPLET` so an incompatible build or target
  can never reuse a winner. A budget-truncated run stores a `tried` bitmask; the
  next run seeds `best` and skips already-measured ordinals. `order_n == 1` is
  the COMPLETE latch. Eviction drops the lowest-refcount quarter at 10 GiB.
- `so-<key>.ck` / `pf-<key>.ck` — superopt checkpoints, merged under a file lock;
  `so_claim` hands out 64-index chunks so concurrent builds cooperatively
  partition the gate space.
- `sl-<salt>.ck` — the JIT↔AOT slice cache, salted with version only (no
  triplet), safe because the stored value is re-intersected with the current
  target's `searchable`.

### Forecast

`src/mccforecast.h` predicts the duration in ms of the next search tick from a
rolling 10-sample window. It is a 13-model ensemble (`rw, ses, ar1, lin,
pspline, gam, bsts, bridge, gp, gbm, holt, theilsen, movmed`), each scored by
online one-step accuracy over the window; the 3 most accurate are taken and the
one closest to their median is returned. Self-contained — own `exp`, own median,
own Gaussian elimination, no libm.

`ast_search_should_stop` (`src/mccast.c:14934`) stops when
`expect_ms() > remaining` — i.e. *before* starting a tick predicted not to
finish, rather than overrunning. A rejected candidate is not marked `tried`,
which is what makes resume correct.

### --stats

`mccstats.c` paints a live TTY panel (throttled to one repaint per 50 ms) with
sections for SEARCH, COMBO (including a 40-sample Unicode sparkline of recent
scores), STRATEGY fire counts, FOLD cycle stats, a GATES on/searchable map, JIT
counters, and the WINNER. Fed by push callbacks from the search, all guarded by
`if (mcc_stats_mask)`. Forked children never paint (`mcc_stats_owner_pid`).

## JIT reuse

`ast_reemit_with_gates` (`src/mccast.c:17319`) is the single entry point AOT and
JIT both call. It clones the arena, sets the gate mask, runs the identical
`ast_run_strat_cycle`, then `ast_reemit`.

**Intent blobs** (`src/mccjit_intent.c`) are pointer-free serialized snapshots of
one function's arena plus its signature and type graph — everything needed to
recompile it in another process. Every `Sym*`/`CType.ref` is interned into a
handle table. `warm_gates` carries the AOT-chosen gate mask forward so the JIT
search warm-starts.

**Dispatch mode 6** (default under embed-JIT or `-run`) replaces the function
entry with `jmp *slot(%rip)` through a `.data` slot and re-splices the AOT bytes
after it, so the body can be swapped atomically.

**Recompile triggers**: a process-start constructor walking `__mccjit_registry[]`;
a hotness counter stub (default threshold 1000); and profile-guided
respecialization on a dominant constant argument.

**Deopt is a differential stub, not on-stack replacement.** The published entry
is a KGC (known-good cache) stub: memo hit → variant; else run both variant and
baseline, compare, memoize on agreement; **on mismatch return the baseline's
answer and count a miss**. Enough misses poisons the variant permanently.
Memoization requires purity tier 0 and a scalar signature ≤6 args.

**Embedding** (`MCC_EMBED_JIT`): the engine is built as `libmcc_jitengine.a`,
converted to a C blob by `tools/bin2c`, and compiled into `mcc`. At link time
`mccjit_embed_finalize` (`src/mccjit_embed.c:1865`) copies each intent blob into
`.data`, **generates C source as a string** declaring the registry and boot
constructor, and feeds it back through `mcc_compile_string`. Functions named
`mccjit_*` / `mcc_jit_*` are never baked, or a self-hosted `mcc` would recurse
into its own swap machinery.

## Verification

The `memcmp` above is always on and not gated. Layered on top:

- `MCC_AST_VERIFY` — per-function verdict line, one of
  `faithful | desync:<line> | empty | bail | stackresidue | unfaithful`.
  `=2` escalates a gap to an error.
- `MCC_AST_VERIFY_DIFF`, `MCC_AST_UNFAITHFUL_DUMP` — byte windows and first
  differing offset.
- `MCC_AST_TREECHK` — tree-shape check with a faithful/unfaithful phase label.
- `MCC_AST_HASH_OUT` — identity-level rather than byte-level comparison; the
  paired call sites at `src/mccast.c:16366` (AOT) and `src/mccjit_embed.c:449`
  (JIT) are the one diffable AOT-vs-JIT comparison point.
- `tools/tracediff.sh` — diff two configs' TRACE logs to find where they split.

CI ratchets: `ast-verify-ratchet` sweeps `tests/exec/*.c` at -O2 and diffs the
non-faithful set against a checked-in per-target baseline; the gap set may only
shrink. `tests/ast/replay.cmake` uses -O0 execution as the behavioural oracle and
asserts the named function actually took the replay/promote/inline path.

## Current state

**Fidelity** on mcc's own amalgamated TU at -O2 (the mandated corpus;
`tests/exec` reads several points high):

- 1438 faithful / 207 unfaithful / 206 desync of 1851 = **77.7%**.
- History: 59.5% at session start → 75.3% (2026-07-27) → 77.7% after the
  `OPASSIGN` -O3→-O2 restage. Non-faithful ceiling 48.5% → 41.0%.

**The 207 unfaithful**, attributed:

- 45 (22%) — chained assignment. `a = b = 0` emits both stores but never
  materializes the RHS into the register; 5 missing bytes.
- 24 (12%) — `nocode_wanted` / noreturn dead regions. Replay emits a `jmp rel32`
  the parser suppresses.
- 90 of the 92 same-length byte-differs — pure reorder or register-name
  canonicalization, not model bugs.
- **2 — a genuinely lost sign extension.** Parser emits `movslq`, replay emits
  `movzbl` (`imm_ext`/`modrm` in `x86_64-dis.c`). Repro:
  `long long v = (signed char)g8();` is unfaithful, `(unsigned char)` is
  faithful. Harmless *only* because the comparison rejects it — anyone who fixes
  `imm_ext`/`modrm` into faithfulness without fixing the extension ships a
  miscompile.

**Desync sites**: `call_begin`/nocode 89; `vpush` vstack SYNC 43; `cmp_invert` 24
(all `TOK_LAND`/`TOK_LOR`); `vstore` ternary/landor 15; `call_begin` callee-kind
14; `vpush` value-model 4 (was 119). `AST_VS_MAX`=64 and its capacity arm fires
0 times of 214 — do not raise it.

**-O4 is not paying for itself**: 8.384 s and 26 funcs/s for 94.1 KB, vs -O3 at
0.020 s and 10190 funcs/s for 102.5 KB — **~420× the compile time for ~8%
smaller output**, and measured 16% *slower at runtime* before the PROMOTE floor.
The in-process score is a speed proxy being used where size is wanted.
`MCC_AST_SEARCH_FLOOR` was the attempted fix and explicitly did not work.
Warm -O4 compiles also cost the same as cold — 8 successive runs all ~4.7 s for a
byte-identical binary, with convergence ~35 compiles away.

**Other open items**: the JIT→AOT slice-cache graduation path is unit-proven only
(all observed records have `proven=0`); the superopt driver discards a better
in-process result; PE x86_64 runtime JIT faults with `0xC0000005`; sub-slice
promotion is blocked on a missing Optimization Reconciler.

Open work is tracked in `docs/TODO.md` — see the `AST recorder fidelity — INDEX`
header and the F1-F10 follow-up keys.
