extern int printf(const char *, ...);

static int selfbits(int x, int y)
{
	int a = x ^ x;
	int b = x & x;
	int c = x | x;
	int d = (x + y) & (x + y);
	int e = (x * y) ^ (x * y);
	int f = (x - y) | (x - y);
	return a + b + c + d + e + f;
}

static unsigned uselfbits(unsigned u, unsigned v)
{
	unsigned a = (u + v) ^ (u + v);
	unsigned b = (u * v) & (u * v);
	return a + b;
}

int main(void)
{
	printf("ident_bit=%u\n", (unsigned)selfbits(6, 10) + uselfbits(4u, 3u));
	return 0;
}
