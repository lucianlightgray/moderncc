#include <stdio.h>

static int spin(int v)
{
	int i, s = 0;
	for (i = 0; i < 3; i++)
		s += v + i;
	return s;
}

static int overwritten(int x)
{
	int t, u;
	t = x + 1;
	u = x + 3;
	spin(x);
	t = x + 2;
	u = x + 4;
	return t + u;
}

int main(void)
{
	printf("dse_call=%d\n", overwritten(5) + spin(2));
	return 0;
}
