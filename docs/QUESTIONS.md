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

**ANSWER (human, 2026-08-15):** Keep gcc's leniency. Both `a[1]` and `*(a+1)` stay accepted (Mode a) — mcc matches gcc here, not clang. T-lin-10011's remaining DoD, the accept-forms fixture, is written in the gcc mode: `a[1]` compiles.

### Q-lin-10005 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10012
Raise `MCC_MAX_ALIGN` for 32-byte vectors, or keep the documented incompatibility?

32-byte vectors are laid at 16-byte alignment (8 on i386/arm), so passing one across an mcc/gcc object boundary is a struct-ABI incompatibility. Raising the cap is an ABI change whose blast radius nobody has measured.

**Assumed for now:** Mode (a). The cap stays and the incompatibility is documented rather than fixed.

**Cost if wrong:** Every object mcc has ever emitted that carries a 32-byte vector in a struct changes layout. The measurement — how many cells and how many banks move — is the first half of the task either way.

REF: DETAILS.md#q-lin-10005-raise-mcc-max-align-for-32

**ANSWER (human, 2026-08-15):** Raise `MCC_MAX_ALIGN` for 32-byte vectors (NOT Mode a). Do the ABI change so a 32-byte vector in a struct is laid at 32-byte alignment and is cross-TU-compatible with gcc. The blast-radius measurement (how many cells / banks move) is the first half of T-lin-10012 either way; carry it, then re-bank.

### Q-lin-10007 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10057
`kept_coverage` host-sensitivity: raise `--tol`, make the metric host-stable, or encode "bank from stage2"?

The gcc-hosted and stage2 self-hosted compilers disagree: `fallback 98 / kept 82.7770` against `100 / 82.7139` at `-O0`. The 0.06pp spread is outside the tool's `--tol` of 0.05pp, so banking from a gcc host re-breaks CI, which tests the stage2 tree.

**Assumed for now:** Mode (a). The floors stay at the lower of the two and the rule stays a convention.

**Cost if wrong:** A convention that is not enforced by the tool gets got wrong silently, which is the failure this whole class of task exists to remove. Redoing it is one re-bank.

REF: DETAILS.md#q-lin-10007-kept-coverage-host-sensitivity-raise-tol

**ANSWER (human, 2026-08-15):** Make the metric host-stable (NOT raise `--tol`, NOT a convention). Fix the gcc-host vs stage2 disagreement so `kept_coverage` produces the same figure regardless of which compiler hosts the measurement; then the floor is tool-enforced rather than a convention that can be got wrong silently. T-lin-10057.

**MODE CORRECTION (lin-x64, 2026-08-15):** filed as mode (a), but kept_coverage: the recorded assumption is that the floors stay at the lower of the two and the rule stays a convention, which is a decision not to act. Under §9 a mode-(a) question leaves its task ACTIVE; this one's task is BLOCKED, and BLOCKED is the accurate state. The label was wrong, not the state — read this as mode (b). Found by a consistency pass over TODO states against QUESTIONS modes; see DETAILS.md#review-pass-2026-08-15-what-the-consistency-audit-found.
### Q-lin-10008 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10040, and it re-ranks T-lin-10033 through T-lin-10038
Is the 2026-08-09 device-path freeze still standing?

Six rows are frozen by that decision: the dispatcher (three subsystems, priced nowhere), 115 indirect blocks, recursion (no data at all), the `pe` lowerable floors, debt #3's descriptor staleness (fixed and unreachable until binding 2 grows), and float in the emitter (landed for `double`, and it moved the device-executable fraction by ~0.0 iteration-weighted points).

**Assumed for now:** Mode (a). The freeze stands and none of the six is scheduled.

**Cost if wrong:** Nothing is redone by waiting. Unfreezing without the decision would schedule three subsystems against a lever the break-even table already prices as negative.

REF: DETAILS.md#q-lin-10008-is-the-2026-08-09-device

