#include <stdio.h>
#include <stdlib.h>

static long work(int n, int pass)
{
	int buf[64];
	long acc = 0;
	int i, j;
	for (i = 0; i < 64; i++)
		buf[i] = (i * pass + 1) & 0x3ff;
	for (j = 0; j < n; j++) {
		int t = buf[j & 63];
		for (i = 0; i < 64; i++)
			buf[i] = (buf[i] + t + i) & 0xffff;
		acc += buf[j & 63];
	}
	return acc;
}

static long vwork(int n, int pass)
{
	int m = 32 + (pass & 31);
	int vla[64];
	long acc = 0;
	int i, j;
	for (i = 0; i < m; i++)
		vla[i] = (i ^ pass) & 0x1ff;
	for (j = 0; j < n; j++) {
		for (i = 1; i < m; i++)
			vla[i] = (vla[i] + vla[i - 1] + j) & 0xffff;
		acc += vla[m - 1];
	}
	return acc;
}

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 40000;
	int p;
	long acc = 0;
	for (p = 0; p < 8; p++) {
		acc += work(n, p);
		acc += vwork(n, p);
	}
	printf("vlaloop %ld\n", acc);
	return 0;
}
