---
name: emit-size-measurement-state-restore
description: "emit-size (MCC_AST_SEARCH_EMITSIZE) measurement must replay into the isolated scratch Section, never the live text section"
metadata: 
  node_type: memory
  type: project
  originSessionId: 03a48166-3306-413e-8864-3d35390a4ca5
---

`ast_search_emit_size` (MCC_AST_SEARCH_EMITSIZE) folds a candidate to completion
and replays it just to measure emitted byte size. It MUST replay into the private
scratch Section (`ast_scratch_enter`/`ast_scratch_measure_exit`), never the live
`cur_text_section`. Measuring in-place and hand-rewinding the shared codegen
cursors is an unbounded state-leak surface: any missed piece (local_stack, vtop,
pinned regs, promo/graft counters, section reloc/sym state, …) corrupts the
subsequent *real* emit → a rare, timing-dependent miscompile of the emitted
program (`exec-search-emitsize/enum` segfaulted on CI macOS/arm64; the search's
fair-interleave tick scheduler picks a different winning config by CPU speed, so
it reproduced on the runners but NOT on this host — 0/300 local, 0/50 even under
ASan).

**Why:** the isolated variant (MCC_AST_SEARCH_EMITISO) never failed in CI because
the scratch Section swap leaves live text/reloc/sym untouched and a text leak
trips a hard zero-leak assert. A first attempt to hand-restore local_stack/vtop
in the non-iso branch (2aafe509) was insufficient and passed one CI run only by
luck; the real fix (126f4bbc) removed the non-iso branch and always isolates.
Safe because emit-size scoring is opt-in/test-only (real -O4 uses static-cost)
and the winner is re-emitted by the normal pipeline regardless.

**How to apply:** never resurrect an in-place "emit into the live section then
rewind" measurement — route measurement replays through `ast_scratch_enter`.
When auditing any measure-and-discard replay, its restore surface is
`ast_scratch_measure_exit`'s field set. Related: [[mccast-rawalloc-region]],
[[reemit-faithful-gate]], [[4b-backend-parity]].
