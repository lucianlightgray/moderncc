/* `!(a > a)` is code-free: the parser runs the comparison and swaps the flags,
   so the arena kept the un-negated operator behind AST_FB_CMP_INVERT_LATE and
   ast_ident_node folded `a > a` to 0. The store was then dropped as dead.
   Both directions matter -- `!(a == a)` executed a store that must not run. */
#include <stdio.h>

static unsigned long arr[8] = {1, 2, 3, 4, 5, 6, 7, 8};

static unsigned long taken(unsigned long a) {
	if (!(a > a))
		arr[5] = a;
	return arr[5];
}

static unsigned long not_taken(unsigned long a) {
	if (!(a == a))
		arr[6] = a;
	return arr[6];
}

static unsigned long float_cmp(double d) {
	if (!(d > d))
		arr[7] = 100;
	return arr[7];
}

int main(void) {
	printf("%lu %lu %lu\n", taken(7), not_taken(9), float_cmp(1.5));
	return 0;
}
