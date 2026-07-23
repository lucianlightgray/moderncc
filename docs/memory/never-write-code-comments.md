---
name: never-write-code-comments
description: The user does not want code comments written — omit them in all edits
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 10dce1d5-17da-4840-a3da-ec59de680bd3
---

Never write code comments (in any language: C, CMake, etc.). Emit only the code.

**Why:** The user does not want comments in code I write. This holds EVEN THOUGH
the existing codebase (and the user's own recent commits) contain heavy rationale
comments — do NOT infer from surrounding comment density that new comments are
wanted. Reaffirmed 2026-07-08 ("No. Do not add comments.") after I wrongly deleted
this memory on the (mistaken) grounds that the current code contradicted it.

**How to apply:** Put explanatory rationale in the commit message or in
`docs/NOTES.md` / `docs/TODO.md`, not in inline comments. When editing existing
files, don't add comments; leave existing ones as-is unless asked to strip them.
Match surrounding *code* style, but never its comment density.
