extern int printf(const char *, ...);

int gn = 12;
int gk;

/* The tail call sits in an ARM of an `AST_If` op 5, not directly under the
 * Return. `ast_tco_run` matched only the direct shape, so this cell fires
 * nothing before the arm matcher lands. It is also the shape
 * `rir_tern_normalise` folds `if (c) return acc; return f(...);` into, so
 * keeping it firing is what lets that fold keep the tail-recursive two-exit
 * `if` instead of eating it. */
static long long sum_tern(long long n, long long acc)
{
	return n <= 0 ? acc : sum_tern(n - 1, acc + n);
}

/* Both arms are self tail calls. The guard block is deliberately two
 * statements so the fold leaves this ternary flat rather than nesting it
 * inside a second one. */
static long long alt(long long n, long long acc)
{
	if (n <= 0) {
		gk = 1;
		return acc;
	}
	return (n & 1) ? alt(n - 1, acc + n) : alt(n - 1, acc - n);
}

int main(void)
{
	long long a = sum_tern(gn, 0);
	long long b = alt(gn, 0);
	printf("%lld %lld %d\n", a, b, gk);
	return 0;
}
