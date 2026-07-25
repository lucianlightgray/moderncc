---
name: push-to-main-needs-authorization
description: User wants the /loop to push to main; the harness classifier may still gate each push
metadata: 
  node_type: memory
  type: feedback
  originSessionId: ce8a644d-b75b-482c-95d6-97e871b8dc73
---

The project's `docs/TODO.md` "How to process" says: "Mark an item in progress, commit+push to main, then start it." Committing directly to `main` and pushing is the intended workflow (solo self-host compiler repo; all work lands on main).

**Why:** The Claude Code auto-mode classifier blocks `git push origin main` by default ("bypasses PR review… user never explicitly authorized"). On 2026-07-25 the user **explicitly authorized it** mid-loop ("pull, merge, commit and push to main"), and the push then succeeded (`4ccc3860..4a5b416a`). So the user genuinely wants the loop to push to main.

**How to apply:** In the `/loop` sessions, commit work locally, then push to main — the user wants it. If a specific push is still gated by the classifier (each push is re-evaluated, so a fresh session may block again), do the local commit and surface a one-line note asking the user to approve or to add a `git push` Bash permission rule (that would let every future loop iteration push without prompting). Never work around a denial maliciously. See [[roi-scheduler-determinism]].
