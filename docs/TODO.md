# TODO

Finish Replay_IR: reproduce the parser's machine code byte-for-byte on every target at every `-O`, `-O0` included, then delete the AST recorder and the operation journal.

Two bars, both required. **Replay** (`rir_verify`) replays a captured body against the parser's own bytes. **C2** re-emits from the reconstructed arena and compares — the harder bar, and the one still open. Replay is at `faithful + empty == fn` on all twelve target keys at `-O0`/`-O1`/`-O2`/`-O3`, gated by the 48 `ast/rir-parity-*` cells. C2 is at a **63-body gap** over the eleven distinct keys, down from 114, with three keys already at 100%.

## Scoreboard

Per key at HEAD, `c2ok/c2try (c2bytes+c2len+c2skip+c2invalid)`, full 278-file corpus, `c2err=0` everywhere. Reproduce with `tools/c2_sweep.sh <builddir> <key> -O1`, which needs an mcc built `-DMCC_REPLAY_IR_C2=1` and run in place. The whole twelve-key sweep takes about 30 seconds, so re-measure rather than copying a row forward.

| key | c2ok/c2try | gap | arenahasheq |
| --- | --- | --- | --- |
| x86_64 | 1146/1146 | 0 | 786/1182 |
| x86_64-osx | 1139/1139 | 0 | 785/1175 |
| x86_64-win32 | 1243/1243 | 0 | 852/1278 |
| arm64-osx | 1203/1205 | 2 (2 len) | 812/1241 |
| arm64-win32 | 1283/1286 | 3 (3 len) | 869/1322 |
| arm64 | 1168/1172 | 4 (1 bytes + 3 len) | 792/1208 |
| i386 | 1133/1142 | 9 (2 bytes + 7 len) | 769/1177 |
| i386-win32 | 1254/1263 | 9 (2 bytes + 7 len) | 854/1298 |
| arm | 1109/1119 | 10 (5 bytes + 5 len) | 759/1154 |
| arm-win32 / arm-wince | 1228/1240 each | 12 (5 bytes + 7 len) | 842/1275 |
| riscv64 | 1128/1142 | 14 (1 bytes + 8 len + 2 skip + 5 invalid) | 783/1179 |

The gap is **23 distinct bodies**. Fix per body, not per key — every remaining class is one source body seen from several keys:

| bodies | keys | op |
| --- | --- | --- |
| `atomic_gcc_style.c::main` | 6 | vstore |
| `ternary_op.c::tst_yarpgen` | 6 | store |
| `atomic_inlang_rmw.c::main` | 4+2 | vstore |
| `overflow_narrow.c::main` | 4 | load |
| `div_mod_shift.c::main`, `cmp_invert.c::main` | 4 each | jmpcond (bytes) |
| `signbit_inline.c::main`, `popcount_inline.c::ref_clrsb64` | 4 each | genop |
| `bitfields.c::main` | 3 | call |
| `variadic_macros.c::sum_impl`, `variadic_promotions.c::sum_ints` | 2+1 each | gv (bytes) + vstore |
| `grep.c::pmatch`, `runner.c::run_capture` | 2 each | genop |
| `utf8_string_literal.c::main` | 2 | call |
| `arm64.c::myprintf` | riscv64, arm64 | vstore, va_arg |
| `integer_promotion.c::main`, `struct_abi.c::vsum`, `struct_packed_indirect.c::main`, `variadic_promotions.c::sum_doubles` | 1 each | genop/jmpcond, vstore, call, vstore |

`arm-win32` and `arm-wince` share a define set and must read identically. Any sweep where those two rows differ has a harness bug, not a codegen one — cheapest available check that a run measured what it thinks.

## Close the C2 gap

Instruments, in the order they pay: `RIRDUMP=1` for ops either side of the blamed index with byte windows; `-DRIR_DBG_OPTRACE=1` plus `RIRDBG=<funcname>` for the `[ent]` entry stream, the `[stmt]` tap naming which entry pushed each BasicBlock statement, the `[vst]` tap printing the call-tail spill test's own inputs, and diffable `PARSE`/`C2` op streams; `MCC_REPLAY_IR=6` for `[rir-dump]`/`[rir-diff]` node pairs. A structural `[rir-dump]` diff against the tree found three of the four defects closed so far, each time in one body.

