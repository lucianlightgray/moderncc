#include <stdio.h>

int ga = 3, gb = 5, gn = 20;

int main(void)
{
	int i, s = 0;
	int a = ga, b = gb, n = gn;
	for (i = 0; i < n; i++) {
		s += (a * b);
		s += (a * b) + i;
	}
	printf("%d\n", s);
	return 0;
}
