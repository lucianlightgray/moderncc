extern int printf(const char *, ...);

int main(int argc, char **argv)
{
	(void)argv;
	int n = 1234567;
	int d = argc + 136;
	int q = n / d;
	int r = n % d;
	unsigned u = (unsigned)n, ud = (unsigned)d;
	unsigned uq = u / ud;
	unsigned ur = u % ud;
	printf("%d %d %u %u\n", q, r, uq, ur);
	return 0;
}
