/* Tier-3 register promotion (docs/AST.md §4/§10/§18.2 Tier 3). `calc` is a
 * single-BasicBlock, call-free function, so its address-not-taken integer locals
 * (a, b, c) are promoted into pinned caller-saved registers (R11/R10/R9) with no
 * stack load/store traffic — the first optimization that deliberately beats -O0.
 * The parameter `n` is read before it is written, so it is NOT promoted (its ABI
 * slot value is still live). Strictly opt-in (MCC_AST_PROMOTE); byte-verify is
 * bypassed for a promoted function, so this exit-code equality vs -O0 is the gate.
 * REPLAYED + PROMOTES target calc. */
static int calc(int n) {
	int a = n + 1; /* 6 */
	int b = a * 3; /* 18 */
	int c = b + a; /* 24 */
	a = c - b;     /* 6  */
	return a + b + c - 6; /* 6 + 18 + 24 - 6 = 42 */
}

int main(void) {
	return calc(5);
}