**ANSWER (human, 2026-08-15):** NO — the 2026-08-09 device-path freeze is NOT still standing. **UNBLOCK.** The six frozen rows are schedulable again; T-lin-10040 unblocks and T-lin-10033–T-lin-10038 are re-ranked for scheduling.

**MODE CORRECTION (lin-x64, 2026-08-15):** filed as mode (a), but the device-path freeze: the recorded assumption is that the freeze stands and none of the six is scheduled, which is a decision not to act. Under §9 a mode-(a) question leaves its task ACTIVE; this one's task is BLOCKED, and BLOCKED is the accurate state. The label was wrong, not the state — read this as mode (b). Found by a consistency pass over TODO states against QUESTIONS modes; see DETAILS.md#review-pass-2026-08-15-what-the-consistency-audit-found.
### Q-lin-10010 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10058
Should `node-census`'s `all_invokes_on_cpu` be gated at all, or reported only?

It is a ratio over the compiler's own source, so it moves whenever call density changes: it fell 94.9385% -> 94.8004% purely because `src/mcc.c` amalgamated ~2700 new lines. It is not a regression signal in either direction. The external-only ceiling, 99.2540%, is the number the headline rests on.

**Assumed for now:** Mode (a). It stays gated, and every move costs an investigation that finds the corpus grew.

**Cost if wrong:** One bank and one cell registration either way.

REF: DETAILS.md#q-lin-10010-should-node-censuss-all-invokes-on

**ANSWER (human, 2026-08-15):** Neither gate-as-is nor report-only. `node-census` should make an honest effort to run on all available hardware — CPU, JIT, GPU. Refactor to **auto-detect available hardware at runtime** and ungate the CPU/GPU paths so they run whenever the hardware is present, with the only overrides being explicit `--jit-always-cpu` / `--jit-always-gpu` flags. T-lin-10058.

**MODE CORRECTION (lin-x64, 2026-08-15):** filed as mode (a), but all_invokes_on_cpu: the recorded assumption is that it stays gated and every move costs an investigation, which leaves the task's subject untouched. Under §9 a mode-(a) question leaves its task ACTIVE; this one's task is BLOCKED, and BLOCKED is the accurate state. The label was wrong, not the state — read this as mode (b). Found by a consistency pass over TODO states against QUESTIONS modes; see DETAILS.md#review-pass-2026-08-15-what-the-consistency-audit-found.
### Q-lin-10011 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10064
Arm the 63 `EXTRA` cells and take the three pre-existing divergences red?

Arming is one `-I` each and it goes red: at `-O0` `full_language.c` has 303 bodies, 299 faithful, 1 empty and three that are not, against `rir_parity`'s hard 100% bar. The three are pre-existing byte divergences the gate is masking, not new ones.

**Assumed for now:** Mode (a). The cells stay unarmed while the three defects are open, and the fact that the `EXTRA` has never contributed anything is recorded rather than hidden.

**Cost if wrong:** Nothing is redone. The risk of the assumption is that an unarmed cell reads as coverage — which is exactly the shape T-lin-10003 exists to refuse, so this one should not stay assumed for long.

REF: DETAILS.md#q-lin-10011-arm-the-63-extra-cells-and

**ANSWER (human, 2026-08-15):** Do not simply arm-and-take-red. Convert the three pre-existing divergences (and red cells generally) into **investigation/research tasks plus implementation TODO tasks** — each divergence becomes a tracked item with a root-cause investigation and a fix task, rather than a masked or a loudly-red gate. T-lin-10064.

**MODE CORRECTION (lin-x64, 2026-08-15):** filed as mode (a), but the 63 EXTRA cells: the recorded assumption is that they stay unarmed, and arming them IS the task, so the assumption forbids the work. Under §9 a mode-(a) question leaves its task ACTIVE; this one's task is BLOCKED, and BLOCKED is the accurate state. The label was wrong, not the state — read this as mode (b). Found by a consistency pass over TODO states against QUESTIONS modes; see DETAILS.md#review-pass-2026-08-15-what-the-consistency-audit-found.
### Q-lin-10012 — [lin-x64] — 2026-08-14T12:40Z — BLOCKS: T-lin-10077
Adopt `__divdc3`-style complex division?

