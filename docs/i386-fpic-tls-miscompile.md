---
name: i386-fpic-tls-miscompile
description: mcc-i386 -fPIC TLS codegen segfaults (no GD/LDM; emits GOT32X for global __thread)
metadata: 
  node_type: memory
  type: project
  originSessionId: 50380c9a-e3de-41d4-8d8f-7f8c2a25aa75
---

Found via linux/386 Docker (the 32-bit sysroot — see [[docker-i386-exec]]).
mcc-i386 has NO i386 GD/LDM TLS codegen:

- Non-PIC (default): emits `R_386_TLS_LE` for both global and static `__thread`;
  links + runs correctly. GOOD.
- `-fPIC`: emits `R_386_GOT32X`/`R_386_PC32` for a global `__thread` (wrongly
  treating the TLS symbol like a regular GOT global) and `R_386_TLS_LE` for a
  static `__thread`. The linked PIE/exe **segfaults** at runtime.
- Non-TLS `-fPIC` is fine, so it's TLS-specific, not general PIC breakage.

gcc-i386 `-fPIC` (for contrast): global→GD (`R_386_TLS_GD` + `___tls_get_addr`),
static→LDM (`R_386_TLS_LDM` + `R_386_TLS_LDO_32`), IE→`R_386_TLS_GOTIE`,
LE→`R_386_TLS_LE`.

Repro: write a `__thread` TU (no libc needed), `cmake-cross/mcc-i386 -fPIC -c
tls.c -o pic.o`, then in `docker run --platform linux/386 i386/debian`:
`gcc -m32 -pie -fPIE pic.o -o e && ./e` → SIGSEGV (139). Same TU non-PIC
(`mcc-i386 -c` + `gcc -m32 def.o`) → exit 0.

Fix (a real codegen task, deferred): emit proper i386 GD/LDM under PIC, or fall
back to IE, or hard-error — anything but silently emitting crashing code. Note
mcc's *linker* already relaxes GD/LDM relocs from external objects (that's the
existing x86_64-only `tests/tls/run_models.cmake`); the gap is mcc's own i386
PIC TLS *codegen*. TODO item: docs/TODO.md "Fix i386 -fPIC TLS codegen".
Related: [[arm64-linux-runtime-libcalls]].
