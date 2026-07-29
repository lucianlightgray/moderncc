extern int printf(const char *, ...);
extern void *alloca(__SIZE_TYPE__);

static int fill(char *p, int n, int v)
{
	int i;

	for (i = 0; i < n; i++)
		p[i] = (char)v;
	return n;
}

static int sum(char *p, int n)
{
	int i, s = 0;

	for (i = 0; i < n; i++)
		s += p[i];
	return s;
}

static int arg_use(int a, char *p, int b, int n)
{
	return a + b + sum(p, n);
}

static int in_arg(int n)
{
	char *p;

	return arg_use(1, (p = alloca((__SIZE_TYPE__)n), fill(p, n, 2), p), 3, n);
}

static int loop_alloca(int k)
{
	int i, s = 0;

	for (i = 0; i < k; i++) {
		char *p = alloca(16);

		fill(p, 16, i + 1);
		s += sum(p, 16);
	}
	return s;
}

static int nested(int n)
{
	char *p = alloca((__SIZE_TYPE__)n);

	fill(p, n, 4);
	{
		char *q = alloca((__SIZE_TYPE__)n);

		fill(q, n, 5);
		if (p == q)
			return -1;
		return sum(p, n) + sum(q, n);
	}
}

static int alive_after_scope(int n)
{
	char *p;

	{
		p = alloca((__SIZE_TYPE__)n);
		fill(p, n, 7);
	}
	return sum(p, n);
}

static int big(void)
{
	char *p = alloca(4096);

	fill(p, 4096, 1);
	return sum(p, 4096);
}

int main(void)
{
	printf("%d %d %d %d %d\n", in_arg(5), loop_alloca(4), nested(9),
				 alive_after_scope(6), big());
	return 0;
}
