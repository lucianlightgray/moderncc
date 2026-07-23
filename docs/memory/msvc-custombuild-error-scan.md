---
name: msvc-custombuild-error-scan
description: "VS-generator CustomBuild fails on any canonical-error line in a custom command's output, even if the process exits 0"
metadata: 
  node_type: memory
  type: reference
  originSessionId: 61ed84fb-f511-427d-95ea-5392a1905333
---

On the Visual Studio generator (MSBuild), an `add_custom_target`/`add_custom_command`
step is reported as **failed** (`MSB8066 ... exited with code -1`) if the command's
console output contains any line matching MSBuild's canonical error format
(`origin : error CODE: message`) — **even when the process itself exits 0** and does
its job. Direct `cmd`/ctest/Ninja runs don't scan output, so a bug like this only
manifests under the VS generator (and reproduces on x64, not just arm64).

Concrete case (fixed in `tools/bench.c` `detect()`, commit 8f47db27): mccbench probed
MSVC `cl` with GNU flags (`-dumpmachine`/`--version`); cl replied
`cl : Command line error D8003: missing source filename` on stderr, which failed the
`bench` target. Fix: probe cl's version from the banner it prints to **stderr when run
with no args** (exit 0, no error line), captured so nothing leaks.

Takeaway for any custom target that spawns tools on Windows: keep child stderr/stdout
either redirected/captured or free of `... : error ...`-shaped lines. mccbench's
*measurement* spawns already dodge this by redirecting child output to NUL; only the
uncaptured *probe* leaked. See [[moderncc-windows-build]].
