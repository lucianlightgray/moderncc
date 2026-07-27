#include <stdio.h>

int ga = 3, gb = 9, gc = 41, gd = 17;

int main(void)
{
	int a = ga, b = gb, c = gc, d = gd;
	int r = a < b ? c : d;
	printf("%d\n", r);
	return 0;
}
