/* Regression: casting the address of a weak symbol to _Bool must NOT fold to a
 * constant 1. A weak symbol may resolve to address 0 at link time, so
 * (_Bool)&weak_sym has to be evaluated at run time. mcc's gen_cast folded any
 * (VT_CONST|VT_SYM) address to 1 without checking the weak bit (T-mac-30042);
 * every other symbol-address fold in mccgen.c already guards on !sym->a.weak.
 * Exit 0 only when both cases are correct. */

int absent_weak(void) __attribute__((weak));
int present_weak(void) __attribute__((weak));
int present_weak(void) {
	return 7;
}

int main(void) {
	_Bool a = (_Bool)&absent_weak;  /* unresolved weak -> address 0 -> 0 */
	_Bool p = (_Bool)&present_weak; /* defined            -> nonzero   -> 1 */
	if (a != 0)
		return 1;
	if (p != 1)
		return 2;
	return 0;
}
