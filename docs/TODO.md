# TODO

## Sessions

| SessionId | Platform | Arch  | Band        | Next ID | Last seen         |
| --------- | -------- | ----- | ----------- | ------- | ----------------- |
| mac-arm64 | macOS    | arm64 | 30000–49999 | 30003   | 2026-08-15T01:20Z |
| lin-x64   | Linux    | x64   | 10000–29999 | 10368   | 2026-08-15T02:40Z |
| win-x64   | Windows  | x64   | 50000–69999 | 50004   | 2026-08-15T02:33Z |

## Contracts — blocking, highest priority


## In progress — mac-arm64   ← only mac-arm64 writes this zone


## In progress — lin-x64     ← only lin-x64 writes this zone

- [ ] T-lin-10001 [C] A task representation with an explicit resume state, replacing the C11 threading implementation
      OWNER: lin-x64 | STATE: IN_PROGRESS | SHA: dc7a3ed9 | TS: 2026-08-15T01:55Z
      REF: DETAILS.md#t-lin-10001-slice-3-approach-l2-prime | DEPS: — | NOTE: slice 2 DONE; slice 3 (L2′) approach PUBLISHED — two of the contract's premises for it are stale (the pool joins; nothing is detached). Implementation held until win-x64's T-lin-10092/win suite number lands


- [ ] T-lin-10365 [S] An isolated, iterative Windows-on-ARM CI hook on a `woa/**` branch
      OWNER: lin-x64 | STATE: IN_PROGRESS | SHA: 3cf6e238 | TS: 2026-08-15T02:10Z
      REF: DETAILS.md#q-lin-10013-answer-ci-is-the-woa-executor | DEPS: — | NOTE: executes Q-lin-10013's answer; unblocks T-lin-10086/T-lin-10087


## In progress — win-x64     ← only win-x64 writes this zone

## Open — claimable

- [ ] T-lin-10088 [X] win-x64 — vendor the `gcc-c-torture-execute` corpus into a container for `pe/x-oracle`
      OWNER: — | STATE: OPEN | SHA: c5197398 | TS: 2026-08-15T02:40Z
      REF: DETAILS.md#fleet-capabilities-docker-qemu-on-all-three | DEPS: — | Q: Q-lin-10014 ANSWERED | NOTE: docker on the Windows box carries/fetches the corpus without vendoring it into the tree — the "deliberately not vendored" property is preserved. pe/x-oracle then runs all 1,693 programs with no new code
- [ ] T-lin-10045 [S] `-fopt-slice`: revise into the governor over every AST/RIR slice-capable strategy, integrated with the other slice optimizers
      OWNER: — | STATE: OPEN | SHA: 3749f816 | TS: 2026-08-15T02:30Z
      REF: DETAILS.md#q-lin-10006-answer-fopt-slice-is-the-governor-not-a-pass | DEPS: — | Q: Q-lin-10006 ANSWERED | NOTE: not "own or delete" — it was never a pass. Carries forward: the disk-cache determinism defect, and OPT_SLICE at MCC_OPTD_LEVEL(9) leaves opt-cache-determinism a permanent 77 with no subject. First slice = a shipped level with the determinism claim gated
- [ ] T-lin-10042 [X] mac-arm64 — the Metal parity staged plan, WITH fp64 (2,200-3,400 lines)
      OWNER: — | STATE: OPEN | SHA: 0d33d71e | TS: 2026-08-15T02:20Z
      REF: DETAILS.md#q-lin-10009-answer-metal-parity-scheduled-with-fp64 | DEPS: — | Q: Q-lin-10009 ANSWERED | NOTE: human scheduled the fp64 variant. No CI differential exists or can — land it in slices each checkable by the hand-run per-value differential, not as one unwatched arm
- [ ] T-lin-10086 [S] `arm64-win32` execution on a `windows-11-arm` CI runner (was [X] win-x64)
      OWNER: — | STATE: OPEN | SHA: 3cf6e238 | TS: 2026-08-15T02:10Z
      REF: DETAILS.md#q-lin-10013-answer-ci-is-the-woa-executor | DEPS: T-lin-10365[S] | NOTE: Q-lin-10013 ANSWERED — CI is the executor, so this is no longer win-x64-only. SPLIT: the `arm-win32` (ARM32) half has NO executor — Windows 11 on ARM64 does not run ARM32 apps — and must not be reported green with the arm64 half
