#include <stdio.h>

struct pt {
	int x;
	int y;
};

static int bump(struct pt *p, int k)
{
	p->x += k;
	p->y ^= k;
	p->x &= 0xffff;
	return p->x + p->y;
}

static int churn(struct pt *p, int k)
{
	int i;
	int t = 0;

	for (i = 0; i < 5; i++) {
		p->x += i;
		p->y |= i;
		t += p->x - p->y;
	}
	return t + k;
}

static int slots(int *a, int k)
{
	a[0] += k;
	a[1] -= k;
	return a[0] + a[1];
}

int main(void)
{
	struct pt p;
	int a[2];

	p.x = 10; p.y = 6;
	a[0] = 1; a[1] = 2;
	printf("opassign=%d\n", bump(&p, 3) + churn(&p, 4) + slots(a, 5));
	return 0;
}
