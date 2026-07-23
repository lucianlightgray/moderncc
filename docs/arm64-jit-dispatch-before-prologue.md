---
name: arm64-jit-dispatch-before-prologue
description: arm64 mode-6 JIT dispatch stub is emitted BEFORE the prologue; targets must be complete self-contained functions
metadata: 
  node_type: memory
  type: reference
  originSessionId: 67d0aedf-cd7d-4b10-9436-d0babc5c5d0a
---

The arm64 mode-6 runtime-JIT dispatch stub (`adrp x16,slot; ldr; ldr; br x16`) is
emitted at `arm64_func_start_offset` — BEFORE gfunc_prolog's 224-byte frame — so the
function symbol/entry IS the stub, and `*slot` initially targets start+16 (the full
function incl. prologue). This means every swapped-in `*slot` target (recompiled
variant, KGC stub, trampoline, slice kernel) must be a COMPLETE self-contained
function with its own prologue+epilog; the stub pushes nothing, so each is entered
clean at its own entry. Do NOT reintroduce a frameless AOT-body target or per-stub
`leave`-style teardown — that was the dead end that broke `jit/selftest-slice-live`.

Contrast x86: its stub sits AFTER the single `push rbp`, and trampolines carry a
leading `leave` (mccjit_make_trampoline) to undo it; arm64 trampoline is just
`return variant` (no teardown) because nothing precedes it.

Mechanics (src/mccast.c ast_func_end, arm64 mode-6 block): splice base =
`ast_body_ind_sv` on x86 but `cg_arm64_func_start_offset` on arm64; the splice moves
the prologue +16, so `cg_arm64_func_sub_sp_offset += 16` after the splice keeps
gfunc_epilog patching the moved stack-sub NOP slots. Fixed the frameless-leaf return
corruption (git ~70f2abc0); repro was `int f(int x){return x+1;}` hot loop under
`MCC_JIT=1 -O4 -run` on arm64-Linux. See [[arm64-jit-wx-dualmap]],
[[4b-backend-parity]].
