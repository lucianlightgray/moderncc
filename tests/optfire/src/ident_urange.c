extern int printf(const char *, ...);

static int urange(unsigned u, unsigned long w)
{
	int a = (u >= 0u);
	int b = (u < 0u);
	int c = (0u <= u);
	int d = (0u > u);
	int e = (w >= 0ul);
	int f = (0ul <= w);
	return a + b + c + d + e + f;
}

int main(void)
{
	printf("ident_urange=%d\n", urange(5u, 7ul));
	return 0;
}
