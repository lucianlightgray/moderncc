---
name: mccast-rawalloc-region
description: New AstArena-allocating functions must live in the push_macro/undef raw-alloc region of mccast.c
metadata: 
  node_type: memory
  type: reference
  originSessionId: 6da1abd0-6b46-4030-914d-2ce0fa01c5cd
---

`src/mcc.h` poisons `malloc`/`realloc`/`free` to `use_mcc_malloc`/`_realloc`/`_free` (undeclared → deliberate compile error) to force `mcc_malloc`/`mcc_free` everywhere. `mccast.c` carves out ONE exception: a `#pragma push_macro("malloc"...)` + `#undef malloc/realloc/free` block near the top (opens ~line 33) closed by `#pragma pop_macro(...)` (~line 547). Only inside that window do raw `malloc`/`free`/`calloc` work — that's where `ast_arena_new/clone/free` and the SoA arena allocators live.

Any new function that mallocs AstArena field arrays (e.g. `ast_slice_extract`) MUST go inside that window, because `ast_arena_free` frees those fields with raw `free` — allocating them with `mcc_malloc` outside the window would be an allocator mismatch (and raw `malloc` outside the window is a hard compile error via the poison). Guard embed-only additions with `#if MCC_EMBED_JIT` so the default build stays byte-identical (M8 self-host bar). See [[reemit-faithful-gate]].
