extern int printf(const char *, ...);
static unsigned long long seed = 88172645463325252ull;
static unsigned long long xs(void){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
static int refmul_i(int a, int b, int *r) { long long w = (long long)a * b; *r = (int)w; return w != (long long)(int)w; }
static int refmul_u(unsigned a, unsigned b, unsigned *r) { unsigned long long w = (unsigned long long)a * b; *r = (unsigned)w; return w > 0xffffffffull; }
static int refadd_i(int a, int b, int *r) { long long w = (long long)a + b; *r = (int)w; return w != (long long)(int)w; }
static int refsub_i(int a, int b, int *r) { long long w = (long long)a - b; *r = (int)w; return w != (long long)(int)w; }
static int refadd_u(unsigned a, unsigned b, unsigned *r) { unsigned long long w = (unsigned long long)a + b; *r = (unsigned)w; return w > 0xffffffffull; }
static int refsub_u(unsigned a, unsigned b, unsigned *r) { *r = a - b; return a < b; }
int main(void) {
	int i;
	{
		long long l1, l2;
		unsigned long long ul1, ul2;

		if (!__builtin_add_overflow(9223372036854775807LL, 1LL, &l1) || l1 != -9223372036854775807LL - 1)
			return 5;
		if (__builtin_add_overflow(1LL, 2LL, &l2) || l2 != 3)
			return 6;
		if (!__builtin_add_overflow(18446744073709551615ULL, 1ULL, &ul1) || ul1 != 0)
			return 7;
		if (__builtin_sub_overflow(5ULL, 2ULL, &ul2) || ul2 != 3)
			return 8;
		if (!__builtin_mul_overflow(4611686018427387904LL, 4LL, &l1))
			return 13;
		if (__builtin_mul_overflow(3LL, 4LL, &l2) || l2 != 12)
			return 14;
		if (!__builtin_mul_overflow(9223372036854775807ULL, 3ULL, &ul1))
			return 15;
		if (__builtin_mul_overflow(6ULL, 7ULL, &ul2) || ul2 != 42)
			return 16;
	}
	{
		signed char sc1, sc2, a8, b8;
		unsigned char uc1, uc2, ua8, ub8;
		short sh1, sh2, a16, b16;
		unsigned short us1, us2, ua16, ub16;
		int j, k;

		for (j = -128; j <= 127; j += 3) {
			for (k = -128; k <= 127; k += 7) {
				int w;

				a8 = (signed char)j;
				b8 = (signed char)k;
				w = (int)a8 + (int)b8;
				sc2 = (signed char)w;
				if (__builtin_add_overflow(a8, b8, &sc1) != (w != (int)sc2) || sc1 != sc2)
					return 17;
				w = (int)a8 - (int)b8;
				sc2 = (signed char)w;
				if (__builtin_sub_overflow(a8, b8, &sc1) != (w != (int)sc2) || sc1 != sc2)
					return 18;
				ua8 = (unsigned char)j;
				ub8 = (unsigned char)k;
				uc2 = (unsigned char)(ua8 + ub8);
				if (__builtin_add_overflow(ua8, ub8, &uc1) != ((unsigned)ua8 + ub8 > 255u) ||
						uc1 != uc2)
					return 19;
				a16 = (short)(j * 200);
				b16 = (short)(k * 200);
				w = (int)a16 + (int)b16;
				sh2 = (short)w;
				if (__builtin_add_overflow(a16, b16, &sh1) != (w != (int)sh2) || sh1 != sh2)
					return 20;
				ua16 = (unsigned short)(j * 300);
				ub16 = (unsigned short)(k * 300);
				us2 = (unsigned short)(ua16 + ub16);
				if (__builtin_add_overflow(ua16, ub16, &us1) !=
								((unsigned)ua16 + ub16 > 65535u) ||
						us1 != us2)
					return 21;
			}
		}
	}
	for (i = 0; i < 3000; i++) {
		unsigned long long v = xs();
		int a = (int)v, b = (int)(v >> 32);
		unsigned ua = (unsigned)v, ub = (unsigned)(v >> 32);
		int r1, r2; unsigned u1, u2;
		if (__builtin_add_overflow(a, b, &r1) != refadd_i(a, b, &r2) || r1 != r2) { printf("addi %d %d\n", a, b); return 1; }
		if (__builtin_sub_overflow(a, b, &r1) != refsub_i(a, b, &r2) || r1 != r2) { printf("subi %d %d\n", a, b); return 2; }
		if (__builtin_add_overflow(ua, ub, &u1) != refadd_u(ua, ub, &u2) || u1 != u2) { printf("addu\n"); return 3; }
		if (__builtin_sub_overflow(ua, ub, &u1) != refsub_u(ua, ub, &u2) || u1 != u2) { printf("subu\n"); return 4; }
		if (__builtin_mul_overflow(a, b, &r1) != refmul_i(a, b, &r2) || r1 != r2) { printf("muli %d %d\n", a, b); return 9; }
		if (__builtin_mul_overflow(ua, ub, &u1) != refmul_u(ua, ub, &u2) || u1 != u2) { printf("mulu\n"); return 10; }
		a >>= 20; b >>= 18; ua >>= 20; ub >>= 18;
		if (__builtin_mul_overflow(a, b, &r1) != refmul_i(a, b, &r2) || r1 != r2) { printf("muli2\n"); return 11; }
		if (__builtin_mul_overflow(ua, ub, &u1) != refmul_u(ua, ub, &u2) || u1 != u2) { printf("mulu2\n"); return 12; }
	}
	printf("OK\n");
	return 0;
}
