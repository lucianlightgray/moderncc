# TODO

## Sessions

| SessionId | Platform | Arch  | Band        | Next ID | Last seen         |
| --------- | -------- | ----- | ----------- | ------- | ----------------- |
| mac-arm64 | macOS    | arm64 | 30000–49999 | 30004   | 2026-08-15T07:55Z |
| lin-x64   | Linux    | x64   | 10000–29999 | 10374   | 2026-08-15T15:00Z |
| win-x64   | Windows  | x64   | 50000–69999 | 50009   | 2026-08-15T14:15Z |

## Contracts — blocking, highest priority


## In progress — mac-arm64   ← only mac-arm64 writes this zone

## In progress — lin-x64     ← only lin-x64 writes this zone

- [ ] T-lin-10001 [C] A task representation with an explicit resume state, replacing the C11 threading implementation
      OWNER: lin-x64 | STATE: IN_PROGRESS | SHA: dc7a3ed9 | TS: 2026-08-15T01:55Z
      REF: DETAILS.md#t-lin-10001-slice-3b-the-teardown-is-bounded-and-the-test-says-so | DEPS: — | NOTE: slices 1/2/3a/3b DONE and green at 1dc90229 (L2′ complete; T-lin-10031 closed on it). REMAINING: slice 4 = narrow mccjit_swap_lock to the codegen region instead of holding it across each tick (own contention measurement; deliberately not bundled with 3b), then the <threads.h> single-threaded backend. No task depends on this any more. Handoff state: DETAILS.md#lin-x64-handoff-2026-08-15-preboot


## In progress — win-x64     ← only win-x64 writes this zone

## Open — claimable
- [ ] T-win-50005 [X] win-x64 — arm64-win32 COFF: add the AArch64 TLS relocations to `coff_emit_reloc`, then flip arm64 default `-c` to COFF + re-bank o0-baseline arm64-win32 + switch `arm64pe_diff.py` to force ELF
      OWNER: — | STATE: OPEN | SHA: bc0bc6bf | TS: 2026-08-15T14:15Z
      REF: DETAILS.md#t-lin-10083-win-x64-flip-default-c-to | DEPS: — | NOTE: found doing T-lin-10083. arm64-win32 was deliberately LEFT on ELF-default because `coff_emit_reloc` (src/objfmt/mccpe.c:2119) lacks the AArch64 TLS relocs — `tests/exec/features_c99_c11/tls.c` → "unsupported relocation type 549" (R_AARCH64_TLSLE_*), 296/297 corpus objects. Add the TLS arm (map to IMAGE_REL_ARM64_SECREL* per the local-exec model), confirm 297/297, THEN extend the `#if defined MCC_TARGET_X86_64 || defined MCC_TARGET_I386` gate in libmcc.c mcc_set_output_type to include arm64, re-bank, and change arm64pe_diff.py's PE arm to pass `-Wl,-oformat=pe-arm64` (its ELF-vs-ELF premise breaks once arm64 -c defaults to COFF). win-x64 has no arm64-Windows linker so verify via the o0-baseline re-encode + a mingw-arm64 link if one is vendored
- [ ] T-win-50006 [X] win-x64 — arm-win32 COFF: implement the ARM32 arm of `coff_emit_reloc` (there is none), then flip arm-win32 default + re-bank
      OWNER: — | STATE: OPEN | SHA: bc0bc6bf | TS: 2026-08-15T14:15Z
      REF: DETAILS.md#t-lin-10083-win-x64-flip-default-c-to | DEPS: — | NOTE: found doing T-lin-10083. `coff_emit_reloc` has NO `MCC_TARGET_ARM` case — it falls to `return -1`, so arm-win32 `-c` COFF fails on any file with a relocation (only 3/40 reloc-free files compile; "unsupported relocation type 2" = R_ARM_ABS32). LOW PRIORITY: arm-win32 (ARM32 Windows) is a dead platform with no executor (see T-lin-10086 split) — do this only after arm64 (T-win-50005). Add the ARM32 IMAGE_REL_ARM_* mapping (ADDR32/BRANCH24/etc.), confirm the corpus re-encodes, then flip + re-bank
