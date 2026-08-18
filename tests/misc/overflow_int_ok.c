/* T-mac-30166 known-positive: integer results must still work and compute
 * correctly (2e9 + 2e9 overflows int -> returns 1). */
int main(void) {
	int r;
	int o = __builtin_add_overflow(2000000000, 2000000000, &r);
	unsigned u;
	int o2 = __builtin_mul_overflow(3u, 4u, &u);
	return (o == 1 && o2 == 0 && u == 12) ? 0 : 1;
}
