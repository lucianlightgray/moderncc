#include <stdio.h>

int gx = 11;

int main(void)
{
	int x = gx;
	int a = x + 0;
	int b = x * 1;
	int c = x | 0;
	int d = x & -1;
	int e = x ^ 0;
	int f = x - 0;
	printf("%d\n", a + b + c + d + e + f);
	return 0;
}
