#include <stdio.h>

struct point {
	int x, y;
};
struct rect {
	struct point lo, hi;
};

union bits {
	unsigned u;
	unsigned char b[4];
};

int main(void) {
	struct point a = {3, 4};
	struct point *pa = &a;

	printf("dot: %d %d\n", a.x, a.y);
	pa->x = 30;
	printf("arrow: %d %d\n", pa->x, a.y);

	struct point c = a;
	c.x = 99;
	printf("copy: a=%d c=%d\n", a.x, c.x);

	struct rect r = {{0, 0}, {10, 20}};
	r.hi.x += 5;
	printf("nested: %d %d %d %d\n", r.lo.x, r.lo.y, r.hi.x, r.hi.y);

	struct point arg = {1, 2};
	struct point ret = (struct point){arg.y, arg.x};
	printf("swap: %d %d\n", ret.x, ret.y);

	union bits w;
	w.u = 0;
	w.b[0] = 1;
	w.b[1] = 2;
	w.b[2] = 3;
	w.b[3] = 4;
	printf("union: sum=%d\n", w.b[0] + w.b[1] + w.b[2] + w.b[3]);
	return 0;
}
