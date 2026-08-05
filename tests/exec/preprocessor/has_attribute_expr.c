#include <stdio.h>

#define A unused
#define B A
#define F(X) __has_attribute(X)

int main(void) {
	int a = __has_attribute(unused);
	int b = __has_attribute(B);
	int c = F(unused);
	int d = F(B);
	int e = __has_attribute(
		unused
	);
	int f = __has_attribute(no_such_attribute_at_all);
#ifdef __has_attribute
	int g = 1;
#else
	int g = 0;
#endif
	if (a == 1 && b == 1 && c == 1 && d == 1 && e == 1 && f == 0 && g == 1)
		printf("OK\n");
	return 0;
}
