#include <stdio.h>

int gx = 6;

int main(void)
{
	int x = gx;
	int r = x * 3 + x * 5;
	printf("%d\n", r);
	return 0;
}
