#include <stdio.h>

static int cascade(int x)
{
	int a = 1;
	int b, c;

	if (a)
		b = 2;
	else
		b = x;
	if (b == 2)
		c = 5;
	else
		c = x;
	if (c == 5)
		return 30;
	return 40;
}

int main(void)
{
	printf("sccp_fix=%d\n", cascade(9));
	return 0;
}
