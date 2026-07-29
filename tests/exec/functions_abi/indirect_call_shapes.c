#include <stdio.h>

struct ops {
	int (*add)(int, int);
	int (*mul)(int, int);
};

static int do_add(int a, int b) { return a + b; }
static int do_mul(int a, int b) { return a * b; }

static struct ops table[2] = {{do_add, do_mul}, {do_mul, do_add}};

static int via_arrow(struct ops *o, int a, int b) { return o->add(a, b); }
static int via_dot(int i, int a, int b) { return table[i].mul(a, b); }
static int via_cast(void *p, int a, int b)
{
	return ((int (*)(int, int))p)(a, b);
}

int main(void)
{
	int s = 0;
	s += via_arrow(&table[0], 3, 4);
	s += via_dot(0, 3, 4);
	s += via_arrow(&table[1], 5, 6);
	s += via_dot(1, 5, 6);
	s += via_cast((void *)do_add, 7, 8);
	printf("s=%d\n", s);
	return 0;
}