N37's compiler half. `csweep.C64/C80.CDIV` and `CDIVSEL` hide 283 refs-agree points each, 44 of them finite and three a 53%-relative-error complex divide rather than rounding. Annex G does not specify the accuracy of finite results, and mcc honours its Annex G claim on the cases G.5.1 actually mandates (infinity/NaN recovery).

**Assumed for now:** Mode (a). mcc keeps its current division and the divergence is banked with the reasoning, the way item 22's was.

**Cost if wrong:** If the answer is to adopt it, every `csweep` complex row is re-banked once.

REF: DETAILS.md#q-lin-10012-adopt-divdc3-style-complex-division

**ANSWER (human, 2026-08-15):** DO adopt `__divdc3`-style complex division (NOT Mode a). Replace mcc's current finite-case complex divide; re-bank every `csweep` complex row once against the new results. T-lin-10077.

### Q-mac-30001 — [mac-arm64] — 2026-08-15T00:23Z — BLOCKS: T-lin-10090
How should the arm64 `if-conversion-abs` cycles/instructions re-take be produced, given no `perf` on Darwin and no arm64-Linux host in the fleet?

The task's deliverable is `tools/optlevel-bench.py --cycles if-conversion-abs --cycle-reads 21` run twice; that path is Linux-`perf`-only (`perf_pair`:326 / `perf_insns`:178, and `main` returns 77 without `perf`, :983). macOS / Apple Silicon exposes no userspace `perf`, and retired-instruction counts need private kperf (entitlement/root). The fleet is {Darwin arm64, Linux x64, Windows x64}, so no host both is arm64 and has `perf`; the figure has no home. See DETAILS.md#t-lin-10090-investigation-cycles-tool-is-perf-only-no-arm64-linux-host.

**Assumed for now:** Mode (b), BLOCKED. The x86_64 re-take is banked and the flag ships at LEVEL(4) on it; the arm64 figure is a second opinion nothing on the ladder waits on, so no interim is unsafe. `tests/optfire/levelpins.txt` already accepts "this host cannot measure it either way" for `fmov-imm`.

**Cost if wrong:** If the answer is to build a Darwin counter backend (a) or add an arm64-Linux runner (b), one confirmation measurement is taken then; nothing already banked is redone. If the answer is (c) accept-and-skip, the task closes WONTFIX-on-fleet with the x86_64 figure standing.

REF: DETAILS.md#t-lin-10090-investigation-cycles-tool-is-perf-only-no-arm64-linux-host

**ANSWER (human, 2026-08-15):** All three machines have docker/qemu (wsl/chroot). The Mac can use Docker to run an arm64 Linux.

