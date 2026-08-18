/* T-mac-30215 known-positive: a switch whose case labels are all enumerators
 * (or ranges of them) must NOT warn under -Wswitch, even with -Werror. */
enum E { A, B, C };
int f(enum E e) {
	switch (e) {
	case A: case B: case C: return 1;
	}
	return 0;
}
int main(void) { return f(A) == 1 ? 0 : 1; }
