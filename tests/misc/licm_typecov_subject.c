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

int main(void)
{
	double v[3] = {1, 2, 3};
	int w[3] = {1, 2, 3};
	printf("%.1f %d\n", fl(v, 3, 2, 3), il(w, 3, 2, 3));
	return 0;
}
