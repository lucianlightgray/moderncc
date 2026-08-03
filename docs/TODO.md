# TODO

Cut the AST recorder and the operation journal out and leave Replay_IR as the compiler's only intermediate representation. **Cut to Replay_IR** at the end of this file is the staged plan; P0 through P4 have landed and `MCC_RIR_PROD` now defaults to **1**, so Replay_IR is the production arena — read P4 before touching the arena, it is where the three wrong-code classes are written down. **`MCC_RIR_ONLY` now defaults to 1 as well**: the recorder's per-body decline verdict was the arena's admission gate, and with it bypassed the arena is adopted on its own pre-flight alone. That widened the optimized population by 6.3% of bodies; the eight defects the widening exposed are closed, the three `optfire` cells that died with the gates they measure are deleted, and the tree is green at **8252/8252**. **The next step is the deletion itself** — read P5, which is now a pure deletion that moves nothing by construction. Everything above P5 is the C2 work, which the plan no longer blocks on — the per-body fallback decision means a body Replay_IR cannot re-emit keeps the parser's bytes rather than blocking the deletion.

Two bars, both required. **Replay** (`rir_verify`) replays a captured body against the parser's own bytes. **C2** re-emits from the reconstructed arena and compares — the harder bar, and the one still open. Replay is at `faithful + empty == fn` on all twelve target keys at `-O0`/`-O1`/`-O2`/`-O3`, gated by the 48 `ast/rir-parity-*` cells.

**The corpus is now all of `tests/`, not `tests/exec`.** 657 files against 277, and about twice the bodies per key — 1980–2521 against 1146–1318. `tests/exec` was never a wrong measurement, but it was a narrow one: it reads a 14-body gap where the whole tree reads 148, and it says `c2bytes=0` everywhere where the whole tree finds byte divergences on ten of twelve keys. `C2_CORPUS=exec` still produces the historical board for continuity; `C2_CORPUS=all` is the default and is the bar.

## Handoff — state at this checkpoint

The tree is green: `ctest -j 8` **8252/8252** with no env set, on a short checkout path with nothing else running.

### Where the cut stands

| phase | state |
| --- | --- |
| P0 harness, P1 journal verify-half cut, P2 decouple + admission predicate, P3 capture rebrand | landed |
| P4 cutover | landed; **`MCC_RIR_PROD` defaults to 1**, arena drives **91.5%** of bodies (`used=2011 fallback=11 skip=176` of 2198, x86_64 `-O2`, whole corpus) |
| P5 switch | **landed. `MCC_RIR_ONLY` defaults to 1**; the three `optfire` cells that died with it are deleted and the suite is 8252/8252 |
| P5 deletion | **done.** The hooks, the state, all 14 gates, `MCC_RIR_ONLY`, `MCC_AST_VERIFY` and 22 dead cells are gone. Nothing named `ast_hook_`, `ast_bail`, `ast_desync`, `ast_vn` or `ast_try_active` survives under `src/`. Tree green at 8231/8231 |
| `fix-imaginary` | branch ready, merges clean, **unblocked** — the three `ast-verify-ratchet` cells are deleted |
| P6 split + `ast_*` → `ir_*` | not started |

### Next steps, in order

1. **Land `fix-imaginary`** — `ast-verify-ratchet-{O1,O2,O3}` are deleted, which were the only three cells blocking it. It merges clean and needs no rebase. **This is unblocked now.**
2. **P6**: split `mccast.c` and rename `ast_*` → `ir_*`. The `ir_`/`IR_` namespace is verified empty.
3. Then the items under **Open work raised after the cutover** — the poison-over-deletion change, the `ast_cycle_env` convergence, the pointer test, glibc headers, and `MCC_TRACE` on `full_language.c`.

### What the deletion did

**Done, and gated.** `-1,618` lines of `src/mccast.c` (76 hook definitions plus their 64 forward declarations), all **124** call sites in `src/mccgen.c` with the 26 `#if MCC_CONFIG_OPTIMIZER` blocks they left empty, all **76** declarations in `src/mccast.h`, and **12 of the 14** recorder-shape gates, which measurement confirmed had exactly two references each — a declaration and an assignment, no reader. **The six keepers are kept**: `ast_label_id`, `ast_label_forget`, `ast_sv_hi`, `ast_cmp_invert_late`, `ast_bad_type`, `ast_func_has_asm`. Verified mechanically — the set of top-level definitions lost is *exactly* the `ast_hook_*` set, nothing else.

Twenty cells died with it and are deleted: `ast-verify-ratchet-{O1,O2,O3}` with `tests/ast/verify_ratchet.cmake` and all four `tests/ast/verify-baseline/` files; the **fourteen** `ast/rir-c3-*` cells with `tests/ast/rir_c3.cmake` and the `rir-c3` custom target — P4 predicted these die with the recorder and they did, all fourteen, in one run; `ast/treecheck` and `ast/tracediff` with their two scripts; and `optfire/default-nocode_call`, a fifteenth recorder-shape gate cell the inventory had not listed. `MCC_AST_INT128=1` is out of `rir_parity.cmake`'s forced-`-O0` env, exactly as **Keep the measurement honest** said to do it — *when* the gate dies, not before. 8252 → **8232** registered cells, and `ctest -j 8` is **8232 of 8232, zero failures**.

Gates met: the twelve-key object A/B reads **19,425 compared, 12 differ** — the single `-O3` `scopes.c` widening described above and nothing else, unchanged before and after the gate removal, which is what proves those 12 gates were readerless. `C2_NO_EXTRA=1 O0_AB_CHECK=1 tools/o0_ab.sh` passes against the bank on all twelve keys with `arm-win32 == arm-wince`. `tracegate`/`schemagate`/`targetgate` clean.

**The state went too, and the recorder is now gone entirely.** `ast_bail`, `ast_desync`, `AST_SET_BAIL`/`AST_SET_DESYNC`, `ast_gap_note` and the whole gap-note block, the shadow vstack (`ast_vs`, `ast_vn`, `ast_cf_if`/`ast_cf_savebb`/`ast_cf_top`), `ast_capture`, `ast_in_op` (with its ten increment/decrement sites in `src/mccgen.c` and the lone `ast_bail = 1` in `va_arg`), `ast_in_call`, the ternary and land/or stacks, `ast_ret_val`, `ast_last_return`, `ast_saw_nocode`/`ast_saw_chain`, `ast_call_pending`/`ast_inc_pending`/`ast_vdup_pending`/`ast_opassign_store_pending` — all deleted. `ast_replay_ok` is now literally "the arena has a body". `ast_try_active` is gone and `rir_try_active`, which `src/mccrir.c` has owned since P2, gates `ast_func_begin` and `ast_func_end` directly; `ast_ret_bad`, `ast_bad_vtype` and `ast_wide_vtype` went with it, and with them the last two recorder gates, `MCC_AST_INT128` and `MCC_AST_LDOUBLE`. `ast_verify_env`, `MCC_AST_VERIFY`, `MCC_AST_VERIFY_OUT` and both `[ast-verify]` verdict blocks are gone. `tests/fuzz/runner.c`'s `GATES[]` lost its eight dead rows and the `RECORDER_2026_07_31` combination that consisted entirely of them; every `MCC_AST_*` the fuzzer still names is verified to exist in `src/mccast.c`.

**`MCC_RIR_ONLY` is gone and `MCC_RIR_PROD` is now report-only.** `ast_func_end` adopts the arena unconditionally: `rir_prod_env` is `ast_replay_env && !rir_env`, with no gate and no `ast_verify_env` term. `MCC_RIR_PROD` survives *only* as the verbosity level the measurement recipes in this file are written against — unset or `0` is silent, `2` still prints `[rir-prod] <verdict>` per body and the `[rir-prod-total]` atexit line. **It can no longer turn production off**, which is the point: there is nothing left to fall back to.

**Two more cells died with the off-switch, and the inventory had no row for them.** `ast/rir-inert` and `ast/journal-inert-build`, with `tests/ast/journal_inert.cmake` and the `mcc_nojrn` target they existed to drive. Both proved the capture substrate is *byte-inert* by compiling the corpus twice under `MCC_RIR_PROD=0` and comparing. That property is now vacuous and the cells are unsatisfiable by construction: with capture as the sole producer, a compiler built `MCC_IR_CAPTURE=0` has no arena at all, so it keeps the parser's bytes while `mcc` re-emits from the arena — the two legs *must* differ. P4 saw the shape of this coming ("with the cutover on, `mcc` and `mcc_nojrn` legitimately differ because one has a different producer") and re-pointed the cell rather than retiring it; the off-switch's removal is what finally made it unanswerable. 8232 → **8230** cells; upstream has since added one back, so the tree reads 8231.

### The `MCC_RIR_ONLY` flip, as measured

One line: `ast_env_gate("MCC_RIR_ONLY", 0)` → `1` at `src/mccast.c:2040`. Note the switch lives in **`mccast.c`**, not `mccrir.c` as an earlier revision of this file guessed.

- **Object A/B with the switch off is byte-identical**: `cmake-p5base` (pre-flip, nothing set) against the flipped binary under `MCC_RIR_ONLY=0`, over `find tests -name '*.c'` × twelve keys × `-O1`/`-O2`/`-O3`, `SOURCE_DATE_EPOCH` pinned — **19,425 of 19,425 identical, 0 differ**, with `arm-win32 ≡ arm-wince` on every row.
- **`ctest` before the flip: 8255/8255.** After the flip: 8251/8255, and the four are the three `optfire` cells plus the root-owned-leftovers artifact below.
- **The three `optfire` cells failed for exactly the recorded reason**, read off the cell output rather than predicted: `optfire/opassign` and `optfire-arm64/opassign` both report *"pass DID NOT FIRE (MCC_AST_OPASSIGN=0 and MCC_AST_OPASSIGN=1 objects are byte-identical at -O2)"*, and `optfire/default-landor_invert` reports *"MCC_AST_LANDOR_INVERT is default-off at -O2, expected default-on"*. Both gates' only readers are inside the hook block (`src/mccast.c:2524` in `ast_hook_cmp_invert`, `:2653` in `ast_hook_vdup`), so with the arena as sole producer they correctly stop changing output. The cells are deleted — one line each from `tests/optfire/differs.txt` and `defstate.txt`, plus their two `arch.txt` scoping entries. 8255 → 8252 registered cells, and 8252/8252 green.
- **The flip was verified by counters, not by one object.** `MCC_RIR_PROD=2` on `tests/exec/programs/grep.c` reads `used=14 fallback=0 skip=0` with the switch defaulted on against `used=10 fallback=0 skip=4` under `MCC_RIR_ONLY=0` — the four bodies the recorder declines are exactly the widening.
- Four side configurations green in distinct build dirs; `tracegate`/`schemagate`/`targetgate` clean; `src/mccrir.c` still has no `MCC_TRACE`.

**A trap this flip re-paid for, and the file already warned about it.** `riscv64-promote-docker` leaves **root-owned** files under `<build>/riscv64-promote-docker/corpus/` on every *successful* run, so the *next* run fails in 0.00 s with a permission error. It failed that way three times here and each time read as a real regression. `sudo rm -rf` the directory and re-run the cell alone before believing it; it passed in 33 s every time from clean. The failure is a property of the previous run, not of any change.

### Measurement setup, and the traps that cost time

- Build for measurement: `cmake -S . -B bc2 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMCC_ENABLE_CROSS=ON -DCMAKE_C_FLAGS=-DMCC_REPLAY_IR_C2=1`.
- **`vendor/` is gitignored but a *symlink* named `vendor` is not matched by `/vendor/`.** A git worktree has no sysroots, so four ELF cross keys compile ~77 files of 276 and still print a green board. Symlink it, verify `ls vendor/gentoo-stage3-riscv64-glibc/usr/include`, and **never `git add -A`**. The same applies to a `cmake-cross` symlink, which is what makes the docker-gated cells actually run instead of SKIP.
- **A long checkout path fails 22 `exec*/bound_global`** — `bt_info.file` is `char[100]`. Not a regression. Configure through a short symlink (`/tmp/mccw`) to avoid it.
- **Root-owned leftovers under `<build>/riscv64-promote-docker/`** fail that cell in 0.00 s with a permission error and read as a real failure. Clear them before believing it.
- **`superopt/promote-floor` is load-sensitive and will read as a regression.** It compiles at `-O4`, which is the time-budgeted size-scored search, and asserts the search does not subtract `MCC_AST_PROMOTE`. Under `ctest -j 8` it failed once in 1.12 s and passed the next full run in 0.61 s, and it passes standalone every time. A slower run explores fewer candidates and can miss the promoted variant. Re-run it alone before believing it, exactly like the `selfhost-*` contention class below.
- **Do not run a twelve-key sweep and `ctest -j 8` concurrently** — the contention alone fails several `selfhost-*` cells.
- **Pin `SOURCE_DATE_EPOCH`** for any object A/B, or a file differs from its own second compile.
- **`ast_env_int` returns its default for values `<= 0`**, so `MCC_RIR_PROD=0` means *on*; unset it instead. `ast_env_gate` handles `0` correctly.
- **`cp -a` of a build dir is not a reference build** — `CTestTestfile.cmake` bakes the binary dir absolutely and the copy silently runs the original binaries.
- Verifying a switch by compiling one file and diffing the object proves nothing: only 69 of 562 files differ at `-O2` on x86_64. Use a file already known to differ, or the `[rir-prod-total]` counters at `MCC_RIR_PROD=2`.

### The Windows stage2 reds after the merge, closed

The three failure groups on every Windows stage2 job (`pe`, `sanitize`, `dynamic`) at the P5 merge were two defects, both closed:

- **`selfhost-*` (all eight) and `o4-aot-jit` Part 1: `STATUS_STACK_OVERFLOW` (0xC00000FD) in any mcc-linked mcc compiling `src/mcc.c` at `-O1`+.** The recursion is `ast_replay_bb`'s else-arm — one frame per `else if` link — and it predates P5; what P5 changed is that the arena now admits the wide population, so the long option-chain bodies replay. The amplifier is self-hosted codegen: mcc gives `ast_replay_bb` a **27,664-byte frame** (`mov $0x6c10,%eax; call __chkstk`), so ~37 levels exhaust the **1MB** default PE stack reserve. gcc-linked stage-zeros survive on mingw's 2MB reserve and small frames, which is why the tree read green everywhere but a stage2. The fix raises `pe_template`'s `SizeOfStackReserve` to **0x00800000** for all PE targets — the arm64 arm of that `#if` had already chosen 8MB, so this deletes the `#if` rather than adding one. Reserve is address space, not commit; `-stack=` still overrides. Debugging note that cost an hour: both vendored and scoop gdb **crash (0xC0000409) loading mcc's PE DWARF** — `objcopy --strip-debug` on a `-g` build leaves the COFF symtab, gdb then runs and backtraces against it, and `addr2line` on the un-stripped twin names the statics.
- **`exec-replay-promote/c11_complex_convert`: the fconst reuse list, third act** — see the new kind-tag bullet under **What closed**. Win32-only because the win64 ABI passes 16-byte `_Complex` by reference, which is the shape that materializes the constant temp.

### The three expected `optfire` failures

`optfire/opassign`, `optfire-arm64/opassign`, `optfire/default-landor_invert`. `optfire` probes *which* optimizations fire; both gates are recorder-shape gates with no reader outside the hook block, so they correctly stop changing output. **They are resolved by the deletion, not by a code change** — do not attempt to fix them.

### Method that worked, for whoever picks this up

Every phase was gated on a number measured before and after, and the phases that moved nothing said so with a byte-identical object A/B over the whole corpus × twelve keys × three `-O` levels. Three separate agents reported a defect as "pre-existing" that measurement showed was theirs, and two predictions written into this file were falsified by the next measurement. **Re-measure rather than carrying a row forward** — this file says so in several places and it earned every one of them.

## Scoreboard

Per key at HEAD, produced by `tools/c2_sweep.sh <builddir> <key> <opt>` against an mcc built `-DMCC_REPLAY_IR_C2=1` under `-DMCC_ENABLE_CROSS=ON`, run in place. One twelve-key corpus takes about 12 minutes; re-measure rather than copying a row forward.

Read `ok=` on every row before believing it. `rir_report` is an **atexit** handler, so a file that fails to compile still prints `[rir-total]` and still contributes bodies — counting `[rir-total]` lines is not an honesty check. The sweep counts only files that exited 0 and prints that count.

`C2_CORPUS=all`, 657 files, `-O1`. `gap` is `c2try - c2ok`. **The `arenahasheq` column is frozen at the reading below and cannot be re-measured** — P4 deleted the counter and the `tools/c2_sweep.sh` column with it, deliberately, because it scored the arena against the recorder's tree rather than against the parser. It is kept here only so the classes it named still resolve:

| key | c2ok/c2try | gap | classes | arenahasheq (retired) | ok |
| --- | --- | --- | --- | --- | --- |
| x86_64 | 1944/1948 | 4 | 4 len | 1414/1988 | 522 |
| x86_64-win32 | 2306/2317 | 11 | 2 bytes + 7 len + 2 err | 1616/2364 | 527 |
| arm64 | 2114/2124 | 10 | 1 byte + 8 len + 1 invalid | 1506/2164 | 543 |
| i386 | 1937/1947 | 10 | 10 len | 1384/1987 | 519 |
| arm | 2052/2064 | 12 | 2 bytes + 9 len + 1 invalid | 1442/2104 | 541 |
| arm64-win32 | 2331/2346 | 15 | 2 bytes + 11 len + 2 err | 1637/2392 | 522 |
| x86_64-osx | 2449/2462 | 13 | 3 bytes + 8 len + 2 err | 1656/2508 | 571 |
| i386-win32 | 2310/2327 | 17 | 2 bytes + 13 len + 2 err | 1593/2378 | 526 |
| arm-win32 / arm-wince | 2270/2287 each | 17 | 3 bytes + 12 len + 2 err | 1575/2334 | 519 |
| riscv64 | 2035/2048 | 13 | 2 bytes + 9 len + 2 invalid, plus 7 skip | 1463/2094 | 539 |
| arm64-osx | 2458/2477 | 19 | 4 bytes + 13 len + 2 err | 1679/2521 | 566 |

**The table above predates P4 and its `ok` column no longer matches the tree** (it reads 522 files on x86_64 where the same sweep now reads 562), so do not diff a fresh row against it. The `all` board as re-measured at the lost-intermediate commit, `C2_CORPUS=all C2_NO_EXTRA=1`, `-O1`, twelve keys, before → after, as `gap` over `c2try`:

| key | before | after | c2try |
| --- | --- | --- | --- |
| x86_64 | 7 | 7 | 2125 |
| x86_64-osx | 4 | 4 | 1873 |
| x86_64-win32 | 4 | 4 | 2058 |
| i386 | 13 | **11** | 2124 |
| i386-win32 | 9 | **8** | 2076 |
| arm | 13 | **12** | 2102 |
| arm-win32 / arm-wince | 9 each | **8** each | 2053 |
| arm64 | 11 | 11 | 2162 |
| arm64-osx | 9 | 9 | 2146 |
| arm64-win32 | 7 | 7 | 2108 |
| riscv64 | 15 | 15 | 2096 (plus 7 skip, 1 invalid) — 2086 before the `ASM_REGVAR_ASMREG` fix brought a 552nd file in |

110 → 104. `faithful` is identical on all twelve keys either side, which is the check that matters: a capture-stream perturbation shows up there first.

`C2_CORPUS=exec` for continuity with every earlier board in this file, `-O1`, gap only: x86_64 **0**, x86_64-osx **0**, x86_64-win32 **0**, i386 **0**, arm64 1, arm64-osx 1, arm64-win32 1, i386-win32 1, arm 1, arm-win32 1, arm-wince 1, riscv64 3 (plus 2 skip). Ten over twelve keys, down from 16 when the lost-intermediate class closed.

**Forced `-O0` tracks `-O1` to within three bodies on every key** (`C2_FORCE=1 C2_CORPUS=all … -O0`): x86_64 7, x86_64-osx 18, x86_64-win32 15, arm64 10, arm64-osx 21, arm64-win32 16, i386 9, i386-win32 18, arm 11, arm-win32 18, arm-wince 18, riscv64 13. `-O2` and `-O3` were byte-identical to `-O1` on every counter when last measured on five keys. C2 replays the arena's own emission and the optimizer passes are a separate question (C3), so this is what one would expect; it means the completion bar's "at every `-O`" is close to one measurement, but the -O0 column is now cheap enough to keep printing and it is NOT identical, so print it.

`arm-win32` and `arm-wince` share a define set and must read identically. Any sweep where those two rows differ has a harness bug, not a codegen one — the cheapest available check that a run measured what it thinks.

## Close the C2 gap

**The one rule this repo has paid for six times: fix at the USE site, never at the capture site.** A change where a primitive *consumes* an operand — the wrap `JOP_GENOP` performs, the `JOP_GV` wrap, the `JOP_VSTORE` and `JOP_STORE` arms, the argcast loop — is bound to the right node by the op's own recorded snapshot, and every such change has landed. A change where a cast or a lowering *happens* — a mark at `gen_cast`, a region bracketing `gen_negf` — perturbs the very stream `rir_verify` replays, and every such change failed. Read the banked negatives before designing another attempt.

