/* T-win-50025 diagnostic: arm64-Windows (__int256)negative-double gives 0.
   Distinguish the compile-time const-fold path from the runtime
   __mcc_i256_from_f64 path, and check plain double negation. */
#include <stdio.h>

typedef unsigned long long u64;

static void p256(const char *n, __int256 v) {
	u64 *l = (u64 *)&v;
	printf("%-14s %016llx%016llx%016llx%016llx\n", n, l[3], l[2], l[1], l[0]);
}

/* mirrors __mcc_i256_from_f64's signature (ptr, double, int) to test whether
   the trailing int arg survives the arm64-Windows call ABI when a double sits
   between the two integer args */
static int __attribute__((noinline)) abitest(void *p, double d, int flag) {
	(void)p; (void)d;
	return flag;
}

typedef unsigned long long L;
static void repro_mag(L r[4], double x) {
	int i;
	for (i = 0; i < 4; i++) r[i] = 0;
	if (x >= 1.0) r[0] = 1;          /* nonzero magnitude marker */
}
static void repro_neg(L r[4], const L a[4]) {
	L z[4] = {0, 0, 0, 0}, borrow = 0;
	int i;
	for (i = 0; i < 4; i++) {
		L d = z[i] - a[i], b1 = z[i] < a[i], t = d - borrow, b2 = d < borrow;
		r[i] = t;
		borrow = b1 | b2;
	}
}
static void __attribute__((noinline)) repro_fromf(L r[4], double x, int u) {
	if (x < 0.0) {
		if (u) { int i; for (i = 0; i < 4; i++) r[i] = 0; return; }
		repro_mag(r, -x);
		repro_neg(r, r);
		return;
	}
	repro_mag(r, x);
}

int main(void) {
	double x = -1e30;
	volatile double vx = -1e30;

	printf("abitest(p,-1e30,0)=%d want 0\n", abitest(&x, -1e30, 0));
	printf("abitest(p,-1e30,7)=%d want 7\n", abitest(&x, -1e30, 7));
	printf("abitest(p,+1e30,1)=%d want 1\n", abitest(&x, 1e30, 1));

	u64 nb;
	double nx = -x;                 /* want +1e30 */
	__builtin_memcpy(&nb, &nx, 8);
	printf("neg(-1e30) bits=%016llx (1e30 ~ 0x46293e5939a08cea)\n", nb);

	/* replicate __mcc_i256_from_f64's negative path step by step */
	printf("vx<0.0 = %d want 1\n", vx < 0.0);
	{ double nvx = -vx; u64 nn; __builtin_memcpy(&nn, &nvx, 8);
	  printf("(-vx) bits=%016llx want 46293e5939a08cea\n", nn); }
	p256("(i256)(-vx)", (__int256)(-vx));   /* positive magnitude conv, want +1e30 */
	printf("abitest(p,-vx,9)=%d want 9\n", abitest(&x, -vx, 9));

	/* self-contained minimal repro of __mcc_i256_from_f64's negative branch */
	{
		unsigned long long out[4];
		repro_fromf(out, vx, 0);   /* want all-ffff (=-1) if the branch works */
		printf("repro fromf(-1e30,0) = %016llx%016llx%016llx%016llx  want all-f\n",
		       out[3], out[2], out[1], out[0]);
	}

	p256("const-neg", (__int256)-1e30);     /* compile-time constant */
	p256("var-neg", (__int256)vx);          /* runtime: __mcc_i256_from_f64 */
	p256("var-x", (__int256)x);             /* x=-1e30 (maybe folded) */
	p256("pos", (__int256)1e30);
	p256("pos-then-neg", -(__int256)1e30);  /* runtime __int256 negate */
	p256("neg-small", (__int256)-123456789.75);
	return 0;
}
