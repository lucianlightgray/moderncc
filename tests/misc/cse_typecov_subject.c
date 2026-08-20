extern int printf(const char *, ...);

static double dcse(double a, double b)
{
	double x = a * b - a;
	double y = a * b - a;
	return x + y;
}

static float scse(float a, float b)
{
	float x = a * b - a;
	float y = a * b - a;
	return x + y;
}

static long pcse(char *base, long n)
{
	char *x = base + n;
	char *y = base + n;
	return (x - base) + (y - base);
}

static int icse(int a, int b)
{
	int x = a * b - a;
	int y = a * b - a;
	return x + y;
}

static __int128 ncse(__int128 a, __int128 b)
{
	__int128 x = a * b - a;
	__int128 y = a * b - a;
	return x + y;
}

static long double lcse(long double a, long double b)
{
	long double x = a * b - a;
	long double y = a * b - a;
	return x + y;
}

int main(void)
{
	char buf[16];
	printf("%.1f %.1f %ld %d %.1Lf %lld\n", dcse(3, 4), (double)scse(3, 4),
				 pcse(buf, 5), icse(3, 4), lcse(3, 4), (long long)ncse(3, 4));
	return 0;
}
