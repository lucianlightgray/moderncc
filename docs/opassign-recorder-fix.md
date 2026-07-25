---
name: opassign-recorder-fix
description: MCC_AST_OPASSIGN gate — models compound-assign-through-pointer in the AST recorder so advance()/p->field op= become replayable
metadata: 
  node_type: memory
  type: project
  originSessionId: 5a90f7b3-a57a-4c37-85fc-8132faec5230
---

**Landed commit `e46617f0` (local, push gated) — `MCC_AST_OPASSIGN` (default OFF).**

Root cause (from [[roi-scheduler-determinism]] phase-2 (b)/(d)): compound assignment `lval op= rhs` through a pointer to a struct member (`p->vx -= d`) does `vdup()` on the member lvalue (mccgen.c `expr_eq` ~10298), pushing a register-resident lvalue `r=VT_LVAL|reg`. `ast_hook_vpush` (mccast.c:1993) only models memory-resident leaves (const/sym/VT_LOCAL/VT_LLOCAL-lval) → desync → `ast_replay_ok` rejects the WHOLE function → the entire `p->field op= …` class (incl. nbody's hot `advance()`) never entered the replay/optimizer path. This is the prerequisite the AOT-foundations advance() analysis (TODO items 2/3/70-73) silently assumed.

Fix (modeled at expression level, NOT by accepting an unsound reg-lvalue leaf):
- `ast_hook_vdup()` (new hook, called from `expr_eq` op= path before `vdup()`) arms a one-shot iff the top `ast_vs` lval is PURE (`ast_expr_pure`).
- `ast_hook_vpush` consumes it → `ast_dup_sub` deep-copies the top node (keeps model a TREE). Recorded shape = ordinary `Store(lval, Binary(op, lval_copy, rhs))` (transparent to all passes), Store tagged `AST_OP_OPASSIGN`.
- statement-context `AST_Store` replay, when tagged AND structurally verified (`value==Binary(op,X,rhs)`, `X==lval` via `ast_struct_eq`, lval pure), re-emits the byte-faithful `vdup` form (ONE addr compute) — matches baseline so the faithfulness gate passes. Correctness never relies on the tag alone; the structural+purity check makes mistag/side-effecting cases fall back to naive/baseline (always correct).

Validated (docker/amd64, recipe [[docker-optimizer-mcc-validation]]): OFF byte-identical; ON bit-matches gcc on nbody/opassign(all ops×types)/edge(nested-store,explicit x=x+y,arr[i++]+=,volatile)/20k-op fuzz/value-context+chained — all -O0/-O2/-O4; deterministic; **3-stage self-host fixpoint byte-identical (o1==o3)**, stage-2 mcc correct; asttool 747/0. advance() now `ast_replay_ok`+faithful+in ROI dump.

**IMPORTANT nuance:** this is a REPLAYABILITY unlock only — nbody advance() is byte-identical OFF==ON because `ivsr`/`licm`/promote still don't FIRE on it (separate TODO item 2/g + ROI phase-2 (a)). Getting the passes to fire on advance() is the natural next step.

Remaining before flipping default-on: cross-arch arm64/riscv64/i386 differential + AOT==JIT on arm64 + tens-of-thousands-of-seeds fuzz campaign. Push constraint: [[push-to-main-needs-authorization]].
