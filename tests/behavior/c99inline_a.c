#include "c99inline.h"


int (*c99inline_pa)(int, int, int) = c99inline_add3;

extern int c99inline_use_b(void);
extern void *c99inline_b_addr(void);




extern int printf(const char *, ...);

int main(void) {
	int x = c99inline_pa(2, 3, 4);
	int y = c99inline_use_b();
	int z = c99inline_add3(10, 11, 14);
	printf("%d %d %d\n", x, y, z);
	if (x != 9)
		return 1;
	if (y != 17)
		return 2;
	if (z != 35)
		return 3;

	if ((void *)c99inline_pa != c99inline_b_addr())
		return 4;
	return 0;
}
