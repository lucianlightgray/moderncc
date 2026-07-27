#include "c99inline.h"

/* First TU: take the address of the plain-inline function too. */
int (*c99inline_pa)(int, int, int) = c99inline_add3;

extern int c99inline_use_b(void);
extern void *c99inline_b_addr(void);

/* Self-checking: exit 0 iff -fc99-inline-body's weak out-of-line body linked
   correctly across both TUs. Prints "9 17 35" for a human reading the log.
   Historically the two-TU + address-taken case was untested on PE (docs/TODO). */
extern int printf(const char *, ...);

int main(void) {
	int x = c99inline_pa(2, 3, 4);   /* 9  */
	int y = c99inline_use_b();       /* 17 */
	int z = c99inline_add3(10, 11, 14); /* 35 */
	printf("%d %d %d\n", x, y, z);
	if (x != 9)
		return 1;
	if (y != 17)
		return 2;
	if (z != 35)
		return 3;
	/* Both TUs' &add3 must be the one collapsed weak copy. */
	if ((void *)c99inline_pa != c99inline_b_addr())
		return 4;
	return 0;
}
