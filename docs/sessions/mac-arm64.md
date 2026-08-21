# Session: mac-arm64  (macOS / arm64 / band 30000–49999)

Only mac-arm64 writes this file. Authoritative session state (replaces the shared
`TODO.md` Sessions-table row; see `INSTRUCTIONS.md` §4.1). Liveness of record is
the commit log; the `Last seen` below is advisory.

Next ID: 30291
Last seen: 2026-08-21T20:37Z
Capabilities: arm64-macos native · x86_64-macos via Rosetta · x86_64-PE via wine (canary) · gpu-metal (Apple GPU) · NO x86_64-linux/qemu-user · NO arm64-WoA
STATUS: 2026-08-21T20:37Z (goal=INSTRUCTIONS.md) — **ACTIVE CLAIM: T-lin-10469 [S] loop UNROLLING (P2, IN_PROGRESS @8b91f904b)** — the highest AVAILABLE mission tier while both P1 mac lanes stay DEPS-blocked (T-lin-10478[P]-mac on T-lin-10476[C], lin in-flight @82de22d9a; T-lin-10489[X]mac on T-lin-10404+10482[P]). Design published: DETAILS#t-lin-10469-loop-unroll-design (default-off `-funroll-loops` knob → inert/o0-neutral/self-host-safe; hooks the existing loop-nest+IV+bound framework like ast_interchange_run; slice 1 = op-3 constant-trip full-unroll + TDD + arm64-osx exec; MANDATORY self-host validation each slice per the 10422 lesson). NEXT: implement slice 1. **10478-mac stays INSTANT-READY** (both Mach-O `-run` legs green O0/O2/O4) — will PARK 10469 and pivot to it the moment 10476[C] lands (P1>P2). 10477 deferred to lin (DETAILS#t-lin-10477-mac-probe). GPU/Metal backlog = [D] CAP gpu-metal (T-mac-30108/30064/30021). Full RESUME HANDOFF = top of `docs/log/mac-arm64.md`. Next mint 30291 (30270–89 intentionally skipped after the T-mac-30290 N9 fix).
