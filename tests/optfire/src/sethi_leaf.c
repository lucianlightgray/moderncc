#include <stdio.h>

static int litleaf(int a, int b)
{
	return a + (b + 1);
}

static int litleaf2(int a, int b, int c)
{
	return a + (b + 7) + (c * 3);
}

int main(void)
{
	printf("sethi_leaf=%d\n", litleaf(4, 9) + litleaf2(2, 3, 5));
	return 0;
}
