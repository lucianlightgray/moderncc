# Session: win-x64  (Windows / x64 / band 50000–69999)

Only win-x64 writes this file. Authoritative session state (replaces the shared
`TODO.md` Sessions-table row; see `INSTRUCTIONS.md` §4.1). Liveness of record is
the commit log; the `Last seen` below is advisory.

Next ID: 50049
Last seen: 2026-08-21T20:14Z
Capabilities: x86_64-windows native (PE) · MSVC/cl + winlibs-ucrt mingw + scoop clang · WSL2 (x86_64-linux native validation) · gpu-vulkan (RTX 2060 — DEVICE-BLOCKED for fault tests without a game-off/TDR window) · NO x86_64-linux/macos native AOT · NO i386-toolchain · NO arm64-woa runner · NO rosetta/qemu
STATUS: 2026-08-21T20:01Z — T-win-50045 (-fno-common) DONE+ARCHIVED (3d06e503), verified cross-fleet (lin ELF, mac Mach-O regression-fix 19df84a58 confirmed on win-PE exec 8225/8225). Ran a fresh win-PE gcc-c-torture -O2 differential (1694): bf-sign-2 → T-win-50046 WITHDRAWN (intended cl-conformance 8d4f0a80, not a bug — caught by full-exec gate + reverted); pr23324 → T-win-50047 (real Win64 struct-ABI SEGV — triaged to root-cause CLASS: a context-sensitive reg-alloc/stack-layout HEISENBUG in the empty-union-sret + large-struct-by-ref call; ALL source-minimizations pass, only the original full-file layout reproduces → needs cdb on the original, DETAILS#t-win-50047-pr23324-win64-struct-abi-segv); pr36321 → T-win-50048 (alloca(0) impl-defined, LOW). Migrated to rev-2 per-session files. No active win claims. P1 win lane (T-lin-10478/win) DEPS-blocked on T-lin-10476[C] (lin-owned, paused pending mac Rosetta/wine findings).
