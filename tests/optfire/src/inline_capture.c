#include <stdio.h>

static int scale(int v, int k)
{
	return v * k + 1;
}

static int blend(int x, int y)
{
	return scale(x, 3) ^ scale(y, 5);
}

static int driver(int seed)
{
	int r = seed;
	int i;

	for (i = 0; i < 5; i++)
		r = blend(r, i) & 0xffff;
	return r;
}

int main(void)
{
	printf("inline_capture=%d\n", driver(11));
	return 0;
}
