/* AST replay: a && / || result used as a value — stored into a variable
 * (`int r = a&&b`) or used arithmetically (`(a&&b)+1`). The VT_CMP->0/1
 * materialization (setcc) is suppressed during capture, so the Binary(&&/||) node
 * stays and replay re-materializes it when the consuming op runs (docs/AST.md §A3).
 *
 * Also covers NESTED short-circuit operands (`(a&&b)||c`, docs/AST.md §18.3): the
 * inner Binary(&&/||) rides as a child of the outer chain, and replay's AST_Binary
 * case recurses — the inner short-circuit renders its own gvtst chain into a VT_CMP
 * that the outer chain then gvtst's, exactly as the parser does. A nested short-
 * circuit used as a branch condition, and a bare-global condition (`if (g)`, whose
 * leaf ->sym is now finalized so replay reconstructs it), are exercised too. */
int gv = 0;

int main(void) {
	int a = 1, b = 3, c = 0;
	int r = a && b;        /* stored bool: 1 */
	int s = (a && b) + 40; /* bool in arithmetic: 41 */

	if (((a && b) || c) != 1) return 91;      /* nested operand as a value */
	if (((a && gv) || (a && !c)) != 1) return 92; /* nested, mixed globals */

	if ((a && b) || gv)    /* nested short-circuit as a branch condition */
		if (!gv)           /* bare-global condition nested in the then-branch */
			return r + s;  /* 1 + 41 = 42 */
	return 99;
}
