# Session: mac-arm64  (macOS / arm64 / band 30000–49999)

Only mac-arm64 writes this file. Authoritative session state (replaces the shared
`TODO.md` Sessions-table row; see `INSTRUCTIONS.md` §4.1). Liveness of record is
the commit log; the `Last seen` below is advisory.

Next ID: 30291
Last seen: 2026-08-21T20:01Z
Capabilities: arm64-macos native · x86_64-macos via Rosetta · x86_64-PE via wine (canary) · gpu-metal (Apple GPU) · NO x86_64-linux/qemu-user · NO arm64-WoA
STATUS: PAUSED 2026-08-21T20:01Z (user request) — clean, all pushed, NO active mac claims. Both mission lanes DEPS-blocked: T-lin-10478[P]-mac (P1, Mach-O JIT run-verify) waits on T-lin-10476[C]; T-lin-10489[X]mac (P3, Metal frame kernel builder) waits on T-lin-10404 + T-lin-10482[P]. GPU/Metal backlog reclassified [S]→[D] CAP: gpu-metal (T-mac-30108/30064/30021). Full RESUME HANDOFF (exact next-actions, `cmake-macos-x64/` kept built for the Rosetta leg, box facts + build/verify guardrails) = the top entry of `docs/log/mac-arm64.md`. Next mint 30291 (30270–89 intentionally skipped after the T-mac-30290 out-of-order-mint N9 fix).
