extern int printf(const char *, ...);

double fabs(double);
double copysign(double, double);

static double signs(void)
{
	return fabs(-6.25) + fabs(1.5) + copysign(2.5, -1.0) + copysign(4.0, 3.0);
}

int main(void)
{
	printf("bfold_sign=%.4f\n", signs());
	return 0;
}