- [ ] T-win-50007 [S] win-x64 — `arm64pe_diff.py` false-positive: model the LLP64-vs-LP64 `long`-width benign case
      OWNER: — | STATE: OPEN | SHA: bc0bc6bf | TS: 2026-08-15T14:15Z
      REF: DETAILS.md#t-lin-10083-win-x64-flip-default-c-to | DEPS: — | NOTE: found doing T-lin-10083. `tools/arm64pe_diff.py --corpus` flags `06_long_width.c` SUSPICIOUS (.text 192 vs 188 bytes, 70 non-reloc byte diffs) — this is the EXPECTED data-model difference: `long` is 32-bit on arm64-Windows (LLP64) and 64-bit on arm64-Linux (LP64), so the codegen legitimately differs. The tool's benign-classifier only knows reloc-site and section-presence differences; teach it that a size difference explained by sizeof(long) 4-vs-8 is benign, OR compile the corpus with a fixed-width type so the diff is data-model-neutral. Pre-existing (not from the COFF flip); the other 5 corpus files are clean
- [ ] T-win-50008 [S] win-x64 — arena take crashes (0xc0000409) compiling `src/mcc.c`: `slice/arena-intern-cap`, `fmt/arena-census-bank(-known-positive)`
      OWNER: — | STATE: OPEN | SHA: bc0bc6bf | TS: 2026-08-15T14:15Z
      REF: DETAILS.md#t-lin-10083-win-x64-flip-default-c-to | DEPS: — | NOTE: found doing T-lin-10083. These three are in T-win-50003 Bucket A but MIS-TRIAGED there as "0 slices / produced nothing" — they actually CRASH: mcc exits 0xc0000409 (STATUS_STACK_BUFFER_OVERRUN) during the arena take's compile of src/mcc.c (cmake/slicerun_interncap.cmake:20 "the compile itself failed"; cmake/fmt_arena_census_mutate.cmake "the arena take failed to compile src/mcc.c"). NOT from the COFF flip — plain `mcc -c src/mcc.c` succeeds in both COFF and ELF. Windows /GS stack-cookie catching an overrun in the arena/intern-cap dump path over a very large TU. Root-cause banked in DETAILS pending; the intern-table-exhaustion dump routine is the prime suspect per the driver's own comment
- [ ] T-lin-10057 [S] Make `kept_coverage` host-stable, so the floor is tool-enforced instead of a convention
      OWNER: — | STATE: OPEN | SHA: 8c9d4c34 | TS: 2026-08-15T11:00Z
      REF: DETAILS.md#q-lin-10007-answer-make-kept-coverage-host-stable | DEPS: — | Q: Q-lin-10007 ANSWERED | NOTE: UNBLOCKED. Human chose host-stable — explicitly NOT raising `--tol` and NOT encoding "bank from stage2" as a convention. DoD: the same tree measured from a gcc host and from a stage2 self-hosted compiler yields the same kept_coverage within the EXISTING --tol of 0.05pp, without widening it. Today they disagree: fallback 98 / kept 82.7770 (gcc host) vs 100 / 82.7139 (stage2) at -O0 — a 0.06pp spread outside tol, so banking from a gcc host re-breaks CI, which tests the stage2 tree. Once it holds, "bank from stage2" stops being a rule
- [ ] T-lin-10058 [S] `node-census`: auto-detect available hardware at runtime and run CPU/JIT/GPU
      OWNER: — | STATE: OPEN | SHA: 8c9d4c34 | TS: 2026-08-15T11:00Z
      REF: DETAILS.md#q-lin-10010-answer-node-census-auto-detects-available-hardware | DEPS: — | Q: Q-lin-10010 ANSWERED | NOTE: UNBLOCKED and RE-SCOPED — human rejected both offered options (gate-as-is / report-only). node-census must make an honest effort to run on all available hardware: auto-detect at runtime, ungate the CPU/GPU paths so they run whenever the hardware is present. ONLY permitted overrides are explicit --jit-always-cpu / --jit-always-gpu. Failure mode to refuse: detection finds no GPU and the census reports a CPU-only figure that reads as full coverage — it must say so instead. all_invokes_on_cpu drifts with the corpus (94.9385% -> 94.8004% purely because src/mcc.c amalgamated ~2700 lines) and is not a regression signal; the external-only ceiling 99.2540% remains the headline. Runs through the T-lin-10082 seam
