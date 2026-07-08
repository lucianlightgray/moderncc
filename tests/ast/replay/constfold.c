/* A7 const-fold template fixture (docs/AST.md §12/§15). Every operand is a
 * compile-time integer constant, so the tree-scope const-fold template collapses
 * the whole return expression to a single Literal. It must (a) still exit with
 * the same value as the -O0 build (template input == output, §15) and (b) with
 * MCC_AST_TEMPLATES on, actually fire the fold (asserted via the dump). The fold
 * is byte-neutral — gen_op already folds these at -O0 — so replay stays faithful. */
int main(void) {
	int a = 2 + 3 * 4;        /* 14 */
	int b = (8 >> 1) + (15 & 6); /* 4 + 6 = 10 */
	int c = 100 / 7 - 100 % 7;   /* 14 - 2 = 12 */
	int d = (1 << 3) | 5;        /* 8 | 5 = 13 */
	return a + b + c + d - 7;    /* 14 + 10 + 12 + 13 - 7 = 42 */
}
