#include <stdio.h>

static int withlit(int a)
{
	return (int)((long)a + 1000L);
}

static int withlit2(int a)
{
	int r;
	r = (int)((long)a ^ 255L);
	return r;
}

int main(void)
{
	printf("narrow_class2=%d\n", withlit(7) + withlit2(9));
	return 0;
}
