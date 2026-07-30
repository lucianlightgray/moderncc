---
name: data-rel-local-selfmiscompile
description: Latent mcc self-miscompile — gcc-built vs mcc-built mcc diverge on .data.rel.local (RELRO) section synthesis when linking an executable
metadata: 
  node_type: memory
  type: project
  originSessionId: fc21d662-74fb-4590-ae82-99a10cd46949
---

There is a latent mcc self-miscompilation in the ELF **executable linker** (RELRO / `.data.rel.local` section synthesis): a gcc-built mcc and an mcc-built mcc, given byte-identical inputs, produce different linked executables. The gcc-built mcc emits a separate `.data.rel.local` section (8 bytes = one pointer); the mcc-built mcc folds those bytes into `.data` (`.data` grows by exactly 0x8). Every other byte diff cascades from that one section shifting all addresses.

**Why:** codegen (`.o` objects) is deterministic — `tools/selfhost-fixpoint.py` compares objects and passes. The divergence is only in the linked *executable*, which that .py never compares; `tools/fixpointgate.c` (ctest `fixpoint-invariant`) compares executables and catches it. mcc source has no `.data.rel.local` string — the section is synthesized by the linker from data carrying relocations to local symbols, and the synthesis logic behaves differently when the linker itself is compiled by mcc vs gcc.

**How it surfaced:** only when linking the larger cmake-cross `<arch>-libmccrt.a` runtime (not the native `libmccrt.a`). Discovered 2026-07-27 while fixing the `linux-gcc-cross` `fixpoint-invariant` CI break caused by [[mcc-add-support-archive-selection]]. The CI break was worked around at the archive-selection layer; this underlying miscompile was NOT fixed and remains latent. Root-causing it (which C construct in the linker mcc miscompiles) is the correct-but-deep fix. Relates to the [[mcc-codegen-gap-is-regalloc]] theme that mcc's own codegen is the frontier.
