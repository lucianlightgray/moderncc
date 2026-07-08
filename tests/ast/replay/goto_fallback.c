/* Safety-net: `goto` is not yet modeled, so the function must fall back to correct
 * -O0 emission (same exit code) without faithfully replaying — proving the
 * byte-verify net (docs/AST.md §17). */
int main(void) {
	int i = 0;
again:
	i++;
	if (i < 42)
		goto again;
	return i;
}
