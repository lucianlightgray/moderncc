#include <stdio.h>

static int roundtrip(int x, int y)
{
	int a = (int)(long)x;
	int b = (int)(long)(x + y);
	int c = (int)(int)y;
	return a + b + c;
}

static int chain(int x)
{
	long w = (long)x;
	int a = (int)w;
	int b = (int)(long)(int)(long)x;
	return a + b;
}

int main(void)
{
	printf("ident_conv=%d\n", roundtrip(3, 4) + chain(5));
	return 0;
}
