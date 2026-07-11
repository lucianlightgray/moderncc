#include <stdio.h>
#include <math.h>
#include <float.h>

static double div_ret(double a, double b) {
	return a / b;
}

static double mul_ret(double a, double b) {
	return a * b;
}

static float fmul_ret(float a, float b) {
	return a * b;
}

static double madd_ret(double a, double b, double c) {
	return a * b + c;
}

int main(void) {
	int ok = 1;
	int m = FLT_EVAL_METHOD;

	ok &= (m >= -1 && m <= 2);
	ok &= (sizeof(float_t) >= sizeof(float));
	ok &= (sizeof(double_t) >= sizeof(double));

	if (m == 0) {
		ok &= (sizeof(float_t) == sizeof(float));
		ok &= (sizeof(double_t) == sizeof(double));
	} else if (m == 1) {
		ok &= (sizeof(float_t) == sizeof(double));
		ok &= (sizeof(double_t) == sizeof(double));
	} else if (m == 2) {
		ok &= (sizeof(float_t) == sizeof(long double));
		ok &= (sizeof(double_t) == sizeof(long double));
	}

	ok &= (FLT_RADIX == 2);
	ok &= (DBL_MANT_DIG >= FLT_MANT_DIG);
	ok &= (LDBL_MANT_DIG >= DBL_MANT_DIG);
	ok &= (DECIMAL_DIG >= DBL_DIG);

	if (m == 0) {
	{
		volatile double a = 1.0, b = 3.0;
		ok &= (div_ret(a, b) == (double)(a / b));
	}
	{
		volatile double a = 7.0, b = 49.0;
		ok &= (div_ret(a, b) == (double)(a / b));
	}
	{
		volatile double a = 1.4142135623730951, b = 1.4142135623730951;
		ok &= (mul_ret(a, b) == (double)(a * b));
	}
	{
		volatile double a = 1.3, b = 1.7;
		volatile double ref = a * b;
		ok &= (mul_ret(a, b) == ref);
	}

	{
		volatile float fa = 1.1f, fb = 1.3f;
		volatile float f = fa * fb;
		double d = (double)f;
		ok &= (d == (double)f);
		ok &= (fmul_ret(fa, fb) == f);
	}
	{
		volatile double x = 1.0 / 3.0;
		float f = (float)x;
		double d = (float)x;
		ok &= (d == (double)f);
		ok &= (d == (double)(float)x);
	}
	{
		volatile long double le = 1.0L / 3.0L;
		double d = (double)le;
		ok &= (d == (double)(double)le);
		ok &= (d == d);
	}

	{
		volatile double a = 1.3, b = 1.7, c = 0.9;
		volatile double p = a * b;
		volatile double r1 = p + c;
		volatile double p2 = a * b;
		volatile double r2 = p2 + c;
		ok &= (r1 == r2);
		ok &= (r1 == (double)((double)(a * b) + c));
		double contracted = madd_ret(a, b, c);
#ifdef _WIN32
		ok &= (contracted == r1 || m == 2);
#else
		ok &= (fma(p, 1.0, c) == r1);
		ok &= (contracted == r1 || contracted == fma(a, b, c) || m == 2);
#endif
	}
	}

	{
		volatile double z = 0.0, one = 1.0, big = DBL_MAX;
		ok &= isnan(z / z);
		ok &= isinf(one / z);
		ok &= (one / z > 0.0);
		ok &= isinf(big + big);
		ok &= (z == -z);
		ok &= !(z / z == z / z);
	}

#ifdef __STDC_IEC_559__
	ok &= (FLT_RADIX == 2);
	ok &= (FLT_MANT_DIG == 24);
	ok &= (DBL_MANT_DIG == 53);
	ok &= (FLT_MAX_EXP == 128);
	ok &= (DBL_MAX_EXP == 1024);
	{
		volatile double z = 0.0, one = 1.0;
		ok &= isinf(one / z);
		ok &= isnan(z / z);
		ok &= (isinf(INFINITY) && INFINITY > 0.0);
		ok &= isnan(NAN);
	}
#endif

	printf(ok ? "OK\n" : "FAIL\n");
	return !ok;
}
