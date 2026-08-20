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

int main(void)
{
	printf("%.1f %d\n", fp(1, 3, 4), ip(1, 3, 4));
	return 0;
}
