extern int printf(const char *, ...);

/* Side-effect ORDER and COUNT across every construct the AST recorder models
 * with a dedicated hook: ternary arms, && / || short-circuit, comma, compound
 * assignment, pre/post increment, nested calls, and function arguments.
 *
 * This exists because a MODEL bug is invisible to everything else. The replay
 * is byte-compared against the parser before it is used, so a bad replay only
 * costs optimization -- but once a body is accepted, the optimizer passes
 * transform the model and re-emit, and THAT output is compared against nothing.
 * A model that misplaces when an effect happens therefore miscompiles only at
 * -O2 and above. That exact bug was caught here once already (see TODO F3a,
 * "emit-at-marker"), and only because the effects were COUNTED: the repo's other
 * optimizer goldens fold results into a checksum, which a reordered or dropped
 * side effect can leave unchanged.
 *
 * Each function records the sequence of effects into a log; main compares the
 * log to the exact expected string. Order, count and short-circuiting are all
 * pinned. Integer-only and no headers, so it runs on every target. */

static char log_buf[256];
static int log_n;

static int ev(int tag, int val)
{
	if (log_n < (int)sizeof log_buf - 1)
		log_buf[log_n++] = (char)tag;
	return val;
}

static void log_reset(void)
{
	log_n = 0;
	log_buf[0] = 0;
}

static int log_is(const char *want)
{
	int i = 0;

	log_buf[log_n] = 0;
	while (want[i] && log_buf[i] && want[i] == log_buf[i])
		i++;
	return want[i] == 0 && log_buf[i] == 0;
}

static int ternary_arms(int c)
{
	return c ? ev('T', 10) : ev('F', 20);
}

static int andor_short(int a, int b)
{
	return ev('a', a) && ev('b', b);
}

static int oror_short(int a, int b)
{
	return ev('c', a) || ev('d', b);
}

static int comma_seq(void)
{
	int r;

	r = (ev('1', 1), ev('2', 2), ev('3', 3));
	return r;
}

static int compound_assign(void)
{
	int x = 1;

	x += ev('p', 2);
	x *= ev('q', 3);
	x -= ev('r', 4);
	return x;
}

static int incdec_order(void)
{
	int i = 5, a, b;

	a = i++;
	b = ++i;
	ev('i', 0);
	return a * 100 + b;
}

static int nested_calls(void)
{
	return ev('x', 1) + ev('y', 2) * ev('z', 3);
}

static int mixed(int c)
{
	int t = 0;

	if (c && ev('m', 1))
		t += ev('n', 2);
	else
		t += ev('o', 4);
	return t;
}

int main(void)
{
	log_reset();
	if (ternary_arms(1) != 10 || !log_is("T"))
		return 1;
	log_reset();
	if (ternary_arms(0) != 20 || !log_is("F"))
		return 2;

	/* short-circuit: the right operand must not be evaluated at all */
	log_reset();
	if (andor_short(0, 1) != 0 || !log_is("a"))
		return 3;
	log_reset();
	if (andor_short(1, 1) != 1 || !log_is("ab"))
		return 4;
	log_reset();
	if (oror_short(1, 1) != 1 || !log_is("c"))
		return 5;
	log_reset();
	if (oror_short(0, 1) != 1 || !log_is("cd"))
		return 6;

	log_reset();
	if (comma_seq() != 3 || !log_is("123"))
		return 7;

	log_reset();
	if (compound_assign() != 5 || !log_is("pqr"))
		return 8;

	log_reset();
	if (incdec_order() != 507 || !log_is("i"))
		return 9;

	log_reset();
	if (nested_calls() != 7 || !log_is("xyz"))
		return 10;

	log_reset();
	if (mixed(1) != 2 || !log_is("mn"))
		return 11;
	log_reset();
	if (mixed(0) != 4 || !log_is("o"))
		return 12;

	printf("effects ok\n");
	printf("OK\n");
	return 0;
}
