# tests/optimizers — optimization coverage suite (T-lin-10466)

One **partial** per optimization: a small, self-contained `.c` of deliberately
UNOPTIMIZED code that mcc *should* transform at `-O1`..`-O4`. Naming: `<opt>N.c`
(constfold1.c, constfold2.c, dce1.c, cse1.c, licm1.c, strength_reduce1.c,
inline1.c, dse1.c, vrp1.c, sccp1.c, tailcall1.c, gvn1.c, reassoc1.c,
switch_convert1.c, if_convert1.c, unroll1.c, …).

**mix/** holds programs that combine partials so several optimizations must
compose (e.g. constfold-then-dce, licm-then-cse).

Each cell asserts its optimization actually FIRED (not just that output is
correct) — mechanism chosen per category once the mcc `--stats`/opt-fire ledger
map is finalized (T-lin-10468):
- `--stats` counter delta (preferred where mcc exposes a counter),
- `-O0` vs `-On` instruction/byte diff (the transform must change codegen),
- gcc `-O2` behavioral or asm-shape parity,
- disassembly grep for the expected instruction pattern.

Correctness is always also checked (exec RC / value), so a "fired but wrong"
optimization fails loudly. Gaps found here (a partial mcc does NOT optimize)
become per-gap fix tasks via T-lin-10468.

Status: SEED. The union optimization catalog (T-lin-10467, agent-driven) and the
mcc-knob map (T-lin-10468) populate the concrete partial list + assertion harness.
