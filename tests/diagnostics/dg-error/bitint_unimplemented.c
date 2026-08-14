/* dg-error: '_BitInt' is not implemented */
/* C23 6.2.5. mcc does not implement it, and correctly does not define
 * __BITINT_MAXWIDTH__, so feature-guarded code takes the right branch.
 *
 * What was wrong was the message for code that uses it anyway: `_BitInt` was not
 * a keyword, so the parser took the identifier path and reported the NEXT token
 * -- "';' expected (got 'a')" -- which points at the wrong token, blames the
 * wrong construct, and reads like a syntax error in the user's own code rather
 * than a missing feature. */
int f(void)
{
	_BitInt(37) a = 100000;
	return (int)a;
}
