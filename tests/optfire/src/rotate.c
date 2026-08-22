extern int printf(const char *, ...);

/* rotate idiom recognition (T-lin-10510, win-x64): a rotate written as OR'd
 * complementary shifts of one non-volatile UNSIGNED scalar folds to a single
 * native rol/ror under -frotate-idiom, and stays a shl/shr/or chain under
 * -fno-rotate-idiom. gcc/clang emit roll/rorl. Covers CONSTANT count (rol/ror
 * reg,imm) and VARIABLE count (rol/ror reg,cl), left and right. The same
 * -frotate-idiom knob also folds the constant-count FUNNEL-shift-left idiom
 * `(x<<C)|(y>>(W-C))` over two DISTINCT unsigned operands to x86 `shld x,y,C`
 * (fnl/fnl64 below; gcc/clang emit shld). Value-exact, so both flag arms must
 * match -O0 (the differ harness checks that). */

static unsigned rl(unsigned x) { return (x << 7) | (x >> 25); }
static unsigned rr(unsigned x) { return (x >> 7) | (x << 25); }
static unsigned long long rl64(unsigned long long x)
{
	return (x << 13) | (x >> 51);
}
static unsigned vrl(unsigned x, int n) { return (x << n) | (x >> (32 - n)); }
static unsigned vrr(unsigned x, int n) { return (x >> n) | (x << (32 - n)); }
static unsigned long long vrl64(unsigned long long x, int n)
{
	return (x << n) | (x >> (64 - n));
}
static unsigned fnl(unsigned x, unsigned y) { return (x << 8) | (y >> 24); }
static unsigned fnlr(unsigned x, unsigned y) { return (y >> 24) | (x << 8); }
static unsigned long long fnl64(unsigned long long x, unsigned long long y)
{
	return (x << 40) | (y >> 24);
}
static unsigned vfnl(unsigned x, unsigned y, int n)
{
	return (x << n) | (y >> (32 - n));
}
static unsigned long long vfnl64(unsigned long long x, unsigned long long y,
                                 int n)
{
	return (x << n) | (y >> (64 - n));
}
static unsigned vfnr(unsigned x, unsigned y, int n)
{
	return (x >> n) | (y << (32 - n));
}
static unsigned long long vfnr64(unsigned long long x, unsigned long long y,
                                 int n)
{
	return (x >> n) | (y << (64 - n));
}

int main(void)
{
	unsigned v = 0x12345678u, u = 0x9abcdef0u;
	unsigned long long w = 0x1122334455667788ull, t = 0x99aabbccddeeff00ull;
	printf("%08x %08x %016llx %08x %08x %016llx %08x %08x %016llx %08x "
	       "%016llx %08x %016llx\n",
	       rl(v), rr(v), rl64(w), vrl(v, 11), vrr(v, 5), vrl64(w, 20),
	       fnl(v, u), fnlr(v, u), fnl64(w, t), vfnl(v, u, 7),
	       vfnl64(w, t, 20), vfnr(v, u, 7), vfnr64(w, t, 20));
	return 0;
}
