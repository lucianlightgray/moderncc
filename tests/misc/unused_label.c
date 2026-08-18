/* T-mac-30130 slice-1: an ordinary label defined but never referenced by a goto
 * or &&label must warn under -Wall (gcc/clang -Wunused-label); mcc only warned
 * unused __label__ (GNU local) declarations. `dead:` here is never targeted. */
int main(void) {
dead:
	return 0;
}
