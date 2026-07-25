---
name: embed-jit-arm64osx-gap
description: "Embedded-JIT baked binaries and in-process -run JIT don't work on arm64-osx in current builds"
metadata: 
  node_type: memory
  type: project
  originSessionId: deb64526-2e4b-4138-98cc-4f7258f4bd37
---

On arm64-osx (Apple Silicon), mcc's runtime JIT is effectively unavailable in every local build as of 2026-07-25 (commit 585cc12b):

- **Baked embedded-JIT executables fail to link.** `mcc --embed-jit <src> -o out` errors with `unresolved reference to '_mccjit_set_search_budget'` / `_mccjit_boot_swap`. Those symbols are registered only for the in-process path via `mcc_add_symbol` (src/mccjit_embed.c:1816-1818); they are NOT in any `arm64-osx-libmccrt.a` (checked every `cmake-*` dir with `nm`). So `--embed-jit` on a file output = broken here → no "with-JIT vs without-JIT" baked-binary comparison possible.
- **In-process `-run --jit` doesn't engage the JIT** on ordinary code (e.g. nbody): 0 `mccjit_*` calls in a `-v128` trace. `--embed-jit` on a trivial main "succeeds" only because it bakes nothing ("no functions were JIT-baked").
- `cmake-build-embedjit/mcc` is stale/broken (same unresolved-symbol errors when baking actually fires).

Consistent with the [[slice-cache-refactor]] caveat: JIT/AOT arena-normalization gap. The slice-cache graduation→PROVEN mechanism is still unit-proven (`asttool slice_graduate` 21/21, `slice_persist` 16/16), just not demonstrable e2e on this platform.

To observe the optimizer search/slice cache: `-v128` sets the TRACE bit (verbose is a bitmask; `-v<N>` ORs it). Slice cache lives at `~/Library/Caches/mcc/sl-<salt>.ck`; needs `MCC_AST_SLICE=1` (default off ⇒ byte-identical). Warm-start trace = `ast_slice_consume: slice warm-start` (mccast.c:13982). Search engine = `combo_run` (mcccombo.h:219), budget `AST_SEARCH_MAX_CAND=128`/function, permutation axis via `MCC_AST_SEARCH_ORDERED=1`.
