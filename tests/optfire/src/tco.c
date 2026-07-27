extern int printf(const char *, ...);

int gn = 10;

static int sum_to(int n, int acc)
{
	if (n <= 0)
		return acc;
	return sum_to(n - 1, acc + n);
}

int main(void)
{
	printf("%d\n", sum_to(gn, 0));
	return 0;
}
