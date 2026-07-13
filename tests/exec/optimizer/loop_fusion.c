#include <stdio.h>

#define N 40

static int a[N], b[N], c[N], d[N];

static unsigned long long hh;
static void mix(long long v) { hh = hh * 1099511628211ULL ^ (unsigned long long)v; }

/* distinct induction variables, same trip, disjoint arrays: fusable. */
static void distinct_iv(void) {
	for (int i = 0; i < N; i++)
		a[i] = i * 3 + 1;
	for (int j = 0; j < N; j++)
		b[j] = a[j] + 2;
}

/* forward (loop-independent) dependence c[i] -> c[i]: fusable, result preserved. */
static void forward_dep(void) {
	for (int i = 0; i < N; i++)
		c[i] = a[i] + b[i];
	for (int i = 0; i < N; i++)
		d[i] = c[i] * 2 - 1;
}

/* backward cross-loop dependence a[i] written, then a[i+1] read, and a shorter
   trip: fusion is illegal and must be refused; result must be preserved. */
static void backward_dep(void) {
	for (int i = 0; i < N; i++)
		a[i] = i + 7;
	for (int i = 0; i < N - 1; i++)
		b[i] = a[i + 1] - 1;
}

int main(void) {
	hh = 1469598103934665603ULL;
	distinct_iv();
	forward_dep();
	backward_dep();
	for (int i = 0; i < N; i++) {
		mix(a[i]);
		mix(b[i]);
		mix(c[i]);
		mix(d[i]);
	}
	printf("%llu\n", hh);
	return 0;
}
