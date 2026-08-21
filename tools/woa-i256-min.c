/* T-win-50041 Bug B minimal repro: (__int256)negative-double -> 0 on arm64-Windows.
   Expect on the runner: runtime=0, const=0 (WRONG), pos=nonzero. On win-x86_64 all
   three are correct. Compile+link+run; -O0 already reproduces (Bug B is not opt-gated). */
#include <stdio.h>

static void show(const char *n, __int256 v) {
	unsigned long long *l = (unsigned long long *)&v;
	printf("%-8s %016llx%016llx%016llx%016llx\n", n, l[3], l[2], l[1], l[0]);
}

int main(void) {
	volatile double vx = -1e30;
	show("runtime", (__int256)vx);     /* runtime __mcc_i256_from_f64(neg) */
	show("const",   (__int256)-1e30);  /* compile-time const-fold */
	show("pos",     (__int256)1e30);   /* positive control: must be nonzero */
	show("negsmall",(__int256)-42.0);  /* small negative: want ...ffffd6 */
	return 0;
}
