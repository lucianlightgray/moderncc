# QUESTIONS

Open questions only, per `INSTRUCTIONS.md` §9. Append-only (`merge=union`): never edit or delete a live block. **Open = a `### Q-` block with no appended `**ANSWER:**` / `**STATUS: ANSWERED**` line** — derive state from the blocks below, there is no rolling open/closed sentinel (one can't be maintained under append-only merge). A question that is ANSWERED has its substance migrated to `DETAILS.md`/`TODO.md` and, once no longer written to, its block is removed with an `ARCHIVED.md` record.

A human answers by appending `**ANSWER:** …` to a block. Q-lin-10001 through Q-lin-10003 are the §0 assumptions, mirrored here as §0 requires rather than asked in-band.

### Q-lin-10481 — [lin-x64] — 2026-08-21T17:38Z — BLOCKS: T-lin-10478 (arm64-windows leg only)
Does the fleet provision an arm64-Windows (WoA) execution path — real ARM64 Windows hardware or a WoA VM / qemu full-system — so the P1 JIT-coverage matrix can RUN-verify the arm64-windows (PE) triple? No box in the current fleet can execute arm64-WoA PE.
**Assumed for now:** arm64-WoA is capped at fired-only + object-diff (byte-differential proves "optimized", no run proof); every other triple is fully JIT-run-verified. (mode-a — advisory, task stays active.)
**Cost if wrong:** low — re-run the existing optfire cells under the new runner once it exists; no code redo (the matrix already registers the fired-only cell).
REF: DETAILS.md#t-lin-10476-jit-coverage-matrix
