#include <stdio.h>

static int shl3(int x)
{
	long long a = x & 0xffff;
	int r = (int)(a << 3);

	return r;
}

static int shl5(int x)
{
	long long a = x & 0x7ff;
	int r = (int)(a << 5);

	return r;
}

static unsigned ushl2(unsigned x)
{
	unsigned long long a = x & 0xfffu;
	unsigned r = (unsigned)(a << 2);

	return r;
}

int main(void)
{
	int i;
	long sum = 0;

	for (i = 0; i < 16; i++)
		sum += shl3(i * 13) + shl5(i * 7) + (int)ushl2((unsigned)i * 29u);
	printf("vlat=%ld\n", sum);
	return 0;
}
