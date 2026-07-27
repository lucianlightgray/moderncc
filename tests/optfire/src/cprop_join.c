extern int printf(const char *, ...);

static int merge(int c)
{
	int v;

	if (c)
		v = 10;
	else
		v = 10;
	return v * 3 + v;
}

static int merge2(int c, int d)
{
	int a, b;

	if (c) {
		a = 4;
		b = 6;
	} else {
		a = 4;
		b = 6;
	}
	if (d)
		a = a + b;
	else
		a = a + b;
	return a * 2 + b;
}

int main(void)
{
	printf("cprop_join=%d\n", merge(0) + merge(1) + merge2(0, 1) + merge2(1, 0));
	return 0;
}
