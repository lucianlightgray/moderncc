/* dg-error: constant expression expected */
/* T-mac-30022 slice-3.  C23 6.10.3.2: the #embed `limit`/`offset` parameters
 * take a constant-expression, not just a single integer token.  mcc now
 * captures the parenthesized, macro-expanded token sequence and evaluates it
 * through the integer-constant-expression evaluator (so `limit(2+2)`,
 * `limit(1<<10)`, `limit(MACRO)` work), which also means a non-constant
 * operand is refused by the evaluator ("constant expression expected") rather
 * than by the old single-token type check.  The param is diagnosed before the
 * embed file is opened, so the missing file is irrelevant. */
int g;
int f(void)
{
	static unsigned char d[] = {
#embed "no_such_embed_file.bin" limit(g)
	};
	return (int)sizeof d;
}
