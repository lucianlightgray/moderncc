/* AST replay: a && / || result used as a value — stored into a variable
 * (`int r = a&&b`) or used arithmetically (`(a&&b)+1`). The VT_CMP->0/1
 * materialization (setcc) is suppressed during capture, so the Binary(&&/||) node
 * stays and replay re-materializes it when the consuming op runs (docs/AST.md §A3).
 * (Nested short-circuit operands like `(a&&b)||c` still bail.) */
int main(void) {
	int a = 1, b = 3, c = 0;
	int r = a && b;        /* stored bool: 1 */
	int s = (a && b) + 40; /* bool in arithmetic: 41 */
	return r + s - (c || a ? 0 : 99); /* 1 + 41 - 0 = 42 */
}
