/*
 * edigits — first D decimal digits of e = sum_{k>=0} 1/k!, serial bignum.
 *
 * The reference entries under vendor/plb/bench/algorithm/edigits/ (1.py, 1.go)
 * compute e by binary-splitting the series into an exact rational p/q and then
 * doing one big/big division. That needs a full arbitrary-precision divide,
 * which is not "small". This port uses the equivalent scaled-sum formulation
 * that needs only a big/small divide: with S scaled by 10^P,
 *     e * 10^P ~= sum_{k=0}^{m} floor(10^P / k!),
 * and the running term t_k = 10^P / k! is obtained from t_{k-1} by dividing by
 * the single integer k (t_0 = 10^P), so the only bignum ops required are add
 * (S += t) and divide-by-small-int (t /= k). The loop stops when t reaches 0.
 * P = D + GUARD carries GUARD guard digits; the truncation error is bounded by
 * the number of terms (a few hundred at these scales), far inside the guard, so
 * the leading D digits equal e's true digits.
 *
 * Bignum design: base-10^9 limbs in a little-endian array (limb 0 least
 * significant), unsigned int per limb, unsigned long long for the running
 * carry/remainder. Just three operations — set to 10^P, in-place /= small int,
 * and += big — none of which need general multiplication or division.
 *
 * This computation is inherently serial, so there is no threading; the kernel
 * still includes <threads.h> and honours -DNT so it builds and runs identically
 * under every toolchain (output is NT-invariant by construction).
 *
 * Work scale: D = N * EDIGITS_SCALE digits (N = argv[1], default 2000).
 * EDIGITS_SCALE is tuned so N=2000 runs in a few tens of ms. Output matches the
 * CLBG format: the digits in tab-terminated groups of ten with a running count.
 */
#include <threads.h>

extern void *malloc(__SIZE_TYPE__);
extern void free(void *);
extern int atoi(const char *);
extern int printf(const char *, ...);

#ifndef NT
#define NT 4
#endif

#ifndef EDIGITS_SCALE
#define EDIGITS_SCALE 8
#endif

#ifndef EDIGITS_GUARD
#define EDIGITS_GUARD 50
#endif

#define BASE 1000000000u

struct big {
	unsigned *d;
	long n;
	long cap;
};

static struct big big_alloc(long cap) {
	struct big b;
	b.d = malloc((__SIZE_TYPE__)cap * sizeof(unsigned));
	b.n = 1;
	b.cap = cap;
	b.d[0] = 0;
	return b;
}

static void big_set_pow10(struct big *b, long p) {
	long limb = p / 9;
	long rem = p % 9;
	for (long i = 0; i <= limb; i++)
		b->d[i] = 0;
	unsigned v = 1;
	for (long i = 0; i < rem; i++)
		v *= 10u;
	b->d[limb] = v;
	b->n = limb + 1;
}

static void big_copy(struct big *dst, const struct big *src) {
	dst->n = src->n;
	for (long i = 0; i < src->n; i++)
		dst->d[i] = src->d[i];
}

static int big_is_zero(const struct big *b) {
	return b->n == 1 && b->d[0] == 0;
}

static void big_divi(struct big *b, unsigned divisor) {
	unsigned long long rem = 0;
	for (long i = b->n - 1; i >= 0; i--) {
		unsigned long long cur = rem * (unsigned long long)BASE + b->d[i];
		b->d[i] = (unsigned)(cur / divisor);
		rem = cur % divisor;
	}
	while (b->n > 1 && b->d[b->n - 1] == 0)
		b->n--;
}

static void big_add(struct big *a, const struct big *b) {
	long n = a->n > b->n ? a->n : b->n;
	unsigned long long carry = 0;
	for (long i = 0; i < n; i++) {
		unsigned long long s = carry;
		if (i < a->n)
			s += a->d[i];
		if (i < b->n)
			s += b->d[i];
		a->d[i] = (unsigned)(s % BASE);
		carry = s / BASE;
	}
	a->n = n;
	if (carry) {
		a->d[n] = (unsigned)carry;
		a->n = n + 1;
	}
}

int main(int argc, char **argv) {
	int n = argc > 1 ? atoi(argv[1]) : 2000;
	if (n < 1)
		n = 1;
	long d = (long)n * EDIGITS_SCALE;
	long p = d + EDIGITS_GUARD;
	long cap = p / 9 + 4;

	struct big s = big_alloc(cap);
	struct big t = big_alloc(cap);
	big_set_pow10(&t, p);
	big_copy(&s, &t);

	unsigned k = 1;
	for (;;) {
		big_divi(&t, k);
		if (big_is_zero(&t))
			break;
		big_add(&s, &t);
		k++;
	}

	long total = (s.n - 1) * 9;
	unsigned top = s.d[s.n - 1];
	char tb[12];
	int tl = 0;
	if (top == 0)
		tb[tl++] = '0';
	else
		while (top) {
			tb[tl++] = (char)('0' + top % 10);
			top /= 10;
		}
	total += tl;

	char *ds = malloc((__SIZE_TYPE__)total + 1);
	long pos = 0;
	for (int i = tl - 1; i >= 0; i--)
		ds[pos++] = tb[i];
	for (long i = s.n - 2; i >= 0; i--) {
		unsigned v = s.d[i];
		for (int j = 8; j >= 0; j--) {
			ds[pos + j] = (char)('0' + v % 10);
			v /= 10;
		}
		pos += 9;
	}

	char chunk[16];
	for (long i = 0; i < d; i += 10) {
		long rem = d - i < 10 ? d - i : 10;
		long j;
		for (j = 0; j < rem; j++)
			chunk[j] = ds[i + j];
		if (i + 10 <= d) {
			chunk[rem] = 0;
			printf("%s\t:%ld\n", chunk, i + 10);
		} else {
			for (; j < 10; j++)
				chunk[j] = ' ';
			chunk[10] = 0;
			printf("%s\t:%ld\n", chunk, d);
		}
	}

	free(ds);
	free(s.d);
	free(t.d);
	return 0;
}
