#include <stdio.h>
static int enter_mid(int start) {
	int sum = 0;
	void *entry = &&body;
	int i = start;
	goto *entry;
	for (i = 0; i < 5; i++) {
	body:
		sum += i;
	}
	return sum;
}
int main(void) {
	int fail = 0;
	if (enter_mid(2) != 9) fail = 1;
	if (enter_mid(4) != 4) fail = 1;
	puts(fail ? "FAIL" : "OK");
	return fail;
}
