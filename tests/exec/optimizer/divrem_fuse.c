extern int printf(const char *, ...);

static long sdr(long a, long b)
{
	long q = a / b;
	long r = a % b;
	return q * 1000 + r;
}

static unsigned long udr(unsigned long a, unsigned long b)
{
	unsigned long q = a / b;
	unsigned long r = a % b;
	return q * 1000 + r;
}

static int idr(int a, int b)
{
	int q = a / b;
	int r = a % b;
	return q * 1000 + r;
}

int main(void)
{
	volatile long vb = 7;
	long b = vb;
	volatile int vi = 13;
	int ib = vi;

	printf("%ld\n", sdr(100, b));
	printf("%ld\n", sdr(-100, b));
	printf("%ld\n", sdr(100, -b));
	printf("%ld\n", sdr(-100, -b));
	printf("%lu\n", udr(1000000UL, (unsigned long)b));
	printf("%d\n", idr(1000000, ib));
	printf("%d\n", idr(-1000000, ib));
	return 0;
}
