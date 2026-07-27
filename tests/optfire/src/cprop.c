#include <stdio.h>

int gx = 4;

int main(void)
{
	int k = 12;
	int m = k + 5;
	int x = gx;
	int r = x * k + m;
	printf("%d\n", r);
	return 0;
}
