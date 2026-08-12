#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static int data[1 << 16];
static double fdata[1 << 16];

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 200;
	int r, i;
	long acc = 0;
	double facc = 0.0;
	for (i = 0; i < (1 << 16); i++) {
		data[i] = (int)(((unsigned)i * 1103515245u + 12345u) >> 5) % 2001 - 1000;
		fdata[i] = data[i] * 0.125;
	}
	for (r = 0; r < n; r++) {
		int lo = 1 << 30, hi = -(1 << 30);
		for (i = 0; i < (1 << 16); i++) {
			int v = data[i];
			int a = v < 0 ? -v : v;
			acc += a;
			if (v < lo)
				lo = v;
			if (v > hi)
				hi = v;
			acc += v > 0 ? v >> 2 : v << 1;
			facc += fabs(fdata[i]) > 50.0 ? fdata[i] : -fdata[i];
			facc += fdata[i] < 0.0 ? 0.0 : fdata[i] * 0.5;
		}
		acc += lo + hi;
	}
	printf("branchy %ld %.6f\n", acc, facc);
	return 0;
}
