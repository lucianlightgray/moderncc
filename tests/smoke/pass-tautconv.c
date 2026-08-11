extern int printf(const char *, ...);

static volatile int smp_in[10] = {0, 1, 2, 3, 4, 5, -1, -5, 2147483647,
																	(-2147483647 - 1)};
static volatile unsigned smp_u[4] = {0u, 1u, 0xFFFFFFFFu, 7u};

static int smp_c1(int var)
{
	return var <= 0 || (unsigned long)(unsigned)(var - 1) < 0xFFFFFFFFUL;
}

static int smp_c2(int var)
{
	return var <= 0 || (var - 1) != -1;
}

static int smp_c3(int var)
{
	return (var + 3) == 3 || var != 0;
}

static int smp_c4(int var)
{
	return (var - 1) == 2147483647 || (var - 1) != 2147483647;
}

static int smp_n1(int var)
{
	return (var - 1) < 2 || (var - 1) > 4;
}

static int smp_n2(int var)
{
	return (unsigned long)(unsigned)(var - 1) < 0xFFFFFFFEUL;
}

static int smp_n3(int var)
{
	return var <= 0 || (var - 2) != -1;
}

static int smp_n4(int var)
{
	return (var - 1) > 0 && (var - 1) < 0;
}

static int smp_u1(unsigned var)
{
	return var == 0u || var != 0u;
}

int main(void)
{
	int i;

	printf("tautconv ");
	for (i = 0; i < 10; i++) {
		int v = smp_in[i];
		printf("%d%d%d%d%d%d%d%d", smp_c1(v), smp_c2(v), smp_c3(v), smp_c4(v),
					 smp_n1(v), smp_n2(v), smp_n3(v), smp_n4(v));
	}
	printf(" ");
	for (i = 0; i < 4; i++)
		printf("%d", smp_u1(smp_u[i]));
	printf("\n");
	return 0;
}
