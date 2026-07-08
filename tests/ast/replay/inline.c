/* Tier-4 virtual always-inline (docs/AST.md §9, docs/TODO.md Tier 4). A within-TU
 * static leaf helper whose body is `return EXPR;` (scalar params only) is retained at
 * its definition and grafted into a later caller in place of the boundary Call: the
 * args bind to relocated param slots and the return expression is replayed under a
 * frame bias. Opt-in MCC_AST_INLINE; an inlined caller's bytes diverge from -O0, so the
 * exit-code equality vs -O0 is the gate (byte-verify is bypassed for it, like promotion).
 * REPLAYED targets main; INLINES asserts add/scale were grafted (no boundary call). */

static int add(int a, int b) { return a + b; }
static int scale(int x, int k) { return x * k; }
/* multi-statement straight-line body with a local — its slot relocates under the same
 * frame bias as the params. */
static int madd(int a, int b) {
	int p = a * b;
	int q = p + a;
	return q - 1;
}

int main(void) {
	int r = add(3, 4);              /* 7 */
	int s = scale(5, 6);           /* 30 */
	int t = add(scale(2, 3), r);   /* nested: 6 + 7 = 13 */
	int u = madd(2, 3);            /* p=6, q=8, -> 7 */
	return r + s + t + u - 15;     /* 7 + 30 + 13 + 7 - 15 = 42 */
}
