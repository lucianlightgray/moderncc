/* C23 _BitInt(N) slice 3: 128 < N <= 256 (a 4-limb 32-byte struct riding the
 * __int256 kernel).  Differential vs gcc for values; props lock mcc's uniform
 * ABI.  Sectioned like bitint.c/bitint128.c (one -Dtest_* per compile). */

#if defined test_props

#include <stdio.h>
int main(void) {
	printf("maxwidth %d\n", __BITINT_MAXWIDTH__);
	printf("size 129=%zu 192=%zu 193=%zu 200=%zu 255=%zu 256=%zu\n",
				 sizeof(_BitInt(129)), sizeof(_BitInt(192)), sizeof(_BitInt(193)),
				 sizeof(_BitInt(200)), sizeof(_BitInt(255)), sizeof(_BitInt(256)));
	printf("align 129=%zu 200=%zu 256=%zu\n", _Alignof(_BitInt(129)),
				 _Alignof(_BitInt(200)), _Alignof(_BitInt(256)));
	return 0;
}

#elif defined test_arith

#include <stdio.h>
typedef _BitInt(200) i200;
typedef unsigned _BitInt(256) u256;
static void pr(const char *t, u256 x) {
	printf("%s %016llx %016llx %016llx %016llx\n", t,
				 (unsigned long long)(x >> 192), (unsigned long long)(x >> 128),
				 (unsigned long long)(x >> 64), (unsigned long long)x);
}
int main(void) {
	u256 a = (u256)1 << 130;
	u256 b = (u256)0xFF;
	pr("add", a + b);
	pr("sub", a - b);
	u256 m = ((u256)0xDEADBEEF << 96) | (u256)0x12345678;
	pr("mul", m * (u256)3);
	i200 c = -(i200)1000000000000;
	printf("smul %lld\n", (long long)(c * (i200)7));
	printf("sdiv %lld smod %lld\n", (long long)(c / (i200)7),
				 (long long)(c % (i200)7));
	u256 big = ((u256)1 << 200) - (u256)1;
	pr("mask200", big);
	return 0;
}

#elif defined test_bitwise

#include <stdio.h>
typedef unsigned _BitInt(256) u256;
static void pr(const char *t, u256 x) {
	printf("%s %016llx %016llx %016llx %016llx\n", t,
				 (unsigned long long)(x >> 192), (unsigned long long)(x >> 128),
				 (unsigned long long)(x >> 64), (unsigned long long)x);
}
int main(void) {
	u256 e = (u256)0xF0F0F0F0F0F0F0F0ull;
	e |= ((u256)0x0F0F0F0F0F0F0F0Full << 192);
	e ^= (u256)-1;
	pr("bits", e);
	u256 g = ((u256)0xAAAAAAAAAAAAAAAAull << 192) | (u256)0xAAAAAAAAAAAAAAAAull;
	u256 h = ((u256)0xF0F0F0F0F0F0F0F0ull << 192) | (u256)0xF0F0F0F0F0F0F0F0ull;
	pr("and", g & h);
	pr("or", g | h);
	_BitInt(193) n = (_BitInt(193))5;
	printf("not %lld\n", (long long)(~n));
	return 0;
}

#elif defined test_shift

#include <stdio.h>
typedef unsigned _BitInt(256) u256;
typedef _BitInt(200) i200;
static void pr(const char *t, u256 x) {
	printf("%s %016llx %016llx %016llx %016llx\n", t,
				 (unsigned long long)(x >> 192), (unsigned long long)(x >> 128),
				 (unsigned long long)(x >> 64), (unsigned long long)x);
}
int main(void) {
	u256 a = (u256)1;
	pr("shl130", a << 130);
	pr("shl200", a << 200);
	u256 b = (u256)-1;
	pr("shr250", b >> 250);
	i200 s = -(i200)1;
	printf("sar %lld\n", (long long)(s >> 50));
	i200 t = (i200)1 << 150;
	printf("i200sign %d\n", (int)((t << 49) < 0));
	return 0;
}

#elif defined test_conv

#include <stdio.h>
typedef _BitInt(200) i200;
typedef unsigned _BitInt(200) u200;
typedef _BitInt(100) i100;
int main(void) {
	i200 a = (i200)-3;
	printf("to %lld %d %u\n", (long long)a, (int)a, (unsigned)a);
	u200 u = (u200)1234567;
	printf("toint %llu\n", (unsigned long long)u);
	i100 small = (i100)-42;
	i200 wide = (i200)small;
	printf("widen %lld\n", (long long)wide);
	i100 back = (i100)wide;
	printf("narrow %lld\n", (long long)back);
	i200 z = (i200)0, nz = (i200)7;
	printf("bool %d %d\n", (int)(_Bool)z, (int)(_Bool)nz);
	unsigned long long lo = (unsigned long long)(((u200)0xABCDEF << 128) | (u200)9);
	printf("lolimb %llu\n", lo);
	return 0;
}

