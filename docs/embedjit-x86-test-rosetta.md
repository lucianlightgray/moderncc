---
name: embedjit-x86-test-rosetta
description: "x86 embed-jit from arm64 host: x86_64-PE JIT RUNS under wine (full local verify path); native Mach-O/Linux x86 RWX hangs under Rosetta; SysV-Linux needs Docker amd64"
metadata: 
  node_type: memory
  type: reference
  originSessionId: 6da1abd0-6b46-4030-914d-2ce0fa01c5cd
---

Testing x86-specific JIT dispatch code (e.g. `mccjit_patch_make_slot`/`_tramp`, the `#if defined(__x86_64__)` hot-patch rows) from this arm64 macOS host has two blockers:

1. **Selftests aren't registered when cross-targeting x86.** The `jit/selftest-*` targets are gated by `if(MCC_TARGET_IS_HOST)` in CMakeLists — false when building `-arch x86_64` on an arm64 host — so `make jit_selftest_patch` gives "No rule to make target". You CAN still validate the x86 path *compiles* by building `libmcc`/`mcc` in an x86 build dir with `-DMCC_EMBED_JIT=ON` (libmcc.c amalgamates mccast.c + mccjit_embed.c).

2. **Rosetta can't execute dynamically-emitted RWX x86 code** — for a native x86_64 Mach-O/Linux binary (`mmap(PROT_READ|WRITE|EXEC)` + write + call hangs under Rosetta).

Reuse `cmake-macos-x64` for the x86 compile check (revert `MCC_EMBED_JIT` to OFF afterward to leave it as found). See [[build-dir-prefix]].

**UPDATE 2026-07-19 — x86_64-PE JIT RUNS under wine (Rosetta), a full local verification path.** Point 2 above is Mach-O/Linux-specific; **wine 11 executes dynamically-emitted RWX x86_64 code in a PE binary correctly** (probed: `VirtualAlloc(PAGE_EXECUTE_READWRITE)` + write + call returns right). So Windows x86_64 JIT is fully locally verifiable from this arm64 host: build `libmcc-static.a` with homebrew `x86_64-w64-mingw32-gcc` (bypass the winlibs superbuild via a plain CMake toolchain: `CMAKE_SYSTEM_NAME=Windows`, `-DMCC_BUILD_TESTS=OFF` to skip the `mccharness` add_test that breaks generate; the `bin2c.exe` host-tool step fails but only the `mcc` blob target needs it — libmcc builds), then `x86_64-w64-mingw32-gcc jit_selftest_X.c libmcc-static.a -o t.exe` and `wine t.exe`. Used to develop+verify the Win64 mixed KGC stub end-to-end (`d28eaca9`). x86_64-SysV Linux still needs `--platform linux/amd64` Docker (image needs cmake+make; silkeh/clang:18 lacks ninja — use `-G "Unix Makefiles"` + `CC=clang`; build the `mcc` target too so `runmain.o` exists for the `-run` end-to-end block). See [[docker-amd64-repro]].
