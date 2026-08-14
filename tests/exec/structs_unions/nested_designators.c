#include <stdio.h>

struct in3 { int x, y, z; };
struct in2 { struct in3 p; int q; };
struct top { struct in2 a; int arr[3]; struct in3 b[2]; };

int main(void) {
	struct top t = {
		.a.p.y = 20, .a.p.x = 10, .a.p.z = 30,
		.a.q = 40,
		.arr[2] = 3, .arr[0] = 1,
		.b[1].y = 99,
	};
	struct top u = { .a.p.x = 1, 2, 3 };
	printf("t: %d %d %d %d\n", t.a.p.x, t.a.p.y, t.a.p.z, t.a.q);
	printf("arr: %d %d %d\n", t.arr[0], t.arr[1], t.arr[2]);
	printf("b1y: %d\n", t.b[1].y);
	printf("u: %d %d %d\n", u.a.p.x, u.a.p.y, u.a.p.z);
	return 0;
}
