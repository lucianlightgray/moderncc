---
name: build-dir-prefix
description: "Always use the cmake-build- prefix for build directories, never plain build/"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 0431b79e-90f7-40eb-9bc9-a8c6039c62ba
---

Never create or use a `build/` directory in this repo. Always use the
`cmake-build-` prefix for CMake build directories — including throwaway/temp
ones (e.g. `cmake-build-tmp`).

**Why:** `.gitignore` ignores `/cmake-build-*/` and `/build-*/` but NOT plain
`/build/`, so a `build/` dir shows up as untracked clutter. The project already
keeps `cmake-build-debug` and `cmake-build-cross` dirs (CLion's convention).

**How to apply:** Reuse an existing `cmake-build-*` dir when present. The cross
CI leg (`-DMCC_ENABLE_CROSS=ON`) → `cmake-build-cross`; native/default →
`cmake-build-debug`. For a scratch build use `cmake-build-tmp` and remove it
after.

**Generator differs per dir** (verify before building): some are **Ninja**
(e.g. cmake-build-shadow — has `build.ninja`), some **Makefile** (e.g.
cmake-build-embedjit). Running `make <tgt>` in a Ninja dir silently no-ops
("Nothing to be done") — GNU make sees the existing binary file and thinks it's
up-to-date, so you unknowingly test a STALE binary. Prefer
`cmake --build <dir> --target <tgt>` (picks the right driver). Also: after a
`git stash` or CMakeLists edit, re-run `cmake .` or a target can vanish ("No
rule to make target") / libmcc rebuilds from the wrong source. See
[[parallelize-with-agents]].
