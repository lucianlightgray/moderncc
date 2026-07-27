#include <stdio.h>

double fmin(double, double);
double fmax(double, double);

static double bounds(void)
{
	return fmin(2.5, 5.25) + fmax(2.5, 5.25) + fmin(-1.5, 3.0) + fmax(-1.5, 3.0);
}

int main(void)
{
	printf("bfold_minmax=%.4f\n", bounds());
	return 0;
}
