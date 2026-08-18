# QUESTIONS

Open questions only, per `INSTRUCTIONS.md` §9. Append-only, pruned on answer: a question that is answered has its substance migrated to `DETAILS.md` and `TODO.md` and its block reduced to an `ARCHIVED.md` record.

A human answers by appending `**ANSWER:** …` to a block. Q-lin-10001 through Q-lin-10003 are the §0 assumptions, mirrored here as §0 requires rather than asked in-band.

_No open questions. Last resolved: Q-mac-30011 (gatewin → wall-clock no-regression on promote-locals/nbody), ANSWERED 2026-08-17T11:58Z, SHA a27b1b5c — see ARCHIVED.md and DETAILS.md#t-mac-30005-resolved-gatewin-wall-clock-no-regression-promote-locals-nbody._

### Q-lin-10409 — [lin-x64] — 2026-08-18T23:12Z — BLOCKS: GOAL "TODO.md empty" (o0-rebank set: T-mac-30139/30080/30081/30131; and coordination for the remaining ~192 items)
The lin-x64 session has exhausted its safely-completable, x86_64-verifiable queue (this session landed 9 full fixes + 3 slices + 1 spike + findings; DETAILS/ARCHIVED). The remaining ~192 open items each need one of: (a) another platform (arm64/riscv64/mac/win — not runnable on this x86_64 box); (b) the GPU (hardware-gated); (c) a **coordinated fleet o0-baseline rebank** (which would let several correct-but-drift-shifting fixes land — e.g. T-mac-30139 implicit-_Atomic seq_cst, DETAILS#t-mac-30139-atomic-implicit-spike, plus the o0-rebank set 30080/30081/30131); (d) delicate/architecturally-blocked front-end work (30236 line_num-64bit, 30238 packed-_BitInt-bitfields, 30249 empty-struct-layout, 30180 div-by-zero-policy, 30202 AST-operand-identity, 30246 side-effect-free-semantic-check, 30247 constexpr-real-storage); or (e) oracle-divergent cosmetics (gcc≠clang, no single correct output).
**Question:** (1) Do you AUTHORIZE the coordinated fleet o0-baseline rebank (all target keys) so the drift-shifting-but-correct fixes can land? (2) Should lin take on the delicate/blast-radius items (d) solo despite regression risk, or wait for coordination? (3) Any priority ordering among the remaining items?
**Assumed for now (mode a):** lin holds per INSTRUCTIONS §10.11 (clean handoff; queue exhausted for this box), peers (mac/win) continue their platform-specific items, and the o0-rebank + delicate items await your direction. Low cost-if-wrong: nothing is blocked that another session/platform can't pick up from the repo.
**Cost if wrong:** if you intended lin to push blast-radius/delicate items solo now, that work is deferred until you answer (no redo, just delay).
REF: docs/TODO.md § "In progress — lin-x64" session notes (22:44Z/23:06Z tallies) + DETAILS anchors for the 9 fixes.
