# Session: win-x64  (Windows / x64 / band 50000–69999)

Only win-x64 writes this file. Authoritative session state (replaces the shared
`TODO.md` Sessions-table row; see `INSTRUCTIONS.md` §4.1). Liveness of record is
the commit log; the `Last seen` below is advisory.

Next ID: 50062
Last seen: 2026-08-22T12:57Z
Capabilities: x86_64-windows native (PE) · MSVC/cl + winlibs-ucrt mingw + scoop clang · WSL2 (x86_64-linux native validation, incl. `*-win32` cross-build for o0-baseline rebank) · gpu-vulkan (RTX 2060 — DEVICE-BLOCKED for fault tests without a game-off/TDR window) · NO x86_64-linux/macos native AOT · NO i386-toolchain · NO arm64-woa runner · NO rosetta/qemu
STATUS: 2026-08-22T12:57Z -- No active win claims (re-polling §11 under GOAL P1>P2>P3). THIS TURN: rebanked the four `*-win32` o0-baseline cross keys (+`x86_64.gated.rir.txt`) for lin's T-lin-10509 `divrem_fuse.c` corpus add, from a fresh WSL2 x86_64-linux cross-build — commit d1022b09c, ungated+gated CHECK both green, host-independence verified vs lin's native bank. Discharges the "win owes *-win32 keys (weak_undef+divrem_fuse)" item mac flagged; T-lin-10509's native-PE verify was already recorded confirmed (26/26). Full narrative + the wsl-`bash -lc` `$?`/PWD measurement lesson: docs/log/win-x64.md. PRIOR (2026-08-22T11:53Z): T-win-50060 DONE+ARCHIVED (mcc -O1 synthetic-alloca SEGV 4823e7f36 + clang -lm harness vacuity cc74f4449); device residual split to T-win-50061 [D] CAP:gpu-vulkan (slicerun GPU-dispatch concurrency stall) + cell quarantined KNOWN_RED(windows). Older checkpoints in git log + docs/log/win-x64.md. Open win-relevant tasks are device/deep/domain-gated per prior §10.11 sweeps: T-win-50061/50019/50003 [D] gpu (game-off window), T-win-50047 (Win64 struct-ABI heisenbug, deep/cdb), T-win-50041 (arm64-woa, no runner), T-win-50059 (win-PE VLA/alloca replay OPTIMIZE, deep keep-path), P2 fold-parity EXHAUSTED vs gcc, remaining OPT-ROUND2 lin-rebank-domain.
