#include <stdio.h>
int main(void) {
	int ok = 1;
#ifdef __FLT128_MANT_DIG__
	ok = (__FLT128_MANT_DIG__ == 113 && __FLT128_DIG__ == 33 &&
	      __FLT128_MIN_EXP__ == -16381 && __FLT128_MAX_EXP__ == 16384 &&
	      __FLT128_MIN_10_EXP__ == -4931 && __FLT128_MAX_10_EXP__ == 4932 &&
	      __FLT128_DECIMAL_DIG__ == 36 && __FLT128_HAS_DENORM__ == 1 &&
	      __FLT128_HAS_INFINITY__ == 1 && __FLT128_HAS_QUIET_NAN__ == 1 &&
	      __FLT128_IS_IEC_60559__ == 1 && __FLT128_MAX__ > (__float128)0 &&
	      __FLT128_EPSILON__ > (__float128)0 && __FLT128_NORM_MAX__ > (__float128)0);
#endif
	printf("%s\n", ok ? "FLT128_OK" : "FLT128_FAIL");
	return ok ? 0 : 1;
}
