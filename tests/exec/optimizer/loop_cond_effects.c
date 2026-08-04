extern int printf(const char *, ...);

static int calls;

static int tick(void)
{
	calls++;
	return 1;
}

static int do_comma_incr(void)
{
	int a[4] = {10, 20, 30, 40};
	int *p = a;
	int f = 2;
	int sum = 0;

	do {
		sum += *p;
	} while (++p, --f);
	return sum;
}

static int while_comma_incr(void)
{
	int a[4] = {1, 2, 4, 8};
	int *p = a;
	int n = 0;
	int sum = 0;

	while (++n, n <= 3) {
		sum += *p;
		p++;
	}
	return sum;
}

static int do_comma_call(void)
{
	int f = 3;
	int sum = 0;

	do {
		sum += f;
	} while (tick(), --f);
	return sum;
}

static int for_comma_incr(void)
{
	int a[4] = {5, 6, 7, 8};
	int *p = a;
	int i = 0;
	int sum = 0;

	for (; ++p, i < 3; i++)
		sum += *p;
	return sum;
}

static int nested_comma(void)
{
	int i = 0, j, sum = 0;

	do {
		j = 0;
		do {
			sum += i * 10 + j;
		} while (++j, j < 2);
	} while (++i, i < 2);
	return sum;
}

int main(void)
{
	calls = 0;

	if (do_comma_incr() != 30)
		return 1;
	if (while_comma_incr() != 7)
		return 2;
	if (do_comma_call() != 6)
		return 3;
	if (calls != 3)
		return 4;
	if (for_comma_incr() != 21)
		return 5;
	if (nested_comma() != 22)
		return 6;

	printf("calls=%d\n", calls);
	printf("OK\n");
	return 0;
}
