/* Regression T-mac-30234: _Alignof of a variably-modified (VLA) type must be
 * the alignment of its innermost element type, NOT the pointer size. A VLA is
 * represented VT_PTR|VT_VLA (no VT_ARRAY), so type_size() fell into the plain-
 * pointer branch and reported alignment 8 for every VLA: _Alignof(int[n]) gave
 * 8 where gcc-16/clang give 4, _Alignof(char[n]) gave 8 not 1, etc. sizeof(vla)
 * was already correct (it goes through vpush_type_size, which walks the element
 * type). The fix walks the _Alignof operand through all array/VLA levels to the
 * innermost element and takes its alignment. Golden values verified against
 * gcc-16 and clang on this host (they agree on every case below; long double
 * alignment differs between them, so that case is checked self-consistently
 * against _Alignof(long double)). Exit 0 only when every case matches. */

int main(void) {
	int n = 5, m = 3;

	/* Scalar element types: the whole point of the bug. */
	if (_Alignof(char[n])  != 1) return 1;
	if (_Alignof(short[n]) != 2) return 2;
	if (_Alignof(int[n])   != 4) return 3;
	if (_Alignof(double[n]) != 8) return 4;   /* was "correct" only by coincidence */

	/* Nested arrays/VLAs: the fix must walk through every array/VLA level. */
	if (_Alignof(int[n][3]) != 4) return 5;   /* VLA whose element is int[3] (array) */
	if (_Alignof(int[3][n]) != 4) return 6;   /* array-of-VLA (VM overall) */
	if (_Alignof(int[n][m]) != 4) return 7;   /* VLA of VLA -> must recurse */

	/* Pointer element: walk stops at the pointer (VT_PTR, not array/VLA). */
	if (_Alignof(char *[n]) != _Alignof(char *)) return 8;

	/* long double alignment is host-ABI-dependent (gcc 16 vs clang 8 here);
	 * the VLA form must match the scalar form whatever that value is. */
	if (_Alignof(long double[n]) != _Alignof(long double)) return 9;

	/* sizeof of the same VLA types must stay correct (element size * count). */
	if (sizeof(int[n])  != (unsigned long)n * sizeof(int)) return 10;
	if (sizeof(char[n]) != (unsigned long)n)               return 11;

	/* Fixed (non-VLA, VT_ARRAY) arrays must be unaffected by the fix. */
	if (_Alignof(int[4])  != 4) return 12;
	if (_Alignof(char[7]) != 1) return 13;

	return 0;
}
