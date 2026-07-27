extern int printf(const char *, ...);

double sqrt(double);

static double roots(void)
{
	return sqrt(4.0) + sqrt(2.25) + sqrt(9.0) + sqrt(0.25);
}

int main(void)
{
	printf("bfold_sqrt=%.4f\n", roots());
	return 0;
}
