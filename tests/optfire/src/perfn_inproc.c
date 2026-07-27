extern int printf(const char *, ...);

static int chunk(int v, int k)
{
	int a = v * k + 3;
	int b = a ^ (v << 2);
	int c = b + (k * 7);
	int d = c ^ (a >> 1);
	int e = d + (b * 5);
	int f = e ^ (c << 1);
	return (f + a + b + c + d + e) & 0xffff;
}

static int driver(int seed)
{
	int r = seed;
	int i;

	for (i = 0; i < 4; i++) {
		r = chunk(r, 3) + chunk(r, 5);
		r ^= chunk(r, 7) + chunk(r, 9);
		r &= 0xffff;
	}
	return r;
}

int main(void)
{
	printf("perfn_inproc=%d\n", driver(17));
	return 0;
}
