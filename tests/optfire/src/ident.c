extern int printf(const char *, ...);

int gx = 11;

static int fold_identities(int x, long w)
{
	int a = (int)(long)x;
	int b = (int)(long)(x + 1);
	long c = (long)(int)w;
	return a + b + (int)c;
}

int main(void)
{
	printf("%d\n", fold_identities(gx, (long)gx * 3));
	return 0;
}