- [ ] T-lin-10087 [S] W5: mcc cannot self-host on Windows arm64 — reproduce the `0xC0000005` trio in CI (was [X] win-x64)
      OWNER: — | STATE: OPEN | SHA: 3cf6e238 | TS: 2026-08-15T02:10Z
      REF: DETAILS.md#q-lin-10013-answer-ci-is-the-woa-executor | DEPS: T-lin-10365[S] | NOTE: stage1 takes 0xC0000005 on lib/atomic.c, lib/alloca.S, lib/alloca-bt.S, lib/builtin.c (varargs/alloca/stack-probe trio) — a windows-11-arm runner can now reproduce it
- [ ] T-lin-10089 [X] mac-arm64 — the `ast/o0-baseline` quartet is a visible skip with a real reason
      OWNER: — | STATE: OPEN | SHA: 3cf6e238 | TS: 2026-08-15T02:10Z
      REF: DETAILS.md#q-mac-30000-answer-minimal-darwin-headers-sdk-on-apple | DEPS: T-lin-10002[C], T-lin-10367[C] | Q: Q-mac-30000 ANSWERED | NOTE: minimal mcc-authored Darwin headers key the bank; a native Apple host still compiles against the real SDK. This [X] half is the --sysroot wiring + re-key; the header set itself is T-lin-10367[C]
- [ ] T-lin-10366 [S] `MCC_REF_CC` never consults the vendored aarch64 llvm-mingw, so an arm64 Windows host has no reference cc
      OWNER: — | STATE: OPEN | SHA: 3cf6e238 | TS: 2026-08-15T02:10Z
      REF: DETAILS.md#t-lin-10366-ref-cc-is-x86-64-only-on-windows | DEPS: —
- [ ] T-lin-10367 [C] A minimal mcc-authored Darwin libc header set for host-independent bank keys
      OWNER: — | STATE: OPEN | SHA: 3cf6e238 | TS: 2026-08-15T02:10Z
      REF: DETAILS.md#q-mac-30000-answer-minimal-darwin-headers-sdk-on-apple | DEPS: — | NOTE: declarations only, no inline bodies; runtime/osx; all three key_flags branches read it, hence [C]. Authorable from any host — no Apple hardware, no SDK, no licensing gate
- [ ] T-win-50003 [S] win-x64 full native suite — 35 real failures triaged (28 GPU-slice/`slicerun` device↔CPU differentials + "0 slices on Windows"; 4 fp under emitsize/emitiso opt-search; 3 jit/runtime)
      OWNER: — | STATE: OPEN | SHA: 260bb900 | TS: 2026-08-15T01:55Z
      REF: DETAILS.md#t-win-50003-win-x64-full-native-suite-35-real-failures-triaged | DEPS: — | NOTE: surfaced by the first Windows full-suite run (T-win-50002 unblocked mcc_build); this box has an RTX 2060 so GPU cells genuinely dispatch. Bucket A (28) is mccgpu/slicerun owners' (lin/mac) — win-x64 has the NVIDIA box to confirm fixes; Bucket B jit/runtime deferred behind lin's T-lin-10001 L2′ (mccjit_embed.c)
- [ ] T-lin-10364 [S] The wide census carried a pre-existing drift component that a0e26cff has now banked
      OWNER: — | STATE: OPEN | SHA: a0e26cff | TS: 2026-08-14T23:40Z
      REF: DETAILS.md#t-lin-10364-the-pre-existing-half-of-the-census-drift | DEPS: —
