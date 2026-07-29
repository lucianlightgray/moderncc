extern int printf(const char *, ...);

static unsigned long long seed = 6364136223846793005ull;

static unsigned long long xs(void)
{
	seed ^= seed << 13;
	seed ^= seed >> 7;
	seed ^= seed << 17;
	return seed;
}

static int ref_clz32(unsigned v)
{
	int n = 0;
	while (!(v & 0x80000000u)) {
		v <<= 1;
		n++;
	}
	return n;
}

static int ref_ctz32(unsigned v)
{
	int n = 0;
	while (!(v & 1u)) {
		v >>= 1;
		n++;
	}
	return n;
}

static int ref_clz64(unsigned long long v)
{
	int n = 0;
	while (!(v & 0x8000000000000000ull)) {
		v <<= 1;
		n++;
	}
	return n;
}

static int ref_ctz64(unsigned long long v)
{
	int n = 0;
	while (!(v & 1ull)) {
		v >>= 1;
		n++;
	}
	return n;
}

int main(void)
{
	int i;

	for (i = 0; i < 32; i++) {
		unsigned v = 1u << i;
		if (__builtin_clz(v) != ref_clz32(v))
			return 1;
		if (__builtin_ctz(v) != ref_ctz32(v))
			return 2;
	}
	for (i = 0; i < 64; i++) {
		unsigned long long v = 1ull << i;
		if (__builtin_clzll(v) != ref_clz64(v))
			return 3;
		if (__builtin_ctzll(v) != ref_ctz64(v))
			return 4;
	}
	for (i = 0; i < 300; i++) {
		unsigned long long v = xs();
		unsigned w = (unsigned)v;
		if (!v)
			continue;
		if (__builtin_clzll(v) != ref_clz64(v))
			return 5;
		if (__builtin_ctzll(v) != ref_ctz64(v))
			return 6;
		if (w) {
			if (__builtin_clz(w) != ref_clz32(w))
				return 7;
			if (__builtin_ctz(w) != ref_ctz32(w))
				return 8;
		}
	}
	printf("clz %d %d %d\n", __builtin_clz(1u), __builtin_clz(0xffffffffu),
				 __builtin_clzll(1ull));
	printf("ctz %d %d %d\n", __builtin_ctz(1u), __builtin_ctz(0x80000000u),
				 __builtin_ctzll(1ull << 40));
	printf("OK\n");
	return 0;
}
