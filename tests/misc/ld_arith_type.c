static long double a = 1, b = 2;
static double d = 3;
static float f = 4;

int main(void) {
	if (_Generic((a + b), long double: 1, default: 0) != 1) return 1;
	if (_Generic((a * b), long double: 1, default: 0) != 1) return 2;
	if (_Generic((a - b), long double: 1, default: 0) != 1) return 3;
	if (_Generic((a / b), long double: 1, default: 0) != 1) return 4;
	if (_Generic((1 ? a : b), long double: 1, default: 0) != 1) return 5;
	if (_Generic((d + a), long double: 1, default: 0) != 1) return 6;
	if (_Generic((f + a), long double: 1, default: 0) != 1) return 7;
	if (_Generic((a + 1.0L), long double: 1, default: 0) != 1) return 8;

	if (_Generic((d + d), double: 1, default: 0) != 1) return 9;
	if (_Generic((d * 3.0), double: 1, default: 0) != 1) return 10;
	if (_Generic((f + f), float: 1, default: 0) != 1) return 11;
	if (_Generic((d + f), double: 1, default: 0) != 1) return 12;

	__typeof__(a + b) x = a + b;
	if (_Generic(x, long double: 1, default: 0) != 1) return 13;

	return 0;
}
