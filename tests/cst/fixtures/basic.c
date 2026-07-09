int global_counter = 0;

struct Point {
	int x, y;
};

static int add(int a, int b) {
	return a + b;
}

int compute(int n) {
	int acc = 0;
	for (int i = 0; i < n; ++i) {
		acc += add(i, i * 2) - (i % 3 ? 1 : 0);
	}
	return acc;
}

int main(void) {
	struct Point p = {.x = 1, .y = 2};
	return compute(p.x + p.y);
}
