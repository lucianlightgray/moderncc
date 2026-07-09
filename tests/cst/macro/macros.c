#define SQUARE(x) ((x) * (x))
#define LIMIT 100
#define ADD(a, b) ((a) + (b))

int compute(int n) {
	int a = SQUARE(n);
	int cap = LIMIT;
	int b = ADD(a, cap);
	return a + b;
}
