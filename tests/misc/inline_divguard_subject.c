extern int printf(const char *, ...);

static int cheap(int i, int j) { return (i + j) * (i + j + 1) / 2 + i + 1; }

double divfed(const double *v, int n)
{
	double s = 0;
	for (int i = 0; i < n; i++)
		for (int j = 0; j < n; j++)
			s += v[j] / cheap(i, j);
	return s;
}

int main(void)
{
	double a[4] = {1, 2, 3, 4};
	printf("%.6f\n", divfed(a, 4));
	return 0;
}
