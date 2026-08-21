# Session: lin-x64  (Linux / x64 / band 10000–29999)

Only lin-x64 writes this file. Authoritative session state (replaces the shared
`TODO.md` Sessions-table row; see `INSTRUCTIONS.md` §4.1). Liveness of record is
the commit log (`git log -1 --format=%cI` by this author); the `Last seen` below
is advisory.

Next ID: 10492
Last seen: 2026-08-21T21:23Z
Capabilities: x86_64-linux native · arm64/riscv64/i386 via qemu-user · x86_64-PE via wine (canary) · NO gpu-vulkan (device-blocked: vulkaninfo hangs)
STATUS: idle — no active lin claims. Landed T-lin-10476 [C] JIT-coverage matrix (P1) DONE + archived, SHA d40e1a903: optfire triple-matrix + qemu RUNNER hook, arm64/riscv64 ELF legs run-verified via -run (optfire 302/302, full-suite 19 reds all pre-existing T-lin-10092 chronic). Unblocked T-lin-10478[P] (mac/win legs) + 10479/10480; peers notified (CLAIM). Interface: DETAILS #t-lin-10476-impl-interface. Pending: confirm win T-win-50048 alloca(0) x86_64-ELF leg (mccrt rebuild). NOTE: T-lin-10458 [C] carries a >3h-stale lin-x64 claim (heartbeat 12:40Z) — candidate for re-OPEN. Log: docs/log/lin-x64.md
