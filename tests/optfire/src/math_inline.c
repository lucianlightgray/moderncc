extern int printf(const char *, ...);
#pragma comment(lib, "m")

extern double fabs(double);
extern double sqrt(double);

static double crunch(double x, int n)
{
	double acc = 0.0;
	int i;

	for (i = 0; i < n; i++) {
		double v = x - (double)i;

		acc += fabs(v);
		acc += sqrt(fabs(v) + 1.0);
	}
	return acc;
}

int main(void)
{
	printf("math_inline=%ld\n", (long)(crunch(3.5, 8) * 1000.0));
	return 0;
}
