int s6_2_1_filevar = 100;

static int s6_2_1_recurse(int n) {
	int local = n * 2;
	if (n == 0)
		return 0;
	return local + s6_2_1_recurse(n - 1);
}

static int s6_2_1_counter(void) {
	static int c = 10;
	c += 1;
	return c;
}

void s6_2_1_scopes_test(void) {
	int a, b, cc;

	int x = 1;
	printf("x=%d\n", x);
	{
		int x = 2;
		printf("x=%d\n", x);
		{
			int x = 3;
			printf("x=%d\n", x);
		}
		printf("x=%d\n", x);
	}
	printf("x=%d\n", x);

	{
		int lx = 0;
		goto lx_label;
	lx_label:
		lx = 5;
		printf("label-lx=%d\n", lx);
	}

	{
		int i, sum = 0;
		for (i = 0; i < 3; i++) {
			int v = i + 10;
			v += 1;
			sum += v;
		}
		printf("sum=%d\n", sum);
	}

	printf("recurse=%d\n", s6_2_1_recurse(4));

	a = s6_2_1_counter();
	b = s6_2_1_counter();
	cc = s6_2_1_counter();
	printf("counter=%d %d %d\n", a, b, cc);

	{
		extern int s6_2_1_filevar;
		s6_2_1_filevar += 5;
	}
	printf("filevar=%d\n", s6_2_1_filevar);

	{
		struct s6_2_1_t1 {
			int val;
		} obj;
		int s6_2_1_t1 = 42;
		obj.val = s6_2_1_t1 + 1;
		printf("tag-vs-ord=%d %d\n", s6_2_1_t1, obj.val);
	}

	{
		struct s6_2_1_A {
			int mem;
		};
		struct s6_2_1_B {
			int mem;
		};
		int mem = 99;
		struct s6_2_1_A sa;
		sa.mem = 1;
		struct s6_2_1_B sb;
		sb.mem = 2;
		printf("members=%d %d %d\n", sa.mem, sb.mem, mem);
	}

	{
		enum s6_2_1_e { s6_2_1_E_A = 10,
						s6_2_1_E_B = s6_2_1_E_A + 1,
						s6_2_1_E_C = s6_2_1_E_B * 2 };
		printf("enum=%d %d %d\n", (int)s6_2_1_E_A, (int)s6_2_1_E_B, (int)s6_2_1_E_C);
	}

	{
		struct s6_2_1_node {
			int v;
			struct s6_2_1_node *next;
		};
		struct s6_2_1_node n2;
		n2.v = 2;
		n2.next = NULL;
		struct s6_2_1_node n1;
		n1.v = 1;
		n1.next = &n2;
		printf("selfref=%d %d\n", n1.v, n1.next->v);
	}
}
