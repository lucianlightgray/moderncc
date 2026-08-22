extern int printf(const char *, ...);

/* popcount-LOOP idiom (T-lin-10510, win-x64): the Kernighan bit-count loop
 * `while(x){ c++; x &= x-1; }` over an UNSIGNED scalar folds to a straight-line
 * portable-SWAR popcount under -floop-idiom (reusing AST_OP_BITB/BB_POPCOUNT,
 * NOT native popcnt -- so it is arch-independent + CPU-feature-free), and stays a
 * bit-clear loop under -fno-loop-idiom. Covers while + for(;x;), the c++/++c and
 * c+=1 accumulator forms, the `x` and `x != 0` conditions, and 32/64-bit. gcc/
 * clang recognize the same idiom. Value-exact, so both flag arms must match -O0
 * (the differ harness checks that). The x arguments are runtime (parameters), so
 * the loops are not const-folded away. */

static int pc32(unsigned x)
{
	int c = 0;
	while (x) {
		c++;
		x &= x - 1;
	}
	return c;
}
static int pc32f(unsigned x)
{
	int c = 0;
	for (; x;) {
		c += 1;
		x = x & (x - 1);
	}
	return c;
}
static int pc32ne(unsigned x)
{
	int c = 0;
	while (x != 0) {
		++c;
		x &= x - 1;
	}
	return c;
}
static int pc64(unsigned long long x)
{
	int c = 0;
	while (x) {
		c++;
		x &= x - 1;
	}
	return c;
}

int main(void)
{
	unsigned v = 0xdeadbeefu, u = 0x55555555u;
	unsigned long long w = 0x123456789abcdef0ull;
	printf("%d %d %d %d %d\n", pc32(v), pc32f(v), pc32ne(v), pc64(w), pc32(u));
	return 0;
}
