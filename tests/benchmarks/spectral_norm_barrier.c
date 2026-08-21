/*
 * spectral-norm, persistent-team + barrier threading.
 *
 * Ported from The Computer Language Benchmarks Game entry
 * vendor/plb/bench/algorithm/spectral-norm/4.c (OpenMP parallelize by The Anh
 * Tran). The original spawns one long-lived team (`#pragma omp parallel`) that
 * runs the whole 10-iteration loop, synchronizing between the A*u and At*u
 * phases with `#pragma omp barrier`. This port keeps that structure but:
 *   - replaces the OpenMP team with NT C11 <threads.h> workers spawned once,
 *   - replaces `#pragma omp barrier` with a reusable mtx_t/cnd_t barrier,
 *   - de-vectorizes the SSE `__attribute__((vector_size(16)))` math to scalar
 *     doubles (mcc has no GCC vector types), computing the identical value.
 * The condition-variable barrier is what stresses the threading runtime:
 * under mcc's cooperative backend (-DMCC_THREADS_COOP) every barrier_wait is a
 * cnd_wait yield / cnd_broadcast resume through the fiber scheduler.
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
extern _Noreturn void exit(int);

#ifndef NT
#define NT 4
#endif

/* 1.0 / ( (i+j)*(i+j+1)/2 + i + 1 ) */
static double eval_A(int i, int j) {
	int d = (((i + j) * (i + j + 1)) >> 1) + i + 1;
	return 1.0 / d;
}

/* reusable sense-reversing barrier for a fixed party of `parties` threads */
typedef struct {
	mtx_t m;
	cnd_t c;
	int count;
	int phase;
	int parties;
} barrier_t;

static void barrier_init(barrier_t *b, int parties) {
	mtx_init(&b->m, mtx_plain);
	cnd_init(&b->c);
	b->count = 0;
	b->phase = 0;
	b->parties = parties;
}

static void barrier_wait(barrier_t *b) {
	mtx_lock(&b->m);
	int ph = b->phase;
	if (++b->count == b->parties) {
		b->count = 0;
		b->phase++;
		cnd_broadcast(&b->c);
	} else {
		while (ph == b->phase)
			cnd_wait(&b->c, &b->m);
	}
	mtx_unlock(&b->m);
}

struct team_arg {
	double *u, *tmp, *v;
	int N;
	int lo, hi;
	barrier_t *bar;
};

/* out[i] = sum_j src[j] * eval_A(i,j)  (or eval_A(j,i) when transpose) */
static void mult_range(const double *src, double *out, int N, int lo, int hi,
					   int transpose) {
	for (int i = lo; i < hi; i++) {
		double s = 0;
		if (transpose)
			for (int j = 0; j < N; j++)
				s += src[j] * eval_A(j, i);
		else
			for (int j = 0; j < N; j++)
				s += src[j] * eval_A(i, j);
		out[i] = s;
	}
}

static int team_worker(void *arg) {
	struct team_arg *p = arg;
	for (int ite = 0; ite < 10; ite++) {
		/* v = A^t A u */
		mult_range(p->u, p->tmp, p->N, p->lo, p->hi, 0);
		barrier_wait(p->bar);
		mult_range(p->tmp, p->v, p->N, p->lo, p->hi, 1);
		barrier_wait(p->bar);
		/* u = A^t A v */
		mult_range(p->v, p->tmp, p->N, p->lo, p->hi, 0);
		barrier_wait(p->bar);
		mult_range(p->tmp, p->u, p->N, p->lo, p->hi, 1);
		barrier_wait(p->bar);
	}
	return 0;
}

static double spectral_game(int N) {
	double *u = malloc(N * sizeof(double));
	double *tmp = malloc(N * sizeof(double));
	double *v = malloc(N * sizeof(double));
	for (int i = 0; i < N; i++)
		u[i] = 1.0;

	barrier_t bar;
	thrd_t t[NT];
	struct team_arg args[NT];
	int per = N / NT;
	int nt = 0;
	for (int k = 0; k < NT; k++) {
		int lo = k * per;
		int hi = (k < NT - 1) ? lo + per : N;
		if (lo >= hi)
			break;
		args[nt].u = u;
		args[nt].tmp = tmp;
		args[nt].v = v;
		args[nt].N = N;
		args[nt].lo = lo;
		args[nt].hi = hi;
		nt++;
	}
	barrier_init(&bar, nt);
	for (int k = 0; k < nt; k++)
		args[k].bar = &bar;
	for (int k = 0; k < nt; k++) {
		if (thrd_create(&t[k], team_worker, &args[k]) != thrd_success)
			/* a fixed-party barrier can't absorb a missing thread; bail */
			exit(2);
	}
	for (int k = 0; k < nt; k++)
		thrd_join(t[k], NULL);

	double vBv = 0.0, vv = 0.0;
	for (int i = 0; i < N; i++) {
		vv += v[i] * v[i];
		vBv += u[i] * v[i];
	}
	free(u);
	free(tmp);
	free(v);
	return sqrt(vBv / vv);
}

int main(int argc, char *argv[]) {
	int N = argc >= 2 ? atoi(argv[1]) : 2000;
	if (N <= 0)
		N = 2000;
	printf("%.9f\n", spectral_game(N));
	return 0;
}
