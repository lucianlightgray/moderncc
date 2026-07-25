---
name: search-resume-continue
description: The -O>=4 per-function gate search is now resumable/continuable and on by default (MCC_AST_SEARCH)
metadata: 
  node_type: memory
  type: project
  originSessionId: deb64526-2e4b-4138-98cc-4f7258f4bd37
---

Implemented on branch `feat/search-resume-continue` (commit 285c608e, 2026-07-25) in `src/mccast.c`:

- **MCC_AST_SEARCH now defaults ON** whenever there's a search budget (`-O>=4`); still overridable by the env var. At -O0..3 `optimize_search_seconds==0` so codegen is unchanged. This changes default -O>=4 codegen (the per-function combo gate search runs by default) — expect diff/abidiff byte-identity baselines at -O>=4 to shift.
- **CONTINUE** (`ast_search_select`): on a memo hit whose search was left incomplete, seed `best` from the stored winner and re-enter `combo_run`, skipping ordinals already in the record's `tried` mask (`AstComboCtx.skip`) instead of restarting.
- **RESUME/converge**: a COMPLETE latch is persisted in the spare `order_n` memo word (was always 0); complete functions replay their winner (fast, deterministic). Candidate space capped at `AST_SEARCH_CAND_MAX=64` to match the 64-bit `tried`/`skip` bitmask — the precondition for convergence.
- Sound by construction: search only picks configs from `ast_search_searchable`, seeds from the prior winner → a mis-tracked ordinal costs optimality, never correctness.
- Observe with `MCC_AST_SEARCH_VERBOSE=1` (one `[search] continue/store ... COMPLETE|incomplete` line per function) — much cheaper than `-v128`, which is unusable on big TUs.

**arm64 gotcha (cost me a CI red, fixed 2a6da6ea):** on arm64 the runtime mode-6 JIT dispatch/submit path is gated `!ast_search_env` (commit 77bd8ba8 — the search re-emits each candidate to MEMORY and the arm64 GOT/ABS64 dispatch slot corrupts the fn symbol there; "-O4 IS the JIT"). Because MCC_AST_SEARCH now defaults ON at -O4, any -O4 run that wants the runtime submit/override (MCC_JIT_SUBMIT_AOT) must set `MCC_AST_SEARCH=0` or it silently gets static search-opt code and the override never fires (busy_submitted=0). x86_64's gate has no `!ast_search_env` term, so this is arm64-only. This is what actually broke `regression/o4-aot-jit` + `jit-submit-aot-diff` (run 30169564603) — NOT the ROI clock() nondeterminism the earlier 9c3d3930 revert blamed.

Validated: asttool 747/747; benchmarks byte-correct at -O5 with search on; self-host `mcc.c` at -O30 searches 531 funcs to COMPLETE in run 1, byte-identical object on runs 2-3 (deterministic convergence). At -O30 the ~320s wall is the opt pipeline, not the search (search is bounded + skipped on reuse).

Superseded the prior finding that resume was unimplemented (the `tried` field was reserved "for a future unified driver"). Related: [[slice-cache-refactor]], [[moderncc-build-test-loop]]. Self-host compile recipe: extract -D/-I from compile_commands.json for src/mcc.c, pass as argv (mcc `@file` mangles quoted `-D` path macros), `mcc <flags> -O<N> -c src/mcc.c`.
