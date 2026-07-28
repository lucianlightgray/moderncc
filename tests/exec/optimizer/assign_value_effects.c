extern int printf(const char *, ...);

/* An assignment used as a VALUE must evaluate its right-hand side exactly once.
 * The AST recorder currently models the residual value of a store as the RHS
 * SUBTREE (ast_hook_vstore, mccast.c), so a replay of `if ((h = f()))` emits the
 * call twice; the faithfulness check catches that and the body is discarded, so
 * nothing miscompiles today. This test exists so a future fix to that modelling
 * cannot reintroduce the duplication silently: every case below counts its own
 * side effects, so an extra (or missing) evaluation changes stdout rather than
 * hiding inside a checksum. See TODO F3a. */

static long calls;
static long calls2;

static int f(int v)
{
	calls++;
	return v;
}

static int g2(int v)
{
	calls2++;
	return v;
}

static int if_cond(int v)
{
	int h;

	if ((h = f(v)))
		return h + 1;
	return 0;
}

static int while_cond(int n)
{
	int h, s = 0;

	while ((h = f(n))) {
		s += h;
		n--;
	}
	return s;
}

static int chained(int v)
{
	int a, b;

	a = b = f(v);
	return a + b;
}

static int shortcircuit_both(int v, int w)
{
	int x, y = 0;

	if ((x = f(v)) && (y = g2(w)))
		return x + y;
	return x - y;
}

static int nested_assign(int v)
{
	int a, b;

	a = (b = f(v)) + 1;
	return a + b;
}

static int assign_in_arg(int v)
{
	int a;

	return f((a = g2(v)) + 0) + a;
}

static int assign_in_ternary(int v)
{
	int a;

	return (a = f(v)) ? a * 2 : -1;
}

static int assign_in_return(int v)
{
	int a;

	a = 0;
	return (a = f(v)) + a;
}

int main(void)
{
	long c0, d0;

	calls = 0;
	calls2 = 0;

	c0 = calls;
	if (if_cond(7) != 8)
		return 1;
	if (calls - c0 != 1)
		return 2;

	c0 = calls;
	if (if_cond(0) != 0)
		return 3;
	if (calls - c0 != 1)
		return 4;

	c0 = calls;
	if (while_cond(3) != 6)
		return 5;
	if (calls - c0 != 4)
		return 6;

	c0 = calls;
	if (chained(5) != 10)
		return 7;
	if (calls - c0 != 1)
		return 8;

	c0 = calls;
	d0 = calls2;
	if (shortcircuit_both(3, 4) != 7)
		return 9;
	if (calls - c0 != 1 || calls2 - d0 != 1)
		return 10;

	c0 = calls;
	d0 = calls2;
	if (shortcircuit_both(0, 4) != 0)
		return 11;
	/* left operand is 0, so the right side must NOT be evaluated at all */
	if (calls - c0 != 1 || calls2 - d0 != 0)
		return 12;

	c0 = calls;
	if (nested_assign(6) != 13)
		return 13;
	if (calls - c0 != 1)
		return 14;

	c0 = calls;
	d0 = calls2;
	if (assign_in_arg(9) != 18)
		return 15;
	if (calls - c0 != 1 || calls2 - d0 != 1)
		return 16;

	c0 = calls;
	if (assign_in_ternary(4) != 8)
		return 17;
	if (calls - c0 != 1)
		return 18;

	c0 = calls;
	if (assign_in_ternary(0) != -1)
		return 19;
	if (calls - c0 != 1)
		return 20;

	c0 = calls;
	if (assign_in_return(5) != 10)
		return 21;
	if (calls - c0 != 1)
		return 22;

	printf("calls=%ld calls2=%ld\n", calls, calls2);
	printf("OK\n");
	return 0;
}
