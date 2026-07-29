extern int printf(const char *, ...);

static unsigned long long seed = 2862933555777941757ull;

static unsigned long long xs(void)
{
	seed ^= seed << 13;
	seed ^= seed >> 7;
	seed ^= seed << 17;
	return seed;
}

static int ref_pop64(unsigned long long v)
{
	int n = 0;
	while (v) {
		n += (int)(v & 1ull);
		v >>= 1;
	}
	return n;
}

static int ref_clrsb32(int x)
{
	int n = 0, s = (x < 0) ? 1 : 0, i;

	for (i = 30; i >= 0; i--) {
		if (((x >> i) & 1) != s)
			break;
		n++;
	}
	return n;
}

static int ref_clrsb64(long long x)
{
	int n = 0, s = (x < 0) ? 1 : 0, i;

	for (i = 62; i >= 0; i--) {
		if ((int)((x >> i) & 1) != s)
			break;
		n++;
	}
	return n;
}

int main(void)
{
	int i;

	for (i = 0; i < 64; i++) {
		unsigned long long v = 1ull << i;
		if (__builtin_popcountll(v) != 1)
			return 1;
		if (__builtin_parityll(v) != 1)
			return 2;
		if (i < 32) {
			if (__builtin_popcount(1u << i) != 1)
				return 3;
			if (__builtin_parity(1u << i) != 1)
				return 4;
		}
	}
	for (i = 0; i < 400; i++) {
		unsigned long long v = xs();
		unsigned w = (unsigned)v;
		int p = ref_pop64(v);
		if (__builtin_popcountll(v) != p)
			return 5;
		if (__builtin_parityll(v) != (p & 1))
			return 6;
		p = ref_pop64((unsigned long long)w);
		if (__builtin_popcount(w) != p)
			return 7;
		if (__builtin_parity(w) != (p & 1))
			return 8;
	}
	for (i = 0; i < 32; i++) {
		int w = 1 << i;
		if (__builtin_clrsb(w) != ref_clrsb32(w))
			return 9;
		if (__builtin_clrsb(-w) != ref_clrsb32(-w))
			return 10;
	}
	for (i = 0; i < 200; i++) {
		unsigned long long v = xs();
		int w = (int)v;
		if (__builtin_clrsb(w) != ref_clrsb32(w))
			return 11;
		if (__builtin_clrsbll((long long)v) != ref_clrsb64((long long)v))
			return 12;
	}
	if (__builtin_clrsb(0) != 31 || __builtin_clrsb(-1) != 31)
		return 13;
	if (__builtin_clrsbll(0) != 63 || __builtin_clrsbll(-1) != 63)
		return 14;

	printf("pop %d %d %d\n", __builtin_popcount(0u), __builtin_popcount(0xffffffffu),
				 __builtin_popcountll(0xffffffffffffffffull));
	printf("par %d %d\n", __builtin_parity(7u), __builtin_parityll(0xfull));
	printf("OK\n");
	return 0;
}
