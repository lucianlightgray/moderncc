/* AST replay: float/double constants, local stores, arithmetic, int<->fp casts,
 * and a float comparison all replay faithfully (docs/AST.md §A3 floats —
 * including the const-pool reuse that keeps the relocation byte-identical). */
int main(void) {
	double a = 2.5;             /* float local store + const-pool const */
	double b = a * 4.0 + 1.0;   /* float arith with constants = 11.0    */
	float f = 3.0f;
	int n = (int)(b + f);       /* fp add + double->int cast = 14       */
	if (b > 10.0)               /* float comparison in a condition      */
		n += 1;                 /* 15                                   */
	return n;
}
