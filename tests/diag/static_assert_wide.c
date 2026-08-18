/* T-mac-30132: _Static_assert must evaluate its controlling constant at full
 * 64-bit width (do_Static_assert uses expr_const64_pub, not 32-bit expr_const),
 * not fatally reject a >32-bit value. The `== 0` guards prove no truncation. */
_Static_assert(1LL << 32, "2^32 is non-zero");
_Static_assert(0x100000000, "hex 2^32");
_Static_assert((1LL << 40) != 0, "2^40 non-zero");
_Static_assert(!(0x100000000 == 0), "not truncated to 0");
int main(void) { return 0; }
