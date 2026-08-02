# TODO

Finish Replay_IR: reproduce the parser's machine code byte-for-byte on every target at every `-O`, `-O0` included, then delete the AST recorder and the operation journal.

Two bars, both required. **Replay** (`rir_verify`) replays a captured body against the parser's own bytes. **C2** re-emits from the reconstructed arena and compares — the harder bar, and the one still open. Replay is at `faithful + empty == fn` on all twelve target keys at `-O0`/`-O1`/`-O2`/`-O3`, gated by the 48 `ast/rir-parity-*` cells.

**The corpus is now all of `tests/`, not `tests/exec`.** 657 files against 277, and about twice the bodies per key — 1980–2521 against 1146–1318. `tests/exec` was never a wrong measurement, but it was a narrow one: it reads a 14-body gap where the whole tree reads 160, and it says `c2bytes=0` everywhere where the whole tree finds byte divergences on ten of twelve keys. `C2_CORPUS=exec` still produces the historical board for continuity; `C2_CORPUS=all` is the default and is the bar.

## Scoreboard

Per key at HEAD, produced by `tools/c2_sweep.sh <builddir> <key> <opt>` against an mcc built `-DMCC_REPLAY_IR_C2=1` under `-DMCC_ENABLE_CROSS=ON`, run in place. One twelve-key corpus takes about 12 minutes; re-measure rather than copying a row forward.

Read `ok=` on every row before believing it. `rir_report` is an **atexit** handler, so a file that fails to compile still prints `[rir-total]` and still contributes bodies — counting `[rir-total]` lines is not an honesty check. The sweep counts only files that exited 0 and prints that count.

`C2_CORPUS=all`, 657 files, `-O1`. `gap` is `c2try - c2ok`:

| key | c2ok/c2try | gap | classes | arenahasheq | ok |
| --- | --- | --- | --- | --- | --- |
| x86_64 | 1943/1948 | 5 | 5 len | 1414/1988 | 522 |
| x86_64-win32 | 2305/2317 | 12 | 2 bytes + 8 len + 2 err | 1617/2364 | 527 |
| arm64 | 2113/2124 | 11 | 1 byte + 9 len + 1 invalid | 1506/2164 | 543 |
| i386 | 1937/1947 | 10 | 10 len | 1384/1987 | 519 |
| arm | 2052/2064 | 12 | 2 bytes + 9 len + 1 invalid | 1442/2104 | 541 |
| arm64-win32 | 2330/2346 | 16 | 3 bytes + 11 len + 2 err | 1638/2392 | 522 |
| x86_64-osx | 2447/2462 | 15 | 2 bytes + 11 len + 2 err | 1657/2508 | 571 |
| i386-win32 | 2309/2327 | 18 | 2 bytes + 14 len + 2 err | 1594/2378 | 526 |
| arm-win32 / arm-wince | 2269/2287 each | 18 | 4 bytes + 12 len + 2 err | 1576/2334 | 519 |
| riscv64 | 2034/2048 | 14 | 2 bytes + 10 len + 2 invalid, plus 7 skip | 1463/2094 | 539 |
| arm64-osx | 2456/2477 | 21 | 5 bytes + 14 len + 2 err | 1680/2521 | 566 |

`C2_CORPUS=exec` for continuity with every earlier board in this file, `-O1`, gap only: x86_64 **0**, x86_64-osx **0**, x86_64-win32 **0**, arm64 1, arm64-osx 1, arm64-win32 1, i386 2, i386-win32 2, arm 2, arm-win32 2, arm-wince 2, riscv64 3 (plus 2 skip). Fourteen over twelve keys, down from 18.

**Forced `-O0` tracks `-O1` to within three bodies on every key** (`C2_FORCE=1 C2_CORPUS=all … -O0`): x86_64 7, x86_64-osx 18, x86_64-win32 15, arm64 10, arm64-osx 21, arm64-win32 16, i386 9, i386-win32 18, arm 11, arm-win32 18, arm-wince 18, riscv64 13. `-O2` and `-O3` were byte-identical to `-O1` on every counter when last measured on five keys. C2 replays the arena's own emission and the optimizer passes are a separate question (C3), so this is what one would expect; it means the completion bar's "at every `-O`" is close to one measurement, but the -O0 column is now cheap enough to keep printing and it is NOT identical, so print it.

