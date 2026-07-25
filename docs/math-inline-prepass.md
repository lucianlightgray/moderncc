---
name: math-inline-prepass
description: MCC_AST_MATH_INLINE_PREPASS gate — fixes -O4 dropping sqrt/fabs inline + traces local nonneg so sqrt(temp) inlines
metadata:
  node_type: memory
  type: project
---

**Landed 2026-07-25 (commits `1f28ab33` fix a, `ab9822a1` fix b) — `MCC_AST_MATH_INLINE_PREPASS` (default OFF).** TODO item (3)/(g).

Problem: the fabs/sqrt(nonneg)→inline rewrites live inside the `bfold` strategy, so the -O>=4 emit-size search can drop them from its winning order → `sqrt(x*x)` inlines to `sqrtsd` at -O2/-O3 but reverts to `call sqrt` at -O4 (higher -O = worse code).

- **Fix (a):** `ast_math_inline_run` (mccast.c) — standalone pass, same rewrites as `ast_bfold_run`, run unconditionally in `ast_func_end` before the search. A new `math_inlined` flag is threaded through the emit-decision conditions (the pass fired but its AST mutation was being discarded as "nothing changed" — that flag was the missing link).
- **Fix (b):** `ast_local_nonneg` (mccast.c) — `ast_expr_nonneg` proves a Ref-to-local nonneg when the local's address is never taken and it has exactly ONE defining Store whose value is nonneg. Covers `advance()`'s `sqrt(d2)` where `d2 = dx*dx+dy*dy+dz*dz` (single def, sum of squares). Failure mode is errno-only, never a wrong value.

Both gated by `MCC_AST_MATH_INLINE_PREPASS` (the AST_Ref case is skipped when off) ⇒ default -O2/-O4 byte-identical.

**To actually inline sqrt in `advance()` you need BOTH gates: `MCC_AST_OPASSIGN=1 MCC_AST_MATH_INLINE_PREPASS=1`** (OPASSIGN makes advance replayable — see [[opassign-recorder-fix]]; PREPASS does the sqrt-inline). Validated docker/amd64 ([[docker-optimizer-mcc-validation]]): standalone regression fixed; advance sqrt(d2)→sqrtsd at -O2/-O4; nbody bit-matches gcc O0/O2/O4; errno=EDOM preserved; deterministic; byte-identical 3-stage self-host fixpoint; asttool 747/0.

**Caveat (important):** inlining sqrt does NOT close the nbody wall-clock gap — the dominant cost is hot-loop regalloc/LSR (TODO item (2), advance 184 vs gcc 104 insns). This removes the per-iteration sqrt call+spill, a prerequisite. Next: item (2a/2b) IV register-promotion + LSR on advance (now that it's replayable/optimizable).

Remaining before flipping either gate default-on: cross-arch differential + AOT==JIT on arm64 + fuzz soak. Push constraint: [[push-to-main-needs-authorization]].
