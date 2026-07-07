/* FLT_EVAL_METHOD, float_t/double_t (C99/C11 §5.2.4.2.2p8-9, §7.12p2). The
 * evaluation-method macro must be defined and in {-1,0,1,2}, and float_t/
 * double_t must have the widths the reported method mandates. Mirrors gcc
 * c11-float-*.c and clang C11/n1365.c (eval-method). Prints OK. */
#include <stdio.h>
#include <math.h>
#include <float.h>

int main(void) {
	int ok = 1;
	int m = FLT_EVAL_METHOD;

	ok &= (m >= -1 && m <= 2);
	ok &= (sizeof(float_t) >= sizeof(float));
	ok &= (sizeof(double_t) >= sizeof(double));

	/* §7.12p2: 0 -> {float,double}; 1 -> {double,double};
	   2 -> {long double,long double}. Only assert the exact widths for the
	   well-defined methods (m == -1 is indeterminate). */
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

	/* neighbouring <float.h> characteristics must be sane */
	ok &= (FLT_RADIX == 2);
	ok &= (DBL_MANT_DIG >= FLT_MANT_DIG);
	ok &= (LDBL_MANT_DIG >= DBL_MANT_DIG);
	ok &= (DECIMAL_DIG >= DBL_DIG);

	printf(ok ? "OK\n" : "FAIL\n");
	return !ok;
}
