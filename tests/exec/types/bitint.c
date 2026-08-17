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

#elif defined test_temp

#include <stdio.h>
int main(void) {
	/* Widening a LIVE (non-stored) _BitInt(N) arithmetic result must reduce it
	 * mod 2^N before the conversion.  N <= 32 keeps its value in an INT/SHORT
	 * storage integer, so unlike the N > 32 sections above it never went through
	 * the LLONG operand-reduce -- the class the stored-path sections miss. */
	unsigned _BitInt(9) u = 500, v = 400;
	signed _BitInt(9) s = -100;
	unsigned _BitInt(20) w = 999999;
	unsigned _BitInt(32) x = 4000000000u;
	printf("uadd %llu\n", (unsigned long long)(u + u));		/* 1000 -> 488 */
	printf("umul %llu\n", (unsigned long long)(u * u));		/* 250000 -> 144 */
	printf("uaddv %llu\n", (unsigned long long)(u + v));		/* 900 -> 388 */
	printf("usub %llu\n", (unsigned long long)(v - u));		/* -100 -> 412 */
	printf("uchain %llu\n", (unsigned long long)((u + u) + (u + u)));	/* -> 464 */
	printf("sadd %lld\n", (long long)(s + s));			/* -200, no overflow */
	printf("wsq %llu\n", (unsigned long long)(w * w));		/* mod 2^20 = 428929 */
	printf("x2 %llu\n", (unsigned long long)(x + x));		/* mod 2^32 */
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

#elif defined test_call

/* Passing and returning a _BitInt(N) BY VALUE.  A _BitInt is the first scalar
 * carrying VT_BITFIELD that is passed as a whole argument, so the caller's
 * argument-register materialization (classify_x86_64_arg's bit-field retype)
 * must keep the value's N-bit precision -- a mis-typed argument reduces by a
 * garbage shift count and miscompiles on x86_64 (accidentally clean on arm64).
 * Spans all four storage classes: 7=BYTE, 9=SHORT, 20=INT, 33/64=LLONG. */
#include <stdio.h>
typedef signed _BitInt(7)    s7;
typedef unsigned _BitInt(9)  u9;
typedef signed _BitInt(20)   s20;
typedef unsigned _BitInt(33) u33;
typedef signed _BitInt(64)   s64b;
__attribute__((noinline)) s7   id7  (s7 x)           { return x; }
__attribute__((noinline)) u9   id9  (u9 x)           { return x; }
__attribute__((noinline)) s20  add20(s20 a, s20 b)   { return a + b; }
__attribute__((noinline)) u33  add33(u33 a, u33 b)   { return a + b; }
__attribute__((noinline)) s64b neg64(s64b x)         { return -x; }
__attribute__((noinline)) u9   twice9(u9 x)          { return id9(x) + id9(x); }
int main(void) {
	printf("id7 %d %d\n", (int)id7(60), (int)id7(-40));
	printf("id9 %u %u\n", (unsigned)id9(500), (unsigned)id9(3));
	printf("add20 %lld\n", (long long)add20(500000, 700000));		/* wraps mod 2^20 */
	printf("add33 %llu\n",
				 (unsigned long long)add33(5000000000u, 4000000000u));	/* wraps mod 2^33 */
	printf("neg64 %lld\n", (long long)neg64(-123456789012LL));
	printf("twice9 %u\n", (unsigned)twice9(500));				/* (500+500) mod 512 = 488 */
	return 0;
}

#elif defined test_wb

#include <stdio.h>
int main(void) {
	/* C23 wb/uwb literal suffix: type is _BitInt(N) with the minimal width. */
	printf("sz %zu %zu %zu %zu\n", sizeof(0wb), sizeof(64wb), sizeof(127wb),
				 sizeof(255uwb));
	printf("add %d\n", (int)(100wb + 27wb));		/* _BitInt(8), 127 fits */
	printf("wrap %d\n", (int)(5wb * 5wb));			/* _BitInt(4): 25 mod 16 -> -7 */
	printf("uwrap %u\n", (unsigned)(200uwb + 100uwb));	/* _BitInt(u9): 300 */
	printf("umod %u\n", (unsigned)(200uwb + 200uwb));	/* u8: 400 mod 256 = 144 */
	int a = (int)(63wb + 1wb);					/* _BitInt(8), 64 */
	printf("mix %d\n", a);
	unsigned long long w = (unsigned long long)(1000000uwb * 1000000uwb);
	printf("big %llu\n", w);					/* u40: 1e12 fits */
	return 0;
}

#endif
