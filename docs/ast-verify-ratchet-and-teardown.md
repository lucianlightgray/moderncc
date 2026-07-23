---
name: ast-verify-ratchet-and-teardown
description: "The MCC_AST_VERIFY harness, the recorder-fidelity ratchet, and why desync/faithfulness teardown is gated on closing the gap set"
metadata: 
  node_type: memory
  type: project
  originSessionId: 2f7ae584-36eb-417e-802e-cc635262bfe0
---

Goal the user set (2026-07-15): eventually **retire the AST recorder's `desync` +
`faithfulness` safety net** and trust the test suite as the source of truth. My
recommendation (accepted): **demote, don't delete** — keep the byte-exact
baseline-vs-replay check as a verify-mode oracle and drive the gap set to zero
first, because deleting the net today converts every unmodeled construct from a
graceful baseline-fallback into a silent miscompile.

**The measurement that blocks teardown.** Swept the exec corpus + full_language.c
(1119 recorded functions) at -O2 with the new instrumentation: **~24% are
non-faithful today** — 205 `desync`, 50 `unfaithful`, 10 `stackresidue` (801
faithful, 39 empty, 14 bail). The suite is green only because faithfulness
silently falls back to baseline for these. Gaps cluster in struct/union byval
ABI, bitfields, VLAs, C11 atomics, `__attribute__((cleanup))`, `_Complex`,
`_Noreturn`, inline asm. `optimizer/` corpus is ~99.6% faithful (the recorder is
complete for the code the replay tests were written around, not the language).

**Progress (commit `26c60422`, pushed):** modeled struct-by-value `VT_LLOCAL`
lvalues in `ast_hook_vpush` (accept as `AST_Ref` — round-trips via `vpushv`;
narrow guard in `ast_hook_vstore` for the member-store-into-VT_LLOCAL case that
stays a baseline fallback). **win32 gaps 255→215** (desync 205→165, faithful
811→849, +38), 0 new gaps, exec 4427/4427. Also added **desync-line attribution**:
`AST_SET_DESYNC()` macro records `__LINE__` of first firing, surfaced in
`[ast-verify]` as `desync:<line>` — sweep + tally to find the dominant desync
site. That found the win2 cause: 63 desyncs at the `ast_hook_vpush` type/class
gate, 49 of them `VT_LLOCAL bt=VT_STRUCT lval=1`. Next desync causes by line:
`:1348` vpush shadow-depth mismatch (39, deep), `:2213` call_begin (27), landor/
ternary (16), member (13).

**Codegen fix (commit `f56aee6f`): `__builtin_expect` comparison-fold.** The
dominant Linux "unfaithful" (1145 of 1317, but only 6 distinct glibc `*_unlocked`
stdio inline helpers ×~186 TUs) was NOT a recorder bug — replay produced BETTER
code. Root cause: `TOK_builtin_expect` parsed both args via
`parse_builtin_params(0,"ee")`; evaluating the hint arg pushed a value onto the
vstack above the predicate, and TCC's single-flags-register model spills a buried
`VT_CMP`, so a relational predicate (`ptr>=end` in `__glibc_unlikely`) got
materialized (`setcc;movzx`) before the if/?: saw it → `cmp $0;jcc` instead of a
direct branch. AST replay had the whole tree and folded it → replay shorter →
flagged unfaithful. Fix (`mccgen.c` `TOK_builtin_expect`): parse the hint with
`expr_const64()` (it's always a constant — glibc `__glibc_likely/unlikely`, std
usage) so it's consumed without touching the vstack, leaving the predicate's
`VT_CMP` live to fold. Baseline now emits `cmp;jl` not `cmp;setge;movzx;cmp$0;je`
— **better codegen AND faithful**. This validated the user's original teardown
instinct: byte-exact faithfulness over-counts "replay is better" as gaps. win32
barely moved (215→214, msvcrt has no `__builtin_expect`); Linux was the payoff.
User's chosen direction for this class: **improve baseline codegen to match**
(not relax the oracle). Full ctest green except 4 pre-existing env failures
(mingw-asan×2, config-defines, cross-factory — identical on clean HEAD).

**Infra landed (commit `accbd4b7`):**
- `MCC_AST_VERIFY` env in `mccast.c` `ast_func_end`: `=1` prints per-fn verdict
  `faithful/desync/unfaithful/stackresidue/bail/empty`; `=2` = `mcc_error_noabort`
  on a genuine gap (desync/unfaithful/stackresidue) so the compile fails.
  `MCC_AST_VERIFY_OUT=<file>` appends; `MCC_AST_VERIFY_DIFF=<fnsubstr|all>` dumps
  the baseline-vs-replay byte divergence in-process (divergent replay bytes never
  reach the .o — faithfulness restores baseline, so must capture live).
