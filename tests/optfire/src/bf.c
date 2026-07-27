#include <stdio.h>

int gx = 4;

int main(void)
{
	int x = gx, r = 0;
	if (x != 1 && x != 3 && x != 5 && x != 7 && x != 9 && x != 11)
		r = 100;
	printf("%d\n", r);
	return 0;
}
