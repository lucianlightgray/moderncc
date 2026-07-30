---
name: osx-selfhost-jit-sympool-corruption
description: "macOS selfhost-jit is FIXED — it was two self-compile miscompiles (storeval-arg vstack underflow + RANGE landor-invert drop), NOT a JIT bug"
metadata: 
  node_type: memory
  type: project
  originSessionId: d8e85f80-7e78-45fe-a389-93af6d22f450
---

**FIXED 2026-07-29. `selfhost-jit` PASSES on arm64 macOS** (JIT output byte-identical to
AOT). It was never a JIT bug — plain `-O2+`/`-O4` self-compile miscompiled mcc, and
running the result (`--jit -O4 -run`, or even `MCC_JIT=0 -run`) crashed. TWO bugs:

1. **-O2+ heap corruption (commit b6af0f7f).** The `MCC_AST_STOREVAL_CALL` replay
   (`ast_replay_value` AST_Invoke live-arg) ran `vrotb(3)` at value-stack depth 1;
   `vstack` (`gen_vstack+1`) sits right after `nb_sym_pools`/`sym_pools` in MCCState, so
   `vtop[-2]` overwrote them → crash in `__sym_malloc`. Fix: guard the rotation on real
   depth, bail to baseline (setjmp) if short.

2. **-O4 RANGE fold inverted negated asserts (commit 2c88e797).** `ast_range_try` folds
   `lo<=x && x<=hi` → `(unsigned)(x-lo) <= span` (TOK_ULE) but wiped ALL fbits incl.
   `AST_FB_LANDOR_INVERT` (set by the parser for the assert's `!(...)`). The folded plain
   comparison's replay path (unlike the landor path ~ast_replay_value:5870) ignores that
   bit, so the branch inverted — asserting when IN range (r=0 → abort in arm64 `intr`).
   Fix: fold to `TOK_UGT` (= ULE^1) when the invert bit is set, baking in the negation.

**Durable lessons:**
- selfhost-fixpoint's byte-identity (o1==o2==o3) passes a STABLE miscompile and does not
  RUN the result or test -O4 — it cannot catch a self-compile miscompile. selfhost-jit
  (which executes the self-compiled mcc) is what caught both. **Follow-up worth doing:
  add a -run/-O4 leg to selfhost-fixpoint.** See [[ratchet-not-selfhost-coverage]].
- Debugging method for layout-dependent heap heisenbugs (lldb/ASan/gmalloc all failed):
  a `dynarray_add` sanity guard + a `mcc_dbg_check_syms(where)` reading MCCState at fixed
  offsets, sprinkled as checkpoints and gated to one funcname → ~10 native-speed cycles.
- To find a miscompiled static function: it may carry a leading `_` in the ELF symbol
  (macOS-config mcc). Diff `objdump -d --disassemble=_fn` between gate-on and gate-off.
