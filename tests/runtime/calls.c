#include <stdio.h>
#include <stdlib.h>

static int lane(int a, int b) { return a * 3 + b; }
static int lane2(int a, int b, int c) { return lane(a, b) ^ (c << 1); }
static int lane3(int a, int b, int c, int d) { return lane2(a, b, c) + d * 5; }
static int lane4(int a, int b, int c, int d, int e)
{
	return lane3(a, b, c, d) - e;
}
static int lane5(int a, int b, int c, int d, int e, int f)
{
	return lane4(a, b, c, d, e) + f * 7;
}

static long tailsum(long n, long acc)
{
	if (n <= 0)
		return acc;
	return tailsum(n - 1, acc + (n & 0x3f));
}

static long ack(long m, long n)
{
	if (m == 0)
		return n + 1;
	if (n == 0)
		return ack(m - 1, 1);
	return ack(m - 1, ack(m, n - 1));
}

static double blend(double a, double b, double t)
{
	return a + (b - a) * t;
}

static double chain(double x, int depth)
{
	if (depth == 0)
		return x;
	return blend(x, chain(x * 0.5 + 0.25, depth - 1), 0.5);
}

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 400000;
	int i;
	long acc = 0;
	double d = 0.0;
	for (i = 0; i < n; i++) {
		acc += lane5(i, i + 1, i + 2, i + 3, i + 4, i + 5);
		acc ^= lane3(i, i >> 1, i >> 2, i >> 3);
		d += chain(i * 1e-6, 6);
	}
	for (i = 0; i < 16; i++)
		acc += tailsum(4000L, 0L);
	acc += ack(2L, 7L);
	printf("calls %ld %.9f\n", acc, d);
	return 0;
}
