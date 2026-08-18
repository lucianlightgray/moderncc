/* T-mac-30130 slice-2 known-positive: distinct __label__ names, and the same
 * name shadowed in a nested block, are valid. Exit 0. */
int main(void) {
	__label__ a, b;
	{ __label__ a; a: ; (void)&&a; }
	goto a;
a:
	goto b;
b:
	return 0;
}