- [ ] T-lin-10064 [S] Root-cause the three `rir_parity` divergences the unarmed `EXTRA` is masking, and file a fix task per divergence
      OWNER: — | STATE: OPEN | SHA: 8c9d4c34 | TS: 2026-08-15T11:00Z
      REF: DETAILS.md#q-lin-10011-answer-divergences-become-tracked-investigation-and-fix-tasks | DEPS: — | Q: Q-lin-10011 ANSWERED | NOTE: UNBLOCKED and RE-SCOPED — human rejected arm-and-take-red AND leave-masked. Each divergence becomes a tracked investigation + an implementation task; "red cells generally" is now a standing rule, a red cell is a task-shaped object. THIS task is the investigation half: at -O0 full_language.c has 303 bodies, 299 faithful, 1 empty and 3 that are not, against rir_parity's hard 100% bar. DoD = a root cause for each of the three + one fix task filed per divergence (allocate from lin's band at file time); arming the 63 EXTRA cells (one -I each) follows once the bar is met, NOT before. The EXTRA has never contributed anything — that stays recorded, not hidden
- [ ] T-lin-10040 [S] The device dispatcher is not merely absent — it is unwritable from what exists
      OWNER: — | STATE: OPEN | SHA: 8c9d4c34 | TS: 2026-08-15T11:00Z
      REF: DETAILS.md#q-lin-10008-answer-the-device-path-freeze-is-lifted | DEPS: — | Q: Q-lin-10008 ANSWERED | NOTE: UNBLOCKED — the 2026-08-09 device-path freeze is lifted, all six frozen rows are schedulable. SCHEDULABLE IS NOT JUSTIFIED: the break-even table that motivated the freeze is not repealed and still prices this lever negative (three subsystems, priced nowhere), and float-in-the-emitter already demonstrated the shape empirically — it landed for `double` and moved the device-executable fraction by ~0.0 iteration-weighted points. Whoever takes this either re-prices the lever or states that the value is something other than the device-executable fraction. Re-ranks with T-lin-10033..10038
- [ ] T-lin-10045 [S] `-fopt-slice`: revise into the governor over every AST/RIR slice-capable strategy, integrated with the other slice optimizers
      OWNER: — | STATE: OPEN | SHA: 3749f816 | TS: 2026-08-15T02:30Z
      REF: DETAILS.md#q-lin-10006-answer-fopt-slice-is-the-governor-not-a-pass | DEPS: — | Q: Q-lin-10006 ANSWERED | NOTE: not "own or delete" — it was never a pass. Carries forward: the disk-cache determinism defect, and OPT_SLICE at MCC_OPTD_LEVEL(9) leaves opt-cache-determinism a permanent 77 with no subject. First slice = a shipped level with the determinism claim gated
