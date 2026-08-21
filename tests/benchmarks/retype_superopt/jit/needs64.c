#include <stdio.h>
static int mix(int x, int y) {
	long a = x, b = y + 1;
	a = a * 2654435761L + b;
	a ^= a >> 7;
	return (int)(a % 100000);
}
int main(void) {
	long s = 0;
	for (int i = 0; i < 200000; i++)
		s += mix(i, i * 3 + 1);
	printf("%ld\n", s);
	return 0;
}
