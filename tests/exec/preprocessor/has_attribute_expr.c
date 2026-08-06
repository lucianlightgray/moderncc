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
	/* the builtin answer must survive the stale mccdefs.h stub */
	int h = __has_feature(c_atomic);
	int i = __has_extension(c_atomic);
	/* a user #define of a builtin macro wins, as in gcc and clang */
#define __has_attribute(x) 42
	int j = __has_attribute(unused);
#undef __has_attribute
#define __has_feature(x) 43
	int k = __has_feature(c_atomic);
#undef __has_feature
	/* and after a plain #undef the name is no longer a macro at all */
#undef __has_extension
#ifdef __has_extension
	int l = 1;
#else
	int l = 0;
#endif
	if (a == 1 && b == 1 && c == 1 && d == 1 && e == 1 && f == 0 && g == 1 &&
			h == 1 && i == 1 && j == 42 && k == 43 && l == 0)
		printf("OK\n");
	return 0;
}
