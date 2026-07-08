/* AST replay: a struct-returning function (`return s`) replays — the aggregate
 * return value is captured and replay re-runs gfunc_return (register-return for a
 * small aligned struct, or the sret hidden-pointer copy for a large one). The
 * caller side (`struct P r = make(...)`) still falls back — it allocates a result
 * temp whose `loc` offset diverges on replay — so REPLAYED targets `make`, not
 * main (docs/AST.md §A3 / §8). */
struct P { int x, y; };

struct P make(int a, int b) {
	struct P p;
	p.x = a;
	p.y = b;
	return p;
}

int main(void) {
	struct P r = make(40, 2);
	return r.x + r.y; /* 42 */
}
