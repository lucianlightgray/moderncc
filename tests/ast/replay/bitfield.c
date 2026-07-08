/* AST replay: bit-field member read + write. The read-modify-write mask/shift
 * (adjust_bf/load_packed_bf) runs inside the suppressed gv/vstore, so member
 * access + Store + arithmetic on bit-field members all replay (docs/AST.md §3). */
struct F { unsigned a : 3, b : 5, c : 8; int s : 10; };

int main(void) {
	struct F f;
	f.a = 5;   /* bit-field writes */
	f.b = 20;
	f.c = 100;
	f.s = -83; /* signed bit-field */
	return f.a + f.b + f.c + f.s; /* 5+20+100-83 = 42 */
}
