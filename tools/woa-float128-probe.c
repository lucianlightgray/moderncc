/* T-win-50023 diagnostic probe: run each check from tests/exec/types/float128.c
   individually and print its value, so the failing arm64-Windows __float128
   check is observable (the real test prints nothing on any failure). Compiled
   + run by tools/woa-diag.sh on a windows-11-arm runner. Integer/hex prints
   only (no %f) so a broken printf-float path cannot hide the real value. */
#include <stdio.h>
#include <string.h>

typedef unsigned long long u64;

static void bits(const char *n, __float128 x) {
	u64 lo, hi;
	memcpy(&lo, (char *)&x, 8);
	memcpy(&hi, (char *)&x + 8, 8);
	printf("%-16s bits=%016llx:%016llx\n", n, hi, lo);
}
static u64 hi_of(__float128 x) {
	u64 h;
	memcpy(&h, (char *)&x + 8, 8);
	return h;
}
static __float128 addq(__float128 a, __float128 b) { return a + b; }
static __float128 mulq(__float128 a, __float128 b) { return a * b; }

struct S { __float128 q; int tag; };

int main(void) {
	printf("C1  sizeof(__float128)=%llu  want 16\n", (u64)sizeof(__float128));

	bits("C2 (f128)1.5", (__float128)1.5);
	printf("C2  hi_of(1.5)=%016llx  want 3fff800000000000\n", hi_of((__float128)1.5));

	volatile double m = 1000000.0;
	__float128 p = (__float128)m * (__float128)m;
	bits("C3 1e6*1e6", p);
	printf("C3  (ll)p=%lld  want 1000000000000\n", (long long)p);

	__float128 sum = addq((__float128)2, (__float128)5);
	bits("C4 addq(2,5)", sum);
	printf("C4  (ll)sum=%lld  want 7\n", (long long)sum);

	__float128 prod = mulq((__float128)1.5, (__float128)2);
	bits("C5 mulq(1.5,2)", prod);
	printf("C5  (ll)prod=%lld  want 3\n", (long long)prod);

	__float128 a = (__float128)7, b = (__float128)3;
	bits("C6 a=7", a);
	bits("C6 -a", -a);
	printf("C6  (ll)(-a)=%lld  want -7\n", (long long)(-a));

	printf("C7  cmp a>b=%d a<b=%d a>=a=%d a<=a=%d a==a=%d a!=b=%d  want 1 0 1 1 1 1\n",
	       (a > b), (a < b), (a >= a), (a <= a), (a == a), (a != b));

	printf("C8  (int)(f128)-5=%d  want -5\n", (int)(__float128)(-5));
	printf("C9  (uint)(f128)5u=%u  want 5\n", (unsigned)(__float128)5u);
	printf("C10 (ll)(1e6*1e6)=%lld  want 1000000000000\n",
	       (long long)((__float128)1000000 * (__float128)1000000));
	printf("C11 (ll)(f128)2.5=%lld  want 2 (trunc)\n", (long long)(__float128)2.5);

	struct S s;
	s.q = (__float128)42.5;
	s.tag = 9;
	bits("C12 s.q", s.q);
	printf("C12 (ll)s.q=%lld s.tag=%d  want 42 9\n", (long long)s.q, s.tag);

	static __float128 g;
	bits("C13 g(static)", g);
	printf("C13 (ll)g=%lld  want 0\n", (long long)g);

	return 0;
}
