#include <math.h>
#include <stdio.h>

static volatile double a = 2.5, b = 7.5, c = 3.25, one = 1.0;
static volatile float fa = 2.5f, fb = 7.5f, fc = 3.25f;
static volatile long double la = 2.5L;

static volatile double bi, li;
static volatile float bif, lif;
static volatile long double bil, lil;

#define CHK1(fn)             \
	bi = __builtin_##fn(a);     \
	li = fn(a);                 \
	if (bi != li)               \
		ok = 0;
#define CHK2(fn)             \
	bi = __builtin_##fn(a, b);  \
	li = fn(a, b);              \
	if (bi != li)               \
		ok = 0;
#define CHK1F(fn)            \
	bif = __builtin_##fn##f(fa);\
	lif = fn##f(fa);            \
	if (bif != lif)             \
		ok = 0;
#define CHK2F(fn)                \
	bif = __builtin_##fn##f(fa, fb);\
	lif = fn##f(fa, fb);            \
	if (bif != lif)                 \
		ok = 0;

int main(void) {
	int ok = 1;

	CHK1(sqrt)
	CHK1(floor)
	CHK1(ceil)
	CHK1(trunc)
	CHK1(round)
	CHK1(rint)
	/* msvcrt.dll exports only C89 math; the C99 additions nearbyint/erf/erfc/
	   fma/remainder live in libmingwex (which mcc does not link) and are absent
	   from mcc's msvcrt.def, so a call to them fails to link, or loads a symbol
	   msvcrt.dll lacks, on Windows. Skip them there; every non-PE target still
	   exercises the whole family. */
#ifndef _WIN32
	CHK1(nearbyint)
#endif
	CHK1(exp)
	CHK1(exp2)
	CHK1(log)
	CHK1(log2)
	CHK1(log10)
	CHK1(sin)
	CHK1(cos)
	CHK1(tan)
	CHK1(atan)
	CHK1(cbrt)
	CHK1(sinh)
	CHK1(cosh)
	CHK1(tanh)
	CHK1(expm1)
	CHK1(log1p)
	CHK1(logb)
#ifndef _WIN32
	CHK1(erf)
	CHK1(erfc)
#endif
	CHK1(tgamma)

	CHK2(fmin)
	CHK2(fmax)
	CHK2(pow)
	CHK2(fmod)
	CHK2(atan2)
	CHK2(hypot)
	CHK2(fdim)
#ifndef _WIN32
	CHK2(remainder)
#endif
	CHK2(nextafter)

	CHK1F(sqrt)
	CHK1F(floor)
	CHK1F(exp)
	CHK1F(log)
#ifndef _WIN32
	CHK1F(erf)
#endif
	CHK1F(expm1)
	CHK2F(fmin)
	CHK2F(pow)
	CHK2F(hypot)

#ifndef _WIN32
	bi = __builtin_fma(a, b, c);
	li = fma(a, b, c);
	if (bi != li)
		ok = 0;
	bif = __builtin_fmaf(fa, fb, fc);
	lif = fmaf(fa, fb, fc);
	if (bif != lif)
		ok = 0;
#endif
	bil = __builtin_sqrtl(la);
	lil = sqrtl(la);
	if (bil != lil)
		ok = 0;
	bil = __builtin_floorl(la);
	lil = floorl(la);
	if (bil != lil)
		ok = 0;

	if (__builtin_sqrt(4.0) != 2.0)
		ok = 0;
	if (__builtin_floor(2.75) != 2.0)
		ok = 0;
	if (__builtin_fmin(one, a) != 1.0)
		ok = 0;
#ifndef _WIN32
	if (__builtin_fma(2.0, 3.0, 4.0) != 10.0)
		ok = 0;
#endif

	printf("%g %g %g %g\n", __builtin_sqrt(a), __builtin_floor(b),
				 __builtin_fmax(a, b), __builtin_pow(a, 2.0));
	printf(ok ? "OK\n" : "FAIL\n");
	return 0;
}
