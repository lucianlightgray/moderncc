---
name: always-commit-and-push-after-green
description: Standing rule — always commit AND push to origin/main once all runnable tests pass
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 028aa6b8-97bc-4230-8224-a08a0d2dd59e
---

Once the change is done and all tests I can run pass, commit **and push** to
`origin/main` without waiting to be asked each time.

**Why:** the user works linear-on-`main` and wants fixes landed, not staged
locally. Stated as a general standing rule (2026-07-08).

**How to apply:** after a green run, commit to `main`, then `git push origin
main`. The remote moves fast (other pushes land mid-task) — expect the first
push to be rejected; `git fetch` + `git rebase origin/main`, **rebuild + re-run
the suite** (upstream may have added interacting code), then push again. Don't
branch first here — direct-to-`main` matches their workflow. See
[[moderncc-windows-build]] for the build/test setup and [[ci-log-zip-commit-ahead]].
