/* Tier-4 per-site specialization (docs/AST.md §19.3). With MCC_AST_INLINE + MCC_AST_TEMPLATES,
 * a CONSTANT argument bound to a READ-ONLY param is constant-propagated: its value is
 * substituted at the param's Ref sites during the graft (no slot store + reload), so gen_op
 * folds the resulting constant arithmetic and — inside the graft (pass 2, not byte-verified) —
 * a condition that folds to a compile-time constant selects its taken branch and DROPS the
 * dead one entirely. The graft is exec-golden gated (an inlined caller's bytes diverge from
 * -O0), and REPLAYED=main + SPECIALIZES asserts the substitution fired.
 *
 * Correctness idea: each `choose` call passes a CONSTANT flag, so the dead branch is
 * eliminated. The dead branch of each returns a DIFFERENT value than the live one, so if the
 * wrong branch (or both) were emitted the exit code would differ — the exec-golden equality
 * to -O0 is the real gate, the SPECIALIZES dump proves the path was taken not bypassed. */

/* read-only params flag/a/b — a constant flag folds and its dead branch vanishes */
static int choose(int flag, int a, int b) {
	if (flag)
		return a + 1;
	else
		return b + 2;
}
/* nested constant condition: `k` folds, the `k > 0` branch is selected per site */
static int clampk(int x, int k) {
	if (k > 0)
		return x + k;
	return x - k;
}
/* constant-arg arithmetic fold: both args const at the call -> pure constant */
static int mul(int x, int y) { return x * y; }
/* a runtime (non-constant) arg still binds via a slot — specialization is per-arg */
static int addk(int x, int k) { return x + k; }

int main(void) {
	int p = choose(1, 10, 99);  /* flag=1 -> 10+1 = 11 (else dead) */
	int q = choose(0, 99, 20);  /* flag=0 -> 20+2 = 22 (then dead) */
	int c = clampk(5, 3);       /* k=3>0 -> 5+3 = 8 (else dead) */
	int m = mul(4, 6);          /* both const -> 24 */
	int v = 10;                 /* runtime value */
	int r = addk(v, 5);         /* x runtime, k=5 const -> 15 */
	return p + q + c + m + r - 38; /* 11+22+8+24+15 - 38 = 42 */
}
