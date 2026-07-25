---
name: docker-optimizer-mcc-validation
description: How to build+test an optimizer-enabled mcc in Docker on this Windows host (for TODO opt-gate work)
metadata: 
  node_type: memory
  type: reference
  originSessionId: ce8a644d-b75b-482c-95d6-97e871b8dc73
---

Fast way to validate `MCC_AST_*` optimizer changes without the full cmake/JIT build, on this win32 host:

Build an optimizer-enabled amd64 mcc directly from the amalgamation in a debian container:
`gcc -w -DMCC_CONFIG_OPTIMIZER=1 -Isrc -Iinclude -Isrc/formats -Isrc/objfmt -Isrc/arch/i386 -Isrc/arch/x86_64 -Isrc/arch/arm64 -Isrc/arch/arm -Isrc/arch/riscv64 -O0 -o /tmp/mcc src/mcc.c`
(bookworm-slim needs `apt-get install -y gcc` first; ~30s).

Gotchas:
- Run mcc with `-B/src -I/src/runtime/include` — the compiler's own headers (mccdefs.h, stddef.h) live in `runtime/include`, NOT `include/` (which is the public libmcc.h only).
- To avoid glibc multiarch (`bits/libc-header-start.h`) errors, use freestanding test programs: `extern int printf(const char*,...);` instead of `#include <stdio.h>`.
- mcc's own linker fails in slim images (`crt1.o not found`); isolate codegen by `mcc -c foo.c -o foo.o` then `gcc foo.o -o foo` to link+run.
- Docker on this host needs `MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*'` on every `docker run`/`exec` with `-v` mounts (MSYS rewrites container paths). Use a persistent `docker run -d ... sleep N` container + `docker exec` to build once and iterate.

This gives byte-identical-object determinism checks (`md5sum` over repeat compiles) and mcc-vs-gcc correctness. Full AOT==JIT / arm64 needs the cmake debug preset with the embed-jit blob — heavy. Amalgamation entry is `src/mcc.c` → `libmcc.c` → rest. See [[roi-scheduler-determinism]].
