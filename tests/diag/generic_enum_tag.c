/* T-mac-30124: a _Generic association with an elaborated `enum TAG` must parse
 * (the `:` is the association separator, not a C23 fixed-underlying-type
 * introducer). Also verify fixed-underlying-type and enum bit-fields still work. */
#include <stdio.h>
enum E { A, B };
int main(void) {
	int fails = 0;
	if (_Generic((enum E)A, enum E: 2, default: 9) != 2) fails++;   /* was rejected */
	enum F : unsigned char { X = 200 };
	if (sizeof(enum F) != 1 || (unsigned)X != 200) fails++;         /* fixed-underlying */
	enum G : long { Y = -1 };
	if (sizeof(enum G) != sizeof(long) || (long)Y != -1) fails++;  /* signed underlying (sizeof(long): 8 LP64, 4 LLP64) */
	struct S { enum E b : 2; } s = { B };
	if (s.b != B) fails++;                                          /* enum bit-field */
	printf("generic_enum_tag fails=%d\n", fails);
	return fails;
}
