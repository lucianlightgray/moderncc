#include <stdio.h>

int main(void) {
	volatile long double near_one = 1.0L - 0x1p-64L;
	volatile long double half_up = 2.5L;
	volatile long double half_dn = -2.5L;
	volatile long double huge_p = 1e300L;
	volatile long double huge_n = -1e300L;
	volatile long double not_a_num = 0.0L / 0.0L;

	printf("%d\n", (int)near_one);
	printf("%d\n", (int)(short)near_one);
	printf("%d\n", (int)(signed char)near_one);
	printf("%u\n", (unsigned)near_one);
	printf("%lld\n", (long long)near_one);
	printf("%d %d\n", (int)half_up, (int)half_dn);
	printf("%d\n", (int)huge_p);
	printf("%d\n", (int)huge_n);
	printf("%d\n", (int)not_a_num);
	return 0;
}
