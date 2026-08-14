/* dg-error: address of register variable */
/* C11 6.3.2.1p3: decaying a `register` array to a pointer takes its address,
 * which is undefined. gcc and clang both reject this form; mcc used to accept
 * it silently.
 *
 * unary() has always diagnosed the EXPLICIT forms -- &a, &a[0], &x. What was
 * missing is implicit decay, which never reaches unary(): in mcc an array is
 * VT_PTR | VT_ARRAY, so decay is not a conversion, it is gen_cast() dropping
 * the flag on the way out. That is where the check now lives.
 *
 * Deliberately NOT covered, because the two references disagree and mcc follows
 * gcc: `a[1]` is accepted by gcc and rejected by clang. `*a` and `*(a+1)` are
 * rejected by both and are still accepted here -- see docs/TODO.md. */
int f(void)
{
	register int a[4];
	int *p = a;
	return p[0];
}
