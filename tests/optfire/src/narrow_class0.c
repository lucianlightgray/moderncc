extern int printf(const char *, ...);

static int chained(int a, long c, long d)
{
	return (int)((int)((long)a * c) + d);
}

static int chained2(int a, long c, long d)
{
	return (int)((int)((long)a + c) - d);
}

int main(void)
{
	printf("narrow_class0=%d\n", chained(7, 5L, 11L) + chained2(3, 4L, 2L));
	return 0;
}
