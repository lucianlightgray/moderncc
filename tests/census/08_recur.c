static int fib(int n) {
	if (n < 2) return n;
	return fib(n - 1) + fib(n - 2);
}
static int fact(int n) { return n <= 1 ? 1 : n * fact(n - 1); }
int compute(int n) {
	int a = fib(n);
	int b = fact(n);
	return a + b;
}
