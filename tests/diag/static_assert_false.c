/* Known-positive for T-mac-30132: a wide controlling value that is genuinely
 * zero must still FAIL (proving the assert isn't just always-passing). */
_Static_assert((1LL << 32) == (1LL << 33), "wide equal-false must fail");
int main(void) { return 0; }
