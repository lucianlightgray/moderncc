/* Safety-net: `long double` is not modeled (ast_bad_type), so the function must
 * fall back to correct -O0 emission without faithfully replaying — proving the
 * byte-verify net (docs/AST.md §17). */
int main(void) {
	long double x = 20.5L;
	return (int)(x + x + 1.0L); /* 42 */
}
