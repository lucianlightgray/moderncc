---
name: drive-wsl-from-git-bash-msys-guard
description: "Driving WSL (wsl.exe) from this host — Git Bash needs MSYS_NO_PATHCONV or it mangles Unix-path args; PowerShell renders wsl's own output as UTF-16"
metadata: 
  node_type: memory
  type: reference
  originSessionId: 02515c73-126c-4612-9b64-0f5c21a87765
---

This Windows host has WSL2 with two distros: `docker-desktop` (Docker Desktop's
backend) and **`Ubuntu` (26.04 LTS, x86_64)** installed 2026-07-22 via
`wsl --install -d Ubuntu --no-launch` (the `--no-launch` skips the interactive
OOBE user-create that would hang a non-interactive shell → only `root` exists
until first-run setup is completed; use `-u root`).

**Driving WSL from the Bash tool (Git Bash / MSYS):** works and is more
ergonomic than PowerShell (natural bash quoting for nested `bash -c`; clean UTF-8
from the Linux side — PowerShell renders `wsl.exe`'s own management output like
`-l -v`/`--status` as UTF-16 spaced-out letters). BUT you MUST set
`MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*'`, because MSYS silently rewrites
Unix-path ARGUMENTS passed directly to `wsl.exe` into Windows paths. Proven:
`wsl -d Ubuntu -u root readlink -f /etc` → empty (mangled) without the guard,
→ `/etc` with it. Paths *inside* a quoted `bash -c '… /etc/…'` are safe either
way (not seen as separate args). Same guard the repo's docker scripts use
(see [[x86-optimizer-differential-docker]], [[moderncc-windows-build]]).

**Why WSL Ubuntu matters for the loop:** a native x86_64 Linux env without the
Windows→Linux 9p/virtiofs bind-mount stalls that plague Docker here — clone/copy
the repo into the WSL fs and build there. Cross triples still need
`apt install qemu-user-static gcc-<arch>-linux-gnu` inside Ubuntu. Complements,
doesn't replace, the Docker path. See [[native-x86-first-docker-for-cross]].
