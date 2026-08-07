#include <stdio.h>
#include <stdlib.h>

#define N 256

static double a[N][N], b[N][N], c[N][N], d[N][N];
static double rowsum[N], colsum[N];

int main(int argc, char **argv)
{
	int reps = argc > 1 ? atoi(argv[1]) : 6;
	int r, i, j, k;
	double acc = 0.0;

	for (i = 0; i < N; i++)
		for (j = 0; j < N; j++) {
			a[i][j] = (i * 3 + j * 5) % 17 * 0.25 - 2.0;
			b[i][j] = (i * 7 + j * 2) % 19 * 0.125 - 1.0;
		}

	for (r = 0; r < reps; r++) {
		for (j = 0; j < N; j++)
			for (i = 0; i < N; i++)
				c[i][j] = a[i][j] * 1.5 + b[i][j];

		for (i = 0; i < N; i++)
			for (j = 0; j < N; j++)
				d[i][j] = c[i][j] - b[i][j] * 0.25;

		for (i = 0; i < N; i++)
			rowsum[i] = 0.0;
		for (j = 0; j < N; j++)
			colsum[j] = 0.0;
		for (i = 0; i < N; i++)
			for (j = 0; j < N; j++)
				rowsum[i] += d[i][j];
		for (i = 0; i < N; i++)
			for (j = 0; j < N; j++)
				colsum[j] += d[i][j];

		for (i = 0; i < N; i++)
			for (k = 0; k < N; k++) {
				double aik = a[i][k];
				for (j = 0; j < N; j++)
					c[i][j] += aik * b[k][j];
			}
	}
	for (i = 0; i < N; i++)
		acc += rowsum[i] + colsum[i] + c[i][i] + d[i][i];
	printf("loopnest %.6f\n", acc);
	return 0;
}
