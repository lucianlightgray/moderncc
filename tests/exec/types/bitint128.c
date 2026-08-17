/* C23 6.2.5 _BitInt(N), slice 2 (64 < N <= 128): a 16-byte 2-limb value whose
 * value is reduced to N bits, sharing the __int256 arithmetic kernel.  Verified
 * byte-for-byte against gcc-16 across width classes and operations.  _BitInt
 * literal suffixes (wb/uwb) are a separate feature; this file uses casts. */
#if defined test_props

#include <stdio.h>
int main(void) {
	printf("maxwidth %d\n", __BITINT_MAXWIDTH__);
	printf("size 65=%zu 96=%zu 100=%zu 127=%zu 128=%zu\n",
				 sizeof(_BitInt(65)), sizeof(_BitInt(96)), sizeof(_BitInt(100)),
				 sizeof(_BitInt(127)), sizeof(_BitInt(128)));
	printf("align 65=%zu 100=%zu 128=%zu\n", _Alignof(_BitInt(65)),
				 _Alignof(_BitInt(100)), _Alignof(_BitInt(128)));
	return 0;
}

#elif defined test_arith

#include <stdio.h>
typedef _BitInt(100) i100;
typedef unsigned _BitInt(100) u100;
typedef _BitInt(128) i128;
typedef unsigned _BitInt(128) u128;
int main(void) {
	u128 a = (u128)0xFFFFFFFFFFFFFFFFull;		/* 2^64-1 */
	a += (u128)5;								/* no wrap: 2^64+4 */
	printf("uadd hi=%016llx lo=%016llx\n", (unsigned long long)(a >> 64),
				 (unsigned long long)a);
	i128 s = (i128)-100;
	s *= (i128)3;
	printf("smul %lld\n", (long long)s);
	u100 b = (u100)0xFFFFFFFFFFFFFFFFull;
	b += (u100)0x1000000000ull;
	printf("u100add hi=%016llx lo=%016llx\n", (unsigned long long)(b >> 64),
				 (unsigned long long)b);
	i100 c = (i100)-500;
	c /= (i100)7;
	printf("i100div %lld\n", (long long)c);
	i100 d = (i100)-8, e = (i100)3;
	printf("divmod %lld %lld\n", (long long)(d / e), (long long)(d % e));
	i128 f = (i128)1000000000;
	f *= f;
	printf("bigmul %lld hi=%llx\n", (long long)f,
				 (unsigned long long)((u128)f >> 64));
	return 0;
}

#elif defined test_bitwise

#include <stdio.h>
typedef unsigned _BitInt(128) u128;
typedef _BitInt(100) i100;
int main(void) {
	u128 e = (u128)0xF0F0F0F0F0F0F0F0ull;
	e |= ((u128)0x0F0F0F0F0F0F0F0Full << 64);
	e ^= (u128)-1;
	printf("bits hi=%016llx lo=%016llx\n", (unsigned long long)(e >> 64),
				 (unsigned long long)e);
	u128 g = (u128)0xAAAAAAAAAAAAAAAAull, h = (u128)0xF0F0F0F0F0F0F0F0ull;
	printf("and %016llx or %016llx\n", (unsigned long long)(g & h),
				 (unsigned long long)(g | h));
	i100 n = (i100)5;
	printf("not %lld\n", (long long)(~n));
	return 0;
}

#elif defined test_shift

#include <stdio.h>
typedef _BitInt(128) i128;
typedef unsigned _BitInt(128) u128;
typedef _BitInt(65) i65;
int main(void) {
	u128 a = (u128)1;
	a <<= 100;
	printf("shl hi=%016llx lo=%016llx\n", (unsigned long long)(a >> 64),
				 (unsigned long long)a);
	i128 f = (i128)-1024;
	printf("sar %lld shr %llu\n", (long long)(f >> 3),
				 (unsigned long long)((u128)f >> 3));
	i65 b = (i65)1;
	b <<= 64;
	printf("i65sign %lld\n", (long long)(b >> 64));
	return 0;
}

#elif defined test_conv

#include <stdio.h>
typedef _BitInt(100) i100;
typedef unsigned _BitInt(100) u100;
typedef _BitInt(128) i128;
int main(void) {
	i100 a = (i100)-3;
	printf("to %d %ld %lld %u\n", (int)a, (long)a, (long long)a, (unsigned)a);
	int j = (int)(i100)1000000;
	printf("toint %d\n", j);
	u100 b = (u100)5;
	i100 c = (i100)b;								/* widen-signed from unsigned */
	printf("cross %lld\n", (long long)c);
	i100 d = (i100)-1;
	i128 e = (i128)d;								/* _BitInt -> wider _BitInt */
	printf("bi2bi %lld hi=%016llx\n", (long long)e,
				 (unsigned long long)((unsigned _BitInt(128))e >> 64));
	_Bool z = (_Bool)(i128)0, nz = (_Bool)(i128)77;
	printf("bool %d %d\n", z, nz);
	i100 m = (i100)100;
	int n = 7;
	printf("mixint %lld %lld\n", (long long)(m + n), (long long)(m * n));
	printf("cfold %lld %llu\n", (long long)(i100)(-9223372036854775807LL - 1),
				 (unsigned long long)((u100)1000000000000ULL * (u100)1000000));
	return 0;
}

