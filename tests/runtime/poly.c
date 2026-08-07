#include <stdio.h>
#include <stdlib.h>

static int tab[1024];

static long common(int x, int y)
{
	long p = (long)x * y;
	long q = (long)x * y + 1;
	long r = (long)(x + y) * (x - y);
	long s = (long)(x + y) * (x - y) - 2;
	return p + q + r + s + (p * 2 - q) + (r * 3 + s);
}

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 300000;
	int i, k;
	long acc = 0;
	double dacc = 0.0;
	for (i = 0; i < 1024; i++)
		tab[i] = (i * 37 + 11) & 0x3ff;
	for (i = 0; i < n; i++) {
		int x = tab[i & 1023];
		int y = tab[(i * 7 + 3) & 1023];
		long a = (long)x + y + x + y + 3 + 4 + 5;
		long b = ((long)x * 2) * 4 * 8;
		long c = ((long)y << 3) >> 2;
		double d = x * 0.5;
		dacc += ((d + 1.0) + 2.0) + 3.0 + d * 4.0 + d * 4.0;
		acc += a + b + c + common(x, y);
		for (k = 0; k < 4; k++)
			acc += (long)x * k + (long)y * k - ((long)x * k);
	}
	printf("poly %ld %.6f\n", acc, dacc);
	return 0;
}
