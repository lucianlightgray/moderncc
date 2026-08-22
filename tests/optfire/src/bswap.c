extern int printf(const char *, ...);

/* bswap-from-shifts idiom recognition (T-lin-10510, win-x64): the classic
 * byte-reversal written as OR'd masked shifts of one non-volatile scalar folds
 * to a single AST_OP_BSWAP (native bswap/rev) under -fbswap-idiom, and stays a
 * shift/and/or chain under -fno-bswap-idiom. gcc/clang both recognize it. The
 * fold is value-exact, so both flag arms must match -O0 (the differ harness
 * checks that). b32/b64/b32r fire; the reads keep the results live. */

static unsigned b32(unsigned x)
{
	return ((x >> 24) & 0xffu) | ((x >> 8) & 0xff00u) |
	       ((x << 8) & 0xff0000u) | ((x << 24) & 0xff000000u);
}

/* term order permuted -- the recognizer is reassociation-agnostic */
static unsigned b32r(unsigned x)
{
	return ((x << 24) & 0xff000000u) | ((x >> 8) & 0xff00u) |
	       ((x << 8) & 0xff0000u) | ((x >> 24) & 0xffu);
}

static unsigned long long b64(unsigned long long x)
{
	return ((x >> 56) & 0xffull) | ((x >> 40) & 0xff00ull) |
	       ((x >> 24) & 0xff0000ull) | ((x >> 8) & 0xff000000ull) |
	       ((x << 8) & 0xff00000000ull) | ((x << 24) & 0xff0000000000ull) |
	       ((x << 40) & 0xff000000000000ull) | ((x << 56) & 0xff00000000000000ull);
}

int main(void)
{
	unsigned v = 0x11223344u;
	unsigned long long w = 0x1122334455667788ull;
	printf("%08x %08x %016llx\n", b32(v), b32r(v), b64(w));
	return 0;
}
