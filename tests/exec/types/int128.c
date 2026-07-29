#include <stdio.h>

typedef unsigned __int128 u128;
typedef __int128 s128;
typedef int TItype __attribute__ ((mode (TI)));
typedef unsigned int UTItype __attribute__ ((mode (TI)));
typedef int DItype __attribute__ ((mode (DI)));

struct DWstruct { DItype low, high; };
typedef union { struct DWstruct s; TItype ll; } DWunion;

static u128 gu = ((u128)0x1122334455667788ULL << 64) | 0x99aabbccddeeff00ULL;
static s128 gs = -1;

static void pr(const char *n, u128 v) {
	printf("%s %016llx%016llx\n", n,
				 (unsigned long long)(v >> 64), (unsigned long long)v);
}

static int sw(s128 x) {
	switch (x) {
	case 0: return 10;
	case 5000000000LL: return 11;
	case -5000000000LL: return 12;
	case 0x7fffffffffffffffLL: return 13;
	case (-0x7fffffffffffffffLL - 1): return 14;
	default: return 99;
	}
}

static int usw(u128 x) {
	switch (x) {
	case 0xffffffffffffffffULL: return 20;
	case 2: return 22;
	default: return 98;
	}
}

static s128 addv(s128 a, s128 b) { return a + b; }
static u128 mix(int a, u128 b, long long c, u128 d, int e) {
	return b + d + (u128)a + (u128)c + (u128)e;
}

int main(void) {
	u128 a = ((u128)0xfedcba9876543210ULL << 64) | 0x0123456789abcdefULL;
	u128 b = ((u128)0x0000000100000002ULL << 64) | 0xfffffffffffffffdULL;
	s128 sa = (s128)a, sb = (s128)b;
	DWunion dw;
	s128 w;

	printf("%d %d %d %d %d\n", (int)sizeof(u128), (int)_Alignof(u128),
				 (int)sizeof(TItype), (int)sizeof(UTItype), (int)sizeof(DItype));
	pr("gu", gu);
	pr("gs", (u128)gs);
	pr("add", a + b);
	pr("sub", a - b);
	pr("mul", a * b);
	pr("udiv", a / b);
	pr("umod", a % b);
	pr("sdiv", (u128)(sa / sb));
	pr("smod", (u128)(sa % sb));
	pr("and", a & b);
	pr("not", ~a);
	pr("neg", -a);
	pr("shl", a << 70);
	pr("shr", a >> 70);
	pr("sar", (u128)(sa >> 70));
	printf("cmp %d%d%d%d%d%d\n", a < b, a <= b, a > b, a >= b, sa < sb, sa >= sb);
	pr("widen", (u128)(s128)(int)-5);
	pr("widenu", (u128)(unsigned long long)-5);
	printf("narrow %d %lld\n", (int)a, (long long)a);
	pr("addv", (u128)addv(sa, sb));
	pr("mix", mix(-3, a, -5LL, b, 7));
	dw.ll = (TItype)a;
	printf("pun %016llx %016llx\n", (unsigned long long)dw.s.low,
				 (unsigned long long)dw.s.high);
	printf("f %.17g %.9g %.20Lg\n", (double)a, (float)sa, (long double)a);
	pr("fix", (u128)(s128)1.2345678901234e30);
	printf("ovf %d", __builtin_add_overflow(sa, sb, &w));
	pr("", (u128)w);
	printf("ovf2 %d\n", __builtin_mul_overflow(sa, sb, &w));
	printf("sw %d%d%d%d%d%d %d%d%d\n", sw(0), sw(5000000000LL),
				 sw(-5000000000LL), sw(0x7fffffffffffffffLL),
				 sw(-0x7fffffffffffffffLL - 1), sw((s128)1 << 100) == 99,
				 usw(0xffffffffffffffffULL), usw(2), usw(((u128)1 << 64) | 2));
	return 0;
}
