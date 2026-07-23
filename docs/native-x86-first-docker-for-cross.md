---
name: native-x86-first-docker-for-cross
description: "This host is x86_64-native — validate x86/x86_64 on-box (native ctest), use Docker/qemu ONLY for non-native triples"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 02515c73-126c-4612-9b64-0f5c21a87765
---

The Windows dev host is **x86_64-native**. Validation policy for the TODO loop:
validate x86 and x86_64 changes **directly on this machine** (CLion mingw/MSVC
`ctest`, see [[moderncc-windows-build]]) — do NOT reach for Docker for anything
x86. Docker/qemu is only for the OTHER triples that can't run natively here
(riscv64 / arm64 / arm / cross-ABI checks).

**Why:** the loop instruction "Use Docker after native to validate other triples"
means native-first. Native x86 ctest is faster and avoids the Windows→Linux
bind-mount + concurrent-gcc-pipe pain of the Docker path.

**How to apply:** any slice that touches shared files (e.g. `mccast.c` promotion
`#if`, shared codegen) must pass a **native x86_64 ctest on this box** as its
regression/byte-identity gate before commit, even when the slice's primary
feature targets a cross triple (which is Docker-validated separately). See
[[x86-optimizer-differential-docker]] for the one native-x86 exception that still
needs a container: an *optimizer-enabled* build the stock native cross lacks.
