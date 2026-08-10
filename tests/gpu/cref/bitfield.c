struct B {
	int a : 5;
	unsigned int b : 7;
	int c : 19;
	unsigned int d : 1;
	signed char e : 4;
	unsigned short f : 11;
	long long g : 40;
	unsigned long long h : 23;
	_Bool i : 1;
};

static int read_a(struct B s) { return s.a; }

static unsigned int read_b(struct B s) { return s.b; }

static int read_c(struct B s) { return s.c; }

static int read_d(struct B s) { return (int)s.d; }

static int read_e(struct B s) { return s.e; }

static int read_f(struct B s) { return s.f; }

static long long read_g(struct B s) { return s.g; }

static unsigned long long read_h(struct B s) { return s.h; }

static int read_i(struct B s) { return (int)s.i; }

static int sum_ac(struct B s) { return s.a + s.c; }

static unsigned int mix_bf(struct B s) { return s.b * s.f; }

static int cmp_ae(struct B s) { return s.a < s.e; }

static long long wide_gh(struct B s) { return s.g - (long long)s.h; }

static int neg_a(struct B s) { return -s.a; }

static int not_b(struct B s) { return (int)~s.b; }

static int lnot_c(struct B s) { return !s.c; }

static int tern_ad(struct B s) { return s.d ? s.a : s.c; }

static int shift_ca(struct B s) { return s.c >> (s.a & 15); }

static int nested(struct B s) { return (s.a + s.e) * (s.c - s.f) + (int)s.b; }

static struct B mk(int a, unsigned int b, int c, unsigned int d, int e,
									 unsigned int f, long long g, unsigned long long h, int i) {
	struct B s;
	s.a = a;
	s.b = b;
	s.c = c;
	s.d = d;
	s.e = e;
	s.f = f;
	s.g = g;
	s.h = h;
	s.i = i;
	return s;
}

int main(void) {
	struct B p = mk(-7, 100u, -100000, 1u, -5, 2000u, -123456789012LL, 5000000ULL, 1);
	struct B q = mk(15, 127u, 262143, 0u, 7, 2047u, 549755813887LL, 8388607ULL, 0);

	if (read_a(p) != -7 || read_a(q) != 15)
		return 1;
	if (read_b(p) != 100u || read_b(q) != 127u)
		return 2;
	if (read_c(p) != -100000 || read_c(q) != 262143)
		return 3;
	if (read_d(p) != 1 || read_d(q) != 0)
		return 4;
	if (read_e(p) != -5 || read_e(q) != 7)
		return 5;
	if (read_f(p) != 2000 || read_f(q) != 2047)
		return 6;
	if (read_g(p) != -123456789012LL || read_g(q) != 549755813887LL)
		return 7;
	if (read_h(p) != 5000000ULL || read_h(q) != 8388607ULL)
		return 8;
	if (read_i(p) != 1 || read_i(q) != 0)
		return 9;
	if (sum_ac(p) != -100007 || sum_ac(q) != 262158)
		return 10;
	if (mix_bf(p) != 200000u || mix_bf(q) != 259969u)
		return 11;
	if (cmp_ae(p) != 1 || cmp_ae(q) != 0)
		return 12;
	if (wide_gh(p) != -123456789012LL - 5000000LL)
		return 13;
	if (wide_gh(q) != 549755813887LL - 8388607LL)
		return 14;
	if (neg_a(p) != 7 || neg_a(q) != -15)
		return 15;
	if (not_b(p) != ~100 || not_b(q) != ~127)
		return 16;
	if (lnot_c(p) != 0 || lnot_c(q) != 0)
		return 17;
	if (tern_ad(p) != -7 || tern_ad(q) != 262143)
		return 18;
	if (shift_ca(p) != (-100000 >> 9) || shift_ca(q) != (262143 >> 15))
		return 19;
	if (nested(p) != (-7 + -5) * (-100000 - 2000) + 100)
		return 20;
	if (nested(q) != (15 + 7) * (262143 - 2047) + 127)
		return 21;
	return 0;
}
