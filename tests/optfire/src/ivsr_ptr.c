extern int printf(const char *, ...);

static int total(int *a, int n)
{
	int i;
	int t = 0;

	for (i = 0; i < n; i++)
		t += a[i];
	return t;
}

static int weighted(int *a, int n)
{
	int i;
	int t = 0;

	for (i = 0; i < n; i++)
		t += a[i] * (i + 1);
	return t;
}

int main(void)
{
	int buf[24];
	int i;

	for (i = 0; i < 24; i++)
		buf[i] = (i * 7) & 63;
	printf("ivsr_ptr=%d\n", total(buf, 24) + weighted(buf, 24));
	return 0;
}
