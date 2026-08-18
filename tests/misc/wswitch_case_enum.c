/* T-mac-30215: under -Wswitch, a case label whose value is not a member of the
 * controlling enumerated type must be diagnosed (gcc/clang do; mcc only warned
 * the mirror direction, enum-value-not-handled). `case 99` on `enum E{A,B,C}`
 * must warn "case value '99' not in enumerated type 'enum E'". */
enum E { A, B, C };
int f(enum E e) {
	switch (e) {
	case A: return 1;
	case 99: return 2;
	default: return 0;
	}
}
