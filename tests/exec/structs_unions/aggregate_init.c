#include <stdio.h>

struct point {
	int x, y, z;
};

int main(void) {

	int a[5] = {1, 2};
	printf("partial: %d %d %d %d %d\n", a[0], a[1], a[2], a[3], a[4]);

	struct point p = {0};
	printf("zero: %d %d %d\n", p.x, p.y, p.z);

	struct point line[2] = {{1, 2, 3}, {4, 5, 6}};
	printf("nested: %d %d\n", line[0].y, line[1].z);

	char s[6] = "hi";
	printf("str: [%s] len-region=%d c2=%d\n", s, (int)sizeof s, s[2]);

	int d[8] = {[2] = 20, [5] = 50, [0] = 0};
	printf("desig: %d %d %d %d\n", d[0], d[2], d[5], d[7]);

	struct point m = {.z = 9, .x = 1};
	printf("memdesig: %d %d %d\n", m.x, m.y, m.z);

	int o[3] = {[0] = 1, [1] = 2, [0] = 7};
	printf("override: %d %d\n", o[0], o[1]);
	return 0;
}
