---
name: mcc-add-support-archive-selection
description: mcc_add_support picks between plain libmccrt.a and arch-tagged <arch>-libmccrt.a — the rule and the tension it navigates
metadata: 
  node_type: memory
  type: project
  originSessionId: fc21d662-74fb-4590-ae82-99a10cd46949
---

`mcc_add_support` (`src/libmcc.c`) resolves the runtime support archive when two candidates sit side by side in a `-B` dir: the plain `[<crossprefix>]libmccrt.a` and the cmake-cross-staged `<arch>[-<os>]-libmccrt.a`.

**The rule (as of 2026-07-27):** prefer the plain name WHEN IT IS ARCH-CORRECT (decided by `mcc_support_arch_match` in `mccelf.c` — a pure read-only ELF `e_machine` probe, never a load attempt, because loading a wrong-arch member trips `mcc_error_noabort` and `nb_errors != 0` fails the whole link). Then add the arch-tagged archive as an a-la-carte SUPPLEMENT (pulls only still-undefined symbols, so a complete plain archive makes it inert). If plain is absent or wrong-arch, use the arch-tagged one, then plain with an error as last resort.

**Why the dance:** two opposing scenarios, both with empty `MCC_CONFIG_CROSSPREFIX` and same target arch:
- Native self-host fixpoint (`fixpoint-invariant`, cmake's own build dir): the plain native `libmccrt.a` is authoritative/complete; linking the arch-tagged cross build instead diverges from self-host byte-identity (exposes [[data-rel-local-selfmiscompile]]). Wants PLAIN.
- Stage-built cross mcc (`tools/selfhost-cross-native.sh`, empty crossprefix, `-B` at a multi-target staging dir): on an x86_64 host the plain `libmccrt.a` is the HOST arch (wrong for an arm64/riscv64 target) — the "invalid object file" + unresolved `__ashldi3/__clear_cache/__floatunsitf` trap. Wants ARCH-TAGGED.

fd108bd4 first tried "arch-tagged FIRST" and broke the native fixpoint. The current plain-first-when-arch-correct + supplement satisfies both. Note: on a host where host==target (e.g. arm64 mac), `selfhost-cross-native.sh` with STALE cmake-cross archives segfaults at stage2 — but that reproduces on pristine main too; it is stale-archive rot, not this logic. That test is SKIP (77) on CI (no `cmake-cross` dir / qemu / sysroot on runners).
