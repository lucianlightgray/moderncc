extern int printf(const char *, ...);

/* constant-count rotate idiom recognition (T-lin-10510, win-x64): the rotate
 * written as `(x<<C)|(x>>(W-C))` over one non-volatile UNSIGNED scalar folds to a
 * single AST_OP_ROTL (native `rol`) under -frotate-idiom, and stays a shl/shr/or
 * chain under -fno-rotate-idiom. gcc/clang both emit `roll`. Value-exact, so both
 * flag arms must match -O0 (the differ harness checks that). rl/rr/rl64 fire. */

static unsigned rl(unsigned x) { return (x << 7) | (x >> 25); }
/* right-rotate written as shr|shl -- folds to rol by (W-C) */
static unsigned rr(unsigned x) { return (x >> 7) | (x << 25); }
static unsigned long long rl64(unsigned long long x)
{
	return (x << 13) | (x >> 51);
}

int main(void)
{
	unsigned v = 0x12345678u;
	unsigned long long w = 0x1122334455667788ull;
	printf("%08x %08x %016llx\n", rl(v), rr(v), rl64(w));
	return 0;
}
