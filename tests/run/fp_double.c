#include <stdint.h>
#include <stdio.h>

extern double aux_dsum(double a, double b, double c, double d, double e,
											 double f, double g, double h, double i, double j);
extern double aux_dmix(int32_t a, double b, int32_t c, double d);
extern float aux_fadd(float a, float b);
extern double aux_f2d(float a);
extern float aux_d2f(double a);
extern double aux_dspill(double a, double b, double c, double d, double e,
												 double f, double g, double h, int32_t i1,
												 int32_t i2, int32_t i3, int32_t i4, int32_t i5,
												 int32_t i6, int32_t i7, int32_t i8, double i,
												 double j);

static double poly(double x) { return ((3.0 * x + 2.0) * x + 1.0) * x + 0.5; }

int main(void) {
	printf("poly=%.6f\n", poly(2.0));
	printf("sum=%.6f\n", aux_dsum(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
	printf("mix=%.6f\n", aux_dmix(3, 1.5, 4, 2.25));
	printf("fadd=%.6f\n", (double)aux_fadd(1.25f, 2.5f));
	printf("f2d=%.6f\n", aux_f2d(0.75f));
	printf("d2f=%.6f\n", (double)aux_d2f(9.5));
	printf("div=%.6f\n", aux_dsum(1, 1, 1, 1, 1, 1, 1, 1, 1, 1) / 4.0);
	printf("neg=%.6f\n", -aux_dmix(1, 0.25, 2, 0.5));
	printf("spill=%.6f\n",
				 aux_dspill(0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 1, 2, 3, 4, 5, 6, 7,
										8, 8.5, 9.5));
	{
		double acc = 0.0;
		int i;
		for (i = 0; i < 8; i++)
			acc = acc * 2.0 + 0.5;
		printf("loop=%.6f\n", acc);
	}
	return 0;
}
