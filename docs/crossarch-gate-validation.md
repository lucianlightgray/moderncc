---
name: crossarch-gate-validation
description: Recipe + results for validating optimizer gates across arches via cross-mcc + qemu (docker); MCC_AST_OPASSIGN/PROMO_ARROW green on x86_64/arm64/riscv64/i386
metadata:
  node_type: reference
  type: project
---

**Cross-arch differential recipe (docker, one debian:bookworm-slim container):**
`apt-get install gcc gcc-multilib gcc-aarch64-linux-gnu gcc-riscv64-linux-gnu qemu-user`.
Build a per-target mcc as a host tool: `gcc -w -DMCC_CONFIG_OPTIMIZER=1 -DMCC_TARGET_<T> -Isrc -Iinclude -Isrc/formats -Isrc/objfmt -Isrc/arch/<a> -O0 -o mcc-<T> src/mcc.c` (T ∈ X86_64/ARM64/RISCV64/I386). Compile a freestanding test with it (`-B/src -I/src/runtime/include`, use `extern` decls not system headers), link with the matching cross gcc (`aarch64/riscv64-linux-gnu-gcc`, or `gcc -m32` for i386, native `gcc` for x86_64), run under `qemu-aarch64/-riscv64 -L /usr/<triple>` (i386/x86_64 run native). Diff mcc output vs the same-arch gcc reference. Byte-identity check: gate-off object vs no-env object. Fire check: gate-on vs gate-off object differ.

**Results 2026-07-25 — `MCC_AST_OPASSIGN` + `MCC_AST_PROMO_ARROW` (both arch-neutral) validated correct on 4 arches:** x86_64 (native), arm64 (qemu), riscv64 (qemu), i386 (native -m32). Tests: nbody (FP), a 20 000-op randomized compound-assign/all-types/all-ops fuzz, and `batt` (struct accumulate + linked-list chase + swap + arrays). ALL bit-match gcc at -O0/-O2/-O4; gates-OFF byte-identical on all four; gates FIRE (on≠off) on all four for `batt`. See [[opassign-recorder-fix]], [[promo-arrow-and-hotloop-gaps]].

**arm32 (armv7) blocked — environmental, NOT a gate defect:** mcc arm object links as `EABI version 0` vs `arm-linux-gnueabihf`'s `EABI version 5` (float-ABI/`.ARM.attributes` mismatch) → plain gcc link fails pre-run. Needs the soft-float/static-link harness (cf. `abidiff-docker`).

**Still open before flipping these gates default-on:** the tens-of-thousands-of-seeds fuzz campaign; AOT==JIT on arm64 (needs the JIT engine — qemu can't); a native-arm64 3-stage self-host; arm32 via the ABI-matched harness. NB `MCC_AST_MATH_INLINE_PREPASS` is x86_64-only (its rewrites are `#ifdef MCC_TARGET_X86_64`), so it's inert on the other arches by construction. Push constraint: [[push-to-main-needs-authorization]].
