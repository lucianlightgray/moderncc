extern int printf(const char *, ...);

static unsigned long long seed = 88172645463325252ull;

static unsigned long long xs(void)
{
	seed ^= seed << 13;
	seed ^= seed >> 7;
	seed ^= seed << 17;
	return seed;
}

static unsigned short b16(unsigned short x) { return __builtin_bswap16(x); }
static unsigned int b32(unsigned int x) { return __builtin_bswap32(x); }
static unsigned long long b64(unsigned long long x) { return __builtin_bswap64(x); }

int main(void)
{
	unsigned long long acc = 0;
	int i;

	/* fixed edges first, so a failure names itself */
	printf("%04x %08x %016llx\n", (unsigned)b16(0x1234), b32(0x11223344u),
				 b64(0x1122334455667788ull));
	printf("%04x %08x %016llx\n", (unsigned)b16(0), b32(0), b64(0));
	printf("%04x %08x %016llx\n", (unsigned)b16(0xffff), b32(0xffffffffu),
				 b64(0xffffffffffffffffull));
	printf("%04x %08x %016llx\n", (unsigned)b16(0x00ff), b32(0x000000ffu),
				 b64(0x00000000000000ffull));

	for (i = 0; i < 200; i++) {
		unsigned long long v = xs();
		acc = acc * 31 + b16((unsigned short)v);
		acc = acc * 31 + b32((unsigned int)v);
		acc = acc * 31 + b64(v);
		/* round trip must be the identity at every width */
		if (b16(b16((unsigned short)v)) != (unsigned short)v)
			return 1;
		if (b32(b32((unsigned int)v)) != (unsigned int)v)
			return 2;
		if (b64(b64(v)) != v)
			return 3;
	}
	printf("acc=%016llx\n", acc);
	printf("OK\n");
	return 0;
}
