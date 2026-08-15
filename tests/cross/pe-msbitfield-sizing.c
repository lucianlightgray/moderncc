/* pe/ms-bitfield-sizing (T-win-50015 gap a): mcc's -mms-bitfields struct/union
 * sizing must agree byte-for-byte with a native Windows compiler. mingw-gcc
 * defaults to the MS bit-field layout on a Windows target, so it is the oracle.
 *
 * The gap-a regression: a union whose only member is a zero-width bit-field
 * (`union {int :0;}`) contributes 0 to size on every native compiler; mcc's
 * ms-mode over-sized it to sizeof(int)=4, inflating the enclosing struct from
 * 8 to 12. The `outer` struct below is the pe/torture-classes shape that caught
 * it; the others are controls that must not move. Printed, not static-asserted,
 * so the .sh can diff mcc's line against mingw's rather than bake ABI constants.
 */
int printf(const char *, ...);

struct outer {
	struct inner {
		signed int a : 3;
		signed int b : 5;
	} in;
	union empty {
		int : 0;
	} e;
	signed int c : 9;
};

union just_zero {
	int : 0;
};

struct differently_typed {
	char h;
	int w : 3;
	char t;
};

struct same_type_run {
	int a : 5;
	int b : 5;
	int c : 5;
};

int main(void) {
	printf("outer %d\n", (int)sizeof(struct outer));
	printf("just_zero %d\n", (int)sizeof(union just_zero));
	printf("differently_typed %d\n", (int)sizeof(struct differently_typed));
	printf("same_type_run %d\n", (int)sizeof(struct same_type_run));
	return 0;
}