- Close the atomics pair, 10 of the 63: `atomic_gcc_style.c::main` on six keys and `atomic_inlang_rmw.c::main` on six. On arm64-osx the first is want 1280 got 1304, blamed at `op=vstore idx=156 win=[192,196)` with `firstdiff == firstblk == 192`, so the trial runs 24 bytes long from the first atomic block. `alloc_local_slot` and `rir_slotrec` are the only allocator the atomic aggregate lowerings reach, and that list is ordinal rather than position-keyed — check it before the arena.
- Close `ternary_op.c::tst_yarpgen`, 6 keys, want 460 got 452 on arm64-osx and short on every key. Blamed at `op=store idx=206 win=[364,368)`, a JOP_STORE register spill the parser makes mid-expression and the trial does not. This is the scheduling-and-allocation class, not a model-shape one: the body is a single yarpgen expression with enough live values to spill, and the trial reaching that point with fewer live registers is the symptom. Expect it to be the last one closed.
- Close `div_mod_shift.c::main` and `cmp_invert.c::main`, 4 keys each, both `c2bytes` at `op=jmpcond`, both absent from every 64-bit key. Comparison polarity on a 32-bit target; `AST_FB_CMP_INVERT_LATE` already models the parser's polarity act, so start by asking whether the mark's snapshot says to apply it here.
- Close `signbit_inline.c::main` and `popcount_inline.c::ref_clrsb64`, 4 keys each, `op=genop`. Both are the inline builtin expansions; `JOP_BITBUILTIN` and `JOP_SIGNBIT` already carry the expansion as one primitive with an `AST_Unary`, so the question is whether the 32-bit expansion is a different shape than the modelled one.
- Close `overflow_narrow.c::main`, the 4 that remain of the `load` class after the untyped-Load argument wrap. Same four 32-bit keys, same `int`-to-`long long` register-pair shape, so it is a second link the one-Convert wrap does not recover — the cast CHAIN case the argcast comment already names.
- Resolve riscv64's 2 `c2skip` bodies, `bitfields.c::main` and `struct_packed_indirect.c::dump`, both `rir_arena_mismatch`, plus its 5 `c2invalid`. These are **not** the `gen_negf` class: that one stays refused because the memory sign-flip compiles only off x86_64/i386/arm64/arm, the tree's own answer is `ast_hook_bail()` in `gen_negf`, and a riscv64 probe with the guard bypassed aborts the compiler on `sv->r & VT_LVAL` at `riscv64-gen.c:438`. Those 7 are half of riscv64's remaining 14.
- Settle whether the PE `tgmath.h` shadowing is real before reordering any header lookup. It does not reproduce on linux/x86_64: `tgmath.h` exists only at `runtime/include/tgmath.h`, yet `mcc-x86_64-win32` compiles `tgmath_dispatch.c` with rc=0 under `-B runtime/win32 -B runtime -I runtime/include`, under `-B runtime/win32 -B runtime` with no `-I`, and under `-B runtime/win32 -I runtime/include`, for a full 278-file `fn=1278`. Re-run those three flag sets on the macOS host before changing anything, since reordering to fix a defect one host cannot see risks moving the census on the host that can.

## Raise arena fidelity

- Raise `arenahasheq`, 812 of 1241 on arm64-osx. Measure as `arenahasheq` and as diffs-per-body falling, not as first-divergence classes disappearing — fixing divergence #1 only exposes #2. Classify by **structural** dump diff, not by node index: `ast_intention_hash` is a structural walk that hashes neither `type_ref` nor `ival` on a `Ref`, so the large Convert-versus-Ref index-transposition class and every `ref`-only diff are not fidelity defects and cost nothing to ignore. Live classes: 38 field-only, 32 extra-Convert-over-Binary, 20 missing-Convert-over-Literal, 14 tree-extra-Store. The last two are understood and not cheap — the tree builds cast chains where one push record recovers only one link, and `tree-extra-Store` is dead code under `nocode_wanted` that the tree records and the op filter drops.
- The extra-Convert-over-Binary class is now **measured, not guessed**, and the measurement says do not touch it. `rir_cast_seq` counts `gen_cast` entries and `RIR_M_ARGCAST` carries, per argument, whether that argument's cast fired at all — the tree's own condition, since `ast_hook_convert` records on every `gen_cast` call rather than on a type change. Gating the whole argcast wrap on that flag reads **+25 arenahasheq and -16 c2ok on every key**: the wraps the tree does not make are load-bearing for emission. The flag is therefore consulted only for the untyped-Load case it was added for. Any future attempt to close the class has to explain that trade first.
- Land explicit operand binding as a side-table keyed by op index, not a node field and not positional binding.
- Widen C3 pass equivalence past the field-identical population — the same work as raising arena fidelity, since the paired population *is* the field-identical population. Add the optimized-versus-optimized byte tap by replicating C2's whole save/restore prologue just before the `orig`/`orig_rel` frees in `ast_func_end`, where `ind` is the end of the optimized body: a different point with different live state. Report the coverage difference separately rather than as a mismatch, because the tree runs passes only where `ast_replay_ok` holds while Replay_IR accepts every body. `folds`/`samefolds`/`pairfired` stay printed and unpinned — two arenas can reach the same hash by different fold counts.

## Finish the capture path

- Phase F: un-embed `RirOp` from `JrnOp`. This is a design change, not a lift — Replay_IR must own an op stream with its own vstack buffer (the shape `rir_mvs`/`mvs_off`/`mvs_n` already has for marks) instead of reading `jrn_ops[]` and indexing `jrn_vs[]`. `jrn_snap_vstack` falls out of the same change: it has no `mccgen.c` call site to hang on, so it cannot be lifted independently.
- Discharge F against the same obligation every prior slice met: byte-identical `MCC_REPLAY_IR=5` sweep log and identical object sha256 at `-O0`, `-O1`, `-O2`, `-O3` and forced `-O0`. That compares capture against parser, tree and journal at once and is strictly stronger than cross-checking two capture paths against each other. Replace site by site; do not run both paths side by side.

