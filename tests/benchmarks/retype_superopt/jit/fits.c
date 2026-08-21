#include <stdio.h>
static int step(int x, int y) {
	long a = (x & 0x3fff), b = (y & 0x3fff);
	a = a * 3 + b;
	a = (a ^ (a >> 2)) & 0x0fffffff;
	return (int)a;
}
int main(void) {
	long s = 0;
	for (int i = 0; i < 200000; i++)
		s += step(i, i * 7 + 2);
	printf("%ld\n", s);
	return 0;
}
