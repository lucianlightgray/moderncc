#if defined test_layout

#include <stdio.h>
#include <string.h>

struct S {
	char c;
	__int256 v;
};

int main(void) {
	unsigned char b[32];
	__int256 v = 0;
	int i;

	printf("sizeof %d alignof %d\n", (int)sizeof(__int256),
				 (int)__alignof__(__int256));
	printf("sizeof unsigned %d\n", (int)sizeof(unsigned __int256));
	printf("struct %d %d\n", (int)sizeof(struct S),
				 (int)__builtin_offsetof(struct S, v));
	printf("macro %d\n", __SIZEOF_INT256__);
	v = (__int256)0x0102030405060708ull;
	memcpy(b, &v, 32);
	for (i = 0; i < 32; i++)
		printf("%02x", b[i]);
	printf("\n");
	return 0;
}

#elif defined test_arith

#include <stdio.h>
#include <string.h>

static void pr(const char *t, const void *p) {
	unsigned long long w[4];
	memcpy(w, p, 32);
	printf("%s %016llx%016llx%016llx%016llx\n", t, w[3], w[2], w[1], w[0]);
}

int main(void) {
	__int256 a = 1, b, c;
	unsigned __int256 u;

	a = a << 255;
	pr("min", &a);
	b = a - 1;
	pr("max", &b);
	pr("min/-1", (c = a / -1, &c));
	pr("min%-1", (c = a % -1, &c));
	pr("min*-1", (c = a * -1, &c));
	u = ~(unsigned __int256)0;
	pr("umax", &u);
	pr("umax/3", (u = u / 3, &u));
	c = -a;
	pr("-min", &c);
	c = a;
	c++;
	pr("min++", &c);
	c--;
	c -= 1;
	pr("min-1", &c);
	printf("cmp %d %d %d %d\n", a < b, (unsigned __int256)a < (unsigned __int256)b,
				 a == a, b > a);
	printf("logic %d %d %d\n", !a, a && b, a || b);
	return 0;
}

#elif defined test_const

#include <stdio.h>
#include <string.h>

static const __int256 k0 = ((__int256)1 << 200) + 3;
static const __int256 k1 = ((__int256)-1) / 7;
static const __int256 k2 = ((__int256)1 << 255) >> 255;
static const unsigned __int256 k3 = ((unsigned __int256)1 << 255) >> 255;
static const __int256 k4 = (__int256)0x0123456789abcdefull * 0xfedcba987ull;
static const __int256 k5 = ~(__int256)0 ^ (__int256)0xffull;
static const int k6 = ((__int256)1 << 200) > (__int256)0;
static const int k7 = (int)(((__int256)-1) >> 3);

static void pr(const char *t, const void *p) {
	unsigned long long w[4];
	memcpy(w, p, 32);
	printf("%s %016llx%016llx%016llx%016llx\n", t, w[3], w[2], w[1], w[0]);
}

int main(void) {
	pr("k0", &k0);
	pr("k1", &k1);
	pr("k2", &k2);
	pr("k3", &k3);
	pr("k4", &k4);
	pr("k5", &k5);
	printf("k6 %d k7 %d\n", k6, k7);
	return 0;
}

#elif defined test_abi

#include <stdio.h>
#include <string.h>

static void pr(const char *t, const void *p) {
	unsigned long long w[4];
	memcpy(w, p, 32);
	printf("%s %016llx%016llx%016llx%016llx\n", t, w[3], w[2], w[1], w[0]);
}

static __int256 add3(__int256 a, __int256 b, __int256 c) { return a + b + c; }

static __int256 mixed(int a, double d, __int256 x, const char *s, __int256 y,
											int z) {
	return x * y + a + z + (int)d + (int)s[0];
}

static unsigned __int256 vsum(int n, ...) {
	unsigned __int256 t = 0;
	__builtin_va_list ap;
	__builtin_va_start(ap, n);
	while (n-- > 0)
		t = t + __builtin_va_arg(ap, unsigned __int256);
	__builtin_va_end(ap);
	return t;
}

int main(void) {
	__int256 r;
	unsigned __int256 u;
	__int256 arr[4];
	int i;

	r = add3((__int256)1 << 128, (__int256)1 << 64, (__int256)7);
	pr("add3", &r);
	r = mixed(1, 2.0, (__int256)1000, "A", (__int256)1000, 3);
	pr("mixed", &r);
	u = vsum(3, (unsigned __int256)1, (unsigned __int256)2,
					 (unsigned __int256)0x10000);
	pr("vsum", &u);
	for (i = 0; i < 4; i++)
		arr[i] = (__int256)1 << (i * 64);
	pr("arr3", &arr[3]);
	return 0;
}

