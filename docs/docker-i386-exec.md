---
name: docker-i386-exec
description: Docker linux/386 runs i686 binaries on the arm64 macOS host — the path to ungating i386-execution tests
metadata: 
  node_type: memory
  type: reference
  originSessionId: 50380c9a-e3de-41d4-8d8f-7f8c2a25aa75
---

`docker run --rm --platform linux/386 i386/debian:bullseye-slim uname -m` →
`i686` on this Apple-Silicon macOS host (Docker Desktop emulates). This is the
viable execution environment for i386-**Linux** test ungating, because:
- qemu-i386 user-mode is NOT installed (brew qemu on macOS ships system
  emulators + `qemu-system-i386`, not linux-user `qemu-i386`).
- 32-bit wine on arm64 macOS is not viable for i386-PE.
- `i686-w64-mingw32-gcc` exists (i386-**PE**), but the fastcall harness
  `host_spawn_ex`'s the linked exe, which a 32-bit PE can't satisfy here.

For "ungate i386-fastcall-abi": the ABI is now validated on this host via
`tools/i386fastcall-docker.sh` (committed 18378bb5). mcc-i386 emits i386 ELF on
the host; a linux/386 container does the reference gcc build + 3-way cross-link
(mcc↔gcc both directions + mcc↔mcc) + exec + the float-before-reg reject check.
All pass against `cmake-cross/mcc-i386`, confirming fastcall codegen is
gcc-compatible both ways. Remaining: wire the script into CMake as a
docker-gated ctest (skip 77 when docker/mcc-i386 absent). The native harness
`suite_i386fastcall` (tools/mccharness.c:1486) still does compile+link+**run**
all on the host (CMakeLists.txt ~4551, gated on `TARGET mcc-i386`), so it can't
work off-i386 without a Docker execution backend. Related: [[docker-amd64-repro]].
