#include <stdio.h>
#include <stdlib.h>

static double a[600][600], b[600][600], c[600][600];

int main(int argc, char **argv) {
	int n = argc > 1 ? atoi(argv[1]) : 200;
	int reps = argc > 2 ? atoi(argv[2]) : 1;
	int i, j, k, r;
	double sum = 0.0;
	if (n > 600)
		n = 600;
	for (i = 0; i < n; i++)
		for (j = 0; j < n; j++) {
			a[i][j] = (i * 3 + j * 7) % 11 * 0.25 - 1.0;
			b[i][j] = (i * 5 + j * 2) % 13 * 0.125 - 0.5;
		}
	for (r = 0; r < reps; r++)
		for (i = 0; i < n; i++)
			for (k = 0; k < n; k++) {
				double aik = a[i][k];
				for (j = 0; j < n; j++)
					c[i][j] += aik * b[k][j];
			}
	for (i = 0; i < n; i++)
		sum += c[i][i];
	printf("matmul %d %.6f\n", n, sum);
	return 0;
}
