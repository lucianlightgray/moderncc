extern int printf(const char *, ...);

static double fl(double *v, int n, double a, double b)
{
	double s = 0;
	for (int i = 0; i < n; i++)
		s += v[i] * (a * b) + (a * b);
	return s;
}

static int il(int *v, int n, int a, int b)
{
	int s = 0;
	for (int i = 0; i < n; i++)
		s += v[i] * (a * b) + (a * b);
	return s;
}

static long double dl(long double *v, int n, long double a, long double b)
{
	long double s = 0;
	for (int i = 0; i < n; i++)
		s += v[i] * (a * b) + (a * b);
	return s;
}

static long long ql(long long *v, int n, long long a, long long b)
{
	long long s = 0;
	for (int i = 0; i < n; i++)
		s += v[i] * (a * b) + (a * b);
	return s;
}

int main(void)
{
	double v[3] = {1, 2, 3};
	int w[3] = {1, 2, 3};
	long double x[3] = {1, 2, 3};
	long long y[3] = {1, 2, 3};
	printf("%.1f %d %.1Lf %lld\n", fl(v, 3, 2, 3), il(w, 3, 2, 3),
				 dl(x, 3, 2, 3), ql(y, 3, 2, 3));
	return 0;
}
