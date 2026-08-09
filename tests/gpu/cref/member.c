struct S {
	int a;
	unsigned int b;
	short c;
	unsigned char d;
	long long e;
	unsigned long long f;
	signed char g;
	unsigned short h;
};

static int sum_ab(struct S s) { return s.a + (int)s.b; }

static int narrow_cd(struct S s) { return s.c - s.d; }

static int narrow_gh(struct S s) { return s.g * 2 + s.h; }

static long long wide_ef(struct S s) { return s.e - (long long)s.f; }

static unsigned int mixed_bd(struct S s) { return s.b / (s.d + 1u); }

static int cmp_ac(struct S s) { return s.a < s.c; }

static int neg_a(struct S s) { return -s.a; }

static int not_c(struct S s) { return ~s.c; }

static int lnot_d(struct S s) { return !s.d; }

static int tern_ab(struct S s) { return s.a > 0 ? (int)s.b : s.c; }

static long long shift_ea(struct S s) { return s.e >> (s.a & 31); }

static int nested(struct S s) { return (s.a + s.c) * (s.g - s.d) + (int)s.b; }

static struct S mk(int a, unsigned int b, short c, unsigned char d, long long e,
									 unsigned long long f, signed char g, unsigned short h) {
	struct S s;
	s.a = a;
	s.b = b;
	s.c = c;
	s.d = d;
	s.e = e;
	s.f = f;
	s.g = g;
	s.h = h;
	return s;
}

int main(void) {
	struct S p = mk(-7, 9u, -300, 200, -1234567890123LL, 5ULL, -5, 60000);
	struct S q = mk(100000, 4294967295u, 32767, 255, 9223372036854775807LL,
									18446744073709551615ULL, 127, 1);

	if (sum_ab(p) != 2)
		return 1;
	if (sum_ab(q) != (int)(100000 + (int)4294967295u))
		return 2;
	if (narrow_cd(p) != -500)
		return 3;
	if (narrow_cd(q) != 32512)
		return 4;
	if (narrow_gh(p) != 59990)
		return 5;
	if (narrow_gh(q) != 255)
		return 6;
	if (wide_ef(p) != -1234567890128LL)
		return 7;
	if (wide_ef(q) != 9223372036854775807LL - (long long)18446744073709551615ULL)
		return 8;
	if (mixed_bd(p) != 0u)
		return 9;
	if (mixed_bd(q) != 4294967295u / 256u)
		return 10;
	if (cmp_ac(p) != 0)
		return 11;
	if (cmp_ac(q) != 0)
		return 12;
	if (neg_a(p) != 7)
		return 13;
	if (neg_a(q) != -100000)
		return 14;
	if (not_c(p) != 299)
		return 15;
	if (not_c(q) != -32768)
		return 16;
	if (lnot_d(p) != 0)
		return 17;
	if (lnot_d(q) != 0)
		return 18;
	if (tern_ab(p) != -300)
		return 19;
	if (tern_ab(q) != (int)4294967295u)
		return 20;
	if (shift_ea(p) != (-1234567890123LL >> 25))
		return 21;
	if (shift_ea(q) != (9223372036854775807LL >> (100000 & 31)))
		return 22;
	if (nested(p) != (-307) * (-205) + 9)
		return 23;
	if (nested(q) != (100000 + 32767) * (127 - 255) + (int)4294967295u)
		return 24;
	return 0;
}