#elif defined test_convert

#include <stdio.h>
#include <string.h>

static void pr(const char *t, const void *p) {
	unsigned long long w[4];
	memcpy(w, p, 32);
	printf("%s %016llx%016llx%016llx%016llx\n", t, w[3], w[2], w[1], w[0]);
}

int main(void) {
	__int256 r;
	signed char sc = -3;
	unsigned char uc = 250;
	short s = -300;
	unsigned short us = 60000;
	int n = -70000;
	unsigned int un = 4000000000u;
	long long ll = -1234567890123456789ll;
	unsigned long long ull = 18000000000000000000ull;

	r = (__int256)sc;
	pr("sc", &r);
	r = (__int256)uc;
	pr("uc", &r);
	r = (__int256)s;
	pr("s", &r);
	r = (__int256)us;
	pr("us", &r);
	r = (__int256)n;
	pr("i", &r);
	r = (__int256)un;
	pr("ui", &r);
	r = (__int256)ll;
	pr("ll", &r);
	r = (__int256)ull;
	pr("ull", &r);
	r = (__int256)(_Bool)1;
	pr("b", &r);

	r = ((__int256)-1) << 100;
	printf("narrow %d %d %d %lld %llu\n", (int)(signed char)r, (int)(short)r,
				 (int)r, (long long)r, (unsigned long long)r);
	printf("bool %d %d\n", (int)(_Bool)r, (int)(_Bool)(__int256)0);
	r = (__int256)5;
	printf("mix %lld\n", (long long)(r + 3));
	printf("rank %lld\n", (long long)(r + 0x7fffffffffffffffll));
	return 0;
}

#elif defined test_shift

#include <stdio.h>
#include <string.h>

static void pr(const char *t, const void *p) {
	unsigned long long w[4];
	memcpy(w, p, 32);
	printf("%s %016llx%016llx%016llx%016llx\n", t, w[3], w[2], w[1], w[0]);
}

int main(void) {
	volatile int z = 0, one = 1, big = 255, over = 256, neg = -1;
	__int256 a = (__int256)-1, r;
	unsigned __int256 u = ~(unsigned __int256)0, ur;

	r = a << z;
	pr("shl0", &r);
	r = a << one;
	pr("shl1", &r);
	r = a << big;
	pr("shl255", &r);
	r = a << over;
	pr("shl256", &r);
	r = a << neg;
	pr("shlneg", &r);
	r = a >> big;
	pr("sar255", &r);
	r = a >> over;
	pr("sar256", &r);
	r = a >> neg;
	pr("sarneg", &r);
	ur = u >> big;
	pr("shr255", &ur);
	ur = u >> over;
	pr("shr256", &ur);
	ur = u >> neg;
	pr("shrneg", &ur);
	return 0;
}

#elif defined test_replay_cmp

#include <stdio.h>

static int tobool(__int256 x) { return (_Bool)x; }

static int lt1(__int256 a, __int256 b) { return (a < b) + 1; }

static int chain(__int256 a, __int256 b) { return (a == b) + (a < b) + 1; }

int main(void) {
	__int256 z = 0, o = 1, big = (__int256)1 << 200;

	printf("bool %d %d %d\n", tobool(z), tobool(o), tobool(big));
	printf("lt1 %d %d %d\n", lt1(z, o), lt1(o, z), lt1(big, big));
	printf("chain %d %d\n", chain(z, o), chain(o, o));
	return 0;
}

#elif defined test_boolctx

#include <stdio.h>

int main(void) {
	__int256 z = 0, nz = 7, big = (__int256)1 << 200;
	printf("if %s %s %s\n", z ? "T" : "F", nz ? "T" : "F", big ? "T" : "F");
	printf("tern %d %d\n", z ? 11 : 22, nz ? 11 : 22);
	printf("logic %d %d\n", nz && z, z || nz);
	int c = 0;
	__int256 u = 0;
	while (u) {
		c++;
		if (c > 3)
			break;
	}
	for (__int256 i = 3; i; i -= 1)
		c++;
	printf("loops %d\n", c);
	return 0;
}

#endif
