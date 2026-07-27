#include <stdio.h>

int ga = 6, gb = 7;

int main(void)
{
	int a = ga, b = gb;
	int p = (a * b + 3);
	int q = (a * b + 3);
	int r = p + q + (a * b + 3);
	printf("%d\n", r);
	return 0;
}
