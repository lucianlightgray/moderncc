#include <stdio.h>
int wsym(void) __attribute__((weak));

int wsym(void) {
	return 99;
}

int main(void) {
	printf("%d\n", wsym());
	return 0;
}
