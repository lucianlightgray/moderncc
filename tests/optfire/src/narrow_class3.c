#include <stdio.h>

static int mixed(int a, long c)
{
	return (int)((long)a * c);
}

static int mixed2(int a, long c)
{
	return (int)((long)a & c);
}

int main(void)
{
	printf("narrow_class3=%d\n", mixed(6, 7L) + mixed2(12, 10L));
	return 0;
}
