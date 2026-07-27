#include <stdio.h>

static int dist(int x, int y)
{
	int a = x * 3 + x * 5;
	int b = y * 9 - y * 2;
	int c = x * 6 + x;
	return a + b + c;
}

static long ldist(long v)
{
	long a = v * 7 + v * 11;
	return a;
}

int main(void)
{
	printf("reassoc_muldist=%ld\n", (long)dist(6, 4) + ldist(3L));
	return 0;
}
