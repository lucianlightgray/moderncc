---
name: x86-optimizer-differential-docker
description: "How to validate mcc's AST optimizer gates (divmagic + the flip-pending family) on x86 via Docker on this Windows host — the recipe, the -O1/-O2 gate-firing gotcha, and the tracked templates"
metadata: 
  node_type: memory
  type: project
  originSessionId: 9f316561-fde9-407b-ac32-978f8fb64b01
---

Validating mcc's AST reemit optimizer (divmagic, branchless select, promotion, replay-inline, the `MCC_AST_*` flip-pending family) on x86 from this Windows host, differentially vs gcc under Docker. Hard-won during the 2026-07-22 x86 loop.

**The normal build has NO optimizer.** `cmake-cross/mcc-i386` (and CI cross builds) compile with `-DMCC_TARGET_I386` but NOT `MCC_CONFIG_OPTIMIZER`, so the entire reemit optimizer is compiled out — you cannot exercise any gate with the stock cross mcc. Build your own in a linux/amd64 container (mcc is a single-source amalgamation):
`gcc -O1 -w -DMCC_CONFIG_OPTIMIZER=1 [-DMCC_TARGET_I386=1] -I src -I src/arch/{i386,x86_64,arm,arm64,riscv64} -I src/objfmt -I src/formats -I include src/mcc.c -o mcc-opt`
(x86_64 target is the host default — omit `-DMCC_TARGET_*`. `libmcc.h` is in `include/`, `dwarf.h` in `src/formats/` — need all those `-I`.)

**GOTCHA that wastes hours: the reemit optimizer only runs at `-O1`/`-O2`.** `ast_replay_env = (optimize>=1 || embed_jit)`. Setting `MCC_AST_DIVMAGIC=1` / any `MCC_AST_*=1` at `-O0` is INERT — zero codegen delta. Isolate a gate by compiling the SAME program at `-O2` with the gate OFF vs ON and diffing `objdump -d` (strip objdump's `file format`/filename header line first — it embeds the .o name and makes false diffs; reuse one fixed object filename).

**Test programs must be FREESTANDING** (no libc headers — the cross mcc lacks the container's multiarch headers): `extern int printf(const char*,...);`, define your own INT_MIN etc., link with gcc which supplies crt + printf. mcc force-includes `<mccdefs.h>` → add `-I runtime/include`. **Put each optimized construct in its own small leaf function** — the reemit optimizer BAILS on large/volatile/call-heavy functions, so a single giant `main()` never triggers the pass.

**Docker on this Windows host:** prefix every `docker run` with `MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*'` (else git-bash rewrites container `/w`→`W:/`). Host path for `-v` via `HP="$(pwd -W)"`. The Windows→Linux bind mount is GLACIAL for many small reads (cmake configure stalled in D-state >11 min) — mount the repo RO and `cp -a /repo/{src,include,runtime} /b/`, then build from the copy. i386 ELF output runs under a `linux/386` container (host binfmt); 64-bit divmagic needs `i386-libmccrt.a` (`__mcc_?mulh64`).

**Self-compiling `src/mcc.c` with your opt mcc (e.g. to repro a search/codegen fault on a real large TU), two extra pieces beyond the freestanding recipe** (2026-07-22): (1) the amalgamation includes the generated `mccdefs_.h` under `#if MCC_CONFIG_PREDEFS` — it does NOT exist in a raw tree; build `tools/c2str.c` and run `c2str runtime/include/mccdefs.h /gen/mccdefs_.h`, add `-I/gen`. (2) mcc.c pulls REAL libc headers (`stdio.h`…), so the self-compile needs `apt-get install libc6-dev` + `-isystem runtime/include` (mcc's own `stdarg.h`/`stddef.h`) + `-isystem /usr/include -isystem /usr/include/x86_64-linux-gnu`; use `-c` (compile-only) to skip the crt1.o/link path entirely. Used this to confirm the `MCC_AST_SEARCH=1 -O4 src/mcc.c` segfault (old JIT-§ TODO) no longer reproduces on native Windows-PE OR Linux-ELF (both rc=0 at -O4/-O10/+emitsize) → item pruned.

**Tracked templates that already encode this:** `tools/i386divmagic-docker.sh` (the `i386-divmagic-soak-docker` ctest — self-builds the opt mcc, 8.6M-check divmagic soak vs a volatile-divisor idiv oracle) and `tests/qemu/native-optcheck.sh` (opt-ON build + `^exec/`×2, gated≤default; now stages a container-local source copy for the Windows bind-mount). Result of the sweep: 0 miscompiles; divmagic + 8/11 other gates fired+clean on x86. See [[moderncc-windows-build]], [[ci-emit-drift-eol]].
