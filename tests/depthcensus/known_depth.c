#include <stdio.h>

static int fact(int n) { return n <= 1 ? 1 : n * fact(n - 1); }

static int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

static int chain(int n) { return n == 0 ? 0 : 1 + chain(n - 1); }

int main(void) {
	int i, s = 0;
	for (i = 0; i < 8; i++)
		s += fact(i);
	for (i = 0; i < 10; i++)
		s += fib(i);
	s += chain(12);
	printf("%d\n", s);
	return 0;
}
