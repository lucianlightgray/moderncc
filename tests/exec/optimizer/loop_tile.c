#include <stdio.h>

#define N 37
#define M 50

static int A[N][M], B[N][M];

static unsigned long long hh;
static void mix(long long v) { hh = hh * 1099511628211ULL ^ (unsigned long long)v; }


static void elementwise(void) {
	for (int i = 0; i < N; i++)
		for (int j = 0; j < M; j++)
			A[i][j] = B[i][j] * 3 + i - j;
}


static void forward(void) {
	for (int i = 0; i < N; i++)
		for (int j = 1; j < M; j++)
			A[i][j] = A[i][j - 1] + B[i][j];
}


static void skewed(void) {
	for (int i = 1; i < N; i++)
		for (int j = 0; j < M - 1; j++)
			A[i][j] = A[i - 1][j + 1] + 1;
}

int (*RP)[M];
int (*RQ)[M];

static void aliased_rowptr_skew(void) {
	for (int i = 1; i < M - 1; i++)
		for (int j = 1; j < N; j++)
			RP[j][i] = RQ[j - 1][i + 1] + 1;
}

int main(void) {
	hh = 1469598103934665603ULL;
	for (int i = 0; i < N; i++)
		for (int j = 0; j < M; j++)
			B[i][j] = (i * 7 + j * 13) % 101;
	for (int i = 0; i < N; i++)
		A[i][0] = i + 1;
	elementwise();
	forward();
	skewed();
	RP = B;
	RQ = B;
	aliased_rowptr_skew();
	for (int i = 0; i < N; i++)
		for (int j = 0; j < M; j++) {
			mix(A[i][j]);
			mix(B[i][j]);
		}
	printf("%llu\n", hh);
	return 0;
}
