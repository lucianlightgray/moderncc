/* Regression: in a char32_t string a UTF-16 surrogate value can only come from
 * a hex escape (a UCN surrogate is rejected; a literal astral character decodes
 * to a single code point), and gcc-16/clang keep each hex escape as an
 * independent char32_t unit. mcc ran a surrogate-pair combining loop on the
 * U"" path, so `U"\xD800\xDC00"` fused into the single code point 0x10000
 * (n=2) instead of the two units 0xD800 0xDC00 (n=3). The fix removes the
 * surrogate-combine from the char32_t lowering (T-mac-30244). Exit 0 only when
 * every case matches the gcc-16 oracle. */

#include <uchar.h>

int main(void) {
	/* adjacent hex-escape surrogates stay two independent char32_t units */
	char32_t a[] = U"\xD800\xDC00";
	if (sizeof(a) / sizeof(a[0]) != 3) return 1;
	if (a[0] != 0xD800u || a[1] != 0xDC00u || a[2] != 0) return 2;

	/* a genuine astral UCN is a single char32_t code point (never a pair) */
	char32_t b[] = U"\U0001F600";
	if (sizeof(b) / sizeof(b[0]) != 2) return 3;
	if (b[0] != 0x1F600u || b[1] != 0) return 4;

	/* a lone hex-escape surrogate is kept verbatim */
	char32_t c[] = U"\xD800";
	if (sizeof(c) / sizeof(c[0]) != 2) return 5;
	if (c[0] != 0xD800u || c[1] != 0) return 6;

	return 0;
}
