#include <stdio.h>

static int rels(int x, long v)
{
	int a = (x == x);
	int b = (x != x);
	int c = (x < x);
	int d = (x >= x);
	int e = (v <= v);
	int f = (v > v);
	return a + b + c + d + e + f;
}

int main(void)
{
	printf("ident_rel=%d\n", rels(3, 4L));
	return 0;
}