- [ ] T-lin-10004 [S] Implement `_BitInt(N)` (C23 6.2.5); the keyword is diagnosed, the type is absent
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10004-implement-bitintn-c23-625-the-keyword | DEPS: —
- [ ] T-lin-10005 [S] `__bf16`: finish encode/decode and ABI now that `VT_BTYPE` is 5 bits
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10005-bf16-finish-encodedecode-and-abi-now | DEPS: —
- [ ] T-lin-10006 [S] Parse the `__m512` / `__m256h` / `__m128h` types (52 cells)
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10006-parse-the-m512-m256h-m128h-types | DEPS: —
- [ ] T-lin-10007 [S] Parse `__float128` / `_Float128` (28 cells)
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10007-parse-float128-float128-28-cells | DEPS: —
- [ ] T-lin-10008 [S] Parse `_Complex _Float16` (9 cells)
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10008-parse-complex-float16-9-cells | DEPS: —
- [ ] T-lin-10010 [S] Implement reversed `scalar_storage_order`; refusing it is the safe interim, not the feature
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10010-implement-reversed-scalar-storage-order | DEPS: —
- [ ] T-lin-10011 [S] Register-array decay: `*a` and `*(a+1)` are still accepted
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10011-register-array-decay-a-and-a1 | DEPS: — | Q: Q-lin-10004
- [ ] T-lin-10012 [S] 32-byte vectors are laid at 16-byte alignment, so cross-TU to gcc is incompatible
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10012-32-byte-vectors-are-laid-at | DEPS: — | Q: Q-lin-10005
- [ ] T-lin-10013 [S] `__int256` has no literal suffix
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10013-int256-has-no-literal-suffix | DEPS: —
- [ ] T-lin-10014 [S] DWARF describes an `__int256` as its underlying four-limb struct
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10014-dwarf-describes-an-int256-as-its | DEPS: —
- [ ] T-lin-10015 [S] `__int256` arithmetic is a call per operation
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10015-int256-arithmetic-is-a-call-per | DEPS: —
- [ ] T-lin-10016 [S] `__int256` float conversions need an oracle before they need code
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10016-int256-float-conversions-need-an-oracle | DEPS: —
- [ ] T-lin-10017 [S] A same-TU `call` in inline asm is resolved to a displacement instead of a relocation
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10017-a-same-tu-call-in-inline | DEPS: —
- [ ] T-lin-10018 [S] `ptr_unlink` for-condition-store segfault
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10018-ptr-unlink-for-condition-store-segfault | DEPS: —
- [ ] T-lin-10019 [S] `run-tier/x86_64` fails `tls_threads` when `MCC_JIT=1` meets an active AST replay
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10019-run-tierx86-64-fails-tls-threads | DEPS: —
- [ ] T-lin-10020 [S] i386 `R_386_TLS_GOTIE` gap, and the declined upstream `7f7845cd` (VT_VOID)
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10020-i386-r-386-tls-gotie-gap | DEPS: —
- [ ] T-lin-10021 [S] `ast_locrec_skip` consumes by count, and it should consume by fit
      OWNER: — | STATE: OPEN | SHA: 8a92ee01 | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10021-ast-locrec-skip-consumes-by-count | DEPS: — | NOTE: logic already by-fit (8a92ee01); owed probe's firing subject not in tests/exec — see DETAILS#t-lin-10021-investigation-logic-stale-fixed-probe-trigger-unfound
- [ ] T-lin-10023 [S] `-O3` re-emission leaves the pre-inline body in `.text`, and only deferral can reclaim it
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10023-o3-re-emission-leaves-the-pre | DEPS: —
- [ ] T-lin-10024 [S] `ast.orphan_bytes` undercounts by ~2.4%; fix it before banking the byte figure
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10024-astorphan-bytes-undercounts-by-24-fix | DEPS: T-lin-10023[S]
- [ ] T-lin-10025 [S] `rf-1`: the per-function size trial scores on body length, which does not predict object size
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10025-rf-1-the-per-function-size | DEPS: T-lin-10023[S]
- [ ] T-lin-10026 [S] P6 — split `src/mccast.c` (~17k lines) and rename `ast_*` to `ir_*`
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10026-p6-split-srcmccastc-17k-lines-and | DEPS: —
- [ ] T-lin-10027 [S] RIR deletion residue: keep or delete each of six named symbols and tools
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10027-rir-deletion-residue-keep-or-delete | DEPS: —
- [ ] T-lin-10029 [S] The lazy JIT route fails to build a variant on programs the sync route handles
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10029-the-lazy-jit-route-fails-to | DEPS: —
- [ ] T-lin-10030 [P] The embed JIT is measured only on x86_64
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10030-the-embed-jit-is-measured-only | DEPS: —
  - [ ] T-lin-10030/mac [P] The embed JIT is measured only on x86_64 — mac-arm64
        OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
        REF: DETAILS.md#t-lin-10030-the-embed-jit-is-measured-only | DEPS: — | NOTE: native arm64 evidence banked (jit family 66/66 green, selfcheck boots the engine); conformance half corpus-gated — see DETAILS#t-lin-10030-mac-native-arm64-embed-jit-evidence
  - [ ] T-lin-10030/lin [P] The embed JIT is measured only on x86_64 — lin-x64
        OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
        REF: DETAILS.md#t-lin-10030-the-embed-jit-is-measured-only | DEPS: —
  - [ ] T-lin-10030/win [P] The embed JIT is measured only on x86_64 — win-x64
        OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
        REF: DETAILS.md#t-lin-10030-the-embed-jit-is-measured-only | DEPS: —
