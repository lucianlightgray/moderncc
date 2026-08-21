# GOAL

Execute `INSTRUCTIONS.md` (the coordination loop) until `TODO.md` has no open
items — **but not breadth-first.** Work the three mission priorities **in order**:
exhaust higher-priority *available* work before pulling lower-priority work.

This thematic ordering is **subordinate to, and composes with**, the read-only
`INSTRUCTIONS.md` §11 task-TYPE order (`[C]` with dependents > `BLOCKER` inbox >
`[X]` > `[P]` > `[S]`) and the `DEPS` gate. Rule: **among the tasks §11 makes
takeable right now, take the one serving the highest mission priority.** Mission
priority never overrides a blocked `DEPS`, a `[C]` contract with waiters, or an
owned task's TTL.

---

## P1 — 100% JIT coverage on every OS  ← HIGHEST

Every AST/RIR construct mcc can compile must also **run correctly through the JIT
(`-run`) on every supported target triple** — x86_64 / arm64 / riscv64 across
Linux (ELF), macOS (Mach-O), and Windows (PE). Concretely, for every optimizer
pass and every language construct, on **every** triple, a test must prove both:
(a) it **fires** (bytes/counter change vs `-fno-`/`-O0`), and (b) the optimized
program **still computes the correct result when JIT-executed** on that OS/arch.

- A pass green only on x86_64-ELF is **NOT** covered — register slots and relocs
  rebind per target ([[verify-codegen-on-every-target]]).
- **"Fired-only"** (byte-diff with no execution) is a stopgap, never the goal.
  The current optfire cross-arch cells are fired-only; closing that is P1's core.

Foundation + fan-out: `DETAILS.md#t-lin-10476-jit-coverage-matrix` (contract) →
tasks **T-lin-10476..10480**. Motivating test-suite audit:
`DETAILS.md#t-lin-10476-optfire-audit`.
Known ceiling: **arm64-Windows (WoA)** has no execution path in the current fleet
— capped at fired-only until a runner exists (`QUESTIONS.md` Q-lin-10481).

## P2 — Optimizer parity with gcc/clang, then beyond

Bring mcc's optimization coverage to **parity with gcc and clang**, then **past
them** using strategies proven in other production compilers (LuaJIT, Julia,
Dart, V8, Go, Swift, …). Parity scoreboard: `tests/optfire/coverage.txt`.
Cross-language catalog + gap analysis: `DETAILS.md#t-lin-10467-lang-opt-catalog`,
`DETAILS.md#t-lin-10468-optimizer-gaps`.

Open gaps — parity: **T-lin-10469** (loop unroll), **10470** (auto-vec / SLP),
**10471** (GVN), **10473** (IPA const-prop / specialize). Beyond gcc/clang:
**10474** (escape-analysis → heap-to-stack + allocation sinking), **10475**
(whole-program TFA → devirt + tree-shake), **10425** (packed-vector / XMM-resident
codegen), **10455 / 10457** (type-substitution superoptimizer — novel to mcc).

## P3 — GPU coverage: every node/slice on the GPU (Vulkan + Metal)

Make **every** AST/RIR node and slice runnable on the GPU — via **Vulkan/SPIR-V**
and Apple **Metal/MSL** — **EXCEPT** anonymous / externally-linked `INVOKE` calls
(no device-side definition exists to emit). Research the Vulkan and Metal
coverage gaps, then implement the SPIR-V / MSL lowering and device dispatch for
every remaining node/slice category until the only refusals are the excepted
INVOKE class.

Program + fan-out: `DETAILS.md#t-lin-10482-gpu-node-coverage` → tasks
**T-lin-10482** (program) + children. The program anchor summarizes prior GPU
work (the strategy ladder 10402/03/04, JIT-inline 10410, coverage-measurement
10398) and the device-infra cluster in `TODO.md` (10033/40/45/48/50/52/58/61/74/
81/82).
