/* T-win-50041 Bug B: (__int256)large double miscompiles on arm64-Windows.
   Round 12 — distinguish "negative-branch-specific" from "runtime large-left-
   shift broken regardless of sign". `vpos` forces the RUNTIME positive path
   (w256_from_double_mag + mcc_w256_shl) — const cases are const-folded. */
#include <stdio.h>

static void show(const char *n, __int256 v) {
	unsigned long long *l = (unsigned long long *)&v;
	printf("%-14s %016llx%016llx%016llx%016llx\n", n, l[3], l[2], l[1], l[0]);
}

int main(void) {
	volatile double vneg30 = -1e30, vpos30 = 1e30;   /* e≈47 (large shl) */
	volatile double vpos18 = 1e18, vneg18 = -1e18;   /* e≈8  (small shl) */
	volatile double vneg6 = -1e6;                    /* e<0  (shr) */
	show("run-neg-1e30", (__int256)vneg30);          /* negative large */
	show("run-pos-1e30", (__int256)vpos30);          /* POSITIVE large runtime — KEY */
	show("run-pos-1e18", (__int256)vpos18);          /* positive small-shl runtime */
	show("run-neg-1e18", (__int256)vneg18);          /* negative small-shl */
	show("run-neg-1e6", (__int256)vneg6);            /* negative shr path */
	show("cfold-neg1e30", (__int256)-1e30);          /* const-fold negative */
	show("cfold-pos1e30", (__int256)1e30);           /* const-fold positive */
	return 0;
}
