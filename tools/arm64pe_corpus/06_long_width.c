/* EXPECTED-DIVERGENCE case: `long` is 8 bytes under LP64 (arm64-Linux/ELF) and
   4 bytes under LLP64 (arm64-Windows/PE). The harness will (correctly) flag the
   .text as SUSPICIOUS because it cannot know the divergence is an intended ABI
   difference: the ELF build uses x-registers / 64-bit ldr, the PE build uses
   w-registers / 32-bit ldr for the same source.

   This file exists to DOCUMENT and demonstrate that the harness surfaces
   type-width-driven codegen differences (rather than silently passing them).
   When validating width-neutral codegen, avoid bare `long`; this case is the
   deliberate exception and its SUSPICIOUS verdict is the expected, correct
   result — a human confirms it is the LP64/LLP64 `long` split, not a bug. */
long shl2(long x) { return x << 2; }
long widen(int x) { return (long)x + 1; }
unsigned long masks(unsigned long a, unsigned long b) { return (a & b) | (a ^ b); }
