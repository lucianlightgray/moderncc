extern int printf(const char *, ...);

struct attr {
	unsigned aligned : 5, packed : 1, weak : 1;
};

static unsigned long long mask_of(int bits)
{
	return bits >= 64 ? ~0ULL : ((1ULL << bits) - 1);
}

static long long pick_ll(int a, int b)
{
	return a < b ? 0x123456789abcdefLL : -0x76543210fedcbaLL;
}

static int log2p1(int i)
{
	int ret;

	if (!i)
		return 0;
	for (ret = 1; i >= 1 << 8; ret += 8)
		i >>= 8;
	if (i >= 1 << 4)
		ret += 4, i >>= 4;
	if (i >= 1 << 2)
		ret += 2, i >>= 2;
	if (i >= 1 << 1)
		ret++;
	return ret;
}

int main(void)
{
	struct attr a;
	int n, bad = 0;

	for (n = 0; n <= 64; n++) {
		unsigned long long got = mask_of(n);
		unsigned long long want;

		if (n >= 64)
			want = ~0ULL;
		else {
			want = 0;
			for (int k = 0; k < n; k++)
				want |= 1ULL << k;
		}
		if (got != want) {
			printf("mask %d: got %llu want %llu\n", n, got, want);
			bad++;
		}
	}

	if (pick_ll(1, 2) != 0x123456789abcdefLL) {
		printf("pick_ll lo\n");
		bad++;
	}
	if (pick_ll(2, 1) != -0x76543210fedcbaLL) {
		printf("pick_ll hi\n");
		bad++;
	}

	for (n = 1; n <= 16; n <<= 1) {
		a.aligned = (unsigned)log2p1(n);
		if ((int)a.aligned != log2p1(n) || 1 << (a.aligned - 1) != n) {
			printf("aligned %d: field=%u\n", n, a.aligned);
			bad++;
		}
	}

	printf("%s\n", bad ? "FAIL" : "PASS");
	return 0;
}
