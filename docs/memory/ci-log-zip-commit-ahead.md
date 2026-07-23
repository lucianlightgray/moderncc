---
name: ci-log-zip-commit-ahead
description: "When handed a CI logs_<id>.zip to fix, the failing commit is often ahead of local HEAD — get the SHA and fetch first"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 028aa6b8-97bc-4230-8224-a08a0d2dd59e
---

The user drops a GitHub Actions `logs_<runid>.zip` at the repo root and says
"Fix". The failing run is frequently at a commit **ahead of the local working
tree**, so a bug won't reproduce against local HEAD until you sync.

**Why:** local `cmake-*` build dirs and HEAD lag the pushed `origin/main` that CI
actually ran. Reproducing at the wrong commit wastes a full build+repro cycle
(hit this on run 78268310740: local was at 41e4c6ef, CI ran at 5a134051, three
commits ahead).

**How to apply:** before reproducing, pull the real commit from the logs and
sync to it. The per-job `2_Run actions_checkout@v5.txt` files (and the top-level
job logs) contain the 40-char SHA repeated many times:
- unzip, then grep a checkout log for `[0-9a-f]{40}` → that's the run's commit.
- `git fetch --all`; if the SHA isn't in local history, `git merge --ff-only
  origin/main` (or check out the SHA) and rebuild before repro.
Job-log triage: `grep -c "The following tests FAILED"` per top-level `NN_*.txt`;
per-test detail is inline under `--output-on-failure`. See [[moderncc-windows-build]].
