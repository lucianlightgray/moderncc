---
name: arm64-run-far-call-veneer
description: arm64 mcc -run veneers far external CALL26 so it reaches host libc (fixes the long-standing arm64/macOS exec far-libc failures)
metadata: 
  node_type: memory
  type: project
  originSessionId: 4a27ad4b-e020-4eb7-b0c9-a42b8f1abc66
---

Native arm64 `mcc -run` mmaps run-memory tens of GB from libc, so a direct `bl`/`b` (R_AARCH64_CALL26/JUMP26, ±128MB) to `printf`/`memcpy`/`___rt_exit`/… overflows → "R_AARCH64_(JUMP|CALL)26 relocation failed". `build_got_entries` deliberately skips GOT/PLT for MEMORY output (works on x86_64 where rel32 reaches ±2GB), so arm64 had no in-range path. Fixed by `arm64_veneer_memory_calls` in `src/arch/arm64/arm64-link.c` (declared in mcc.h, called from `mcc_relocate_ex` after `build_got_entries`, both arm64-gated): for MEMORY output it scans reloc sections and, for every CALL26/JUMP26 whose target sym is out of the run-memory (SHN_UNDEF resolved via dlsym, or SHN_ABS host address from `mcc_add_symbol`), emits one 16-byte veneer in a new `.mcc.veneer` exec section — `ldr x16,[pc,#8]` (0x58000050) `; br x16` (0xd61f0200) `; .quad target` — whose `.quad` carries an R_AARCH64_ABS64 reloc holding the resolved far address, and redirects the CALL26's `r_info` sym to the (near) veneer's local symbol. In-program/near calls keep their direct branch. This fixed exec/ (297/297, was the "7 don't-chase fails"), the replay variants, AND the arm64 JIT selftests (they hit the same far-call in recompiled code) on arm64-macOS, and clears `ast/arm64` (ELF) exec on CI. Supersedes the old "arm64 macOS exec fails are pre-existing env noise" guidance in [[native-host-validation-gotchas]].
