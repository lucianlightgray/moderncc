#include <stdio.h>
int main(void) {
	int ok = 1;
	int n = __builtin_printf_unlocked("A%d\n", 1);
	ok &= (n == 3);
	__builtin_fprintf_unlocked(stdout, "B%d\n", 2);
	__builtin_fputs_unlocked("C3\n", stdout);
	printf("%s\n", ok ? "UNLOCKED_OK" : "UNLOCKED_FAIL");
	return ok ? 0 : 1;
}
