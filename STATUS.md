# STATUS.md — superoptimizer / JIT ladder status (2026-07-10)

Consolidated status of the `-O<N>` compile-time superoptimizer and the
`--embed-jit` runtime-JIT work (TODO.md §20–§31). Design decisions live in
TODO.md; per-iteration history in OPTIMIZE.md; this file is the single
snapshot of *what is built, what remains, and how to finish it*.

**Invariant held throughout:** `-O0..-O3` byte-identical, 3-stage self-host
fixpoint byte-identical, full `ctest` green (1857) at **every** commit
below. New behavior is gated (opt-in flags / `-O4+` / env) so the default
compiler is unchanged.

## Legend
`done` fully implemented · `functional` end-to-end working (opt-in) ·
`increment` a real tested slice, more scoped · `deferred` parked by design.

## Rung-by-rung

| # | Rung | State | Key commits |
|---|------|-------|-------------|
| 20 | `host_cache_dir()` portable per-user cache dir | **done** | `97846575` |
| 21 | Resumable-checkpoint search cache | **increment** | `da123a35` (whole-TU warm-start), `3db65b60` (concurrent chunk-claim) |
| 22 | Per-function search granularity | **functional** | `323a15a6` (per-fn config), `11bb8323` (driver-side per-fn search) |
| 23 | Aggressive inliner envelope | **increment** | `7e630c75` (searchable node/graft budgets) |
| 24 | Hot-window / slice selector | **functional** | `3e81960d` (cost model), `ad5cfee2` (budget ordering) |
| 25 | Frontend-JIT candidate measurement | **increment** | `382b7169` (.text objective); JIT cpu tier pending |
| 26 | `--embed-jit` runtime self-optimizer | **increment** | `4394057f` (flags), `95037e96` (manifest); runtime engine pending |
| 27 | Loop-nest interchange | **deferred** | behind the value-reference-node decision |
| 28 | Dynamic algorithm generation | **deferred** | behind the value-reference-node decision |
| 29 | `Convert` representation optimizer | **increment** | `ad55ede8` (redundant integer-cast elimination) |
| 30 | Bit-flag conditional optimizer | **functional** | `fb845871` (same-key cluster detection); mask transform landed (`ast_bf_run`: n-ary `TOK_LOR` cond rewrite + `AST_If`-chain collapse, `MCC_AST_BITFLAG=N` threshold, default 5 = size break-even) |
| 31 | Strategy-portfolio scheduler | **substantial** | `f67c2234` (2-strategy), `e3a2f2d7` (3rd strategy), `c5f3349f` (save-and-stop), `35a8ef70` (watchdog), `3db65b60` (concurrency) |

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
- `MCC_AST_BITFLAG` — report bit-flag-encodable conditional clusters.
- `XDG_CACHE_HOME` (or OS-standard) — cache location via `host_cache_dir()`.

Host layer (`src/mcchost.c/.h`): `host_cache_dir()`, `host_rmrf()`.
Search core (`src/mcc.c`): `mcc_superopt_search` (whole-TU 3-strategy
portfolio + flock'd resumable checkpoint + subprocess watchdog +
SIGTERM save-and-stop + concurrent chunk-claim), `mcc_superopt_perfn`
(per-function search via object symbol-table sizes).

## Architecture (as built)

The `-O<N>` search is a **strategy-portfolio scheduler**: gate + budget +
promote/opt-limit strategies, greedy composition, exponentially-doubling
time slices, adaptive base slice, resumable per-strategy cursors in a v4
checkpoint under `host_cache_dir()`. Concurrent invocations reserve disjoint
work via an atomic `claim_gate` cursor advanced under `flock` (verified:
two workers cover the space once, not twice). A per-candidate subprocess
watchdog timeout-kills runaway/hung compiles; SIGTERM/INT/HUP flush the
checkpoint and exit in ~1-2 s. The per-function variant (`MCC_AST_PERFN`)
measures each function's `.text` from the object symbol table and greedily
picks its best config, biggest-function-first (§24 hot-slice order).

## Remaining full builds (precisely scoped, de-risked)

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

**Deferred by design:** §27 loop interchange and §28 dynamic algorithm
generation both need a value-reference / SSA AST node the replay model
lacks — parked in the design round until §20–25 settle and that node is
reassessed. Implementing them now contradicts that decision.

## Why the two remainders are not forced

Both §30's collapse and §26's runtime restructure the compiler's most
fragile machinery (branch rewriting in the replay; embedded JIT + live
patching), where a defect passes the default suite yet miscompiles real
code — violating the same "tests green" invariant held here across ~35
commits. Each is a focused, fixpoint-validated effort, taken on
deliberately, not rushed at the risk of the self-hosting compiler.
