#include <stdio.h>

int gk = 7, gn = 25;

int main(void)
{
	int i, s = 0;
	int k = gk, n = gn;
	for (i = 0; i < n; i++)
		s += i * k;
	printf("%d\n", s);
	return 0;
}
