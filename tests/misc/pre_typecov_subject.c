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

static long long np(int c, __int128 a, __int128 b)
{
	__int128 t;
	if (c)
		t = a + b;
	else
		t = 0;
	__int128 u = a + b;
	return (long long)(t + u);
}

int main(void)
{
	printf("%.1f %d %.1Lf %lld\n", fp(1, 3, 4), ip(1, 3, 4),
				 lp(1, 3, 4), (long long)np(1, 3, 4));
	return 0;
}
