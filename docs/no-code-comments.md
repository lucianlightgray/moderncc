---
name: no-code-comments
description: Never write code comments in this project; put rationale in commit messages / NOTES / TODO
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 18b71fb2-b41e-43c2-badc-4a9e895e9ae7
---

Never write code comments in the moderncc/mcc codebase — not in src/, tools/,
runtime/, tests/, or CMake. No explanatory comments, no rationale comments, no
section headers.

**Why:** The project deliberately strips all code comments (commit 2d569157
"Strip all code comments; migrate rationale to NOTES/TODO"). The maintainer
reaffirmed it directly: "never write code comments."

**How to apply:** Write comment-free code. Put any rationale that would have
been a comment into the git commit message, or into docs/NOTES.md / docs/TODO.md
/ the relevant docs/*.md file. When editing existing code, do not add comments;
if a change needs explanation, it goes in the commit message.
