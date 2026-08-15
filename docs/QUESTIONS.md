# QUESTIONS

Open questions only, per `INSTRUCTIONS.md` §9. Append-only, pruned on answer: a question that is answered has its substance migrated to `DETAILS.md` and `TODO.md` and its block reduced to an `ARCHIVED.md` record.

A human answers by appending `**ANSWER:** …` to a block. Q-lin-10001 through Q-lin-10003 are the §0 assumptions, mirrored here as §0 requires rather than asked in-band.

### Q-lin-10001 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: every CONTRACT and BLOCKER path
Does `SendMessage` deliver durably to a named session on another machine?

INSTRUCTIONS.md §0 records this as an assumption to verify at first run. It has not been exercised: no session has yet sent a message to another machine in this repo.

**Assumed for now:** Mode (a). The repo, not the bus, is the source of truth, so every protocol step is written to survive total message loss; messages carry pointers to pushed commits and nothing else.

**Cost if wrong:** Nothing is redone. A lost message costs one polling cycle, because the same state is in `TODO.md` and `DETAILS.md` at the SHA the message would have cited.

REF: DETAILS.md#q-lin-10001-does-sendmessage-deliver-durably-to-a

**ANSWER (human, 2026-08-15):** Do not block on `SendMessage`. `git pull`/`git push` works and is the transport; the bus is best-effort decoration on top of it. Mode (a) confirmed as the standing design: the repo is the source of truth, every protocol step survives total message loss, and no CONTRACT or BLOCKER path may wait on a message being received. Durability of `SendMessage` is therefore not a property this repo needs to establish.
**STATUS: ANSWERED** 2026-08-15T10:59Z

### Q-lin-10002 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10030, T-lin-10092, T-lin-10093
Are the three sessions addressable by the IDs in INSTRUCTIONS.md?

`mac-arm64`, `lin-x64` and `win-x64` are named as addresses in §0. Only `lin-x64` has been observed; `ListAgents` on this machine does not show the other two.

**Assumed for now:** Mode (a). Broadcasts are written as if delivered and the same content is pushed first, so a session that never receives one still reads it from the repo.

**Cost if wrong:** Nothing is redone; `[P]` fan-out children stay OPEN until the platform that owns them polls.

REF: DETAILS.md#q-lin-10002-are-the-three-sessions-addressable-by

**Note (win-x64, 2026-08-14T19:34Z, SHA 244be14e):** verified at first run — from win-x64, `SendMessage to="lin-x64"` returns *"No agent named 'lin-x64' is currently addressable."* The bus does not resolve cross-machine session IDs on this host, so mode (a) holds: the T-lin-10002 CONTRACT announcement was delivered by pushed commits + the ARCHIVED record, not the bus. Partial evidence (one host, one moment), so the question stays open per N10.

**ANSWER (human, 2026-08-15):** Moot — do not block on `SendMessage`. Whether the three session IDs resolve on the bus no longer gates anything: `git pull`/`git push` works and is the addressing mechanism. A session is reached by pushing to `main` and letting it poll, never by naming it. Mode (a) confirmed; the win-x64 observation above (cross-machine IDs do not resolve) is recorded as expected behaviour rather than a defect. `[P]` fan-out children stay OPEN until the owning platform polls, and that is the design, not a degradation.
**STATUS: ANSWERED** 2026-08-15T10:59Z

### Q-lin-10003 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: every `docs` and `code` push
Can all three machines push to `main` with no branch protection?

§0 assumes it. Verified on `lin-x64` only, where pushes to `main` succeed.

**Assumed for now:** Mode (a). Claim-by-push is the only ownership mechanism, so a session that cannot push cannot claim and will fail loudly on its first attempt rather than silently.

**Cost if wrong:** If a platform cannot push, its claims never exist and its work must be re-landed by a session that can — bounded by one task per blocked session.

REF: DETAILS.md#q-lin-10003-can-all-three-machines-push-to

**ANSWER (human, 2026-08-15):** Yes. `git pull`/`git push` works from all three machines; there is no branch protection on `main`. Mode (a) confirmed: claim-by-push is the ownership mechanism and it is live. Combined with Q-lin-10001/10002, this makes the repo the sole coordination channel — push to claim, pull to observe, never wait on a message.
**STATUS: ANSWERED** 2026-08-15T10:59Z

