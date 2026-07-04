#include <stdio.h>

int main(void) {

	int neg = -1;
	unsigned u = (unsigned)neg;
	printf("s2u: %u\n", u);

	int big = 0x1234;
	unsigned char uc = (unsigned char)big;
	printf("narrow: %u\n", (unsigned)uc);

	printf("uac: %d\n", (int)((-1 + 1u) == 0u));

	unsigned char a = 1, b = 2;
	printf("promote: %d\n", a - b);

	printf("ftrunc: %d %d\n", (int)3.9, (int)-3.9);

	int n = 1000003;
	printf("rt: %d\n", (int)((double)n) == n);

	short sh = -12345;
	long lo = sh;
	printf("widen: %ld\n", lo);

	printf("bool: %d %d\n", !!42, !!0);
	return 0;
}
