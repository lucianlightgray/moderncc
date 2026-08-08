#include <stdio.h>

static int fib(int n) {
	if (n < 2)
		return n;
	return fib(n - 1) + fib(n - 2);
}

static int is_even(int n);
static int is_odd(int n);

static int is_even(int n) { return n == 0 ? 1 : is_odd(n - 1); }
static int is_odd(int n) { return n == 0 ? 0 : is_even(n - 1); }

static int ack(int m, int n) {
	if (m == 0)
		return n + 1;
	if (n == 0)
		return ack(m - 1, 1);
	return ack(m - 1, ack(m, n - 1));
}

static int treesum(int depth, int seed) {
	if (depth == 0)
		return (seed & 7) + 1;
	return (treesum(depth - 1, seed * 2 + 1) * 3 +
					treesum(depth - 1, seed * 3 + 2) + depth) &
				 1023;
}

int main(void) {
	int i;

	for (i = 0; i <= 12; i++)
		printf("fib %d\n", fib(i));

	for (i = 0; i < 4; i++)
		printf("parity %d %d\n", is_even(i), is_odd(i));

	printf("ack %d\n", ack(2, 3));
	printf("treesum %d\n", treesum(6, 1));

	return 0;
}
