#include <stdint.h>
#include <stdio.h>

extern long double aux_ldsum(long double a, long double b, long double c,
														 long double d, long double e, long double f,
														 long double g, long double h);
extern long double aux_ldmix(int32_t a, long double b, double c, long double d);
extern double aux_ld_to_d(long double a);
extern long double aux_d_to_ld(double a);
extern long double aux_ldscale(long double a, int32_t n);

int main(void) {
	long double a = aux_ldsum(1, 2, 3, 4, 5, 6, 7, 8);
	printf("sum=%.6f\n", (double)a);
	printf("mix=%.6f\n", (double)aux_ldmix(2, 0.5L, 1.25, 0.25L));
	printf("conv=%.6f\n", aux_ld_to_d(3.5L));
	printf("back=%.6f\n", (double)aux_d_to_ld(2.5));
	printf("scale=%.6f\n", (double)aux_ldscale(0.75L, 5));
	{
		long double q = 1.0L;
		int i;
		for (i = 0; i < 4; i++)
			q = q * 2.0L + 0.5L;
		printf("loop=%.6f\n", (double)q);
	}
	printf("cmp=%d\n", aux_ldsum(1, 1, 1, 1, 1, 1, 1, 1) == 8.0L);
	return 0;
}
