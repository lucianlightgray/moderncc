/*
 * spectral-norm, fork-join threading.
 *
 * Ported from The Computer Language Benchmarks Game entry
 * vendor/plb/bench/algorithm/spectral-norm/3.c (Contributed by Mr Ledrug),
 * replacing its two `#pragma omp parallel for` regions with C11 <threads.h>:
 * each mult_Av / mult_Atv spawns NT worker threads over disjoint row ranges
 * and joins them (fork-join per parallel region). Builds unchanged against a
 * native <threads.h> (glibc/pthread) and against mcc's cooperative backend
 * (-DMCC_THREADS_COOP), so the same source measures both.
 */

/*
 * <threads.h> must lead, and the usual libc headers are declared by hand
 * rather than #included: mcc's cooperative backend (-DMCC_THREADS_COOP) defines
 * its own `once_flag` in mcc_coop_threads.h, which collides with the `once_flag`
 * that glibc's <stdlib.h> pulls in (bits/types/once_flag.h). Declaring the few
 * functions we use keeps ONE source building under the native, coop, gcc and
 * clang toolchains alike (this mirrors how mcc_coop_threads.h declares malloc).
 */
#include <threads.h>

extern void *malloc(__SIZE_TYPE__);
extern void free(void *);
extern int atoi(const char *);
extern int printf(const char *, ...);
extern double sqrt(double);

#ifndef NT
#define NT 4
#endif

static int A(int i, int j) { return (i + j) * (i + j + 1) / 2 + i + 1; }

static double dot(const double *v, const double *u, int n) {
	double sum = 0;
	for (int i = 0; i < n; i++)
		sum += v[i] * u[i];
	return sum;
}

struct mul_arg {
	const double *v;
	double *out;
	int n;
	int lo, hi;
	int transpose;
};

static int mul_worker(void *arg) {
	struct mul_arg *a = arg;
	for (int i = a->lo; i < a->hi; i++) {
		double sum = 0;
		if (a->transpose)
			for (int j = 0; j < a->n; j++)
				sum += a->v[j] / A(j, i);
		else
			for (int j = 0; j < a->n; j++)
				sum += a->v[j] / A(i, j);
		a->out[i] = sum;
	}
	return 0;
}

static void mult(const double *v, double *out, int n, int transpose) {
	thrd_t t[NT];
	struct mul_arg args[NT];
	int per = (n + NT - 1) / NT;
	int nt = 0;
	for (int k = 0; k < NT; k++) {
		int lo = k * per;
		int hi = lo + per;
		if (hi > n)
			hi = n;
		if (lo >= hi)
			break;
		args[nt].v = v;
		args[nt].out = out;
		args[nt].n = n;
		args[nt].lo = lo;
		args[nt].hi = hi;
		args[nt].transpose = transpose;
		if (thrd_create(&t[nt], mul_worker, &args[nt]) != thrd_success)
			mul_worker(&args[nt]);
		else
			nt++;
	}
	for (int k = 0; k < nt; k++)
		thrd_join(t[k], NULL);
}

static double *tmp;

static void mult_AtAv(const double *v, double *out, int n) {
	mult(v, tmp, n, 0);
	mult(tmp, out, n, 1);
}

int main(int argc, char **argv) {
	int n = argc > 1 ? atoi(argv[1]) : 2000;
	if (n <= 0)
		n = 2000;

	double *u = malloc(n * sizeof(double));
	double *v = malloc(n * sizeof(double));
	tmp = malloc(n * sizeof(double));

	for (int i = 0; i < n; i++)
		u[i] = 1;
	for (int i = 0; i < 10; i++) {
		mult_AtAv(u, v, n);
		mult_AtAv(v, u, n);
	}

	printf("%.9f\n", sqrt(dot(u, v, n) / dot(v, v, n)));

	free(u);
	free(v);
	free(tmp);
	return 0;
}
