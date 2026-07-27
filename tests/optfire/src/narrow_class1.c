extern int printf(const char *, ...);

static int widened(int a, int b)
{
	return (int)((long)a + (long)b);
}

static unsigned uwidened(unsigned a, unsigned b)
{
	return (unsigned)((unsigned long)a * (unsigned long)b);
}

int main(void)
{
	printf("narrow_class1=%u\n", (unsigned)widened(11, 22) + uwidened(5u, 6u));
	return 0;
}
