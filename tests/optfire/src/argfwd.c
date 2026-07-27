extern int printf(const char *, ...);

static int tbl[4] = {10, 20, 30, 40};

static int pick(const int *p, int k)
{
	return p[k];
}

static int addk(int x, int k)
{
	return x + k;
}

int main(void)
{
	int i = 2;
	int a = pick(tbl, i);
	int b = addk(a, i);
	int c = pick(tbl, b - a - 1);

	printf("argfwd=%d\n", a + b + c);
	return 0;
}
