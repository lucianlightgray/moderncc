extern int printf(const char *, ...);

static int pair(int s)
{
	int a, b;

	a = b = s;
	return a + b;
}

static int triple(int s)
{
	int a, b, c;

	a = b = c = s;
	return a + b + c;
}

static int loopsum(int s)
{
	int a, b, i;
	int t = 0;

	a = b = s;
	for (i = 0; i < 6; i++)
		t += a + b + i;
	return t;
}

int main(void)
{
	int total = pair(3) + triple(5) + loopsum(7);

	printf("chainstore=%d\n", total);
	return 0;
}
