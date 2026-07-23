---
name: ast-replay-symdbg-debugging
description: How to reproduce/debug heap-dependent AST-replay (MCC_AST_REPLAY) crashes that only fail on some hosts
metadata: 
  node_type: memory
  type: reference
  originSessionId: 9eb77bd4-1b1c-4b4d-9594-fe679b8aec60
---

AST-replay bugs (MCC_AST_REPLAY=1) often present as garbage symbol names, "unresolved reference to ''", "branch out of range", or segfaults that reproduce on one CI host/compiler but not another — they are stale `Sym*`/jump-list reads whose outcome depends on what the recycled sym-pool slot happens to hold.

To make them deterministic: build mcc_s with ASan **plus** `-DSYM_DEBUG=1`. SYM_DEBUG (src/mccgen.c sym_free) makes `sym_free` call real `free()` instead of the `sym_free_first` pool free-list, so a dangling `Sym*` read becomes a catchable heap-use-after-free instead of a benign recycled-slot read.

  cmake -S . -B cmake-symdbg -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang \
    -DMCC_DARWIN_HOST=ON -DMCC_AST=ON -DMCC_CST=ON -DMCC_BUILD_SANITIZE=ON -DCMAKE_C_FLAGS="-DSYM_DEBUG=1"
  ninja -C cmake-symdbg mcc_s
  ASAN_OPTIONS=detect_leaks=0 MCC_AST_REPLAY=1 cmake-symdbg/mcc_s -c file.c -o /dev/null

Caveat: SYM_DEBUG is *stricter* than reality — it flags dangling reads that are benign in the pool build (the freed sym's data survives). Not every SYM_DEBUG UAF is a real failure; confirm against the actual pool-build symptom (wrong output / real error). A real GNU gcc for the "macos gcc" CI job is Homebrew `gcc-16` (`/opt/homebrew/bin/gcc-16`); `/usr/bin/gcc` is Apple clang. Related: [[no-code-comments]], [[build-dir-prefix]].
