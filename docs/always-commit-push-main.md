---
name: always-commit-push-main
description: "User wants fixes committed and pushed straight to main, no branch/PR"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: cabf3d63-8b96-4086-add4-ca8dcdf47d95
---

For this repo (moderncc), the user works trunk-based: commit fixes and push directly to `main` without asking, no feature branch or PR.

**Why:** Solo maintainer; CI on main is the gate, and they iterate by pushing and reading the resulting CI run.

**How to apply:** After a verified fix, commit (split unrelated fixes into separate logical commits) and `git push origin main`. If the push is rejected, `git fetch` + `git rebase origin/main` then push again — main advances frequently from other sessions. This overrides the default "commit/push only when asked" and "branch first" guidance.
