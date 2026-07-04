#include <stdio.h>

static int side_effect_count;

static int eval(int v) {
	side_effect_count++;
	return v;
}

int main(void) {
	int v = eval(5) ?: eval(99);
	printf("%d %d\n", v, side_effect_count);

	side_effect_count = 0;
	int z = eval(0) ?: eval(7);
	printf("%d %d\n", z, side_effect_count);

	int n = 0;
	int w = (n++, 5) ?: 99;
	printf("%d %d\n", w, n);
	return 0;
}
