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
/* internal control flow (two ifs) with a single tail return: the branches use fresh
 * code offsets, so no label scoping is needed. */
static int clamp(int x, int lo, int hi) {
	int r = x;
	if (x < lo)
		r = lo;
	if (x > hi)
		r = hi;
	return r;
}
/* early + tail returns: each stores to the graft's result slot and (if non-tail) jumps to
 * the inline-end join — a phi via memory, so multiple such grafts feeding one call each own
 * a distinct slot instead of fighting over a return register. */
static int sgn(int x) {
	if (x < 0)
		return -1;
	if (x > 0)
		return 1;
	return 0;
}
/* scalar float params + return: args bind via vstore, the result coalesces through a
 * double-typed result slot. */
static double area(double w, double h) { return w * h; }
/* non-leaf callee: its own call to the graftable `add` grafts recursively (guarded
 * against cycles by the graft stack). */
static int quad(int x) { return add(x, x) * 2; }
/* a `switch` (case/default) and a loop with an early `break` — self-contained control
 * flow: their break/case AST_Jumps target the callee's own switch/loop, not the caller's. */
static int pick(int k) {
	switch (k) {
	case 1:
		return 3;
	case 2:
		return 5;
	default:
		return 0;
	}
}
static int firsthit(const int *a, int n) {
	for (int i = 0; i < n; i++)
		if (a[i])
			return i;
	return n;
}
/* struct-by-value RETURN with scalar params: coalesces through a struct-sized result
 * slot (memory-agnostic). (Struct *params* are ABI-dependent and stay a real call.) */
struct Pair {
	int a, b;
};
static struct Pair mkpair(int a, int b) {
	struct Pair p;
	p.a = a;
	p.b = b;
	return p;
}
/* `goto` + a named label: the callee's labels are scoped (label floor) so they don't
 * collide with the caller's — main has no label here, but see the loop below. */
static int gsum(int n) {
	int s = 0, i = 0;
loop:
	if (i < n) {
		s += i;
		i++;
		goto loop;
	}
	return s;
}

int main(void) {
	int r = add(3, 4);              /* 7 */
	int s = scale(5, 6);           /* 30 */
	int t = add(scale(2, 3), r);   /* nested: 6 + 7 = 13 */
	int u = madd(2, 3);            /* p=6, q=8, -> 7 */
	int c = clamp(99, 0, 5);       /* -> 5 */
	int g = sgn(-4) + sgn(0) + sgn(9); /* early/tail returns, multi-arg: -1+0+1 = 0 */
	int d = (int)area(2.5, 4.0);       /* double params + return: 10.0 -> 10 */
	int q = quad(2);                   /* non-leaf: add(2,2)*2 = 8 (add grafts inside) */
	int arr2[3] = {0, 0, 7};
	int p = pick(2) + firsthit(arr2, 3); /* switch: 5, break-loop: 2 -> 7 */
	struct Pair pr = mkpair(2, 3);       /* struct return: {2,3} */
	int m = pr.a + pr.b;                 /* 5 */
	int gs = gsum(5);                    /* goto loop: 0+1+2+3+4 = 10 */
	return r + s + t + u + c + g + d + q + p + m + gs - 60;
	/* 7+30+13+7+5+0+10+8+7+5+10 - 60 = 42 */
}
