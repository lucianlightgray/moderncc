extern int printf(const char *, ...);

static int withlit(int a)
{
	return (int)((long long)a + 1000LL);
}

static int withlit2(int a)
{
	int r;
	r = (int)((long long)a ^ 255LL);
	return r;
}

int main(void)
{
	printf("narrow_class2=%d\n", withlit(7) + withlit2(9));
	return 0;
}
