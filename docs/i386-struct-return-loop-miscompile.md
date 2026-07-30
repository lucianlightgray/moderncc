---
name: i386-struct-return-loop-miscompile
description: "the i386-codegen-diff d2 -O2 miscompile was DIVMAGIC (ltemp spill-slot reuse clobbers the live dividend); FULLY FIXED 2026-07-30 by making get_temp_local_var ltemp-aware, opt preserved"
metadata:
  node_type: memory
  type: project
  originSessionId: d8e85f80-7e78-45fe-a389-93af6d22f450
---

FULLY FIXED 2026-07-30, optimization preserved. The `i386-codegen-diff-docker` `d2` divergence
(`-O2 mcc=131 gcc=91`) was **NOT** the struct-by-value return in a loop (red herring — the loop
is byte-perfect, `s`=17547250 identical at -O0/-O2). Real cause: `d2` ends
`return (s&0x7fffffff)%251`, strength-reduced by the signed divmagic fold. That fold materializes
the dividend `x` into a reserved `ast_ltemp` slot so its several re-reads are stable — but the
value-stack spill allocator (`get_temp_local_var`, shared `src/mccgen.c`) only avoided live
`arr_temp_local_vars` slots, NOT ltemp slots. The replay temp frontier (`ast_alloc_temp_loc`)
seeds its floor from `loc`/`locrec_min`, not `ast_ltemp_cur`, so under i386 register pressure it
descended into the reserved region and gave the 64-bit mul's low-product spill the SAME slot as
`xoff`, clobbering the still-live dividend. `f(17547250)`=91 in isolation (low pressure → distinct
slot); only in-context `main` collided → 131.

Fix (commit `1a980d5b`): added `ast_ltemp_overlaps()` and made `get_temp_local_var` keep
allocating lower (monotonic) until the spill clears every reserved ltemp slot — mirrors the
existing arr_temp_local_vars overlap guard. This is the true source fix and closes the whole bug
class (any ltemp user under spill pressure), so i386 divmagic is **default-ON again** at -O2+.
(An interim carve-out `7c039681`, divmagic default-OFF on i386, was superseded the same day.)
Conservative (only pushes spills lower), byte-inert where the temp frontier never reaches the
ltemp region — proven by `fixpoint-invariant` (native arm64 byte-identity) + 107 optfire ctests.
i386 validated: d1..d14 × -O0..-O3 vs `gcc -m32`, struct-pressure div/mod soak, ~26M randomized
32/64-bit signed+unsigned div/mod-vs-idiv checks (0 fails). Relates to [[mcc-codegen-gap-is-regalloc]];
NOT [[landor-invert-fold-bug-class]].

Repro gotcha: the cmake i386 preset on an arm64 host builds an ARM64 `libmccrt.a` (EM:183) —
link-fails any runtime-helper case (float/int128). Build a real one:
`for c in runtime/lib/*.c; do i686-linux-gnu-gcc -m32 -O2 -ffreestanding -Iruntime/lib -Iinclude -c $c; done; ar rcs i386-librt.a *.o`.
Build mcc-i386 from the amalgamation (`-DMCC_TARGET_I386`), compile `-I<b>/include -B<b>/include`,
link `i686-linux-gnu-gcc -m32 -static <obj> i386-librt.a -lm`, run under `qemu-i386-static`.
