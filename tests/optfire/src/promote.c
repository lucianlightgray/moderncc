#include <stdio.h>

static int churn(int n, int seed)
{
	int a = seed;
	int b = seed + 1;
	int c = seed + 2;
	int d = seed + 3;
	int i;

	for (i = 0; i < n; i++) {
		a = a + b;
		b = b ^ c;
		c = c + d;
		d = d + a;
		a &= 0xffff;
		b &= 0xffff;
		c &= 0xffff;
		d &= 0xffff;
	}
	return (a + b + c + d) & 0xffff;
}

int main(void)
{
	printf("promote=%d\n", churn(9, 4));
	return 0;
}