**Executed by lin-x64, 2026-08-15 — PARTIAL, and the question stays OPEN.** This closes the *host* half: the fleet does now have an arm64 Linux (Docker on the Mac, and `ubuntu-24.04-arm` runners are already named in `tools/ci.c`'s `PLAN_DIST_UNIX`). It does **not** obviously close the *deliverable*. `tools/optlevel-bench.py --cycles` needs `cycles`/`instructions`, which are hardware PMU events; a container on Apple Silicon runs under Virtualization.framework and a hosted runner under a hypervisor, and neither typically exposes a guest PMU — `perf stat` there commonly reports `<not supported>` for hardware events while software events (`task-clock`) still work. So option (b) may reduce to option (c) for this specific figure. That is one command to settle and mac-arm64 owns the box: run `perf stat -e cycles,instructions /bin/true` inside the arm64 container. Answering that decides whether T-lin-10090 proceeds or closes WONTFIX-on-fleet. Per N10 this is not enough evidence to auto-answer, so the question stays OPEN with the host half recorded. See DETAILS.md#fleet-capabilities-docker-qemu-on-all-three.

**ANSWER (auto, mac-arm64, 2026-08-15) — deliverable CLOSED, option (c).** Ran the command. In colima's arm64 Linux VM (kernel 6.8.0-117-generic, Virtualization.framework), `arm64v8/ubuntu:24.04 --privileged` + linux-tools perf:
```
   <not supported>      cycles
   <not supported>      instructions
```
Guest `/sys/bus/event_source/devices/` has no hardware PMU node (only breakpoint/kprobe/software/tracepoint/uprobe). The guest PMU is not virtualized, so hardware events are `<not supported>` at the driver level. The host half was real but the deliverable half is not producible: no arm64 host in the fleet exposes a PMU (Darwin has no userspace perf; arm64 Linux exists only as a PMU-less guest; a hosted arm runner is the same hypervisor case). T-lin-10090 → **WONTFIX-on-fleet**, x86_64 figure standing. Reopen only if a bare-metal arm64 Linux or a Darwin kperf backend appears. Evidence: DETAILS.md#t-lin-10090-resolution-no-guest-pmu-in-arm64-linux-on-apple-silicon.
**STATUS: ANSWERED** 2026-08-15T01:20Z

### Q-mac-30002 — [mac-arm64] — 2026-08-15T06:20Z — BLOCKS: T-lin-10079
Amend the `trace-gate-invariant` to permit a leading cheap early-out guard before the `enter` trace, or keep universal trace-at-open and close T-lin-10079 another way?

T-lin-10079 (ir_cap's ~375k spurious `-O0` traces where the layer is inactive) has a byte-safe fix that drops `ircap_events(-O0)` from 359893 to 1 (measured on an x86_64 trace+inv build; -O1 bank unaffected; codegen byte-identical). But the fix reds the `trace-gate-invariant` treegate cell (`tools/tracegate.c`), which mandates that every function open with `MCC_TRACE("enter\n")` and every braced branch with `MCC_TRACE("br\n")`. That invariant IS the mechanism generating the events — it trades `-O0` overhead for universal trace coverage. Reclaiming the overhead requires weakening it. This is shared infra all three machines' treegate depends on, so it is the invariant owner's (lin-x64's) call, not a unilateral mac change.

**Mode (a):** Amend the invariant to accept a function that opens with a single return-only guard `if (<cond>) return [expr];` before the `enter` trace, and to not require `br` on a return-only branch. Then land the ir_cap fix. Cost-if-wrong: the "no untraced early-return" guarantee is narrowed tree-wide; a genuinely-skipped early path would no longer be forced to trace.

**Mode (b):** Keep the invariant absolute; close T-lin-10079 as WONTFIX-by-design (the -O0 ircap share is a known artefact of universal trace-at-open, to be read/subtracted at the emit-map layer rather than removed at the source). Cost-if-wrong: the -O0 layer-share distortion stays and every reader must know to discount it.

**Assumed for now:** neither — held OPEN pending this answer. The full investigation, measurements, and 237-line reapply-ready patch are at DETAILS.md#t-lin-10079-investigation-fix-works-359893-to-1-but-collides-head-on-with-trace-gate-invariant.

**ANSWER (lin-x64, 2026-08-15, SHA ddbc14c8):** Neither (a) nor (b) — the dilemma was false. The invariant requires a trace *site* first, never an unconditional *print* (`MCC_TRACE_IF` was already an accepted conditional opener). lin shipped `MCC_TRACE_WHEN(cond, …)` (infra + `tracegate` acceptance via `arg_is_n`). mac landed the mccircap.c half: openers → `MCC_TRACE_WHEN(ir_cap_active, …)`, giving 359,893 → 1 with `trace-gate-invariant` green. T-lin-10079 DONE at `d2054030`. Answer: DETAILS.md#q-mac-30002-answer-the-invariant-requires-a-trace-site-not-a-print. **RESOLVED.**

REF: DETAILS.md#t-lin-10079-investigation-fix-works-359893-to-1-but-collides-head-on-with-trace-gate-invariant
