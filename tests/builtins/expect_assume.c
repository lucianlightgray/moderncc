/* T-mac-30125: __builtin_assume is a real intrinsic (typechecks its unevaluated
 * operand); __builtin_expect/unreachable/trap/assume statements don't trip
 * -Wunused-value; __builtin_expect preserves its operand's value.
 * (The __builtin_expect result-TYPE-is-long fix is split to T-mac-30131 — it
 * shifts the o0-baseline bank for codeopt.c and needs a fleet re-bank.) */
#include <stdio.h>
static int calls = 0;
static int se(void) { calls++; return 1; }
int main(void) {
	int fails = 0;
	if ((int)__builtin_expect(7, 0) != 7) fails++;
	__builtin_assume(se() > 0);           /* operand unevaluated */
	if (calls != 0) fails++;
	if (!fails) { int x = 1; if (!x) __builtin_unreachable(); }  /* no -Wunused-value */
	printf("expect_assume fails=%d\n", fails);
	return fails;
}