- [ ] T-lin-10042 [X] mac-arm64 — the Metal parity staged plan, WITH fp64 (2,200-3,400 lines)
      OWNER: — | STATE: OPEN | SHA: 0d33d71e | TS: 2026-08-15T02:20Z
      REF: DETAILS.md#q-lin-10009-answer-metal-parity-scheduled-with-fp64 | DEPS: — | Q: Q-lin-10009 ANSWERED | NOTE: human scheduled the fp64 variant. No CI differential exists or can — land it in slices each checkable by the hand-run per-value differential, not as one unwatched arm. SCOPING (mac-arm64, 2026-08-15): it is EXTEND-not-rebuild — the MSL base survives (345 `msl_*` refs across src/mccgpu.{c,h}, mccslice.h, mccfmt.h) and its per-value differential RUNS natively on this M1 (gpu/msl-slice-differential + -known-positive + -real all green here; the hand-run tool is tools/slicerun.c). So the per-slice verification path is confirmed feasible on-box; the work is the ~2200-line MSL/SPIR-V parity gap + fp64. FIRST-SLICE POINTERS: the MSL emitter is in src/mccgpu.h (macro/inline-heavy — msl_iv/bv/pv value builders at ~260-340, msl_arith/guard_div/widen/int_of_bool), the SPIR-V arm + differential in tools/slicerun.c; fp64 is ALREADY partially present in BOTH (mccgpu.h ~1330-1447 `f64`, slicerun.c ~109/452-471/804 Float64/double), so this is op-by-op parity completion, not greenfield fp64. Method for slice 1: diff the two arms' op coverage against the gpu/msl-slice-differential corpus, take the smallest fp64 op MSL lowers differently than SPIR-V, close it, hand-run the per-value differential. Wants a fresh dedicated start (per its own "not one unwatched arm"), not a session-tail rush
- [ ] T-lin-10086 [S] `arm64-win32` execution on a `windows-11-arm` CI runner (was [X] win-x64)
      OWNER: — | STATE: OPEN | SHA: 3cf6e238 | TS: 2026-08-15T02:10Z
      REF: DETAILS.md#q-lin-10013-answer-ci-is-the-woa-executor | DEPS: T-lin-10365[S] | NOTE: Q-lin-10013 ANSWERED — CI is the executor, so this is no longer win-x64-only. SPLIT: the `arm-win32` (ARM32) half has NO executor — Windows 11 on ARM64 does not run ARM32 apps — and must not be reported green with the arm64 half
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
- [ ] T-lin-10010 [S] Implement reversed `scalar_storage_order`; refusing it is the safe interim, not the feature
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10010-implement-reversed-scalar-storage-order | DEPS: —
- [ ] T-lin-10012 [S] 32-byte vectors are laid at 16-byte alignment, so cross-TU to gcc is incompatible
      OWNER: lin-x64 | STATE: IN_PROGRESS | SHA: 3acffeb1 | TS: 2026-08-15T15:00Z
      REF: DETAILS.md#t-lin-10012-the-references-measured-alignof-is-not-the-abi-and-the-rule-is-per-target | DEPS: — | Q: Q-lin-10005 ANSWERED | NOTE: human chose the ABI change, NOT the documented-incompatibility hold. PHASE 1 + 1b DONE (measurement only, no code): they REVISE the instrument the answer named. (a) MCC_MAX_ALIGN is the WRONG knob — it also feeds bare `__attribute__((aligned))` at mccgen.c:5815 (a second unasked ABI change), wide256_slice.h:59 is immune, and one per-arch constant cannot express a rule that must now differ per target. The surgical site is mccgen.c:7064 alone. (b) `_Alignof` is NOT the ABI: on x86_64 gcc reports 16 and lays the member at 32, clang reports 32 and lays it at 32 — the references disagree on the report and agree on the LAYOUT, and mcc's `_Alignof` of 16 already matches gcc. The defect is that mcc lays out with that 16: `struct{char; v32}` is 48 in mcc, 64 in both references. (c) THE RULE IS PER TARGET — x86_64/i386: layout alignment = the vector's size; arm64: min(size,16), which mcc ALREADY matches and where a change would be a REGRESSION (measured vs gcc-16 + Apple clang). So the earlier "eleven columns move" is wrong: only the x86_64 and i386 columns move, for the single banked object tests/exec/types/int256_gates.c, and arm64 must not. REMAINING, in order: (1) the missing fixture — NO test in the tree compiles a 32-byte vector, so the fix would land completely unexercised; write the per-target cross-TU fixture asserting offsetof/sizeof identical to that target's reference cc (must pass UNCHANGED on arm64, is its known-positive; fails today on x86_64/i386), (2) change mccgen.c:7064, (3) re-bank only the columns the rule moves. Re-bank is lin-x64's: target-keyed keys and o0_ab refuses banking from a partial measurement
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
  - [x] T-lin-10030/mac [P] The embed JIT is measured only on x86_64 — mac-arm64
        OWNER: mac-arm64 | STATE: DONE | SHA: 20a82ad3 | TS: 2026-08-15T04:00Z
        REF: DETAILS.md#t-lin-10030-mac-conformance-provisioned-and-the-simplectest-ub-flag | DEPS: — | NOTE: DONE. Embed-JIT measured natively on arm64: jit family 66/66 + selfcheck boot the engine, and with the corpora provisioned host-local (gcc c-torture + llvm-test-suite, same not-vendored shape as T-lin-10088) jit/xoracle-conformance (535 progs) + coverage both PASS. The one flag (SimpleCTest UB) was a harness gap, fixed as T-mac-30003. Parent stays open until /lin + /win
  - [ ] T-lin-10030/lin [P] The embed JIT is measured only on x86_64 — lin-x64
        OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
        REF: DETAILS.md#t-lin-10030-the-embed-jit-is-measured-only | DEPS: —
  - [ ] T-lin-10030/win [P] The embed JIT is measured only on x86_64 — win-x64
        OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
        REF: DETAILS.md#t-lin-10030-the-embed-jit-is-measured-only | DEPS: —
