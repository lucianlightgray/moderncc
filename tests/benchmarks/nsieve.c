/*
 * nsieve — prime-count sieve of Eratosthenes.
 *
 * Ported from The Computer Language Benchmarks Game entry
 * vendor/plb/bench/algorithm/nsieve/1.c (C entry by bearophile). The original
 * main() runs three independent sieves of sizes 10000<<(m-i); those three
 * passes are data-independent, so this port runs them on NT C11 <threads.h>
 * workers (fork-join) instead of a serial loop, then prints the results in the
 * original order. Serial (-DNT=1) output is byte-identical to the parallel run.
 *
 * Work scale: sieve sizes are N*1000, N*500, N*250 (N = argv[1], default via
 * run.sh = 2000 → 2.0M / 1.0M / 0.5M), chosen so runtime at the suite's default
 * N is comparable to the spectral-norm kernels.
 */

/*
 * <threads.h> must lead, and libc functions are declared by hand rather than
 * #included: mcc's cooperative backend (-DMCC_THREADS_COOP) defines its own
 * once_flag, which collides with the one glibc's <stdlib.h> pulls in. Declaring
 * the few functions we use keeps ONE source building under the native, coop,
 * gcc and clang toolchains alike.
 */
#include <threads.h>

extern void *malloc(__SIZE_TYPE__);
extern void *memset(void *, int, __SIZE_TYPE__);
extern void free(void *);
extern int atoi(const char *);
extern int printf(const char *, ...);

#ifndef NT
#define NT 4
#endif

struct sieve_arg {
	unsigned m;
	unsigned count;
};

static int sieve_worker(void *arg) {
	struct sieve_arg *a = arg;
	unsigned m = a->m, count = 0, i, j;
	unsigned char *flags = malloc(m);
	memset(flags, 1, m);
	for (i = 2; i < m; ++i)
		if (flags[i]) {
			++count;
			for (j = i << 1; j < m; j += i)
				flags[j] = 0;
		}
	free(flags);
	a->count = count;
	return 0;
}

int main(int argc, char **argv) {
	unsigned n = argc > 1 ? (unsigned)atoi(argv[1]) : 2000;
	enum { NJOB = 3 };
	struct sieve_arg jobs[NJOB];
	jobs[0].m = n * 1000u;
	jobs[1].m = n * 500u;
	jobs[2].m = n * 250u;

	thrd_t th[NJOB];
	int t = 0;
	while (t < NJOB) {
		int batch = NT < NJOB - t ? NT : NJOB - t;
		if (batch < 1)
			batch = 1;
		for (int k = 0; k < batch; k++)
			thrd_create(&th[t + k], sieve_worker, &jobs[t + k]);
		for (int k = 0; k < batch; k++)
			thrd_join(th[t + k], 0);
		t += batch;
	}

	for (int i = 0; i < NJOB; i++)
		printf("Primes up to %8u %8u\n", jobs[i].m, jobs[i].count);
	return 0;
}
