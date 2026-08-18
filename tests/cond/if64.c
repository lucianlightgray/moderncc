/* Regression for T-mac-30089: #if/#elif controlling constants wider than 32 bits
 * must be evaluated at full 64-bit width (expr_preprocess uses expr_const64_pub,
 * not the 32-bit expr_const), instead of a fatal "bad preprocessor expression".
 * Self-checking: main returns 0 iff every branch was taken correctly. The
 * `== 0` guards prove the wide values are not truncated to 0. */
#include <stdio.h>
#define ADDR_BASE 0x100000000
int main(void) {
	int ok = 0, checks = 0;
	checks++;
#if 0x100000000
	ok++;                 /* 2^32 is non-zero -> true */
#endif
	checks++;
#if ADDR_BASE
	ok++;                 /* 64-bit address macro -> true */
#endif
	checks++;
#if (1ULL << 40)
	ok++;                 /* 2^40 -> true */
#endif
	checks++;
#if 0
#elif 0x200000000
	ok++;                 /* elif shares the path -> true */
#endif
	checks++;
#if 0xFFFFFFFFU + 1 == 0x100000000
	ok++;                 /* internal arithmetic is 64-bit */
#endif
	checks++;
#if 0x100000000 == 0
	ok--;                 /* MUST NOT fire: 2^32 is not 0 (no truncation) */
#endif
	checks++;
#if (1LL << 40) == 0
	ok--;                 /* MUST NOT fire */
#endif
	printf("if64 checks=%d ok=%d\n", checks, ok);
	return ok == 5 ? 0 : 1;
}
