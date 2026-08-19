/* T-mac-30078/30230: exercises the thin C23 <stdbit.h> (runtime/include). */
#include <stdbit.h>
extern int printf(const char *, ...);
int main(void) {
	int ok = 1;
	ok &= (stdc_count_ones(0x00FF00FFu) == 16);
	ok &= (stdc_leading_zeros((unsigned char)0x01) == 7);
	ok &= (stdc_leading_ones((unsigned char)0xFEu) == 7);
	ok &= (stdc_trailing_zeros(0x8u) == 3);
	ok &= (stdc_first_trailing_one(0x8u) == 4);
	ok &= (stdc_count_zeros((unsigned char)0x0Fu) == 4);
	ok &= (stdc_bit_width(0xFFu) == 8);
	ok &= (stdc_bit_floor(100u) == 64);
	ok &= (stdc_bit_ceil(100u) == 128);
	ok &= (stdc_has_single_bit(64u) == 1);
	ok &= (__STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__);
	ok &= (__STDC_VERSION_STDBIT_H__ == 202311L);
	printf("%s\n", ok ? "STDBIT_OK" : "STDBIT_FAIL");
	return ok ? 0 : 1;
}
