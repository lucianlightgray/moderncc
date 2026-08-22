# Log: win-x64 — narrative checkpoints (append-only)

Moved out of `TODO.md` In-progress zone per INSTRUCTIONS.md rev-2 §4.1. Older
per-session checkpoints (pre-2026-08-21T20:00Z) live in git history on the
`## In progress — win-x64` TODO zone; the durable facts are consolidated here.

## 2026-08-22T12:57Z -- *-win32 o0-baseline keys rebanked for lin's divrem_fuse.c (T-lin-10509 win obligations discharged)

Pulled to c59f1e63e; INSTRUCTIONS.md now rev-2 (GOAL.md P1>P2>P3 mission priority, `[D]` device type + DEVICE-BLOCKED/PARKED, per-session `sessions/`+`log/` files, KNOWN_RED registry). No active win claims; Invalidations empty; QUESTIONS Q-lin-10481 (WoA runner) still open. Per §11 the takeable win work was the OWED follow-through on lin's T-lin-10509 DivRemPairs landing (b3d117f3): the native-PE verify was already recorded confirmed (`ctest -R divrem` = 26/26, DETAILS#t-lin-10509-verify-correction), but the four `*-win32` o0-baseline cross keys were stale — lin rebanked native x86_64 (93002348, ungated pass only) + mac arm64-osx (fa32afeab); mac's handoff flagged "win owes *-win32 keys (weak_undef+divrem_fuse)".