- Ratchet: `tests/ast/verify_ratchet.cmake` + per-target baseline
  `tests/ast/verify-baseline/<cpu>-<os>.txt` (x86_64-win32 = 255 gaps), wired as
  ctest **`ast-verify-ratchet`**. Sweeps corpus, fails if the gap set drifts from
  baseline (can only shrink). No baseline for a target → SKIP 77. Regenerate:
  `cmake -DMCC=<mcc> -DCORPUS=tests/exec -DEXTRA=tests/diff/full_language.c -DBASELINE=<f> -DTMPDIR=<d> -DREGEN=1 -P tests/ast/verify_ratchet.cmake`.
  Baselines committed: **x86_64-win32 (214 gaps)** and **x86_64-linux (759 gaps,
  commit `57a925ba`)** — both ratchets active. Other targets SKIP until regenned
  (Linux via the mcc-ci docker image; see [[moderncc-windows-build]]).

  **Linux residue / next lever:** Linux is dominated by glibc stdio inline helpers
  (few distinct funcs ×~186 TUs). The `__builtin_expect` fold fixed the 3 READ
  helpers (getc/fgetc/getchar_unlocked → faithful, 1317→759). The 3 WRITE helpers
  (putc/fputc/putchar_unlocked) = 557 of the 587 remaining unfaithful (95%) — they
  carry a SECOND, deeper divergence. **RESOLVED (2026-07-15, via MCC_CONFIG_TRACE +
  a temp MCC_FORCE_REPLAY gate that keeps replay output so it can be objdumped): it
  is a genuine AST-replay MISCOMPILE, not a better schedule.** For
  `__builtin_expect(c,0) ? cold : (*p++ = v)` (assignment in a ternary arm), replay
  (a) hoists the side-effecting store ABOVE its guard → executes unconditionally
  (writes even on the overflow/cold path), and (b) DROPS the RHS operand — emits
  `mov [ptr],al` where `al` is garbage (high byte of ptr+1) because `v`/`ch` is
  never loaded. Faithfulness correctly rejects it and ships the correct baseline. So
  the putc-family "unfaithful" is **faithfulness catching a real replay bug** — the
  OPPOSITE of the getc case (replay was better). Had faithfulness been deleted, this
  would ship as a miscompile: strong validation of "demote, don't delete." Repro:
  build `-DMCC_CONFIG_TRACE=ON`, compile `int t(unsigned char**wp,unsigned char**we,
  int ch){return __builtin_expect(*wp>=*we,0)?ovf():(unsigned char)(*(*wp)++=ch);}`
  at -O2; to see replay's wrong output, re-add a `!getenv("MCC_FORCE_REPLAY")` guard
  on the `if (!faithful)` restore in `ast_func_end` and objdump with MCC_FORCE_REPLAY=1.
  Root cause pinned to `ast_hook_vstore` (`mccast.c`): it appended every `AST_Store`
  to `ast_cur_bb` UNCONDITIONALLY; inside a ternary arm (ast_in_call cleared) the
  block is still the function block, so a conditional store was modeled as an
  unconditional statement → replay emitted it before the guard + dropped the RHS.
  **HARDENING FIX landed (commit `a5be8b46`):** desync when a vstore occurs while a
  ternary (`ast_tern_top>0`) or short-circuit (`ast_lor_top>0`) arm is open — the
  recorder now DECLINES the shape instead of emitting a wrong model the net must
  catch (teardown-readiness: post-faithfulness, desync→baseline instead of
  miscompile). getc-family stay faithful (read+increment, no assignment); putc-
  family move unfaithful→desync (same gap set, 0 faithful regressions, win32
  4427/4427). Also: **verify_ratchet now keys on (file,func) not verdict** so a
  reclassification (unfaithful→desync) is not a false regression; verdict stays in
  the baseline as info. The FULLY-FAITHFUL fix (nest arm statements under the
  `AST_If` so replay emits the store under the branch — the thing that would
  actually reduce the gap count 759→~200) remains the deep follow-up: needs the AST
  model + replay to represent a statement (not just a value) in a ternary arm.
  NOT the offset class, NOT the condition-fold — a third, distinct class.

**Gap-closing loop (proven, safe because the net catches bad fixes):** pick an
`unfaithful` cluster → `MCC_AST_VERIFY_DIFF` to see the byte divergence → the
tractable class is a **stack-slot offset** mismatch = a `loc` allocation that
bypasses `ast_alloc_loc` (which records offsets in baseline + replays them).
First & only offset-class fix: `gfunc_return` struct-return-in-reg temp
(`mccgen.c` ~10150) was raw `loc=(loc-size)&-align` → routed through
`ast_alloc_loc`; closed 10 unfaithful (50→40), 0 regressions.

**The offset/raw-loc class is now EXHAUSTED (verified 2026-07-15 by a full
diff-classified sweep).** All other raw `loc -=` sites in mccgen.c are either
already `ast_alloc_loc`-guarded (cplx_local:4783, struct sites 9541/9663), feed
only atomic-lowering that comes out `desync` not `unfaithful` (alloc_local_slot:
6342), have the reuse-cache wrinkle and aren't the cause (get_temp_local_var:1600),
or aren't exercised (VLA/asan 12027-12312). **The remaining 40 win32 unfaithful are
codegen-SHAPE divergence** — baseline single-pass vs AST replay differ in
evaluation order, operand-spill (spill-vs-keep-in-reg), branch/setcc folding,
builtin-fold branch, and DCE — NOT offsets (displacement bytes match). Examples:
bitfields.c `dump` (load order + movslq), check_exports (setcc condition inversion),
libm_builtin_fold fabs/trunc (fold branch), noreturn.c `drive_int` (replay DCEs an
indirect call block). Closing these = a different, higher-risk workstream: make
replay reproduce baseline's exact instruction selection, OR relax faithfulness from
byte-exact to a semantic-equivalence check. The 205 `desync` (bigger bucket) =
recorder refuses to model VLA/atomics/cleanup/complex/struct-byval — teaching those
is the other big workstream.

Related recurring build fact: **`MCC_EMBED_JIT` defaults ON but is glibc-only**
(`src/mccjit_embed.c` needs sys/mman.h/pthread/fork) — gated OFF on WIN32 in this
commit (`CMakeLists.txt` ~1662, mirrors `MCC_EMBED_MCCRT`). A real Windows JIT
port is still TODO (see TODO.md). [[commit-push-main]]
