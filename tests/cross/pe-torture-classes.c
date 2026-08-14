/* Faithful reconstructions of the Windows-specific gcc c-torture divergence
   classes the wt/winspec pilot named (the exact corpus is not vendored here).
   mcc must agree with a Win64 reference (mingw-gcc) on each. The two classes
   not checked here are covered elsewhere: 20101011-1 (integer divide-by-zero
   raising a catchable exception) by pe/seh, and pr36321 (__builtin_alloca(0)
   returning NULL vs non-NULL) which is implementation-defined and documented,
   not a miscompile. */
#include <stdio.h>
#include <stdarg.h>

/* pr92904: an over-aligned aggregate passed through Win64 varargs */
typedef struct {
	__attribute__((aligned(16))) long long a, b;
} S16;
static long long vsum(int n, ...) {
	va_list ap;
	long long t = 0;
	int i;
	va_start(ap, n);
	for (i = 0; i < n; i++) {
		S16 s = va_arg(ap, S16);
		t += s.a + s.b;
	}
	va_end(ap);
	return t;
}

/* pr23324: signed bitfields in a nested struct next to an empty union */
struct inner {
	signed int a : 3;
	signed int b : 5;
};
union empty {
	int : 0;
};
struct outer {
	struct inner in;
	union empty e;
	signed int c : 9;
};

int main(void) {
	S16 x = {10, 20}, y = {3, 9};
	long long va = vsum(2, x, y);

	struct outer o;
	o.in.a = -1;
	o.in.b = -3;
	o.c = -5;

	/* pr123864: __builtin_mul_overflow_p / __builtin_mul_overflow */
	long long a = 1000000000LL;
	unsigned u = ~0U;
	int ovp = __builtin_mul_overflow_p(a, (long long)u, (long long)0);
	long long r = 0;
	int ov = __builtin_mul_overflow(a, (long long)u, &r);

	printf("va=%lld bf=%d,%d,%d sz=%d ovp=%d ov=%d r=%lld\n",
				 va, o.in.a, o.in.b, o.c, (int)sizeof(struct outer), ovp, ov, r);
	return 0;
}
