---
name: reemit-faithful-gate
description: "-O3 self-host n/a root cause — ast_reemit_retain must require ast_fn_faithful"
metadata: 
  node_type: memory
  type: project
  originSessionId: 880d8856-a372-46e4-a8f6-12b9e18d600d
---

The CI benchmark's all-n/a `mcc [-O3]` rows (macos-arm64) were caused by the AST
forward-inline **reemit** path shipping broken code. Root cause: in `ast_func_end`
(src/mccast.c) `keep_inline` was gated on `ast_fn_faithful` but `keep_reemit`
(`ast_reemit_retain`) was **not**. So a function whose AST replay is not
byte-faithful (e.g. `parse_include`) could still be queued for end-of-TU
reemission; `ast_func_end` restores the correct first-pass bytes when
`faithful==0`, but `ast_reemit` re-runs the same unfaithful replay with no
fallback → miscompiled body (arm64: clobbered arg register in the Mach-O
framework branch → SIGSEGV inside pstrcpy). Fix: `keep_reemit = ast_fn_faithful
&& ast_reemit_retain(...)`. See [[ast-replay-symdbg-debugging]], [[macos-arm64-status]].

Bug is layout/perturbation-sensitive: after commit 5968f6ab the default-node-limit
manifestation is masked even without the fix, but the gate is the correct
invariant and prevents recurrence. Minimal repro knob was `MCC_AST_INLINE_NODES=26`
(pre-5968f6ab). Full details in FIX.md.