## Keep the measurement honest

- Bank the corpus census against header resolution, never against a remembered number. `MCC_CONFIG_AUTO_MCCDIR` resolves mcc's own freestanding headers from **argv[0]'s directory**, so a compiler built into a scratch dir with no sibling `include/` silently loses `stdarg.h`/`stdbool.h`/`stdatomic.h`/`threads.h`; a build omitting `-DMCC_CONFIG_MCCDIR` finds no system headers at all and reads 476; a glibc sysroot resolves `<threads.h>` to glibc's rather than mcc's shim and costs 25 functions. Every one of these leaves `rc`, the file count and the failing-file list unchanged — the census is the only thing that moves, which is what makes it a trap.
- Keep the C2 harness mirroring the tree's replay prologue exactly across `vstack`/`vtop`, `loc`, `anon_sym`, `ast_pinned_regs`, `ast_rp_bsym`, `ast_rp_csym`, `ast_rp_switch`, `ast_temp_frontier`, `ast_rp_nlabel`, `ast_fconst_i`, `ast_locrec_i`, `sym_free_first`. Leftover allocator state reads as a codegen difference; one omission, a dirty vstack, costs 194 bodies.
- Keep the `-O0` cells honest. `ast_replay_env` needs `optimize >= 1`, so `-O0` without `FORCE` journals nothing; the cell exits 1 on "0 bodies journalled" rather than 77, and `FORCE` without `SRCDIR` is fatal. The 38 `o4 || optimize >= 1` gate names are derived by regex over `${SRCDIR}/*.c` per run so a cross cell cannot drift from the native one. Forced `-O0` legitimately reads 3 bodies fewer than `-O1` on every ELF and Mach-O key and 0 fewer on the PE keys — `grep.c::tolower`, `c11_threads.c::thrd_equal`, `arm64.c::putchar`, all `static inline` shims whose out-of-line copy `-O1` emits and `-O0` does not. That is an emit difference, not a census loss.
- Give the cross `x86_64-osx` compiler a macOS SDK include path: `mcc_add_macos_sdkincludepath` sits inside `#ifdef MCC_TARGET_IS_HOST`, so cross-compiling x86_64 macOS on an arm64 mac finds no system headers, compiles 79 of 278 files, reports `fn=483` and still reads `c2ok 452/452 = 100%` — a plausible, wrong row.
- Extend the native-versus-cross cross-check to riscv64 and i386. arm64 has it: measured natively in a `linux/arm64` container, `c2ok=1175/1180` identical to the cross reading, so C2 is cross/native-invariant there. Neither of the other two can run natively on this host.
- Measure the `MCC_AST_*` gate ledger at more than one `-O`. The six `STOREVAL_*` gates are worth +5 bodies at `-O0` and fire nowhere at `-O1`, so a single-gate ledger at one level — what `tools/gate-ledger.sh` produces — is a lower bound.
- Keep `src/mccrir.c` free of any `MCC_TRACE(` call; `tools/tracegate.c` checks a file only if it contains one, so the first call pulls that file's ~250 unannotated branches into the checked set at once. Run `./cmake-debug/tracegate src` and `./cmake-debug/schemagate src` before every push.

## Land and delete

- Flip `MCC_REPLAY_IR_C2` on per target as each key individually reaches 100%, not all at once on the aggregate. Add the `tests/ast/rir_c2.cmake` gate with it.
- Ungate `MCC_REPLAY_IR`: drop the CMake option, remove the `REPLAY_IR` row from `GATES[]` in `tests/fuzz/runner.c`, make **capture** unconditional while **verify** stays gated — `rir_verify` costs +7.7% wall clock on every compile (4.30 s against 4.63 s over 266 corpus compiles at `-O1`), which is not a tax for every user build, and after deletion there is no tree column and no journal column left for a three-way comparison to read. `MCC_REPLAY_IR_C2` then deletes with its two `#ifndef` lines. Once ungated, `rir_parity.cmake` and `rir_c3.cmake`'s 77 paths are dead and must become hard failures, or a build that silently lost Replay_IR reports green.
- Delete the AST recorder and the operation journal together, and not before C2 is 100% on every key. The tree still exclusively carries the optimization passes, the loop transforms, `ast_intention_hash`/`ast_slice_ident_hash` and the AOT/JIT equivalence invariant; each must move to Replay_IR or be shown dead first. Retire rather than port the journal's `raw=`/`rawb=` breadth bucket — it is zero on 15 of 17 banked keys, and the two exceptions carry one stale row (`arch/arm64.c myprintf raw=14 rawb=688`) that the current compiler reports only in the baseline. Rebank those two in whatever mode produced their depth files; banking breadth from cross would write 1208 rows against a depth ceiling of 1202/1210 and half-ratchet the key. Move `rir_dbg_on` out of `mccast.c` on the way.
