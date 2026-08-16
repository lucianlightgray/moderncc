/* dg-error: '_BitInt(65)' exceeds the 64-bit maximum this target supports */
/* C23 6.2.5.  mcc implements _BitInt(N) for N <= 64 (a single storage integer)
 * and defines __BITINT_MAXWIDTH__ to 64 accordingly.  Wider _BitInt needs the
 * multi-limb representation (slice 2) and is refused honestly rather than
 * miscompiled -- a parses-but-wrong _BitInt is worse than a clean refusal. */
int f(void)
{
	_BitInt(65) a = 1;
	return (int)a;
}
