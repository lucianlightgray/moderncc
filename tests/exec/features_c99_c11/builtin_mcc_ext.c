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
 *   __builtin_setjmp        gcc's has restricted semantics needing compiler
 *                           support; the same program hangs or fails to build
 *                           under gcc. mcc implements the stronger C setjmp
 *                           contract, so the oracle is _setjmp, not gcc's
 *                           builtin -- and the equivalence is asserted below
 *                           by running both in the same program.
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

#if defined __MCC__ && defined __x86_64__
extern int _setjmp(void *);
extern void _longjmp(void *, int);
static int sj_reached, sj_sink;
static void sj_unwind(int n, void **buf) {
	if (n == 0) {
		sj_reached = 1;
		__builtin_longjmp(buf, 1);
	}
	sj_unwind(n - 1, buf);
}
static int sj_work(int x) { sj_sink += x; return x * 3; }

/* Six long-lived locals the optimizer wants in callee-saved registers, which
 * is precisely what the save/restore exists for. Run once through
 * __builtin_setjmp and once through the platform's _setjmp: the two must agree
 * exactly, which is the real assertion -- gcc's builtin is not the oracle
 * here, C setjmp semantics is. */
static long sj_regs(int use_builtin, void **bb, void *rawbuf) {
	int p = sj_work(1), q = sj_work(2), r = sj_work(3);
	int s = sj_work(4), t = sj_work(5), u = sj_work(6);
	int jr = use_builtin ? __builtin_setjmp(bb) : _setjmp(rawbuf);
	if (jr == 0) {
		if (use_builtin)
			__builtin_longjmp(bb, 7);
		else
			_longjmp(rawbuf, 7);
	}
	return (long)jr * 1000000 + p + q * 10 + r * 100 + s * 1000 + t * 10000
				 + u * 100000;
}
#endif

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

#if defined __x86_64__
	/* setjmp/longjmp: 0 on the direct call, the value on the second return,
	 * locals surviving a multi-frame unwind, longjmp(buf, 0) surfacing as 1,
	 * a reusable buffer, and agreement with _setjmp on callee-saved state. */
	{
		static void *sjb[5];
		volatile int a = 1, b = 2;
		int sr = __builtin_setjmp(sjb);
		if (sr == 0) {
			a = 10;
			b = 20;
			sj_unwind(4, sjb);
			CK(0);
		}
		CK(sr == 1);
		CK(a == 10 && b == 20);
		CK(sj_reached == 1);
	}
	{
		static void *sjb0[5];
		int q = __builtin_setjmp(sjb0);
		if (q == 0)
			__builtin_longjmp(sjb0, 0);
		CK(q == 1);          /* longjmp(buf, 0) must return 1 */
	}
	{
		static void *sjb2[5];
		int n = 0;
		if (__builtin_setjmp(sjb2) == 0) {
			n = 1;
			__builtin_longjmp(sjb2, 1);
		}
		CK(n == 1);
	}
	{
		static void *bb[5];
		static char raw[512] __attribute__((aligned(16)));
		CK(sj_regs(1, bb, raw) == sj_regs(0, bb, raw));
	}
#endif

#endif

	printf(fails ? "FAIL\n" : "OK\n");
	return fails ? 1 : 0;
}
