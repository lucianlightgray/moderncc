/* AST replay: variable-length arrays (`int a[n]`) — the VLA lexical-scope-edge
 * query (docs/AST.md §4/§18.3). The runtime size computation is an ordinary
 * captured Store; the machine-tier alloc sequence (gen_vla_sp_save(locorig),
 * vpush_type_size + gen_vla_alloc, gen_vla_sp_save(addr)) is captured as one coarse
 * Unary(AST_OP_VLA) effect, and the paired SP restore at the scope edge as either a
 * Return annotation (function-scope) or an AST_OP_VLA_RESTORE effect (a nested
 * block's `}`). Replay re-issues the exact ops with the captured frame-slot offsets
 * (no loc decrement — the frame size stays parse-final). REPLAYED targets sum. */
static int sum(int n) {
	int a[n];       /* 1-D VLA */
	int i, s = 0;
	for (i = 0; i < n; i++)
		a[i] = i + 1;
	{
		int b[n];   /* nested-block VLA — its SP restore fires at the inner `}` */
		for (i = 0; i < n; i++)
			b[i] = a[i] * 2;
		for (i = 0; i < n; i++)
			s += b[i];
	}
	for (i = 0; i < n; i++)
		s += a[i];
	return s; /* (2+4+..+2n) + (1+2+..+n) = 3*(n(n+1)/2); n=8 -> 3*36 = 108 */
}

int main(void) {
	return sum(8) - 66; /* 108 - 66 = 42 */
}
