# Session: lin-x64  (Linux / x64 / band 10000–29999)

Only lin-x64 writes this file. Authoritative session state (replaces the shared
`TODO.md` Sessions-table row; see `INSTRUCTIONS.md` §4.1). Liveness of record is
the commit log (`git log -1 --format=%cI` by this author); the `Last seen` below
is advisory.

Next ID: 10542
Last seen: 2026-08-22T17:07Z
Capabilities: x86_64-linux native · arm64/riscv64/i386 via qemu-user · x86_64-PE via wine (canary) · gpu-vulkan OK (2026-08-22 RE-CHECK: vulkaninfo does NOT hang; enumerates AMD Radeon 610M [RADV] + NVIDIA RTX 5070 Ti; tests/gpu/run.sh full-scale N=2^32 = 9/9 checks green, cpu==gpu differential PASS, half-split 4917 M/s. NOTE the vk host device-score picks the AMD iGPU, not the NVIDIA dGPU — prior "device-blocked: vulkaninfo hangs" was stale)

STATUS (2026-08-22T17:07Z — GOAL-LOOP): **NO active lin claims, tree CLEAN, all pushed.** Just closed **T-lin-10492 (P1, LAST P1-cluster item) DONE 5a3549c16** — qemu optfire differ cells (arm64+riscv64) now run the AOT-vs-JIT oracle (OPTFIRE_AOT=1, route (a)); test-infra-only, no source change (mcc's Linux crt link needs only crt1/crti/crtn, resolved by the default {R}-crtprefix once --sysroot is passed). 383/383 optfire green, no AOT-vs-JIT divergence on either cross arch. Also delisted a pre-existing coverage-ledger 'funnel' red (win T-lin-10510 loose end). **P1 (100% JIT coverage) now fully exhausted → next is P2 optimizer parity** (biggest gcc/clang gaps: T-lin-10470 auto-vec continuation, 10471 GVN[mac], 10473 IPA[mac], 10474 escape-analysis, 10475 TFA, 10425 packed-vector, 10455/10457 superopt; or round-2 pool). Full narrative → docs/log/lin-x64.md.

**This session's P2 deliverables (all verified + pushed):**
1. Census unblock (3dc76230f) — rebanked wide RIR-coverage census on elf + delisted fleet-wide `rir-coverage-census` KNOWN_RED; diagnosed compound (not single-cause) drift, coordinated the accept with mac (option-a).
2. T-lin-10541 unlocked-stdio Linux fix (da13de145, DONE+ARCHIVED) — mccdefs.h Linux `#else` maps printf_unlocked/fprintf_unlocked/fputs_unlocked → locked `__builtin_` forms (glibc dropped the symbols); o0-neutral, cli 486/486, KNOWN_RED delisted. mac-handoff, co-owned w/ T-mac-30047.
3. **T-lin-10470 AUTO-VECTORIZER (P2's biggest gcc/clang parity gap) — 4 slices, all default-off, x86_64-SysV-gated, released OPEN:**
   - SLP straight-line (07a616f68/db445f7ea): N adjacent isomorphic float/double array-object ops → one 16B SSE op via the existing gen_vector_op path. id38 gap→partial.
   - loop-vec exact-trip (ed77fc96c): strip-mines countable const-multiple loops → packed body. id37 gap→partial.
   - loop-vec const remainder (8b1d54fd4): arbitrary const trip via full vector iters + unrolled scalar tail.
   - loop-vec RUNTIME/symbolic trip (f5e65f2a7): variable N via bound N&~(lanes-1) + a cloned scalar remainder loop continuing from the shared IV.
   Crux discovery: the AST_PF_EMIT re-emit gate needs a per-pass result flag or the fold is silently dropped (Load type read via ast_ident_etype, not ast_type_t). Gated #if x86_64&&!PE — off-x86_64 the scalarized 16B vec-store libcall SEGVs in a strided loop; loopvec cell pinned x86_64 in arch.txt. SLP stays universal (correct-scalarized on arm64/riscv64).
   VERIFIED: optfire matrix 383/383 all arches, cli 486/486, o0-baseline/census/cover3(128 flags)/coverage-check all green; o0- and On-neutral.

**Hiccups (lessons logged):** a pull-rebase autostash conflict shipped conflict markers in 07a616f68 (fixed db445f7ea) + a commit-msg backtick-execution bug — use plain commit messages; after a conflicted autostash, scan for markers before committing.

**NEXT P2** (10470 remaining, OPEN): non-unit/reverse stride, >16B AVX, arm64 NEON / riscv64 RVV packed (also fixes the off-x86_64 SEGV), non-array/restrict bases via an alias oracle (cf T-lin-10495), reductions/gather-scatter, wire onto a shipped -O for gcc/clang PARITY (owes a cross-fleet rebank). Or the round-2 pool 10493..10524 (avoid mac 10471/10473/30291, win 10510). Then P3 GPU (T-lin-10482, gpu-vulkan available here).
