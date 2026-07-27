extern int printf(const char *, ...);

static int branchy(int x, int y, int c)
{
	int r;

	if (c)
		r = x * y + x * y;
	else
		r = x * y - 1;
	r += x * y;
	return r;
}

static int looped(int x, int y, int n)
{
	int i;
	int r = 0;

	for (i = 0; i < n; i++)
		r += x * y;
	r += x * y;
	return r;
}

static int mixed(int x, int y, int c)
{
	int a = x * y;
	int r;

	if (c)
		r = a + 1;
	else
		r = x * y + 2;
	return r + x * y;
}

int main(void)
{
	int s = branchy(2, 3, 1) + branchy(2, 3, 0) + looped(4, 5, 3) +
					mixed(6, 7, 0) + mixed(6, 7, 1);

	printf("cse_join=%d\n", s);
	return 0;
}