#elif defined test_call

#include <stdio.h>
typedef _BitInt(128) i128;
typedef unsigned _BitInt(100) u100;
typedef _BitInt(65) i65;
static i128 addfn(i128 a, i128 b) { return a + b; }
static u100 mulfn(u100 a, u100 b) { return a * b; }
static i65 sqfn(i65 a) { return a * a; }
static i128 many(i128 a, i128 b, i128 c, i128 d, i128 e) { return a - b + c - d + e; }
int main(void) {
	printf("add %lld\n",
				 (long long)addfn((i128)1000000000000LL, (i128)2000000000000LL));
	printf("mul %llu\n",
				 (unsigned long long)mulfn((u100)123456789, (u100)987654321));
	printf("sq %lld\n", (long long)sqfn((i65)-123456));
	printf("many %lld\n",
				 (long long)many((i128)100, (i128)10, (i128)5, (i128)2, (i128)1));
	return 0;
}

#elif defined test_aggregate

#include <stdio.h>
typedef _BitInt(100) i100;
struct S { int x; i100 v; unsigned char c; };
static i100 garr[3] = {(i100)10, (i100)-20, (i100)30};
static struct S gs = {42, (i100)-9999, 7};
int main(void) {
	struct S s;
	s.x = 1;
	s.v = (i100)-12345;
	s.c = 9;
	printf("struct %d %lld %d sz=%zu\n", s.x, (long long)s.v, s.c,
				 sizeof(struct S));
	printf("glob %d %lld %d\n", gs.x, (long long)gs.v, gs.c);
	i100 arr[3] = {(i100)10, (i100)-20, (i100)30};
	i100 sum = (i100)0;
	for (int k = 0; k < 3; k++)
		sum += arr[k];
	printf("arr %lld\n", (long long)sum);
	i100 gsum = (i100)0;
	for (int k = 0; k < 3; k++)
		gsum += garr[k];
	printf("garr %lld\n", (long long)gsum);
	return 0;
}

#elif defined test_cmp

#include <stdio.h>
typedef _BitInt(100) i100;
typedef unsigned _BitInt(100) u100;
typedef unsigned _BitInt(128) u128;
int main(void) {
	i100 g = (i100)-5, h = (i100)5;
	printf("scmp %d%d%d%d%d%d\n", g < h, g <= h, g > h, g >= h, g == h, g != h);
	u100 ug = (u100)5, uh = (u100)7;
	printf("ucmp %d%d%d%d\n", ug < uh, ug > uh, ug == uh, ug != uh);
	u128 big = (u128)-1;
	printf("umax %d %d\n", big > (u128)1, big < (u128)1);
	i100 z = (i100)0, nz = (i100)3;
	/* logical-not and explicit compares reduce correctly; the implicit
	 * boolean context (if/?:) of a wide-struct value is a separate mcc gap
	 * shared with __int256, exercised elsewhere, not asserted here. */
	printf("lnot %d %d\n", !z, !nz);
	printf("eqz %d %d\n", z == (i100)0, nz == (i100)0);
	return 0;
}

#elif defined test_boolctx

#include <stdio.h>
typedef _BitInt(100) i100;
typedef unsigned _BitInt(128) u128;
int main(void) {
	i100 z = (i100)0, nz = (i100)7;
	printf("if %s %s\n", z ? "T" : "F", nz ? "T" : "F");
	printf("tern %d %d\n", z ? 11 : 22, nz ? 11 : 22);
	printf("logic %d %d %d\n", nz && z, z || nz, nz && nz);
	u128 u = (u128)0;
	int c = 0;
	while (u) {
		c++;
		if (c > 3)
			break;
	}
	printf("while %d\n", c);
	for (i100 i = (i100)3; i; i -= (i100)1)
		c++;
	printf("for %d\n", c);
	return 0;
}

#elif defined test_mixwidth

#include <stdio.h>
typedef _BitInt(70) i70;
typedef _BitInt(100) i100;
typedef unsigned _BitInt(90) u90;
typedef _BitInt(120) i120;
int main(void) {
	i70 a = (i70)1000;
	i100 b = (i100)2000000;
	printf("add %lld sub %lld\n", (long long)(a + b), (long long)(b - a));
	i70 x = (i70)-5;
	i100 y = (i100)3;
	printf("mul %lld div %lld\n", (long long)(x * y), (long long)(b / a));
	u90 u = (u90)500;
	i120 v = (i120)-7;
	printf("mixsign %lld\n", (long long)(v + u));
	printf("cmp %d %d %d\n", a < b, b < a, a == (i70)1000);
	return 0;
}

#endif