Instruments, in the order they pay: `RIRDUMP=1` for ops either side of the blamed index with byte windows; `-DRIR_DBG_OPTRACE=1` plus `RIRDBG=<funcname>` for the `[ent]` entry stream (which prints each entry's `nocode` — that is how the dead-code class was found), `[stmt]` naming which entry pushed each BasicBlock statement, `[vst]`/`[gop]` for the vstore and genop admission inputs; `MCC_REPLAY_IR=6` for `[rir-dump]`/`[rir-diff]` node pairs against the tree. A structural `[rir-dump]` diff against the tree found nearly everything that closed.

`RIRC2TREE=1` drives C2 from the tree's arena instead. A body that diverges identically in both legs is a **shared replay** defect in `ast_replay_bb`/`ast_replay_value`, not a model defect; it still blocks the bar, but it is different work and it benefits both models. Caveat: `RIRC2TREE` falls back to the RIR arena when `ast_replay_ok(ast_cur)` is false, so re-confirm any individual body before attacking it as a replay defect.

### What closed, and the shape that worked

- **The inline-asm operand class.** `asm_instr` evaluates the operands, then `save_regs(0)`, then two `asm_gen_code` calls; only the last three were journalled, and `JOP_ASMGEN` replays off the parser's own vstack snapshot so its bytes were always right. The operand evaluation — pointer gv, deref, spills — was orphaned shadow-stack nodes nothing emitted. A `RIR_M_ASMOPS` mark taken immediately before `save_regs(0)` collects them into one `AST_OP_ASMOPS` statement whose replay calls `save_regs` itself. Closed `fancy_copy`, `fancy_copy2`, `sigaddset1`, `sigdelset1`, `memcpy1`, `memcpy2`, `mconstraint_test`, `other_constraints_test`.
- **The dead-code jump class.** `rir_op_effect` drops ops under `nocode_wanted`, but a break's arena `Jump` comes from a mark and a switch's dispatch jump comes from replaying the `AST_If` — neither is an op. `CODE_OFF_BIT` lives outside `RIR_NOEVAL_MASK`, so the mark filter let both through. Closed `optimize_out_test`.
- **A narrowing cast on an lvalue, at the `gv`.** `(int)zeros_c[1]` where `zeros_c` is `long` makes the parser's gv load 32 bits; the arena's untyped Load still named the long, so replay loaded 64, added 64 and truncated after. The gv's snapshot is where the narrowed type survives. Same wrap as the line below, widened to `VT_INT`.
- **`!cmp` left the arena's operator meaning the opposite of what the source wrote — wrong code, found by the fuzzer.** `if (!(a > a)) arr[5] = a;` dropped the store at `-O1`/`-O2`/`-O3` and `if (!(a == a))` executed one that must not run; `MCC_RIR_PROD=0` was correct, so this was live only because P4 flipped the default. `!` on a comparison emits nothing — the parser swaps `jtrue`/`jfalse` and flips `cmp_op` — and `RIR_M_CMPINV`'s `inflags` arm modelled that as `AST_FB_CMP_INVERT_LATE` **instead of** flipping the recorded token, which the cmp-invert commit chose deliberately: on arm64 a comparison materialises through CSINC, so the token flip replays `cset eq`+`cbnz` where the parser wrote `cset ne`+`cbz` — same length, same meaning, different bytes. Correct for emission, and it left the node saying `>` where the program says `<=`. `ast_ident_node`'s self-comparison fold reads that operator as a value, folded to 0, and `ast_sccp_scan` discarded the live arm. **The fix separates the two questions instead of choosing between them**: the operator always moves to the inverted one, and the flag now says only that replay must re-run the comparison the other way round and swap the flags, which `ast_replay_value` does by emitting `gen_op(bop ^ 1)` under the flag. Bytes are unchanged by construction — the same two acts in the same order — and every pass reading the operator sees the program's own sense. Both capture sites move together, `ast_hook_cmp_invert` and the mark. Closes seeds 240739, 240981 and 241595 of the nightly campaign at all four `-O` levels on x86_64; 257/257 `ast/*`, `ast-*` and `optfire/*` on arm64. `tests/fuzz/corpus/repro_cmp_invert_self.c` carries all three directions including the float case the flag was written for.
- **`gv` on a sub-int lvalue.** `gv` on a char or short lvalue emits the widening load itself, and both the placement (inside a ternary arm) and the signedness (`(unsigned char)` leaves no op) were missing from the arena. Wrapping the operand in a `Convert` at the `JOP_GV` site closed `mt_workload.c::main`, `rev64_mt.c::main` on x86_64 and `corpus.c::str_hash`.
- **A label's identity in the arena.** `rir_hook_label` and `rir_hook_goto` were handed the label's *token*, and two `__label__ l1` in different statement-expression scopes are the same token, so the arena's labels aliased where the parser's `label_find` scoping keeps them apart -- `ast_rp_label_floor` scopes only the inline-graft path. A monotonic id per label Sym, forgotten when `label_pop` frees it, fixes the identity; a bare Sym pointer does not, because `label_pop` frees at every scope exit and the allocator recycles. Two call sites needed reading rather than replacing: the goto arm reassigns `s` to a per-goto cleanup thunk before the hook runs, and `rir_hook_cleanup_thunk` records the same label from `g->cleanup_label->v`, which has to move to the id too or the thunk and the goto stop agreeing.
- **A discarded call's statement position.** A call whose value is discarded becomes an arena statement only when the shadow stack drops it, and inside a statement expression the parser's `vpop` lands AFTER the following goto's and label's marks. `rir_flush_effect_top` takes an unparented effectful `AST_Invoke` off the shadow stack at those two marks; it must BLANK the entry, because `rir_drop` re-`rir_stmt`s any effectful node it pops without checking for a parent and a node added to one BasicBlock twice reads as "nchild disagrees with sibling chain". Neither this nor the label id closes anything alone; together they closed `local_label_test`.
- **A compound assignment as a store-chain source.** `ast_finalize_chainstores` declined if either store was an `AST_OP_OPASSIGN`; only the second has to be a plain assignment. Shared-replay, so it moved both models. Closed `fuzz_next` and 39 divergences in `tests/fuzz/runner.c` on the x86_64 keys.
- The **code-free cast on a 32-bit target**, the same class **at a call argument**, and riscv64's **`gen_negf` spill** — all documented at length in earlier revisions of this file, all closed by widening the wrap a primitive already performs on its own operands.
- **The lost-intermediate class, width half: the 64-bit cast lowering on a 32-bit target.** `(long long)(int)(v >> 3)` with `v` a `unsigned long long` lowers, on every `MCC_PTR_SIZE == 4` key, entirely inside the `RIR_R_SYNTH` region `gen_cast` opens at `src/mccgen.c:4547` — the `(int)` is `lexpand(); vpop();` and the `(long long)` is `gv_dup(); vpushi(31); gen_op(TOK_SAR); lbuild()`. Neither leaves a conversion op, so the arena stored the raw 64-bit `Binary >>` and replay skipped the sign-extend-from-32. The fix is the **region's own value**: `rir_hook_castsynth_end(type, ds, ss)` closes that region with `rir_rend_to_val(RIR_R_SYNTH, type->t)` whenever the block actually did the 64-bit work (`ds == 8`, or `ss == 8 && ds >= 4`), and `rir_region`'s `RIR_R_SYNTH` rend wraps the shadow top in a `Convert` of that type. Closed `overflow_narrow.c::main` on all five 32-bit keys and `integer_promotion.c::main` on i386 — six divergences on both corpora, none anywhere else, `faithful` unmoved on all twelve keys.
- **The lost-intermediate class, const-fold half: a code-free float cast over an operand the parser already folded.** `(double)(3 + 4) / 2` folds to `3.5` in the parser with no op at all; the arena kept `Binary /` over an int `Binary +` and an int `Literal 2`. The `JOP_GENOP` operand wrap already had the right snapshot — it was skipping every float destination outright (`is_float(st)`). Narrowed that skip to "float destination **and** the operand is not a constant subtree the parser folded", which is exactly the code-free case: a non-constant int operand of a float op always leaves a `cvt_itof`, and a genuinely-float operand already carries a float type so the existing `rir_child_has_type` guard stops the wrap. **This closes P4's wrong-code defect 2** — `tests/exec/expressions/cast_operator.c` prints `avg: 7` under `MCC_RIR_PROD=1` where it printed `6`. It moves **no** C2 counter on either corpus, which is the point: the byte compare validates un-optimized emission and cannot see `ast_run_templates` folding `7 / 2` as an integer divide off a type the arena never had.

- **The op-assign vdup, second half: a dereferenced lvalue target, not just a member.** `*(unsigned *)(data + r) += a - b` (`ptr_longlong_arith32.c::main`) reached the arena as `Store(Load(Convert(Binary + data r)), Binary +(Ref data, ...))` where the tree has the target's own `Load(Convert(Binary + …))` in the RHS's left slot. `rir_reconcile_sv` had refilled the vdup slot from the SValue, which by then named `data`, so replay dereferenced `data` and the promoted register held 4. The `IR_OP_VPUSHV` `RIR_M_OPASSIGN` arm was already the right site — it just would not fire, because `rir_lvalue_shape` accepted only `AST_Unary MEMBER`/`MEMBER_ARROW`. It now also accepts an `AST_Load` over a pure address, and `rir_addr_pure` learned `AST_Binary` over `+ - * & | ^ << >> >>>` (no divide, which can trap). The bitfield/array/VLA exclusion the earlier half paid for moved to the top of the function so it covers both shapes. **Closes `ptr_longlong_arith32` in all 13 optimizer suites and `array_2d_iv` in `exec-ivsrptr`** (`g2[i][j] += i + j` is the same shape). Both `exec` and `all` C2 boards are byte-identical on all twelve keys — the class is invisible to the byte compare, exactly like the const-fold half.
- **The fconst reuse list, third act: a recorded constant whose replay never asks.** `gen_complex_cast`'s const path materialises the 16-byte complex into rodata and records it (`src/mccgen.c:6351`), but the arena captures the *result* as a `VT_CONST|VT_SYM` ref, so the promote re-emission pushes the ref and never re-runs the cast — the entry sits unconsumed at the head and every scalar reuse after it is off by one. On x86_64-win32, `check_arg(4.0, …)` then loads `im` from the first half of the complex literal (4.0 for 0.0) and `re` from the wrong slot entirely; checks 11–22 of `c11_complex_convert` all fail, promote-only. The RIR-side consumer survives the same stream because `rir_hook_fconst_reuse` resyncs **by position**; the ast-side list is sequential and cannot. The fix tags each entry with the site that recorded it — scalar `gv` (`:2437`) or complex cast — and the sequential consumer skips a kind mismatch at the head: record order is emission order, so a mismatched head entry is one whose consumer already passed or will never ask. The discriminator is exact: complex is `VT_STRUCT` + `ref->a.is_complex` and never `is_float`, so the two sites cannot see each other's constants.
- **A dead-code float constant desynchronised the whole reuse list.** `ast_fconst_record` appended `vtop->sym->c` unconditionally, but a constant materialised under `NODATA_WANTED` gets `size = 0`, so `put_extern_sym` assigns it nothing and `c` is recorded as **0**. `ast_fconst_reuse` treats a returned 0 as "no reuse", so such an entry can never be consumed — it only shifts every later index by one. In `flt_eval_method.c::main` the parser records 30 entries of which 4 are these zeros; the replay makes 26 calls and never asks for the dead ones, so from index 23 on every reuse is off by two and `ast_fconst_push_ref` stamps the *caller's* type onto the *previous* constant's symbol. The visible damage was a `movd` (4 bytes) against the 8-byte `+inf` double, i.e. `isnanf(0.0f)`, and `ok` went to 0. `ast_fconst_record` now skips `c == 0`. This is a **shared-replay** defect, not a model defect: it fixes the recorder too, and `ast-verify-ratchet-{O1,O2,O3}` went 136 gaps to 135 with `features_c99_c11/flt_eval_method.c main unfaithful` becoming faithful — `tests/ast/verify-baseline/x86_64-linux.txt` re-banked. **Closes `flt_eval_method` in `exec-replay-promote`.**

### The class that only Replay_IR can reach

**A body the recorder declines is a body whose shared-replay defects have never been executed.** `flt_eval_method.c::main` was `[ast-verify] unfaithful` on the tree, so the parser's bytes were restored and `AST_PF_EMIT` never ran — the `ast_fconst[]` desync was there the whole time and cost nothing. Replay_IR is `rfaithful` on the same body, so promotion runs, the arena is re-emitted, and the latent bug becomes wrong code. Expect more of these as Replay_IR's faithful population widens past the recorder's: the tell is that the failing gate is a *pass* gate (`MCC_AST_PROMOTE=1` here) and that the same body reads `unfaithful`/`desync` in `tests/ast/verify-baseline/`. Check that file before blaming the arena. Two consequences worth acting on: the C2 byte compare can never see this class, and the fix belongs in the shared replay where it pays twice.

**Two verify baselines carry an unmeasured prediction.** `tests/ast/verify-baseline/x86_64-darwin.txt` and `x86_64-win32.txt` both list `features_c99_c11/flt_eval_method.c main unfaithful`, the same verdict `x86_64-linux.txt` carried. The `c == 0` skip is target- and host-independent, so both should now read faithful and both need re-banking on their own hosts — they are left alone here because a cross build's gap set does not reproduce a host-native one (measured: `mcc-x86_64-win32` against the mingw-hosted `x86_64-win32.txt` drifts on dozens of unrelated bodies, because the header resolution differs). `arm64-darwin.txt` lists the body as `desync`, a different verdict, and is expected to be unmoved.

### Banked negatives — do not re-pay for these

- **Marking code-free `gen_cast` calls**, three variants: at every such call wrapping the shadow-stack top → `c2ok 531/1146`, `c2invalid=517`, two core dumps; narrowed to a const-only `AST_Binary` outside every region → `c2err=3`, `faithful` down one; bound to the operand the marker's own vstack snapshot names → `faithful` holds but `c2err` 0 to 1-3 on every key and neither target body moves. Stamping the bound node's type instead of wrapping is worse again.
- **Bracketing `gen_negf` as `RIR_R_FNEG`** on the `RIR_R_INC` pattern **aborts the compiler**. Region markers are part of the captured stream.
- **Relaxing the `!lv` guard in `JOP_STORE` generally**: -30 `c2ok` on every key. The float restriction is what makes it safe.
- **Dropping the argcast wrap's `AST_Convert` skip** without the wider test **segfaults** every PE key.
- **Widening `rir_child_width_differs` itself** to the effective width: same `c2ok`, minus 14 to 20 `arenahasheq` on every key.
- **Retyping the cast chain at its conversion op** for `overflow_narrow.c::main`, two variants, both no-op. The `(int)` narrow in `(long long)(int)(v>>3)` emits no op, so there is no op to attach to. The conversion-op route is dead for the whole lost-intermediate class; **the route that worked was the region's own value**, not a field on an op — see the two lost-intermediate entries under "What closed". A region's `rval` is invisible to `rir_verify`, because `rir_run` skips every entry whose `tag != RIR_T_OP`, so putting new data on a region marker that already exists is a USE-site change and not a capture-site one. Adding a *new* region still is not; that negative stands below.
- **Closing the `RIR_R_SYNTH` cast region at its `rbegin` instead of its `rend`**: -14 bodies on every 32-bit key (`narrow_ranged.c` ×10, `popcount_inline.c` ×2, `signbit_inline.c`, `bitfields.c`, `overflow_inline.c`, `struct_packed_indirect.c`), for +5 — i386 gap 2 to 16. At the `rbegin` the recorded vstack still reads the 64-bit *source*, so the `Convert` is correct but premature: `rir_stamp_sv`'s widen loop then sees a node typed 4 bytes against a snapshot that says 8 and wraps it in a *second* `Convert` back to the source width, and replay materialises a high half the parser never emitted (`b9 00 00 00 00` after the call in `narrow_ranged.c::udiv`). `faithful` held in both variants, which is the tell that the failure is reconstruction and not capture. Any future region-value wrap belongs at the end of the region, where the snapshot has already moved.
- **Cloning the op-assign target at the vdup for a bitfield lvalue**: -2 `c2ok` on x86_64 (`bitfields.c::main`, `bitfields_ms.c::main`), a `c2len` divergence both times. The `IR_OP_VPUSHV` clone that fixed `tal_realloc_impl` has to exclude `VT_BITFIELD` (and `VT_ARRAY`/`VT_VLA`) targets and impure address subtrees; with that exclusion the twelve-key board is unmoved on both corpora. **Re-measured while closing `bitfields_ms`, and the cost is still exactly 2** — x86_64 `exec` 1113/1113 → 1111/1113. The byte windows say why, and they say the clone is the wrong shape rather than a near miss. The parser evaluates the bitfield lvalue's address **once** (`mov -0x10(%rbp),%rax`), lets the vdup'd copy spill to a temp inside `gv`'s `save_regs`, and reloads that temp for the read-modify-write; with the clone the trial re-materialises the address into a second register (`mov -0x10(%rbp),%rcx`) and evaluates the *value* first, so the whole sequence is transposed at identical length. Modelling this needs the parser's own spill placement, not a subtree copy. The runtime defect the exclusion left behind is closed by refusing the shape in `rir_prod_take` instead — production-only, so it cannot cost a `c2ok` at all.
- **Dropping every mark recorded with any `nocode` bit set**: -25 bodies. A label, case or default recorded with `CODE_OFF` set is usually the very thing that turns code back on. Narrowing that to `RIR_M_JUMP` only is still -13, because `gjmp_acs` sets `CODE_OFF` on the way out and so every break looks unreachable by the time the hook runs; the hook has to be handed `nocode_wanted` as it stood BEFORE the `gjmp`.
- **Touching `error1`'s opening `mcc_exit_state(s1)`** (`src/libmcc.c:651`), either variant, **deadlocks the JIT self-host**. It is the release half of the `mcc_enter_state` that `MCC_SET_STATE` (`src/mcc.h:2128`) wraps every `mcc_warning`/`mcc_error` in, and `mcc_compile_sem` is a binary semaphore: deleting it strands the take, and adding a matching `mcc_enter_state(s1)` before each of the three returns doubles it. The regime, not the diagnostic, is what was wrong — see P4 defect 3.
- **Widening `ast_sccp_has_label` itself to count `case`/`default` markers**, rather than adding a second predicate for the one site that deletes a subtree. Six call sites, and only `ast_sccp_scan`'s dead arm needs it: `ast_cprop_switch_meet` is handed the switch node whose body always holds case markers, so the widened predicate reads true there on every switch in the tree and disables switch cprop outright; `ast_cprop_stmt`'s loop guard, `ast_licm_at_loop`, `ast_ltemp_run` and `ast_ivsr_run` would each go conservative on any loop containing a `case`. `ast_jt`'s identical-arm merge is the one other site with the same hazard and does not need it: two arms both carrying the same `case` value, or both carrying `default`, is not legal C, so `ast_ident_same` can never hold over a real one.
- **Reading `AST_FB_NOCODE` on every statement kind** rather than only `AST_If`: -25 bodies. Other kinds pack raw values into `fbits` — `ASMGEN` its vstack window, `MEMBER` its `VT_NONLVAL` — and alias any new flag bit. Any future `AST_FB_` read in `ast_replay_bb`'s statement loop must be kind-guarded for the same reason.

### Still open, largest first

Counts are divergences over the twelve keys on the `all` corpus.

- `bounds_stress.c::test16` and `::test17`, **24**, both on all twelve keys: `strcpy(q = alloca(strlen(demo) + 1), demo)` inside a call argument. The trial re-evaluates `strlen(demo) + 1` and calls `alloca` a second time instead of reusing the value. Nested call in an argument, so the same family as `struct_assign_test`.
- `fuzz/runner.c::triage`, `::interesting`, `::main`, **27**, on the nine non-x86_64 keys only — the x86_64 keys closed with the store-chain fix. On arm64-osx the trial emits one extra `ldr x0,[x0]` at a `genop`: an extra dereference the parser did not make.
- `statement_expr_test`, **7**, is what is left of the statement-expression class after `local_label_test` closed. It is now a `c2bytes` divergence at identical length -- pure ordering, not a missing or extra instruction. The two defects that made up the class are described under "what closed"; whatever is left is a third.
- `full_language.c`, **42** over its seven keys, six bodies: `struct_assign_test` (call), `statement_expr_test` (jmp), `s7_9_iso646_test` (store, and a BYTE divergence not a length one), `local_label_test` (call), `coherency_test` (genop), `char_short_test` (cvt_csti). Of these, `struct_assign_test` and `char_short_test` are confirmed RIR-model defects (the tree leg gets them right or differs); the rest reproduce identically in the tree leg and are shared-replay work. `longlong_test` adds 5 more on the keys where it diverges.
- `rev64_mt.c::main`, **12**, still open on all keys — a different and much larger divergence (want 2056 got 2009) from the ternary one that closed in the same file's sibling.
- `ternary_op.c::tst_yarpgen`, **7**: a `JOP_STORE` register spill the parser makes mid-expression and the trial does not. Scheduling and allocation, not model shape — a single yarpgen expression with enough live values to spill, and the trial reaching that point with fewer live registers is the symptom. Expect it last.
- `s7_9_iso646_test`, **12** including `run_s7_9.c`: a pure ORDERING difference at identical length. The parser stores then loads the next condition; the trial hoists the load before the store and picks a different register for it. Same family as `AST_FB_STORE_ADDR_LATE`.
- `zero_bss.c::main` **5**, `smoke.c::main` **5**, `struct_ret_variadic.c::mkv` **3**, `varargs.c::mix` **3**, `strpbrk.c::strpbrk` **2**, `struct_packed_indirect.c::main` **1**, `integer_promotion.c::main` **1** (riscv64 only), `run_s7_28.c::s7_28_wconv` **1**, `run_s_annFGK.c::s_annFGK_annex_test` **1**. `overflow_narrow.c::main` is closed.
- The **lost-intermediate** class is closed for every body it was blamed for. `overflow_narrow.c::main` and i386's `integer_promotion.c::main` closed with the `RIR_R_SYNTH` region value; `cast_operator.c::main` closed with the genop float wrap; both are written up under "What closed". `grep.c::pmatch` does not diverge on any key at HEAD and should not be carried forward as a member. What is left of `integer_promotion.c::main` is riscv64 only and is the `VT_QLONG` representation item, not the width class — the `(unsigned)` widen on riscv64 takes the `trunc = 32` leg with no synth region to hang a value on, and widening the operand wrap for it is not the fix.
- riscv64's 7 `c2skip` and 2 `c2invalid` on the `all` corpus, and its one remaining compiler abort (down from four; the other three were the `asm_parse_regvar` numbering bug).
- **The lost-intermediate class over a pointer**, found by the JIT seam and costing **zero** C2 counters, which is why it had never been seen. `for_each_elem` (`src/mcc.h:1662`) compares `elem` against `(type *)(sec->data + sec->data_offset)`; the cast from `unsigned char *` is code-free, the arena keeps the `unsigned char *`, and replay compares two pointers of different pointee type. Bytes are identical — both operands are the same width and the bodies are `c2ok` — so the only observable is that `combine_types` emits a "pointer type mismatch in comparison" warning the parser did not. It is benign *today* only because nothing downstream reads the pointee; a `+`/`-` or a `[]` on such an operand would scale by the wrong size. The same shape as the two closed lost-intermediate halves, and the same rule applies: the route is a region's own value or an existing primitive's operand wrap, never a mark at the `gen_cast`.

## Raise arena fidelity

- Raise arena fidelity. **The `arenahasheq` counter no longer exists** — P4 retired it; the last reading was 1680 of 2521 on arm64-osx and the Scoreboard's column is frozen at that sweep. Re-instrument before quoting a number, and prefer C2, which scores against the parser. Measure as `arenahasheq` and as diffs-per-body falling, not as first-divergence classes disappearing — fixing divergence #1 only exposes #2. Classify by **structural** dump diff, not by node index: `ast_intention_hash` is a structural walk that hashes neither `type_ref` nor `ival` on a `Ref`, so the large Convert-versus-Ref index-transposition class and every `ref`-only diff are not fidelity defects and cost nothing to ignore. Live classes: field-only, extra-Convert-over-Binary, missing-Convert-over-Literal, tree-extra-Store. The last two are understood and not cheap — the tree builds cast chains where one push record recovers only one link, and `tree-extra-Store` is dead code under `nocode_wanted` that the tree records and the op filter drops.
- The extra-Convert-over-Binary class is **measured, not guessed**, and the measurement says do not touch it. Gating the whole argcast wrap on `RIR_M_ARGCAST`'s per-argument fired flag reads **+25 arenahasheq and -16 c2ok on every key**: the wraps the tree does not make are load-bearing for emission. The flag is therefore consulted only for the untyped-Load case it was added for. Any future attempt to close the class has to explain that trade first.
- Land explicit operand binding as a side-table keyed by op index, not a node field and not positional binding.
- Widen C3 pass equivalence past the field-identical population — the same work as raising arena fidelity, since the paired population *is* the field-identical population. Add the optimized-versus-optimized byte tap by replicating C2's whole save/restore prologue just before the `orig`/`orig_rel` frees in `ast_func_end`, where `ind` is the end of the optimized body: a different point with different live state. Report the coverage difference separately rather than as a mismatch, because the tree runs passes only where `ast_replay_ok` holds while Replay_IR accepts every body.

## Finish the capture path

- Phase F: un-embed `RirOp` from `JrnOp`. This is a design change, not a lift — Replay_IR must own an op stream with its own vstack buffer (the shape `rir_mvs`/`mvs_off`/`mvs_n` already has for marks) instead of reading `jrn_ops[]` and indexing `jrn_vs[]`. `jrn_snap_vstack` falls out of the same change: it has no `mccgen.c` call site to hang on, so it cannot be lifted independently.
- Discharge F against the same obligation every prior slice met: byte-identical `MCC_REPLAY_IR=5` sweep log and identical object sha256 at `-O0`, `-O1`, `-O2`, `-O3` and forced `-O0`. That compares capture against parser, tree and journal at once and is strictly stronger than cross-checking two capture paths against each other. Replace site by site; do not run both paths side by side.

## The TinyCC upstream sync

`tinycc/mob` is fetched as the `tinycc` remote. The merge base is `a338258d` (2026-06-13); `main` descends from tcc linearly and has never carried a merge commit from it.

**A plain `git merge tinycc/mob` is unusable and always will be.** Every file is renamed and relocated (`tccgen.c` → `src/mccgen.c`, `tccelf.c` → `src/objfmt/mccelf.c`, `x86_64-gen.c` → `src/arch/x86_64/x86_64-gen.c`) and every identifier with them, so git's rename detection finds nothing: the trial merge produced **31 modify/delete conflicts** and would have resurrected `tccgen.c` alongside `src/mccgen.c`. Sync by porting semantics per commit, never by merging.

**Synced at `2be0218b` (2026-07-30), 17 commits.** What they were and what happened to each:

| upstream | disposition |
| --- | --- |
| `85ba3ae8` `'n'` operand modifier not negating | **ported.** moderncc had the identical bug — `val = -val` computed then `(int)sv->c.i` printed — in all four sites (`i386-asm.c`, `arm-asm.c`, `riscv64-asm.c` ×2, the last also needing `val == 0` for `'z'`) |
| `2be0218b` arm64 addend on LLP64 | **ported.** `arm64_sym`'s `unsigned long addend` → `addr_t`; `unsigned long` is 32-bit on LLP64 so large addends truncated |
| `31020bf9` quoted `LIBRARY` in `.def` | **ported.** New `get_libname`, heap `dllname`, `.dll` default extension |
| `384614a9` + `43158eaa` ELF boundary symbols | **ported.** `linker_sym` bit on `struct sym_attr`, `set_linker_sym`/`provide_linker_sym`/`update_linker_sym`/`finalize_linker_symbols`, called after `layout_sections` so the final section order decides `_etext`/`_edata`/`_end`. Verified against gcc: all six names agree, and a user definition of `end` correctly wins (PROVIDE semantics) |
| `5673055a` `b895aa7e` `04206d29` `97a6d8c7` `38059770` TLS | **already present, and moderncc is ahead.** See below |
| `a667b53f` `ff85981b` `d9d02c56` `add111e6` win32 self-host with tcc 0.9.27 | not applicable — moderncc does not bootstrap from tcc 0.9.27 |
| `4ed845d0` Makefile doc targets, `.github/workflows` | not applicable — CMake, and a different CI |
| `da58264a` tests2 fixtures | moderncc has its own test tree; the boundary-symbol behaviour is covered by the new `exec/programs/linker_symbols.c` golden, which self-gates so ELF, PE and Mach-O all print the same thing |
| `7f7845cd` review some recent changes | one applicable hunk, deliberately **not** taken — see below |

**The TLS group was already reconciled, item by item, and mostly by moderncc arriving first.** Checked individually rather than assumed: riscv64 carries the addend on *both* halves of the TPREL pair, byte-identical to upstream's final form; `mcc_elf_end_file` already keeps undefined TLS symbols typed `STT_TLS`; i386 local-exec already emits `fc` rather than `0` (different encoding, same effect); PE TLS already exists on i386 and arm64 (`gen_pe_tls_base`, `pe_tls_index_sym`), which is what `38059770` adds upstream. x86_64 float and `long double` TLS were verified behaviourally against gcc. And moderncc is **ahead** on two counts upstream has no equivalent for: riscv64 general-dynamic TLS (`riscv64_tls_gd_a0`) and arm64 dynamic TLS in a shared object (`9f76b124`).

What is left of the TLS commits is *refactor* — "remove stupid copy&paste code", "integrate tls into gen_modrm()", "remove stupid loops" — with no behavioural delta. Upstream moved TLS **into** `gen_modrm`; moderncc handles it at the `load`/`store` sites. Porting that reshaping into backends carrying Replay_IR capture hooks would perturb the captured stream, move objects, invalidate the byte-identity gates, and buy nothing. Not done, and it should not be done later either.

**One real upstream fix was deliberately declined.** `7f7845cd` adds `vpop(), vpushi(0)` to `gen_cast`'s `dbt_bt == VT_VOID` arm, "do not confuse backends with VT_VOID in registers". That is a capture-site change in the single most banked-dangerous function in this tree, and moderncc shows no symptom: a void-cast battery (`(void)(a+b)`, `(void)g()`, `(void)dg()`, `(void)(a?g():b)`, `(void)(void)a`, comma-expression) matches gcc at `-O0`/`-O1`/`-O2`/`-O3` and compiles clean on all eleven cross keys. **Re-measure before taking it**; if a symptom ever appears, the fix belongs at the use site.

**A pre-existing gap this sync found, not caused by it.** A *static* i386 link of a TLS program fails with `Unknown relocation type for got: 16` (`R_386_TLS_GOTIE`). Reproduced identically on a build from before any of this work, so it is not a regression; upstream's commits do not fix it either.

*Gate as met: `ctest -j 8` **8254 of 8254**, zero failures; twelve-key object A/B against the pre-port build **19,557 of 19,557 byte-identical, 0 differ** — the port touches only link-time and asm-operand paths, so no codegen moved; `tools/o0_ab.sh` re-banked because the corpus gained one file, and the rebank is **+12 lines, -0** across the twelve object banks, i.e. exactly one new sha256 per key with every pre-existing hash unchanged; `tracegate`/`schemagate`/`targetgate` clean; four side configurations green.*

## Keep the measurement honest

- Bank the corpus census against header resolution, never against a remembered number. `MCC_CONFIG_AUTO_MCCDIR` resolves mcc's own freestanding headers from **argv[0]'s directory**, so a compiler built into a scratch dir with no sibling `include/` silently loses `stdarg.h`/`stdbool.h`/`stdatomic.h`/`threads.h`; a build omitting `-DMCC_CONFIG_MCCDIR` finds no system headers at all and reads 476; a glibc sysroot resolves `<threads.h>` to glibc's rather than mcc's shim and costs 25 functions. Every one of these leaves `rc`, the file count and the failing-file list unchanged — the census is the only thing that moves, which is what makes it a trap.
- **Done** (`tools:`): `c2_sweep.sh` no longer selects `$BUILD/mcc` for a key the host is not. The native compiler carries no sysroot flags, but which key it IS depends on the host — x86_64 on a Linux host, arm64-osx on a mac — so asking for key `x86_64` on a mac silently measured `arm64-osx` with `rc` 0 and a plausible row. It now prefers an explicit `mcc-<key>` whenever one was built.
- The `all` corpus compiles 519–571 of 657 files per key. The rest are `dg-error` cases, host-specific `darwin/`, arch-specific `arch/` and files that need a driver; they are excluded by the `rc` check, not by a list, so a file that starts compiling joins the census automatically. `full_language.c` needs `-I <repo root> -DCC_NAME=CC_gcc` and enters only on the 7 keys where the C2 probe's own error does not abort the compile — `extra=` on the row says whether it did.
- Keep the C2 harness mirroring the tree's replay prologue exactly across `vstack`/`vtop`, `loc`, `anon_sym`, `ast_pinned_regs`, `ast_rp_bsym`, `ast_rp_csym`, `ast_rp_switch`, `ast_temp_frontier`, `ast_rp_nlabel`, `ast_fconst_i`, `ast_locrec_i`, `sym_free_first`, `ast_rp_asmops`. Leftover allocator state reads as a codegen difference; one omission, a dirty vstack, costs 194 bodies.
- Keep the `-O0` cells honest. `ast_replay_env` needs `optimize >= 1`, so `-O0` without `FORCE` journals nothing and the cell exits 1 on "0 bodies journalled" rather than 77. **`FORCE` is now two explicit env vars, `MCC_RIR_FORCE=1 MCC_AST_INT128=1`**, the first read at `src/mccast.c:2035`, and `SRCDIR` is gone from `rir_parity.cmake` and from all twelve callers; the old form derived the 38 `o4 || optimize >= 1` gate names by regex over `${SRCDIR}/*.c` and would hard-fail the moment one of those gates was deleted. Forced `-O0` legitimately reads 3 bodies fewer than `-O1` on x86_64, arm, arm64, arm64-osx, i386 and riscv64, 2 fewer on x86_64-osx, and 0 fewer on the five PE keys — `grep.c::tolower`, `c11_threads.c::thrd_equal`, `arm64.c::putchar`, all `static inline` shims whose out-of-line copy `-O1` emits and `-O0` does not. That is an emit difference, not a census loss. `tools/c2_sweep.sh`'s own `C2_FORCE=1` still derives the 38 gate names by regex over `src/*.c` and refuses to run if it finds none; that is now the only derivation left in the tree, and it should follow `rir_parity.cmake` to `MCC_RIR_FORCE` rather than be the last thing coupled to gate spellings the recorder's deletion will change.
- **`MCC_AST_INT128=1` is in that env for a measured reason: without it forced `-O0` loses 2 bodies, on x86_64 and x86_64-osx only.** Measured either side of the cut on the whole twelve-key matrix: x86_64 `1146 → 1144` against an `-O1` of 1149, x86_64-osx `1140 → 1138` against 1142, and every other key identical either way (arm64 1186 both ways; the five PE keys unmoved at their `-O1` figure). Both bodies are `tests/exec/types/int128.c`, whose two out-of-line `__int128` bodies exist at `-O0` only when that gate is on — the derived list forced it and `MCC_RIR_FORCE` alone does not. Bisected to that one gate of the 38: `MCC_AST_INT128=1` alone takes that file from `fn=4` to `fn=6`, `MCC_AST_TEMPLATES=1` moves nothing. Forced `-O0` is the coverage baseline every deletion phase is diffed against, and its worth is that it covers the population `-O1` covers minus only the three shims above; narrowing it by 2 on the two keys where C2 already reads 100% would remove coverage exactly where a P1/P4 regression would be most legible. The gate name is a coupling, but a cheap and self-healing one — when the gate dies with the recorder those bodies are captured unconditionally and the env entry becomes a no-op, so **delete the line then, and do not re-derive a gate list either way**.
- The C2 gap is ratcheted per key by `tests/ast/rir_c2.cmake` (`ast/rir-c2-{O1,O2,O3}` and eleven `ast/rir-c2-<triple>`), a banked gap and not a 100% bar. Each cell banks `BANKGAP` (`c2try - c2ok`), `BANKSKIP` and `BANKFN`; the `BANKFN` floor is the honesty half, since a gap ratchet over an unpinned population passes by measuring fewer bodies — with `vendor/` absent the ELF keys read `ok=77 fn=476` and a gap of 1, which is the header-resolution trap above wearing a green tick. The cell SKIPs unless the probe reports `c2try>0`, so a stock build without `-DMCC_REPLAY_IR_C2=1` cannot pass it vacuously, and it counts only files that exited 0. **The cells ratchet the `exec` corpus, not `all`** — deliberately, because a ctest cell has to be seconds and one twelve-key `all` sweep is twelve minutes. `all` is the bar and the sweep measures it; `exec` is the regression tripwire and ctest measures it. Do not conflate the two numbers. Banks as re-measured by rerunning each cell after the lost-intermediate class closed: x86_64 `0/1149`, x86_64-osx `0/1142`, x86_64-win32 `0/1237`, i386 `0/1152`, arm64 `1/1199`, arm64-osx `1/1224`, arm64-win32 `1/1297`, i386-win32 `1/1257`, arm `1/1130`, arm-win32 and arm-wince `1/1235`, riscv64 `3/1156` plus 2 `c2skip` — ten over the twelve keys, down from sixteen. Rebank by rerunning the cell, never by editing the number to match a memory. A gap *below* the bank only prints a STATUS line, so a closed body sits unratcheted until someone rebanks; treat that line as a to-do, not as noise.
- **A new hook call compiles on the host and still breaks every stage2 CI job.** The amalgamated, optimizer-on build hides two whole classes of missing declaration. `mccast.h` declares its hooks behind `MCC_CONFIG_OPTIMIZER && defined(MCC_INTERNAL)`, which is why every `ast_hook_*` call in `mccgen.c` sits inside `#if MCC_CONFIG_OPTIMIZER` — the moment mcc compiles mcc without `-DMCC_CONFIG_OPTIMIZER=1` (selfhost-jit, cross-factory, o4-aot-jit) an unguarded call is an implicit declaration, which mcc treats as a hard error. And with `MCC_SINGLE_SOURCE=OFF` each `src/*.c` is its own translation unit, so a file that only saw `mccrir.h` through `mccgen.c` no longer does. Before pushing a new hook call, build all four: `-DMCC_SINGLE_SOURCE=OFF`, `-DMCC_CONFIG_OPTIMIZER=OFF`, `-DMCC_REPLAY_IR=OFF`, `-DMCC_CONFIG_ASM=OFF`. Each takes about a minute and all four together would have caught both defects. **P3 moved 813 lines into a new translation unit (`src/mccircap.c`) and all four were verified green on it, in isolation** — a shared build directory silently carries each `-D` into the next configure, so use a distinct one per configuration or the matrix tests the union rather than the cases.
- Do not run the twelve-key sweep and `ctest -j8` at the same time. Seven `selfhost-fixpoint` cells fail under that contention and pass individually — a false red that costs a bisect.
- A **native x86_64** reading, taken in an emulated `linux/amd64` container, is NOT the cross x86_64 row: `tests/exec` reads `c2ok=1114/1115` with one byte divergence where the cross build reads a clean `1120/1120`. The populations differ — 269 files against 265, 1151 bodies against 1156 — because a native build resolves the container's glibc headers and the cross build resolves the Gentoo stage3 sysroot, which is exactly the census trap the first item in this section describes. Do not read the difference as a codegen difference until the file set is pinned. The same run left a `qemu-x86_64` core dump on one `tests/exec/statements` file; that is emulation noise, not an mcc crash — the native arm64 container compiles the same file fine.
- Extend the native-versus-cross cross-check to riscv64 and i386. arm64 has it, re-measured at HEAD in a `linux/arm64` container against `tests/exec`: native reads `c2ok=1179/1180`, cross reads `1169/1170` — the same gap of 1 over a population 10 bodies larger, because a native build resolves glibc's headers directly instead of through the stage3 sysroot. On the `all` corpus the two are **not** comparable and must not be put in one table: the native build compiles 556 files against the cross build's 543, `full_language.c` among them, so the native row carries divergences the cross row never had a chance to see. Pin the file set before comparing anything wider than `exec`. Neither i386 nor riscv64 can run natively on this host; both need silicon or a full-system emulator, not `qemu-user`.
- **Ten of twelve keys are confirmed host-invariant off macOS** — a second host stack reproduces the board. On a Windows x86_64 host, mcc built with the vendored winlibs gcc and `-DMCC_REPLAY_IR_C2=1` reproduced all five PE-key rows byte-exact, and the `arm-win32` ≡ `arm-wince` identity gate held natively. A WSL Ubuntu clone with the five ELF cross compilers and the four Gentoo stage3 glibc sysroots reproduced x86_64/i386/arm/arm64/riscv64 exactly on the same host. This is host-invariance, which is weaker than and distinct from the on-target-native check the line above wants.
- The PE `tgmath.h` shadowing is **not real** — settled on the macOS host, do not reorder any header lookup.
- Keep the fifteen files the repo-wide comment strip deliberately skipped, since in each the comment is the test payload and not prose. The ten `tests/diagnostics/dg-error/*.c` carry the expected diagnostic in a `/* dg-error: ... */` marker that `run_dgerror.cmake` greps out and hard-fails on when it is absent; `tests/exec/preprocessor/comment.c` is the comment-lexing permutation corpus; `tests/cst/kinds/comment.c` and `tests/cst/hashinv/spaced.c` exercise CST comment promotion and the H_s/H_t comment-invariance split; `tests/preprocess/asm/gas_comments.S` is the GAS comment corpus; and `tests/exec/programs/grep.c` is greped as its own input by the `{SELF}` golden, which expects the trailing vim modeline in the output. The strip also preserved line numbering in `tests/**` C sources because `tests/exec/goldens.h` pins diagnostics as `file.c:line:`.
- Keep `src/mccrir.c` free of any `MCC_TRACE(` call; `tools/tracegate.c` checks a file only if it contains one, so the first call pulls that file's ~250 unannotated branches into the checked set at once. Run `./cmake-c2all/tracegate src` and `./cmake-c2all/schemagate src` before every push.

## The imaginary-literal fix, held on a branch

`fix-imaginary` (`a3c51e8d`) closes the imaginary-literal defect this file has carried as open: the lexer took `i`/`I`/`j`/`J` only *after* the float suffix, so `1.0iF` — exactly what glibc's `complex.h` writes — did not lex; and the order that did lex did not fold, so `double _Complex z = 1.0i;` at file scope reported "initializer element is not constant". Both halves are fixed, folding in `gen_imaginary_complex` so the parser and the recorder stay on one path, and it drags out a latent bug in `mk_complex_type`, which maps every non-float base to the `long double _Complex` cache slot — an integer imaginary literal took whichever complex type happened to be cached there. Twenty suffix spellings, file and block scope, runtime values byte-identical to gcc.

**It is not on `main`, and the reason is worth more than the fix.** The fold makes complex constants rodata references, and the AST recorder cannot model that: `ast/replay-complex_imag` stops replaying `mk` at all, and the recorder's fidelity ratchet gains three gaps — `c11_imaginary_suffix.c::local_scope`, `::main`, and `feature_macros.c::main`, all `desync` or `unfaithful`.

**Replay_IR is `rfaithful` on every one of them, before and after, and the fold takes `mk` from 20 ops to 6.** So this is not a regression in the compiler; it is a front-end improvement that the recorder alone cannot follow, measured on the one body where the two models can be compared directly.

Landing it then would have meant re-banking a fidelity ratchet to accept a recorder regression, on a subsystem P5 deletes outright.

**Re-measured after P4 flipped the default, and the cost has already shrunk from four cells to three.** `ast/replay-complex_imag` now **passes**: it failed only because the recorder was the producer, and Replay_IR was `rfaithful` on `mk` all along. Merged onto the flipped `main` the branch costs exactly the three `ast-verify-ratchet-{O1,O2,O3}` cells and nothing else — the other 22 failures in that run are the long-checkout-path `bound_global` artifact, not the fix. Those three cells measure the **recorder's** fidelity and P5 deletes them outright, so **land `fix-imaginary` the moment P5 lands**; it merges clean and needs no rebase.

Two further findings from it, neither a complex defect, both filed here so they are not re-diagnosed as one. mcc advertises `__GNUC__ 4` / `__GNUC_MINOR__ 2`; glibc gates `CMPLX` on `__GNUC_PREREQ (4, 7)`, so `CMPLX(5.0, 6.0)` parses as a call to an implicitly-declared function and reports "initializer element is not constant" for an entirely different reason. And `bits/floatn.h` leaves `__HAVE_FLOAT128 0` against an unconditional `__HAVE_FLOAT64X 1`, which `tgmath.h` `#error`s on. One version-advertisement question underlies both. With the fix, the complex family against **glibc's own** `complex.h` goes from 2/8 files to 6/8 on the five ELF keys, and those two are the remainder.

## Open work raised after the cutover

- **Use `AST_Poison` for dead arms instead of deleting them.** The `dead_code.c::main` defect was `ast_sccp_scan` folding `if (const)` by *discarding* the dead arm, guarded by an enumeration — `ast_sccp_has_label` counted only `AST_Jump op == 4` (a label definition) and missed `op == 2` (case) and `op == 3` (default), so the fold deleted case entries whose bodies were live. The fix added a second memoized predicate beside it, which closes the bug but leaves the shape: **a deletion guarded by an enumeration of everything that must not be deleted, which is only ever as complete as its last audit.** Poisoning the subtree instead keeps the node, so every existing scan — labels, cases, defaults, and whatever is added next — still sees it, and correctness stops depending on the guard being exhaustive. `AST_Poison` already exists, five passes already produce it (`ast_narrow_make`, `ast_dse_kill`, `ast_sccp`, `ast_jt_run`, `ast_bf_drop`) and `rir_op_effect` emits it, so the vocabulary is there. Before doing it, confirm two things: that replay emits nothing for a poisoned subtree, and that every pass treats `AST_Poison` as opaque rather than walking into it. If both hold, the two `has_label`/`has_case` predicates and their negative can go with the change.
- **`array_in_struct_init.c` is `ast_cycle_env`, not an `-O3` anomaly.** Measured on arm: the prod-on/prod-off objects agree at `-O2` and differ at `-O3`; `MCC_AST_CYCLE=0` at `-O3` makes them agree, and `MCC_AST_CYCLE=1` at `-O2` reproduces the divergence. `ast_cycle_env` is `optimize >= 3` (`src/mccast.c:2060`) and re-runs the strategy cycle to a fixpoint, so the arena and the tree converge to different fixpoints on this body. **The three `-O` levels should agree**; that they do not on exactly one body is the tell. Fix the convergence, do not special-case the file.
- **Instrument a test for the lost-intermediate over a pointer.** `for_each_elem` (`src/mcc.h:1662`) expands to `elem < (type *)(sec->data + sec->data_offset)`; the `(ElfW_Rel *)` on an `unsigned char *` emits no op, so the arena keeps `unsigned char *` and replay compares mismatched pointer types. It costs **zero C2 counters**, which is exactly why it went unseen for so long, and it is benign only because nothing downstream reads the pointee — a `+`, `-` or `[]` on such an operand would scale by the wrong size. There is no test for it. Write one that *does* scale: a code-free pointer cast whose result is then indexed or offset, so the wrong pointee size becomes a wrong address rather than a wrong type. It should fail today.
- **Finish real glibc header support.** mcc advertises `__GNUC__ 4` / `__GNUC_MINOR__ 2`. glibc gates `CMPLX` on `__GNUC_PREREQ (4, 7)`, so `CMPLX(5.0, 6.0)` parses as a call to an implicitly-declared function; and `bits/floatn.h` leaves `__HAVE_FLOAT128 0` against an unconditional `__HAVE_FLOAT64X 1`, which `tgmath.h:66` `#error`s on. With the imaginary-literal fix the complex family goes from 2/8 to 6/8 files against glibc's own headers on the five ELF keys, and those two are the remainder. One version-advertisement question underlies both — decide what mcc should claim and what it must then implement, rather than raising the number and finding out. The sweep's `-I runtime/include`-first ordering is a *preference* once this lands, not a requirement.
- **`MCC_TRACE` `full_language.c` to isolate its remaining defects.** It is the second front and the only file that reaches classes `tests/exec` does not, but it is also the file that aborts the C2 probe on several keys and fails to parse under `MCC_REPLAY_IR=1` with an armed recorder. Build with `-DMCC_CONFIG_TRACE=ON` and use `tools/tracediff.sh` to diff the trace of a passing key against a failing one; that is the instrument the tree-era work used to localise `ast_hook_vdup`, and nothing has pointed it at this file. Do it before attacking any individual body.
- **Audit the rest of the arena for fields that do not mean what they say.** The `!cmp` defect was not that a pass was wrong; it was that the arena carried an operator whose sense lived in a *separate flag*, so a pass reading the operator alone was reading a half-truth. `AST_FB_LANDOR_INVERT` is the same shape and is still live — `ast_replay_value` applies it, and nothing else consults it. The general rule the two suggest: **a flag may say how replay reaches a value; it may never be the only place the value's meaning is written.** `AST_FB_CMP_INVERT_LATE` now obeys that. Walk the remaining `AST_FB_*` bits and the `ast_op` conventions and decide, one at a time, which side of the line each is on. The cheap standing check is the runtime A/B (`-O1` with and without `MCC_RIR_PROD=1`, stdout and exit code) — it is what the fuzzer effectively ran.
- **`selfhost-fixpoint-O3` — mcc's own `-O3` inliner miscompiles `ast_narrow_elim_srcrange`.** Every macOS stage2 cell reds on it; `-O1`, `-O2`, `-Os`, `-gates` and all three `memmodel` levels pass, and it passed at the commit before the P5 default flip. Same object size, **three bytes**, one instruction: for `macho_be32`'s `p[3] = (unsigned char)v;` a clang-built mcc emits `ldrb w1, [x29, #0xa8]` and an mcc-built one emits `ldr`. The following `strb` discards the difference, so the damage is zero and the gate is still right — it has caught mcc changing a codegen decision when it compiles itself, which is the whole job.

  Localised, all of it measured rather than reasoned:
  - **The probe.** `void be32(unsigned char *p, unsigned v) { p[0]=v>>24, p[1]=v>>16, p[2]=v>>8, p[3]=v; }` at `-O3`. `ldrb` is the agreeing answer.
  - **One TU.** A per-file `-O2`/`-O3` mix over the 22 multi-TU sources, delta-debugged: **`mccgen.c` alone at `-O3`** reproduces it, everything else at `-O2`. `mccgen.c` `#include`s `mccast.c` at line 15054, so that TU is where the narrow pass lives — the result is self-consistent, not a coincidence.
  - **Two gates, and only two.** With `mccgen.c` at `-O3`, `MCC_AST_INLINE=0` **or** `MCC_AST_PROMOTE=0` makes it agree; `CYCLE`, `TEMPLATES`, `COLOR`, `PRE`, `IVSR`, `LICM_TEMP`, `NARROW`, `CSE_JOIN`, `CPROP_JOIN`, `SETHI`, `REASSOC`, `DIVMAGIC`, `RANGE`, `SCCP_FIX` all still diverge. So it is the inliner **crossed with** register promotion, and `INLINE` is why only `-O3` shows it.
  - **One graft.** `MCC_AST_INLINE_LIMIT` binary-searches to **#55**: limit 54 agrees, 55 diverges. Instrumenting the two graft sites names it — `ast_vlat_context_at` grafted into `ast_narrow_elim_srcrange`, at that function's first `return ast_vlat_context_at(a, c, out);`.
  - **The symptom.** With the decision printed from `ast_narrow_elim_fits`, the limit-54 compiler reads `srcrange=1 ctx={lo=0 hi=4294967295 tt=51 st=1}` and the limit-55 one reads `srcrange=0`. **The grafted call returns 0 where it must return 1**, so `ctx` is never written and the caller's `AstVLat` is read uninitialised — which is why a clang-built and an mcc-built compiler disagree at all.

  **Where to look, and the trap.** `ast_inline_graft` calls `save_regs(0)` immediately before `ast_replay_bb(e->ast, ...)` (mccast.c ~2754). That is the same shape as the banked "a promoted store target loses its assignment" defect below: a promoted register spilled inside the graft, `save_reg_upstack` rewriting the vstack entry to the temp, and the pinned register left stale. `ast_vlat_context_at` returns 0 at exactly three points — `!ast_vlat_env`, `!ast_vlat_use_of(...)` and `el.state != AST_VLAT_FACT` — and `el` comes from a struct-returning call, so the hidden return slot is the first thing to check under the graft's frame `bias`. **Synthetic reductions of the shape do not reproduce**: four attempts (out-pointer callee, struct-returning inner call, non-inlinable inner call, added frame pressure) all agree at every `-O` level, so work from the two real binaries, not from a model of them. And the banked negative from the promoted-store defect applies here too — do not fix it by making `save_reg_upstack` skip pinned registers.

- **mcc cannot self-host on Windows arm64.** Every `stage2` cell on `windows-arm64-msvc` and `windows-arm64-mingw` dies the same way: the stage1 mcc, itself built by the host `cl`/`gcc`, takes an access violation (`code=3221225477`) compiling `lib/atomic.c`, `lib/alloca.S`, `lib/alloca-bt.S` and `lib/builtin.c` — the runtime library, four objects, before anything of the compiler proper is reached. Not a target-codegen defect: a cross build from macOS produces `arm64-win32-libmccrt.a` from those same sources without complaint, so the crash is in mcc **running** on Windows arm64, not in what it emits for it. That leaves the host ABI — varargs, `alloca`, the stack probe — as the place to look, and it needs a Windows arm64 machine to look on. `windows-arm64-mingw` carries `experimental` and does not red the workflow; `windows-arm64-msvc` does not and does.
- **Three of the nightly campaign's seven repros are not miscompiles and never were.** Seeds 240466, 240914 and 241631 are syntactically invalid C — a stray `}` whose `for (...) {` is gone — which is why `triage` could attribute none of them and why the campaign's "2 new classes" over-counts. The reducer bug that produced them is fixed (`reduce` restored the whole chunk on a rejected step, reviving lines an *earlier accepted* step had removed, so `keep[]` stopped describing any candidate that had been tested), and `reduce` now refuses to bank a final candidate that does not reproduce. Delete those three from any list of open defects; the real content of that campaign is the `!cmp` class, which seeds 240739, 240981 and 241595 all reduce to.

## Cut to Replay_IR

### The finding that sets the order

**The recorder's tree is the production optimizer at `-O1` and above, not a side-car.** `ast_replay_env = optimize >= 1 || embed_jit || MCC_FORCE_REPLAY` (`src/mccast.c:2035`) arms the whole recorder through `ast_try_active` (`src/mccast.c:16016`); `ast_replay_body(ast_cur)` then **overwrites** the parser's bytes at `src/mccast.c:18186`, and the parser's copy is restored only on `!faithful` (`src/mccast.c:18804`). The passes mutate `ast_cur` in place (`ast_run_strat_cycle`, `src/mccast.c:18306`) and `AST_PF_EMIT` re-emits it (`src/mccast.c:18420`). `ast_cur` is also the only source of JIT blobs and retain pools — six call sites, `src/mccast.c:18045, 18486, 18526, 18572, 18910, 18912-18913`.

So "delete `mccast.c`" would delete the optimizer and the JIT. The correct statement of the work is narrower and harder: **the recorder is a redundant second producer of the same arena, and Replay_IR takes its place.** The arena, the replay engine, the ~10,800 lines of passes and the slice/JIT infrastructure all survive; what dies is the 1,716-line hook recorder (`src/mccast.c:2311-4026`, 143 definitions, 126 call sites in `src/mccgen.c`, 78 declarations in `src/mccast.h`), the journal's verify half (~550 of `src/mccast.c:14495-15971`), and the measurement scaffolding that exists only to compare the two producers.

**`-O0` is the A/B precisely because the recorder is absent there.** At `-O0` no arena is allocated and the parser's bytes ship untouched, so every phase below must leave `-O0` objects byte-identical — a cut that moves an `-O0` byte has touched something it had no business touching. `MCC_FORCE_REPLAY` at `-O0` arms Replay_IR over every body independently of the recorder, which is what makes it a coverage proof rather than a comparison.

### Decisions taken

- **Cutover: per-body fallback.** Production uses `rir_arena` when that body's C2 check passes and keeps the parser's bytes when it does not. This is not new machinery — the `-O1` path already is *emit from the arena → compare against the parser → restore on mismatch*, and per-body fallback is that same shape with `rir_arena` substituted for `ast_cur`. It converts the remaining 18-body C2 gap from a precondition for deleting 4,500 lines into an optimization gap on 18 bodies per key. Close them because they are worth closing, not because the deletion waits on them.
- **Scope: refactor the optimizer and the JIT onto Replay_IR**, rather than leaving them mechanically re-producered. The pass drivers, the retain pools, the `-O3` forward-inline re-emit and the JIT blob producers each get rewired to the Replay_IR arena as a first-class source.
- **Renames: full split and `ast_*` → `ir_*`.** `src/mccast.c` splits into arena+replay, passes, and slice/search units, and the prefix goes with it. The `ir_`/`IR_` namespace is verified empty across `src/`, `tools/` and `include/`. Size: ~10,800 `ast_` and ~2,750 `AST_` tokens over `mccast.c`, `mccrir.c`, `mccgen.c`, `mccjit_embed.c`, `mccjit_intent.c`, `mccast.h` and `tools/asttool.c`.

**The split must extend the include chain, not create link units.** `mccast.c` and `mccrir.c` are `#include`d back to back into one translation unit (`src/libmcc.c:11-12`; `src/mccgen.c:15229-15235`), and `mccrir.c` reads `mccast.c`'s file statics — `jrn_ops`, `jrn_vs`, `ast_cur`, `ast_base_depth`, `ast_body_ind_sv`. The ordering is load-bearing for the journal's macro trick as well: `src/mccgen.c:143-234` `#define`s 61 codegen primitives to `jrn_` twins and `src/mccgen.c:15155-15234` `#undef`s all 61 *before* the includes, which is the only reason `JRN_W0(load, …)` can define `jrn_load` whose body calls `(load)(…)`.

### The gate at each phase

1. `-O0` objects byte-identical across the corpus, before and after.
2. `MCC_RIR_FORCE=1` at `-O0`: `faithful + empty == fn` on all twelve keys.
3. `C2_NO_EXTRA=1 tools/c2_sweep.sh` counters unmoved — for a re-homing or a rename, *unmoved by one* is the whole proof. Only P4 is allowed to move them.

### P0 — harness first, nothing deleted

- **Done**: `rir_parity.cmake`'s derived forced-`-O0` gate list is now an explicit `MCC_RIR_FORCE=1 MCC_AST_INT128=1`, the first read at `src/mccast.c:2035` beside `MCC_FORCE_REPLAY`, which still works and is still the name the `-O0` A/B in this plan is written against. `SRCDIR` is gone from the script and from all twelve cells. The board is unchanged from the derived form on every key; `MCC_AST_INT128` is the one gate of the 38 that was worth bodies, and **Keep the measurement honest** records why it is set explicitly and when to delete it.
- **Done**: `tests/ast/rir_c2.cmake` is a banked-gap ratchet over `MCC_REPLAY_IR=5`, wired as `ast/rir-c2-{O1,O2,O3}` plus eleven cross cells, banked at the 18-body gap and SKIPping a build without `-DMCC_REPLAY_IR_C2=1`. Numbers and the population floor are in **Keep the measurement honest**. The 100% bar belongs to the bodies, not to the deletion.
- **Done**: `tools/o0_ab.sh` is the `-O0` A/B itself — per-key object sha256 over the corpus plus the forced-`-O0` coverage board, banked under `tests/ast/o0-baseline/` and re-checked with `O0_AB_CHECK=1`. It refuses to measure a key whose sysroot is absent, because the first run without one read 76-79 objects of 276 on four keys and still printed a green board on every row.
- Add the `-O0` object-identity cell. `tests/ast/journal_inert.cmake` already supports `-DRIR=1` (`:21-26`) and is the right driver.

### P1 — free cuts — **Done**

Nothing here was in the `-O1` path and nothing here was read by Replay_IR. Landed as `p1-free-cuts`, **-859 lines of `src/mccast.c`**, with the twelve-key C2 board and the `-O0` corpus byte-identical on both sides — measured, not inherited.

- The journal's verify/report half is gone: `JRN_FIX_LIST`/`JFIX_*`, `jrn_sv_eq`, `jrn_prefix_eq`, `jrn_classify`, `jrn_deep_reg_live`, `jrn_run`, `jrn_emit_line`, `jrn_blame`, `jrn_site`, `jrn_verify`, `jrn_configure`, the whole `jrn_oracle_*` family, `jrn_report`, the `jrn_tot_*`/`jrn_fixhist`/`jrn_ophist`/`jrn_fixpair`/`jrn_regdiff_n` counters, `jrn_fix_name`, and the now-dead `#else` inert stubs. `MCC_JOURNAL`, `MCC_JOURNAL_OUT` and `MCC_JOURNAL_ORACLE` no longer exist as environment gates; `MCC_JOURNAL_HOOKS` does, and still names the capture substrate.
- The record substrate is untouched. `jrn_issue` is verbatim minus its one write into the verify half (`jrn_regdiff_n++`, which took `int got` with it); `jrn_reset` lost only its two verdict lines. `jrn_gap`, `jrn_bad`, `jrn_active`, `jrn_depth`, `jrn_snap_vstack`, `jrn_begin`/`jrn_end`, `jrn_new_op`, `jrn_raw_add`, `jrn_op_name`, `jrn_replaying`, `jrn_fconst_take`/`jrn_fconst_note`, `jrn_pred`, `JrnOp`, `JRN_OP_LIST` and every `JRN_W*`/`JRN_R*`/`JRN_WSV` wrapper stay. `struct JrnSub` and the sub-op table died with `jrn_oracle_ops`, which collapsed `jrn_begin`/`jrn_end`'s nesting branches to a plain depth counter.
- The gate in `ast_func_begin` is now `if (rir_env && ast_try_active)`; capture (`jrn_active = 1/0`) still drives Replay_IR. `jrn_started` is gone with `jrn_verify`, and the `jrn_oracle_want` disjunct at the verdict site reduced to `if (ast_verify_env)` — which made `ast_declined` and `ast_replay_tried` dead locals, so they went too.
- `ast_hook_stmt` (an empty switch) and `ast_hook_data` are deleted with their two `mccgen.c` call sites and their `mccast.h` declarations. `ast_hook_data` took its exclusively-dead diagnostic family with it — `ast_data_zero_check`, `ast_data_estimate`, `ast_data_roundtrips`, `ast_data_reemit`, `ast_data_reemit_selftest`, `AstDataRec`/`ast_data_recs`, the three `ast_data_pipe_*` buffers, the five `ast_data_*` counters and the `MCC_AST_DATA_REPORT`/`MCC_AST_DATA_REEMIT` gates. The real `.data`→`.bss` and string-merge transforms are the parser's, at `src/mccgen.c:14383/14401/14418`, and are untouched; so are `ast_data_all_zero` and `ast_strpool_find_or_add`, which they call.
- Tests, build and tools: `ast-journal-parity-{O1,O2,O3}`, the 30 `tests/ast/journal-baseline/` files, `verify_ratchet.cmake`'s entire JOURNAL mode, `tools/jrn_sweep.sh`, the 130 `journal-sweep-*`/`journal-native-*`/`journal-regen-*` targets and `journal_sweep.cmake`/`journal_native.cmake`/`journal_report.cmake` are all gone. `verify_ratchet.cmake` keeps only the recorder's `[ast-verify]` ratchet, which dies in P5.
- `tests/ast/journal_inert.cmake` survives with one mode instead of three. `ast/journal-inert` (the `MCC_JOURNAL=1` runtime cell) is deleted — `ast/rir-inert` already covers that ground — and the driver's gate is now unconditionally `MCC_REPLAY_IR=1`/`[rir-verify]`. `mcc_nojrn` and `ast/journal-inert-build` are kept and re-pointed onto that gate: proving the capture substrate is byte-inert still matters for Replay_IR, and it is the same proof.
- The `raw=`/`rawb=` breadth bucket was **retired, not ported**. That discharges the twelve outstanding `.depth.txt` rebanks: the files are deleted rather than regenerated in twelve environments.

### P2 — decouple Replay_IR from recorder state

Replay_IR reads five recorder-owned globals; all must move before the recorder can die.

**Done (state ownership).** `src/mccrir.c` now owns `rir_base_depth`, `rir_body_ind_sv` and `rir_reloc0_sv`, all three sampled in `rir_hook_body_begin`, and no longer names `ast_base_depth`, `ast_body_ind_sv`, `ast_reloc0_sv` or `ast_try_active`. `src/mccast.c` was not touched; its four globals are still live for the recorder and for the journal's verify half, and they die with those. Twelve-key C2 board and the `-O0` corpus objects are byte-for-byte unmoved; the 50 `ast/rir-parity-*` cells pass.

What the transfer turned out to rest on:

- **The sample point is exact, not approximate.** `rir_hook_body_begin()` (`src/mccgen.c:14588`) and `ast_func_begin()` (`:14589`) are adjacent statements after `gfunc_prolog`/`mcc_debug_prolog_epilog`/`func_vla_arg`, so `ind`, `cur_text_section->reloc->data_offset` and `(int)(vtop - vstack + 1)` are identical at both. `rir_hook_body_begin` has exactly one call site.
- **The recorder assigned `ast_base_depth` only inside `if (ast_try_active)`; Replay_IR assigns unconditionally.** Not a behaviour change: every `rir_base_depth` read is behind `rir_started`, and `rir_started` is set only under `ast_try_active` (`src/mccast.c:16060`), so the stale-value case was never observable.
- **The cleanup rebase is a save/restore, and it is also unobservable today.** Confirmed: `ast_hook_cleanup_call_begin` early-returns unless `ast_vn == 0`, so `ast_cleanup_bias` collapses to `(vtop - vstack + 1) - ast_base_depth` and the pair is exactly *set to current depth / put it back*. `rir_hook_cleanup_call_begin/end` (`src/mccgen.c:12243`/`:12258`) mirror that, including the `bias < 0` no-op leg. It cannot move a counter, because **every reader of `rir_base_depth` is replay-time**: `rir_op_effect`, `rir_mark_apply` and `rir_region` are reached only from `rir_to_arena`/`rir_run`, which run only from `rir_verify`, which runs once at `ast_func_end` — long after every cleanup pair has closed. The sibling exists so the invariant survives the recorder's deletion, not because it is load-bearing now. `tests/exec/features_c99_c11/cleanup.c` does exercise the hook.
- **`ast_try_active` at the `arenahasheq` gate was dead weight.** `rir_verify` is called only under `if (rir_started)`, and `rir_started` implies `ast_try_active`, so the term was always true there. It is now `rir_started`, which is the honest predicate for that site and is Replay_IR's own.

**Done — the `ast_ret_bad` widening.** `src/mccrir.c` owns `rir_try_active = rir_env && !debug_modes && !cur_func_inline_extern`, sampled in `rir_hook_body_begin`; the arming block in `ast_func_begin` is now `if (rir_try_active)` and its redundant inner `if (rir_env)` is gone. `-O0` objects byte-identical on all twelve keys; all 153 `^ast` cells pass.

**The `-O1` boards did not move — by one line, not by luck.** `ast_ret_bad` is `ast_bad_vtype(func_vt.t) && (func_vt.t & VT_BTYPE) != VT_STRUCT`, and `ast_bad_vtype` is `ast_bad_type && !ast_wide_vtype` (`src/mccast.c:2411-2432`), where `ast_wide_vtype` returns `ast_ldouble_env` for `VT_LDOUBLE` and `ast_int128_env` for `VT_INT128`/`VT_QLONG`. Both of those gates default on at `optimize >= 1`, so at `-O1` the predicate already reads false for exactly the two type classes it names — the term was only ever costing bodies where those gates are off. Twelve-key `exec` and `all` boards at `-O1` are byte-identical before and after, per-file logs included.

**Where it does pay: forced `-O0`.** `MCC_REPLAY_IR=1 MCC_FORCE_REPLAY=1` at `-O0` gains 2 bodies on x86_64 (`fn` 1144 → 1146, `faithful` 1109 → 1111) and 2 on x86_64-osx (1138 → 1140, 1103 → 1105); the other ten keys are unmoved, and `empty` stays 35 with `unfaithful`/`diverge`/`rewind`/`error` all 0 everywhere. Both bodies are `tests/exec/types/int128.c::addv` and `::mix`, the `__int128`-returning pair, and both verify `rfaithful` — 2 admitted, 2 faithful, 0 new failures. `tests/ast/o0-baseline/{board.txt,x86_64.rir.txt,x86_64-osx.rir.txt}` re-banked; `board.gated.txt` and the `.gated.rir.txt` files did not move.

**So `MCC_AST_INT128=1` in the forced-`-O0` env is now a no-op for Replay_IR coverage** — the gated board already read 1146/1140 and the ungated board now reads the same. It still gates the recorder's own tree, which the `[ast-verify]` ratchet measures, so leave the line where **Keep the measurement honest** put it and delete it with the recorder, not before.

**Superseded — the `ast_ret_bad` widening, on its own commit.** Deliberately *not* in the above, because it changes the measured population and a pure ownership transfer must not. The admission predicate Replay_IR should have is `rir_env && !debug_modes && !cur_func_inline_extern`, **dropping `!ast_ret_bad`** — an AST *value-model* limit (`long double`, `__int128`, `_Complex` returns) that Replay_IR does not share and is currently paying for. Where it goes: today Replay_IR has no admission predicate of its own at all — it is armed entirely by `src/mccast.c:16060`, `if ((jrn_env || rir_env) && ast_try_active) { ... if (rir_env) { rir_reset(); rir_active = 1; rir_started = 1; } }`, which is why `ast_try_active` is the de facto gate. The predicate belongs in `rir_hook_body_begin` as a `rir_try_active` that `rir_reset`/`rir_active`/`rir_started` key off, which also lets the arming block at `:16060` shrink to the recorder's half when the recorder dies. Expect the population to widen and new failures to surface; that is coverage arriving. Measure it at `-O0` under `MCC_RIR_FORCE=1` first, then take a fresh twelve-key board — the C2 row moves by construction, so the before/after board is the deliverable, not a regression report.

- Re-home the reverse edges: `rir_dbg_on` and its two taps out of `src/mccast.c:14805/14944`; `ast_alloc_loc`/`ast_alloc_temp_loc`/`ast_fconst_*` into neutral frame and constant services with Replay_IR and the recorder as observers rather than a priority chain buried in `ast_fconst_reuse` (`src/mccast.c:2279`); `ast_sym_defer` → `sym_defer` (Replay_IR needs it too — `rir_snap_types` exists because `Sym*` are unstable).
- Model to copy: `rir_hook_slot_record/replay` (`src/mccgen.c:7679/7684`) is already a clean parser↔Replay_IR pair that never touches `mccast.c`.

*Gate: C2 counters unmoved on every key, except for the measured `ast_ret_bad` widening.*

### P3 — rebrand the capture substrate

**Done.** `jrn_*` → `ir_cap_*`, `JrnOp` → `IrCapOp`, `JOP_*` → `IR_OP_*`, `JRN_OP_LIST`/`_ENUM`/`_NAME` → `IR_OP_*`, `JRN_W0`/`W1`/`W2`/`R1`/`R2`/`WSV`/`REC` → `IR_CAP_*`, `MCC_JOURNAL_HOOKS` → `MCC_IR_CAPTURE`, `MCC_JRN_HAVE_*` → `MCC_IR_HAVE_*`, `MCC_JRN_VA_START_VOID` → `MCC_IR_VA_START_VOID`. Nothing named `jrn`/`JRN`/`JOP` survives under `src/`, `tools/` or `include/`. Line-for-line: `mcc.h` 11/11, `mccasm.c` 6/6, `mccrir.c` 135/135, `mccgen.c` 213/209 (the four extra lines are the new include).

**The substrate is `src/mccircap.c`**, 813 lines, joined to the include chain between `mccast.c` and `mccrir.c` in both places that build the translation unit — `src/libmcc.c` and the `!MCC_AMALGAMATED` block at the foot of `src/mccgen.c`. It is not a link unit and must never become one. The move is exact: the file is `src/mccast.c:14416-15224` verbatim under the rename, `#pragma push_macro("gjmp")`/`pop_macro` still bracketing the whole region, wrapped in `mccrir.c`'s own `#if MCC_CONFIG_OPTIMIZER && (defined(MCC_INTERNAL) || !defined(MCC_AMALGAMATED))`.

What the split rested on:

- **`tools/asttool.c` `#include`s `mccast.c` and is unaffected**, because everything from `src/mccast.c:1289`'s `#ifdef MCC_INTERNAL` down is already invisible to it — `MCC_INTERNAL` comes from `mcc.h:4` and `asttool` never includes `mcc.h`. Had the substrate been outside that guard, `asttool` would have needed the new unit too.
- **Four references run backwards into the substrate** and now need declarations at `src/mccast.c:2272-2277`: `ir_cap_reset`, `ir_cap_gap`, `ir_cap_active`. `ir_cap_fconst_take`/`ir_cap_fconst_note` already had them. `ir_cap_active`, `ir_cap_raw` and `ir_cap_vs` are declared as tentative definitions in both files, which is one object in one translation unit, and is the idiom `src/mccast.c:4068` was already using.
- **`tools/targetgate.c`'s `ALLOWED[]` had to learn the new name** — the substrate carries two `MCC_TARGET_I386`/`MCC_TARGET_X86_64` conditionals. `tools/schemagate.c` did not: `IR_OP_` does not intersect the `AST_OP_`/`AST_FB_`/`RIR_R_`/`RIR_M_` define spaces, and its counts are unmoved at 38/21/27/28/18.

*Gate held: the twelve-key `C2_CORPUS=exec` `-O1` board is identical row for row before and after (`arm-win32` and `arm-wince` agreeing, as they must); `C2_NO_EXTRA=1 O0_AB_CHECK=1 tools/o0_ab.sh` passes against the bank unchanged; a direct object-sha256 A/B against the merge base over 656 corpus files × `-O1`/`-O2`/`-O3` × twelve keys is byte-identical on all 23,616 cells; 153 of 153 `^ast` cells pass; `tracegate` and `schemagate` clean.*

### P4 — the cutover

The only phase with real risk, and the only one allowed to move a counter. **It is built, measured, and shipped OFF by default.** `MCC_RIR_PROD=1` turns it on; unset, nothing about the compiler changes. The reason is below, and it is the finding of the phase, not a failure of nerve: three separate classes of **wrong code** come out of running the existing passes over the Replay_IR arena, and the arena's own lossiness is the cause of all three.

**Landed and unconditional.**

- `rir_prod_take()` (`src/mccrir.c`) builds the arena for the body just parsed, runs the pre-flight — non-empty op stream, no `ir_cap_bad`/`rir_unbal`/`rir_ovf`, no `rir_arena_mismatch`, `ast_validate`, `rir_emit_safe`, no `AST_OP_ASM` node — and **detaches** `rir_arena` (sets it to NULL so the next body allocates a fresh one) so the caller owns it. `rir_prod_replay_begin`/`_end` set up and tear down exactly the C2 prologue (`rir_c2_active`, `ir_cap_replaying`, `ast_replaying=0`, the four `rir_*rec_i` cursors, `loc = rir_body_loc_sv`, `ast_rp_*`, `ast_temp_frontier`) around the one replay that does the byte compare.
- `ast_func_end` substitutes the arena: `ast_cur = rir_prod_take()` and the tree arena is freed. **Every consumer moves with it for free**, because they all read `ast_cur` — `ast_run_strat_cycle`, the loop transforms, `AST_PF_EMIT`, `ast_plan_promotion`, `ast_inline_retain`/`ast_reemit_retain` (and therefore the `-O3` forward-inline re-emit through `ast_reemit`), `ast_baseline_retain`, `mccjit_embed_note`, `mccjit_embed_stash_leaf`. There is no separate rewiring step; item 2 of the plan collapses into item 1.
- Per-body fallback is the existing machinery, exactly as predicted: the first `ast_replay_body(ast_cur)` is compared byte-for-byte against the parser, and `!faithful` restores the parser's bytes at the site that already existed. The population is unchanged — the swap is gated on `ast_replay_ok(tree)` so no body is *newly* attempted.
- `MCC_RIR_PROD` gates it. `0`/unset is off; `1` is on; `2` also prints `[rir-prod] <verdict>\t<file>\t<func>` per body plus a `[rir-prod-total]` atexit line, into `MCC_RIR_PROD_OUT` if set. Verdicts: `used`, `fallback` (adopted, bytes diverged), `nomodel` (pre-flight refused), `noreplay` (`ast_replay_ok` false). The fifth verdict `nojit` is **gone** — defect 3 below closed and the seam carve-out with it. **Note `ast_env_int` returns the default for a value `<= 0`**, so the gate is `ast_env_gate(...) ? ast_env_int(...) : 0` — reading it with `ast_env_int` alone makes `MCC_RIR_PROD=0` mean *on*, which cost half a day.
- Production is off whenever `MCC_REPLAY_IR` or `MCC_AST_VERIFY` is set. Both are measurement modes for one of the two producers and must keep measuring that producer; `ast-verify-ratchet-*` and the whole `MCC_REPLAY_IR=5` sweep therefore read exactly what they read before.
- **Deleted**: the `RIRC2TREE` control leg and the four tree-versus-tree counters `arenacmp`, `arenacounteq`, `arenahasheq`, `treenodes` (and `rir_body_hasheq`, `rir_treekindhist`, the `heq=` field of `[rir-c2part]`, and the `arenahasheq` column of `tools/c2_sweep.sh`). **`arenahasheq` is retired deliberately** — it measured fidelity against the thing being deleted; C2 measures against the parser and is the metric that survives. The **Scoreboard** table's `arenahasheq` column is dead and must not be re-derived. The C3 *pair* probe (`pair=`/`samefolds=`/`samehash=`/`pairfired=`) survives, because the 14 `ast/rir-c3-*` cells are a live gate over it; it is now gated inline on `rir_env >= 6 && ast_replay_ok(ast_cur) && intention hashes equal` instead of on the deleted counter, and it dies with the recorder in P5.
- `rir_emit_safe` and its `rir_unsafe`/`rir_bb_slot` helpers left `#if MCC_REPLAY_IR_C2` — production needs them. This is the "amalgamated build hides a missing declaration" class again: it compiled everywhere `-DMCC_REPLAY_IR_C2=1` was set and broke `selfhost-*` and `target-link-gate`, which build the five target sets without it.
- `tests/ast/journal_inert.cmake` runs both legs under `MCC_RIR_PROD=0`. The cell proves the **capture substrate** is byte-inert; with the cutover on, `mcc` and `mcc_nojrn` legitimately differ because one has a different *producer*, not a side-car. Re-pointing it keeps the property it was built to prove.

**The gate, measured.** With `MCC_RIR_PROD` unset:

- `C2_NO_EXTRA=1 O0_AB_CHECK=1 tools/o0_ab.sh bc2 all` passes on all twelve keys, object sha256 and forced-`-O0` counters unmoved against the bank.
- Direct object-sha256 A/B against a clean `origin/main` build over `find tests -name '*.c'` × twelve keys × `-O1`/`-O2`/`-O3`: **19,686 of 19,686 objects byte-identical, 0 differ, 0 build failures.**
- 153 of 153 `^ast` cells pass, and the full `ctest -j 8` failure set is byte-for-byte the same list the same binary produces with `MCC_RIR_PROD=0` — 22 cells, every one of them `exec*/bound_global`, which is the long-checkout-path artifact recorded below and not a change. A pristine `origin/main` build configured under a short path is 8,254 of 8,254 green.
- `tracegate`, `schemagate`, `targetgate` clean; `src/mccrir.c` still contains no `MCC_TRACE`. All four side configurations (`MCC_SINGLE_SOURCE=OFF`, `MCC_CONFIG_OPTIMIZER=OFF`, `MCC_REPLAY_IR=OFF`, `MCC_CONFIG_ASM=OFF`) build green in distinct build directories.

**What it costs when it is on** (`MCC_RIR_PROD=1`, same corpus, same A/B). Files whose object moves, of files that compiled:

| key | -O1 | -O2 | -O3 |
| --- | --- | --- | --- |
| x86_64 | 31/565 | 367/564 | 367/564 |
| x86_64-osx | 26/524 | 337/523 | 337/523 |
| x86_64-win32 | 28/527 | 278/526 | 278/526 |
| i386 | 34/560 | 48/559 | 48/559 |
| i386-win32 | 30/526 | 42/525 | 42/525 |
| arm | 32/554 | 44/553 | 45/553 |
| arm-win32 / arm-wince | 28/519 each | 38/518 each | 39/518 each |
| arm64 | 28/556 | 290/555 | 290/555 |
| arm64-osx | 30/558 | 292/557 | 292/557 |
| arm64-win32 | 24/522 | 276/521 | 276/521 |
| riscv64 | 75/552 | 76/551 | 76/551 |

The `-O2`/`-O3` explosion on the x86_64 and arm64 keys was **one cause**: `opt_promote = optimize >= 2` there, and register promotion was disabled for Replay_IR-sourced bodies, so nearly every file lost it. **That carve-out is gone** (defect 1 below) and the two keys re-measured on the `all` corpus read x86_64 `-O2` 69/562 and arm64 `-O2` 46/555; the table's `-O2`/`-O3` columns are P4's pre-fix reading everywhere else. The `-O1` column and the i386/arm/riscv64 columns are the arena's own effect. riscv64's 75 is the extra `ident` firings the arena's spare `Convert` nodes admit; on `tests/exec/preprocessor/hashdefine.c::main` the RIR arena fires `[ast-ident] 3` where the tree fires none, and the body gets **longer**.

**Per-body verdicts at `-O1`** (`MCC_RIR_PROD=2`, all 657 files, twelve keys):

| key | bodies | used | fallback | nomodel | noreplay |
| --- | --- | --- | --- | --- | --- |
| x86_64 | 2478 | 2221 | 22 | 72 | 163 |
| x86_64-osx | 2215 | 1991 | 18 | 70 | 136 |
| x86_64-win32 | 2364 | 2108 | 23 | 77 | 156 |
| i386 | 2465 | 2199 | 28 | 67 | 171 |
| i386-win32 | 2378 | 2155 | 23 | 71 | 129 |
| arm | 2418 | 2183 | 27 | 43 | 165 |
| arm-win32 / arm-wince | 2334 each | 2139 | 21 | 45 | 129 |
| arm64 | 2479 | 2173 | 25 | 81 | 200 |
| arm64-osx | 2463 | 2162 | 23 | 81 | 197 |
| arm64-win32 | 2392 | 2137 | 19 | 85 | 151 |
| riscv64 | 2409 | 2116 | 21 | 73 | 199 |

`noreplay` is the recorder's own refusal and is not a cost — those bodies were never optimized. `nomodel` is the pre-flight; `fallback` is the byte compare. **The banked fallback list**, 37 distinct bodies, all twelve keys unless noted: `bounds_stress.c::test16`/`::test17`, `rev64_mt.c::main`, `sweep_int64.c::main`, `overflow_inline.c::main`, and from `full_language.c` `struct_assign_test`, `s_stddef_stdint`, `s7_9_iso646_test`, `macro_test`, `char_short_test`, plus (11 keys) `ldfcast`, `ffcast`, `dfcast`, `bfa2`, `bfa3`; (9) `builtin_overflow.c::main`, `full_language.c::longlong_test`; (8) `fuzz/runner.c::triage`/`::main`/`::interesting`; (7) `full_language.c::s7_6_inttypes_test`/`::s7_22_intarith_test`; (6) `run_s_stddef.c::s_stddef_stdint`, `run_s7_9.c::s7_9_iso646_test`, `run_s7_6.c::s7_6_inttypes_test`, `run_s7_22.c::s7_22_intarith_test`; (5) `overflow_narrow.c::main`; (2) `struct_ret_variadic.c::mkv`; (1) `struct_packed_indirect.c::main`, the five `alloca_inline.c` bodies, `run_s7_28.c::s7_28_wconv`, `full_language.c::callsave_test`/`::alloca_test`. That list is the **C2 gap wearing production clothes** and matches the Scoreboard's classes body for body.

**Why it is off — three wrong-code classes, each reproduced and each rooted in arena lossiness.**

1. **Register promotion is unsound on the arena, twice — CLOSED.** Both halves are fixed and `do_promote`'s `&& !ast_rir_arena` carve-out is **gone**; the conservative escape rule was never needed and was not written. What the two halves turned out to be:

   - **The local-array extent.** `tests/ast/replay/promote.c` exited 35 instead of 42: `int arr[4]` passed to `sumptr(arr, 4)` reached the arena as a `Ref` of type `int *`, so `ast_plan_promotion`'s **fourth** loop — the `VT_ARRAY`/`VT_STRUCT` pass at `src/mccast.c:5138`, not the escape loop at `:5117`, which reads `sz` from the pointee and gets 4 in *both* legs — skipped it entirely and never poisoned `[base, base+16)`. **P4's note that `sv->sym` is NULL for a local was wrong**: `unary` sets `vtop->sym = s` for locals too (`src/mccgen.c:11030`) and the sym survives the decay, so `sv->sym->type` still reads `VT_PTR|VT_ARRAY` with the element `Sym` — the extent was in the SValue all along. `rir_leaf_slot`'s array-decay rescue now takes the `VT_LOCAL` case as well as `VT_CONST|VT_SYM`, guarded on `sv->c.i == sv->sym->c` so it only fires where the SValue still names the symbol's own slot, and on `!VT_VLA`.
   - **The op-assign's vdup.** `src/mccpp.c::tal_realloc_impl` was miscompiled because the arena's `al->p += adj_size` read `Convert t=5 (Ref r=0x100)` where the tree reads `Unary MEMBER_ARROW (Ref al)` — `r=0x100` is `VT_LVAL` over **register 0**. The vdup at `src/mccgen.c:11771` pushes a vstack slot Replay_IR does not model, so `rir_reconcile_sv` refilled it from the SValue, which by then held the target's address in a physical register. Replay reproduced the parser's bytes from it (the body was `c2ok`, never a fallback), but promotion reallocated `%rax` and left the base stale. Fixed in a new `IR_OP_VPUSHV` arm of `rir_op_effect`: when the immediately preceding mark was `RIR_M_OPASSIGN`, the depth matches, the target SValue is a register lvalue and the shadow top is an unparented pure `MEMBER`/`MEMBER_ARROW` subtree, push `ast_dup_sub` of it — exactly what `ast_hook_vpush` does for the recorder at `src/mccast.c:2322`. **Bitfield targets must be excluded**: without that the arena for `bitfields.c::main` and `bitfields_ms.c::main` re-emits at a different length, which is the whole cost the rule had.

   Bisected with `MCC_AST_PROMOTE_LIMIT` rather than `MCC_AST_OPT_LIMIT` — it isolates the promotion decision directly, and `tal_realloc_impl` is the **75th promoted** body of `src/mcc.c` at `-O2` (P4's 76th *optimized* body, same function). With both fixes `selfhost-smoke` and `selfhost-smoke-gates` pass under `MCC_RIR_PROD=1` with promotion on, and the twelve-key `all` board is **byte-identical row for row** with the arena change on and off.
2. **The const-fold templates were unsound on the arena — FIXED.** `tests/exec/expressions/cast_operator.c` printed `avg: 6` for `(int)(((double)(3 + 4) / 2) * 2)`, which is 7: the arena lost the code-free `(double)` on a constant and `ast_run_templates` folded `7 / 2` as an integer. It now prints 7 under `MCC_RIR_PROD=1`, closed by the genop float wrap under "What closed". Two things it taught, both still true. **The bug needed two statements to show**: the minimal `double avg = (double)(3+4)/2;` alone reproduced nothing — it takes a *sibling* integer `7 / 2` in the same body for the templates pass to match the folded shape, which is why a one-line repro read green. And **the byte compare cannot see this class at all**: `cast_operator.c::main` was `c2ok` before and after, because C2 validates un-optimized emission and the damage is a later pass reading a type. Every pass that reads a type off the arena stays suspect; the only test that caught this one is a golden that happened to exist.
3. **The JIT seam — CLOSED, and it was never about the arena's shape.** `selfhost-jit` (`--jit -O4 -run src/mcc.c`) segfaulted with the arena adopted, `MCC_AST_OPT_LIMIT=0` did not help, and P4 carved the swap out whenever `ast_jit_env`, `ast_jit_splice_env`, `ast_jit_dispatch_env`, `ast_jit_fns_n`, `ast_search_env`, `ast_roi_env`, `ast_slice_env`, `embed_jit` or `MCC_OUTPUT_MEMORY` was in play (verdict `nojit`). **The carve-out is gone and the cell passes.** The whole `ast_rir_seam` predicate is deleted; there is no JIT-shaped exception left in `ast_func_end`.

   What the seam actually was, in three links:

   - **The arena drops the code-free pointer cast in `for_each_elem`.** `src/mcc.h:1662` expands to `elem < (type *)(sec->data + sec->data_offset)`; the `(ElfW_Rel *)` on an `unsigned char *` emits no op, so the arena kept the raw `unsigned char *` and replay compared `struct <anonymous> *` against `unsigned char *`. `combine_types` therefore calls `mcc_warning("pointer type mismatch in comparison")` — a diagnostic the tree's arena never produced. It moves **no bytes** (both sides are pointers of the same width, and `objfmt/mccelf.c::update_relocs`, `::cfi_fde_shndx` are `c2ok` either way); it only makes a diagnostic *possible*. This is the lost-intermediate class over a pointer and is still open — see "Still open".
   - **`error1` releases the compile state before it returns.** `src/libmcc.c:651` opens with `mcc_exit_state(s1)`, paired with the `mcc_enter_state(s1)` that `#define MCC_SET_STATE(fn) (mcc_enter_state(s1), fn)` (`src/mcc.h:2128`) wraps every `mcc_warning`/`mcc_error` in. Both halves are **no-ops whenever `s1->error_set_jmp_enabled` is 1**, which is every ordinary compile (`mcc_compile` sets it at `src/libmcc.c:825`). That is the only regime the pairing was ever exercised in: from a `USING_GLOBALS` file such as `mccgen.c` the macro does *not* enter, so with the flag clear the pairing is one-sided.
   - **The runtime JIT recompile is the one compile in mcc that runs with the flag clear.** `mccjit_recompile_common` (`src/mccjit_embed.c:594`) does `mcc_enter_state(js)` on a fresh `js` and then runs arbitrary codegen. So the warning above really did set `mcc_state = NULL` and post `mcc_compile_sem`, and `gen_op` dereferenced `mcc_state` four lines later (`src/mccgen.c:4060`, `warn_extra_ptr_zero_cmp`) — SIGSEGV. `mccjit_embed.c` being arena-generic was true and was never the question.

   **The fix is the JIT seam adopting the protocol every other compile already uses**: `mccjit_recompile_common` now brackets the re-emit in `js->error_set_jmp_enabled = 1` plus a `setjmp(js->error_jmp_buf)`, exactly as `mcc_compile` does, with `stk_data_floor` saved and restored around it. Inside that bracket `mcc_enter_state`/`mcc_exit_state` are the no-ops they are during any other compile, so a diagnostic can no longer tear the state down; and a *hard* error in a JIT recompile now unwinds to the setjmp and abandons that recompile (`entry` stays NULL, the caller keeps the original code) instead of `exit(1)`-ing the user's program. `ast_reemit` additionally saves/sets/restores `mcc_state->warn_none` around its `ast_replay_body`, mirroring what `ast_func_end` already does for the tree's replay — re-emitting a body the parser has already diagnosed must not diagnose it a second time.

   **Two things this cost, banked.** Removing `error1`'s opening `mcc_exit_state(s1)` outright *looks* correct (with the flag set it is dead; with the flag clear it strands the state) and **deadlocks**: it is the release half of the `MCC_SET_STATE` pair, and `mcc_compile_sem` is a binary semaphore. Adding a matching `mcc_enter_state(s1)` before each of `error1`'s three returns deadlocks too, for the mirror reason. Do not touch `error1`; fix the caller's regime.
4. Minor, and fixed rather than carved out: **replaying an `AST_OP_ASM` is destructive.** `full_language.c::get_asm_string` re-assembles `asm volatile(... "some_symbol: .long 0" ...)`, the assembler errors on the duplicate label, the `longjmp` out of `mcc_assemble_internal` leaves the tokenizer stack pushed on the asm buffer, and the *parser* then reads `.long 0` and reports `';' expected (got '0')` at the closing brace. The tree never hit it because the recorder drops that body's asm entirely. The pre-flight refuses any arena containing an `AST_OP_ASM`; a narrower rule needs to know whether the asm text defines a symbol or emits into a section, and re-emitting either is not idempotent for the tree's replay either.

**Next step, in the order that de-risks.** None of this needs new machinery — the switch exists and the measurement is a shell loop away.

- **Done — the lost-intermediate class.** Both halves closed, written up under "What closed": the region-value route, not the field-on-an-op route the earlier revision of this file predicted. Defect 2 above is gone; defect 1 is now entirely the array-extent item below. The default still does not flip, because defects 1 and 3 stand. The cheap standing check for defect 2's family is a **runtime** A/B, not a byte one — compile and run every `tests/exec` file at `-O1` with and without `MCC_RIR_PROD=1` and diff stdout and exit code. 252 of 276 build standalone and all 252 now agree; before the fix `cast_operator.c` was the one that did not, and C2 called it clean.
- **Done**: the local-array extent and the op-assign vdup, and with them `do_promote`'s `!ast_rir_arena`. It did come from the SValue after all — see defect 1 above.
- **Done, and it was worth the whole `-O2` column.** Object-move A/B against the same binary with the switch off, `all` corpus minus `full_language.c`: x86_64 `-O2` **367/564 → 69/562**, arm64 `-O2` **290/555 → 46/555**, x86_64 `-O1` 31/565 → 30/562 (unmoved; the population differs by the dropped file). The remaining `-O2` moves are the arena's own effect, not the missing pass. The other nine keys' `-O2`/`-O3` columns are still at P4's reading and want re-measuring.
- **Done — the JIT seam, defect 3.** `selfhost-jit` passes under `MCC_RIR_PROD=1` with the `ast_rir_seam` carve-out deleted; the diagnosis and the two banked negatives are under defect 3 above. **All three of P4's blockers are closed.**
- `tests/exec/optimizer/*` and the 26 `optfire*` cells are the sensitive instrument for "a pass fired differently". **All 26 pass under `MCC_RIR_PROD=1` at HEAD**, `chainstore` included; the list of movers this line used to carry is closed. They are the right regression suite for each of the steps above; do not rebank them, use them.

**Two measurement traps banked while doing this.**

- `ast_env_int(name, dflt)` returns `dflt` when the value parses `<= 0`, so no `MCC_*` gate read through it can be turned *off* from the environment. `ast_env_gate` is the one that honours `0`.
- A `cp -a` of a CMake build directory is **not** a reference build: `CTestTestfile.cmake` has the original binary directory baked in absolutely, so `ctest --test-dir <copy>` silently runs the *original* build's binaries. A pristine reference has to be configured from its own source tree (`git archive origin/main | tar -x -C …`). Comparing against the copy read 119 "pre-existing" failures that did not exist. And when comparing, prefer the same binary with the feature switched off — it is exact, and it is one run.
- `exec*/bound_global` fails for every configuration in a worktree whose absolute path is long: `bt_info.file` is `char[100]` (`src/mccrun.c:783`) and the truncated path prints as `bound_globa:7`. It is a property of the checkout path, not of any change.

*Gate as met: `-O0` unchanged; at `-O1`/`-O2`/`-O3` every one of 19,686 objects is byte-identical to the pre-cutover build with the switch off, and the per-key fallback list above is what the switch costs when it is on.*

**Still owed by P4, deliberately not attempted.** Flipping `MCC_REPLAY_IR_C2` on per target as each key reaches 100% on the `all` corpus (no key is there). Ungating `MCC_REPLAY_IR` — dropping the CMake option, removing the `REPLAY_IR` row from `GATES[]` in `tests/fuzz/runner.c`, making capture unconditional while verify stays gated (`rir_verify` costs +7.7% wall clock, 4.30 s against 4.63 s over 266 corpus compiles at `-O1`), and turning `rir_parity.cmake`/`rir_c3.cmake`'s 77 skip paths into hard failures. Both of those belong after the default flips, not before: ungating capture for every user build is only worth its cost once the arena is the producer.

### The object-move columns, all twelve keys

Same binary, `MCC_RIR_PROD` on versus off, `all` corpus minus `full_language.c`, `SOURCE_DATE_EPOCH` pinned. This is what the switch currently costs in changed code, and it is the column the array-extent work moved most (x86_64 `-O2` was 367 before promotion came back).

| key | `-O2` | `-O3` | population |
| --- | --- | --- | --- |
| arm-win32 / arm-wince | 32 each | 33 each | 518 |
| i386-win32 | 36 | 36 | 525 |
| arm | 38 | 39 | 553 |
| arm64-win32 | 39 | 39 | 521 |
| i386 | 42 | 42 | 559 |
| arm64 | 46 | 46 | 555 |
| arm64-osx | 47 | 47 | 557 |
| x86_64-win32 | 50 | 50 | 526 |
| x86_64-osx | 56 | 56 | 523 |
| x86_64 | 69 | 69 | 562 |
| riscv64 | 76 | 76 | 551 |

`arm-win32` and `arm-wince` agree at both levels, as they must.

**`-O2` and `-O3` are identical on nine of twelve keys, and differ by exactly one body on the three arm keys** — `tests/exec/pointers_arrays/array_in_struct_init.c`, which moves at `-O3` and not at `-O2`. The file's earlier claim that `-O2`/`-O3` are byte-identical on every counter was measured on five keys and is very nearly right; this is the one exception, and it is worth one look before the switch flips.

**Two traps in taking this measurement, both of which produced a wrong answer first.** Without `SOURCE_DATE_EPOCH` pinned, an embedded timestamp makes a file differ between its own two compiles, which inflated the arm `-O3` count from 39 to 40 and invented a second `-O3`-only body. And a per-file enumeration must skip a file the *off* leg fails to compile, or a pre-existing failure reads as a move.

**Done — a pre-existing riscv64 crash, and it was a units mismatch, not a float-register gap.** `tests/diff/complex_abi/complex_abi.c` aborted at **every** `-O` with `freg: Assertion 'r >= 8 && r < 16' failed`, switch off, and nothing measured it because the `rc` check excludes non-compiling files. It is not a complex defect at all — `harness.h` alone reproduces it, and the four-line minimum is `void f(int c){ register long a0 __asm__("a0") = c; __asm__ volatile("ecall" ::"r"(a0)); }`. A plain `long a0 = c;` with the same `"r"` constraint is fine.

`asm_parse_regvar` was corrected to return a **codegen** index, which is right for `VT_VALMASK`. But `asm-constraints.inc.c` then assigned `op->vt->sym->r & VT_VALMASK` straight into `op->reg`, which riscv64's constraint solver and `asm_gen_code`'s `mcc_ireg`/`mcc_freg` read as a **machine** encoding — `load(r=-10)` reaching `freg`. The shared prologue has always worked on x86_64 and i386 because there the two numberings coincide (0-7 = eax-edi); on riscv64 codegen 0-7 are a0-a7 but machine 10-17, and codegen 8-15 are fa0-fa7 against machine 42-49.

Fixed with an arch hook, `ASM_REGVAR_ASMREG`, identity by default so no other backend changes, and riscv64 mapping the two ranges and returning -1 for anything else — which reads as "no register asked for" and lets the allocator choose, the same philosophy the `asm_parse_regvar` split used. Verified by decoding the emitted bytes rather than by the absence of a crash: `ld a0,-32(s0)` / `ld a7,-40(s0)` / `ecall`, so the binding is honoured. riscv64 now compiles **552** files of the `all` corpus instead of 551. **Any other backend whose codegen and asm register numberings differ has the same latent bug**; arm and arm64 are worth a look.

### The `MCC_RIR_PROD=1` census — the remaining work list

Measured on a **short checkout path**, which matters: the 22 `exec*/bound_global` failures both P4 and the array-extent work reported as pre-existing are an artifact of a long worktree path (`bt_info.file` is `char[100]`). On `/home/llg/Projects/moderncc` the suite is **8255/8255 with the switch off**, so the prod-on comparison is exact and noise-free.

The census read **36 failures of 8255** and named five distinct compiler defects plus their selfhost consequences. **All six defects are now closed** — a sixth turned up that the census had no row for.

| body | suites | closed by |
| --- | --- | --- |
| `ptr_longlong_arith32` | 13 | the op-assign vdup taking an `AST_Load` over a pure address — modelled, not refused |
| `array_2d_iv` | 1 | the same one line; it was never a separate defect |
| `flt_eval_method` | 1 | the `c == 0` skip in `ast_fconst_record` — a shared-replay defect, not a model one |
| `bitfields_ms` | 11 | refusing a parented register-lvalue `Ref` in `rir_prod_take` — a **refusal**, not a model fix |
| `chainstore` | 2 | the arena honouring `ast_chainstore_env` |
| `selfhost-output-parity-O2/O3` | 2 | the do-while comma condition — the sixth defect |
**Deleting the JIT seam's carve-out widened the population.** The four `exec-search*` suites set `MCC_AST_SEARCH=1` and were excluded from the arena swap outright, so closing defect 3 exposed the same bodies there too — +12 cells at the time, every one of them a body already named, none new. Coverage arriving, not regression; they close with their bodies.


**Three defects, all invisible to C2 and all found by the runtime A/B.**

- **The dangling register lvalue.** `s->a += 6` on a bitfield leaves the arena's op-assign vdup slot as `Ref r=<reg>|VT_LVAL` — *dereference whatever is in that machine register* — because the `IR_OP_VPUSHV` clone excludes `VT_BITFIELD` targets. The register's producer is a **sibling's emission**, a dataflow edge no pass can see, so `ast_plan_promotion` promoted the pointer out of its frame slot, deleted the defining `mov -0x10(%rbp),%rax` and left the dependent `mov (%rax),%eax` reading whatever was there. It is **not MS-specific** — plain `bitfields.c` has the identical shape and escapes only because the recorder declines that body, so with the tree as producer it is never optimized at all. A 15-line repro is `struct S { unsigned x:12, a:4; } _s, *s = &_s; s->a += 6; ++s->a;`. `rir_prod_take` now refuses any arena holding a parented register-lvalue `Ref`, which is production-only (`rir_prod_take` returns NULL under `rir_env`) and therefore **cannot move a C2 counter by construction**; the body keeps the parser's bytes, which is what the tree does with it anyway. Cost over the 2198 bodies of the `all` corpus: `used` 2013 → 2004, `nomodel` 49 → 62. **This is a refusal, not a model fix** — it is also what makes `ptr_longlong_arith32` and `array_2d_iv` stop failing, and their underlying arena defect is untouched. Modelling it means making the clone byte-faithful for a bitfield target; see the banked negative.
- **The chainstore gate.** `rir_op_effect`'s `IR_OP_VSTORE` arm set the `chained` fbit unconditionally where `ast_hook_vstore` sets it only under `ast_chainstore_env`, so `ast_finalize_chainstores` fired even with `MCC_AST_CHAINSTORE=0` and the gate-off and gate-on objects came out identical — which is exactly what `optfire`'s `differ` mode calls "pass DID NOT FIRE". x86_64 and arm64 hid it because `ast_plan_promotion` has its own `ast_chainstore_env` block; i386 has no promotion machinery at all and riscv64's does not fire on that case, which is why only those two cells failed. With the gate honoured all four objects (prod × gate) match the prod-off pair byte-for-byte.
- **The do-while comma condition.** `while (c = (unsigned char)*p, c != '\0')` reached the arena as `If op=4 nc=2`, with the condition buried as the last statement of the held-store BasicBlock. The tree has `nc=3` — body, condition, prologue — and `ast_replay_bb`'s `op == 4` arm reads slot 1 as the condition and slot 2 as the prologue. `rir_if_safe`'s case 4 did not catch it because both slots were BasicBlocks and it only checks slot 0. It re-emitted the parser's bytes exactly and mispromoted by **one instruction** (`mov %ecx,%r15d` for `mov %eax,%r15d`). The body is `libmcc.c::dynarray_split`, and `#pragma comment(option, "-mms-bitfields")` in `bitfields.c` is the only test in the tree that reaches it — which is why the stage-1 mcc failed on exactly `bitfields.c` and `bitfields_ms.c` and nothing else. `rir_cf_cond` now parks the held stores in `rir_cfpfx`, which the `RIR_R_DO` rend already appends as child 2.
**A prediction was right for the wrong reason.** The census expected the seven `selfhost-*` cells to clear with the five bodies. They did — but `output-parity-O2/O3` were a **sixth** defect with no row, a stable self-compile miscompile that only that cell can see, because `fixpoint` compares stage2 against stage3 and a stable miscompile survives it. Treat `selfhost-output-parity` as its own row in any future census.

The instrument for this list is the runtime A/B, not the byte compare: compile and run every `tests/exec` file at `-O1` with and without the switch and diff stdout and exit code. C2 calls every one of these bodies clean, because it validates un-optimized emission. **Run it under the failing cell's own gates** — the three modelled bodies all read SAME at a plain `-O1`; `ptr_longlong_arith32` needs `-O2`, `array_2d_iv` needs `-O2 MCC_AST_OPASSIGN=1 MCC_AST_IVSR_PTR=1`, and `flt_eval_method` needs `-O1 MCC_AST_TEMPLATES=0 MCC_AST_PROMOTE=1` (neither gate alone reproduces it). The gate sets are on the suite's `set_tests_properties` line in `CMakeLists.txt`.

**P5 readiness, measured rather than assumed.** The 45 `ast/replay-*` cells — the fixture suite the plan says survives the recorder and becomes Replay_IR's regression suite — pass **100% with `MCC_RIR_PROD=1` as well as with it off**. They assert `[ast-replay]`/`[ast-promote]`/`[ast-inline]` markers, which are producer-agnostic, so that inheritance is confirmed and not merely hoped for. Whatever else P5 has to do, it does not have to rewrite those 45 cells.

### P4 is complete — Replay_IR is the production arena by default

`MCC_RIR_PROD` defaults to **1**. `MCC_RIR_PROD=0` still turns it off, and that escape hatch should live until P5 lands.

What the flip rests on, all measured on a short checkout path with nothing else running:

- **Full `ctest` 8255/8255 with the default on**, and 8255/8255 with it off. The census that read 36 failures reads none.
- **The arena drives 91.5% of bodies** — over the whole `all` corpus at `-O2` on x86_64, `used=2011 fallback=11 skip=176` of 2198. The other 8.5% keep the parser's bytes, which is the per-body fallback working as designed, not a defect.
- `used` is 2011 and not the 2004 the bitfield refusal measured alone: the op-assign modelling recovered seven bodies the refusal would have declined. **The two fixes compose better than either half**, which is the argument for landing modelling rather than refusals wherever it is affordable.
- **C2 boards unmoved**: `exec` gap 10 over twelve keys, `all` gap 104. C2 runs under `MCC_REPLAY_IR=5`, which disables production, so this is the control.
- **`-O0` A/B unmoved** on all twelve keys, objects and counters, `arm-win32 == arm-wince`. The recorder never ran at `-O0` and still does not.
- `tracegate`/`schemagate`/`targetgate` clean; four side configurations green in distinct build dirs.

**A weak check worth not repeating.** "Compile one file with and without the switch and diff the object" proves nothing — only 69 of 562 files differ at `-O2` on x86_64, so most files are identical either way and read as "the flip did not take effect". Verify against a file already known to differ, or against the `[rir-prod-total]` counters at `MCC_RIR_PROD=2`.

### P5 — delete the recorder — blocked, and on exactly one thing

**The premise "nothing consumes the recorder's tree" is false, and the counter-example is one line.** `ast_replay_ok(ast_cur)` at `src/mccast.c:16553` is evaluated on the **tree**, and it is the arena's admission gate: `rir_prod_take()`'s arena is adopted only for a body the *recorder* also came out replayable on, and is otherwise freed with the verdict `noreplay`. `ast_replay_ok` is `!ast_bail && !ast_desync && ast_vn == 0 && ast_cf_top == 0 &&` root-has-a-child, and the first four terms are recorder state. With the recorder gone the predicate collapses to "the arena has a body", which is true of every arena the pre-flight admitted — so **the deletion widens the optimized population by every body the recorder declines, and those bodies have never been through a single pass.**

There is a second consumer, and it turns out to be free: when the pre-flight refuses (`nomodel`), `ast_cur` stays the tree and **the tree is optimized and re-emitted**. That path is not a fallback to the parser's bytes, contrary to how P4 reads.

**Measured at HEAD on x86_64 over `find tests -name '*.c'`, 657 files.** Per-body verdicts at `-O1` (`MCC_RIR_PROD=2`), 2503 bodies: `used` 2244, `fallback` 22, `nomodel` 80 (3.2%), `noreplay` **157 (6.3%)**. The two halves of the deletion were then simulated separately against the same binary with `SOURCE_DATE_EPOCH` pinned — files whose object moves, of files that compiled:

| what the deletion changes | `-O1` | `-O2` |
| --- | --- | --- |
| widen — adopt the arena on its own pre-flight, ignoring the recorder's verdict | **36/562** | **67/562** |
| narrow — drop the tree as the `nomodel` producer | **0/565** | **0/564** |
| both, i.e. the P5 end state | **36/563** | **66/561** |

**The narrow half is free; the widen half is the whole cost.** So exactly one property of the recorder is load-bearing — its per-body decline verdict, which keeps 6.3% of bodies out of the optimizer entirely. Everything else P5 lists really is dead.

**The three hangs are closed, and they were three unrelated defects, none of them a Replay_IR modelling error.** The whole corpus now compiles under `MCC_RIR_ONLY=1` on all twelve keys at `-O1` and `-O2`, 657 files, no hang and no crash; the three files run five times each at both levels on every key with zero failures. What they turned out to be:

- **The range-designator element type — a dangling stack `Sym`.** `decl_designator` (`src/mccgen.c:13391`) builds a stack-local `Sym aref` for `[a ... b] = x`, points a stack-local `CType` at it, pushes that on the vstack and calls `ast_hook_bail()`, which is why the recorder declines the body. Replay_IR had no sibling, so the arena stored `&aref` as its `type_ref` and replay dereferenced a dead frame — ASan names it in one run as a stack-use-after-return in `is_complex_type` from `vstore` in `ast_replay_bb`. **`rir_snap_types` does not save it**: its guard is `if (!rir_env) return;`, so the snapshot table is inert in production. Turning that guard into `rir_env || rir_prod_env` does fix the two files, and **it is a banked negative** — `rir_xt[]` is reset per body by `rir_reset` while an arena retained for inlining outlives it, and `ast/replay-inline` goes 42 → 33. The landed fix is the recorder's own shape: `rir_hook_bail()` sets a flag that `rir_prod_take` refuses on, which is production-only and cannot move a C2 counter. Closes `struct_init.c` and `range_designator_copy.c`. Modelling this properly needs snapped `Sym`s with the arena's lifetime, not a per-body table.
- **`cleanup.c` was not a defect at all — it was `O(n²)`.** `main` expands `INCR_GI7` to **65536** cleanup blocks, which is a legitimate 524,519-node arena with ~49,000 distinct local slots. `ast_du_find` was a linear probe over a fixed 2048-entry table, and on overflow `ast_du_state` went `-1` and *every* escape query fell back to `ast_cprop_escapes_scan`, a full arena walk — so `cprop`, `cse` and `dse` each became quadratic in node count. Every stack sample during the hang is in that scan. The table now grows and is hashed; the file compiles in 0.6 s. The results are identical either way by construction — that is what `MCC_CONFIG_AST_SHADOW`'s `ast_du_diverge` asserts — so this moves no byte anywhere.
- **`MCC_AST_OPT_LIMIT=0` was a false lead on all three.** It "fixes" `struct_init.c` and `range_designator_copy.c` only because with no pass firing there is no `AST_PF_EMIT` and the parser's bytes ship; the dangling type is read by the *first* `ast_replay_body`. On `cleanup.c` it does not help at all, which the earlier reading did not check. Per-gate `MCC_AST_*=0` tables were also useless, for the recorded reason.

**Two more files, and the sweep is the only thing that found them.** The three were what x86_64 showed; `tests/exec/programs/grep.c::compile` and `tests/exec/features_c99_c11/atomic_counter.c::atomic_counter_test` abort at `-O2` on the **three arm64 keys only**, in `store`'s trailing `assert(0)` (`src/arch/arm64/arm64-gen.c`). A promoted local used as a store target is a bare register SValue, and arm64's `store` has no register-destination case — x86_64 and i386 have carried one all along and arm64's own `load` already had it. Added, and both files verified to produce byte-identical stdout under `qemu-aarch64` with the switch off and on. **Sweep every key before believing a fix**: two of the three original files are clean on arm64 at both levels and would have read as "already fixed" there.

**And a sixth, which only `ctest` found: a promoted store target loses its assignment.** `MCC_RIR_ONLY=1` newly admits `mccelf.c::mcc_write_elf_file`, whose `if (fd < 0 || (f = fdopen(fd, "wb")) == NULL)` is a `Store` *used as a value*, so it replays through `ast_replay_value`, which pushed the target and then the value and called `vstore`. With `f` promoted, the target is a bare register; `save_regs(0)` inside the call spills it, `save_reg_upstack` rewrites the vstack entry to the temp it just stored to, and the assignment lands in the temp while the promoted register keeps its stale value. The self-compiled stage-1 mcc then got a garbage `FILE*` and spun — the visible symptom was `selfhost-output-parity` hanging on `tests/exec/arch/riscv_asm.c`, 1,600 bodies away from the cause. `ast_replay_bb`'s *statement* arm has always avoided this by evaluating the value and then calling `ast_promo_write`; the two `ast_replay_value` arms now do the same, restricted to a value subtree containing a call — the only thing that can spill a pinned register, since `get_reg` and its `save_found` fallback both skip them. A 30-line repro is in the commit. **Banked negative: do not fix it by making `save_reg_upstack` skip pinned registers.** That is the tempting one-liner and it is semantically right, but it also deletes every *harmless* spill of a promoted register and moves **36 objects at `-O2`/`-O3`** with the switch unset. The narrow fix moves zero. And x86_64's `store` did not mask the register indices in its register-to-register modrm byte, so a destination of `r12`-`r15` encoded as a different register pair — unreachable while every such target was being spilled, fixed alongside.

**Done — what `MCC_RIR_ONLY=1` cost, and it was 3 of 8255 `ctest` cells, all the same non-defect.**

- **3 × `optfire`** — `optfire/opassign`, `optfire-arm64/opassign`, `optfire/default-landor_invert`. These were not defects: `MCC_AST_OPASSIGN` and `MCC_AST_LANDOR_INVERT` are both on the list of 14 recorder-shape gates with no reader outside the hook block, and with the arena as the sole producer they stop changing the output, which is exactly what `optfire`'s `differ` mode reports as "pass DID NOT FIRE". **The three cells are deleted**, one line each from `tests/optfire/differs.txt` and `tests/optfire/defstate.txt` plus their two `tests/optfire/arch.txt` scoping entries. The two *gates* are still there and die with the recorder.

**Done — `dead_code.c::main`, and it was never a Replay_IR defect at all.** The 19 `exec*/dead_code` cells were reading a **pass** bug that the recorder's own decline verdict had been hiding since the pass was written. `ast_sccp_scan` folds `if (const)` by keeping the taken arm and discarding the dead one, guarded by `ast_sccp_has_label(dead)` — and `ast_sccp_has_label_compute` counts only `AST_Jump` with `op == 4`, a *label* definition. `op == 2` is a **case** marker and `op == 3` is **default**, and a `case` inside a dead arm is exactly the thing that turns `nocode_wanted` back off (`gind()` clears it), so the code after it is live. `switch (i) { if (0) { ... case 41: printf("caseok"); } }` therefore lost its arm, its case entry and its body.

Two things it teaches. **The producer was irrelevant**: a 25-line reduction of the two switches reproduces identically under `MCC_RIR_PROD=0`, i.e. with the recorder's own tree as the arena. `dead_code.c::main` escaped only because the recorder declines *that* body, which is the "class that only Replay_IR can reach" one more time — a latent shared defect that costs nothing until the population widens. And **the earlier reading that "the first replay is faithful and the re-emit is not" was right about the symptom and wrong about the cause**: the re-emit is not idempotent *because the arena it re-emits is not the arena the first replay saw*, the pass having deleted the case markers in between. `MCC_AST_SCCP=0` did not name it because there is no such gate — SCCP is a strategy in `ast_run_strat_cycle`, and the only `MCC_AST_*` that turns it off is `MCC_AST_TEMPLATES=0`, which turns everything off. Confirm a pass with `MCC_AST_REPLAY_DUMP=1` and read the `[ast-*]` counters; do not trust a gate name that produces no change in them.

The fix is a second memoized predicate, `ast_sccp_has_case`, consulted beside `ast_sccp_has_label` at the one site that *deletes* a subtree. **It must not widen `ast_sccp_has_label` itself**: five of that predicate's six call sites are loop- and switch-shaped conservatism guards, and one of them (`ast_cprop_switch_meet`) is handed the switch node itself, whose body always holds case markers — widening there disables switch cprop outright. `ast_sccp_has_case` also stops at a nested `AST_If op=6`, because an inner switch's cases are internal to it and deleting the pair together is sound. Cost with the switch unset: **zero objects** over 19,425, so no body in the corpus but this one has the shape.

**`MCC_RIR_ONLY` is the switch, and it now defaults to 1** (`src/mccast.c:2040` — the switch is in `mccast.c`, not `mccrir.c`). On, `ast_func_end` adopts `rir_prod_take()`'s arena on the pre-flight alone and keeps the parser's bytes when the pre-flight refuses — exactly the state the deletion produces, measurable while the recorder is still there to be compared against. It is the same shape `MCC_RIR_PROD` had for P4, and it exists so that P5 does not have to be a 3,000-line deletion and a behaviour change in one commit. `MCC_RIR_ONLY=0` still turns it off and that escape hatch should live until the deletion lands. `MCC_RIR_ONLY` is inert under `MCC_REPLAY_IR` and `MCC_AST_VERIFY`, because `rir_prod_env` already is.

**The order P5 now has to run in.**

1. **Done** — the three hangs, plus the two arm64 aborts and the self-host miscompile the wider sweeps turned up. The whole corpus compiles under the switch on all twelve keys.
2. **Done** — `dead_code.c::main` (the SCCP case-marker bug above) and the riscv64 self-host abort (the indeterminate spill `r2` below). `MCC_RIR_ONLY=1 ctest -j 8` is **8252 of 8255**, and the three are the `optfire` cells that die with the gates they measure.
3. Justify the moves the switch makes, the way P4 justified its own — the instrument is the runtime A/B (compile and run every `tests/exec` file, diff stdout and exit code), not the byte compare, because the byte compare cannot see a pass reading a type off the arena. The runtime A/B itself is **252 of 252 identical at `-O1` and at `-O2`**, switch on against switch off, stdout and exit code both. The board it has to justify, same binary switch on versus off, `all` corpus minus `full_language.c`, `SOURCE_DATE_EPOCH` pinned, as `-O1` / `-O2` over the population:

| key | `-O1` | `-O2` | population |
| --- | --- | --- | --- |
| x86_64-win32 | 25 | 51 | 526 |
| arm-win32 / arm-wince | 27 each | 33 each | 518 |
| i386-win32 | 28 | 34 | 525 |
| x86_64-osx | 30 | 53 | 523 |
| arm | 35 | 41 | 553 |
| x86_64 | 36 | 67 | 564 |
| i386 | 36 | 42 | 559 |
| arm64-win32 | 36 | 65 | 521 |
| arm64-osx | 45 | 76 | 557 |
| arm64 | 47 | 79 | 555 |
| riscv64 | 83 | 85 | 552 |

x86_64 reads 36 and 67 against the 36/562 and 66/561 P5 measured by simulating the widen half alone, so the switch is doing exactly and only what that simulation said it would. `arm-win32` and `arm-wince` agree at both levels, as they must.
4. **Done** — `MCC_RIR_ONLY` is on by default and the three `optfire` cells are deleted. The switch-off object A/B is **19,425 of 19,425 byte-identical over twelve keys × three `-O` levels**, and `ctest` is 8252/8252. The measurements are written up under **The `MCC_RIR_ONLY` flip, as measured** in the Handoff.
5. *Then* delete. **"With the switch already on, the deletion moves nothing by construction" was a prediction, and the measurement falsified it** — see the item below. It moves exactly one file.

**The deletion has a second reader of the recorder's decline verdict, and `MCC_RIR_ONLY` never bypassed it.** With the 1,618 lines of hooks removed, the twelve-key object A/B against the same corpus reads **19,425 compared, 12 differ** — one file, `tests/exec/statements/scopes.c`, on all twelve keys and at **`-O3` only**. The reference leg is exact: the pre-deletion binary under `MCC_RIR_ONLY=1`, which is the flipped state by construction.

The cause is not the arena. `MCC_RIR_PROD=2` reads `used=15 fallback=0 skip=0` on that file in both legs, so adoption is identical. The pass counters are what move: `[ast-inline]` goes **2 → 6**, and the extra lines are `grafted f6`, `specialized f6 (1 const arg)` and `re-emitted main_6 (forward inline)`. **`ast_fn_inlinable` (`src/mccast.c:2516`) and `ast_reemit_retain` (`:2895`) both open with `... || ast_bail || ast_desync`** — the recorder's own per-body decline state. `MCC_RIR_ONLY` bypasses that verdict at the *adoption* site (`ast_replay_ok`) and nowhere else, so the `-O3` forward-inliner kept consulting it right up to the deletion. With the recorder gone those two flags are permanently 0 and the graft path widens.

This is the same finding P5 already made — the decline verdict was load-bearing — with a **second** consumer the inventory missed, and it is intrinsic to the deletion rather than something to preserve: `ast_bail`/`ast_desync` are recorder state and there is nothing left to consult. The widened graft is correct on the one body that moves: `scopes.c` compiled `-O3` and run produces **byte-identical stdout and exit code** either side. Justify any further widening the same way — the runtime A/B, not the byte compare, because a pass reading a type off the arena is invisible to the byte compare.

**Done — the riscv64 self-host abort, and `r=50` is `VT_LOCAL`, not an asm register.** `selfhost-riscv64-docker` aborted under the switch at `freg: Assertion 'r >= 8 && r < 16' failed`, from `store(r=50)` under `ir_cap_store`. 50 also happens to be what `asm_compute_constraints`' `'f'` case allocates last, which makes this look like the `ASM_REGVAR_ASMREG` units bug wearing a different hat. It is not: `VT_LOCAL` is `0x32` and `VT_VALMASK` is `0x7f`, so `store(p->r & VT_VALMASK, &sv)` in `save_reg_upstack` (`src/mccgen.c:1921`) prints exactly 50 when `p` is an ordinary memory lvalue. **Read the constant before believing a register number.**

Why that entry was reached at all is the real defect, and it is the *second* half of a trap `rir_op_effect`'s `IR_OP_STORE` arm already had a comment about. `save_reg_upstack` builds its spill target as a bare stack `SValue` and sets only `type.t`, `r`, `c.i` and `sym` — `type.ref`, `c.q.hi` and **`r2`** are whatever the frame held. `ir_cap_store` snapshots that struct verbatim, and `rir_leaf_slot` records `sv->r2` into the arena's `wide_r2` for any value below `VT_CONST`, so replay pushed a `VT_LVAL|VT_LOCAL` leaf carrying `r2 = 0`. `save_reg_upstack`'s own `p->r2 == r` arm then matched it while saving register 0 and tried to spill a memory lvalue. `type.ref` had already been sanitized at this site (it is interned through `rir_ptr_sym`); `r2` now is too, at the same place and for the same reason. `ast_sv_hi` was never exposed — it returns 0 unless the SValue is a pure constant, which is the shape of guard `r2` was missing. **The recorder has the identical unguarded clamp in `ast_finalize_leaf` and is safe only because it is fed real vstack entries, which `vsetc` initialises.** Cost: **zero** C2 counters on either corpus and zero objects with the switch unset.

*Gate as met for the switch itself, all with `MCC_RIR_ONLY` unset and re-measured after the two fixes above: object sha256 A/B against a clean `origin/main` build over `find tests -name '*.c'` × twelve keys × `-O1`/`-O2`/`-O3` is **19,425 of 19,425 identical, 0 differ**, with exit-code parity on every one of the 7,884 file×key pairs so nothing dropped out of the population silently; `C2_NO_EXTRA=1 O0_AB_CHECK=1 tools/o0_ab.sh bc2 all` passes on all twelve keys with `arm-win32 == arm-wince`; the `C2_CORPUS=exec` board reads gap **10** and `C2_CORPUS=all` reads **104**, both unmoved row for row; a full `ctest -j 8` on a short checkout path is **8255 of 8255 with no failures**; `tracegate`/`schemagate`/`targetgate` clean and `src/mccrir.c` still has no `MCC_TRACE`; the four side configurations build green in distinct directories. Under the switch, the whole corpus compiles on twelve keys at `-O1` and `-O2` with no hang, no crash and no new failure, and `selfhost-riscv64-docker` passes.*

**`selfhost-riscv64-docker` does not need docker and should not be run through it.** The cell's script takes a native `qemu-riscv64` path first and only falls back to a container, and that path needs `$root/cmake-cross/riscv64-libmccrt.a` — a plain `bc2` built `-DMCC_ENABLE_CROSS=ON` has every cross runtime in it, so a `cmake-cross` symlink pointing at the build directory is enough to turn the cell from SKIP into a real run. A worktree without one reports SKIP and the cell is invisible; that is how this abort survived a full `ctest`. `selfhost-arm64-native` is gated the same way. Like `vendor`, a `cmake-cross` **symlink** is not matched by `.gitignore`'s `/cmake-cross/`, so never `git add -A`.

**The inventory, verified against the tree at this commit** (the line numbers in earlier revisions of this section had all moved): the hook bodies are `src/mccast.c:2318-4023`, 1,706 lines; 76 declarations in `src/mccast.h` and 124 call sites in `src/mccgen.c`; the shadow vstack (`ast_vs`, `ast_cf_if`/`ast_cf_savebb`/`ast_cf_top`, `ast_vn`); `ast_bail`/`ast_desync`/`AST_SET_BAIL`/`AST_SET_DESYNC`/`ast_gap_note`/`ast_replay_ok`/`ast_try_active`; the recorder-shape `MCC_AST_*` gates in `ast_configure` (14 have no reader outside the hook block: `CALL_NORETURN`, `CLEANUP_RET`, `CONVERT_GV`, `FNEG`, `INDIRECT_LOAD`, `INT128`, `LANDOR_FOLD`, `LANDOR_INVERT`, `LDOUBLE`, `NOCODE_CALL`, `OPASSIGN`, `REGPAIR`, `TERNARY_DISCARD`, `VOIDRET_EXPR` — and `ast_jit_guard_env` at `src/mccast.c:1384` is already dead today, declared and never assigned); the 4 `tests/ast/verify-baseline/` files; `ast-verify-ratchet-{O1,O2,O3}`, `ast/treecheck`, `ast/tracediff`, `tools/gate-ledger.sh`'s recorder half; `MCC_AST_INT128=1` in `rir_parity.cmake`'s forced-`-O0` env; and `MCC_RIR_PROD` and `MCC_RIR_ONLY` themselves.

**Six things inside the hook block are not the recorder's and must stay**: `ast_label_id` and `ast_label_forget` (`src/mccgen.c` calls both, and `rir_hook_goto`/`rir_hook_label`/`rir_hook_cleanup_thunk` are the reason the id exists at all), `ast_sv_hi` and `ast_cmp_invert_late` (three and one call sites in `src/mccrir.c`), `ast_bad_type` (`src/mccast.c:7037`, in the passes), and `ast_func_has_asm` (`src/mccrir.c:2541`). `ast_bad_vtype`/`ast_wide_vtype` survive only as long as `ast_try_active`'s `ast_ret_bad` term does.

**The 44 `ast/replay-*` cells survive and become Replay_IR's regression suite for free** — they assert `[ast-replay]`/`[ast-promote]`/`[ast-inline]` markers, which are producer-agnostic. So do the 20 `asttool` suites, which never touch the recorder.

**The compile-time dividend cannot be measured until the deletion happens.** `MCC_RIR_ONLY=1` still builds the shadow tree; it only stops reading it. Whatever the recorder costs per compile is still there, and the first honest reading of it is the deletion's own before/after.

### P6 — split and rename

`mccast.c` (19,082 lines) splits along the boundaries the map already found: arena + replay + hashing (`85-1117`, `5640-7082`, `14413-14494`), the passes (`7083-14412`), slice/search/JIT infrastructure (`16076-18053`), and the drivers. Then `ast_*` → `ir_*`, `AST_*` → `IR_*` across the seven files and `tools/asttool.c`. Rename the things that were never AST while the diff is already open: `ast_data_all_zero`, `ast_strpool_find_or_add`, `ast_pinned_regs`, `ast_alloc_loc`, and the six `ast_*_env` codegen gates are all genuine compiler machinery carrying the prefix by accident. `tools/targetgate.c:3-7` whitelists `src/mccast.c` by name and needs the new unit names. Run `./cmake-debug/tracegate src` and `./cmake-debug/schemagate src` before every push, and keep the new units free of any `MCC_TRACE(` call for the reason recorded above.

## External suites: the gcc and llvm C tests over a self-hosted `-O3` mcc

The compiler under test is `mcc` self-hosted at `-O3` — `tools/selfhost-o3.py cmake-release cmake-release/mcc-o3 -O3` compiles `src/mcc.c` with the release `mcc` and links with `mcc` itself (its runtime supplies the x87 long-double helpers GNU ld cannot resolve). `tools/selfhost-fixpoint.py cmake-release --opt=-O3` is clean: `o1 == o2 == o3`, 2,921,935 bytes, byte-identical across all three stages, so the `-O3` self-host is a fixpoint and not merely a build that happened to link.

`tools/xsuite.py` runs the two external trees and `tools/xsuite-report.py` reads its `results.jsonl`. The harness honors each suite's own directives rather than compiling everything blind: DejaGnu `dg-do`/`dg-error`/`dg-options`/`dg-require-effective-target`/target selectors on the gcc side, lit `RUN:`/`REQUIRES:`/`UNSUPPORTED:`/`-verify` on the llvm side. A test whose directives ask for something this host or this compiler cannot express — another architecture's intrinsics, `-mavx512f`, OpenMP, LTO, a `%clang_cc1 -triple aarch64` — is **skipped with the reason recorded**, never scored as a failure. 47,715 `.c` files in, 24,242 skipped by directive, **23,473 tests run at each of `-O0` and `-O3`**.

| suite | tests | `-O0` pass | `-O3` pass | skipped |
|---|---:|---:|---:|---:|
| `gcc.c-torture/compile` | 1,834 | 92.2% | 91.9% | 182 |
| `gcc.c-torture/execute` | 1,853 | 83.0% | 81.8% | 64 |
| `gcc.dg` | 13,396 | 76.5% | 76.4% | 5,518 |
| `c-c++-common` | 2,304 | 64.7% | 64.6% | 1,102 |
| `gcc.target/{i386,x86_64}` | 803 | 65.9% | 65.9% | 9,018 |
| `gcc.misc-tests` | 32 | 81.2% | 81.2% | 44 |
| `gcc.c-torture/unsorted` | 1 | 0.0% | 0.0% | 0 |
| `clang/test` | 2,972 | 70.0% | 70.0% | 7,725 |
| `compiler-rt/test` | 278 | 38.8% | 38.8% | 566 |
| **total** | **23,473** | **75.5%** | **75.3%** | **24,242** |

Most of the failing column is a **known feature boundary, not a defect**: 952 `__attribute__((vector_size))`, 1,305 implicit-declaration/implicit-int rejections the tests still rely on, 798 unresolvable GNU or clang headers, 276 nested functions, 217 inline-asm opcodes and constraints, 112 `_Complex`, 48 `#embed`/C23. That leaves 1,885 in `other` — the bucket worth mining, and the only one that is a work list.

### The `-O3` column is where the defects are

**46 tests pass at `-O0` and fail at `-O3`; 3 go the other way.** The list is `cmake-release/xsuite/o3-only-failures.txt`. Five are confirmed by hand, outside the harness:

1. `gcc.c-torture/execute/{20000412-2,conversion,medce-1}.c` — **wrong code at `-O3`**: each compiles clean and aborts at runtime (`rc=134`), and each runs to 0 at `-O0`. Three separate optimizer miscompiles, and the nearest thing to the P4 wrong-code classes this sweep found.
2. `gcc.c-torture/execute/pr68506.c` — **`internal compiler error: vstack leak (-1)`** at `-O3` only.
3. `gcc.c-torture/compile/930503-2.c` — mcc **segfaults** at `-O3`; at `-O0` it compiles.

Two more shapes in that list are optimizer *quality*, not correctness, and should not be filed as bugs: the `link_error0` idiom (`pure-1.c`, `compare-3.c`, `ieee/fp-cmp-{6,9}.c`) fails only because `-O3`'s inlining leaves a call `-O0`'s folding had already removed, and the `builtin-convert-*`/`builtin-ctype-*` rows want `__builtin_` forms mcc does not carry.

### Open items, in the order they are worth paying for

1. The three `-O3` wrong-code aborts above. Bisect each against the pass set the way P4's classes were bisected — they are the only runtime-visible optimizer defects in 23,473 tests.
2. The `vstack leak (-1)` ICE and the `930503-2.c` segfault: both are `-O3`-only compiler crashes, both single files, both cheap to reduce.
3. **142 `FAILEXE` at `-O0`** — programs that build and then abort with the optimizer off. `gcc.c-torture/execute/{20020412-1,pr41935,pr82210}.c` and `gcc.dg/{20050527-1,field-merge-6,gnu23-empty-init-1}.c` fault with `SIGSEGV`, the rest with the test's own `abort()`. This is a baseline-codegen list, not an optimizer list, and it is the larger of the two.
4. **1,386 `XPASS`** — tests carrying `dg-error`/`expected-error` that mcc accepted silently (664 `gcc.dg`, 441 `c-c++-common`, 248 `clang/test`). Each is a missing diagnostic. Low severity individually; as a set it is the honest measure of how much of C's constraint checking mcc does not do.
5. `gcc.dg/pr97459-{2,4,5,6}.c` hang at runtime at both opt levels (15s timeout), and `gcc.dg/O16384.c` never finishes compiling — the latter is pathological by construction and is not a defect.

**Two harness caveats, so the board is not read as stronger than it is.** A test expected to be rejected is scored `PASS` when mcc exits nonzero *for any reason*, so a file rejected over an unsupported extension rather than the intended constraint violation still counts. And `dg-output` text, `FileCheck` patterns and dump-scan `dg-final` are not verified at all — a `dg-do run` test is scored on its exit status alone. Both make the pass column optimistic; neither affects the `-O3`-only delta, which compares mcc against itself.

### Phase 1 — the `-O0` defects the sweep found, and what closed

The board above reads `-O0` and `-O3` as the harness ran them, but a test's own
`dg-options "-O2"` overrides the column: 111 of the 142 baseline failures are
genuine `-O0` defects, the rest only fail with the optimizer the test asked for.
`tools/xsuite.py --force-opt` strips the test's `-O` and re-runs at the column's
level; that is the list phase 1 works from.

**Closed, with `ctest` at 8096 of 8096 and the `-O3` self-host fixpoint still byte-identical:**

1. `__attribute__((aligned(N)))` on a **member** was replacing the natural alignment instead of only raising it. GCC lowers alignment only when `packed` is also present. `gcc.dg/align-1.c`, `c-c++-common/attr-aligned-1.c`, and `gcc.dg/bf-ms-layout-2.c`.
2. The same attribute on a **typedef** *may* lower alignment — the kernel's `unaligned_u64` idiom — and the naive form of fix 1 breaks it. The two cases are now distinguished by a new `SymAttr.type_aligned` bit set in `sym_to_attr` and cleared by an explicit declarator attribute. `tests/diff/parts/legacy_expr.h`'s `aligntest9` is the regression that catches this.
3. `__alignof__` applied to a **typedef name** dropped the typedef's alignment entirely (`__alignof__(T)` read 8 where the type was `aligned(16)`). The type-name branch of `unary` now carries the parsed alignment out through `gen_sizeof_parsed_align`.
4. A union whose only member is a zero-width bitfield took that member's alignment; GCC gives it 1. `gcc.dg/empty1.c`.
5. Eight missing `__SIZEOF_*` predefines — `SHORT`, `FLOAT`, `DOUBLE`, `LONG_DOUBLE`, `SIZE_T`, `PTRDIFF_T`, `WCHAR_T`, `WINT_T`. Tests gate whole bodies on these, so the absence read as a silent pass-with-nothing-executed rather than an error. `gcc.dg/torture/pr58416.c`.
6. `__INCLUDE_LEVEL__` was undefined. `gcc.dg/cpp/strify2.c`.
7. `//` is not a comment in strict ISO C89/C90 — `1 //**/ 2` is `1/2` there, and mcc read it as `1`. Gated on `std_strict_ansi`, so `-std=gnu89` keeps the extension. `gcc.dg/cpp/pr61854-1.c`, `-5.c`.

**Converted from silent wrong code to a diagnostic:** a struct or union member
with a variable-length array type. mcc accepted `struct S { int a[n]; }`, computed
a *static* size for it (7 read as 8, and `{int a; char b[n]; int c;}` read as 16
with `c` overlapping `b`), and the resulting binaries segfaulted or corrupted the
frame. Eleven tests were failing this way — `20040423-1`, `20041218-2`, `pr41935`,
`pr82210`, `gnu23-empty-init-1`, `packed-vla`, `vla-stexp-*`, `typename-vla-*`,
`pr99122-2`. They now fail to compile with a named error instead of building a
wrong program. `tests/cli/cases.h`'s `pedantic_diagnostics` case banked the old
accept-and-miscompile behaviour and moves with it.

**The feature that replaces that diagnostic** is runtime-sized structs, and it is
the largest single item the sweep found: the member's size, every offset after it,
the struct's own size and any copy of it all become runtime expressions. The
machinery for the scalar case already exists — `vpush_type_size` reads a VLA's
size out of a frame slot (`type->ref->c`) — so the shape of the work is to give a
struct carrying a VLA member the same treatment, plus `alloca` at declaration and
runtime offsets in member access. Nineteen tests ride on it, `pr51990`/`pr99122-1`
among them, and it is phase 2's first entry.

**Still open at `-O0`, largest clusters first:** `hardbool` (14, a GCC 14 type
attribute), preprocessor conformance (~10 — digraph spelling must survive
stringification, C90 pp-number rules, comments inside skipped groups),
`__builtin_object_size` and its dynamic form (7), `scalar_storage_order` (4),
`_Complex` division checks (5), and the varargs ABI for over-aligned and
`__int128` arguments (`pr92904`, `stdarg-3`).

### Phase 3 — the `-O3` column, and the four defects behind it

The `-O3`-only list was 46 tests. Five were confirmed by hand as wrong code or
a compiler crash; the causes turned out to be four, and closing them took the
list to **30**, the `-O3` ICE count from 10 to 5, and the runtime-abort count in
`c-torture/execute` from 30 to 23. Re-measured over the whole tree from this
commit: **20,513 tests per column, 76.4% at `-O0` and 76.3% at `-O3`** — the two
columns are now within 28 tests of each other, against 39 before.

1. **`ast_sym_defer` chained deferred syms through `sym->next`** — the same
   field that holds a struct's member list. Deferring a function-local struct
   type turned its field chain into the deferred list, and
   `x86_64_has_unaligned_field` then walked it until the stack ran out. The
   trigger is ordinary: any function-local aggregate passed by value to a call,
   because the replay runs at `ast_func_end`, after the body's scope has been
   popped. `930503-2`, `930530-1`, `pr26213`, and `pr68506`'s `vstack leak` ICE
   and `medce-1`'s wrong code fall out of it too. The deferred set is a plain
   array now and leaves `sym->next` alone. This one was live at every `-O` level
   and on both arenas — `MCC_RIR_PROD=0` did not move it.
2. **`AST_FB_CMP_INVERT_LATE` was applied only to a live comparison.** When both
   operands fold to constants there are no flags to swap and the inversion was
   dropped, so `!(f < 0)` with `f` known zero emitted **0**. `origin/main`'s own
   fix for this class (run the opposite operator, then swap) is the right shape
   and is what arm64's instruction pairing needs, but it leaves the folded case
   open; the merge keeps both halves. `pr110954-1`.
3. **`value64` truncates to 32 bits for every type narrower than a long long.**
   That is what the parser's folding wants, and it makes the function useless as
   a *does this constant fit in `signed char`* test — which is exactly what the
   narrowing pass asked it. It answered yes for -1634678893 and eliminated the
   `(signed char)` conversion. `ast_ii_cval_fits` does the real
   width-and-signedness check; both narrow paths use it. `pr70941`, `pr81814`,
   `20001009-1`.
4. **A call whose target lost its function type** dereferenced a NULL ref in
   `gfunc_call`. The replay already runs under an error sink with a byte-compare
   fallback, so raising the error instead lands in the designed path and keeps
   the parser's bytes. `Wsequence-point-2`.

**Reverted, and worth recording as a trap**: answering `__builtin_object_size`
subobject queries from the argument's declared array type. It matched gcc on the
plain cases and closed `pr101836_1`, but a trailing array's answer depends on
whether the member is flexible, not on its declared bound — the whole-tree
re-run turned up five tests that went from passing to aborting against the one
gained. `-1` is a permitted answer and a safe one. A real implementation needs
the flex-array rules.

**Still open at `-O3`**, from the re-run: `20000412-2` and `conversion` (both
`va_arg` of a variably-sized type), `990208-1` (a label address inside a
function the inliner takes), `pr85582-2`, `20000715-1`, `pr54877`, and the
`link_error` idiom in `bcp-1`/`pure-1`/`compare-3`, which is optimizer quality
rather than a defect — those tests require a fold `-O0` already performs and
`-O3`'s inlining undoes.

## Vector types

`__attribute__((vector_size(N)))` is implemented, on the shape `_Complex`
already established: an anonymous struct of N/sizeof(elem) fields carrying
`SymAttr.is_vector`, with the struct's alignment set to the vector's size.
Everything the aggregate machinery already does — `sizeof`, `_Alignof`, brace
initialization, assignment, parameter passing, return, compound literals —
comes for free and needed no new code. The operations are lowered element-wise
against a stack temp, which is correct but scalar: there is no SIMD codegen
behind this, and the ABI is the one mcc gives a struct of that size, not the
SysV vector classification, so a vector crossing a translation-unit boundary to
gcc-compiled code is not yet ABI-compatible.

What is covered: element-wise `+ - * / % & | ^ << >>` and unary `- ~`; the six
comparisons, which yield a signed-integer vector of the same width with -1 for
true as GCC does; scalar broadcast on either side; subscripting as both rvalue
and assignment target; casts between vectors of the same size and between a
vector and a same-size scalar; `__builtin_shuffle` with a one- or two-vector
source and a *runtime* mask; `__builtin_shufflevector` with constant indices;
and `__builtin_convertvector`.

Two type-system decisions worth recording. Vector types are **interned** by
(element type, count) in `gen_vector_type_cache` — without that, the type minted
for a comparison result is a distinct anonymous struct from the operand's and
every `v4si e = a == b;` fails as an incompatible initializer. And
`compare_types` treats two vectors of the same size and element count as
compatible, which is GCC's `-flax-vector-conversions` rather than its default;
the strict rule needs the element type carried in the comparison result, which
is the natural next step.

**The board**: of the 914 runs (457 tests × two columns) that the sweep had
blocked on `vector_size`, **777 pass**. The remaining 134 are a long tail —
30 parse shapes, 16 conversions, 16 other builtins (`__builtin_reduce_*`, the
`ia32` intrinsics), 10 that assign to a read-only location, 8 `_Complex`
element types, and 4 `__mode__(vector_size)`.

Two guards came out of this work rather than the feature: `||`/`&&`/`?:` on a
vector reached `gvtst` and crashed on a bogus jump chain, and now says *used
vector type where a scalar is required* as GCC does; and a vector whose element
count exceeds 1024 is refused rather than materialized, because the struct
representation makes `vector_size(1 << 29)` into 134 million field syms
(`gcc.dg/pr69973.c` hung the compiler before the cap).

### The baseline after vectors, and the computed-goto regression the re-run caught

Re-measured over the whole tree with the self-hosted `-O3` compiler:
**20,513 tests per column, 78.3% at `-O0` and 78.1% at `-O3`**, up from 76.4/76.3
before vector types. 778 runs moved from failing to passing; the `vector-ext`
failure bucket went from 914 to 8.

31 runs moved the other way and they split three ways. Twenty are **`XPASS`** —
tests that assert GCC rejects an invalid vector conversion, which mcc now
accepts because `compare_types` takes the `-flax-vector-conversions` rule. That
is the documented cost of the lax rule and it is a missing diagnostic, not wrong
code. Four are **timeouts under load** that pass when run alone. The remaining
seven were a real regression, and the re-run is the only reason it was found.

**Taking a label's address disabled nothing, so register promotion ran on the
body and the label definitions vanished from the output.** `goto *tab[n]` then
jumped into code that was never emitted, and the program hung. It reproduces in
nine lines:

```c
int f(int n) {
  static const void *tab[] = {&&a, &&b};
  int r = 0;
  goto *tab[n];
a: r = 10; goto done;
b: r = 20; goto done;
done: return r;
}
```

`-O0` and `-O1` print `10 20`; `-O2` and `-O3` hang. Bisected to the P5 merge —
`1d7963ac` passes, `1dce997a` does not — and it is not the deletion itself but
what the deletion exposed: with the recorder gone, nothing declines a body that
takes a label's address, and `ast_plan_promotion`'s guard only knew about
`ast_func_has_asm`. `ast_func_has_labeladdr` is set where `&&label` is parsed and
joins that guard. `920302-1`, `20040302-1`, `20050527-1` are closed; `990208-1`
still fails, and that one is the inliner rather than promotion — it is a label
address inside a function the inliner takes, which is the pre-existing entry in
the `-O3` list.

The lesson worth keeping: the P5 board was green on `ctest` 8073 of 8073 through
the whole merge. The external suites are what read the difference, and only
because the sweep was re-run over the whole tree rather than over the tests that
had failed before.
