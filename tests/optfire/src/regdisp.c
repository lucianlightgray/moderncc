#include <stdio.h>

struct rec {
	int a;
	int b;
	int c;
	int pad[5];
	int d;
};

static struct rec tbl[6];

static int sweep(int n)
{
	int i;
	int t = 0;

	for (i = 0; i < n; i++) {
		tbl[i].a = i + 1;
		tbl[i].b = i * 2;
		tbl[i].c = i ^ 3;
		tbl[i].d = i + 9;
	}
	for (i = 0; i < n; i++) {
		t += tbl[i].a + tbl[i].b;
		t ^= tbl[i].c + tbl[i].d;
	}
	return t & 0xffff;
}

int main(void)
{
	printf("regdisp=%d\n", sweep(6));
	return 0;
}
