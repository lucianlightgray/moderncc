---
name: dist-diagrt-off-embedjit-build
description: Dist build is the only config that is non-amalgamated + diag-rt=off + embed-jit; catches decls guarded behind MCC_CONFIG_DIAG_RT>=2 that new unconditional callers need
metadata: 
  node_type: memory
  type: project
  originSessionId: 0e23b11d-c35b-4529-991e-a7ec2049ddef
---

The **Dist** workflow (`.github/workflows/dist.yml`, `mcc-ci dist --preset ... --plat ...`) builds each source as its own TU (`MCC_AMALGAMATED=0`) with `--diag-rt off` (`MCC_CONFIG_DIAG_RT=0`) **and** `--embed-jit`. That specific combo is NOT exercised by the test Matrix (which is amalgamated and/or diag-rt>=2), so a whole class of regression is Dist-only and stays green everywhere else.

Failure mode: a helper whose **definition** is unconditional (e.g. `get_sym_ref` in mccgen.c) but whose **declaration** in mcc.h is gated behind `#if MCC_CONFIG_DIAG_RT >= 2` (a stale bounds-checker guard). When a new unconditional caller appears (e.g. `mccjit_intent.c` MCCJIT_ROLE_DATA path, or the mccast.c AOT slot path), the separate TU can't see the prototype → implicit-declaration hard error under clang/mingw (C99+) + silent int→pointer corruption. All Dist platforms die identically (linux/macos clang + mingw), so it reads like "many failures" but is one root cause. Fix: declare unconditionally.

**Repro locally (this is a Mac, the failing jobs include macos-arm64-clang):**
- Fast single-file: take libmcc.c's compile command from `cmake-build-embedjit/compile_commands.json`, flip `MCC_CONFIG_DIAG_RT=2`→`=0`, compile the suspect file standalone with `-fsyntax-only`.
- Full: `cmake -S . -B cmake-build-distrepro -G Ninja -DCMAKE_BUILD_TYPE=Release -DMCC_EMBED_JIT=ON -DMCC_CONFIG_OPTIMIZER=ON -DMCC_CONFIG_DIAG_RT=off` then `cmake --build cmake-build-distrepro`.

See [[build-dir-prefix]], [[ci-workflows-canonical]], [[mcc-jit-unification]].
