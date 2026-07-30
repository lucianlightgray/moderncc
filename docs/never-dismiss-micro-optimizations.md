---
name: never-dismiss-micro-optimizations
description: "All optimization techniques are valid — never dismiss micro-opts a priori; ROI must report whether an opt EVER fires, not pre-judge its worth"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: bd6d0076-11b2-40d8-822c-3b826b60e503
---

Never dismiss a micro-optimization or any optimization technique as "probably not worth it" / "tables are tiny" / "won't pay off." In this project every optimization technique is treated as valid; the correct move is to implement/measure it, never to pre-judge it out.

**Why:** The user corrected me for hedging ("the durable wins are 1,2,4; I'd not assume the O(n²) intern pays off"). This repo's whole thesis is an exhaustive optimizer + JIT search over permutations×combinations×slices — a technique that looks inert is often only inert because an *enabling* gate is off (the recorded pattern: a pass measured "inert" only because upstream left every candidate `desync`, and `MCC_AST_OPASSIGN` unblocked the entire hot-loop story). Dismissal hides the real gap.

**How to apply:**
- Propose/implement all reduction & optimization techniques; do not rank some as "not worth trying."
- Frame measurement as an *ever-fires ledger*, not a payoff gate: report whether a technique EVER changes the AST / finds a promoted match, at function / TU / whole-corpus scope. Distinguish "fired but small here" from "never fired anywhere" — only the second is a reason to change course, and even then toward fixing the *enabler*, not deleting the technique.
- A never-firing technique is a named, known gap to refine (better query/reconciliation, wider vocabulary, a missing gate) — surfaced explicitly, never silently absent or pruned.

Encoded in docs/TODO.md: the ROI "ever-fires ledger" bullet (Strategy scheduler §) and the handle-reduction bullet (JIT runtime §). Relates to [[mcc-codegen-gap-is-regalloc]] (measure the real lever, don't assume).
