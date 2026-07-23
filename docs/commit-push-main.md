---
name: commit-push-main
description: "User wants changes committed and pushed directly to main, no feature branch"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 36903e1e-32ea-4184-9758-aa7d4969b3ac
---

The user works directly on `main` and wants me to commit and push there without branching first.

**Why:** Solo-owned repo; the default "branch off main before committing" harness rule is unwanted friction here.

**How to apply:** When work is done and verified, commit on `main` and `git push` — do not create a feature branch or open a PR unless asked. Still only commit when the change is complete and the user has authorized it (this standing preference counts as authorization). Related: [[ci-workflows-generated.md]].
