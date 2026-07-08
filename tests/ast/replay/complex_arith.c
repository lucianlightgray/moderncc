/* AST replay: _Complex arithmetic. A _Complex operand routes to gen_complex_op
 * inside the suppressed gen_op, and its result temp is now an ordinal frame slot
 * (cplx_local wraps ast_alloc_loc), so complex +/-/* replay (docs/AST.md §A3).
 * (__real__/__imag__ extraction still falls back — needs a coarse member-style
 * hook — so REPLAYED targets addc, and main falls back.) */
#include <complex.h>

static double _Complex addc(double _Complex a, double _Complex b) {
	return a + b;
}

int main(void) {
	double _Complex z = addc(20.0 + 1.0 * I, 22.0 + 5.0 * I); /* 42 + 6i */
	return (int)__real__ z;                                   /* 42 */
}
