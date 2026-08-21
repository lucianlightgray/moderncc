extern int printf(const char *, ...);

static double fp(int c, double a, double b)
{
	double t;
	if (c)
		t = a + b;
	else
		t = 0;
	double u = a + b;
	return t + u;
}

static int ip(int c, int a, int b)
{
	int t;
	if (c)
		t = a + b;
	else
		t = 0;
	int u = a + b;
	return t + u;
}

static long double lp(int c, long double a, long double b)
{
	long double t;
	if (c)
		t = a + b;
	else
		t = 0;
	long double u = a + b;
	return t + u;
}

static long long qp(int c, long long a, long long b)
{
	long long t;
	if (c)
		t = a + b;
	else
		t = 0;
	long long u = a + b;
	return t + u;
}

int main(void)
{
	printf("%.1f %d %.1Lf %lld\n", fp(1, 3, 4), ip(1, 3, 4),
				 lp(1, 3, 4), qp(1, 3, 4));
	return 0;
}