- [ ] T-lin-10032 [S] `MCCJIT_POOL_MAX` is 64 and `mccjit_pool_start` clamps to it silently
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10032-mccjit-pool-max-is-64-and | DEPS: —
- [ ] T-lin-10033 [S] The Vulkan dispatch destroys resources under a still-pending command buffer
      OWNER: — | STATE: OPEN | SHA: 8c9d4c34 | TS: 2026-08-15T11:00Z
      REF: DETAILS.md#t-lin-10033-the-vulkan-dispatch-destroys-resources-under | DEPS: — | Q: Q-lin-10008 ANSWERED | NOTE: re-ranked — the 2026-08-09 device-path freeze that de-ranked this row is lifted (never BLOCKED, only de-ranked). Schedulable, not justified: the break-even table still prices the device lever negative
- [ ] T-lin-10034 [S] `mcc_gpu_mem_index` picks the worst memory type on this machine
      OWNER: — | STATE: OPEN | SHA: 8c9d4c34 | TS: 2026-08-15T11:00Z
      REF: DETAILS.md#t-lin-10034-mcc-gpu-mem-index-picks-the | DEPS: — | Q: Q-lin-10008 ANSWERED | NOTE: re-ranked — the 2026-08-09 device-path freeze that de-ranked this row is lifted (never BLOCKED, only de-ranked). Schedulable, not justified: the break-even table still prices the device lever negative
- [ ] T-lin-10035 [S] `devs[0]` is chosen with no scoring while `VkPhysicalDeviceLimits` is transcribed and unread
      OWNER: — | STATE: OPEN | SHA: 8c9d4c34 | TS: 2026-08-15T11:00Z
      REF: DETAILS.md#t-lin-10035-devs0-is-chosen-with-no-scoring | DEPS: — | Q: Q-lin-10008 ANSWERED | NOTE: re-ranked — the 2026-08-09 device-path freeze that de-ranked this row is lifted (never BLOCKED, only de-ranked). Schedulable, not justified: the break-even table still prices the device lever negative
- [ ] T-lin-10036 [S] `ast_ladder_gpu_run` uploads `tin` twice per rung and two memsets are dead
      OWNER: — | STATE: OPEN | SHA: 8c9d4c34 | TS: 2026-08-15T11:00Z
      REF: DETAILS.md#t-lin-10036-ast-ladder-gpu-run-uploads-tin | DEPS: — | Q: Q-lin-10008 ANSWERED | NOTE: re-ranked — the 2026-08-09 device-path freeze that de-ranked this row is lifted (never BLOCKED, only de-ranked). Schedulable, not justified: the break-even table still prices the device lever negative
- [ ] T-lin-10037 [S] The emitter's constant cache binds before module size
      OWNER: — | STATE: OPEN | SHA: 8c9d4c34 | TS: 2026-08-15T11:00Z
      REF: DETAILS.md#t-lin-10037-the-emitters-constant-cache-binds-before | DEPS: — | Q: Q-lin-10008 ANSWERED | NOTE: re-ranked — the 2026-08-09 device-path freeze that de-ranked this row is lifted (never BLOCKED, only de-ranked). Schedulable, not justified: the break-even table still prices the device lever negative
