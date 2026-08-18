/* T-mac-30130 slice-2: two __label__ declarations of the same name in one block
 * are a constraint violation — gcc/clang error "duplicate label declaration".
 * mcc silently accepted them. This file must be REJECTED. */
int main(void) {
	__label__ x, x;
	return 0;
}
