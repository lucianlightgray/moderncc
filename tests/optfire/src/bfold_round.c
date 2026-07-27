#include <stdio.h>

double floor(double);
double ceil(double);
double trunc(double);

static double rounds(void)
{
	return floor(2.75) + ceil(2.25) + trunc(-2.75) + floor(-3.5) + ceil(-1.5);
}

int main(void)
{
	printf("bfold_round=%.4f\n", rounds());
	return 0;
}
