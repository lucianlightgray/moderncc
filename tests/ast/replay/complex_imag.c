/* AST replay: an imaginary literal (`2.0i`) builds a `0 + val*i` _Complex pair.
 * The construction (gen_imaginary_complex) is suppressed during capture and folded
 * into one Unary(AST_OP_IMAG) node wrapping the scalar value; replay re-runs it
 * (docs/AST.md §A3 _Complex construction). REPLAYED targets mk. */
#include <complex.h>

static double _Complex mk(double r) { return r + 2.0i; }

int main(void) {
	double _Complex z = mk(40.0);
	return (int)(__real__ z + __imag__ z); /* 40 + 2 = 42 */
}
