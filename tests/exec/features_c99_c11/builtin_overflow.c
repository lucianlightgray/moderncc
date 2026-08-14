extern int printf(const char *, ...);

static int fails;
#define CK(cond)                          \
	do {                                    \
		if (!(cond)) {                        \
			printf("FAIL line %d\n", __LINE__); \
			fails++;                            \
		}                                     \
	} while (0)

int main(void) {
	int r;
	long long rl;
	unsigned ru;
	unsigned long long rull;
	short rs;
	unsigned char rc;

	CK(__builtin_add_overflow(2, 3, &r) == 0 && r == 5);
	CK(__builtin_sub_overflow(10, 3, &r) == 0 && r == 7);
	CK(__builtin_mul_overflow(1000, 1000, &r) == 0 && r == 1000000);

	CK(__builtin_add_overflow(2000000000, 2000000000, &r) == 1);
	CK(__builtin_mul_overflow(100000, 100000, &r) == 1);
	CK(__builtin_sub_overflow(-2000000000, 2000000000, &r) == 1);

	CK(__builtin_mul_overflow(-2147483647 - 1, -1, &r) == 1);

	CK(__builtin_add_overflow(100LL, 23LL, &rl) == 0 && rl == 123);
	CK(__builtin_add_overflow(9223372036854775807LL, 1LL, &rl) == 1);
	CK(__builtin_mul_overflow(4000000000LL, 4000000000LL, &rl) == 1);
	CK(__builtin_mul_overflow(-9223372036854775807LL - 1, -1LL, &rl) == 1);
	CK(__builtin_mul_overflow(3LL, 7LL, &rl) == 0 && rl == 21);

	CK(__builtin_add_overflow(0xFFFFFFFFu, 1u, &ru) == 1);
	CK(__builtin_mul_overflow(0x10000u, 0x10000u, &ru) == 1);
	CK(__builtin_add_overflow(0xFFFFFFFFFFFFFFFFull, 1ull, &rull) == 1);
	CK(__builtin_mul_overflow(10ull, 20ull, &rull) == 0 && rull == 200);

	CK(__builtin_add_overflow(30000, 30000, &rs) == 1);
	CK(__builtin_add_overflow(100, 200, &rc) == 1);
	CK(__builtin_sub_overflow(5, 10, &rc) == 1);
	CK(__builtin_add_overflow(40, 50, &rc) == 0 && rc == 90);

	/* The carry-chain family. Each returns the sum and writes the carry OUT
	 * through the last argument, with a carry IN as the third operand, so the
	 * result is two dependent adds and the carry is either overflowing. Values
	 * checked against clang, which is where these builtins come from. */
	{
		unsigned char cb = 0;
		unsigned short cs = 0;
		unsigned ci = 0;
		unsigned long cl = 0;
		unsigned long long cll = 0;
		CK(__builtin_addcb(200, 100, 0, &cb) == 44 && cb == 1);
		CK(__builtin_addcs(60000, 10000, 0, &cs) == 4464 && cs == 1);
		CK(__builtin_addc(0xFFFFFFFFu, 1u, 0u, &ci) == 0 && ci == 1);
		CK(__builtin_addc(1u, 2u, 0u, &ci) == 3 && ci == 0);
		CK(__builtin_subc(3u, 5u, 0u, &ci) == 0xFFFFFFFEu && ci == 1);
		CK(__builtin_addcl(~0ul, 1ul, 0ul, &cl) == 0 && cl == 1);
		CK(__builtin_subcll(0ull, 1ull, 1ull, &cll) == 0xFFFFFFFFFFFFFFFEull && cll == 1);
		/* The carry IN must reach the result, not just the carry out. */
		CK(__builtin_addc(1u, 1u, 1u, &ci) == 3 && ci == 0);
		CK(__builtin_addc(0xFFFFFFFFu, 0u, 1u, &ci) == 0 && ci == 1);
	}

#if defined __x86_64__ || defined __i386__
	/* Runtime CPU feature detection. The values are a property of the machine,
	 * so nothing here asserts a specific feature is present -- what is asserted
	 * is the internal consistency that a wrong CPUID decode would break:
	 * baselines that the target guarantees, implication between a feature and
	 * the one it extends, and that an unknown name is 0 rather than garbage. */
	{
		__builtin_cpu_init();
#if defined __x86_64__
		/* x86-64 mandates these; a CPU without them cannot run this binary. */
		CK(__builtin_cpu_supports("sse") != 0);
		CK(__builtin_cpu_supports("sse2") != 0);
		CK(__builtin_cpu_supports("cmov") != 0);
#endif
		CK(__builtin_cpu_supports("__mcc_no_such_feature") == 0);
		CK(__builtin_cpu_supports("") == 0);
		/* Each of these extends the one before it, so supporting the later
		 * without the earlier is a decode error rather than a CPU. */
		if (__builtin_cpu_supports("sse4.2"))
			CK(__builtin_cpu_supports("sse4.1") != 0);
		if (__builtin_cpu_supports("sse4.1"))
			CK(__builtin_cpu_supports("ssse3") != 0);
		if (__builtin_cpu_supports("ssse3"))
			CK(__builtin_cpu_supports("sse3") != 0);
		if (__builtin_cpu_supports("avx2"))
			CK(__builtin_cpu_supports("avx") != 0);
		if (__builtin_cpu_supports("avx512f"))
			CK(__builtin_cpu_supports("avx2") != 0);
		/* Repeated calls must agree: the feature vector is cached after the
		 * first CPUID and a stale-cache bug would show up here. */
		CK(__builtin_cpu_supports("sse2") == __builtin_cpu_supports("sse2"));
	}
#endif

	printf(fails ? "FAIL\n" : "OK\n");
	return fails ? 1 : 0;
}
