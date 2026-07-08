/* AST replay: _Complex *construction* from the imaginary unit `I`
 * (`re + im*I`, docs/AST.md §18.3). `I` is `__builtin_complex(0.0f, 1.0f)` — a
 * rodata _Complex constant. ast_hook_builtin_complex_begin/end suppress the two
 * scalar-const arg pushes + rodata materialization and capture the const as one Ref
 * leaf (its anon Sym persists across discard/replay). The float->double _Complex
 * widening cast also materializes a rodata const; its ELF symbol index is recorded
 * ordinally (ast_fconst reuse) so replay's relocation is byte-identical. REPLAYED
 * targets mk. */
#include <complex.h>

static double _Complex mk(double re, double im) {
	return re + im * I; /* build a complex from real + imaginary parts */
}

int main(void) {
	double _Complex z = mk(40.0, 2.0);
	return (int)(__real__ z + __imag__ z); /* 40 + 2 = 42 */
}
