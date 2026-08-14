/* Builtins whose in-tree coverage cannot live in a three-way fixture.
 *
 * Each of these is REJECTED AT COMPILE TIME by at least one reference, so
 * putting them in builtin_overflow.c cost that cell its gcc reference and took
 * diff3/builtin_overflow from "1 agree" to "0 agree, 1 ref-cant-build" -- the
 * exact failure the diff3 runner was repaired for on the same day. The
 * `#ifdef __MCC__` below is the tree's own marker for that: diff3's runner
 * reports the file MCC-ONLY and skips it rather than losing a reference over
 * it, while the exec goldens still run it under mcc.
 *
 * Why each one cannot be shared:
 *
 *   __builtin_addc / subc   clang-only; gcc has no __builtin_addcb at all.
 *   __builtin_cpu_supports  gcc validates the feature name at COMPILE time and
 *                           errors on an unknown one, so the negative cases --
 *                           which are the interesting ones -- cannot be
 *                           written in a program gcc must also build.
 */

extern int printf(const char *, ...);

static int fails;
#define CK(cond)                            \
	do {                                      \
		if (!(cond)) {                          \
			printf("FAIL line %d\n", __LINE__);   \
			fails++;                              \
		}                                       \
	} while (0)

int main(void) {
#ifdef __MCC__
	/* The carry chain. Each returns the sum and writes the carry OUT through
	 * the last argument, with a carry IN as the third operand, so the result is
	 * two dependent operations and the carry is either overflowing. Values
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
		/* The carry IN must reach the result, not only the carry out. */
		CK(__builtin_addc(1u, 1u, 1u, &ci) == 3 && ci == 0);
		CK(__builtin_addc(0xFFFFFFFFu, 0u, 1u, &ci) == 0 && ci == 1);
	}

#if defined __x86_64__ || defined __i386__
	/* Runtime CPU feature detection. Nothing here asserts that a particular
	 * feature is present -- that is a property of the machine. What is asserted
	 * is the internal consistency a wrong CPUID decode would break. */
	{
		__builtin_cpu_init();
#if defined __x86_64__
		CK(__builtin_cpu_supports("sse") != 0);
		CK(__builtin_cpu_supports("sse2") != 0);
		CK(__builtin_cpu_supports("cmov") != 0);
#endif
		CK(__builtin_cpu_supports("__mcc_no_such_feature") == 0);
		CK(__builtin_cpu_supports("") == 0);
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
		/* Cached after the first CPUID; a stale cache would show up here. */
		CK(__builtin_cpu_supports("sse2") == __builtin_cpu_supports("sse2"));
	}
#endif

#endif

	printf(fails ? "FAIL\n" : "OK\n");
	return fails ? 1 : 0;
}
