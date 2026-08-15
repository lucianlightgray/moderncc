/* ci/reproducible-object subject (T-lin-10374): a broad codegen surface so that
 * "two mcc -c of this file are byte-identical" is a meaningful determinism
 * assertion, not a trivial one. Self-contained (only a printf prototype). An
 * object file carries no .note.gnu.build-id (that is added at link), so any
 * byte difference between two compiles is genuine codegen nondeterminism. */
int printf(const char *, ...);

struct pt { int x, y; };
union u { long l; double d; struct pt p; };
struct bits { unsigned a : 3; int b : 5; unsigned long long c : 40; int : 0; char d; };

static const char *const names[] = {"alpha", "beta", "gamma", "delta"};
static double table[8] = {0.5, 1.25, -3.0, 42.0, 0.0, 1e10, -1e-10, 3.14159};

static long fib(long n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

static int classify(int c) {
	switch (c) {
	case 'a': case 'e': case 'i': case 'o': case 'u': return 1;
	case '0' ... '9': return 2;
	default: return c >= 'a' && c <= 'z' ? 3 : 0;
	}
}

static double dot(const struct pt *a, const struct pt *b) {
	return (double)a->x * b->x + (double)a->y * b->y;
}

static unsigned long mix(struct bits *s, unsigned long seed) {
	s->a = (unsigned)(seed & 7);
	s->b = (int)(seed >> 3) & 0xf;
	s->c = seed ^ 0x5a5a5a5aUL;
	return s->a + (unsigned long)s->b + s->c + (unsigned char)s->d;
}

typedef int (*fp)(int);

int main(void) {
	struct pt p = {3, 4}, q = {-1, 2};
	union u u;
	struct bits bs = {0};
	fp f = classify;
	long acc = 0;
	u.d = dot(&p, &q);

	for (int i = 0; i < 4; i++)
		acc += (long)names[i][0] + (long)(table[i] * 100.0);

	for (unsigned long s = 0; s < 5; s++)
		acc += (long)mix(&bs, s * 2654435761UL);

	acc += fib(20);
	acc += f('e') * 10 + f('7') * 100 + f('z');

	printf("acc=%ld u=%f cls=%d\n", acc, u.d, classify('x'));
	return (int)(acc & 0x7f);
}
