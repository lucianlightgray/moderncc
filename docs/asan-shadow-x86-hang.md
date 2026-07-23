---
name: asan-shadow-x86-hang
description: "x86_64 -fasan-shadow heap-test hang root cause (unmapped ShadowGap) + qemu can't run the shadow runtime"
metadata: 
  node_type: memory
  type: project
  originSessionId: 9bd9e324-1267-4171-9bfa-1e5adcd69786
---

The x86_64 `cli/asan_shadow_native_{overflow,use_after_free}` + `asan_shadow_manual_link` CI hangs (90s Timeout, added by 867a688c) were the ShadowGap left unmapped: `asan_init` in `runtime/lib/mccasan.c` mapped LowShadow + HighShadow as two regions, so when ASLR placed a heap block (our mmap malloc) or the SIGILL reporter's `s[-8..7]` dump window such that the shadow addr fell in the gap, the shadow access nested-faulted inside the handler → deadlock (0 CPU, SIGTERM-masked, ASLR-flaky). Fix (commit 9623a4ed, on 2026-07-20): map ONE contiguous NORESERVE region `[0x7fff8000,0x10007fff8000)` covering Low+Gap+High, matching the arm64 branch. **CI-verification-pending** — see below.

Gotcha: **qemu-user (amd64-on-arm64 Docker) cannot run the -fasan-shadow runtime at all** — even a trivial program doing just the ~16TB `MAP_FIXED|MAP_NORESERVE` shadow mmap hangs under emulation (killed by timeout). So this hang is NOT locally reproducible off a native x86_64 host; my [[docker-amd64-repro]] path is useless for asan-shadow. Verify via real x86_64 CI only. Related: [[macos-arm64-status]].
