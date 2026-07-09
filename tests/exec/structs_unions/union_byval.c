#include <stdio.h>
#include <string.h>

typedef union {
	int i;
	float f;
	unsigned char b[4];
} U4;
typedef union {
	long l;
	double d;
	struct {
		int lo, hi;
	} p;
	unsigned char b[8];
} U8;

static void dump(const char *t, const void *p, int n) {
	const unsigned char *b = p;
	printf("%s:", t);
	for (int i = 0; i < n; i++)
		printf(" %02x", b[i]);
	printf("\n");
}

static U4 u4_mut(U4 u) {
	u.i ^= 0x0f0f0f0f;
	return u;
}
static U8 u8_pass(U8 u) {
	return u;
}

struct A {
	int x, y;
};
struct B {
	int x, y;
};
typedef union {
	struct A *pa;
	struct B *pb;
	void *pv;
}
__attribute__((transparent_union)) TU;
static int tu_first(TU u) {
	return *(int *)u.pv;
}
static int tu_nested(TU u) {
	return tu_first(u) + 1;
}

int main(void) {
	U4 a;
	memset(&a, 0, sizeof a);
	a.f = 1.5f;
	a = u4_mut(u4_mut(a));
	printf("U4 %g\n", a.f);
	dump("U4", &a, sizeof a);

	U8 b;
	memset(&b, 0, sizeof b);
	b.d = 2.5;
	b = u8_pass(b);
	printf("U8 %g %d %d\n", b.d, b.p.lo, b.p.hi);
	dump("U8", &b, sizeof b);

	struct A A = {11, 99};
	struct B B = {22, 99};
	int n = 77;
	printf("TU %d %d %d %d\n",
				 tu_first(&A), tu_first(&B), tu_nested(&A), tu_first((TU){.pv = &n}));
	return 0;
}
