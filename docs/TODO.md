# TODO

## Sessions

| SessionId | Platform | Arch  | Band        | Next ID | Last seen         |
| --------- | -------- | ----- | ----------- | ------- | ----------------- |
| mac-arm64 | macOS    | arm64 | 30000–49999 | 30007   | 2026-08-15T23:25Z |
| lin-x64   | Linux    | x64   | 10000–29999 | 10392   | 2026-08-16T00:17Z |
| win-x64   | Windows  | x64   | 50000–69999 | 50022   | 2026-08-16T00:16Z |

## Contracts — blocking, highest priority


## In progress — mac-arm64   ← only mac-arm64 writes this zone

## In progress — lin-x64     ← only lin-x64 writes this zone

- HANDOFF-BOX-FACTS (lin-x64, kept for successors): FULL SUITES RUN AT `-j16` ON THIS BOX — user directive 2026-08-16, until withdrawn. This SUPERSEDES M-TODO-0005's prominent "run full suites at -j32 on this host" line; do NOT revert a -j16 invocation to -j32 (DETAILS.md#lin-x64-full-suite-parallelism-is-j16-superseding-the-j32-convention). Independently the better setting here: the -j32 §8 run reported 2 failures and only ONE was genuine (slice/quiesce passes standalone in 0.35s = load-induced T-lin-10074; slice/census reproduced = real, T-lin-10391). After ANY aborted suite, wait for /proc/loadavg < 8 before starting the next — a killed -j32 run holds the 1-min average above 100 and a suite started into that decay measures contention, not the tree. Corpora are host-local symlinks in vendor/ -> ~/Projects/{gcc,llvm-test-suite,llvm-project} and are NOT tracked: if they vanish, six cells silently become skip stubs instead of failing (T-lin-10388). cmake-cross measures 7/7 active o0-baseline keys incl. BOTH *-osx; cmake-debug is the plain native build

- [ ] T-lin-10001 [C] A task representation with an explicit resume state, replacing the C11 threading implementation
      OWNER: lin-x64 | STATE: IN_PROGRESS | SHA: dc7a3ed9 | TS: 2026-08-15T13:25Z
      REF: DETAILS.md#t-lin-10001-slice-3b-the-teardown-is-bounded-and-the-test-says-so | DEPS: — | NOTE: PAUSED (heartbeat intentionally stale; TTL-eligible for any session to resume). slices 1/2/3a/3b DONE and green at 1dc90229 (L2′ complete; T-lin-10031 closed on it). REMAINING: slice 4 = narrow mccjit_swap_lock to the codegen region instead of holding it across each tick (own contention measurement; deliberately not bundled with 3b), then the <threads.h> single-threaded backend. No task depends on this any more. Handoff state: DETAILS.md#lin-x64-handoff-2026-08-15-preboot
- [ ] T-lin-10390 [X] lin-x64 — re-bank lin's six active `o0-baseline` keys under the new `keys.txt` manifest
      OWNER: lin-x64 | STATE: IN_PROGRESS | SHA: 283d454b | TS: 2026-08-15T23:34Z
      REF: DETAILS.md#t-mac-30006-done-per-session-measurable-o0-baseline-with-a-key-manifest | DEPS: — | NOTE: executes mac's CONTRACT 283d454b (T-mac-30006 DONE, option (c) = per-session-measurable manifest; my re-pricing at 14539c6b fed the `provisionable` state, which points at the qemu-<arch>-<libc>-fetch fixture). SPLIT OF THE ACTIVE SET: mac banked arm64-osx; lin takes the other SIX a Linux cross box measures — x86_64, x86_64-win32, i386-win32, arm64-win32, arm-win32, x86_64-osx (all six have a cmake-cross/mcc-<key> and need NO sysroot; only the 4 bare-ELF `provisionable` keys do). CMD: `C2_NO_EXTRA=1 O0_AB_BANK=1 tools/o0_ab.sh cmake-cross measurable` plus the `MCC_DEV=1 O0_AB_GATES=1` gated line. Clears the da0932e2 algebraic_identities.c drift on lin's keys. SEQUENCED, not delayed: NOT started while slice/cref-oracle-* is mid-run at load ~35 — those cells fail on any per-program stall against a 180 s budget, so a concurrent full o0_ab measurement risks manufacturing the exact stall class T-lin-10359 just proved was host contention, and a false red there is indistinguishable from a real one. Object hashes are load-independent; the risk is to the NEIGHBOURING cells' verdicts, not to this measurement. EXPECT THE WIN32 KEYS TO MOVE TWICE: win's T-win-50015 (ms_bitfields=1, target-keyed on MCC_TARGET_PE) moves all four *-win32 keys and its landing is blocked on a Linux session re-banking them — lin volunteered at 6c0dd551. Sequential re-banks of the same keys, not a conflict. Safe because the board summary is banked only from `all` (mac's note), so a `measurable` re-bank does not clobber it. Verify: ast/o0-baseline + ast/o0-baseline-gated green, and o0_ab reports no key that mcc can build here yet appears in neither manifest state
