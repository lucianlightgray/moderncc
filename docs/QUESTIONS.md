# QUESTIONS

Open questions only, per `INSTRUCTIONS.md` §9. Append-only, pruned on answer: a question that is answered has its substance migrated to `DETAILS.md` and `TODO.md` and its block reduced to an `ARCHIVED.md` record.

A human answers by appending `**ANSWER:** …` to a block. Q-lin-10001 through Q-lin-10003 are the §0 assumptions, mirrored here as §0 requires rather than asked in-band.

### Q-lin-10001 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: every CONTRACT and BLOCKER path
Does `SendMessage` deliver durably to a named session on another machine?

INSTRUCTIONS.md §0 records this as an assumption to verify at first run. It has not been exercised: no session has yet sent a message to another machine in this repo.

**Assumed for now:** Mode (a). The repo, not the bus, is the source of truth, so every protocol step is written to survive total message loss; messages carry pointers to pushed commits and nothing else.

**Cost if wrong:** Nothing is redone. A lost message costs one polling cycle, because the same state is in `TODO.md` and `DETAILS.md` at the SHA the message would have cited.

REF: DETAILS.md#q-lin-10001-does-sendmessage-deliver-durably-to-a

### Q-lin-10002 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10030, T-lin-10092, T-lin-10093
Are the three sessions addressable by the IDs in INSTRUCTIONS.md?

`mac-arm64`, `lin-x64` and `win-x64` are named as addresses in §0. Only `lin-x64` has been observed; `ListAgents` on this machine does not show the other two.

**Assumed for now:** Mode (a). Broadcasts are written as if delivered and the same content is pushed first, so a session that never receives one still reads it from the repo.

**Cost if wrong:** Nothing is redone; `[P]` fan-out children stay OPEN until the platform that owns them polls.

REF: DETAILS.md#q-lin-10002-are-the-three-sessions-addressable-by

**Note (win-x64, 2026-08-14T19:34Z, SHA 244be14e):** verified at first run — from win-x64, `SendMessage to="lin-x64"` returns *"No agent named 'lin-x64' is currently addressable."* The bus does not resolve cross-machine session IDs on this host, so mode (a) holds: the T-lin-10002 CONTRACT announcement was delivered by pushed commits + the ARCHIVED record, not the bus. Partial evidence (one host, one moment), so the question stays open per N10.

### Q-lin-10003 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: every `docs` and `code` push
Can all three machines push to `main` with no branch protection?

§0 assumes it. Verified on `lin-x64` only, where pushes to `main` succeed.

**Assumed for now:** Mode (a). Claim-by-push is the only ownership mechanism, so a session that cannot push cannot claim and will fail loudly on its first attempt rather than silently.

**Cost if wrong:** If a platform cannot push, its claims never exist and its work must be re-landed by a session that can — bounded by one task per blocked session.

REF: DETAILS.md#q-lin-10003-can-all-three-machines-push-to

### Q-lin-10004 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10011
Register arrays: follow clang and reject `a[1]`, or carry a subscript-suppression flag to keep gcc's leniency?

C11 6.3.2.1p3 makes decaying a `register` array undefined. mcc now rejects the forms both references reject. `*a` and `*(a+1)` are still accepted, and neither can simply be checked, because `a[1]` compiles to exactly the same two calls (`gen_op('+'); indir();`). The references disagree: gcc accepts `a[1]`, clang rejects it, each consistently with its own treatment of `*(a+1)`, which both reject.

**Assumed for now:** Mode (a). Both forms stay accepted, which is what ships today and matches gcc.

**Cost if wrong:** One fixture and the `gen_op`/`indir` flag, if the answer is clang. Nothing else depends on it.

REF: DETAILS.md#q-lin-10004-register-arrays-follow-clang-and-reject

### Q-lin-10005 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10012
Raise `MCC_MAX_ALIGN` for 32-byte vectors, or keep the documented incompatibility?

32-byte vectors are laid at 16-byte alignment (8 on i386/arm), so passing one across an mcc/gcc object boundary is a struct-ABI incompatibility. Raising the cap is an ABI change whose blast radius nobody has measured.

