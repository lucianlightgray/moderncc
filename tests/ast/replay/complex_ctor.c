#include <complex.h>

static double _Complex mk(double re, double im) {
	return re + im * I;
}

int main(void) {
	double _Complex z = mk(40.0, 2.0);
	return (int)(__real__ z + __imag__ z);
}