`arm-win32` and `arm-wince` share a define set and must read identically. Any sweep where those two rows differ has a harness bug, not a codegen one — the cheapest available check that a run measured what it thinks.

## Close the C2 gap

**The one rule this repo has paid for six times: fix at the USE site, never at the capture site.** A change where a primitive *consumes* an operand — the wrap `JOP_GENOP` performs, the `JOP_GV` wrap, the `JOP_VSTORE` and `JOP_STORE` arms, the argcast loop — is bound to the right node by the op's own recorded snapshot, and every such change has landed. A change where a cast or a lowering *happens* — a mark at `gen_cast`, a region bracketing `gen_negf` — perturbs the very stream `rir_verify` replays, and every such change failed. Read the banked negatives before designing another attempt.

Instruments, in the order they pay: `RIRDUMP=1` for ops either side of the blamed index with byte windows; `-DRIR_DBG_OPTRACE=1` plus `RIRDBG=<funcname>` for the `[ent]` entry stream (which prints each entry's `nocode` — that is how the dead-code class was found), `[stmt]` naming which entry pushed each BasicBlock statement, `[vst]`/`[gop]` for the vstore and genop admission inputs; `MCC_REPLAY_IR=6` for `[rir-dump]`/`[rir-diff]` node pairs against the tree. A structural `[rir-dump]` diff against the tree found nearly everything that closed.

`RIRC2TREE=1` drives C2 from the tree's arena instead. A body that diverges identically in both legs is a **shared replay** defect in `ast_replay_bb`/`ast_replay_value`, not a model defect; it still blocks the bar, but it is different work and it benefits both models. Caveat: `RIRC2TREE` falls back to the RIR arena when `ast_replay_ok(ast_cur)` is false, so re-confirm any individual body before attacking it as a replay defect.

### What closed, and the shape that worked

- **The inline-asm operand class.** `asm_instr` evaluates the operands, then `save_regs(0)`, then two `asm_gen_code` calls; only the last three were journalled, and `JOP_ASMGEN` replays off the parser's own vstack snapshot so its bytes were always right. The operand evaluation — pointer gv, deref, spills — was orphaned shadow-stack nodes nothing emitted. A `RIR_M_ASMOPS` mark taken immediately before `save_regs(0)` collects them into one `AST_OP_ASMOPS` statement whose replay calls `save_regs` itself. Closed `fancy_copy`, `fancy_copy2`, `sigaddset1`, `sigdelset1`, `memcpy1`, `memcpy2`, `mconstraint_test`, `other_constraints_test`.
- **The dead-code jump class.** `rir_op_effect` drops ops under `nocode_wanted`, but a break's arena `Jump` comes from a mark and a switch's dispatch jump comes from replaying the `AST_If` — neither is an op. `CODE_OFF_BIT` lives outside `RIR_NOEVAL_MASK`, so the mark filter let both through. Closed `optimize_out_test`.
- **`gv` on a sub-int lvalue.** `gv` on a char or short lvalue emits the widening load itself, and both the placement (inside a ternary arm) and the signedness (`(unsigned char)` leaves no op) were missing from the arena. Wrapping the operand in a `Convert` at the `JOP_GV` site closed `mt_workload.c::main`, `rev64_mt.c::main` on x86_64 and `corpus.c::str_hash`.
- **A compound assignment as a store-chain source.** `ast_finalize_chainstores` declined if either store was an `AST_OP_OPASSIGN`; only the second has to be a plain assignment. Shared-replay, so it moved both models. Closed `fuzz_next` and 39 divergences in `tests/fuzz/runner.c` on the x86_64 keys.
- The **code-free cast on a 32-bit target**, the same class **at a call argument**, and riscv64's **`gen_negf` spill** — all documented at length in earlier revisions of this file, all closed by widening the wrap a primitive already performs on its own operands.

### Banked negatives — do not re-pay for these

- **Marking code-free `gen_cast` calls**, three variants: at every such call wrapping the shadow-stack top → `c2ok 531/1146`, `c2invalid=517`, two core dumps; narrowed to a const-only `AST_Binary` outside every region → `c2err=3`, `faithful` down one; bound to the operand the marker's own vstack snapshot names → `faithful` holds but `c2err` 0 to 1-3 on every key and neither target body moves. Stamping the bound node's type instead of wrapping is worse again.
- **Bracketing `gen_negf` as `RIR_R_FNEG`** on the `RIR_R_INC` pattern **aborts the compiler**. Region markers are part of the captured stream.
- **Relaxing the `!lv` guard in `JOP_STORE` generally**: -30 `c2ok` on every key. The float restriction is what makes it safe.
- **Dropping the argcast wrap's `AST_Convert` skip** without the wider test **segfaults** every PE key.
- **Widening `rir_child_width_differs` itself** to the effective width: same `c2ok`, minus 14 to 20 `arenahasheq` on every key.
- **Retyping the cast chain at its conversion op** for `overflow_narrow.c::main`, two variants, both no-op. The `(int)` narrow in `(long long)(int)(v>>3)` emits no op, so there is no op to attach to. The conversion-op route is dead for the whole lost-intermediate class (`overflow_narrow`, `grep.c::pmatch`, `integer_promotion`): the intermediate has to be recorded as **new** data — a C2-only source-width field on the widening op, read by reconstruction and ignored by `rir_verify` so it cannot perturb replay.
- **Dropping every mark recorded with any `nocode` bit set**: -25 bodies. A label, case or default recorded with `CODE_OFF` set is usually the very thing that turns code back on. Narrowing that to `RIR_M_JUMP` only is still -13, because `gjmp_acs` sets `CODE_OFF` on the way out and so every break looks unreachable by the time the hook runs; the hook has to be handed `nocode_wanted` as it stood BEFORE the `gjmp`.
- **Reading `AST_FB_NOCODE` on every statement kind** rather than only `AST_If`: -25 bodies. Other kinds pack raw values into `fbits` — `ASMGEN` its vstack window, `MEMBER` its `VT_NONLVAL` — and alias any new flag bit. Any future `AST_FB_` read in `ast_replay_bb`'s statement loop must be kind-guarded for the same reason.

### Still open, largest first

Counts are divergences over the twelve keys on the `all` corpus.

- `bounds_stress.c::test16` and `::test17`, **24**, both on all twelve keys: `strcpy(q = alloca(strlen(demo) + 1), demo)` inside a call argument. The trial re-evaluates `strlen(demo) + 1` and calls `alloca` a second time instead of reusing the value. Nested call in an argument, so the same family as `struct_assign_test`.
- `fuzz/runner.c::triage`, `::interesting`, `::main`, **27**, on the nine non-x86_64 keys only — the x86_64 keys closed with the store-chain fix. On arm64-osx the trial emits one extra `ldr x0,[x0]` at a `genop`: an extra dereference the parser did not make.
- `statement_expr_test` and `local_label_test`, **14**, are one root and it is **located**. A void-effect call inside a statement expression becomes an arena statement only when the shadow stack drops it at the next reconcile, and the parser's `vpop` for that call lands AFTER the following `goto`'s and label's marks -- so the arena reads `Jump(goto)`, `Jump(label)`, `Invoke` where the parser emitted the call first. `local_label_test` has `heq=1`: the arena is structurally identical to the tree, so this is shared-replay ordering, not a model defect. Twenty-two lines reproduce it, no headers beyond a printf declaration:

  ```c
  void t(void) { int a; goto m1;
  m2:  a = 1 + ({ __label__ q1, q2, q3; goto q1;
                  q2: printf("aa3\n"); goto q3;
                  q1: printf("aa2\n"); goto q2; q3:; 1; });
       printf("a=%d\n", a); return;
  m1:  printf("bb2\n"); goto m2; }
  ```

  It is a `c2bytes` divergence at identical length, and `RIRDBG=t -DRIR_DBG_OPTRACE=1` shows it in one screen: the `[stmt]` line for the Invoke carries `ent=27`, the two `[stmt]` Jump lines carry `ent=23` and `ent=25`, and `[ent] 27` is the `vpop`. Disabling `rir_hold_inline` entirely changes nothing, so the hold is not it.

  There are **two** defects here, and the ordering one alone is not worth landing. Flushing an unparented effectful `AST_Invoke` off the shadow stack as a statement at the `RIR_M_GOTO` and `RIR_M_LABEL` marks closes the variant of the reproducer whose inner `__label__` names do NOT alias the outer ones -- and leaves the aliasing variant, which is what the real bodies are, exactly where it was. Measured over the whole corpus on x86_64-osx it is `c2ok`-neutral and costs one `arenahasheq`. Two notes for whoever lands it: `rir_drop` re-`rir_stmt`s any effectful node it pops without checking whether it already has a parent, so the flushed shadow entry has to be blanked to `AST_NONE` rather than left in place, or the body reports "nchild disagrees with sibling chain"; and the ASMOPS trick of leaving a parented node in place does not work here for the same reason.

  The second defect is that `rir_hook_label(t)` and `rir_hook_goto(tok)` are handed the label's **token**, and `ast_rp_label_get` keys on it. Two `__label__ l1` declarations in different statement-expression scopes are the same token, so the arena's labels alias where the parser's `label_find`/`label_push` scoping keeps them apart -- `ast_rp_label_floor` scopes only the inline-graft path. A token is not enough; the id has to be per label *instance*. The Sym pointer is unique while alive but `label_pop` frees local labels at each scope exit and the allocator recycles, so a bare pointer aliases again -- it needs a monotonic id assigned at `label_push` and dropped at `label_pop`, which means two more hooks. Note this also moves the tree's `ast_hook_label`, or `heq` falls.
- `full_language.c`, **42** over its seven keys, six bodies: `struct_assign_test` (call), `statement_expr_test` (jmp), `s7_9_iso646_test` (store, and a BYTE divergence not a length one), `local_label_test` (call), `coherency_test` (genop), `char_short_test` (cvt_csti). Of these, `struct_assign_test` and `char_short_test` are confirmed RIR-model defects (the tree leg gets them right or differs); the rest reproduce identically in the tree leg and are shared-replay work. `longlong_test` adds 5 more on the keys where it diverges.
- `rev64_mt.c::main`, **12**, still open on all keys — a different and much larger divergence (want 2056 got 2009) from the ternary one that closed in the same file's sibling.
- `ternary_op.c::tst_yarpgen`, **7**: a `JOP_STORE` register spill the parser makes mid-expression and the trial does not. Scheduling and allocation, not model shape — a single yarpgen expression with enough live values to spill, and the trial reaching that point with fewer live registers is the symptom. Expect it last.
- `s7_9_iso646_test`, **12** including `run_s7_9.c`: a pure ORDERING difference at identical length. The parser stores then loads the next condition; the trial hoists the load before the store and picks a different register for it. Same family as `AST_FB_STORE_ADDR_LATE`.
- `zero_bss.c::main` **5**, `smoke.c::main` **5**, `overflow_narrow.c::main` **5**, `struct_ret_variadic.c::mkv` **3**, `varargs.c::mix` **3**, `strpbrk.c::strpbrk` **2**, `integer_promotion.c::main` **2**, `struct_packed_indirect.c::main` **1**, `run_s7_28.c::s7_28_wconv` **1**, `run_s_annFGK.c::s_annFGK_annex_test` **1**.
- `overflow_narrow.c::main`, `grep.c::pmatch` and `integer_promotion.c::main` are the **lost-intermediate** class and share one root: the intermediate type of a cast chain is not recoverable from anything already captured. See the banked negative above for why the conversion-op route is dead.
- riscv64's 7 `c2skip` and 2 `c2invalid` on the `all` corpus, and its one remaining compiler abort (down from four; the other three were the `asm_parse_regvar` numbering bug).

## Raise arena fidelity

- Raise `arenahasheq`, 1680 of 2521 on arm64-osx. Measure as `arenahasheq` and as diffs-per-body falling, not as first-divergence classes disappearing — fixing divergence #1 only exposes #2. Classify by **structural** dump diff, not by node index: `ast_intention_hash` is a structural walk that hashes neither `type_ref` nor `ival` on a `Ref`, so the large Convert-versus-Ref index-transposition class and every `ref`-only diff are not fidelity defects and cost nothing to ignore. Live classes: field-only, extra-Convert-over-Binary, missing-Convert-over-Literal, tree-extra-Store. The last two are understood and not cheap — the tree builds cast chains where one push record recovers only one link, and `tree-extra-Store` is dead code under `nocode_wanted` that the tree records and the op filter drops.
- The extra-Convert-over-Binary class is **measured, not guessed**, and the measurement says do not touch it. Gating the whole argcast wrap on `RIR_M_ARGCAST`'s per-argument fired flag reads **+25 arenahasheq and -16 c2ok on every key**: the wraps the tree does not make are load-bearing for emission. The flag is therefore consulted only for the untyped-Load case it was added for. Any future attempt to close the class has to explain that trade first.
- Land explicit operand binding as a side-table keyed by op index, not a node field and not positional binding.
- Widen C3 pass equivalence past the field-identical population — the same work as raising arena fidelity, since the paired population *is* the field-identical population. Add the optimized-versus-optimized byte tap by replicating C2's whole save/restore prologue just before the `orig`/`orig_rel` frees in `ast_func_end`, where `ind` is the end of the optimized body: a different point with different live state. Report the coverage difference separately rather than as a mismatch, because the tree runs passes only where `ast_replay_ok` holds while Replay_IR accepts every body.

## Finish the capture path

- Phase F: un-embed `RirOp` from `JrnOp`. This is a design change, not a lift — Replay_IR must own an op stream with its own vstack buffer (the shape `rir_mvs`/`mvs_off`/`mvs_n` already has for marks) instead of reading `jrn_ops[]` and indexing `jrn_vs[]`. `jrn_snap_vstack` falls out of the same change: it has no `mccgen.c` call site to hang on, so it cannot be lifted independently.
- Discharge F against the same obligation every prior slice met: byte-identical `MCC_REPLAY_IR=5` sweep log and identical object sha256 at `-O0`, `-O1`, `-O2`, `-O3` and forced `-O0`. That compares capture against parser, tree and journal at once and is strictly stronger than cross-checking two capture paths against each other. Replace site by site; do not run both paths side by side.

## Keep the measurement honest

- Bank the corpus census against header resolution, never against a remembered number. `MCC_CONFIG_AUTO_MCCDIR` resolves mcc's own freestanding headers from **argv[0]'s directory**, so a compiler built into a scratch dir with no sibling `include/` silently loses `stdarg.h`/`stdbool.h`/`stdatomic.h`/`threads.h`; a build omitting `-DMCC_CONFIG_MCCDIR` finds no system headers at all and reads 476; a glibc sysroot resolves `<threads.h>` to glibc's rather than mcc's shim and costs 25 functions. Every one of these leaves `rc`, the file count and the failing-file list unchanged — the census is the only thing that moves, which is what makes it a trap.
- **Done** (`tools:`): `c2_sweep.sh` no longer selects `$BUILD/mcc` for a key the host is not. The native compiler carries no sysroot flags, but which key it IS depends on the host — x86_64 on a Linux host, arm64-osx on a mac — so asking for key `x86_64` on a mac silently measured `arm64-osx` with `rc` 0 and a plausible row. It now prefers an explicit `mcc-<key>` whenever one was built.
- The `all` corpus compiles 519–571 of 657 files per key. The rest are `dg-error` cases, host-specific `darwin/`, arch-specific `arch/` and files that need a driver; they are excluded by the `rc` check, not by a list, so a file that starts compiling joins the census automatically. `full_language.c` needs `-I <repo root> -DCC_NAME=CC_gcc` and enters only on the 7 keys where the C2 probe's own error does not abort the compile — `extra=` on the row says whether it did.
- Keep the C2 harness mirroring the tree's replay prologue exactly across `vstack`/`vtop`, `loc`, `anon_sym`, `ast_pinned_regs`, `ast_rp_bsym`, `ast_rp_csym`, `ast_rp_switch`, `ast_temp_frontier`, `ast_rp_nlabel`, `ast_fconst_i`, `ast_locrec_i`, `sym_free_first`, `ast_rp_asmops`. Leftover allocator state reads as a codegen difference; one omission, a dirty vstack, costs 194 bodies.
- Keep the `-O0` cells honest. `ast_replay_env` needs `optimize >= 1`, so `-O0` without `FORCE` journals nothing; the sweep's own `C2_FORCE=1` derives the 38 `o4 || optimize >= 1` gate names by regex over `src/*.c` per run, exactly as `rir_parity.cmake` does, and refuses to run if that regex finds none. Without it an `-O0` row reads `fn=0` and a perfect board over an empty population.
- Do not run the twelve-key sweep and `ctest -j8` at the same time. Seven `selfhost-fixpoint` cells fail under that contention and pass individually — a false red that costs a bisect.
- Extend the native-versus-cross cross-check to riscv64 and i386. arm64 has it, re-measured at HEAD in a `linux/arm64` container against `tests/exec`: native reads `c2ok=1179/1180`, cross reads `1169/1170` — the same gap of 1 over a population 10 bodies larger, because a native build resolves glibc's headers directly instead of through the stage3 sysroot. On the `all` corpus the two are **not** comparable and must not be put in one table: the native build compiles 556 files against the cross build's 543, `full_language.c` among them, so the native row carries divergences the cross row never had a chance to see. Pin the file set before comparing anything wider than `exec`. Neither i386 nor riscv64 can run natively on this host; both need silicon or a full-system emulator, not `qemu-user`.
- **Ten of twelve keys are confirmed host-invariant off macOS** — a second host stack reproduces the board. On a Windows x86_64 host, mcc built with the vendored winlibs gcc and `-DMCC_REPLAY_IR_C2=1` reproduced all five PE-key rows byte-exact, and the `arm-win32` ≡ `arm-wince` identity gate held natively. A WSL Ubuntu clone with the five ELF cross compilers and the four Gentoo stage3 glibc sysroots reproduced x86_64/i386/arm/arm64/riscv64 exactly on the same host. This is host-invariance, which is weaker than and distinct from the on-target-native check the line above wants.
- The PE `tgmath.h` shadowing is **not real** — settled on the macOS host, do not reorder any header lookup.
- Keep the fifteen files the repo-wide comment strip deliberately skipped, since in each the comment is the test payload and not prose. The ten `tests/diagnostics/dg-error/*.c` carry the expected diagnostic in a `/* dg-error: ... */` marker that `run_dgerror.cmake` greps out and hard-fails on when it is absent; `tests/exec/preprocessor/comment.c` is the comment-lexing permutation corpus; `tests/cst/kinds/comment.c` and `tests/cst/hashinv/spaced.c` exercise CST comment promotion and the H_s/H_t comment-invariance split; `tests/preprocess/asm/gas_comments.S` is the GAS comment corpus; and `tests/exec/programs/grep.c` is greped as its own input by the `{SELF}` golden, which expects the trailing vim modeline in the output. The strip also preserved line numbering in `tests/**` C sources because `tests/exec/goldens.h` pins diagnostics as `file.c:line:`.
- Keep `src/mccrir.c` free of any `MCC_TRACE(` call; `tools/tracegate.c` checks a file only if it contains one, so the first call pulls that file's ~250 unannotated branches into the checked set at once. Run `./cmake-c2all/tracegate src` and `./cmake-c2all/schemagate src` before every push.

## Land and delete

- Flip `MCC_REPLAY_IR_C2` on per target as each key individually reaches 100% on the `all` corpus, not all at once on the aggregate. Add the `tests/ast/rir_c2.cmake` gate with it. No key is there yet; `tests/exec` alone has three.
- Ungate `MCC_REPLAY_IR`: drop the CMake option, remove the `REPLAY_IR` row from `GATES[]` in `tests/fuzz/runner.c`, make **capture** unconditional while **verify** stays gated — `rir_verify` costs +7.7% wall clock on every compile, which is not a tax for every user build, and after deletion there is no tree column and no journal column left for a three-way comparison to read. `MCC_REPLAY_IR_C2` then deletes with its two `#ifndef` lines. Once ungated, `rir_parity.cmake` and `rir_c3.cmake`'s 77 paths are dead and must become hard failures, or a build that silently lost Replay_IR reports green.
- Delete the AST recorder and the operation journal together, and not before C2 is 100% on every key. The tree still exclusively carries the optimization passes, the loop transforms, `ast_intention_hash`/`ast_slice_ident_hash` and the AOT/JIT equivalence invariant; each must move to Replay_IR or be shown dead first. Retire rather than port the journal's `raw=`/`rawb=` breadth bucket — it is zero on 15 of 17 banked keys, and the two exceptions carry one stale row (`arch/arm64.c myprintf raw=14 rawb=688`) that the current compiler reports only in the baseline. Move `rir_dbg_on` out of `mccast.c` on the way.
