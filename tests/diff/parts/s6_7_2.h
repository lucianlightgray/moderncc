struct s6_7_2_anon {
	int top;
	struct {
		int mid;
		union {
			int deep;
			int deep2;
		};
	};
};

struct s6_7_2_fam {
	int n;
	int a[];
};

struct s6_7_2_boolbf {
	_Bool b : 1;
	unsigned rest : 7;
};

enum s6_7_2_e { s6_7_2_A,
								s6_7_2_B,
								s6_7_2_C = 10,
								s6_7_2_D,
								s6_7_2_E = 10,
};

void s6_7_2_specifiers(void) {
	struct s6_7_2_anon x;
	x.top = 1;
	x.mid = 2;
	x.deep = 3;
	printf("anon: %d %d %d\n", x.top, x.mid, x.deep);
	x.deep2 = 7;
	printf("anon-union-overlap: %d\n", x.deep == 7);

	printf("fam-size: %d\n", (int)(sizeof(struct s6_7_2_fam) == sizeof(int)));
	struct s6_7_2_fam *f = malloc(sizeof(struct s6_7_2_fam) + 3 * sizeof(int));
	f->n = 3;
	for (int i = 0; i < f->n; i++)
		f->a[i] = i * 10;
	printf("fam: %d %d %d\n", f->a[0], f->a[1], f->a[2]);
	free(f);

	struct s6_7_2_boolbf bb;
	bb.b = 1;
	printf("boolbf1: %d\n", bb.b == 1);
	bb.b = 2;
	printf("boolbf2: %d\n", bb.b == 1);
	bb.b = 0;
	printf("boolbf0: %d\n", bb.b == 0);

	printf("enum: %d %d %d %d %d\n",
				 s6_7_2_A, s6_7_2_B, s6_7_2_C, s6_7_2_D, s6_7_2_E);
	printf("enum-dup: %d\n", s6_7_2_C == s6_7_2_E);
	printf("enum-int: %d\n", (int)(sizeof(s6_7_2_A) == sizeof(int)));

	_Atomic(int) g;
	g = 42;
	printf("atomic: %d\n", (int)g);
	printf("atomic-size: %d\n", (int)(sizeof(_Atomic(int)) == sizeof(int)));

	union s6_7_2_u {
		char c;
		int i;
		double d;
	} u;
	printf("union-size: %d\n", (int)(sizeof(u) >= sizeof(double)));
	u.d = 0;
	u.i = 99;
	printf("union-store: %d\n", u.i);

	struct s6_7_2_pack {
		unsigned a : 5, b : 5;
	} pk;
	pk.a = 31;
	pk.b = 17;
	printf("pack: %d %d %d\n", pk.a, pk.b, (int)(sizeof(pk) == sizeof(unsigned)));
}
