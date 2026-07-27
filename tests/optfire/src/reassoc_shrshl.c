#include <stdio.h>

static int aligndown(int x)
{
	int a = (x >> 4) << 4;
	int b = (x >> 2) << 2;
	return a + b;
}

static long lalign(long v)
{
	long a = (v >> 8) << 8;
	return a;
}

int main(void)
{
	printf("reassoc_shrshl=%ld\n", (long)aligndown(1000) + lalign(123456L));
	return 0;
}
