# STATUS.md — superoptimizer / JIT ladder status (2026-07-10)

Consolidated status of the `-O<N>` compile-time superoptimizer and the
`--embed-jit` runtime-JIT work (TODO.md §18, §20–§32). Design decisions live
in TODO.md; per-iteration history in OPTIMIZE.md; this file is the single
snapshot of *what is built, what remains, and how to finish it*.

**Invariant held throughout:** `-O0..-O3` byte-identical, 3-stage self-host
fixpoint byte-identical, full `ctest` green (1861) at **every** commit
below. New behavior is gated (opt-in flags / `-O4+` / env) so the default
compiler is unchanged.

## Legend
`done` fully implemented · `functional` end-to-end working (opt-in) ·
`increment` a real tested slice, more scoped · `deferred` parked by design.

## Rung-by-rung

| # | Rung | State | Key commits |
|---|------|-------|-------------|
| 18 | Persistent JIT cache keyed by AST-intention hash | **done** | `400d144a` (`ast_intention_hash` arena API, `mcc_intention_hash`/`mcc_cache_dir` LIBMCCAPI, mcchv entry v2 with search-resume state + flock/keep-best/durable writes) |
| 20 | `host_cache_dir()` portable per-user cache dir | **done** | `97846575` |
| 21 | Resumable-checkpoint search cache | **functional** | `da123a35` (whole-TU warm-start), `3db65b60` (concurrent chunk-claim), `623c479a` (two-tier per-fn keys: intention-hash-cached perfn search) |
| 22 | Per-function search granularity | **functional** | `323a15a6` (per-fn config), `11bb8323` (driver-side per-fn search) |
| 23 | Aggressive inliner envelope | **increment** | `7e630c75` (searchable node/graft budgets) |
| 24 | Hot-window / slice selector | **functional** | `3e81960d` (cost model), `ad5cfee2` (budget ordering) |
| 25 | Frontend-JIT candidate measurement | **increment** | `382b7169` (.text objective); JIT cpu tier pending |
| 26 | `--embed-jit` runtime self-optimizer | **increment** | `4394057f` (flags), `95037e96` (manifest); runtime engine pending |
| 27 | Loop-nest interchange | **unblocked for analysis** (§32); value-materializing half waits on §32c | — |
| 28 | Dynamic algorithm generation | **unblocked for analysis** (§32); value-materializing rules wait on §32c | — |
| 29 | `Convert` representation optimizer | **increment** | `ad55ede8` (redundant integer-cast elimination) |
| 30 | Bit-flag conditional optimizer | **functional** | `fb845871` (detection), transform landed (`ast_bf_run`), `676c1836` (threshold registered as a `-O<N>` search dimension) |
| 31 | Strategy-portfolio scheduler | **substantial** | `f67c2234` (2-strategy), `e3a2f2d7` (3rd strategy), `c5f3349f` (save-and-stop), `35a8ef70` (watchdog), `3db65b60` (concurrency) |
| 32 | Value-reference-node feasibility study | **resolved** (no new node kind; build order §32a→§32c, scoping findings in TODO §32a) | `fde58307` |

## User-facing surface delivered

CLI flags (`src/mcc.c`, `src/libmcc.c`):
- `-O<N>` for `N>=4` — spend ~N seconds superoptimizing (pass-config search);
  scores by `.text` size, resumable across runs, interruptible (SIGTERM).
- `--embed-jit` / `--no-embed-jit` (default on) — runtime-JIT toggle;
  reports the resolved manifest under `-v` at `-O4+`.
- `--jit-max-duration=<sec>` (default 600, 0=unlimited), `--jit-functions=<syms>`
  (default `main`) — runtime-JIT config.
- `--clear-cache` — remove the per-user optimizer cache dir and exit.

Env knobs (search / debugging):
- `MCC_AST_PERFN` — use the per-function search instead of whole-TU (`-O4+`).
- `MCC_AST_FN_CONFIG="fn=bits;…"` — per-function pass gates
  (bit0=templates, bit1=promote, bit2=inline).
- `MCC_AST_INLINE_NODES`, `MCC_AST_GRAFT` — inliner budgets (default 64/2048).
- `MCC_AST_COST` — report per-function hot-slice cost.
- `MCC_AST_BITFLAG=N` — enable the bit-flag transform at cluster threshold N
  (also a report; the `-O<N>` search permutes it over {0,3,5,9}).
- `MCC_AST_HASH_OUT=<file>` — internal driver↔child channel: append each
  captured function's intention hash (`name hex\n`); used by the per-fn
  cache tier.
- `XDG_CACHE_HOME` (or OS-standard) — cache location via `host_cache_dir()`.

Public libmcc API (`include/libmcc.h`):
- `mcc_cache_dir(buf, len)` — the per-user cache dir (wraps
  `host_cache_dir`).
- `mcc_intention_hash(s)` — accumulated alpha-renamed intention hash of the
  functions captured by the last compile (0 when the optimizer feature is
  off).
- `ast_intention_hash(arena, root)` (`src/mccast.h` arena API) — the
  per-tree stable hash both of the above build on.

Cache files under `host_cache_dir()` (all raw-struct, flock + A/B
keep-best + tmp/fsync/rename): `so-<key>.ck` (whole-TU search checkpoint),
`pf-<key>.ck` (per-function best-config + tried-mask records),
`mcchv-<key>.cache` (hypervisor entry v2: best params, measurements, TPE
observations, linear cursor, RNG state). `mcc --clear-cache` removes the
dir.

