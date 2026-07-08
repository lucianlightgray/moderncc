/* AST replay: calling a struct-returning function and using the result
 * (`struct r = mk(...)`, register-return ABI). The post-call register->temp
 * reconstruction is reproduced at replay with an ordinal frame slot (ast_alloc_loc)
 * so the temp offset matches the parse-build (docs/AST.md §8 / §A3). */
struct P { int x, y; };

static struct P mk(int a, int b) {
	struct P p;
	p.x = a;
	p.y = b;
	return p;
}

int main(void) {
	struct P r = mk(40, 2);
	return r.x + r.y; /* 42 */
}
