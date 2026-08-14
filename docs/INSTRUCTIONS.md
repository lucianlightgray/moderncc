# INSTRUCTIONS.md — Multi-Machine Coordination Protocol

Three concurrent Claude sessions, one repo, one branch (`main`). This file is human-edited only and **read-only to sessions**; re-read it at PREFLIGHT and whenever a pull shows it changed.

| Session ID  | Platform | Arch  | ID band     |
| ----------- | -------- | ----- | ----------- |
| `mac-arm64` | macOS    | arm64 | 30000–49999 |
| `lin-x64`   | Linux    | x64   | 10000–29999 |
| `win-x64`   | Windows  | x64   | 50000–69999 |

**Prime directive:** research once, encode as a contract, fan out implementation. Every unit of work is done exactly once unless typed per-platform.

**IDs:** `T-lin-10042`, `Q-mac-30007`. One counter per session across all kinds, stored in the Sessions table `Next ID` column: read your row, mint, increment **in the same commit**. Only you write your row. Never mint outside your band; never reuse an ID. All timestamps UTC.

## 0. Assumptions (verify at first run; mirror into `QUESTIONS.md`, never ask in-band)

- `SendMessage` delivers to a named session, durably. Even so: the repo, not the bus, is the source of truth — the protocol must survive total message loss.
- Sessions are addressable by the IDs above.
- All machines can push to `main`; no branch protection blocks doc commits.

## 1. Files

| File              | Contains                                                                      | Pattern                       |
| ----------------- | ------------------------------------------------------------------------------ | ----------------------------- |
| `INSTRUCTIONS.md` | This protocol                                                                  | Read-only to sessions         |
| `TODO.md`         | Task **state only**: ID, type, title, owner, state, pointer, timestamp         | High churn, ownership-zoned   |
| `DETAILS.md`      | Research, contracts, nuance, rationale, evidence, verification specs           | Append-only, section-keyed    |
| `QUESTIONS.md`    | **Open** questions only                                                        | Append-only, pruned on answer |
| `ARCHIVED.md`     | One-line records of DONE tasks + answered questions (formats §8/§9)            | Append-only                   |

All paths under `docs/`. **Hygiene:** `DETAILS.md` is the only unbounded file — all depth goes there. Every other doc stays succinct and pristine: exactly the context needed to take and complete work, behind pointers. **Routing:** needed to *decide what to do* → `TODO.md`; needed to *do it correctly* → `DETAILS.md`. `TODO.md` entries are one line + pointer, never prose.

## 2. Task types (mandatory at creation; untyped = unclaimable)

| Type  | Meaning                                                 | Claiming                                 |
| ----- | ------------------------------------------------------- | ---------------------------------------- |
| `[S]` | Platform-agnostic; done **once**                        | First pushed claim wins; never duplicate |
| `[P]` | Required on **all three** platforms (valid duplication) | Fans into `<ID>/mac`, `/lin`, `/win`     |
| `[X]` | One platform only (e.g. code signing)                   | Only the named session may claim         |
| `[C]` | Interface others depend on; blocks dependent `[P]` work | One owner; highest priority              |

- **DEPS gate:** claimable only when every `DEPS` entry is DONE.
- **Anti-redundancy:** before creating `[S]`/`[C]`, grep `TODO.md`, `ARCHIVED.md`, `DETAILS.md`; cite existing anchors, don't re-research.
- **Fan-out:** `[P]` parent stays open until all children DONE. Creator writes the shared approach to `DETAILS.md` *first*. The session completing the last child closes and archives the parent in the same commit.

## 3. Task states

```
OPEN → CLAIMED → IN_PROGRESS → { DONE | BLOCKED }
  ↑         │           │              │
  └─────────┴───────────┴──────────────┘   TTL expiry or unblocking re-OPENs
```

