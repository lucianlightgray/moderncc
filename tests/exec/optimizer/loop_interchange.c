#include <stdio.h>

#define N 24

static int A[N][N], B[N][N], C[N][N], D[N][N];

static unsigned long long hh;
static void mix(long long v) { hh = hh * 1099511628211ULL ^ (unsigned long long)v; }

/* col-major store from array reads: interchange-beneficial, array-only, legal. */
static void copy_add(void) {
	for (int j = 0; j < N; j++)
		for (int i = 0; i < N; i++)
			D[i][j] = A[i][j] + B[i][j];
}

/* transpose read: locality-neutral, must stay correct whether or not it fires. */
static void trans(void) {
	for (int j = 0; j < N; j++)
		for (int i = 0; i < N; i++)
			C[i][j] = A[j][i] * 2 - 1;
}

/* row-major store: already unit-stride innermost, must NOT be interchanged. */
static void fill(void) {
	for (int i = 0; i < N; i++)
		for (int j = 0; j < N; j++)
			A[i][j] = i * 7 + j * 3;
}

/* NON-associative scalar accumulator: reordering would change the result, so the
   transform must refuse (scalar carried dependence the dep test cannot model). */
static long long scal(void) {
	long long s = 1;
	for (int j = 0; j < N; j++)
		for (int i = 0; i < N; i++)
			s = s * 31 + A[i][j];
	return s;
}

/* loop-carried array dependence: result must be preserved. */
static void stencil(void) {
	for (int i = 1; i < N; i++)
		for (int j = 1; j < N; j++)
			A[i][j] = A[i - 1][j] + A[i][j - 1];
}

int main(void) {
	hh = 1469598103934665603UL;
	for (int i = 0; i < N; i++)
		for (int j = 0; j < N; j++) {
			A[i][j] = (i * 31 + j * 17) % 101;
			B[i][j] = (i + 2 * j) % 97;
		}
	fill();
	copy_add();
	trans();
	long long sc = scal();
	stencil();
	for (int i = 0; i < N; i++)
		for (int j = 0; j < N; j++) {
			mix(A[i][j]);
			mix(C[i][j]);
			mix(D[i][j]);
		}
	printf("%llu %lld\n", hh, sc);
	return 0;
}
