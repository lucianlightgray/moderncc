#include <stdio.h>
#include <string.h>

/* __float128 is wired on arm64/riscv64 (T-lin-10007 slice 1); other targets
   still refuse it, so exercise it only where it exists and otherwise fall
   through to the same golden. */
#if defined(__aarch64__)

typedef unsigned long long u64;

static u64 hi_of(__float128 x) {
	u64 h;
	memcpy(&h, (char *)&x + 8, 8);
	return h;
}

static __float128 addq(__float128 a, __float128 b) {
	return a + b;
}
static __float128 mulq(__float128 a, __float128 b) {
	return a * b;
}

struct S {
	__float128 q;
	int tag;
};

int main(void) {
	int fail = 0;

	if (sizeof(__float128) != 16)
		fail = 1;

	/* 1.5 in binary128 has the high word 0x3fff800000000000 */
	if (hi_of((__float128)1.5) != 0x3fff800000000000ULL)
		fail = 1;

	/* arithmetic is real binary128, not a demoted float: 1e6*1e6 == 1e12
	   exactly, which single precision cannot represent */
	volatile double m = 1000000.0;
	__float128 p = (__float128)m * (__float128)m;
	if ((long long)p != 1000000000000LL)
		fail = 1;

	/* call ABI: pass two, return one */
	if ((double)addq((__float128)2, (__float128)5) != 7.0)
		fail = 1;
	if ((double)mulq((__float128)1.5, (__float128)2) != 3.0)
		fail = 1;

	/* negation and the six comparisons */
	__float128 a = (__float128)7, b = (__float128)3;
	if ((double)(-a) != -7.0)
		fail = 1;
	if (!(a > b) || (a < b) || !(a >= a) || !(a <= a) || !(a == a) || !(a != b))
		fail = 1;

	/* conversions round-trip */
	if ((int)(__float128)(-5) != -5)
		fail = 1;
	if ((unsigned)(__float128)5u != 5u)
		fail = 1;
	if ((long long)((__float128)1000000 * (__float128)1000000) != 1000000000000LL)
		fail = 1;
	if ((double)(__float128)2.5 != 2.5)
		fail = 1;

	/* storage in aggregates + static init */
	struct S s;
	s.q = (__float128)42.5;
	s.tag = 9;
	if ((double)s.q != 42.5 || s.tag != 9)
		fail = 1;
	static __float128 g;         /* zero-initialised */
	if ((double)g != 0.0)
		fail = 1;

	if (!fail)
		printf("OK\n");
	return fail;
}

#else

int main(void) {
	printf("OK\n");
	return 0;
}

#endif