- `OPEN`: typed, unowned, DEPS satisfied. `CLAIMED`: owner set and pushed, not started. `IN_PROGRESS`: owner heartbeats each checkpoint. `DONE`: per §8.
- `BLOCKED`: owner released, question logged (§9), leaves the active queue. Sessions never idle on it (N8); the system-wide loop does **not** terminate while BLOCKED tasks remain (§10.11).
- **3 h TTL — sole staleness mechanism.** Heartbeat = last push **by the owner** touching the task (commit timestamp + author, not the `TS:` field; third-party touches never reset it). Re-OPEN only when a fresh pull confirms heartbeat >3 h: clear owner; record in `DETAILS.md` the prior owner, reason, **last green SHA, and remaining work** (next claimant resumes, not restarts); broadcast `CLAIM`. Re-opener needn't take the task.
- **Owner re-verification:** before pushing any result, confirm post-pull you still own the task. If not, abandon the push; salvage output as a `DETAILS.md` note + `FYI`. Remote ownership beats local memory.
- **Heartbeat-only commits are legal:** a `docs(TODO)` TS bump before the TTL lapses. Size slices so real checkpoints land well inside 3 h.

### TODO.md line format

```
- [ ] T-lin-10042 [P] Implement config file watcher
      OWNER: lin-x64 | STATE: IN_PROGRESS | SHA: a1b2c3d | TS: 2026-08-14T10:22Z
      REF: DETAILS.md#t-lin-10042-config-watcher | DEPS: T-mac-30038[C]
```

`SHA:` = last relevant **content** commit (contract/DETAILS anchor), not the claim commit.

## 4. Git protocol

### 4.1 Setup (every machine)

```bash
git config pull.rebase true
git config rebase.autoStash true
git config core.autocrlf false   # critical on win-x64
```

`.gitattributes` (created at PREFLIGHT bootstrap, before any concurrent appends):

```
* text=auto eol=lf
docs/DETAILS.md   merge=union
docs/ARCHIVED.md  merge=union
docs/QUESTIONS.md merge=union
```

`merge=union` keeps both sides of concurrent appends (dedupe opportunistically); never apply to `TODO.md`. **In union-merged files, never edit existing lines** (concurrent edits duplicate; deletions resurrect) — state changes are appends, e.g. `**STATUS: ANSWERED** <TS> <SHA>`. Only archival removes blocks, and only ones no longer written to.

### 4.2 Commit classes — never mix in one commit

| Class  | Touches              | Gate                   |
| ------ | -------------------- | ---------------------- |
| `docs` | `docs/**` only       | None; push immediately |
| `code` | source, build, tests | Green per §8           |

Prefixes: `docs(TODO):`, `docs(DETAILS):`, `feat(lin-x64):`, `fix(win-x64):`, `contract(T-mac-30038):`.

### 4.3 Claim-by-push — a claim exists only on the remote

```bash
git pull --rebase
# TODO.md: set OWNER + STATE: CLAIMED
git add docs/TODO.md      # explicit add — never -a (smuggles code into docs commits)
git commit -m "docs(TODO): claim T-lin-10042 [lin-x64]"
git push
```

Push rejected → pull-rebase, re-read the task; if now owned, drop your claim and re-select. **Never force-push or rewrite pushed history.**

### 4.4 Conflicts

