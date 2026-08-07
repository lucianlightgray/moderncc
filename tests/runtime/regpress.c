#include <stdio.h>
#include <stdlib.h>

static double fbuf[4096];
static long ibuf[4096];

static double heavy(double s)
{
	double a0 = s, a1 = s * 1.5, a2 = s + 2.0, a3 = s - 3.0;
	double a4 = s * 0.25, a5 = s + 5.5, a6 = s * 2.5, a7 = s - 7.25;
	double a8 = s + 8.125, a9 = s * 3.75, aa = s - 10.5, ab = s + 11.25;
	double ac = s * 4.5, ad = s - 13.0, ae = s + 14.75, af = s * 5.125;
	a0 = a0 * a1 + a2;
	a3 = a3 * a4 + a5;
	a6 = a6 * a7 + a8;
	a9 = a9 * aa + ab;
	ac = ac * ad + ae;
	a1 = a1 + a0 * a3;
	a4 = a4 + a6 * a9;
	a7 = a7 + ac * af;
	aa = aa + a1 * a4;
	ad = ad + a7 * aa;
	return a0 + a1 + a3 + a4 + a6 + a7 + a9 + aa + ac + ad + ae + af;
}

static long iheavy(long s)
{
	long b0 = s, b1 = s ^ 0x5555, b2 = s + 3, b3 = s - 7;
	long b4 = s << 1, b5 = s >> 2, b6 = s * 11, b7 = s ^ 0x0f0f;
	long b8 = s + 17, b9 = s - 23, ba = s * 5, bb = s ^ 0x1234;
	b0 = b0 * b1 + b2;
	b3 = b3 ^ (b4 + b5);
	b6 = b6 * b7 - b8;
	b9 = b9 ^ (ba + bb);
	b1 = b1 + b0 * b3;
	b4 = b4 ^ (b6 * b9);
	b7 = b7 + b1 * b4;
	return b0 + b1 + b3 + b4 + b6 + b7 + b9 + ba + bb;
}

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 40000;
	int r, i;
	double facc = 0.0;
	long iacc = 0;
	for (i = 0; i < 4096; i++) {
		fbuf[i] = i * 0.03125 - 32.0;
		ibuf[i] = (i * 2654435761L) & 0xffffff;
	}
	for (r = 0; r < n; r++)
		for (i = 0; i < 64; i++) {
			facc += heavy(fbuf[(r + i) & 4095]);
			iacc += iheavy(ibuf[(r * 3 + i) & 4095]);
		}
	printf("regpress %.6f %ld\n", facc, iacc);
	return 0;
}
