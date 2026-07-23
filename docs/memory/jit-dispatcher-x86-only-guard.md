---
name: jit-dispatcher-x86-only-guard
description: the §26 AST JIT entry-dispatcher block in mccast.c emits raw x86 opcodes and must stay behind an i386/x86_64
metadata: 
  node_type: memory
  type: project
  originSessionId: 34a19c6c-5d01-4b37-8ff8-98596a7bad42
---

The §26 "entry-dispatcher" block in `ast_func_end` (src/mccast.c, gated by
`if (ast_jit_dispatch_env && faithful && !ast_jit_splice_env)`) emits **raw x86
machine code** directly — `o(0xc031)`, `g(0x48/0x83/0xbd...)`, and `oad(...)` for
jcc rel32. `oad` is declared/defined ONLY for `MCC_TARGET_I386 || MCC_TARGET_X86_64`
(mcc.h ~1686, mccgen.c ~286). Any use outside that guard breaks every non-x86 host
build: clang → `error: call to undeclared function 'oad'`
(-Werror=implicit-function-declaration); gcc → compiles an implicit decl, fails at
link (`undefined reference to oad`). Hits arm64 / riscv64 / macos-arm64 / msvc-arm64.

**Why it recurs:** §26 W-stages land on `main` frequently and each tends to expand
this block (W2.2 added the guard/deopt; W2.3 added non-null speculative
specialization + `ast_nonnull_params`/`ast_nonnull_fold` helpers). Fixed once in
25d546b3 by wrapping the block AND the speculation-only helper cluster in
`#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64) ... #endif`.

**How to apply:** when reviewing/merging any new §26 dispatcher work, verify the new
emission (and any static helper used *only* by it — else `-Wunused-function` on
non-x86) stays inside that guard. Keep the `if (...)` runtime condition OUTSIDE the
`#if` so `ast_jit_dispatch_env` stays read (no `-Wunused-but-set-variable`). Verify
with a docker arm64 build (clang is strictest): see [[moderncc-windows-build]] host
can't build arm64 natively — use `docker run --platform linux/arm64 ubuntu:24.04`.
Related: [[ci-log-zip-commit-ahead]] (the run that caught this was a commit since
rebased away).
