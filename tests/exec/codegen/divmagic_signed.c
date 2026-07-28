/* Signed constant division/remainder strength-reduction (divmagic) must keep
 * its result SIGNED. Regression for a bug in ast_divmagic_try_s64 where the
 * quotient's logical-shift sign bit was left unsigned (U64), so `q2 + signbit`
 * took the usual-arithmetic-conversion path to unsigned and poisoned the type
 * of `x / C` and `x % C`. The values stayed correct, but a `(x % C) < 0` test
 * on the result then emitted UNSIGNED branches and never negated a negative
 * remainder -- e.g. abs(LLONG_MIN % 251) came out 96 instead of 160.
 *
 * The failing shape needs (a) divmagic to actually fire, which requires the
 * dividend to be a PURE operand (a `volatile` load is not pure, so divmagic
 * bails and the bug hides) whose value the optimizer cannot fold to a constant,
 * and (b) the divide/modulo to flow DIRECTLY into the sign test with no
 * intervening typed store. A noinline helper taking the dividend as a parameter
 * gives exactly that: pure, value unknown, literal divisor. The reference is a
 * separate helper using a `volatile` divisor -- a plain hardware divide with no
 * divmagic -- for the ground truth. */

extern int printf(const char *, ...);

/* Under test: literal divisor, dividend is an unknown pure parameter, and the
 * modulo/divide feeds the sign test directly (as in the original d10 repro). */
#define GEN_LL(sfx, C)                                                       \
	__attribute__((noinline)) static long long absmod_ll_##sfx(long long x) {\
		return (x % (C)) < 0 ? -(x % (C)) : (x % (C));                       \
	}                                                                        \
	__attribute__((noinline)) static int negdiv_ll_##sfx(long long x) {      \
		return (x / (C)) < 0;                                                \
	}
#define GEN_I(sfx, C)                                                        \
	__attribute__((noinline)) static int absmod_i_##sfx(int x) {             \
		return (x % (C)) < 0 ? -(x % (C)) : (x % (C));                       \
	}                                                                        \
	__attribute__((noinline)) static int negdiv_i_##sfx(int x) {             \
		return (x / (C)) < 0;                                                \
	}

GEN_LL(a, 3)
GEN_LL(b, 7)
GEN_LL(c, 251)
GEN_LL(d, 1000003)
GEN_LL(e, -13)
GEN_LL(f, -251)
GEN_I(a, 3)
GEN_I(b, 7)
GEN_I(c, 251)
GEN_I(d, 65537)
GEN_I(e, -13)
GEN_I(f, -251)

/* Ground truth: volatile divisor -> hardware divide, never divmagic. */
static long long ref_absmod_ll(long long a, long long d) {
	volatile long long dd = d;
	long long m = a % dd;
	return m < 0 ? -m : m;
}
static int ref_negdiv_ll(long long a, long long d) {
	volatile long long dd = d;
	return (a / dd) < 0;
}
static int ref_absmod_i(int a, int d) {
	volatile int dd = d;
	int m = a % dd;
	return m < 0 ? -m : m;
}
static int ref_negdiv_i(int a, int d) {
	volatile int dd = d;
	return (a / dd) < 0;
}

static int fails;

static void ck_ll(long long v, long long got_mod, long long ref_mod, int got_div,
				  int ref_div, long long c) {
	if (got_mod != ref_mod) {
		printf("FAIL ll%%%lld v=%lld got=%lld want=%lld\n", c, v, got_mod, ref_mod);
		fails++;
	}
	if (got_div != ref_div) {
		printf("FAIL ll/%lld v=%lld\n", c, v);
		fails++;
	}
}
static void ck_i(int v, int got_mod, int ref_mod, int got_div, int ref_div, int c) {
	if (got_mod != ref_mod) {
		printf("FAIL i%%%d v=%d got=%d want=%d\n", c, v, got_mod, ref_mod);
		fails++;
	}
	if (got_div != ref_div) {
		printf("FAIL i/%d v=%d\n", c, v);
		fails++;
	}
}

#define LLMIN (-9223372036854775807LL - 1)
#define LLMAX 9223372036854775807LL
#define IMIN (-2147483647 - 1)
#define IMAX 2147483647

int main(void) {
	static const long long v_ll[] = {
		LLMIN, LLMAX, -1, 0, 1, -251, 251,
		-1000000000007LL, 1000000000007LL, -160, 160};
	static const int v_i[] = {IMIN, IMAX, -1, 0, 1, -251, 251, -160, 160};

	for (unsigned i = 0; i < sizeof(v_ll) / sizeof(v_ll[0]); i++) {
		long long v = v_ll[i];
		ck_ll(v, absmod_ll_a(v), ref_absmod_ll(v, 3), negdiv_ll_a(v), ref_negdiv_ll(v, 3), 3);
		ck_ll(v, absmod_ll_b(v), ref_absmod_ll(v, 7), negdiv_ll_b(v), ref_negdiv_ll(v, 7), 7);
		ck_ll(v, absmod_ll_c(v), ref_absmod_ll(v, 251), negdiv_ll_c(v), ref_negdiv_ll(v, 251), 251);
		ck_ll(v, absmod_ll_d(v), ref_absmod_ll(v, 1000003), negdiv_ll_d(v), ref_negdiv_ll(v, 1000003), 1000003);
		ck_ll(v, absmod_ll_e(v), ref_absmod_ll(v, -13), negdiv_ll_e(v), ref_negdiv_ll(v, -13), -13);
		ck_ll(v, absmod_ll_f(v), ref_absmod_ll(v, -251), negdiv_ll_f(v), ref_negdiv_ll(v, -251), -251);
	}
	for (unsigned i = 0; i < sizeof(v_i) / sizeof(v_i[0]); i++) {
		int v = v_i[i];
		ck_i(v, absmod_i_a(v), ref_absmod_i(v, 3), negdiv_i_a(v), ref_negdiv_i(v, 3), 3);
		ck_i(v, absmod_i_b(v), ref_absmod_i(v, 7), negdiv_i_b(v), ref_negdiv_i(v, 7), 7);
		ck_i(v, absmod_i_c(v), ref_absmod_i(v, 251), negdiv_i_c(v), ref_negdiv_i(v, 251), 251);
		ck_i(v, absmod_i_d(v), ref_absmod_i(v, 65537), negdiv_i_d(v), ref_negdiv_i(v, 65537), 65537);
		ck_i(v, absmod_i_e(v), ref_absmod_i(v, -13), negdiv_i_e(v), ref_negdiv_i(v, -13), -13);
		ck_i(v, absmod_i_f(v), ref_absmod_i(v, -251), negdiv_i_f(v), ref_negdiv_i(v, -251), -251);
	}

	printf(fails ? "FAIL %d\n" : "OK\n", fails);
	return fails != 0;
}
