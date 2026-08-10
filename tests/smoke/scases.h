#ifndef MCC_SMOKE_SCASES_H
#define MCC_SMOKE_SCASES_H

#include <stdio.h>

#include "smoke.h"

static long long sms_ga = 6, sms_gb = 7, sms_gn = 20;

#define SMS_SHAPES(X) \
	X(BFOLD, sms_bfold) \
	X(IDENT, sms_ident) \
	X(NARROW, sms_narrow) \
	X(CPROP, sms_cprop) \
	X(CSE, sms_cse) \
	X(LTEMP, sms_ltemp) \
	X(IVSR, sms_ivsr) \
	X(PRE, sms_pre) \
	X(LICM, sms_licm) \
	X(DSE, sms_dse) \
	X(SCCP, sms_sccp) \
	X(JT, sms_jt) \
	X(BF, sms_bf) \
	X(RANGE, sms_range) \
	X(DIVMAGIC, sms_divmagic) \
	X(ABS, sms_abs) \
	X(SELECT, sms_select) \
	X(REASSOC, sms_reassoc) \
	X(SETHI, sms_sethi) \
	X(TCO, sms_tco) \
	X(INLINE, sms_inline) \
	X(CLOAD, sms_cload) \
	X(TERNRET, sms_ternret) \
	X(TERNRETX, sms_ternretx)

static long long sms_bfold(void)
{
	long long v = sms_ga;
	long long k = (3 * 4 + 5) * (6 - 2) / 2 - (1 << 3);
	double g = (double)(sms_gb & 1023);
	double c = __builtin_sqrt(16.0) + __builtin_fabs(-6.25) +
						 __builtin_sqrt(2.25);
	return v * k + ((7 * 9) % 5) + (long long)(c * 4.0) +
				 (long long)__builtin_fabs(-g);
}

static long long sms_ident(void)
{
	long long a = sms_ga, b = sms_gb;
	return (a + 0) * 1 + (b | 0) + (a ^ 0) - (b - 0) + (a & -1);
}

static long long sms_narrow(void)
{
	long long v = sms_ga;
	int n = (int)(short)(v & 0xffff);
	unsigned char c = (unsigned char)(v >> 8);
	return (long long)n * 3 + (long long)c + (sms_gb & 1);
}

static long long sms_cprop(void)
{
	int k = 12;
	int m = k + 5;
	int x = (int)sms_ga;
	int r = x * k + m;
	return (long long)r + sms_gb;
}

static long long sms_cse(void)
{
	long long a = sms_ga, b = sms_gb;
	long long p = (a * b + 3);
	long long q = (a * b + 3);
	return p + q + (a * b + 3);
}

static long long sms_ltemp(void)
{
	long long i, s = 0;
	long long a = sms_ga, b = sms_gb, n = sms_gn & 31;
	for (i = 0; i < n; i++) {
		s += (a * b);
		s += (a * b) + i;
	}
	return s;
}

static long long sms_ivsr(void)
{
	long long i, s = 0, n = sms_gn & 31;
	for (i = 0; i < n; i++)
		s += i * 12;
	return s + sms_ga;
}

static long long sms_pre(void)
{
	int a = (int)sms_ga, b = (int)sms_gb, x = (int)sms_gn;
	int t;
	if (x)
		t = a * b;
	else
		t = 1;
	t = t + a * b;
	return (long long)t;
}

static long long sms_licm(void)
{
	long long i, s = 0, n = sms_gn & 31;
	long long a = sms_ga, b = sms_gb;
	for (i = 0; i < n; i++)
		s += a * b + 17;
	return s;
}

static long long sms_dse(void)
{
	long long x;
	x = 5;
	x = 11;
	x = sms_ga;
	return x + sms_gb;
}

static long long sms_sccp(void)
{
	int flag = 0;
	int r = (int)sms_ga;
	if (flag)
		r = r * 100;
	else
		r = r + 1;
	if (!flag)
		r += 7;
	return (long long)r + sms_gb;
}

static long long sms_jt(void)
{
	long long x = sms_ga, y = sms_gb, r;
	if (x > 3)
		r = y + 1;
	else
		r = y + 1;
	return r;
}

static long long sms_bf(void)
{
	long long x = sms_ga, r = 0;
	if (x != 1 && x != 3 && x != 5 && x != 7 && x != 9 && x != 11)
		r = 100;
	return r + sms_gb;
}

static long long sms_range(void)
{
	long long v = sms_ga, r = 0;
	if (v > 10 && v < 5)
		r += 1000;
	if (v >= 0 && v <= 0)
		r += 7;
	if (v > 3)
		r += 2;
	return r + sms_gb;
}

