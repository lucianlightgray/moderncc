# Session: win-x64  (Windows / x64 / band 50000–69999)

Only win-x64 writes this file. Authoritative session state (replaces the shared
`TODO.md` Sessions-table row; see `INSTRUCTIONS.md` §4.1). Liveness of record is
the commit log; the `Last seen` below is advisory.

Next ID: 50049
Last seen: 2026-08-21T22:51Z
Capabilities: x86_64-windows native (PE) · MSVC/cl + winlibs-ucrt mingw + scoop clang · WSL2 (x86_64-linux native validation) · gpu-vulkan (RTX 2060 — DEVICE-BLOCKED for fault tests without a game-off/TDR window) · NO x86_64-linux/macos native AOT · NO i386-toolchain · NO arm64-woa runner · NO rosetta/qemu
STATUS: 2026-08-21T22:33Z -- P1 T-lin-10477 optfire phantom-coverage cells DONE (2c171311 ledger honesty + 8934f442/1c09170c -fswitch-jumptable flag+cell). A source-verified firing-signal map showed 4 audited passes are UNIMPLEMENTED in mcc -> honest reclassification, not a fabricated cell (id10 covered|jt,sccp; id22/id49/id77 partial; id75 base); id79 switch-jumptable DOES exist -> exposed it as a first-class o0/On-neutral `-fswitch-jumptable` knob (SPECIAL default == old env/opt-search) + a real run-verified differ cell (optfire-x86_64/switch_jt PASS via mcc -run on PE). cover3 regen also fixed pre-existing unroll-loops staleness. cli 474/474, exec 374/374, optfire-x86_64 48/48, cover3+coverage-check green. Follow-up 468f367e: switch_jt cell needed -fPIC + i386 arch.txt pin (mac 30866a84e found x86_64-Mach-O refuses a jumptable unless pic; i386 refuses one under pic) -- test-infra only, win-PE re-verified PASS, mac/lin asked to re-run their legs. Earlier this session: T-lin-10478/win (P1 native-PE optfire -run leg, 86da969c). No active win claims. Parent T-lin-10478[P] open for lin/mac legs.