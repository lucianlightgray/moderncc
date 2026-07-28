extern int printf(const char *, ...);

static int mixed(int a, long long c)
{
	return (int)((long long)a * c);
}

static int mixed2(int a, long long c)
{
	return (int)((long long)a & c);
}

int main(void)
{
	printf("narrow_class3=%d\n", mixed(6, 7LL) + mixed2(12, 10LL));
	return 0;
}
