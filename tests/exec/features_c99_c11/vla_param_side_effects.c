#include <stdio.h>

static int calls;

static int side(int v) {
	calls++;
	return v;
}

static int outer_effect(int i, int a[i++]) {
	(void)a;
	return i;
}

static int nested(int rows, int cols, int g[side(rows)][side(cols)]) {
	int t = 0;
	for (int r = 0; r < rows; r++)
		for (int c = 0; c < cols; c++)
			t += g[r][c];
	return t;
}

int main(void) {
	if (outer_effect(10, 0) != 11) {
		puts("FAIL outer");
		return 1;
	}

	calls = 0;
	int buf[3][4];
	for (int r = 0; r < 3; r++)
		for (int c = 0; c < 4; c++)
			buf[r][c] = r * 10 + c;

	int sum = nested(3, 4, buf);
	if (sum != 138) {
		printf("FAIL nested sum=%d\n", sum);
		return 1;
	}
	if (calls != 2) {
		printf("FAIL calls=%d\n", calls);
		return 1;
	}

	puts("OK");
	return 0;
}
