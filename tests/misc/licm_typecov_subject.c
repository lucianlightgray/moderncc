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

static long long nl(__int128 *v, int n, __int128 a, __int128 b)
{
	__int128 s = 0;
	for (int i = 0; i < n; i++)
		s += v[i] * (a * b) + (a * b);
	return (long long)s;
}

int main(void)
{
	double v[3] = {1, 2, 3};
	int w[3] = {1, 2, 3};
	long double x[3] = {1, 2, 3};
	__int128 y[3] = {1, 2, 3};
	printf("%.1f %d %.1Lf %lld\n", fl(v, 3, 2, 3), il(w, 3, 2, 3),
				 dl(x, 3, 2, 3), nl(y, 3, 2, 3));
	return 0;
}
