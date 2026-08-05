#include <stdint.h>

double aux_dsum(double a, double b, double c, double d, double e, double f,
								double g, double h, double i, double j) {
	return a + b + c + d + e + f + g + h + i + j;
}

double aux_dmix(int32_t a, double b, int32_t c, double d) {
	return (double)a + b + (double)c + d;
}

float aux_fadd(float a, float b) { return a + b; }

double aux_f2d(float a) { return (double)a * 2.0; }

float aux_d2f(double a) { return (float)(a / 2.0); }

double aux_dspill(double a, double b, double c, double d, double e, double f,
									double g, double h, int32_t i1, int32_t i2, int32_t i3,
									int32_t i4, int32_t i5, int32_t i6, int32_t i7, int32_t i8,
									double i, double j) {
	return a + b + c + d + e + f + g + h + i + j +
				 (double)(i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8);
}
