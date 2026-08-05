#include <stdint.h>

long double aux_ldsum(long double a, long double b, long double c, long double d,
											long double e, long double f, long double g,
											long double h) {
	return a + b + c + d + e + f + g + h;
}

long double aux_ldmix(int32_t a, long double b, double c, long double d) {
	return (long double)a + b + (long double)c + d;
}

double aux_ld_to_d(long double a) { return (double)a; }

long double aux_d_to_ld(double a) { return (long double)a + 0.25L; }

long double aux_ldscale(long double a, int32_t n) {
	long double r = a;
	int32_t i;
	for (i = 0; i < n; i++)
		r = r * 2.0L;
	return r;
}