- [ ] T-lin-10038 [S] No tree-recursion exec golden exists, and the failure mode is a GPU hang
      OWNER: — | STATE: OPEN | SHA: 8c9d4c34 | TS: 2026-08-15T11:00Z
      REF: DETAILS.md#t-lin-10038-no-tree-recursion-exec-golden-exists | DEPS: — | Q: Q-lin-10008 ANSWERED | NOTE: re-ranked — the 2026-08-09 device-path freeze that de-ranked this row is lifted (never BLOCKED, only de-ranked). Schedulable, not justified: the break-even table still prices the device lever negative
- [ ] T-lin-10041 [X] mac-arm64 — the native MSL region arm
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10041-mac-arm64-the-native-msl-region | DEPS: —
- [ ] T-lin-10043 [S] `tests/emitmap/bank.json`'s tolerances cannot fail, and the selfhost cell is already drifted inside them
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10043-testsemitmapbankjsons-tolerances-cannot-fail | DEPS: T-lin-10003[C]
- [ ] T-lin-10044 [S] `rir-coverage.py`'s `wide` corpus silently drops 9 sources and computes every percentage over the rest
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10044-rir-coveragepys-wide-corpus-silently-drops | DEPS: —
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
- [ ] T-lin-10073 [X] lin-x64 — the two wine `run-tier` cells are load-sensitive
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10073-measured-the-mechanism-is-a-foreign-wineserver | DEPS: — | NOTE: MEASURED 2026-08-14 — mechanism is a FOREIGN wineserver (not CPU, not -j width; serial retry at loadavg 7 still timed out). Corroborated 2026-08-15 under a 24-way compile load at loadavg 24.5: 15/15 rounds pass, both cells 7.19s/2.85s with no wineserver resident. Run `pgrep -a wineserver` before attributing either cell to a commit
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
      REF: DETAILS.md#q-lin-10012-answer-adopt-divdc3-style-complex-division | DEPS: — | Q: Q-lin-10012 ANSWERED | NOTE: human said ADOPT `__divdc3`-style complex division (not the bank-the-divergence hold). Replace mcc's current finite-case complex divide — the three 53%-relative-error finite quotients are a QoI defect even though Annex G does not specify finite accuracy and mcc's G.5.1 infinity/NaN claim is honoured. Then re-bank every `csweep` complex row once (C64/C80.CDIV + CDIVSEL, 283 refs-agree points each, 44 finite)
