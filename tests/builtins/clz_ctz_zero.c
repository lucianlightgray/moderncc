/* Regression for T-mac-30110: the runtime de Bruijn clz/ctz helpers must agree
 * with the constant-fold path on the zero argument (both return the bit width),
 * instead of the table[0] slot (clz->31/63, ctz->0). clz/ctz of 0 is UB, but
 * mcc must not contradict itself: the runtime value (through a volatile) must
 * equal the fold value for the same expression. main returns 0 iff consistent. */
#include <stdio.h>
int main(void) {
	volatile unsigned u = 0;
	volatile unsigned long long l = 0;
	int rclz = __builtin_clz(u), rclzll = __builtin_clzll(l);
	int rctz = __builtin_ctz(u), rctzll = __builtin_ctzll(l);
	int fails = 0;
	if (rclz != 32) fails++;
	if (rclzll != 64) fails++;
	if (rctz != 32) fails++;
	if (rctzll != 64) fails++;
	/* fold path for the same UB input must match the runtime */
	if (__builtin_clz(0u) != rclz) fails++;
	if (__builtin_ctz(0u) != rctz) fails++;
	printf("clzctz0 clz=%d clzll=%d ctz=%d ctzll=%d fails=%d\n",
				 rclz, rclzll, rctz, rctzll, fails);
	return fails;
}
