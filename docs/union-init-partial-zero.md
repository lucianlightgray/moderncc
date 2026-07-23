---
name: union-init-partial-zero
description: Arch-asm Operand structs hold a union; brace-initializing an automatic one only zeroes the first (small) member — gcc leaves e.v garbage where clang zeroes it
metadata: 
  node_type: memory
  type: project
  originSessionId: 25b4f64b-d5b4-44e6-98ca-09b512d871bb
---

The per-arch assembler `Operand` structs (e.g. src/arch/riscv64/riscv64-asm.c) wrap a union whose first member is a `uint8_t reg`, with `ExprValue e` behind it. An automatic `Operand x = { OP_..., { 0 } }` only guarantees the first union member is zeroed; homebrew gcc-16 leaves `x.e.v`'s upper bytes as stack garbage while clang zeroes the whole union — host-compiler-dependent codegen bugs (caused the 2026-07-03 dash-s-bytes-riscv64 CI failure via `mv` emitting a garbage addi immediate).

**Why:** C only zero-initializes the named (first) union member; the rest is indeterminate for automatics.

**How to apply:** never rely on a brace-initialized automatic Operand's `e.v`; assign `e.v` explicitly before use, or use the file-static `zimm`/`zero` consts (static storage → fully zeroed). Audit new pseudo-instruction cases in [[macos-arm64-status]]-adjacent arch files for reads of `imm`/`op.e` without a preceding assignment.
