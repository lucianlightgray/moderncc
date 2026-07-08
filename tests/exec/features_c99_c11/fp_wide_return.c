/* C99 §5.1.2.3 / Annex F.6: a function returning float/double must remove any
 * extra range and precision — the returned value is the declared return type,
 * not a wider evaluation-format intermediate. This matters where
 * FLT_EVAL_METHOD == 2 (i386 x87 evaluates in long double): a result that is
 * finite in the wide format but out of range for the return type must narrow.
 * On FLT_EVAL_METHOD == 0 targets (x86_64 SSE, arm64, riscv64) the narrowing is
 * a no-op and the checks hold trivially; on x87 they exercise real narrowing.
 * Mirrors gcc c11-float-*.c wide-return cases / clang C11/n1365.c. Prints OK. */
#include <float.h>
#include <math.h>
#include <stdio.h>

/* FLT_MAX + FLT_MAX is finite in long double but overflows float: the return
   must narrow to +inf regardless of the evaluation format. */
static float over_float(void) {
	float x = FLT_MAX;
	return x + x;
}

/* DBL_MAX + DBL_MAX likewise overflows double. */
static double over_double(void) {
	double x = DBL_MAX;
	return x + x;
}

/* A value whose float-rounded result differs from its long double value:
   after return the caller must see the float-rounded 1/3, and a redundant
   narrowing must be idempotent. */
static float third(void) {
	float a = 1.0f, b = 3.0f;
	return a / b;
}

int main(void) {
	int ok = 1;

	ok &= isinf(over_float()) && over_float() > 0;
	ok &= isinf(over_double()) && over_double() > 0;

	float t = third();
	float t2 = (float)(1.0f / 3.0f);
	ok &= (t == t2);           /* returned value already narrowed to float */
	ok &= ((float)t == t);     /* narrowing the return again is idempotent  */

	printf(ok ? "OK\n" : "FAIL\n");
	return 0;
}
