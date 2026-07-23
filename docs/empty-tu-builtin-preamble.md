---
name: empty-tu-builtin-preamble
description: "Why an empty-translation-unit pedantic diagnostic can't naively check tok==TOK_EOF at parse start"
metadata: 
  node_type: memory
  type: project
  originSessionId: 50380c9a-e3de-41d4-8d8f-7f8c2a25aa75
---

The "ISO C requires a translation unit to contain at least one declaration"
(empty-TU) pedantic diagnostic is still an open gap in the dg-error tier. The
obvious implementation — in `mccgen_compile` (src/mccgen.c ~line 588), after the
initial `next()`, check `if (tok == TOK_EOF) mcc_pedantic(...)` — does NOT work.

At that point the preprocessor is positioned in mcc's builtin `<command line>`
preamble (predefined builtins, e.g. `struct` decls for `__builtin_va_list`), so
the first token is always real content (observed: `tok` spelled `struct`,
`file->filename == "<command line>"`), never `TOK_EOF`, for both empty and
non-empty user files.

To do it right you must detect that the *user* source file (and its real
includes) contributed no external declaration — i.e. distinguish the builtin
`<command line>` preamble from user code, e.g. track whether any decl was parsed
while `file` is not the command-line/builtin preamble. Deferred as low-value
(empty files are rare) vs the plumbing cost. Related: [[reemit-faithful-gate]].
