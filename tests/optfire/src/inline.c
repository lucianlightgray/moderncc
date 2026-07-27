#include <stdio.h>

int gx = 9;

static int twice(int v)
{
	return v + v;
}

int main(void)
{
	int x = gx;
	int r = twice(x) + twice(x + 1);
	printf("%d\n", r);
	return 0;
}
