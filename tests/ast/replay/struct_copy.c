/* AST replay: struct assignment/copy — direct (`g1 = g2`) and through pointers
 * (`*a = *b`, `q = *b`) — plus `(*p).x` member access on a dereferenced struct
 * pointer. Struct→struct vstore records a Store; replay reproduces the aggregate
 * copy (memmove / gen_struct_copy). Deref-to-struct is a reconstructable lvalue
 * (docs/AST.md §A3 aggregates). */
struct P { int x, y; };

static void copy(struct P *a, struct P *b) { *a = *b; }

int main(void) {
	struct P s, d;
	s.x = 40;
	s.y = 2;
	copy(&d, &s);        /* d = s via *a = *b */
	struct P q = d;      /* direct struct copy */
	return (*&q).x + q.y; /* (*p).x + member = 42 */
}
