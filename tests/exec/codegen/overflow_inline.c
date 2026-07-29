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