- [ ] T-lin-10078 [S] N36 residue — `/` and `%` on over-wide bit-fields are still per-operation truncated
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10078-n36-residue-and-on-over-wide | DEPS: —
- [ ] T-lin-10081 [S] `MCC_GPU_LOCK`: replicate the resident state per context, then narrow the lock
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10081-mcc-gpu-lock-replicate-the-resident | DEPS: —
- [ ] T-lin-10082 [S] The `--jit-always-gpu` boundary is struct member access, not link-time symbols
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10082-the-jit-always-gpu-boundary-is | DEPS: —
- [ ] T-lin-10084 [X] win-x64 — non-constant `__except` filters and `__finally` need funclet codegen
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10084-win-x64-non-constant-except-filters | DEPS: —
- [ ] T-lin-10092 [P] Record a clean full native suite number on each platform
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10092-record-a-clean-full-native-suite | DEPS: —
  - [x] T-lin-10092/mac [P] Record a clean full native suite number on each platform — mac-arm64
        OWNER: mac-arm64 | STATE: DONE | SHA: 408a8979 | TS: 2026-08-15T05:00Z
        REF: DETAILS.md#t-lin-10092-mac-the-darwin-suite-number-zero-genuine-failures | DEPS: — | NOTE: DONE. NUMBER: 10060 cells, 0 GENUINE failures. All 16 reds were Timeouts, all environmental: 15 flagsweep-exec load-induced (pass serially -j1); selfhost-jit not a hang — completes byte-identical at 412-540s, over the 300s bound, fixed by TIMEOUT->720 (408a8979). Matches lin's Linux 0/9051. Parent stays open until /lin + /win close
  - [x] T-lin-10092/lin [P] Record a clean full native suite number on each platform — lin-x64
        OWNER: lin-x64 | STATE: DONE | SHA: a4b2baf1 | TS: 2026-08-15T08:30Z
        REF: DETAILS.md#t-lin-10092-lin-the-linux-full-native-suite-is-clean | DEPS: — | NOTE: DONE. NUMBER: 10062 cells, 1011 skipped, 9051 run, 0 failures, 86 min. Already archived at 32d29fc4 — line kept visible only until the [P] parent closes, per mac's convention on /mac; whoever lands /win removes all three children + the parent together
  - [ ] T-lin-10092/win [P] Record a clean full native suite number on each platform — win-x64
        OWNER: win-x64 | STATE: IN_PROGRESS | SHA: 260bb900 | TS: 2026-08-15T01:55Z
        REF: DETAILS.md#t-lin-10092-record-a-clean-full-native-suite | DEPS: — | NOTE: NUMBER RECORDED (first ever on Windows): 9387 cells, 8388 pass / 945 skip / 54 fail at 9b21c352; 19 false-reds fixed at 260bb900 -> 8388 / 964 / 35. Not clean — the 35 residual reds are triaged in T-win-50003 (28 GPU-slice, 4 fp opt-search, 3 jit/runtime). @lin: number is landed, slice-3 (L2′) hold can release
- [ ] T-lin-10093 [P] `ci/must-run-registered` green on each platform
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10093-cimust-run-registered-green-on-each | DEPS: T-lin-10003[C]
  - [x] T-lin-10093/mac [P] `ci/must-run-registered` green on each platform — mac-arm64
        OWNER: mac-arm64 | STATE: DONE | SHA: 38e5a0ee | TS: 2026-08-15T01:15Z
        REF: DETAILS.md#t-lin-10093-mac-must-run-registered-green-on-darwin | DEPS: T-lin-10003[C] | NOTE: GREEN on Darwin — 141/141 must-run rows registered, no NOT-REGISTERED violations (registration half; run/pass half is T-lin-10092/mac). Parent stays open until /lin + /win
  - [x] T-lin-10093/lin [P] `ci/must-run-registered` green on each platform — lin-x64
        OWNER: lin-x64 | STATE: DONE | SHA: 92ea0a4e | TS: 2026-08-15T09:10Z
        REF: DETAILS.md#t-lin-10093-lin-must-run-registered-green-on-linux | DEPS: T-lin-10003[C] | NOTE: DONE, BOTH halves. Registration: 143/143 rows registered, no violations. Run/pass: --results over the 10062-cell suite JUnit, also 143 satisfied — no must-run row reported Skipped. mac has 141 (the delta is my two osx/headers-parse rows). Parent needs /win
  - [ ] T-lin-10093/win [P] `ci/must-run-registered` green on each platform — win-x64
        OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
        REF: DETAILS.md#t-lin-10093-cimust-run-registered-green-on-each | DEPS: T-lin-10003[C] | NOTE: BLOCKED on Windows selfhost — the ci/must-run-registered, ci/gate-contract and ci/registration-stubs cells are skip-stubbed at CMakeLists 7331 because the selfhost/census drivers need an mcc that can rebuild itself (MCC_EMBED_MCCRT, or a Darwin/mingw target); the MSVC build cannot, so they never run as live cells here (python3 IS wired). Not a quick fix — needs Windows/MSVC selfhost. The gate-contract *tool* is green when run directly (T-win-50001, 251effdc)

## Blocked — awaiting QUESTIONS.md

_Empty — Q-lin-10007/10008/10010/10011 were answered 2026-08-15 and all four tasks moved to “Open — claimable”._

## Invalidations             ← shared, append-only; removed only on re-scope (§5.2)