- [ ] T-lin-10031 [S] The JIT teardown is unbounded above
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10031-the-jit-teardown-is-unbounded-above | DEPS: T-lin-10001[C]
- [ ] T-lin-10032 [S] `MCCJIT_POOL_MAX` is 64 and `mccjit_pool_start` clamps to it silently
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10032-mccjit-pool-max-is-64-and | DEPS: —
- [ ] T-lin-10033 [S] The Vulkan dispatch destroys resources under a still-pending command buffer
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10033-the-vulkan-dispatch-destroys-resources-under | DEPS: —
- [ ] T-lin-10034 [S] `mcc_gpu_mem_index` picks the worst memory type on this machine
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10034-mcc-gpu-mem-index-picks-the | DEPS: —
- [ ] T-lin-10035 [S] `devs[0]` is chosen with no scoring while `VkPhysicalDeviceLimits` is transcribed and unread
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10035-devs0-is-chosen-with-no-scoring | DEPS: —
- [ ] T-lin-10036 [S] `ast_ladder_gpu_run` uploads `tin` twice per rung and two memsets are dead
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10036-ast-ladder-gpu-run-uploads-tin | DEPS: —
- [ ] T-lin-10037 [S] The emitter's constant cache binds before module size
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10037-the-emitters-constant-cache-binds-before | DEPS: —
- [ ] T-lin-10038 [S] No tree-recursion exec golden exists, and the failure mode is a GPU hang
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10038-no-tree-recursion-exec-golden-exists | DEPS: —
- [ ] T-lin-10039 [S] `spvgate` reports OK for a case that lowered nothing
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10039-spvgate-reports-ok-for-a-case | DEPS: T-lin-10003[C]
- [ ] T-lin-10041 [X] mac-arm64 — the native MSL region arm
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10041-mac-arm64-the-native-msl-region | DEPS: —
- [ ] T-lin-10043 [S] `tests/emitmap/bank.json`'s tolerances cannot fail, and the selfhost cell is already drifted inside them
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10043-testsemitmapbankjsons-tolerances-cannot-fail | DEPS: T-lin-10003[C]
- [ ] T-lin-10044 [S] `rir-coverage.py`'s `wide` corpus silently drops 9 sources and computes every percentage over the rest
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10044-rir-coveragepys-wide-corpus-silently-drops | DEPS: —
- [ ] T-lin-10046 [S] `ast_env_gate` no longer exists in `src/` and four shell tools still grep for it
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10046-ast-env-gate-no-longer-exists | DEPS: —
- [ ] T-lin-10047 [S] Five number-producing tools are still registered nowhere
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10047-five-number-producing-tools-are-still | DEPS: T-lin-10003[C]
- [ ] T-lin-10048 [S] `BREAKEVEN` is a hand-pinned literal that every lane fraction is scored against
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10048-breakeven-is-a-hand-pinned-literal | DEPS: —
- [ ] T-lin-10049 [S] `flagsweep-cover` and `asm-gas-directives` are `mcc_skip_test` stubs, structurally incapable of failing
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10049-flagsweep-cover-and-asm-gas-directives | DEPS: T-lin-10003[C]
- [ ] T-lin-10050 [S] `--mutate` has no `memcpy`/`memset` in the slice corpus to bite on
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10050-mutate-has-no-memcpymemset-in-the | DEPS: — | NOTE: three investigation slices banked in DETAILS; harness ready, needs uncovered-mutate-site identification
- [ ] T-lin-10051 [S] Debt 6-vi — the chain-store *member* fixture was never written
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10051-debt-6-vi-the-chain-store | DEPS: —
- [ ] T-lin-10052 [S] `storeval-callstore` was never ranked in either direction, and 32 of 34 demoted rows are unpriced
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10052-storeval-callstore-was-never-ranked-in | DEPS: —
- [ ] T-lin-10053 [S] A replay fallback leaves no trace in a default build
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10053-a-replay-fallback-leaves-no-trace | DEPS: —
- [ ] T-lin-10054 [S] `rir-nofb-probe`, `--check-gap-dir` and `--check-low-dir` all pass over an empty input
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10054-rir-nofb-probe-check-gap-dir | DEPS: T-lin-10003[C]
- [ ] T-lin-10056 [S] Two bodies replay byte-identically under one build and not another
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10056-two-bodies-replay-byte-identically-under | DEPS: —
- [ ] T-lin-10059 [S] `matrix.yml` silently drops three GPU gate cells on every Linux cell
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10059-matrixyml-silently-drops-three-gpu-gate | DEPS: —
- [ ] T-lin-10060 [S] `ci.yml` does not pin the Mesa version
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10060-ciyml-does-not-pin-the-mesa | DEPS: —
- [ ] T-lin-10061 [S] No runtime fp64 denormal reading has ever been taken from lavapipe
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10061-no-runtime-fp64-denormal-reading-has | DEPS: —
- [ ] T-lin-10062 [S] `MCC_RIR_STAMP` is off by default, so 39,640 of 39,643 `Binary` nodes read back untyped
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10062-mcc-rir-stamp-is-off-by | DEPS: —
- [ ] T-lin-10063 [S] The `SKIP_RETURN_CODE` count is stated three incompatible ways
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10063-the-skip-return-code-count-is | DEPS: —
- [ ] T-lin-10065 [S] `smokerun --max-level 13` is a silent no-op and the `-O13` ratchet wants the opposite polarity
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10065-smokerun-max-level-13-is-a | DEPS: —
- [ ] T-lin-10066 [S] The covering array is a 3-wise guarantee over 108 of 114 flags
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10066-the-covering-array-is-a-3 | DEPS: —
- [ ] T-lin-10067 [S] The flag-sweep corpus has no `x - 1` shape in a returned expression
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10067-the-flag-sweep-corpus-has-no | DEPS: —
- [ ] T-lin-10068 [S] A stage-2 build dir does not rebuild when a header changes
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10068-a-stage-2-build-dir-does | DEPS: —
- [ ] T-lin-10069 [S] Fifteen "written as live, actually superseded" citations need re-checking or striking
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10069-fifteen-written-as-live-actually-superseded | DEPS: —
- [ ] T-lin-10071 [S] `rir-nofb-probe-self` is flaky and the mechanism is not known
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10071-rir-nofb-probe-self-is-flaky | DEPS: —
- [ ] T-lin-10072 [X] lin-x64 — `optfire/abs` and `optfire/level-abs` fail only in a full parallel run
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10072-lin-x64-optfireabs-and-optfirelevel-abs | DEPS: —
- [ ] T-lin-10073 [X] lin-x64 — the two wine `run-tier` cells are load-sensitive
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10073-lin-x64-the-two-wine-run | DEPS: —
- [ ] T-lin-10359 [X] lin-x64 — `slice/cref-oracle-*` stalls on five programs when the host GPU is busy
      OWNER: — | STATE: OPEN | SHA: d298af58 | TS: 2026-08-14T18:30Z
      REF: DETAILS.md#t-lin-10359-slicecref-oracle-stalls-on-five-programs | DEPS: —
