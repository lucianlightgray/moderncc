/* T-win-50025 diagnostic: arm64-Windows (__int256)negative-double gives 0.
   Distinguish the compile-time const-fold path from the runtime
   __mcc_i256_from_f64 path, and check plain double negation. */
#include <stdio.h>

typedef unsigned long long u64;

static void p256(const char *n, __int256 v) {
	u64 *l = (u64 *)&v;
	printf("%-14s %016llx%016llx%016llx%016llx\n", n, l[3], l[2], l[1], l[0]);
}

int main(void) {
	double x = -1e30;
	volatile double vx = -1e30;

	u64 nb;
	double nx = -x;                 /* want +1e30 */
	__builtin_memcpy(&nb, &nx, 8);
	printf("neg(-1e30) bits=%016llx (1e30 ~ 0x46293e5939a08cea)\n", nb);

	p256("const-neg", (__int256)-1e30);     /* compile-time constant */
	p256("var-neg", (__int256)vx);          /* runtime: __mcc_i256_from_f64 */
	p256("var-x", (__int256)x);             /* x=-1e30 (maybe folded) */
	p256("pos", (__int256)1e30);
	p256("pos-then-neg", -(__int256)1e30);  /* runtime __int256 negate */
	p256("neg-small", (__int256)-123456789.75);
	return 0;
}
