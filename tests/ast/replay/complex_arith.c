#include <complex.h>

static double addre(double _Complex a, double _Complex b) {
	return __real__(a + b);
}

int main(void) {
	double _Complex a = 20.0 + 1.0 * I, b = 22.0 + 5.0 * I;
	return (int)addre(a, b);
}
