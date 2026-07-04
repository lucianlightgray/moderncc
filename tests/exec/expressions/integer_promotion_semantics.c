#include <stdio.h>

int main(void) {

	unsigned char a = 1, b = 5;
	printf("diff: %d\n", a - b);

	signed char c = 100, d = 100;
	printf("add: %d\n", c + d);

	unsigned char z = 0;
	printf("compl: %d\n", ~z);

	unsigned char one = 1;
	printf("shift: %d\n", one << 9);

	short s = 300;
	printf("mul: %d\n", s * s);

	printf("rank: %d\n", (int)(sizeof(a + b) == sizeof(int)));

	char ch = 'A';
	printf("char: %d\n", ch + 1);
	return 0;
}
