/* C23 6.2.5 _BitInt(N), slice 1 (N <= 64): a storage integer whose value is
 * reduced to N bits.  Verified byte-for-byte against gcc-16 across width
 * classes and operations. */
#if defined test_props

#include <stdio.h>
int main(void) {
	printf("maxwidth %d\n", __BITINT_MAXWIDTH__);
	printf("size 1=%zu 7=%zu 8=%zu 9=%zu 16=%zu 17=%zu 32=%zu 33=%zu 64=%zu\n",
				 sizeof(unsigned _BitInt(1)), sizeof(_BitInt(7)), sizeof(_BitInt(8)),
				 sizeof(_BitInt(9)), sizeof(_BitInt(16)), sizeof(_BitInt(17)),
				 sizeof(_BitInt(32)), sizeof(_BitInt(33)), sizeof(_BitInt(64)));
	printf("align 9=%zu 17=%zu 33=%zu\n", _Alignof(_BitInt(9)),
				 _Alignof(_BitInt(17)), _Alignof(_BitInt(33)));
	return 0;
}

#elif defined test_arith

#include <stdio.h>
int main(void) {
	signed _BitInt(7) a = 60; a += 100;			/* wraps mod 128 into [-64,63] */
	unsigned _BitInt(9) b = 500; b += 100;			/* wraps mod 512 */
	signed _BitInt(37) c = 100000, d = 100000;
	signed _BitInt(33) e = 1; e <<= 32;			/* sign bit set -> negative */
	unsigned _BitInt(20) f = 999999; f *= f;		/* wraps mod 2^20 */
	signed _BitInt(50) g = -7; unsigned _BitInt(50) h = 3;
	printf("s7 %d\n", (int)a);
	printf("u9 %u\n", (unsigned)b);
	printf("s37mul %lld\n", (long long)(c * d));
	printf("s33shl %lld\n", (long long)e);
	printf("u20sq %u\n", (unsigned)f);
	printf("s50 %lld %lld %lld\n", (long long)(g / h), (long long)(g % h),
				 (long long)(g * (signed _BitInt(50))h));
	return 0;
}

#elif defined test_conv

#include <stdio.h>
int main(void) {
	signed _BitInt(20) a = -5;
	unsigned _BitInt(20) b = 1000000;
	printf("to %lld %u %d\n", (long long)a, (unsigned)a, (int)a);
	printf("uto %d %ld\n", (int)b, (long)b);
	_BitInt(40) big = 1000000000;
	printf("mixll %lld\n", (long long)(big * big));		/* wraps mod 2^40 */
	int n = 5000; _BitInt(20) m = 500000;
	printf("mixint %lld\n", (long long)(m + n));
	printf("cfold %d %d %d\n", (int)(_BitInt(12))70000,
				 (int)(_BitInt(20))2000000, (int)(unsigned _BitInt(7))(-1));
	return 0;
}

#elif defined test_aggregate

#include <stdio.h>
#include <stddef.h>
struct P { _BitInt(12) a; unsigned _BitInt(28) b; long long c; };
static _BitInt(40) g = -12345678901;
static unsigned _BitInt(9) gu = 700;
_BitInt(33) addone(_BitInt(33) x) { return x + 1; }
int main(void) {
	struct P p = { -3, 200000000u, 999 };
	printf("struct %lld %llu %lld sz=%zu off=%zu,%zu,%zu\n",
				 (long long)p.a, (unsigned long long)p.b, p.c, sizeof(struct P),
				 offsetof(struct P, a), offsetof(struct P, b), offsetof(struct P, c));
	p.a = p.a - 1; p.b += 100000000u;
	printf("struct2 %lld %llu\n", (long long)p.a, (unsigned long long)p.b);
	printf("glob %lld %u\n", (long long)g, (unsigned)gu);
	_BitInt(17) arr[3] = { 0, -1, 70000 };			/* 70000 wraps mod 2^17 */
	printf("arr %lld %lld %lld\n", (long long)arr[0], (long long)arr[1],
				 (long long)arr[2]);
	printf("func %lld\n", (long long)addone(42));
	return 0;
}

#endif
