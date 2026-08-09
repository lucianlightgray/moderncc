static int fact(int n) { return n <= 1 ? 1 : n * fact(n - 1); }

static int chain(int n) { return n == 0 ? 0 : 1 + chain(n - 1); }

static int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

int depth_rec_main(int a, int b) {
	int s = 0;
	int i;
	for (i = 0; i < 12; i++)
		s += fact(i) + chain(i) + fib(i % 10);
	return s + a + b;
}
