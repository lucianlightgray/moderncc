#include <complex.h>

static double _Complex mk(double r) {
	return r + 2.0i;
}

int main(void) {
	double _Complex z = mk(40.0);
	return (int)(__real__ z + __imag__ z);
}
