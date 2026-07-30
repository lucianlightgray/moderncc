---
name: landor-invert-fold-bug-class
description: AST fold passes that rewrite a LAND/LOR node drop AST_FB_LANDOR_INVERT and invert negated conditions — a recurring correctness bug class
metadata: 
  node_type: memory
  type: project
  originSessionId: d8e85f80-7e78-45fe-a389-93af6d22f450
---

A recurring -O2+ miscompile class: an AST fold that rewrites a `TOK_LAND`/`TOK_LOR`
node into a different node (`ast_set_fbits(a,n,0)` clears everything) drops
`AST_FB_LANDOR_INVERT` — the bit the parser sets for `!(...)`. Only the LAND/LOR
replay path (`ast_replay_value` ~5870, swaps jtrue/jfalse via `cmp_op ^= 1`) honors
that bit; the folded plain node does not, so a negated condition comes out with the
branch inverted.

Instances:
- **MCC_AST_RANGE** (`ast_range_try`/`_try_lor`): `lo<=x && x<=hi` → unsigned compare.
  FIXED 2c88e797/311767c7 by baking the negation into the op (`ULE ^ invert`,
  `UGT ^ invert`). See [[osx-selfhost-jit-sympool-corruption]] — this was the macOS
  selfhost-jit blocker.
- **MCC_AST_BITFLAG** (`ast_bf_try_if`/`_ifne`/`_lor`/`_land`): 5+-value equality
  chains → bitmask test. FIXED bcd68443. The if/branch culprit was `ast_bf_try_if`
  (folds `if(x==a||...)`→`if(member)`), which ignored the invert bit on the parsed
  LOR — `objdump` showed `cbnz member` where `cbz` was needed. `try_if`/`_ifne` bake
  the negation into the emitted op (member↔member^1 per the invert bit); `try_lor`/
  `_land` (value context) skip when inverted. **Lesson: use `tracediff.sh <mcc> r.c
  MCC_AST_BITFLAG=5 MCC_AST_BITFLAG=100` (fold on vs off, LANDOR held on) — it pointed
  straight at `ast_bf_try_if`, which four rounds of manual reasoning had missed.**

**Method to find/verify:** a self-verifying differential program (checker fns vs the
direct expression, prints OK/FAIL n) compiled by mcc `-O2` vs gcc, then gate-bisect
with `MCC_AST_<GATE>=0` to find the culprit (`MCC_AST_LANDOR_INVERT=0` is the tell for
this class). Disassemble a static fn as `objdump -d --disassemble=<fn>` (leading `_`
on macOS-config); a `cset ne`/`cbnz` where a `cset eq`/`cbz` is expected = inverted.

**Audit lead:** other `ast_set_fbits(a,n,0)` sites that rewrite LAND/LOR nodes may have
the same bug; grep `ast_op(a,n)==TOK_LAND||TOK_LOR` near fold rewrites.
