/* shadow.c — CST symbol-ref shadowing boundary fixture (slice I).
 *
 * A name declared at file scope and again inside a function. Correct scoped
 * resolution would map the two uses of `x` to *different* defs:
 *   - `outer`'s `return x;`  -> the local `int x = 2;`
 *   - `reader`'s `return x;` -> the global `int x = 1;`
 *
 * The CST resolver (slice I) keys def offsets by identifier token id with a
 * single slot (cst_defoff[v]), so a redeclaration overwrites the previous def:
 * "last-declaration-wins", with no scope stack. Both uses therefore resolve to
 * the SAME def (the last-declared local `x`). symref-shadow.cmake pins that
 * behavior as the documented v1 boundary — building a scope stack (LSP-era)
 * would split the two mappings and flip the assertion. */

int x = 1;

int outer(void) {
	int x = 2;
	return x;
}

int reader(void) {
	return x;
}
