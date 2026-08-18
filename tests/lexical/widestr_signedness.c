/* T-mac-30147: a 4-byte wide-string literal must match the array element in
 * signedness — char32_t (U"") is unsigned int, so a signed int[] is a
 * different character type and must be rejected (gcc/clang both error). */
int a[] = U"a";
