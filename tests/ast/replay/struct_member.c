/* AST replay: scalar struct member access — `.` on a local struct and `->`
 * through a pointer, as both read and write — replays via the coarse-grained
 * Unary(MEMBER) capture (docs/AST.md §A3 aggregates). */
struct P { int x, y; };

static int sum(struct P *p) { return p->x + p->y; } /* arrow reads */

int main(void) {
	struct P p;
	p.x = 30;     /* dot write (local struct) */
	p.y = 12;
	p.x += p.y;   /* dot read + write = 42 */
	return sum(&p) - p.y; /* (42+12) - 12 = 42 */
}
