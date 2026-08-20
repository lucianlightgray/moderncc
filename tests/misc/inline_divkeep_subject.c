extern int printf(const char *, ...);

static double recip(double d) { return 1.0 / d; }
static int twice(int v) { return v + v; }

double owned(const double *v, int n)
{
	double s = 0;
	for (int i = 0; i < n; i++)
		s += v[i] * recip((double)(i + 1));
	return s;
}

int scaled(int x) { return twice(x) + twice(x + 1); }

int main(void)
{
	double a[4] = {1, 2, 3, 4};
	printf("%.6f %d\n", owned(a, 4), scaled(3));
	return 0;
}
