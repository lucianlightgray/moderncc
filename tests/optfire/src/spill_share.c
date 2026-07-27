extern int printf(const char *, ...);

static int acc;

static void sink(int v)
{
	acc += v;
}

static int staged(int p)
{
	int a, b, c, d;

	a = p + 1;
	sink(a);
	b = p * 3;
	sink(b);
	c = p - 5;
	sink(c);
	d = p ^ 9;
	sink(d);
	return acc;
}

int main(void)
{
	int r = staged(4);

	r += staged(11);
	printf("spill_share=%d\n", r);
	return 0;
}
