/* dg-error: '_BitInt(513)' exceeds the 512-bit maximum this target supports */
/* C23 6.2.5.  mcc implements _BitInt(N) for N <= 64 (slice 1, a single storage
 * integer), 64 < N <= 128 (slice 2, a 2-limb 16-byte struct), 128 < N <= 256
 * (slice 3, a 4-limb 32-byte struct) and 256 < N <= 512 (an 8-limb 64-byte
 * struct), all riding the __int256/__int512 arith kernels, and defines
 * __BITINT_MAXWIDTH__ to 512 accordingly.  Wider _BitInt needs an
 * arbitrary-limb representation and is refused honestly rather than miscompiled
 * -- a parses-but-wrong _BitInt is worse than a clean refusal. */
int f(void)
{
	_BitInt(513) a = 1;
	return (int)a;
}
