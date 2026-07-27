#include <stdio.h>

static int src[40];
static int dst[40];

static void gen(void)
{
	int i, j;

	for (i = 0; i < 40; i++)
		src[i] = i * 3;
	for (j = 0; j < 40; j++)
		dst[j] = src[j] + 1;
}

static long total(void)
{
	int i;
	long t = 0;

	for (i = 0; i < 40; i++)
		t += src[i] + dst[i];
	return t;
}

int main(void)
{
	gen();
	printf("fusion=%ld\n", total());
	return 0;
}
