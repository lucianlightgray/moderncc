/* AST replay: passing a struct by value as a call argument — gfunc_call copies
 * the aggregate to the outgoing argument slot, and replay re-runs it (docs/AST.md
 * §8). Both the caller (`sum(a)`) and the by-value-param callee (`sum`) replay. */
struct P { int x, y; };

static int sum(struct P p) { return p.x + p.y; }

int main(void) {
	struct P a;
	a.x = 40;
	a.y = 2;
	return sum(a); /* 42 */
}
