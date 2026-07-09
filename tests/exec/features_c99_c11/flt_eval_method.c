#include <stdio.h>
#include <math.h>
#include <float.h>

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

	printf(ok ? "OK\n" : "FAIL\n");
	return !ok;
}
