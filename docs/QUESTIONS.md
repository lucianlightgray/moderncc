# QUESTIONS

Open questions only, per `INSTRUCTIONS.md` §9. Append-only, pruned on answer: a question that is answered has its substance migrated to `DETAILS.md` and `TODO.md` and its block reduced to an `ARCHIVED.md` record.

A human answers by appending `**ANSWER:** …` to a block. Q-lin-10001 through Q-lin-10003 are the §0 assumptions, mirrored here as §0 requires rather than asked in-band.

### Q-mac-30011 — [mac-arm64] — 2026-08-17T03:35Z — BLOCKS: T-mac-30005
`gate_win_insns` (the "instruction 15" human-mandated metric) asserts mcc's optimizer produces a real instructions-retired win on real input. Its current subject — chain-store on spectral-norm — is DEAD: p5 (172a2f31) deleted the deep-copy repair that produced the banked 0.55s→0.35s / 8% win, leaving only the ast_promo POISON pessimization. Re-measured explicit-arm on a quiet arm64 box: chain-store LOSES -90.7% instructions on spectral and is instruction-FLAT on sieve (+0.0016%), so the banked levelbench-cycles.tsv re-target candidate (chain-store/sieve +1.966% insn) no longer holds. No other gate wins by a margin worth asserting (cycles-table candidates have ~0 instruction delta by construction; main levelbench.tsv ranked gates are all <0.5% or are lowering defects like builtin-math). Retire the assertion, or re-target it at a specific gate/kernel you choose (and accept that today's honest margins are sub-1%)?
**Assumed for now:** none safe — retiring or re-pointing a human-mandated metric is explicitly not a session's unilateral call (gate_win_insns docstring). T-mac-30005 is BLOCKED pending this answer.
**Cost if wrong:** if retired when it should have been re-targeted, mcc loses its one real-input optimizer-win gate; if re-targeted at a gate that later regresses, the metric re-breaks. The gatewin cell stays honestly-red (it is red today either way); the prepared explicit-arm cell fix + known-positive is ready to land once the subject is decided (blocked under N4 while gatewin is red).
REF: DETAILS.md#t-mac-30005-retarget-candidate-chain-store-sieve-is-dead-on-instructions-too-escalating
