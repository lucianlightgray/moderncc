# QUESTIONS

Open questions only, per `INSTRUCTIONS.md` §9. Append-only, pruned on answer: a question that is answered has its substance migrated to `DETAILS.md` and `TODO.md` and its block reduced to an `ARCHIVED.md` record.

A human answers by appending `**ANSWER:** …` to a block. Q-lin-10001 through Q-lin-10003 are the §0 assumptions, mirrored here as §0 requires rather than asked in-band.

### Q-lin-10481 — [lin-x64] — 2026-08-21T17:38Z — BLOCKS: T-lin-10478 (arm64-windows leg only)
Does the fleet provision an arm64-Windows (WoA) execution path — real ARM64 Windows hardware or a WoA VM / qemu full-system — so the P1 JIT-coverage matrix can RUN-verify the arm64-windows (PE) triple? No box in the current fleet can execute arm64-WoA PE.
**Assumed for now:** arm64-WoA is capped at fired-only + object-diff (byte-differential proves "optimized", no run proof); every other triple is fully JIT-run-verified. (mode-a — advisory, task stays active.)
**Cost if wrong:** low — re-run the existing optfire cells under the new runner once it exists; no code redo (the matrix already registers the fired-only cell).
REF: DETAILS.md#t-lin-10476-jit-coverage-matrix

_No open questions. Last resolved: Q-win-50034 — FLEET C23-STRICTNESS POLICY (human, 2026-08-20, SHA 23b90c3f): the disruptive C23 BREAKING changes (empty `()` == `(void)`, etc.) are honored ONLY under an explicit strict-ISO C23 std (`-std=c23`), NOT the default gnu23 mode — a deliberate divergence from gcc-16's strict default, chosen to keep existing K&R/C17 code + the runtime building by default and the o0-baseline byte-neutral. Shared gate: `mcc.h c23_strict_breaking(s)`. C23 FEATURES stay default-on. (T-mac-30142 landed under it; C99 implicit-int/T-mac-30038 is separate.) See ARCHIVED.md#Q-win-50034 and DETAILS.md#t-mac-30142-c23-empty-paren. Prior: Q-lin-10411 (`<threads.h>` backend) + Q-lin-10409 (o0-rebank AUTHORIZED; lin holds delicate front-end items), ANSWERED 2026-08-19T10:40Z SHA 5cb9e337._
