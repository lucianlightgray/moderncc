static int s6_5_1_side_calls = 0;
static int s6_5_1_side(void) {
	s6_5_1_side_calls++;
	return 3;
}
static int s6_5_1_evaled = 0;
static int s6_5_1_mark(int v) {
	s6_5_1_evaled++;
	return v;
}

#define S6_5_1_TAG(x) _Generic((x), \
		int: 1,                         \
		unsigned: 2,                    \
		long: 3,                        \
		double: 4,                      \
		char *: 5,                      \
		default: 0)

struct s6_5_1_a {
	int tag;
	long ax;
};
struct s6_5_1_b {
	int tag;
	double bx;
};
union s6_5_1_u {
	struct s6_5_1_a a;
	struct s6_5_1_b b;
};

struct s6_5_1_anon {
	int outer;
	union {
		int ai;
		long al;
	};
	struct {
		int sx;
		int sy;
	};
};

void s6_5_1_expr(void) {

	int i = 0;
	unsigned u = 0u;
	long l = 0L;
	double d = 0.0;
	char *cp = "x";
	printf("gen int=%d uns=%d long=%d dbl=%d cp=%d\n",
				 S6_5_1_TAG(i), S6_5_1_TAG(u), S6_5_1_TAG(l), S6_5_1_TAG(d), S6_5_1_TAG(cp));

	s6_5_1_side_calls = 0;
	int r = _Generic(s6_5_1_side(), int: 111, default: 222);
	printf("gen_ctrl r=%d side_calls=%d\n", r, s6_5_1_side_calls);

	s6_5_1_evaled = 0;
	int r2 = _Generic(i, int: s6_5_1_mark(7), double: s6_5_1_mark(9), default: s6_5_1_mark(11));
	printf("gen_sel r2=%d evaled=%d\n", r2, s6_5_1_evaled);

	printf("gen_default=%d\n", _Generic((short)0, default: 55, long: 66));

	_Generic(i, int: i, default: r) = 99;
	printf("gen_lvalue i=%d\n", i);

	int arr[4] = {10, 11, 12, 13};
	printf("subscript a1=%d 1a=%d\n", arr[1], 1 [arr]);

	int m[2][3] = {{0, 1, 2}, {3, 4, 5}};
	printf("md m12=%d flat5=%d sz_row=%d\n",
				 m[1][2], (&m[0][0])[5], (int)sizeof(m[0]));

	union s6_5_1_u un;
	un.a.tag = 4242;
	printf("cis tag=%d\n", un.b.tag);

	struct s6_5_1_anon an;
	an.outer = 1;
	an.ai = 2;
	an.sx = 3;
	an.sy = 4;
	printf("anon %d %d %d %d\n", an.outer, an.ai, an.sx, an.sy);

	int *np = 0;
	printf("not np=%d not42=%d notnot=%d\n", !np, !42, !!42);

	printf("sizeof_unsigned=%d\n", (sizeof(int) - 5) > 0);
}
