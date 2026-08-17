/* dg-error: '_BitInt(257)' exceeds the 256-bit maximum this target supports */
/* C23 6.2.5.  mcc implements _BitInt(N) for N <= 64 (slice 1, a single storage
 * integer), 64 < N <= 128 (slice 2, a 2-limb 16-byte struct) and 128 < N <= 256
 * (slice 3, a 4-limb 32-byte struct), all riding the __int256 arith kernel, and
 * defines __BITINT_MAXWIDTH__ to 256 accordingly.  Wider _BitInt needs an
 * arbitrary-limb representation and is refused honestly rather than miscompiled
 * -- a parses-but-wrong _BitInt is worse than a clean refusal. */
int f(void)
{
	_BitInt(257) a = 1;
	return (int)a;
}
