extern int printf(const char *, ...);

static int acc;

static void sink(int v)
{
	acc += v;
}

static int across(int p)
{
	int k = 7;
	int t = p * p + 3;
	int u = t;

	sink(u);
	return k + t + (p * p + 3);
}

static int across2(int p, int q)
{
	int a = 5;
	int b = 9;
	int e = p * q + p;

	sink(e);
	return a * b + e + (p * q + p);
}

int main(void)
{
	int r = across(3) + across2(4, 5);

	printf("call_window=%d\n", r + acc);
	return 0;
}
