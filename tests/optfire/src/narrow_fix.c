extern int printf(const char *, ...);

static int triple(int a, int b, int c)
{
	return (int)((long)a + (long)b + (long)c);
}

static int quad(int a, int b, int c, int d)
{
	return (int)((long)a + (long)b + (long)c + (long)d);
}

int main(void)
{
	printf("narrow_fix=%d\n", triple(3, 5, 7) + quad(1, 2, 3, 4));
	return 0;
}
