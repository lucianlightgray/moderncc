#include <stdio.h>

int gn = 1234567;

int main(void)
{
	int n = gn;
	int q = n / 7;
	int r = n % 7;
	unsigned u = (unsigned)n;
	unsigned uq = u / 11u;
	printf("%d %d %u\n", q, r, uq);
	return 0;
}
