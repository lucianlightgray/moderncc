/* D1d — Comment promotion (docs/CST.md §D1d): line, inline and block comments
 * become first-class CST_Comment leaf nodes, excluded from H_s but folded into
 * H_t so a comment-only edit leaves the structural hash fixed (§8.4). */
// a line comment
int commented(int x /* inline */) {
	/* a block comment
	   spanning several
	   lines */
	return x + 1; // trailing line comment
}
