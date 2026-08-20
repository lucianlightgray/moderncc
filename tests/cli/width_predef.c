#include <stdio.h>
int main(void) {
	int ok = 1;
	ok &= (__SCHAR_WIDTH__ == 8);
	ok &= (__SHRT_WIDTH__ == 16);
	ok &= (__INT_WIDTH__ == 32);
	ok &= (__LONG_LONG_WIDTH__ == 64);
	ok &= (__INTMAX_WIDTH__ == 64);
	ok &= (__SIG_ATOMIC_WIDTH__ == 32);
	ok &= (__LONG_WIDTH__ == __SIZEOF_LONG__ * 8);
	ok &= (__INTPTR_WIDTH__ == __SIZEOF_POINTER__ * 8);
	ok &= (__PTRDIFF_WIDTH__ == __SIZEOF_PTRDIFF_T__ * 8);
	ok &= (__SIZE_WIDTH__ == __SIZEOF_SIZE_T__ * 8);
	ok &= (__WCHAR_WIDTH__ == __SIZEOF_WCHAR_T__ * 8);
	ok &= (__INT_LEAST8_WIDTH__ == 8 && __INT_LEAST16_WIDTH__ == 16 &&
	       __INT_LEAST32_WIDTH__ == 32 && __INT_LEAST64_WIDTH__ == 64);
	ok &= (__INT_FAST8_WIDTH__ == 8 && __INT_FAST16_WIDTH__ == 32 &&
	       __INT_FAST32_WIDTH__ == 32 && __INT_FAST64_WIDTH__ == 64);
	ok &= (__INTPTR_MAX__ > 0 && __UINTPTR_MAX__ > 0);
	ok &= (__FLT_NORM_MAX__ == __FLT_MAX__);
	ok &= (__DBL_NORM_MAX__ == __DBL_MAX__);
	ok &= (__LDBL_NORM_MAX__ == __LDBL_MAX__);
	printf("%s\n", ok ? "WIDTH_OK" : "WIDTH_FAIL");
	return ok ? 0 : 1;
}
