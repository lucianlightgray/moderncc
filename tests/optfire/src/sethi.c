#include <stdio.h>

int ga = 2, gb = 3, gc = 4, gd = 5, ge = 6;

int main(void)
{
	int a = ga, b = gb, c = gc, d = gd, e = ge;
	int r = a + (b * c + d * e) + (a * b) + c;
	printf("%d\n", r);
	return 0;
}
