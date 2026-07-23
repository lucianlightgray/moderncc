---
name: cross-arch-exec-matrix
description: Which cross-arch binaries can actually be executed for differential/validation on this arm64 macOS host
metadata: 
  node_type: memory
  type: reference
  originSessionId: 50380c9a-e3de-41d4-8d8f-7f8c2a25aa75
---

Execution environments available on this Apple-Silicon macOS host, for
differential/validation of `cmake-cross/mcc-*` cross output:

- **i386-linux**: YES via `docker run --platform linux/386` (i686). Used by
  tools/i386{fastcall,tls,diff}-docker.sh. See [[docker-i386-exec]].
- **x86_64-linux**: YES via `docker run --platform linux/amd64` (see
  [[docker-amd64-repro]]) — but no `mcc-x86_64` cross target exists in
  cmake-cross, so you'd have to build one to differential x86_64 codegen.
- **arm64-macOS**: native (this host) — mcc vs clang differential runs directly,
  fast. mcc's arm64 codegen is clean across arith/struct/float/VLA/union/aliasing
  at -O0..-O3 (verified).
- **armv7 (arm/eabihf)**: YES, but ONLY inside a **native-arm64** Debian
  container (`docker run` with NO `--platform`, or `--platform linux/arm64`):
  `apt install qemu-user-static gcc-arm-linux-gnueabihf libc6-dev-armhf-cross`,
  then `qemu-arm-static ./armv7-binary` runs 32-bit ARM user-mode on the arm64
  host. Do NOT force `--platform linux/amd64`: the nested amd64-on-arm emulation
  makes qemu-arm fail its 32-bit guest VA reservation ("Unable to reserve
  0xfffff000 bytes ... at 0x8000", not fixable with -R/QEMU_RESERVED_VA). Static
  link (`arm-linux-gnueabihf-gcc -marm -static`) avoids the dynamic-loader path.
  Proven 2026-07-22 building `mcc-arm` in-container and running its output under
  qemu-arm (the R_ARM_CALL interworking fix + `arm-interwork-docker` guard). This
  CORRECTS the earlier "armv7 NO here" claim — the mistake was assuming
  `--platform linux/arm/v7` (32-bit ARM container, unsupported) instead of a
  64-bit arm64 container hosting qemu-**user** for the 32-bit ARM *binary*.
- **riscv64**: runs via qemu-user inside a container too (see the
  `riscv64-*-docker` ctests: `qemu-riscv64` on the cross output); the "no ready
  path" note referred to a native/direct qemu-user, not the docker-wrapped path.

Net: i386 (Docker linux/386), arm64 (native), AND armv7/riscv64 (qemu-user in a
native-arm64 container) all give an exec loop here. The committed guards
(`arm-inline-docker`, `arm-interwork-docker`, `riscv64-*-docker`, i386 scripts)
encode the working invocations — model new ones on them.
