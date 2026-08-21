# Session: lin-x64  (Linux / x64 / band 10000–29999)

Only lin-x64 writes this file. Authoritative session state (replaces the shared
`TODO.md` Sessions-table row; see `INSTRUCTIONS.md` §4.1). Liveness of record is
the commit log (`git log -1 --format=%cI` by this author); the `Last seen` below
is advisory.

Next ID: 10492
Last seen: 2026-08-21T20:20Z
Capabilities: x86_64-linux native · arm64/riscv64/i386 via qemu-user · x86_64-PE via wine (canary) · NO gpu-vulkan (device-blocked: vulkaninfo hangs)
STATUS: IN_PROGRESS T-lin-10476 [C] JIT-coverage matrix (P1, unblocks T-lin-10478/win+mac). Stage A landed 82de22d9a — optfire.sh RUNNER hook (OPTFIRE_RUN prefix + OPTFIRE_EXECVIA=run), native green + arm64/qemu -run leg PROVEN end-to-end. Interface published: DETAILS #t-lin-10476-impl-interface. Next: stage B = CMake triple-indexed table + per-triple bootstrap fixture. Log: docs/log/lin-x64.md
