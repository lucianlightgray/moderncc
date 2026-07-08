/* Smoke program compiled by the sanitizer-instrumented mcc (mcc_s). It stays
 * header-free (just an extern printf) so the test does not depend on the build
 * tree's include layout; the point is to drive a full parse/codegen/link pass
 * through the instrumented compiler and check the emitted program runs. */
extern int printf(const char *, ...);

static int fib(int n) {
	int a = 0, b = 1;
	for (int i = 0; i < n; i++) {
		int t = a + b;
		a = b;
		b = t;
	}
	return a;
}

struct point {
	int x, y;
};

static long dot(struct point p, struct point q) {
	return (long)p.x * q.x + (long)p.y * q.y;
}

int main(void) {
	struct point p = {3, 4}, q = {5, 6};
	printf("fib=%d dot=%ld\n", fib(10), dot(p, q));
	return 0;
}