- [ ] T-lin-10074 [S] `slice/quiesce` is structurally flaky and the device lock is built, priced and off
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10074-slicequiesce-is-structurally-flaky-and-the | DEPS: —
- [ ] T-lin-10076 [S] N7 residue — an independent tree-side oracle for the slice evaluator
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10076-n7-residue-an-independent-tree-side | DEPS: —
- [ ] T-lin-10077 [S] N37 — the refs-disagree class is computed per digest, so a category hides its own points
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10077-n37-the-refs-disagree-class-is | DEPS: — | Q: Q-lin-10012
- [ ] T-lin-10078 [S] N36 residue — `/` and `%` on over-wide bit-fields are still per-operation truncated
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10078-n36-residue-and-on-over-wide | DEPS: —
- [ ] T-lin-10079 [S] `ir_cap`'s trace sites fire ~375k times at `-O0` where the layer is inactive
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10079-ir-caps-trace-sites-fire-375k | DEPS: —
- [ ] T-lin-10080 [S] The 31-byte `full_language.c -O0` residual is unattributed
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10080-the-31-byte-full-languagec-o0 | DEPS: —
- [ ] T-lin-10081 [S] `MCC_GPU_LOCK`: replicate the resident state per context, then narrow the lock
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10081-mcc-gpu-lock-replicate-the-resident | DEPS: —
- [ ] T-lin-10082 [S] The `--jit-always-gpu` boundary is struct member access, not link-time symbols
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10082-the-jit-always-gpu-boundary-is | DEPS: —
- [ ] T-lin-10083 [X] win-x64 — flip default `-c` to COFF and re-bank `o0-baseline`
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10083-win-x64-flip-default-c-to | DEPS: T-lin-10002[C]
- [ ] T-lin-10084 [X] win-x64 — non-constant `__except` filters and `__finally` need funclet codegen
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10084-win-x64-non-constant-except-filters | DEPS: —
- [ ] T-lin-10092 [P] Record a clean full native suite number on each platform
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10092-record-a-clean-full-native-suite | DEPS: —
  - [ ] T-lin-10092/mac [P] Record a clean full native suite number on each platform — mac-arm64
        OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
        REF: DETAILS.md#t-lin-10092-record-a-clean-full-native-suite | DEPS: —
  - [ ] T-lin-10092/lin [P] Record a clean full native suite number on each platform — lin-x64
        OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
        REF: DETAILS.md#t-lin-10092-record-a-clean-full-native-suite | DEPS: —
  - [ ] T-lin-10092/win [P] Record a clean full native suite number on each platform — win-x64
        OWNER: win-x64 | STATE: IN_PROGRESS | SHA: 260bb900 | TS: 2026-08-15T01:55Z
        REF: DETAILS.md#t-lin-10092-record-a-clean-full-native-suite | DEPS: — | NOTE: NUMBER RECORDED (first ever on Windows): 9387 cells, 8388 pass / 945 skip / 54 fail at 9b21c352; 19 false-reds fixed at 260bb900 -> 8388 / 964 / 35. Not clean — the 35 residual reds are triaged in T-win-50003 (28 GPU-slice, 4 fp opt-search, 3 jit/runtime). @lin: number is landed, slice-3 (L2′) hold can release
