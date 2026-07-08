/* AST replay: calling a variadic struct-returning function. The struct-return
 * ABI is independent of variadic (varargs affect argument passing, which
 * gfunc_call reproduces from the callee type), so the caller replays like any
 * other struct-returning call (docs/AST.md §8). REPLAYED targets `use`. */
#include <stdarg.h>

struct P { int x, y; };

static struct P mkv(int n, ...) {
	va_list ap;
	va_start(ap, n);
	struct P p;
	p.x = va_arg(ap, int);
	p.y = n;
	va_end(ap);
	return p;
}

int use(void) {
	struct P p = mkv(5, 37);
	return p.x + p.y; /* 37 + 5 = 42 */
}

int main(void) { return use(); }