**Assumed for now:** Mode (a). The cap stays and the incompatibility is documented rather than fixed.

**Cost if wrong:** Every object mcc has ever emitted that carries a 32-byte vector in a struct changes layout. The measurement — how many cells and how many banks move — is the first half of the task either way.

REF: DETAILS.md#q-lin-10005-raise-mcc-max-align-for-32

### Q-lin-10006 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10045
`-fopt-slice`: own the pass, or delete it?

It makes object output depend on the optimizer's disk cache and nothing watches it, because the flag is `MCC_OPTD_LEVEL(9)` and has no subject at any shipped level. `opt-cache-determinism` is a permanent 77, so the defect is invisible rather than absent. The determinism defect reproduces verbatim today.

**Assumed for now:** Mode (b). The task is BLOCKED: owning it means a cache-identity gate at a shipped level, deleting it means removing a pass and its cache, and the two share no work.

**Cost if wrong:** Neither direction is started, so nothing is redone — but the cell stays a permanent 77 and reads as coverage while it does.

REF: DETAILS.md#q-lin-10006-fopt-slice-own-the-pass-or

### Q-lin-10007 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10057
`kept_coverage` host-sensitivity: raise `--tol`, make the metric host-stable, or encode "bank from stage2"?

The gcc-hosted and stage2 self-hosted compilers disagree: `fallback 98 / kept 82.7770` against `100 / 82.7139` at `-O0`. The 0.06pp spread is outside the tool's `--tol` of 0.05pp, so banking from a gcc host re-breaks CI, which tests the stage2 tree.

**Assumed for now:** Mode (a). The floors stay at the lower of the two and the rule stays a convention.

**Cost if wrong:** A convention that is not enforced by the tool gets got wrong silently, which is the failure this whole class of task exists to remove. Redoing it is one re-bank.

REF: DETAILS.md#q-lin-10007-kept-coverage-host-sensitivity-raise-tol

### Q-lin-10008 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10040, and it re-ranks T-lin-10033 through T-lin-10038
Is the 2026-08-09 device-path freeze still standing?

Six rows are frozen by that decision: the dispatcher (three subsystems, priced nowhere), 115 indirect blocks, recursion (no data at all), the `pe` lowerable floors, debt #3's descriptor staleness (fixed and unreachable until binding 2 grows), and float in the emitter (landed for `double`, and it moved the device-executable fraction by ~0.0 iteration-weighted points).

**Assumed for now:** Mode (a). The freeze stands and none of the six is scheduled.

**Cost if wrong:** Nothing is redone by waiting. Unfreezing without the decision would schedule three subsystems against a lever the break-even table already prices as negative.

REF: DETAILS.md#q-lin-10008-is-the-2026-08-09-device

### Q-lin-10009 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10042
Schedule the Metal parity plan, or keep the drop?

Priced at 1,530-2,360 lines behavioural, or 2,200-3,400 with fp64. The drop was reversed by decision on 2026-08-09 and the spec was written; nothing has been scheduled since. It has no CI differential and cannot have one — the only harness is the per-value differential run by hand on the Mac.

**Assumed for now:** Mode (b). The task is BLOCKED. There is no safe interim: starting a 2,000-line arm that no gate can watch is worse than not starting it.

**Cost if wrong:** Nothing is redone while it waits.

REF: DETAILS.md#q-lin-10009-schedule-the-metal-parity-plan-or

### Q-lin-10010 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10058
Should `node-census`'s `all_invokes_on_cpu` be gated at all, or reported only?

It is a ratio over the compiler's own source, so it moves whenever call density changes: it fell 94.9385% -> 94.8004% purely because `src/mcc.c` amalgamated ~2700 new lines. It is not a regression signal in either direction. The external-only ceiling, 99.2540%, is the number the headline rests on.

**Assumed for now:** Mode (a). It stays gated, and every move costs an investigation that finds the corpus grew.

**Cost if wrong:** One bank and one cell registration either way.

