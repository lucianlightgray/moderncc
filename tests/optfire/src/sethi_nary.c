extern int printf(const char *, ...);

static int chain(int a, int b, int c, int d, int e)
{
	return a + b + (c * d) + e;
}

static int chain2(int a, int b, int c, int d)
{
	return a * b * (c + d) * a;
}

int main(void)
{
	printf("sethi_nary=%d\n", chain(1, 2, 3, 4, 5) + chain2(2, 3, 1, 1));
	return 0;
}
