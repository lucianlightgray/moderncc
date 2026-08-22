extern int printf(const char *, ...);

/* ast_sroa (TREE_SROA, -O4) scalar-replaces a never-address-taken local
 * aggregate with per-member scalars. The pass is type-complete: it must split a
 * struct whose members are double/float/pointer/long long, not only int. This
 * subject is DOMINATED by non-int-member structs -- an int-only SROA would fire
 * only on `si` (the lone int struct) and miss the rest, so the >=10 floor proves
 * type-generality, not just that SROA runs. Every struct is a local, fully
 * defined then read, never address-taken => SROA-eligible on every target (the
 * count is an AST-level property, arch-independent). Correctness: -O4 == -O0. */

static double sd(double a, double b)
{
	struct { double x, y; } p;
	p.x = a * 2.0;
	p.y = b + 1.0;
	return p.x + p.y;
}

static float sf(float a, float b)
{
	struct { float x, y; } p;
	p.x = a - 1.0f;
	p.y = b * 3.0f;
	return p.x + p.y;
}

static long sp(int *a, int *b)
{
	struct { int *x, *y; } p;
	p.x = a;
	p.y = b;
	return (p.x != 0) + (p.y != 0) * 2;
}

static long long sq(long long a, long long b)
{
	struct { long long x, y; } p;
	p.x = a << 1;
	p.y = b + 7;
	return p.x + p.y;
}

static int si(int a, int b)
{
	struct { int x, y; } p;
	p.x = a;
	p.y = b;
	return p.x + p.y;
}

int main(void)
{
	int v = 3, w = 4;
	printf("%.1f %.1f %ld %lld %d\n", sd(3.0, 4.0), (double)sf(3.0f, 4.0f),
				 sp(&v, &w), sq(3, 4), si(3, 4));
	return 0;
}
