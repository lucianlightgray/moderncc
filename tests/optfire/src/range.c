#include <stdio.h>

int gx = 14;

int main(void)
{
	int x = gx, r = 0;
	if (x >= 10 && x <= 20)
		r = 1;
	printf("%d\n", r);
	return 0;
}
