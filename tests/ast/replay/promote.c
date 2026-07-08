/* Tier-3 register promotion (docs/AST.md §4/§10/§18.2 Tier 3). A call-free function's
 * address-not-taken integer locals are kept in pinned caller-saved registers
 * (R10/R9/R8 — R10 is used nowhere in the x86_64 backend, R8/R9 only for call args,
 * so a call-free leaf owns them; R11 is excluded — it backs `load`/GOTPCREL) with no
 * stack load/store traffic. The register is seeded from the local's stack slot at
 * function entry, so promotion is valid across arbitrary control flow (loops/if) and
 * even for a parameter or a local read before written — the first optimization that
 * deliberately beats -O0. Strictly opt-in (MCC_AST_PROMOTE); byte-verify is bypassed
 * for a promoted function, so the exit-code equality vs -O0 is the gate. REPLAYED +
 * PROMOTES target loopy (call-free, promotion across a loop) and callful (promotion
 * into callee-saved RBX/R12–R15 that survives an intervening call). */

int ident(int x); /* forward decl; defined below so callful has a real call to cross */

/* Straight-line: a, b, c live in registers with no stack traffic. */
static int calc(int n) {
	int a = n + 1;
	int b = a * 3;
	int c = b + a;
	a = c - b;
	return a + b + c - 6; /* n=5: 6+18+24-6 = 42 */
}

/* Control flow — the real payoff: the accumulator s and the loop-carried value v
 * live in registers across the loop body and back-edge. (i uses ++, so it is
 * poisoned and stays in memory; s and v are promoted, seeded from their slots.) */
static int loopy(int start) {
	int s = 0;
	int v = start;
	for (int i = 0; i < 5; i++) {
		s = s + v;
		v = v + 2;
	}
	return s; /* start=4: 4+6+8+10+12 = 40 */
}

int ident(int x) { return x; }

/* Call-ful: p, q, r must survive the ident() calls, so they are promoted into
 * callee-saved registers (pushed at entry, popped at the return funnel) rather than
 * the caller-saved pool a call would clobber. */
static int callful(int start) {
	int p = ident(start);
	int q = ident(start + 1);
	int r = ident(start + 2);
	int t = ident(p + q + r);
	return p + q + r + t; /* start=1: 1+2+3 + 6 = 12 */
}

int main(void) {
	return calc(5) + loopy(4) + callful(1) - 52; /* 42 + 40 + 12 - 52 = 42 */
}
