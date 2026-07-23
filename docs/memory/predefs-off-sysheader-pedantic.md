---
name: predefs-off-sysheader-pedantic
description: MCC_CONFIG_PREDEFS=OFF loads runtime mccdefs.h (with //@ markers); lexer pedantic diagnostics must exempt system headers or -std=c89 -pedantic-errors dies inside it
metadata: 
  node_type: memory
  type: project
  originSessionId: 34a19c6c-5d01-4b37-8ff8-98596a7bad42
---

`MCC_CONFIG_PREDEFS=OFF` (the `linux-*-predefs-off` presets) makes mcc `#include
<mccdefs.h>` from runtime/include at runtime instead of the compiled-in c2str form.
That header uses `//@` end-of-line markers (c2str turns `//@` lines into build-time
`#if` directives; other lines become runtime string content), so it contains `//`
comments. Under `-std=c89/c90 -pedantic-errors` the lexer raised "C++ style comments
are a C99 feature" as a hard error INSIDE mccdefs.h, aborting before the user TU was
parsed — so all expected C89 pedantic diagnostics (VLA, compound literal, hex float,
long long, mixed decls) went missing → `cli/c9911_diag_gaps{,3,4,5,6}` failed, but
only in predefs-off cells (predefs-on never loads the raw header).

**Root cause:** parser-level `mcc_pedantic()` (mccgen.c) already exempts system
headers, but the lexer-level pedantic checks in mccpp.c never did. Fixed in 37ea7c9b
by adding `pp_in_system_header()` (mirrors mcc_pedantic's walk past internal `:`
buffers, gated on `error_set_jmp_enabled`) and gating the `//`-comment diagnostic on
it. mccdefs.h is a system header (included via `<...>` from the system_header
command-line buffer), so this is the correct exemption; user code still errors.

**How to apply:** if other predefs-off pedantic-diag failures show up, the same
lexer/parser system-header asymmetry is the likely cause — route the lexer pedantic
site through `pp_in_system_header()`. Don't "fix" by rewriting mccdefs.h's `//@`
markers: they're load-bearing for c2str. Repro on native amd64 docker with
`cmake --preset linux-gcc-predefs-off`.