- `TODO.md`, zone you don't own → theirs. Your zone → yours — unless the line is a task you no longer own (§3 re-verification); then theirs.
- `TODO.md` shared **Open**/**Invalidations** zones → keep both; competing claims: first push wins, loser re-selects.
- Sessions table → row-wise to the row's owner.
- Union-merged files → keep both, dedupe.
- Source → first push wins; second rebases and re-verifies before pushing.

## 5. SendMessage

**Messages carry pointers, not payloads. Push first; cite the SHA.** Content not in a pushed commit is divergent truth. Sole exemption: one-line acks (§5.4).

### 5.2 Tiers

| Tier       | Meaning                                  | Receiver action                        |
| ---------- | ---------------------------------------- | -------------------------------------- |
| `BLOCKER`  | Receiver's in-flight work is invalid     | Interrupt now; pull, reassess          |
| `CONTRACT` | Interface/schema/CLI/shared type changed | Interrupt now; pull before next commit |
| `CLAIM`    | Ownership taken, released, re-OPENed     | Consume at next checkpoint             |
| `FYI`      | Finding, gotcha, perf note, dead end     | Batch; consume at next checkpoint      |

Only `BLOCKER`/`CONTRACT` interrupt an in-flight slice. Never inflate `FYI` to `BLOCKER`.

**Lost-BLOCKER recovery:** in the same push, the sender appends to `TODO.md § Invalidations` (shared, append-only — never the owner's zone, whose conflict rule would discard it):

```
INVALID: T-lin-10042 AS-OF a1b2c3d BY mac-arm64 — REF: DETAILS.md#anchor
```

Every session checks § Invalidations at every pull. An owner finding their own task there stops immediately, flips it to `OPEN` (or `BLOCKED` + question if a human is needed). The line is removed by whoever re-scopes the task.

**Escalation:** a `BLOCKER` not resolvable autonomously must become a `QUESTIONS.md` entry (§9); the human answer unblocks downstream. It never silently expires.

### 5.3 Envelope

```
TIER:  CONTRACT
FROM:  lin-x64
TO:    mac-arm64, win-x64
TASK:  T-mac-30038
SHA:   a1b2c3d
REF:   docs/DETAILS.md#t-mac-30038-ipc-envelope
SUBJ:  IPC frame header grew 4 bytes; magic changed to 0x43414C4D
ACT:   Re-read the anchor before writing framing code; existing framing is now wrong.
```

`ACT:` mandatory: what the *receiver* does.

### 5.4 Inbox

- Drain before reading `TODO.md` (§10.2–3) and at every checkpoint.
- Ack every consumed `CONTRACT`/`BLOCKER` with one line citing the SHA (push-first exempt).
- Broadcast only cross-cutting changes; otherwise target.

## 6. Research phase

1. **Dedup:** grep `DETAILS.md`, `TODO.md`, `ARCHIVED.md`, `QUESTIONS.md`; cite, don't re-derive.
2. **Announce:** push a `CLAIM` before dispatching agents.
3. **Dispatch agents in parallel** — one per candidate approach or platform.
4. **Score**, in order: (1) cross-platform viability — failing one platform loses regardless of attestation; (2) corroboration across independent sources; (3) fewest new dependencies; (4) recency/maintenance.
   > Online consensus is overwhelmingly the x64-Linux answer. Score consensus **per platform**, then combine.
5. **Publish before implementing:** chosen approach, rejected alternatives + why, platform deltas, verification spec (§8) → `DETAILS.md`. Push. Broadcast `CONTRACT` (if it constrains others) or `FYI`.
6. **Answer questions en passant** per §9 when research thoroughly resolves one.
7. **Taskify:** typed tasks with `DEPS` → `TODO.md`. Push.

**Divergence fallback:** no approach viable on all three → one `[C]` common abstraction + three `[X]` backends; rationale → `DETAILS.md`.

## 7. Implementation phase

- **TDD:** write/augment the smoke test first, implement until it passes. Each slice's tests validate exactly that slice's work.
- Slices ≈ one verifiable behavior, checkpoint always inside the 3 h TTL.
- `[C]` implemented and pushed **before** anyone claims dependent `[P]` work.
- **Spike budget:** fall back only after verification *proves* the approach cannot complete the task — never on a hunch, never during research. On proof: failure + evidence → `DETAILS.md`; broadcast `BLOCKER` (+ § Invalidations line) to sessions on the same approach; re-enter §6.4 with the next candidate.
- A per-platform discovery others will hit → send immediately at the correct tier.
- No comments in source; nuance → `DETAILS.md` under the task's anchor.

## 8. Definition of done

**Green:** *per slice* — that slice's smoke tests pass on this platform (gates every `code` push); *per task* — the **full native test suite** passes on the owning platform (`[P]` parent: all three children green natively — never assume portability). **Every task's** `DETAILS.md` anchor states its verification command(s); green = exactly those pass. No spec → write one before DONE.

DONE requires, in order:
1. Green per above.
2. Nuance in `DETAILS.md` with a stable anchor.
3. `code` (as applicable) and `docs` commits pushed.
4. Completion announced (`CLAIM`) with the merge SHA.
5. **Only then** migrate: TODO removal + archive record in **one atomic `docs` commit**. Detail worth keeping → `DETAILS.md`. Record:

```
- T-lin-10042 Implement config file watcher | DONE 2026-08-14T14:02Z | SHA e4f5a6b | REF DETAILS.md#t-lin-10042-config-watcher
```

## 9. Questions

Never ask the human in-band. Dedupe against `QUESTIONS.md`, append, push, continue. Format (succinct; depth via REF):

```
### Q-lin-10007 — [lin-x64] — 2026-08-14T11:40Z — BLOCKS: T-lin-10051, T-win-50002
Question text.
**Assumed for now:** interim assumption, if one is safe.
**Cost if wrong:** what gets redone.
REF: DETAILS.md#q-lin-10007
```

**Two modes, chosen explicitly:** **(a)** safe assumption exists → task stays active under the recorded assumption; question is advisory (justified by low cost-if-wrong). **(b)** none → task `BLOCKED`, owner released, immediately claim the next task.

**Human answer:** human appends `**ANSWER:** …`. First session to observe it executes, per affected-task state:
- `BLOCKED` → `OPEN`.
- Active mode-(a) → `CONTRACT` to the owner, who adjusts in-flight. Never yank an owned task to OPEN.
- DONE mode-(a) contradicted by the answer → mint a remediation task scoped by "Cost if wrong".

Then: add/update `TODO.md` items per the answer, append `**STATUS: ANSWERED** <TS> <SHA>`, broadcast `CONTRACT`.

**Autonomous answer:** research/test evidence that **thoroughly** resolves an open question → append `**ANSWER (auto, <SessionId>, <SHA>):** …` citing the `DETAILS.md` evidence anchor; execute the same per-state steps. Partial/speculative findings are appended as notes; the question stays open.

**Migration:** on ANSWERED, substance → relevant docs (evidence → `DETAILS.md`; direction → `TODO.md` items); at the next checkpoint prune the block to an archive record:

```
- Q-lin-10007 <title> | ANSWERED 2026-08-14T15:11Z | SHA c7d8e9f | REF DETAILS.md#q-lin-10007
```

## 10. The loop

```
0.  PREFLIGHT   (first run) git config §4.1. If TODO.md or .gitattributes absent,
                create both (Appendix skeleton + §4.1) via claim-by-push; losers
                rebase in, add only their own Sessions row. Register SessionId +
                Next ID. Push. Re-read INSTRUCTIONS.md now and on any change.
1.  PULL        git pull --rebase
2.  DRAIN INBOX BLOCKER/CONTRACT first.
3.  READ STATE  TODO.md + DETAILS.md anchors from inbox. Check § Invalidations
                (own tasks → stop/flip, §5.2), QUESTIONS.md ANSWERs (execute §9),
                TTL-expired claims (§3, post-pull owner heartbeat >3 h).
4.  CLAIM       Select by §11 (DEPS satisfied only); claim-by-push. Lost race →
                re-select. Broadcast CLAIM.
5.  RESEARCH    §6: dedup → dispatch → score → publish (+spec) → answer → broadcast.
6.  TASKIFY     Findings → typed tasks with DEPS. Push.              ← CHECKPOINT
7.  IMPLEMENT   §7: set IN_PROGRESS + push before starting. TDD. Parallel agents
                where slices are independent.
8.  VERIFY      §8: slice smoke tests per push; full native suite for DONE.
9.  PUBLISH     Re-verify ownership (§3). Nuance → DETAILS.md; status → TODO.md.
                Push. Broadcast.                                      ← CHECKPOINT
10. ARCHIVE     DONE + ANSWERED → ARCHIVED.md records. Push.
11. CONTINUE    Follow-ups → 1. Queue empty but tasks BLOCKED/owned elsewhere →
                wait 10 min → 1. After 6 consecutive empty polls: STOP with a
                one-line handoff note in TODO.md (state is in the repo; human
                relaunches after answering). Empty + nothing BLOCKED + nothing
                in flight anywhere → STOP.
```

**Checkpoint** = pull-rebase → commit (update own `Last seen`) → push → drain inbox → send batched `FYI`s. At every clean boundary and at least every 3 h. Research publishes *before* implementation; implementation findings feed the next iteration's research.

## 11. Scheduling priority (take highest available)

1. `[C]` with dependents waiting.
2. `BLOCKER`-tier inbox items.
3. `[X]` only this session can do.
4. `[P]` children with parent contract published.
5. `[S]` — prefer ones no other session is near.
6. Grooming, dedupe, archive tidying.

Prefer `[X]` over `[S]`: anyone can do `[S]`; only you can do `[X]`.

## 12. Cross-platform hazards (bake into task creation)

- **Line endings:** `core.autocrlf false` + `eol=lf`, or win-x64 whole-file diffs conflict every push.
- **Case:** macOS/Windows case-insensitive, Linux not. Enforce lowercase paths.
- **Paths:** never hardcode `/`; Windows path-length limits bite deep trees.
- **Arch:** arm64 vs x64 affects prebuilt binaries, native modules, SIMD, containers. A dep on `lin-x64` may lack a `darwin-arm64` artifact — check during research, not build.
- **Toolchain drift:** pin versions in a contract task.

## 13. Invariants

### ALWAYS

- **A1** Update `TODO.md` state/pointers as they change.
- **A2** Push `docs` commits immediately; `code` at every green checkpoint. Pull-rebase before every push.
- **A3** `IN_PROGRESS` + push *before* starting work; broadcast after, citing the SHA.
- **A4** Relay emergent cross-session info at the correct tier as soon as pushed.
- **A5** Archive in §8/§9 record format — after announcing, never before; removal + record in one commit.
- **A6** Type every task; mint IDs from `Next ID`, increment same commit.
- **A7** Search existing docs before researching or creating a task.
- **A8** Checkpoint or heartbeat-bump ≤3 h while holding any claim; update `Last seen` each checkpoint; re-verify ownership before pushing results.
- **A9** Escalate unresolvable BLOCKERs to `QUESTIONS.md`; auto-answer on thorough evidence; migrate answer substance to the relevant docs.
- **A10** TDD; every slice extends its smoke tests; full native suite before DONE; every task carries a verification spec.
- **A11** Every doc except `DETAILS.md` stays succinct and pristine; depth behind pointers.
- **A12** TTL re-OPENs record last green SHA + remaining work — resume, not restart.

### NEVER

- **N1** No comments in source; nuance → `DETAILS.md`.
- **N2** No in-band questions; → `QUESTIONS.md` with mode (a)/(b) chosen explicitly.
- **N3** Never force-push or rewrite pushed history.
- **N4** Never push a `code` commit whose slice smoke tests aren't green.
- **N5** Never claim without pushing the claim; never claim with unfinished DEPS.
- **N6** Never duplicate `[S]`; never skip `[P]` on any platform.
- **N7** Never send a message whose content isn't in a pushed commit (acks exempt).
- **N8** Never idle waiting on a blocked task.
- **N9** Never mint outside your band or reuse an ID; never write another session's row.
- **N10** Never auto-answer on partial or speculative evidence.
- **N11** Never re-OPEN a claim with owner heartbeat <3 h (post-pull); never edit lines in union-merged files; never flip an actively-owned task to OPEN when executing an answer.
- **N12** Never edit `INSTRUCTIONS.md`.

## Appendix — TODO.md skeleton

```markdown
# TODO

## Sessions
| SessionId | Platform | Arch  | Band        | Next ID | Last seen         |
| --------- | -------- | ----- | ----------- | ------- | ----------------- |
| mac-arm64 | macOS    | arm64 | 30000–49999 | 30000   | 2026-08-14T10:15Z |
| lin-x64   | Linux    | x64   | 10000–29999 | 10000   | 2026-08-14T10:22Z |
| win-x64   | Windows  | x64   | 50000–69999 | 50000   | 2026-08-14T09:58Z |

## Contracts — blocking, highest priority
## In progress — mac-arm64   ← only mac-arm64 writes this zone
## In progress — lin-x64     ← only lin-x64 writes this zone
## In progress — win-x64     ← only win-x64 writes this zone
## Open — claimable
## Blocked — awaiting QUESTIONS.md
## Invalidations             ← shared, append-only; removed only on re-scope (§5.2)
```

Each session writes only its own Sessions row. Zones keep status churn out of others' lines so rebases apply cleanly. If `TODO.md` still conflicts under load, escalate to per-session files (`docs/todo/<session-id>.md`) with `TODO.md` as backlog + index.
