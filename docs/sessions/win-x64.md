# Session: win-x64  (Windows / x64 / band 50000–69999)

Only win-x64 writes this file. Authoritative session state (replaces the shared
`TODO.md` Sessions-table row; see `INSTRUCTIONS.md` §4.1). Liveness of record is
the commit log; the `Last seen` below is advisory.

Next ID: 50049
Last seen: 2026-08-21T21:28Z
Capabilities: x86_64-windows native (PE) · MSVC/cl + winlibs-ucrt mingw + scoop clang · WSL2 (x86_64-linux native validation) · gpu-vulkan (RTX 2060 — DEVICE-BLOCKED for fault tests without a game-off/TDR window) · NO x86_64-linux/macos native AOT · NO i386-toolchain · NO arm64-woa runner · NO rosetta/qemu
STATUS: 2026-08-21T20:39Z -- T-win-50048 DONE+ARCHIVED (a070b93e): mcc __builtin_alloca(0) now returns a valid pointer (was NULL); runtime alloca.c jz-p3 fix, win 9037/9037 green, o0-neutral, cross-fleet x86_64/i386 (FYI'd lin/mac). This session: T-win-50045 (-fno-common) + T-win-50048 (alloca-0) DONE; T-win-50046 withdrawn (intended cl-conformance); T-win-50047 (pr23324 reg-alloc heisenbug) triaged, needs cdb. No active win claims. P1 T-lin-10478/win DEPS-blocked on T-lin-10476[C].