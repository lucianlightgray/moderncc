/* dg-error: '_BitInt(129)' exceeds the 128-bit maximum this target supports */
/* C23 6.2.5.  mcc implements _BitInt(N) for N <= 64 (a single storage integer,
 * slice 1) and 64 < N <= 128 (a 16-byte 2-limb struct riding the __int256
 * arith kernel, slice 2), defining __BITINT_MAXWIDTH__ to 128 accordingly.
 * Wider _BitInt needs an arbitrary-limb representation and is refused honestly
 * rather than miscompiled -- a parses-but-wrong _BitInt is worse than a clean
 * refusal. */
int f(void)
{
	_BitInt(129) a = 1;
	return (int)a;
}
