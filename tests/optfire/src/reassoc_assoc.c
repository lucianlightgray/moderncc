extern int printf(const char *, ...);

static int nests(int x)
{
	int a = (x + 3) + 5;
	int b = (x & 0xF0) & 0x3C;
	int c = (x | 0x11) | 0x22;
	int d = (x ^ 0x0F) ^ 0x30;
	int e = ((x >> 2) >> 3);
	return a + b + c + d + e;
}

static unsigned unests(unsigned u)
{
	unsigned a = (u << 2) << 3;
	unsigned b = (u >> 1) >> 2;
	unsigned c = (u * 3u) * 5u;
	return a + b + c;
}

int main(void)
{
	printf("reassoc_assoc=%u\n", (unsigned)nests(1000) + unests(64u));
	return 0;
}
