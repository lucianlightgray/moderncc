---
name: fortify-source-neutralized
description: mcc neutralizes _FORTIFY_SOURCE in the driver (after -D) because it has no _chk builtins
metadata: 
  node_type: memory
  type: project
  originSessionId: 10e773c4-467a-4c00-ba6c-1c5adf74523e
---

mcc has no `__builtin___memcpy_chk` (or the rest of the `_chk` family), so glibc's
`<string.h>` fortified `memcpy` inline (emitted when `_FORTIFY_SOURCE > 0`) fails to
compile with "implicit declaration of `__builtin___memcpy_chk`" — breaking ANY program
built with the distro-default `-D_FORTIFY_SOURCE=2` (Debian/Ubuntu/Fedora hardening).

Fix location: `preprocess_start` in `src/mccpp.c` appends `#undef _FORTIFY_SOURCE` +
`#define _FORTIFY_SOURCE 0` **after** `cmdline_defs`. This matters because `mcc_predefs`
(which emits `#include <mccdefs.h>`) runs BEFORE the command-line `-D` defines are
appended — so a header `#define` inside mccdefs.h can never override `-D_FORTIFY_SOURCE=2`.
Neutralizing in the driver, post-`-D`, wins on every target. (Previously mccdefs.h had an
Apple-only `#define _FORTIFY_SOURCE 0` that only appeared to work by SDK luck.)

Surfaced by cli/builtin_object_size failing on all Linux jobs + macOS CI (2026-07-20).
Any test/program relying on fortify being *honored* would break — none do; mcc can't
provide it. See [[docker-macos-tmp-mount]] for how the glibc repro was built.
