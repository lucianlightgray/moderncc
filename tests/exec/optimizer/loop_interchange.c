#include <stdio.h>

#define N 24

static int A[N][N], B[N][N], C[N][N], D[N][N], E[N][N], F[N][N];

static unsigned long long hh;
static void mix(long long v) { hh = hh * 1099511628211ULL ^ (unsigned long long)v; }


static void copy_add(void) {
	for (int j = 0; j < N; j++)
		for (int i = 0; i < N; i++)
			D[i][j] = A[i][j] + B[i][j];
}


static void trans(void) {
	for (int j = 0; j < N; j++)
		for (int i = 0; i < N; i++)
			C[i][j] = A[j][i] * 2 - 1;
}


static void fill(void) {
	for (int i = 0; i < N; i++)
		for (int j = 0; j < N; j++)
			A[i][j] = i * 7 + j * 3;
}



static long long scal(void) {
	long long s = 1;
	for (int j = 0; j < N; j++)
		for (int i = 0; i < N; i++)
			s = s * 31 + A[i][j];
	return s;
}


static void stencil(void) {
	for (int i = 1; i < N; i++)
		for (int j = 1; j < N; j++)
			A[i][j] = A[i - 1][j] + A[i][j - 1];
}

int (*RP)[N];
int (*RQ)[N];

static void rowptr_skew(void) {
	for (int i = 1; i < N; i++)
		for (int j = 1; j < N; j++)
			RP[j][i] = RQ[j - 1][i + 1] + 1;
}

static void desc_inner_skew(void) {
	for (int i = 1; i < N; i++)
		for (int j = N - 1; j >= 1; j--)
			E[j][i] = E[j - 1][i - 1] + 1;
}


static void desc_both_skew(void) {
	for (int i = N - 1; i >= 1; i--)
		for (int j = N - 1; j >= 1; j--)
			F[j][i] = F[j - 1][i - 1] * 2 + 3;
}

int main(void) {
	hh = 1469598103934665603UL;
	for (int i = 0; i < N; i++)
		for (int j = 0; j < N; j++) {
			A[i][j] = (i * 31 + j * 17) % 101;
			B[i][j] = (i + 2 * j) % 97;
			E[i][j] = (i * 13 + j * 5) % 89;
			F[i][j] = (i * 3 + j * 11) % 83;
		}
	fill();
	copy_add();
	trans();
	long long sc = scal();
	stencil();
	RP = B;
	RQ = B;
	rowptr_skew();
	desc_inner_skew();
	desc_both_skew();
	for (int i = 0; i < N; i++)
		for (int j = 0; j < N; j++) {
			mix(A[i][j]);
			mix(B[i][j]);
			mix(C[i][j]);
			mix(D[i][j]);
			mix(E[i][j]);
			mix(F[i][j]);
		}
	printf("%llu %lld\n", hh, sc);
	return 0;
}