- [ ] T-lin-10093 [P] `ci/must-run-registered` green on each platform
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10093-cimust-run-registered-green-on-each | DEPS: T-lin-10003[C]
  - [x] T-lin-10093/mac [P] `ci/must-run-registered` green on each platform — mac-arm64
        OWNER: mac-arm64 | STATE: DONE | SHA: 38e5a0ee | TS: 2026-08-15T01:15Z
        REF: DETAILS.md#t-lin-10093-mac-must-run-registered-green-on-darwin | DEPS: T-lin-10003[C] | NOTE: GREEN on Darwin — 141/141 must-run rows registered, no NOT-REGISTERED violations (registration half; run/pass half is T-lin-10092/mac). Parent stays open until /lin + /win
  - [ ] T-lin-10093/lin [P] `ci/must-run-registered` green on each platform — lin-x64
        OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
        REF: DETAILS.md#t-lin-10093-cimust-run-registered-green-on-each | DEPS: T-lin-10003[C]
  - [ ] T-lin-10093/win [P] `ci/must-run-registered` green on each platform — win-x64
        OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
        REF: DETAILS.md#t-lin-10093-cimust-run-registered-green-on-each | DEPS: T-lin-10003[C] | NOTE: BLOCKED on Windows selfhost — the ci/must-run-registered, ci/gate-contract and ci/registration-stubs cells are skip-stubbed at CMakeLists 7331 because the selfhost/census drivers need an mcc that can rebuild itself (MCC_EMBED_MCCRT, or a Darwin/mingw target); the MSVC build cannot, so they never run as live cells here (python3 IS wired). Not a quick fix — needs Windows/MSVC selfhost. The gate-contract *tool* is green when run directly (T-win-50001, 251effdc)

## Blocked — awaiting QUESTIONS.md

- [ ] T-lin-10040 [S] The device dispatcher is not merely absent — it is unwritable from what exists
      OWNER: — | STATE: BLOCKED | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10040-the-device-dispatcher-is-not-merely | DEPS: — | Q: Q-lin-10008
- [ ] T-lin-10057 [S] `kept_coverage` is host-sensitive and the bank must come from the stage2 tree
      OWNER: — | STATE: BLOCKED | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10057-kept-coverage-is-host-sensitive-and | DEPS: — | Q: Q-lin-10007
- [ ] T-lin-10058 [S] `node-census`'s `all_invokes_on_cpu` may not be worth gating at all
      OWNER: — | STATE: BLOCKED | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10058-node-censuss-all-invokes-on-cpu | DEPS: — | Q: Q-lin-10010
- [ ] T-lin-10064 [S] 63 cells have never compiled their own `EXTRA`, and arming them goes red
      OWNER: — | STATE: BLOCKED | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10064-63-cells-have-never-compiled-their | DEPS: — | Q: Q-lin-10011
## Invalidations             ← shared, append-only; removed only on re-scope (§5.2)

