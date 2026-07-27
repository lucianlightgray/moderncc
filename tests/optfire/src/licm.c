#include <stdio.h>

int ga = 6, gb = 7, gn = 15;

int main(void)
{
	int i = 0, s = 0;
	int a = ga, b = gb, n = gn;
	while (i < n) {
		s += (a + b);
		s ^= (a + b);
		s -= (a + b);
		i++;
	}
	printf("%d\n", s);
	return 0;
}
