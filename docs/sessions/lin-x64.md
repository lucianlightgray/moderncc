# Session: lin-x64  (Linux / x64 / band 10000–29999)

Only lin-x64 writes this file. Authoritative session state (replaces the shared
`TODO.md` Sessions-table row; see `INSTRUCTIONS.md` §4.1). Liveness of record is
the commit log (`git log -1 --format=%cI` by this author); the `Last seen` below
is advisory.

Next ID: 10492
Last seen: 2026-08-21T21:23Z
Capabilities: x86_64-linux native · arm64/riscv64/i386 via qemu-user · x86_64-PE via wine (canary) · NO gpu-vulkan (device-blocked: vulkaninfo hangs)
STATUS: idle — no active lin claims. Resumed the T-lin-10458 type-expansion program this session: (1) closed T-lin-10458 [C] DONE+archived (contract published fc6b3222d, consumed); (2) landed child T-lin-10462 _Imaginary SLICE 1 (beac1967f) — lifted the parse refusal, mk_imaginary_type (1-field struct + new SymAttr is_imaginary bit), sizeof/_Generic/compat/errors, o0-neutral, cli/imaginary_type[_errors]; PARKED with RESUME=slice2 (Annex-G arithmetic). Clean resume points remain: 10459 int128/f128 ungate (recipe published, no slice landed — deep carry-op), 10460/10461 slice2+ tails, 10462 slice2. Earlier this session: T-lin-10476 [C] JIT-coverage matrix DONE (d40e1a903, arm64/riscv64 run-verified; mac's unroll cell verified green on all my legs; win/mac wiring 10478 legs), win alloca(0) ELF confirmed. Log: docs/log/lin-x64.md
