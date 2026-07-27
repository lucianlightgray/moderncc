#include <stdio.h>

static int src[24][50];
static int dst[24][50];

static void seed(void)
{
	int i, j;

	for (i = 0; i < 24; i++) {
		for (j = 0; j < 50; j++)
			src[i][j] = (i * 50 + j) & 15;
	}
}

static void scale(void)
{
	int i, j;

	for (i = 0; i < 24; i++)
		for (j = 0; j < 50; j++)
			dst[i][j] = src[i][j] * 3;
}

static long total(void)
{
	int i, j;
	long t = 0;

	for (i = 0; i < 24; i++) {
		for (j = 0; j < 50; j++)
			t += dst[i][j];
	}
	return t;
}

int main(void)
{
	seed();
	scale();
	printf("tile=%ld\n", total());
	return 0;
}
