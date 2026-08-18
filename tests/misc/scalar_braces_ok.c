/* T-mac-30186 known-positive: a single brace around a scalar initializer is
 * valid and must stay clean under -Wall -Werror. */
int main(void) {
	int a = {5};
	int b = 7;
	return (a == 5 && b == 7) ? 0 : 1;
}
