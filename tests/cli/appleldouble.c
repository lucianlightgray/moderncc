#include <stdio.h>

_Static_assert(sizeof(long double) == sizeof(double),
			   "Apple arm64 ABI: long double is double");
_Static_assert(sizeof(long double) == 8,
			   "Apple arm64 ABI: long double is 8 bytes");

int main(void) {
	long double a = 1.0L / 3.0L;
	double b = 1.0 / 3.0;
	printf("%d\n", a == (long double)b);
	return 0;
}
