#include <stdio.h>

static int widen_add(long a, int c)
{
	int s = 0;
	return (int)((((long)c) << s) + a);
}

static int widen_mul(long a, int c)
{
	int s = 0;
	return (int)((((long)c) >> s) * a);
}

static int widen_sub(long a, int c)
{
	int s = 0;
	return (int)(a - (((long)c) << s));
}

int main(void)
{
	printf("ident_shift=%d\n", widen_add(100L, 7) + widen_mul(3L, 9) +
														 widen_sub(50L, 6));
	return 0;
}
