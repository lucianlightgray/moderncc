#include <stdio.h>

static int grid[64][64];

static void bump(void)
{
	int i, j;

	for (j = 0; j < 64; j++)
		for (i = 0; i < 64; i++)
			grid[i][j] = grid[i][j] + i + j;
}

static long total(void)
{
	int i, j;
	long t = 0;

	for (i = 0; i < 64; i++) {
		for (j = 0; j < 64; j++)
			t += grid[i][j];
	}
	return t;
}

int main(void)
{
	bump();
	bump();
	printf("interchange=%ld\n", total());
	return 0;
}
