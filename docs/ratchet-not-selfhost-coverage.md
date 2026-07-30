---
name: ratchet-not-selfhost-coverage
description: "the recorder-fidelity ratchet does NOT exercise self-host, so an AST gate can green the ratchet while miscompiling mcc itself"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: d8e85f80-7e78-45fe-a389-93af6d22f450
---

The AST recorder-fidelity ratchet (ast-verify / verify-baseline) and the byte-mirror
`fixpoint-invariant` test do NOT run a 3-stage self-host. So flipping an AST gate
default-on at -O2+ can bank the ratchet green while the gate miscompiles mcc's own TU:
the mcc-built mcc (stage2 `mcc1`) then aborts compiling `src/mcc.c` (arm64 SIGABRT /
x86_64 SIGSEGV), reddening every Linux `selfhost-fixpoint{,-O3,-Os,-gates}` cell while
`-O1` and the ratchet stay green.

Concrete case (2026-07-29): MCC_AST_WHILE_COMMA's loop-condition *prefix-store*
admission (commits c614afc4 + d30c5b99) — NOT the base comma recorder — was the
culprit; fixed upstream by re-gating it behind default-off `MCC_AST_LOOPCOND_STORE`
(fa6c2db5).

**Why:** the ratchet measures replay/record fidelity on a corpus, not that mcc can
still build itself. Different coverage.

**How to apply:** before flipping ANY AST gate default-on at -O2+, run
`tools/selfhost-fixpoint.py <build> --opt=-O2` (and -O3, -Os) FIRST. To bisect a
self-host break, disable suspects via `MCC_AST_<NAME>=0` env (forwarded to both
stages). Reproduce on Linux (the break is Linux-only; macOS self-host is blocked by
the JIT anyway) via a native-arm64 container + the `linux-gcc` preset — force the
target with `-DMCC_TARGET_ARCH=arm64` because CMake may misdetect the processor as
x86_64 under colima. Relates to [[mcc-codegen-gap-is-regalloc]].
