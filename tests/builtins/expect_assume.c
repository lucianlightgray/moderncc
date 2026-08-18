/* T-mac-30125: __builtin_expect result type is `long` (sizeof/_Generic), value
 * preserved; __builtin_expect/unreachable/trap don't trip -Wunused-value;
 * __builtin_assume typechecks its (unevaluated) operand. */
#include <stdio.h>
static int calls = 0;
static int se(void) { calls++; return 1; }
int main(void) {
	int fails = 0;
	if (sizeof(__builtin_expect((char)1, 0)) != sizeof(long)) fails++;
	if (_Generic(__builtin_expect(1, 0), long: 1, int: 2, default: 0) != 1) fails++;
	if ((int)__builtin_expect(7, 0) != 7) fails++;
	__builtin_assume(se() > 0);           /* operand unevaluated */
	if (calls != 0) fails++;
	if (!fails) { int x = 1; if (!x) __builtin_unreachable(); }  /* no -Wunused-value */
	printf("expect_assume fails=%d\n", fails);
	return fails;
}
