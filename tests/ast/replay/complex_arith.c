/* AST replay: _Complex arithmetic and __real__/__imag__ extraction. A _Complex
 * operand routes to gen_complex_op inside the suppressed gen_op, its result temp
 * is an ordinal frame slot (cplx_local wraps ast_alloc_loc), and __real__/__imag__
 * is captured via the coarse member hook (docs/AST.md §A3). (REPLAYED targets
 * addre; main falls back on the _Complex construction `re + im*I`.) */
#include <complex.h>

static double addre(double _Complex a, double _Complex b) {
	return __real__(a + b); /* complex add + real-part extract */
}

int main(void) {
	double _Complex a = 20.0 + 1.0 * I, b = 22.0 + 5.0 * I;
	return (int)addre(a, b); /* real part of 42+6i = 42 */
}
