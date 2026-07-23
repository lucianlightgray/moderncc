---
name: docker-amd64-repro
description: Reproduce x86_64/Release-specific mcc failures on the arm64 host via a linux/amd64 Docker container
metadata: 
  node_type: memory
  type: reference
  originSessionId: 4a27ad4b-e020-4eb7-b0c9-a42b8f1abc66
---

To debug x86_64-only or Release-only mcc failures from an arm64 macOS host, run a persistent `linux/amd64` Docker container (QEMU-emulated): `docker run -d --platform linux/amd64 -v <repo>:/moderncc:ro -v <hostbuild>:/build gcc:13 sleep inf`, `apt-get install -y cmake ninja-build`, then configure/build under `/build`. Mount build dirs under `/Users` (Docker Desktop does not share `/private/tmp`, so scratchpad mounts show empty). The emulated build of the amalgamated libmcc TU takes ~5-8 min; use `-j1` if the parallel dep-file write races. Emulation is unreliable for parallel exec/JIT tests (spurious SEGFAULT/timeout under `-j2` load) — run failing tests individually to distinguish real failures from QEMU artifacts, and never regenerate a fidelity baseline from an emulated run (crashed compiles silently drop their gaps → non-deterministic gap set); apply the deterministic delta from a clean CI/native run instead. Complements [[embedjit-x86-test-rosetta]] (Rosetta can't run RWX x86 JIT code). This is how CI #846's x86_64 run_atexit/errors_and_warnings JIT bug and the Release-only jit/selftest-bench closed-form-optimization bug were reproduced and fixed.
