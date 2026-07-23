---
name: cst-ast-independence
description: "CST and AST must stay independent — share only via the one-way explicit cst/CstId reference, never a shared id space"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: a256a0df-044c-4e6c-b79a-70b3fa727d6e
---

The CST and AST arenas must remain independent and only share information in
reliable/safe ways.

**Why:** coupling their id spaces (a shared mutable counter, a back-pointer, or
leaking one arena's local ids into the other) makes edits in one silently
invalidate the other and defeats the "no anonymous / stable name" substrate work.

**How to apply:** keep separate local id spaces (`CstLocal`, `AstLocal`). The only
sanctioned cross-link is the existing **one-way, explicit AST→CST reference** — the
`cst` column (`src/mccast.c:35`) holding an opaque `CstId = (file<<32)|local`
(`src/mcccst.h:57-66`). Nothing reaches the other way. For the global naming
authority, the `(tag,id)` scheme is a *disjoint* encoding at the H_e boundary
(`AST_SLOT` vs `CST_BRANCH` ranges), not a merged namespace — neither arena needs
the other's ids. See docs/AST.md "Global naming authority". Relates to
[[reemit-faithful-gate]].