static long long sms_divmagic(void)
{
	long long v = sms_ga;
	return v / 7 + v % 3 + sms_gb / 5;
}

static long long sms_abs(void)
{
	long long v = sms_ga;
	int w = (int)sms_gb;
	long long x = v < 0 ? -v : v;
	int y = w < 0 ? -w : w;
	return x + y;
}

static long long sms_select(void)
{
	long long a = sms_ga, b = sms_gb;
	long long m = a > b ? a : b;
	long long n = a < b ? a : b;
	return m * 2 + n;
}

static long long sms_reassoc(void)
{
	int x = (int)sms_ga;
	int r = x * 3 + x * 5;
	return (long long)r + sms_gb;
}

static long long sms_sethi(void)
{
	long long a = sms_ga, b = sms_gb;
	return (a * 3 + b * 5) * (a * 7 + b * 11) - (a + b) * (a - b);
}

static long long sms_tco_go(long long n, long long acc)
{
	if (n <= 0)
		return acc;
	return sms_tco_go(n - 1, acc + n);
}

static long long sms_tco(void)
{
	return sms_tco_go(sms_gn & 31, sms_ga);
}

static long long sms_inline_leaf(long long a, long long b)
{
	return a * 3 + b * 5 + 1;
}

static long long sms_inline(void)
{
	long long a = sms_ga, b = sms_gb;
	return sms_inline_leaf(a, b) + sms_inline_leaf(b, a);
}

static long long sms_cload(void)
{
	static const long long tab[8] = {1, 2, 3, 5, 8, 13, 21, 34};
	long long v = sms_ga & 7;
	return tab[(int)v] * 2 + tab[(int)((v + 1) & 7)] + sms_gb;
}

static long long sms_ternret_h(int c, unsigned uu, int ii)
{
	if (c)
		return uu;
	return ii;
}

static long long sms_ternret_t(int c, unsigned uu, int ii)
{
	return c ? uu : ii;
}

static unsigned long long sms_ternret_u(int c, int ii, unsigned uu)
{
	if (c)
		return ii;
	return uu;
}

static long long sms_ternret(void)
{
	int c = (int)(sms_gn & 1);
	unsigned uu = (unsigned)sms_ga;
	int ii = (int)sms_gb;
	return sms_ternret_h(c, uu, ii) * 3 + sms_ternret_t(c, uu, ii);
}

static long long sms_ternretx(void)
{
	int c = (int)(sms_gn & 1);
	int ii = (int)sms_ga;
	unsigned uu = (unsigned)sms_gb;
	unsigned long long r = sms_ternret_u(c, ii, uu);
	short sh = (short)sms_ga;
	long long w = c ? (long long)sh : (long long)uu;
	return (long long)(r >> 1) + w;
}

enum {
#define SMS_TAG(tag, fn) SMS_S_##tag,
	SMS_SHAPES(SMS_TAG)
#undef SMS_TAG
			SMS_S_COUNT
};

typedef long long (*SmsFn)(void);

static const SmsFn sms_fns[] = {
#define SMS_FN(tag, fn) fn,
		SMS_SHAPES(SMS_FN)
#undef SMS_FN
};

static const char *const sms_shape_name[] = {
#define SMS_NM(tag, fn) #tag,
		SMS_SHAPES(SMS_NM)
#undef SMS_NM
};

static const long long sms_corpus[] = {
		0, 1, 2, 3, 4, 5, 7, 9, 11, -1, -3, 8, 15, 16, 31, 63, 64, 127, -128,
		255, 256, 1023, 4096, -4097, 65535, 65536, -65536, 1000000007ll,
		-1000000007ll, LLONG_MIN / 4, LLONG_MAX / 4};

#define SMS_CORPUS_N ((int)(sizeof sms_corpus / sizeof sms_corpus[0]))

static long long sms_eval(int s, long long a, long long b, long long n)
{
	sms_ga = a;
	sms_gb = b;
	sms_gn = n;
	return sms_fns[s]();
}

static long sms_sweep(SmBits *digest)
{
	int s, i, j;
	long n = 0;
	for (s = 0; s < SMS_S_COUNT; s++)
		for (i = 0; i < SMS_CORPUS_N; i++)
			for (j = 0; j < SMS_CORPUS_N; j++) {
				SmBits v = (SmBits)sms_eval(s, sms_corpus[i], sms_corpus[j],
																		sms_corpus[(i + j) % SMS_CORPUS_N]);
				*digest =
						((*digest ^ (SmBits)s) * 1099511628211ull ^ v) * 1099511628211ull;
				n++;
			}
	return n;
}

#endif