Host layer (`src/mcchost.c/.h`): `host_cache_dir()`, `host_rmrf()`.
Search core (`src/mcc.c`): `mcc_superopt_search` (whole-TU 3-strategy
portfolio + flock'd resumable checkpoint + subprocess watchdog +
SIGTERM save-and-stop + concurrent chunk-claim), `mcc_superopt_perfn`
(per-function search via object symbol-table sizes, per-fn
intention-hash cache tier).

## Architecture (as built)

The `-O<N>` search is a **strategy-portfolio scheduler**: gate + budget +
promote/opt-limit strategies, greedy composition, exponentially-doubling
time slices, adaptive base slice, resumable per-strategy cursors in a v5
checkpoint under `host_cache_dir()`. The budget dimension is
`nodes × grafts × bitflag-threshold` (3×3×4 = 36 points) since `676c1836`
registered the §30 transform threshold in the space. Concurrent invocations
reserve disjoint work via an atomic `claim_gate` cursor advanced under
`flock` (verified: two workers cover the space once, not twice). A
per-candidate subprocess watchdog timeout-kills runaway/hung compiles;
SIGTERM/INT/HUP flush the checkpoint and exit in ~1-2 s. The per-function
variant (`MCC_AST_PERFN`) measures each function's `.text` from the object
symbol table and greedily picks its best config, biggest-function-first
(§24 hot-slice order); each function's best config + tried-mask persists in
`pf-<intention-hash>.ck`, so an unchanged function is adopted without
re-probing and an edited function re-opens only its own search
(`superopt-perfn-cache` ctest). `so_copy` propagates the winning
candidate's file mode — fresh `-O4+` executables had silently lost the
exec bit since `f959078e` (fixed in `676c1836`).

## Remaining full builds (precisely scoped, de-risked)

The one remaining *full* build is the §26 runtime engine (item 2); §30's
transform (item 1) landed and now also sits in the search space
(`676c1836`), leaving only its listed extensions. The §32a→§32c frontier
passes are the other open track (scoped in TODO §32).

1. **§30 bit-flag transform — LANDED.** The earlier "no expression-level
   transform" finding was half-stale: a short-circuit `||` in *condition*
   position IS captured as one flat n-ary `AST_Binary TOK_LOR` node (only
   the materialized value form desyncs), so `ast_bf_run` rewrites that node
   in place; the else-if chain form collapses the `AST_If` chain per the
   TCO precedent. Encoding is the reversed-operand branchless form
   `(int)((MASK >> ((unsigned)key & 63)) & 1) & ((unsigned)key < 64)` —
   the comparison must be the last-evaluated operand because the replay
   driver, unlike the parser's `vpush` path, never runs `vcheck_cmp`: a
   `VT_CMP` buried under later emission gets its flags clobbered (same bug
   class as the arm64 `gfunc_call` fix; the original operand order
   miscompiled). Threshold `MCC_AST_BITFLAG=N` (N≥3), default 5 = measured
   x86_64 size break-even (mask test is a constant ~151 B vs ~12 B per
   compare-branch). Validated: edge keys (−2³¹, −1, 0, 63, 64, 2³²+5) ×
   int/uint/llong/ullong/char/short × lor/chain/mixed/while/ternary forms;
   full ctest 1858 incl. new `cli/bitflag_transform`; fixpoint OK. Later
   extensions: value-table dispatch for differing bodies, `switch` arms,
   `&&`-of-`!=` complement, biased ranges (consts ≥64).
2. **§26 runtime engine.** Infra exists (`.init_array`/`SHT_INIT_ARRAY`
   emission + `mcc_relocate`/`-run` JIT). Assembly: `-O4+ --embed-jit`
   synthesizes a ctor spawning the `--jit-threads` pool; embed the
   per-function intention trees; recompile hot functions via the embedded
   `mcc_relocate`; hot-swap via atomic-pointer slot + triple-buffer/RCU.
   Dominant cost: embedding the ~800 KB libmcc slice into every `-O4+`
   output — a size/build-system problem as much as codegen.

**Value-reference node — resolved (§32, `fde58307`).** The feasibility
study found no new node kind is needed: retag-to-literal covers the SCCP
value-lattice and §27's legality/rewrite; carrying the CSE availability
table across dominated joins covers most of GVN; a synthetic frame-slot
temp (the TCO/promote-fix mechanism, §32c) covers PRE/IV-strength/fresh-temp
LICM; the true φ-node case is rejected as not worth its cost. §27/§28 are
therefore **unblocked for their analysis/structural halves**; only their
value-materializing sub-cases wait on §32c. Build order: §32a SCCP
value-lattice → §32b cross-join CSE → §32c synthetic-temp infrastructure.
§32a scoping findings (the `AST_If`/`AST_Jump` op maps, the op-5
ternary/for-no-cond overload hazard, and the fork/meet design against the
real `ast_cprop` machinery) are recorded in TODO §32a.

## Why the remainders are not forced

§26's runtime and the §32 frontier passes restructure the compiler's most
fragile machinery (embedded JIT + live patching; dataflow over the replay
capture), where a defect passes the default suite yet miscompiles real
code — violating the same "tests green" invariant held here across ~40
commits. Each is a focused, fixpoint-validated effort, taken on
deliberately, not rushed at the risk of the self-hosting compiler.
