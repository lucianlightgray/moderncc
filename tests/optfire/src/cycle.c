#include <stdio.h>

static int cascade(int n)
{
	int r = 0;
	int i, j;

	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			int t = 6 * 7 + 3;
			int u = t * 2 - 5;
			int v = u - t + 1;

			r += t + u + v + i * j;
			r ^= (t ^ u) + j;
			r &= 0xffff;
		}
	}
	return r;
}

int main(void)
{
	printf("cycle=%d\n", cascade(5));
	return 0;
}
