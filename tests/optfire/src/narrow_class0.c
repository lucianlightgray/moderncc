extern int printf(const char *, ...);

static int chained(int a, long long c, long long d)
{
	return (int)((int)((long long)a * c) + d);
}

static int chained2(int a, long long c, long long d)
{
	return (int)((int)((long long)a + c) - d);
}

int main(void)
{
	printf("narrow_class0=%d\n", chained(7, 5LL, 11LL) + chained2(3, 4LL, 2LL));
	return 0;
}
