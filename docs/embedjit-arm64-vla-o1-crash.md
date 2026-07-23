---
name: embedjit-arm64-vla-o1-crash
description: "RESOLVED at HEAD (verified 2026-07-15): MCC_EMBED_JIT arm64/macOS vla/basic.c -O1 segfault no longer reproduces; 14 exec-*/basic failures are gone"
metadata: 
  node_type: memory
  type: project
  originSessionId: 58fb3a94-f5bc-407e-978d-b2c7006865b6
---

**RESOLVED — no longer reproduces at HEAD (verified 2026-07-15).** Previously an `MCC_EMBED_JIT=1`
build of `mcc` on arm64/macOS deterministically SIGSEGV'd (rc=139) compiling `tests/exec/vla/basic.c`
at `-O1` (plain compile, no `--embed-jit` flag), which failed the whole 295-case `*/basic` batch and
showed up as **14 `exec-*/basic` ctest failures** (replay, replay-tmpl, replay-promote, narrowfix,
vlat, zerobss, interchange, fusion, tile, mergestrings, search, search-emitsize, search-emitiso,
search-threads).

Verification that it's gone: clean-rebuilt (`cmake --build cmake-build-embedjit --target mcc
--clean-first`, `MCC_EMBED_JIT=ON`, clang) at HEAD → `mcc -O1 -c tests/exec/vla/basic.c` succeeds
5/5, compile+run correct at `-O0..-O3`, and the whole `ctest -R basic$` batch is 34/34 (all 14
`exec-*/basic` pass). Not bisected to one commit; the crash disappeared across the JIT embed
unification / SOC-split window (e0fca0a5 + the `mccjit_embed.c` split), the probable fix.

Implication update: the 14 `*/basic` failures are **no longer expected noise** on the arm64 embed
host — if they reappear, that's a real regression. Note the end-to-end embedded runtime is still
x86_64-only (`mccrun.c` `#ifdef MCC_TARGET_IS_HOST`; `#if MCC_TARGET_X86_64` dispatch mode 6), so
embed-program soaks still belong on an x86_64 CI cell. See [[macos-arm64-status]], [[build-dir-prefix]].