#elif defined test_call

#include <stdio.h>
typedef _BitInt(200) i200;
static i200 add(i200 a, i200 b) { return a + b; }
static i200 shift(i200 a, int n) { return a << n; }
int main(void) {
	i200 a = (i200)3000000000000;
	i200 b = (i200)5;
	printf("add %lld\n", (long long)add(a, b));
	i200 r = shift((i200)7, 160);
	printf("shifted %d\n", (int)((r >> 160) & (i200)0xFF));
	i200 acc = (i200)0;
	for (int i = 0; i < 10; i++)
		acc = add(acc, (i200)i);
	printf("loop %lld\n", (long long)acc);
	return 0;
}

#elif defined test_aggregate

#include <stdio.h>
typedef _BitInt(200) i200;
struct S { int tag; i200 v; char c; };
static struct S gs = {7, (i200)-12345, 'Z'};
static i200 garr[3] = {(i200)10, (i200)20, (i200)30};
int main(void) {
	struct S s = {1, (i200)999999, 'A'};
	printf("struct %d %lld %c sz=%zu\n", s.tag, (long long)s.v, s.c,
				 sizeof(struct S));
	printf("glob %d %lld %c\n", gs.tag, (long long)gs.v, gs.c);
	i200 arr[4];
	for (int i = 0; i < 4; i++)
		arr[i] = (i200)i * (i200)100;
	printf("arr %lld\n", (long long)(arr[0] + arr[1] + arr[2] + arr[3]));
	printf("garr %lld\n", (long long)(garr[0] + garr[1] + garr[2]));
	return 0;
}

#elif defined test_cmp

#include <stdio.h>
typedef _BitInt(200) i200;
typedef unsigned _BitInt(200) u200;
int main(void) {
	i200 a = (i200)1 << 150, b = -(i200)1;
	printf("scmp %d%d%d%d%d%d\n", a > b, a < b, a == b, a != b, a >= b, a <= b);
	u200 c = (u200)1 << 199, d = (u200)1;
	printf("ucmp %d%d%d%d\n", c > d, c < d, c == d, c != d);
	printf("eqz %d %d\n", (i200)0 == (i200)0, (i200)5 == (i200)0);
	return 0;
}

#elif defined test_boolctx

#include <stdio.h>
typedef _BitInt(200) i200;
int main(void) {
	i200 z = (i200)0, hi = (i200)1 << 199;
	printf("if %s %s\n", z ? "T" : "F", hi ? "T" : "F");
	printf("tern %d\n", hi ? 22 : 11);
	printf("logic %d %d %d\n", z && hi, z || hi, !z);
	int count = 0;
	i200 n = (i200)1 << 130;
	while (n) { n >>= 1; count++; }
	printf("while %d\n", count);
	return 0;
}

#elif defined test_mixwidth

#include <stdio.h>
typedef _BitInt(128) i128;
typedef _BitInt(200) i200;
int main(void) {
	i128 a = (i128)1000;
	i200 b = (i200)2000000;
	i200 s = b + a;
	printf("add %lld\n", (long long)s);
	printf("mul %lld\n", (long long)(b * a));
	i200 wide = (i200)a << 100;
	printf("promoted %d\n", (int)((wide >> 100) & (i200)0xFFFF));
	return 0;
}

#elif defined test_wb

#include <stdio.h>
int main(void) {
	printf("sz %zu %zu\n", sizeof(0x1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFwb),
				 sizeof(0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFuwb));
	unsigned _BitInt(200) u = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFuwb;
	printf("ulo %llu\n", (unsigned long long)u);
	_BitInt(200) v = 100000000000000000000wb;
	printf("dec %d\n", (int)(v / 1000000000000000000wb));
	return 0;
}

#elif defined test_float

#include <stdio.h>
typedef _BitInt(200) i200;
typedef unsigned _BitInt(200) u200;
int main(void) {
	i200 a = (i200)1 << 150;
	printf("i2d %.17g\n", (double)a);
	i200 neg = -(i200)123456789012345;
	printf("neg %.17g\n", (double)neg);
	double d = 1.0e30;
	i200 fromd = (i200)d;
	printf("d2i %lld\n", (long long)(fromd >> 60));
	float f = 65536.0f;
	u200 fromf = (u200)f;
	printf("f2i %llu\n", (unsigned long long)fromf);
	return 0;
}

#endif
