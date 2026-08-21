/* T-win-50041 Bug B: (__int256)large-negative-double -> 0 on arm64-Windows.
   Hypothesis (round 9): __mcc_i256_from_f64's negative branch passes `-x` to
   w256_from_double_mag whose `if(!(x>=1.0))return;` leaves 0 — so a miscompiled
   `-x` (unary minus of a large-negative double PARAM) < 1.0 gives exactly 0.
   -42 works because -(-42)=42 is small. Test negp()/ge1() to pinpoint. */
#include <stdio.h>

static void show(const char *n, __int256 v) {
	unsigned long long *l = (unsigned long long *)&v;
	printf("%-10s %016llx%016llx%016llx%016llx\n", n, l[3], l[2], l[1], l[0]);
}

/* mirror __mcc_i256_from_f64's inner steps on a double PARAMETER */
static double __attribute__((noinline)) negp(double x) { return -x; }
static int __attribute__((noinline)) ge1(double x) { return x >= 1.0; }

int main(void) {
	volatile double vx = -1e30;
	show("runtime", (__int256)vx);
	show("const", (__int256)-1e30);
	show("pos", (__int256)1e30);
	show("negsmall", (__int256)-42.0);

	/* the hypothesis probes: -x of a large-negative double param, and >=1.0 on it */
	unsigned long long b;
	double n30 = negp(-1e30);            /* want +1e30 = 0x46293e5939a08cea */
	__builtin_memcpy(&b, &n30, 8);
	printf("negp(-1e30) bits=%016llx want 46293e5939a08cea\n", b);
	double n42 = negp(-42.0);            /* want +42 = 0x4045000000000000 */
	__builtin_memcpy(&b, &n42, 8);
	printf("negp(-42)   bits=%016llx want 4045000000000000\n", b);
	printf("ge1(negp(-1e30)) = %d want 1\n", ge1(negp(-1e30)));
	printf("ge1(negp(-42))   = %d want 1\n", ge1(negp(-42.0)));
	printf("(-1e30 < 0.0)    = %d want 1\n", vx < 0.0);
	return 0;
}
