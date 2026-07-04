#define S6_7_7_MAX 10
void s6_7_7_init_both_ends(void) {
	int a[S6_7_7_MAX] = {1, 3, 5, 7, 9, [S6_7_7_MAX - 5] = 8, 6, 4, 2, 0};
	int i;
	printf("both:");
	for (i = 0; i < S6_7_7_MAX; i++)
		printf(" %d", a[i]);
	printf("\n");

	int b[3] = {[0] = 1, [1] = 2, [0] = 9};
	printf("override: %d %d %d\n", b[0], b[1], b[2]);
}

void s6_7_7_init_inconsistent(void) {
	struct {
		int a[3], b;
	} w[] = {{1}, 2};
	int n = (int)(sizeof w / sizeof w[0]);
	printf("w5: n=%d %d %d %d %d %d\n", n,
		   w[0].a[0], w[0].a[1], w[0].a[2], w[0].b, w[1].a[0]);
}

void s6_7_7_init_mixed(void) {
	struct {
		int a[3], b;
	} w[] = {[0].a = {1}, [1].a[0] = 2};
	int n = (int)(sizeof w / sizeof w[0]);
	printf("w11: n=%d %d %d\n", n, w[0].a[0], w[1].a[0]);
}

void s6_7_7_init_elision(void) {
	short q[4][3][2] = {{1}, {2, 3}, {4, 5, 6}};
	printf("q6: %d %d %d %d %d %d %d\n",
		   q[0][0][0], q[0][1][0], q[1][0][0], q[1][0][1],
		   q[2][0][0], q[2][0][1], q[2][1][0]);
}

void s6_7_7_init_union_next(void) {
	struct {
		union {
			int a;
			char b;
		} u;
		int c;
	} s = {{5}, 7};
	printf("un: %d %d\n", s.u.a, s.c);
	union {
		char c;
		int i;
		long l;
	} u = {.i = 42};
	printf("uany: %d\n", u.i);
}

enum s6_7_7_e { s6_7_7_one,
				s6_7_7_two,
				s6_7_7_three };
void s6_7_7_init_enum_desig(void) {
	int a[4] = {[s6_7_7_two] = 20, [s6_7_7_one] = 10};
	printf("edes: %d %d %d %d\n", a[0], a[1], a[2], a[3]);
	div_t answer = {.quot = 2, .rem = -1};
	printf("dt: %d %d\n", answer.quot, answer.rem);
}

void s6_7_7_init_sparse(void) {
	int a[] = {[10] = 1, [3] = 2};
	printf("sparse: sz=%d %d %d\n",
		   (int)(sizeof a / sizeof a[0]), a[3], a[10]);
}

void s6_7_7_init_string(void) {
	char s[] = "abc";
	char t[3] = "abc";
	printf("str: %d %d %c%c%c %c%c%c\n",
		   (int)sizeof s, (int)sizeof t, s[0], s[1], s[2], t[0], t[1], t[2]);
}

void s6_7_7_typedef_vla(void) {
	int n = 3;
	typedef int s6_7_7_B[n];
	n += 100;
	s6_7_7_B arr;
	printf("vlatd: %d\n", (int)(sizeof arr / sizeof arr[0]));
}

typedef struct s6_7_7_tnode s6_7_7_TNODE;
struct s6_7_7_tnode {
	int count;
	s6_7_7_TNODE *left, *right;
};
void s6_7_7_typedef_incomplete(void) {
	s6_7_7_TNODE a, b, c;
	a.count = 1;
	a.left = &b;
	a.right = &c;
	b.count = 2;
	b.left = 0;
	b.right = 0;
	c.count = 3;
	c.left = 0;
	c.right = 0;
	printf("tnode: %d %d %d\n",
		   a.count, a.left->count, a.right->count);
}

void s6_7_7_init_flat(void) {
	struct {
		int a[2];
		int b;
	} s = {1, 2, 3};
	printf("flat: %d %d %d\n", s.a[0], s.a[1], s.b);
}
