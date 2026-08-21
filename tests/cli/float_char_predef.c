#include <stdio.h>
int main(void) {
	int ok = 1;
	ok = ok && __FLT_DECIMAL_DIG__ == 9 && __FLT_IS_IEC_60559__ == 1;
	ok = ok && __DBL_DECIMAL_DIG__ == 17 && __DBL_IS_IEC_60559__ == 1;
	ok = ok && __LDBL_IS_IEC_60559__ == 1;
#if __LDBL_MANT_DIG__ == 64
	ok = ok && __LDBL_DECIMAL_DIG__ == 21;
#elif __LDBL_MANT_DIG__ == 113
	ok = ok && __LDBL_DECIMAL_DIG__ == 36;
#else
	ok = ok && __LDBL_DECIMAL_DIG__ == 17;
#endif
	printf("%s\n", ok ? "FLOAT_CHAR_OK" : "FLOAT_CHAR_FAIL");
	return ok ? 0 : 1;
}