- [ ] T-lin-10359 [X] lin-x64 — `slice/cref-oracle-*` stalls on five programs when the host GPU is busy
      OWNER: lin-x64 | STATE: IN_PROGRESS | SHA: 6c0dd551 | TS: 2026-08-15T23:08Z
      REF: DETAILS.md#t-lin-10359-verified-on-an-idle-gpu-zero-stalls-644s | DEPS: — | NOTE: ROW VERIFICATION SATISFIED — slice/cref-oracle-gcc-c-torture-execute PASSED 644.57s with ZERO stalls on a confirmed-idle GPU (gpuconform.py:367's stall line absent — the direct signal, not an exit-code inference). programs 1917 / qualified 1562 (floor 1000) / mismatches 0 / funnel-disagreed 0 / all four oracle legs ok=296 mismatch=0 nocompile=0 / dispatches 77094. Self-proving in the same run: "known-positive OK, 1184 mutated batch(es) detected". CONFIRMS the row's thesis — the 2026-08-14 stalls were host contention (bg3_dx11.exe at 72% GPU), not code; idle-GPU time 644.57s beats BOTH recorded passes (1442.91s, 1530.03s). No code change made or needed. ONE UNRECONCILED NUMBER, split out as T-lin-10389 rather than buried: cref tuples 330388 vs the baseline's 2067654 and per-leg ok=296 vs 2130 — a 6.3x drop at an IDENTICAL qualified=1562. Not asserted as a regression (intervening slice work incl. 6707857a deliberately narrows the funnel), but MINTUPLE=50000 cannot distinguish the cases. AWAITING §8: full native suite. Provisioning half: BLOCKER CLEARED — all four cref corpora are now provisioned on lin (vendor/ symlinks -> ~/Projects/{gcc,llvm-test-suite,llvm-project}, host-local + untracked, same shape as mac/win) and all four slice/cref-oracle-* cells re-register as LIVE commands instead of skip stubs after `cmake -S . -B cmake-debug`. Counts match win's table exactly (regression-c 1745, unittests 671). Now running the row's own verification on the idle GPU. The stated in-practice blocker is CLEARABLE on this host after all: the row said "the gcc c-torture corpus is not provisioned", but `~/Projects/{gcc,llvm-test-suite,llvm-project}` ALL exist here (5.5G/2.5G/6.4G); vendor/ simply never had the symlinks (same host-local, untracked shape mac and win already use). Provisioning vendor/gcc-c-torture-execute + the two llvm-test-suite corpora un-skips slice/cref-oracle-* and makes this row's own verification executable. GPU confirmed idle at claim (0% util, 153 MiB, 47C, load 1.70; only 7/9 MiB resident non-compute clients) per the ACTIVITY-not-residence diagnostic


## In progress — win-x64     ← only win-x64 writes this zone

