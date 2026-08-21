/*
 * fannkuch-redux, fork-join threading.
 *
 * Ported from The Computer Language Benchmarks Game fannkuch-redux problem
 * (vendor/plb/bench/algorithm/fannkuch-redux/, Oleg Mazurov's parallel
 * algorithm as seen in 1.go): for a permutation size n it finds the maximum
 * number of prefix-reversal ("pancake flip") operations over all n!
 * permutations and a signed checksum. The permutation-index space [0, n!) is
 * split into NT disjoint contiguous blocks, one C11 <threads.h> worker each;
 * each worker seeds its first permutation from its block-start index via the
 * factorial number system, walks its block, and returns a partial (maxflips,
 * checksum). The combine is order-independent (max over blocks for maxflips,
 * sum over blocks for checksum), and each permutation's checksum sign is
 * derived from the GLOBAL permutation index parity, so the printed result is
 * byte-identical for any NT (serial -DNT=1 included).
 *
 * Work scale: argv[1] = N (run.sh default 2000). fannkuch cost is n!, which
 * explodes, so N maps to a small n = 8 + N/1000, clamped to [2, 11]; the
 * default N=2000 gives n=10 (runtime a few ms, comparable to spectral-norm).
 *
 * <threads.h> must lead, and libc functions are declared by hand rather than
 * #included: mcc's cooperative backend (-DMCC_THREADS_COOP) defines its own
 * once_flag, which collides with the one glibc's headers pull in. Declaring the
 * few functions used keeps ONE source building under the native, coop, gcc and
 * clang toolchains alike.
 */
#include <threads.h>

extern int atoi(const char *);
extern int printf(const char *, ...);
extern void *memcpy(void *, const void *, __SIZE_TYPE__);

#ifndef NT
#define NT 4
#endif

#define MAXN 11

static long Fact[MAXN + 2];

struct fk_arg {
	int n;
	long idx_min;
	long idx_max;
	int maxflips;
	long long checksum;
};

static int fk_worker(void *arg) {
	struct fk_arg *a = arg;
	int n = a->n;
	long idx_min = a->idx_min, idx_max = a->idx_max;
	int p[MAXN + 1], pp[MAXN + 1], count[MAXN + 1];
	int maxflips = 0;
	long long checksum = 0;

	for (int i = 0; i < n; i++)
		p[i] = i;
	count[0] = 0;
	count[1] = 0;
	long rem = idx_min;
	for (int i = n - 1; i > 0; i--) {
		long d = rem / Fact[i];
		count[i] = (int)d;
		rem = rem % Fact[i];
		memcpy(pp, p, (unsigned)n * sizeof(int));
		for (int j = 0; j <= i; j++) {
			if (j + d <= i)
				p[j] = pp[j + d];
			else
				p[j] = pp[j + d - i - 1];
		}
	}

	long idx = idx_min;
	int sign = ((idx_min & 1) == 0);
	for (;;) {
		int first = p[0];
		if (first != 0) {
			int flips = 1;
			if (p[first] != 0) {
				memcpy(pp, p, (unsigned)n * sizeof(int));
				int p0 = first;
				for (;;) {
					flips++;
					for (int i = 1, j = p0 - 1; i < j; i++, j--) {
						int t = pp[i];
						pp[i] = pp[j];
						pp[j] = t;
					}
					int t = pp[p0];
					pp[p0] = p0;
					p0 = t;
					if (pp[p0] == 0)
						break;
				}
			}
			if (maxflips < flips)
				maxflips = flips;
			if (sign)
				checksum += flips;
			else
				checksum -= flips;
		}

		idx++;
		if (idx == idx_max)
			break;

		if (sign) {
			int t = p[0];
			p[0] = p[1];
			p[1] = t;
		} else {
			int t = p[1];
			p[1] = p[2];
			p[2] = t;
			for (int k = 2;; k++) {
				count[k]++;
				if (count[k] <= k)
					break;
				count[k] = 0;
				for (int j = 0; j <= k; j++)
					p[j] = p[j + 1];
				p[k + 1] = first;
				first = p[0];
			}
		}
		sign = !sign;
	}

	a->maxflips = maxflips;
	a->checksum = checksum;
	return 0;
}

int main(int argc, char **argv) {
	int N = argc > 1 ? atoi(argv[1]) : 2000;
	int n = 8 + N / 1000;
	if (n > MAXN)
		n = MAXN;
	if (n < 2)
		n = 2;

	Fact[0] = 1;
	for (int i = 1; i <= n; i++)
		Fact[i] = Fact[i - 1] * i;
	long total = Fact[n];

	struct fk_arg args[NT];
	thrd_t th[NT];
	int nt = NT < (int)total ? NT : (int)total;
	if (nt < 1)
		nt = 1;
	long per = (total + nt - 1) / nt;
	int made = 0;
	for (int k = 0; k < nt; k++) {
		long lo = (long)k * per;
		long hi = lo + per;
		if (hi > total)
			hi = total;
		if (lo >= hi)
			break;
		args[made].n = n;
		args[made].idx_min = lo;
		args[made].idx_max = hi;
		args[made].maxflips = 0;
		args[made].checksum = 0;
		made++;
	}
	int nj = 0;
	for (int k = 0; k < made; k++) {
		if (thrd_create(&th[nj], fk_worker, &args[k]) == thrd_success)
			nj++;
		else
			fk_worker(&args[k]);
	}
	for (int k = 0; k < nj; k++)
		thrd_join(th[k], 0);

	int maxflips = 0;
	long long checksum = 0;
	for (int k = 0; k < made; k++) {
		if (args[k].maxflips > maxflips)
			maxflips = args[k].maxflips;
		checksum += args[k].checksum;
	}

	printf("%lld\nPfannkuchen(%d) = %d\n", checksum, n, maxflips);
	return 0;
}
