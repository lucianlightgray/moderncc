# Session: mac-arm64  (macOS / arm64 / band 30000–49999)

Only mac-arm64 writes this file. Authoritative session state (replaces the shared
`TODO.md` Sessions-table row; see `INSTRUCTIONS.md` §4.1). Liveness of record is
the commit log; the `Last seen` below is advisory.

Next ID: 30292
Last seen: 2026-08-21T21:57Z
Capabilities: arm64-macos native · x86_64-macos via Rosetta · x86_64-PE via wine (canary) · gpu-metal (Apple GPU) · NO x86_64-linux/qemu-user · NO arm64-WoA
STATUS: 2026-08-21T21:57Z (goal=INSTRUCTIONS.md) — **T-lin-10478/mac leg DONE (code 13f5e8c77): P1 Mach-O JIT `-run` coverage LANDED** — optfire differ cells run-verified via `-run` on optfire-arm64-osx (native) + optfire-x86_64-osx (Rosetta), 92/92 green; self-contained if(APPLE) block, arch.txt pins, x64boot freshness fixture, gate-symmetric skips; native optfire 149/149 unchanged (DETAILS#t-lin-10478-mac-coverage). Parent T-lin-10478 awaits win's PE leg (lin ELF/qemu + mac Mach-O legs done). Earlier this turn: **T-lin-10469 loop-unroll full-unroll DONE+ARCHIVED (code c5d075877)** — mcc now has a working default-off `-funroll-loops` (full-unroll of constant-trip for-loops), coverage.txt id32 gap→covered; full native suite green (cli 475/exec 374/diff3 319/selfhost 15/corpusgate 6; treegate's 2 reds = pre-existing docs/refs cprop-typecov, not mine). Residual (partial/runtime/wider-shapes/-O-parity-wiring) → minted **T-mac-30291** [S] P2. **PIVOTING to P1: T-lin-10476[C] LANDED (lin d40e1a903) → T-lin-10478[P]-mac now UNBLOCKED** — my instant-ready Mach-O `-run` legs wire into lin's `_of_matrix` per DETAILS#t-lin-10476-impl-interface §(5) (arm64-osx native + x86_64-osx Rosetta rows, os=macos runner branch, keep gate-symmetric else() mcc_skip_test). NEXT: claim + implement 10478-mac. 10477 phantom-cells still open (lin: mine after 10478-mac if I want). 10489[X]mac (P3 Metal) still DEPS on 10404+10482[P]. GPU/Metal = [D] CAP gpu-metal. Next mint 30292.
