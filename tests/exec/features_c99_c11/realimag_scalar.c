#include <stdio.h>

int main(void) {
	double r = 3.5;
	int i = 42;
	printf("%g %g\n", __real__ r, __imag__ r);
	printf("%d %d\n", __real__ i, __imag__ i);
	return 0;
}