Rebanked from a fresh WSL2 x86_64-linux cross-build (`build-o0ab`, MCC_CROSS_TARGETS=x86_64-win32;i386-win32;arm64-win32;arm-win32) at HEAD. Ungated + gated (`MCC_DEV=1 O0_AB_GATES=1`) passes both CHECK exit 0. Committed **d1022b09c** (13 files): all 4 `*-win32` obj+rir+gated.rir (divrem_fuse.c added, files 325→326 fn+4 faithful+4; weak_undef.c COFF hash refreshed) PLUS `x86_64.gated.rir.txt` (lin's 93002348 ran the ungated pass only, leaving the native gated counters stale on the same corpus add). Native x86_64 obj/rir verified byte-identical to lin's bank (host-independence holds). So `ast/o0-baseline{,-gated}` now green on any MCC_ENABLE_CROSS Linux run.

MEASUREMENT LESSON (banked): `wsl -d Ubuntu bash -lc '… ; echo $?'` — the login shell RESETS `$?` before the inline echo, so every inline exit-code read false-reads 0. Capture the wrapper's exit from the OUTER (git-bash) shell instead, and use `bash -c` (non-login) + ABSOLUTE paths (the login shell also mis-reports PWD after `cd ~/moderncc`). This briefly made o0_ab's CHECK look like a vacuous gate (T-lin-10043 class) — it is NOT: single-key + `measurable` both exit 1 on real drift (verified via outer-shell capture). N10 held: verified before minting a bogus finding.

T-lin-10509 win obligations now fully discharged. No active win claims; re-polling §11.

## 2026-08-22T11:53Z -- T-win-50060 DONE+ARCHIVED (2 [S] fixes) + device residual split to T-win-50061 [D]

The cref-oracle-gcc-c-torture-execute win failure was THREE independent bugs, not one. Fixed the two [S] ones + root-caused the third as device-gated. (1) mcc -O1 SEGV on pr123625-3.c FIXED (4823e7f36, mccast.c: 128B GNU vector -> synthetic alloca -> AST-replay fallback left cg_func_alloca stale -> PE gfunc_epilog:1109 gsym_addr OOB; save+restore cg_func_alloca on the !keep fallback + guard nofb-keep on saved==0). (2) clang-oracle -lm harness vacuity FIXED (cc74f4449, tools/gpuconform.py: drop -lm for clang-on-nt -- MSVC target has no m.lib -> clang nocompiled all 1694 -> qualified=0). After both, the full DoD run went rich+clean: qualified=1463, cref-tuples=1.2M, cref-oracle ok=1662 mismatch=0 (0 miscompiles), GPU dispatches=194259. (3) SOLE remaining failure = slicerun "did not finish" on pr123625-3.c ONLY under the full-parallel run with the RTX 2060 active (GPU-hidden full run has no stall) = device-gated GPU-dispatch concurrency -> T-win-50061 [D] CAP:gpu-vulkan + cell quarantined KNOWN_RED(windows, delist-on-green). Pinpointed the SEGV via a clang+ASan build of mcc (lldb unusable -- missing python311.dll; no cdb). lin blessed the shared mccast.c fallback fix; mac-arm64 running cross-target no-regression ^exec on both Mach-O legs. Full diagnosis: DETAILS#t-win-50060-winpe-slicerun-pr123625 + #t-win-50061-slicerun-gpu-dispatch-stall. Next ID -> 50062. PENDING for next: lin's T-lin-10509 divrem native-PE verify (optfire/divrem + exec/divrem_fuse).

## 2026-08-22T10:39Z — T-win-50060 IN_PROGRESS: mcc -O1 SEGV on large-GNU-vector synthetic-alloca (ROOT-CAUSED + FIXED, verifying)

The cref-oracle floor on pr123625-3.c is NOT a slicerun hang (premise misdiagnosis): `mcc -c` SEGVs (0xC0000005) at -O1..-Os compiling it; -O0 clean. Minimal repro `V a=b; b=g2;` with a 128-byte GNU vector. clang+ASan build pinpointed gfunc_epilog:1109 `gsym_addr(func_alloca,...)` walking a stale patch head OOB. Cause: the 128B vector emits synthetic `alloca`, and the -O1 AST replay (ast_func_end) re-emits the body without resetting cg_func_alloca; the unfaithful replay falls back but the fallback (mccast.c:22418) never restored cg_func_alloca → epilog walks the replay's stale head into the restored orig bytes → SEGV. Same latent hazard as T-win-50058/50059 but on the DEFAULT path via CODEGEN-synthetic allocas (the `ast_body_uses_func_alloca` AST-scan gate misses them). Fix: 3-line mccast.c — save parse-pass cg_func_alloca, restore it in the `!keep` fallback, and add `&& saved_func_alloca==0` to the nofb-keep guard. Conservative crash-close (never changes what's kept → can't hit the -Os/-O2 keep-path regression); NOT the T-win-50059 optimization. Verified: pr123625-3.c compiles+RUNS rc=0 at -O0..-O3, exec.*(vla|alloca) 458/458; full ^exec + cref-oracle confirming before commit. Full diagnosis: DETAILS#t-win-50060-winpe-slicerun-pr123625.

## HANDOFF-BOX-FACTS (kept for successors)

**BUILD/ENV:** MSVC+Ninja in `cmake-release/`, wrap build/ctest in `cmd /c '"…\vcvars64.bat" >nul 2>&1 && <cmd>'`. Main target `mcc`; CLI driver `cli_runner` (a NEW `cases.h` case name needs `cmake -S . -B cmake-release` reconfigure). embed-jit blob auto-built with winlibs-ucrt mingw (`MCC_EMBED_JIT_MINGW_CC`). STALE-BLOB: after a `src/libmcc.c|mccjit_embed.c|blob-config` change `rm cmake-*/{mccjit_engine_mingw.o,libmccjitengine-mingw.a,mccjit_blob.c}`; after a `runtime/win32/include` header change `rm cmake-*/lib/*.o cmake-*/lib/libmccrt.a` (mccrt custom cmds don't track header deps) + note the build uses STAGED headers in `cmake-*/include`. Corpora junctions `vendor/ -> C:/Users/llg/Projects/{gcc-torture,llvm-test-suite,llvm-project}`; `vendor/gcc-c-torture-execute` = 1694 execute tests. VULKAN_SDK + `VK_LOADER_LAYERS_DISABLE=VK_LAYER_AMD_switchable_graphics` for device runs. **Two concurrent ctest invocations COLLIDE on the `mcc_build` fixture (both rebuild+lock mcc.exe) → all fail "Failed test dependencies: mcc_build"; run suites in ONE ctest invocation.** Run PE exit codes via PowerShell `Start-Process -Wait -PassThru` (git-bash rc=127 is a launch artifact). commit BEFORE `git pull --rebase`; grep `'^(<<<<<<<|=======|>>>>>>>)' docs/*.md` before push. CRT: win-PE is SINGLE-CRT UCRT (T-win-50021). SendMessage to peers reachable via Remote Control (ListAgents lists mac-arm64/lin-x64).

**o0-baseline cross-rebank:** `C2_NO_EXTRA=1 O0_AB_BANK=1 sh tools/o0_ab.sh cmake-cross <key>` per-key rebanks one *-win32 obj+rir board (NOT `measurable`); CHECK = `O0_AB_CHECK=1`. Cross-link a PE for exec-verify with JOINED absolute -B (space `-B path` eats the path). i386 cross-link blocked (no 32-bit ucrt staged). exec_runner: `cmake-release\exec_runner.exe <mcc> cmake-release runtime\include tests <work> --only <goldenname>` rc=0=pass; goldens in tests/exec/goldens.h. **o0-baseline is a UNIX-only gate (never runs on win)** → prefer o0-neutral (diagnostic/driver/preprocessor/header/PE-guarded) win changes; rely on lin/mac to catch native codegen drift + rebank.

**WIN ORACLE = cl (MSVC), not gcc-mingw, for bit-field/ABI:** mcc-on-PE deliberately matches the MSVC ABI where C leaves it impl-defined (extended-type bit-field promotion → declared type, 8d4f0a80; MS-bitfield layout). A gcc-c-torture -O2 differential vs gcc-mingw FALSE-POSITIVES on these — the exec goldens encode them via `expect_win32` arms. See DETAILS#t-win-50046-bitfield-promote-64bit-base (WITHDRAWN find).

## 2026-08-21T20:01Z — session summary (/goal loop)

**T-win-50045 (-fno-common) DONE+ARCHIVED (code 3d06e503, archive ef3f2954).** Gated set_elf_sym's bss-merge (mccelf.c:649) on `!nocommon` so mcc's DEFAULT (already -fno-common) multi-defs two tentative `int g;` across TUs like gcc; -fcommon still coalesces. Left branch 647 (real-overrides-tentative) unconditional (gating it regressed mcctest-embedjit `_tls_index defined twice`). o0-neutral. Verified win: cli 469/469, exec 8225/8225, selfhost+diff3+treegate 341/341, mcctest+embedjit. **mac then found+fixed the mirror case on arm64-osx (19df84a58: incoming-tentative meeting existing-real, single-TU asm-alias); I confirmed the symmetric fix clean on win-PE (exec 8225/8225).** lin confirmed ELF (exec 374/374).

**Fresh win-PE gcc-c-torture -O2 differential (1694 tests):** 1477 ok, 11 candidates — 8 mac-known (FORTIFY/QoI/int128). 3 fresh: (a) bf-sign-2 → **T-win-50046 WITHDRAWN** — NOT a bug, intended cl-conformance 8d4f0a80; implemented a combine_types "fix", the full-exec gate reddened integer_promotion+bitfields_ms (expect_win32 arms), reverted. (b) pr23324 → **T-win-50047** real Win64 struct-ABI SEGV, isolated to caller_bf6 (struct et6 by-value + empty-union return; SEGV inside callee member reads = bad struct-arg pointer, likely sret/by-ref register interaction); resists minimization (simplified structs pass) → focused debugger/disasm session. (c) pr36321 → **T-win-50048** alloca(0) spacing impl-defined, LOW.

**Migrated to INSTRUCTIONS.md rev-2 per-session files.** No active win claims. RESTART POINTERS: P1 T-lin-10478/win (native-PE optimizer -run coverage) opens when lin lands T-lin-10476[C]; game-off GPU window → T-win-50019/50003; i386 toolchain → T-mac-30019 part-2; arm64-WoA CI → T-win-50041; the 8 parked win [S] tasks (T-win-50026, T-mac-30059/30097/30039/30065/30058/30223/30211) are low-residual, claimable.

## 2026-08-21T22:33Z — T-lin-10477 optfire phantom-coverage cells DONE

Claimed + landed T-lin-10477 (P1/JIT, the T-lin-10476 audit's prerequisite). A
subagent produced a source-verified firing-signal map (mcc `--stats` counters are
AST-strategy-only; switch-lowering/block-layout/remat are backend with no counter),
and my own PE probes confirmed it. KEY: four of the six audited passes are simply
UNIMPLEMENTED in mcc, so no "real exercising cell" is possible — the honest fix is
accurate reclassification, not a fabricated cell.

- Slice 1 (2c171311, test-only, o0-neutral): id10 covered|jt,sccp; id22 partial|jt
  (jt = local branch-fold, not correlated threading); id49 partial|unroll (only a
  const-trip loop is removed, via unroll; the prior partial|dse note was factually
  wrong); id75 base (no BB reorder; __builtin_expect is code-neutral); id77 partial
  (data merge/.bss, not LLVM materialization hoist). id78 remat already honest (base).
- Slice 2 (8934f442 cell + 1c09170c flag): id79 switch-jumptable is the ONE audited
  pass that genuinely exists (env/opt-search-gated `gcase_jumptable`). Exposed it as
  a first-class `-fswitch-jumptable` knob (MCC_OPT_SWITCH_JUMPTABLE, MCC_OPTD_SPECIAL
  whose default reproduces the exact prior env/opt-search gate) so default codegen is
  byte-identical -> o0- AND shipped-On-neutral, no rebank needed. New run-verified
  differ cell `switch_jt`. NB `-ftree-switch-conversion` is a red herring (drives the
  `bf` bitflag strat, not switch tables). cover3.txt regen also fixed PRE-EXISTING
  staleness (T-lin-10469's unroll-loops flag was never regenerated into the array).

VERIFY (win): optfire-x86_64/switch_jt PASS via mcc -run on native PE (real ctest
cell after reconfigure with MCC_TEST_SH); default==-fno byte-identical; env compat +
-fno-override proven; cli 474/474, exec 374/374, smoke/mcctest/treegate/gate 23/23,
optfire-x86_64 48/48; cover3 verify + coverage-check green. Full-suite requote not
re-run (proven-neutral change; GPU/device/selfhost cells orthogonal).

COORDINATION: mac-arm64 caught + fixed a pre-existing regstub-lint red in my
T-lin-10478/win block (8cfb8751, glob-based per-row skip-stubs) so my new differ rows
auto-cover on every host; I reconfigured + confirmed the PE side clean (48/48) and
ack'd. The switch_jt differ row will auto-register the ELF/Mach-O -run legs on lin/mac
via the differs glob.

TOOLCHAIN NOTE (successors): MCC_TEST_SH for the win optfire-x86_64 cells = a git-bash
sh, e.g. the scoop git sh; coverage-check.sh + cover3.py both run locally on win via
that sh / python. GOTCHA: `git commit -m @'...'@` here-strings mangle multi-line/
apostrophe messages under PowerShell 5.1 and an autostash on a same-command
`pull --rebase` can split a commit — use `git commit -F <file>` and pull/push as
separate commands.


## 2026-08-22T02:15Z — T-lin-10480 part 4 (win-PE typecov -run leg, P1) + T-win-50053 cross-build regression fix

/goal loop. Pulled lin's coop-M:N fiber-park rework (3177c67a, T-lin-10525) — mcc unaffected (runtime header only); smoked cli/coop_mn_win32_multiworker GREEN on native PE (lin's request; the Win32-Fibers path just worked).

**T-lin-10480 part (4) DONE (7f9a33cf, DETAILS#t-lin-10480-typecov-x86_64-pe).** Picked by GOAL P1 + §11: win's optfire differ+counter -run legs already close P1(a+b) for optfire; the ast typecov family (cse/dse/licm/pre) was the last native-PE P1 leg. Added an if(WIN32) block registering ast/{cse,dse,licm,pre}-typecov-x86_64-pe, reusing the shared scripts + the MCC_TYPECOV_RUN env (asserts -O0-run == -O<hi>-run == AOT-O0) mac built. MEASURED native PE: cse=6/dse=2/ltemp=4/pre=4, each -run==AOT-O0; `_noi128` subjects (PE has no __int128); no -B/-I needed (build-dir mcc.exe resolves its runtime). 4/4 green, cli 477/477, o0-neutral test-registration. T-lin-10480 re-PARKED (parts 2/3 shared/lin-domain remain).

**T-win-50053 cross-build regression FIXED (2d9c6399, DETAILS#t-win-50053-cross-build-fix).** While running mcc_cross_build during the typecov verify, found the loop-idiom pass I built in T-win-50053 broke the cross build: `ast_loopidiom_apply` (all-targets) calls `ast_promo_size_unknown`, defined only inside the `#if X86_64||ARM64||RISCV64` guard (mccast.c:4455) → LNK2019 building mcc-arm on arm32/i386. Broken on ALL fleets since ca2199e2 — the loop-idiom slices verified only native x86_64-PE. Fix = relocate the tiny arch-independent helper out of the guard (o0-neutral static-fn move). Verified: mcc_cross_build GREEN, ast/rir-parity-x86_64-win32 GREEN (cross mcc healthy), typecov-x86_64-pe 4/4, cli 477/477. FYI'd lin+mac (2d9c6399): pull before any cross build. T-win-50053 re-OPENED (feature slices init≠0 / restrict-ptr memcpy / parity-wiring remain). LESSON: an unguarded function calling into an arch-`#if` region is a cross-build-only link break native verification misses — run mcc_cross_build when adding such calls.

**Continued (02:35Z):** T-lin-10480 part (2) first cell landed -- `ast/sroa-typecov` (3f4921e9, DETAILS#t-lin-10480-sroa-typecov). Probed which part-2 passes fire on win-PE with a type dimension (sroa/tco/ivsr/range fire; select needs an if-conv shape; sccp counter is condition-driven; narrow is LLP64-inert on PE); SROA had the cleanest type-breadth story and is already type-general. Subject dominated by non-int-member structs (anti-vacuity floor sroa>=16 vs int-only ~4); AST-level count is arch-independent so one threshold holds on all 64-bit native legs. Registered lin(base)+mac(both legs)+win-PE(-run); win 5/5 typecov-x86_64-pe green; FYI'd lin/mac to confirm their legs on pull. T-lin-10480 re-PARKED.
