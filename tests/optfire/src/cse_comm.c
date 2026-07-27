extern int printf(const char *, ...);

static int swapped(int a, int b)
{
	int u = a * b;
	int v = b * a;
	return u + v;
}

static int swapped_mix(int a, int b)
{
	int u = a + b;
	int v = b + a;
	int w = a & b;
	int z = b & a;
	return u + v + w + z;
}

int main(void)
{
	printf("cse_comm=%d\n", swapped(6, 7) + swapped_mix(12, 10));
	return 0;
}