REF: DETAILS.md#q-lin-10010-should-node-censuss-all-invokes-on

### Q-lin-10011 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10064
Arm the 63 `EXTRA` cells and take the three pre-existing divergences red?

Arming is one `-I` each and it goes red: at `-O0` `full_language.c` has 303 bodies, 299 faithful, 1 empty and three that are not, against `rir_parity`'s hard 100% bar. The three are pre-existing byte divergences the gate is masking, not new ones.

**Assumed for now:** Mode (a). The cells stay unarmed while the three defects are open, and the fact that the `EXTRA` has never contributed anything is recorded rather than hidden.

**Cost if wrong:** Nothing is redone. The risk of the assumption is that an unarmed cell reads as coverage — which is exactly the shape T-lin-10003 exists to refuse, so this one should not stay assumed for long.

REF: DETAILS.md#q-lin-10011-arm-the-63-extra-cells-and

### Q-lin-10012 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10077
Adopt `__divdc3`-style complex division?

N37's compiler half. `csweep.C64/C80.CDIV` and `CDIVSEL` hide 283 refs-agree points each, 44 of them finite and three a 53%-relative-error complex divide rather than rounding. Annex G does not specify the accuracy of finite results, and mcc honours its Annex G claim on the cases G.5.1 actually mandates (infinity/NaN recovery).

**Assumed for now:** Mode (a). mcc keeps its current division and the divergence is banked with the reasoning, the way item 22's was.

**Cost if wrong:** If the answer is to adopt it, every `csweep` complex row is re-banked once.

REF: DETAILS.md#q-lin-10012-adopt-divdc3-style-complex-division

### Q-lin-10013 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10086, T-lin-10087
Is Windows-on-ARM hardware available to any session?

`arm64-win32` and `arm-win32` object emission works; execution needs the hardware. wine runs x86 PE only and qemu-user cannot load PE, so there is no emulation route.

**Assumed for now:** Mode (b). Both tasks are BLOCKED. No safe interim exists — an unexecuted target cannot be verified by anything the tree can run.

**Cost if wrong:** Nothing is redone while they wait.

REF: DETAILS.md#q-lin-10013-is-windows-on-arm-hardware-available

### Q-lin-10014 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10088
Can `vendor/gcc-c-torture-execute` be vendored onto the Windows host?

With the corpus present, `pe/x-oracle` runs all 1,693 programs with no new code. The host has no network and the corpus is deliberately not vendored in this tree.

**Assumed for now:** Mode (b). BLOCKED. The W2 reconstructions stand as evidence in the meantime — four of five named divergences are mcc-correct and the fifth is implementation-defined — but they are reconstructions, not the corpus.

**Cost if wrong:** Nothing is redone; the confirmation simply does not exist until the corpus does.

REF: DETAILS.md#q-lin-10014-can-vendorgcc-c-torture-execute-be


### Q-mac-30000 — [mac-arm64] — 2026-08-14T20:15Z — BLOCKS: T-lin-10089
The `ast/o0-baseline` *-osx object bank is header-sensitive and currently encodes the Linux cross host's glibc libc headers (N38); a native Darwin mcc uses the macOS SDK, so 198 of 297 arm64-osx rows differ. To un-skip the quartet on Darwin without a bank that drifts with the SDK, the osx keys need a *pinned* Darwin libc header set wired as `--sysroot` (mirroring `vendor/gentoo-stage3-*`). What set should be vendored — a curated subset of a fixed macOS SDK, and is committing Apple SDK headers into the tree acceptable (licensing)? Or a preferred stand-in, e.g. an mcc-authored minimal darwin libc header set under `runtime/osx`?
**Assumed for now:** none safe — vendoring Apple headers is a licensing decision and there is no in-tree Darwin libc header set to point at, so the quartet keeps skipping on Darwin (mode b, BLOCKED).
**Cost if wrong:** nothing redone; the quartet stays a visible, reasoned skip on Darwin until the header set is chosen.
REF: DETAILS.md#t-lin-10089-investigation-the-osx-bank-encodes-linux-glibc-headers
