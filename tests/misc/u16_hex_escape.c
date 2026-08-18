/* Regression: a hex (or octal) escape is a RAW code unit, not a scalar value to
 * be UTF-16 surrogate-encoded. In a char16_t string `u"\x10000"` gcc-16 masks
 * the escape to the element width (16 bits -> 0x0000) and warns "hex escape
 * sequence out of range"; only a genuine UCN or literal character >= 0x10000
 * surrogate-splits. mcc stored hex escapes and UCNs indistinguishably in the
 * nwchar_t buffer, so the u16 lowering surrogate-split `\x10000` into the pair
 * D800 DC00 (n=3), matching neither gcc (n=2, 0 0) nor clang (hard error). The
 * fix masks non-UCN hex escapes to the u"" element width at the escape site so
 * they never reach the surrogate-split (T-mac-30243). char32_t (U"") keeps the
 * full 32-bit value; UCNs still surrogate-split in u"". Exit 0 only when every
 * case matches the gcc-16 oracle. */

#include <uchar.h>

int main(void) {
	/* over-range hex escape in u"" -> masked to 0x0000, no surrogate split */
	char16_t a[] = u"\x10000";
	if (sizeof(a) / sizeof(a[0]) != 2) return 1;
	if (a[0] != 0x0000 || a[1] != 0) return 2;

	/* another over-range hex escape: 0x1F600 & 0xFFFF == 0xF600 */
	char16_t b[] = u"\x1F600";
	if (sizeof(b) / sizeof(b[0]) != 2) return 3;
	if (b[0] != 0xF600 || b[1] != 0) return 4;

	/* in-range hex escape is unchanged */
	char16_t c[] = u"\xABCD";
	if (sizeof(c) / sizeof(c[0]) != 2) return 5;
	if (c[0] != 0xABCD || c[1] != 0) return 6;

	/* a genuine UCN >= 0x10000 STILL surrogate-splits in u"" */
	char16_t d[] = u"\U00010000";
	if (sizeof(d) / sizeof(d[0]) != 3) return 7;
	if (d[0] != 0xD800 || d[1] != 0xDC00 || d[2] != 0) return 8;

	/* char32_t keeps the full value: no masking, no surrogate split */
	char32_t e[] = U"\x10000";
	if (sizeof(e) / sizeof(e[0]) != 2) return 9;
	if (e[0] != 0x10000u || e[1] != 0) return 10;

	return 0;
}
