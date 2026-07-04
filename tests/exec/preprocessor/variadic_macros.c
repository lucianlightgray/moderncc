#include <stdio.h>

#define LOG(fmt, ...) printf(fmt, __VA_ARGS__)
#define LOG0(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define STR(...) #__VA_ARGS__
#define FIRST(a, ...) (a)

#define COUNT(...) COUNT_(__VA_ARGS__, 5, 4, 3, 2, 1, 0)
#define COUNT_(a, b, c, d, e, n, ...) n

#define SUM(...) sum_impl(COUNT(__VA_ARGS__), __VA_ARGS__)

static int sum_impl(int n, ...) {
	__builtin_va_list ap;
	__builtin_va_start(ap, n);
	int s = 0;
	for (int i = 0; i < n; i++)
		s += __builtin_va_arg(ap, int);
	__builtin_va_end(ap);
	return s;
}

int main(void) {
	LOG("one=%d two=%d\n", 1, 2);
	LOG0("none\n");
	LOG0("two=%d,%d\n", 10, 20);

	printf("stringized: [%s]\n", STR(a, b, c));
	printf("first: %d\n", FIRST(7, 8, 9));

	printf("count: %d %d %d\n", COUNT(x), COUNT(x, y), COUNT(x, y, z, w));
	printf("sum: %d\n", SUM(1, 2, 3, 4, 5));
	return 0;
}
