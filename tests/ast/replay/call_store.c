/* AST replay: storing a call result straight into a local — both an initializer
 * (`int a = f();`) and an assignment (`b = f();`) — replays. Regression guard
 * for the vpop double-emit bug (the store's leftover rvalue must not be re-added
 * as a bare BasicBlock effect). */
static int add1(int x) { return x + 1; }

int main(void) {
	int a = add1(41); /* init from a call = 42 */
	int b;
	b = add1(a);      /* assign from a call = 43 */
	return b - 1;     /* 42 */
}
