extern int printf(const char *, ...);

static unsigned low(unsigned u)
{
	unsigned a = (u << 8) >> 8;
	unsigned b = (u << 24) >> 24;
	return a + b;
}

static unsigned long llow(unsigned long w)
{
	unsigned long a = (w << 16) >> 16;
	return a;
}

int main(void)
{
	printf("reassoc_shlshr=%lu\n", (unsigned long)low(0xABCD1234u) + llow(0x12345678ul));
	return 0;
}
