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
	ok &= (stdc_leading_zeros_uc(0x01) == 7);
	ok &= (stdc_leading_zeros_us(0x0001) == 15);
	ok &= (stdc_leading_zeros_ui(1u) == 31);
	ok &= (stdc_leading_zeros_ull(1ull) == 63);
	ok &= (stdc_leading_zeros_ul(1ul) == (unsigned)(sizeof(unsigned long) * 8 - 1));
	ok &= (stdc_leading_ones_uc(0xFEu) == 7);
	ok &= (stdc_leading_ones_us(0xFFFEu) == 15);
	ok &= (stdc_trailing_zeros_ui(0x8u) == 3);
	ok &= (stdc_trailing_zeros_ull(8ull) == 3);
	ok &= (stdc_trailing_ones_ui(0x7u) == 3);
	ok &= (stdc_first_leading_one_uc(0x80u) == 1);
	ok &= (stdc_first_leading_one_ui(0x80000000u) == 1);
	ok &= (stdc_first_leading_zero_uc(0x7Fu) == 1);
	ok &= (stdc_first_trailing_one_ui(0x8u) == 4);
	ok &= (stdc_first_trailing_zero_ui(0x7u) == 4);
	ok &= (stdc_count_ones_uc(0xFFu) == 8);
	ok &= (stdc_count_ones_ull(0xFull) == 4);
	ok &= (stdc_count_zeros_uc(0x0Fu) == 4);
	ok &= (stdc_count_zeros_ui(0xFu) == 28);
	ok &= (stdc_has_single_bit_ui(64u) == 1);
	ok &= (stdc_has_single_bit_ull(3ull) == 0);
	ok &= (stdc_bit_width_ui(255u) == 8);
	ok &= (stdc_bit_floor_ui(100u) == 64u);
	ok &= (stdc_bit_floor_uc(100u) == 64u);
	ok &= (stdc_bit_ceil_ull(100ull) == 128ull);
	ok &= (stdc_bit_ceil_ui(100u) == 128u);
	printf("%s\n", ok ? "STDBIT_OK" : "STDBIT_FAIL");
	return ok ? 0 : 1;
}
