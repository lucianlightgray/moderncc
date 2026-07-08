/* AST replay: calling a large struct-returning function via the sret
 * hidden-pointer ABI (ret_nregs==0). The caller allocates the result temp (an
 * ordinal frame slot, ast_alloc_loc) and passes its pointer; replay reserves the
 * same slot and re-pushes the captured result (docs/AST.md §8 / §A3). */
struct Big { int a, b, c, d, e; }; /* 20 bytes > 16 -> sret hidden pointer */

static struct Big mk(int v) {
	struct Big b;
	b.a = v;
	b.b = v;
	b.c = v;
	b.d = v;
	b.e = v;
	return b;
}

int main(void) {
	return mk(42).a; /* 42, via an sret call + direct member access */
}
