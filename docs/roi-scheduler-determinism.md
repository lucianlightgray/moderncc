---
name: roi-scheduler-determinism
description: ROI strategy scheduler (MCC_AST_ROI) — determinism landed upstream; I fixed the ROI+emitsize miscompile
metadata: 
  node_type: memory
  type: project
  originSessionId: ce8a644d-b75b-482c-95d6-97e871b8dc73
---

The `MCC_AST_ROI` strategy scheduler (`ast_search_roi_order`, mccast.c) orders the optimizer round-robin by ROI = benefit/cost. It once used `ast_now_ms()` wall-clock as the cost denominator, which was non-deterministic.

**Determinism fix landed UPSTREAM as `4ccc3860`** (not my commit — a parallel session/the user pushed it while my loop ran). Upstream uses the **transform-count delta** (`ast_graft_total`+`ast_promo_total`+`ast_opt_total` around each `.apply`) as the deterministic proxy, and enabled `MCC_AST_ROI` **default-ON at -O>=4** (`optimize_search_seconds>0`). My own local commit `0ebd6fc9` used a `trial->epoch`-delta proxy for the same effect — redundant, kept in branch `roi-epoch-superseded` (safe to delete).

**Corrected root cause (from upstream's TODO):** the arm64 CI red (run 30169564603) was NOT ROI clock() nondeterminism — it was `672b4ffb` flipping `MCC_AST_SEARCH` default-on, which on arm64 suppresses the `!ast_search_env`-gated mode-6 JIT dispatch those two regression tests assert fires; fixed in `2a6da6ea` by pinning `MCC_AST_SEARCH=0` in the tests. The `9c3d3930` ROI-revert was a misdiagnosis.

**My contribution this loop = the ROI+emitsize soundness fix (commit `4a5b416a`, local, push pending auth):** `MCC_AST_ROI=1` + `MCC_AST_SEARCH_EMITSIZE=1` MISCOMPILED (struct-free int loop → 1163150798 vs correct 1126360398) because `ast_search_roi_order` called `ast_search_emit_size` on a clone to score benefit and that emit-size probe replays to MEMORY / is not side-effect-free when called speculatively. Fix: ROI benefit now always uses the pure `ast_cost_score`. Byte-identical to 4ccc3860 for every non-emitsize config (default ROI already took the cost branch since EMITSIZE defaults off); only the previously-broken ROI+emitsize object changes. Removes a footgun for the item-105 `_EMITSIZE` flip.

**Phase-2 (b)/(d) ROOT-CAUSED (commit `ea14ad0b`, local, push gated):** `advance()` (nbody hot fn) is absent from the ROI dump NOT because the scheduler skips it — it never enters the optimize path at all. `ast_replay_ok(ast_cur)` returns 0 for it in `ast_func_end` (mccast.c:14666), so faithful/search/ROI/strategy all skip and the un-optimized baseline is kept. The old "advance IS faithful" note was a misread (it's absent from `MCC_AST_VERIFY_DIFF=all` because it bails in `ast_replay_ok` BEFORE the diff check). Instrumented (docker/amd64 optimizer mcc, `-O2`): desync first set at `ast_hook_vpush` (mccast.c:1993) on an operand `r=VT_LVAL|reg0` = a **register-resident lvalue** produced when a **compound assignment (`op=`) through a pointer to a struct member** `vdup`s the member lvalue to reload it. `ast_hook_vpush` only models memory-resident leaves (const/sym/VT_LOCAL/VT_LLOCAL-lval). Minimal repro: `void t1(struct P*p,double d){ p->vx -= d; }` desyncs; `p->vx = d` (no vdup) is faithful. So ROI (b)/(d) is BLOCKED on the recorder-fidelity gap, filed under the "Promote/inline replay-fidelity gap set" TODO item (added the 1993 bail site there). NO bounded fix in `ast_hook_vpush` (accepting reg-lvalue leaves is unsound — address came from prior codegen, unreconstructable on replay); must model compound-assign at the expression level, needs self-host+fuzz+cross-arch soak.

Still-open phase-2 (a)/(e): fold in a measured (deterministic!) runtime signal so ivsr-class passes don't score benefit 0; wire ROI into the default -O4 out-of-process superopt path.

Validation recipe: [[docker-optimizer-mcc-validation]]. Push constraint: [[push-to-main-needs-authorization]].