- HANDOFF-BOX-FACTS (win-x64, kept for successors): VK_LOADER_LAYERS_DISABLE=VK_LAYER_AMD_switchable_graphics for any device run; VULKAN_SDK=C:/Users/llg/scoop/apps/vulkan/current; corpora junctions in vendor/ point at C:/Users/llg/Projects/{gcc-torture,llvm-test-suite,llvm-project}; commit BEFORE pull (DETAILS#autostash-is-how-conflict-markers-reach-pushed-history); MSVC build needs vcvars64.bat env for cmake --build (INCLUDE/LIB); embed-jit blob now auto-built with the winlibs mingw gcc on MSVC (T-win-50020 = the LINK, d46e9ee2; but embed-JIT still doesn't hot-swap — PE bake path gated off — so the engine reds are NOT paid, that's T-win-50021). CORRECTION: T-lin-10030/win is NOT unblocked (its measurement needs a working swap, not just the link). NEXT WIN WORK: T-win-50021 (PE hot-swap + strtold — the real embed-JIT), T-win-50015 (ms-bitfield ABI; fixture in-tree, two named gaps), T-lin-10092/win requote. requote 2026-08-15T19: 15 reds (composition: slice/fault=T-win-50019, slice/cost, cref-oracle=T-lin-10359, smoke/native+strats, smoke/engines×3+mcctest-embedjit=T-win-50021, 4 fp emitsize/emitiso, runtime-bench-check=vendor/plb; def-verify fixed by reconfigure). jit/replay-parity flipped green vs the prior 15

## Open — claimable
- [ ] T-lin-10391 [S] `slice/census` strands the columns the adding session cannot measure — o0-baseline's defect, without o0-baseline's fix
      OWNER: — | STATE: OPEN | SHA: 599ff401 | TS: 2026-08-16T00:17Z
      REF: DETAILS.md#t-lin-10391-slicecensus-strands-the-columns-the-adding-session-cannot-measure | DEPS: — | NOTE: THIRD recorded instance of one corpus edit stranding fleet banks (after wide_bitfield_arith.c and this same algebraic_identities.c on o0-baseline). slice/census globs its corpus (slicerun_census.cmake:17 file(GLOB_RECURSE tests/exec/*.c)) and compares against a HARD-PINNED per-platform column — arm64-Darwin 983, arm64-Linux 1022, x86_64-Linux 941 — so any new tests/exec fixture moves EVERY column while the adding session can measure only its own. da0932e2 (win) added algebraic_identities.c; blocks moved 941->946 and the cell went red on a box that did not make the change. Linux column re-banked at 3f379a0c (attribution safe by the file's own test: inv-blocks 454 / all-internal 169 / all-external 197 / mixed 87 / any-indirect 1 ALL unmoved — corpus grew, classifier did not). STILL STALE + NOT FIXABLE FROM HERE: arm64-Darwin (mac can re-take) and arm64-Linux (Debian-in-Docker, no current owner). A RE-BANK IS NOT THE FIX — the next fixture reopens it. Asymmetry that is the actual bug: the unbanked-combination branch SKIPS the exact-count half, so an unknown platform degrades quietly while a known one hard-fails. THREE OPTIONS: (a) RECOMMENDED — the header-independent corpus the file itself proposes ("give the census a corpus that does not include system headers, which would make one column serve everywhere"): one column cannot be stranded, strictly better than three-plus-a-manifest, removes the split rather than administering it; (b) a keys.txt-style manifest mirroring T-mac-30006's o0-baseline fix — proven in-tree, cheaper landing; (c) derive the counts instead of pinning them. Verify: add a tests/exec fixture on one platform and require no other platform's slice/census to red — which nothing in the tree satisfies today
- [ ] T-lin-10388 [S] Host-local provisioning vanishes silently, and every loss gets re-recorded as a permanent fleet property
      OWNER: — | STATE: OPEN | SHA: ab7281f0 | TS: 2026-08-15T23:34Z
      REF: DETAILS.md#t-lin-10388-host-local-provisioning-vanishes-silently-and-each-loss-is-re-recorded-as-a-fleet-fact | DEPS: — | NOTE: FOUND while holding T-lin-10359, and it is the ROOT of both that row's false blocker and T-mac-30006's. When a host-local resource disappears (lin's 4 stage3 sysroots + the cref corpus symlinks, all provably present 2026-08-13 per M-TODO-0004: 6.6 GB fetched, ctest -L qemu 39/39, run-tier/{i386,arm,arm64,riscv64} passing) NO cell fails — every dependent cell degrades to an mcc_skip_test echo stub and the suite still reports green, so the loss is indistinguishable from a box that never had it. The next session then reads the skip text, believes it describes the FLEET, and writes it into a task as permanent: T-lin-10359 ("corpus is not provisioned") and T-mac-30006 ("NO current box has the gentoo sysroots", escalated to a HUMAN/infra decision) were BOTH written about a box that had had the thing days earlier. M-TODO-0004 even records the identical gcc-c-torture symlink being added once before, with optlevel/torture-differential skipping silently until it was — the same gap opened, closed, reopened and closed again on one machine in three days. It also corrupts headline numbers: M-TODO-0005 quotes lin at 10025 cells / 0 failures (provisioned); post-reboot T-lin-10092/lin quotes 10062 / 1011 skipped / 9051 run / 0 failures and reads equally clean — but part of that 1011 is capability that WAS present and is gone. FIRST SLICE IS A DETECTOR, NOT A RE-PROVISION: a cell that records whether this box is EXPECTED to have each optional local resource and FAILS when one that was present goes missing, instead of degrading to a stub. That is T-mac-30006 option (c) generalised past o0-baseline — worth noting (c) independently answers this row and (a) does not. Re-provisioning lin is the cheap immediate step and is NOT the fix; without a detector the next reboot reopens all of it. Verify: remove a provisioned resource, re-run, require a NAMED failure rather than a larger skip count
- [ ] T-lin-10389 [S] The cref tuple count fell 6.3x at an identical program count, and `MINTUPLE` cannot see it
      OWNER: — | STATE: OPEN | SHA: 3a699f01 | TS: 2026-08-15T23:34Z
      REF: DETAILS.md#t-lin-10389-the-cref-tuple-count-fell-6x-and-the-floor-cannot-see-it | DEPS: — | NOTE: split out of the T-lin-10359 verification so that row could close on its own evidence. Two runs of slice/cref-oracle-gcc-c-torture-execute over the same corpus, both with every value check clean: qualified 1562 -> 1562 (1.00x) but cref tuples 2067654 -> 330388 (0.16x) and per-leg oracle ok= 2130 -> 296 (0.14x). EITHER the intervening slice/GPU work narrowed the funnel on purpose (6707857a deletes a quarantine + applies the usual arithmetic conversions, which legitimately removes fragments) OR the differential quietly lost five-sixths of its subject and has been reporting mismatches=0 over it. NOT DIAGNOSED — observed only; tools/gpuconform.py + cmake/gpuconform_cref.cmake are unchanged since before the baseline, but src/ast_eval_slice.h, src/mccgpu.c, src/mccslice.h, src/mccast.c all churned after 2026-08-14. EITHER WAY THE FLOOR IS WRONG: MINTUPLE=50000 was clearing by 41x and now clears by 6.6x, and would still pass after another 6x fall — same class as T-lin-10043 (a tolerance that cannot fail). FIRST SLICE = attribution not fix: bisect the tuple count across the 2026-08-14->15 slice/GPU commits, name the change and say whether it was intended; only then choose re-flooring vs restoring coverage. Verify: the bisect names a commit + reason; the re-floored cell goes red when the tuple count halves from its current value
- [ ] T-win-50021 [S] win-x64 — the Windows embed-JIT boots but never hot-swaps (PE bake path gated off) + strtold double-def; this is what actually pays the engine reds, not T-win-50020's link
      OWNER: — | STATE: OPEN | SHA: d46e9ee2 | TS: 2026-08-15T19:01Z
      REF: DETAILS.md#t-win-50020-correction-the-done-was-premature-link-fixed-but-the-pe-hot-swap-is-gated-off-so-the-engine-reds-are-not-paid | DEPS: — | NOTE: split out of the T-win-50020 correction (its DONE was link-only; jit/replay-parity is swap-agnostic and passed, hiding this). smoke/engines FAILS "the --embed-jit binary ran without swapping a single function" — the mingw-blob embed-JIT links+boots+runs AOT but never hot-swaps because the PE bake/swap path is GATED OFF by design (CMakeLists.txt:2981, "ELF + Apple Mach-O; PE bake path still gated off ... can still SIGBUS"). Also mcctest-embedjit FAILS link with libmingwex.a 'strtold defined twice' (mcc's win32 runtime stdlib.h __CRT_INLINE strtold + mcchost.h #define strtold vs libmingwex's export). TWO SLICES: (1) resolve strtold double-def; (2) ungate + make the PE hot-swap work (deep, the 2981 SIGBUS caveat is live — this is the real cost). Pays smoke/engines(+known-positive+identity), mcctest-embedjit, and T-lin-10030/win's measurement half. Verify: smoke/engines green (all 8 required engines swap) + mcctest-embedjit green. STRTOLD ROOT CAUSE (win-x64 investigated, NOT the win32 stdlib inline — that theory disproven twice): mcctest-embedjit's subject full_language.c DEFINES its own strtold at tests/diff/parts/legacy_aggregates.h:880 (a "program redefines a libc function" test). Under --embed-jit the mingw blob (libmccjitengine-mingw.a) has `U strtold` and mcc pulls libmingwex.a's strtold to satisfy it — colliding with the program's def ("defined twice", reported at libmingwex). Only libmingwex among the mingw libs defines strtold; blob+mccrt don't. FIX DIRECTION: mcc's link of the mingw blob's import/runtime libs must resolve the blob's U-refs against the PROGRAM's defs FIRST and not pull a libmingwex archive member that redefines a program symbol (archive-member-granularity / link-order issue in the libmcc.c:1753-1824 mingw-lib add path). Reproduce: `mcc -O1 --embed-jit --jit-functions main -DCC_NAME=CC_gcc -DGCC_MAJOR=19 -I. -Iruntime/include tests/diff/full_language.c -o x.exe`. WHY the blob references strtold at all: mcchost.h:69 guards `#define strtold (long double)strtod` behind `#ifndef __GNUC__` (MSVC-only), so the mingw/GCC-compiled blob skips it and emits a real `U strtold` — the cl.exe blob never did (macro active), which is why this is mingw-path-specific. FIX ANGLES: (a) TESTED + REJECTED (win-x64) — extending the mcchost.h strtold/strtof macro to all _WIN32 (blob routes strtold->strtod) DID compile the mingw blob clean (no mangling; <stdlib.h>'s include guard mitigates the comment's fear) and removed the blob's `U strtold` (nm confirmed), BUT `defined twice` PERSISTS: a SECOND puller exists — the mingw CRT itself (libmingwex/libmsvcrt scanf `%Lf` machinery references strtold, pulled by the program's ordinary printf/CRT usage) drags in libmingwex's strtold.o to collide with full_language.o's own def (nm /tmp/fl.o = `T strtold` only, no U — the program only DEFINES it). So (a) is a dead end. (b) IS THE ONLY COMPLETE FIX (deep; libmcc.c:1753-1824 + mcc's archive walk): mcc must NOT pull a libmingwex archive member (strtold.o) that redefines a symbol the program already STRONG-defines — standard archive semantics (a strong def satisfies refs; a member is pulled only for an UNDEFINED symbol). That one rule handles both pullers (blob + CRT)
- [ ] T-mac-30005 [S] The chain-store 8% gate has never run against real input; when it does it reads +0.0%
      OWNER: — | STATE: OPEN | SHA: 157da7a7 | TS: 2026-08-15T17:09Z
      REF: DETAILS.md#t-mac-30005-the-chain-store-8-gate-has-never-run-against-real-input | DEPS: — | NOTE: runtime-bench-gatewin (tools/runtime-bench.py --assert-gate-wins, CMakeLists.txt:8054) is a documented PERMANENT 77 predicated on vendor/plb being absent (tests/must-run.txt:113), so the 8% chain-store assertion has NEVER been evaluated in CI or on a stock checkout. On a box with vendor/plb provisioned host-local (untracked), the cell runs and the gate reads +0.0% — chain-store on 5.301G vs off 5.302G instructions retired, i.e. the optimization does not fire on spectral-norm. Found during the T-lin-10042 full-native-suite run at b3da6a4a; orthogonal to the MSL arm. FIRST SLICE = characterisation not a fix: establish whether the 8% was ever real for this kernel (bisect / or never), then choose fix-the-gate / re-target GATE_WINS / retire the assertion. [S] not [X]: any UNIX non-emulated build with vendor/plb reproduces identically
- [ ] T-win-50019 [S] — `slice/fault`: the device fault/timeout recovery contract fails all seven assertions on real hardware (RTX 2060)
      OWNER: — | STATE: OPEN | SHA: b57019f9 | TS: 2026-08-15T14:25Z
      REF: DETAILS.md#t-lin-10092-win-requote-b-2026-08-15-15-of-9406 | DEPS: — | NOTE: first-ever real-hardware run of suite_fault (the cell was a no-Vulkan stub on this box until the SDK landed). The contract — timed-out dispatch reports failure, strands exactly one dispatch, device marked unusable, no reuse of pending memory — was authored against lavapipe and the RTX 2060 matches none of it (suite_fault:4156-4185). gpu-lifecycle owners (T-lin-10033 territory); win-x64 reproduces on demand with the VK_LOADER_LAYERS_DISABLE caveat
- [ ] T-win-50005 [X] win-x64 — arm64-win32 COFF: add the AArch64 TLS relocations to `coff_emit_reloc`, then flip arm64 default `-c` to COFF + re-bank o0-baseline arm64-win32 + switch `arm64pe_diff.py` to force ELF
      OWNER: — | STATE: OPEN | SHA: bc0bc6bf | TS: 2026-08-15T14:15Z
      REF: DETAILS.md#t-lin-10083-win-x64-flip-default-c-to | DEPS: — | NOTE: found doing T-lin-10083. arm64-win32 was deliberately LEFT on ELF-default because `coff_emit_reloc` (src/objfmt/mccpe.c:2119) lacks the AArch64 TLS relocs — `tests/exec/features_c99_c11/tls.c` → "unsupported relocation type 549" (R_AARCH64_TLSLE_*), 296/297 corpus objects. Add the TLS arm (map to IMAGE_REL_ARM64_SECREL* per the local-exec model), confirm 297/297, THEN extend the `#if defined MCC_TARGET_X86_64 || defined MCC_TARGET_I386` gate in libmcc.c mcc_set_output_type to include arm64, re-bank, and change arm64pe_diff.py's PE arm to pass `-Wl,-oformat=pe-arm64` (its ELF-vs-ELF premise breaks once arm64 -c defaults to COFF). win-x64 has no arm64-Windows linker (lld-link 22 CAN link arm64 COFF, but nothing here RUNS an arm64 PE). CORRECTION (win-x64, deeper look): reloc 549 = R_AARCH64_TLSLE_* (ELF local-exec TLS). Windows ARM64 TLS is a DIFFERENT model (TEB + _tls_index, not the ELF thread-pointer), so mcc's arm64 codegen emits ELF-model TP-relative sequences that are WRONG for Windows even if the relocs are encoded — this is Windows-ARM64-TLS *codegen*, not just a coff_emit_reloc entry, and is unverifiable (runtime) without an arm64 Windows executor. Likely gated behind the woa CI work (T-lin-10086). Scope realistically: explicit-COFF non-TLS arm64 is already fine; do the default flip only once TLS codegen + an executor exist
- [ ] T-win-50006 [X] win-x64 — arm-win32 COFF: implement the ARM32 arm of `coff_emit_reloc` (there is none), then flip arm-win32 default + re-bank
      OWNER: — | STATE: OPEN | SHA: bc0bc6bf | TS: 2026-08-15T14:15Z
      REF: DETAILS.md#t-lin-10083-win-x64-flip-default-c-to | DEPS: — | NOTE: found doing T-lin-10083. `coff_emit_reloc` has NO `MCC_TARGET_ARM` case — it falls to `return -1`, so arm-win32 `-c` COFF fails on any file with a relocation (only 3/40 reloc-free files compile; "unsupported relocation type 2" = R_ARM_ABS32). LOW PRIORITY: arm-win32 (ARM32 Windows) is a dead platform with no executor (see T-lin-10086 split) — do this only after arm64 (T-win-50005). Add the ARM32 IMAGE_REL_ARM_* mapping (ADDR32/BRANCH24/etc.), confirm the corpus re-encodes, then flip + re-bank
- [ ] T-lin-10057 [S] Make `kept_coverage` host-stable, so the floor is tool-enforced instead of a convention
      OWNER: — | STATE: OPEN | SHA: 8c9d4c34 | TS: 2026-08-15T11:00Z
      REF: DETAILS.md#q-lin-10007-answer-make-kept-coverage-host-stable | DEPS: — | Q: Q-lin-10007 ANSWERED | NOTE: UNBLOCKED. Human chose host-stable — explicitly NOT raising `--tol` and NOT encoding "bank from stage2" as a convention. DoD: the same tree measured from a gcc host and from a stage2 self-hosted compiler yields the same kept_coverage within the EXISTING --tol of 0.05pp, without widening it. Today they disagree: fallback 98 / kept 82.7770 (gcc host) vs 100 / 82.7139 (stage2) at -O0 — a 0.06pp spread outside tol, so banking from a gcc host re-breaks CI, which tests the stage2 tree. Once it holds, "bank from stage2" stops being a rule
- [ ] T-lin-10058 [S] `node-census`: auto-detect available hardware at runtime and run CPU/JIT/GPU
      OWNER: — | STATE: OPEN | SHA: 8c9d4c34 | TS: 2026-08-15T11:00Z
      REF: DETAILS.md#q-lin-10010-answer-node-census-auto-detects-available-hardware | DEPS: — | Q: Q-lin-10010 ANSWERED | NOTE: UNBLOCKED and RE-SCOPED — human rejected both offered options (gate-as-is / report-only). node-census must make an honest effort to run on all available hardware: auto-detect at runtime, ungate the CPU/GPU paths so they run whenever the hardware is present. ONLY permitted overrides are explicit --jit-always-cpu / --jit-always-gpu. Failure mode to refuse: detection finds no GPU and the census reports a CPU-only figure that reads as full coverage — it must say so instead. all_invokes_on_cpu drifts with the corpus (94.9385% -> 94.8004% purely because src/mcc.c amalgamated ~2700 lines) and is not a regression signal; the external-only ceiling 99.2540% remains the headline. Runs through the T-lin-10082 seam
- [ ] T-lin-10040 [S] The device dispatcher is not merely absent — it is unwritable from what exists
      OWNER: — | STATE: OPEN | SHA: 8c9d4c34 | TS: 2026-08-15T11:00Z
      REF: DETAILS.md#q-lin-10008-answer-the-device-path-freeze-is-lifted | DEPS: — | Q: Q-lin-10008 ANSWERED | NOTE: UNBLOCKED — the 2026-08-09 device-path freeze is lifted, all six frozen rows are schedulable. SCHEDULABLE IS NOT JUSTIFIED: the break-even table that motivated the freeze is not repealed and still prices this lever negative (three subsystems, priced nowhere), and float-in-the-emitter already demonstrated the shape empirically — it landed for `double` and moved the device-executable fraction by ~0.0 iteration-weighted points. Whoever takes this either re-prices the lever or states that the value is something other than the device-executable fraction. Re-ranks with T-lin-10033..10038
- [ ] T-lin-10045 [S] `-fopt-slice`: revise into the governor over every AST/RIR slice-capable strategy, integrated with the other slice optimizers
      OWNER: — | STATE: OPEN | SHA: 3749f816 | TS: 2026-08-15T02:30Z
      REF: DETAILS.md#q-lin-10006-answer-fopt-slice-is-the-governor-not-a-pass | DEPS: — | Q: Q-lin-10006 ANSWERED | NOTE: not "own or delete" — it was never a pass. Carries forward: the disk-cache determinism defect, and OPT_SLICE at MCC_OPTD_LEVEL(9) leaves opt-cache-determinism a permanent 77 with no subject. First slice = a shipped level with the determinism claim gated
- [ ] T-lin-10086 [S] `arm64-win32` execution on a `windows-11-arm` CI runner (was [X] win-x64)
      OWNER: — | STATE: OPEN | SHA: 3cf6e238 | TS: 2026-08-15T02:10Z
      REF: DETAILS.md#q-lin-10013-answer-ci-is-the-woa-executor | DEPS: T-lin-10365[S] | NOTE: Q-lin-10013 ANSWERED — CI is the executor, so this is no longer win-x64-only. SPLIT: the `arm-win32` (ARM32) half has NO executor — Windows 11 on ARM64 does not run ARM32 apps — and must not be reported green with the arm64 half
- [ ] T-win-50003 [S] win-x64 full native suite — 35 real failures triaged (28 GPU-slice/`slicerun` device↔CPU differentials + "0 slices on Windows"; 4 fp under emitsize/emitiso opt-search; 3 jit/runtime)
      OWNER: — | STATE: OPEN | SHA: 50790209 | TS: 2026-08-15T12:40Z
      REF: DETAILS.md#t-win-50003-win-x64-full-native-suite-35-real-failures-triaged | DEPS: — | NOTE: RETRIAGED at 50790209 (see DETAILS#t-win-50008-resolved-the-crash-was-setvbuf-not-the-intern-table): the whole "0 slices on Windows" symptom class was one setvbuf fast-fail (T-win-50008, FIXED) — 10 of Bucket A's 28 now pass. Residual: 11 smoke/* = smokerun system() quoting (T-win-50009); slice/src + 6 GPU-cell skips are blocked on the device being INVISIBLE post-reboot (vkEnumeratePhysicalDevices ndev=0, RTX 2060 + vulkan-1.dll present — environmental, needs investigation; the real device-numerics half of Bucket A is HIDDEN behind it, not fixed); Bucket B (4 fp opt-search + 3 jit/runtime) untouched; the embed-JIT half now has a named symbol — the cl-built JIT engine blob references `__report_rangecheckfailure`/`__security_cookie`/`__GSHandlerCheck`/`__isa_available` + `__imp_*` ucrt imports that mcc's in-process linker does not provide (seen in every smoke `--embed-jit` arm at 723e5f1a)
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
  - [x] T-lin-10030/lin [P] The embed JIT is measured only on x86_64 — lin-x64
        OWNER: lin-x64 | STATE: DONE | SHA: 741f7650 | TS: 2026-08-15T15:35Z
        REF: DETAILS.md#t-lin-10030-lin-the-embed-jit-measured-natively-on-x86-64-at-every-level | DEPS: — | NOTE: DONE, BOTH halves of the parent's verification, literally. jit/ family 70 of 70 with ZERO skipped — the default build's 4 skips are config flags (-DMCC_DEV=ON x3, -DMCC_BUILD_STATIC_LIB=ON x1), not missing capability, and all four pass once configured. Conformance --surface embed over 493 qualified oracle programs at EVERY level: -O0/-O1/-O2/-O3 171 correct, -O4 172, and 0 DIFFER at every one. NOT_BAKED 321 of 493 is the JIT declining the program, not a disagreement, so the honest headline is 0 differ over 171 adjudicated. known-positive wired (hot PASS / cold NOT_BAKED); coverage 493 of 800 cross-adjudicable. Parent stays open until /win
  - [ ] T-lin-10030/win [P] The embed JIT is measured only on x86_64 — win-x64
        OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-15T15:45Z
        REF: DETAILS.md#t-lin-10030-the-embed-jit-is-measured-only | DEPS: — | NOTE: corpus prerequisite FULLY MET 2026-08-15 (all four cref corpora provisioned + green on win, DETAILS.md#win-corpus-provisioning-complete-all-four-cref-corpora-live); only the embed-JIT link (T-win-50003 Bucket B) still blocks the measurement half
- [ ] T-lin-10033 [S] The Vulkan dispatch destroys resources under a still-pending command buffer
      OWNER: — | STATE: OPEN | SHA: 8c9d4c34 | TS: 2026-08-15T11:00Z
      REF: DETAILS.md#t-lin-10033-the-vulkan-dispatch-destroys-resources-under | DEPS: — | Q: Q-lin-10008 ANSWERED | NOTE: re-ranked — the 2026-08-09 device-path freeze that de-ranked this row is lifted (never BLOCKED, only de-ranked). Schedulable, not justified: the break-even table still prices the device lever negative
- [ ] T-lin-10037 [S] The emitter's constant cache binds before module size
      OWNER: — | STATE: OPEN | SHA: 8c9d4c34 | TS: 2026-08-15T11:00Z
      REF: DETAILS.md#t-lin-10037-the-emitters-constant-cache-binds-before | DEPS: — | Q: Q-lin-10008 ANSWERED | NOTE: re-ranked — the 2026-08-09 device-path freeze that de-ranked this row is lifted (never BLOCKED, only de-ranked). Schedulable, not justified: the break-even table still prices the device lever negative
- [ ] T-lin-10038 [S] No tree-recursion exec golden exists, and the failure mode is a GPU hang
      OWNER: — | STATE: OPEN | SHA: 8c9d4c34 | TS: 2026-08-15T11:00Z
      REF: DETAILS.md#t-lin-10038-no-tree-recursion-exec-golden-exists | DEPS: — | Q: Q-lin-10008 ANSWERED | NOTE: re-ranked — the 2026-08-09 device-path freeze that de-ranked this row is lifted (never BLOCKED, only de-ranked). Schedulable, not justified: the break-even table still prices the device lever negative
- [ ] T-lin-10043 [S] `tests/emitmap/bank.json`'s tolerances cannot fail, and the selfhost cell is already drifted inside them
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10043-testsemitmapbankjsons-tolerances-cannot-fail | DEPS: T-lin-10003[C]
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
- [ ] T-lin-10061 [S] No runtime fp64 denormal reading has ever been taken from lavapipe
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10061-no-runtime-fp64-denormal-reading-has | DEPS: —
- [ ] T-lin-10062 [S] `MCC_RIR_STAMP` is off by default, so 39,640 of 39,643 `Binary` nodes read back untyped
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10062-mcc-rir-stamp-is-off-by | DEPS: —
- [ ] T-lin-10068 [S] A stage-2 build dir does not rebuild when a header changes
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10068-a-stage-2-build-dir-does | DEPS: — | NOTE: INVESTIGATED (win-x64 via WSL, NOT claimed) — the stated fix ("make mcc emit a depfile CMAKE_DEPFILE_FLAGS_C can consume") is a dead end: mcc ALREADY emits a correct gcc-style depfile (-MD -MT -MF, source+header lines), but CMake IDs mcc as `TinyCC` and its Ninja generator emits ZERO depfile rules for that id — CMAKE_DEPFILE_FLAGS_C/CMAKE_C_DEPFILE_FORMAT are ignored. FOUR bindings falsified (mid-CMakeLists set, -D cache, Compiler/TinyCC-C.cmake on MODULE_PATH, CMAKE_USER_MAKE_RULES_OVERRIDE). Real fix is CMake-side + invasive: force a depfile-supported compiler id (GNU-like) or a custom compile rule. Full diagnosis + next directions: DETAILS.md#t-lin-10068-a-stage-2-build-dir-does (Investigation 2026-08-15). Tree unchanged
- [ ] T-lin-10069 [S] Fifteen "written as live, actually superseded" citations need re-checking or striking
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10069-fifteen-written-as-live-actually-superseded | DEPS: —
- [ ] T-lin-10074 [S] `slice/quiesce` is structurally flaky and the device lock is built, priced and off
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10074-slicequiesce-is-structurally-flaky-and-the | DEPS: —
- [ ] T-lin-10076 [S] N7 residue — an independent tree-side oracle for the slice evaluator
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#t-lin-10076-n7-residue-an-independent-tree-side | DEPS: —
- [ ] T-lin-10077 [S] N37 — the refs-disagree class is computed per digest, so a category hides its own points
      OWNER: — | STATE: OPEN | SHA: 1695806f | TS: 2026-08-14T12:40Z
      REF: DETAILS.md#q-lin-10012-answer-adopt-divdc3-style-complex-division | DEPS: — | Q: Q-lin-10012 ANSWERED | NOTE: human said ADOPT `__divdc3`-style complex division (not the bank-the-divergence hold). Replace mcc's current finite-case complex divide — the three 53%-relative-error finite quotients are a QoI defect even though Annex G does not specify finite accuracy and mcc's G.5.1 infinity/NaN claim is honoured. Then re-bank every `csweep` complex row once (C64/C80.CDIV + CDIVSEL, 283 refs-agree points each, 44 finite)
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
        OWNER: win-x64 | STATE: IN_PROGRESS | SHA: b57019f9 | TS: 2026-08-15T19:01Z | NOTE-4: FOURTH REQUOTE DONE 2026-08-15T19 (mingw-blob build, -j4, VK layers disabled): 15 reds / ~9400. NOT below 15 — the embed-JIT LINK fix (T-win-50020/d46e9ee2) flipped only jit/replay-parity; the engine cells (smoke/engines×3, mcctest-embedjit) still red because the PE hot-swap is gated off (now T-win-50021). Current 15 attributed (all to OPEN tasks — NONE are regressions from the mingw-blob change, verified by rerun): slice/fault(T-win-50019); slice/cost = FLAKY, PASSES standalone (load-induced in the -j4 run, like the flagsweep timeouts — not a genuine red, so really 14); slice/cref-oracle(T-lin-10359); smoke/native + smoke/native-known-positive... wait native-kp PASSES; smoke/native + smoke/strats-known-positive both fail on TWO things: msstruct (T-win-50015 — so msstruct is NOT green, correcting the earlier note) AND `jit-not-baked:signature=180` (T-win-50021 — a NEW UNBANKED bail category my T-win-50020 link-fix EXPOSED: bails-x86_64-windows.txt:47-48 left the jit- scope empty "because embed-JIT cannot link on Windows yet"; it links now but doesn't swap, so the jit arm runs and honestly reports not-baked → needs the jit- scope banked, a follow-on to T-win-50021); smoke/engines(+kp+identity) = jit no-swap(T-win-50021); 4× exec-search-emit{size,iso}/{floating_point,math_library}; mcctest-embedjit(T-win-50021 strtold); runtime-bench-check(vendor/plb=T-mac-30005). GPU device visible this run (RTX 2060, dispatches>0). NOTE-3 (SUPERSEDED — it wrongly said the embed reds were paid). | NOTE-2: THIRD QUOTE 2026-08-15: 15/9406 with the c-torture corpus LIVE in-suite (35 -> 17 -> 15 today) - DETAILS.md#t-lin-10092-win-requote-c-2026-08-15-15-of-9406-with-the-corpus-live. libtest-extra fixed; 13 steady Bucket-B/embed-JIT+msstruct reds + slice/fault (T-win-50019) + the cref-oracle in-suite stall = T-lin-10359 corroborated on the RTX 2060 (passes standalone). Skips 2032 = llvm-test-suite corpora only
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

_Empty — the T-lin-10384 macho-red rescope was resolved by T-lin-10387 (DONE 2026-08-15T20:02Z, archived); no active invalidations._


