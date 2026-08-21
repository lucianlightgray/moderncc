# Session: win-x64  (Windows / x64 / band 50000–69999)

Only win-x64 writes this file. Authoritative session state (replaces the shared
`TODO.md` Sessions-table row; see `INSTRUCTIONS.md` §4.1). Liveness of record is
the commit log; the `Last seen` below is advisory.

Next ID: 50049
Last seen: 2026-08-21T21:34Z
Capabilities: x86_64-windows native (PE) · MSVC/cl + winlibs-ucrt mingw + scoop clang · WSL2 (x86_64-linux native validation) · gpu-vulkan (RTX 2060 — DEVICE-BLOCKED for fault tests without a game-off/TDR window) · NO x86_64-linux/macos native AOT · NO i386-toolchain · NO arm64-woa runner · NO rosetta/qemu
STATUS: 2026-08-21T21:34Z -- P1 T-lin-10478/win DONE (86da969c): native x86_64-PE optfire -run leg LIVE, 46 differ cells fire+run-verify via mcc -run on PE (was 0). This session DONE: T-win-50045 (-fno-common), T-win-50048 (alloca-0), T-lin-10478/win (P1). Withdrawn: T-win-50046 (intended cl-conformance). Triaged: T-win-50047 (reg-alloc heisenbug, needs cdb). rev-2 migration complete. No active win claims. Parent T-lin-10478[P] open for lin/mac legs.