extern int printf(const char *, ...);

/* rotate idiom recognition (T-lin-10510, win-x64): a rotate written as
 * `(x<<C)|(x>>(W-C))` over one non-volatile UNSIGNED scalar folds to a single
 * native `rol` under -frotate-idiom, and stays a shl/shr/or chain under
 * -fno-rotate-idiom. gcc/clang both emit `roll`. CONSTANT count -> AST_OP_ROTL
 * (rol reg,imm); VARIABLE count (side-effect-free var, right count = W-n) ->
 * 2-child AST_OP_ROTL (rol reg,cl). Value-exact, so both flag arms must match
 * -O0 (the differ harness checks that). */

static unsigned rl(unsigned x) { return (x << 7) | (x >> 25); }
static unsigned rr(unsigned x) { return (x >> 7) | (x << 25); }
static unsigned long long rl64(unsigned long long x)
{
	return (x << 13) | (x >> 51);
}
/* variable count */
static unsigned vrl(unsigned x, int n) { return (x << n) | (x >> (32 - n)); }
static unsigned long long vrl64(unsigned long long x, int n)
{
	return (x << n) | (x >> (64 - n));
}

int main(void)
{
	unsigned v = 0x12345678u;
	unsigned long long w = 0x1122334455667788ull;
	printf("%08x %08x %016llx %08x %016llx\n", rl(v), rr(v), rl64(w), vrl(v, 11),
	       vrl64(w, 20));
	return 0;
}
