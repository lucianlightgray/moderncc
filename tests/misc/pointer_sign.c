/* T-mac-30193: assigning between integer pointers that differ only in target
 * signedness must warn under -Wall (gcc/clang -Wpointer-sign); mcc silently
 * accepted it. `unsigned int *u = &i;` where i is int must warn "pointer
 * targets ... differ in signedness". */
int main(void) {
	int i = 0;
	unsigned int *u = &i;
	return u != 0;
}
