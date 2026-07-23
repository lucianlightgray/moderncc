---
name: docker-macos-tmp-mount
description: Docker bind mounts on macOS only share /Users — never mount /tmp or the scratchpad
metadata: 
  node_type: memory
  type: reference
  originSessionId: 10e773c4-467a-4c00-ba6c-1c5adf74523e
---

On this macOS host, Docker Desktop file-sharing only exposes paths under `/Users`. A
`-v /tmp/foo:/x` (or a `/private/tmp/claude-*` scratchpad path) bind-mounts an EMPTY dir
inside the container — the container sees no files, and commands fail confusingly ("no such
file", or a script produces zero output). This wasted several tool calls twice during the
2026-07-20 CI fix.

Rules for Docker repros here:
- Put any bind-mounted workdir UNDER the repo (`/Users/llg/Projects/moderncc/...`), e.g. a
  gitignored `.w-foo/` dir; clean it up after.
- Don't `-v /tmp/script.sh:/s.sh` — inline the script into `sh -c '...'` instead.
- When copying source into a container, EXCLUDE `cmake-build-*`, `.git`, `vendor` (the
  gentoo stage3 has GB of dangling symlinks) or `cp`/`tar` stalls/fails silently.
See [[docker-amd64-repro]], [[docker-i386-exec]].
