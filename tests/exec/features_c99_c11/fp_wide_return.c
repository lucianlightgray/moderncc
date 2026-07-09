#include <float.h>
#include <math.h>
#include <stdio.h>

static float over_float(void) {
	float x = FLT_MAX;
	return x + x;
}

static double over_double(void) {
	double x = DBL_MAX;
	return x + x;
}

static float third(void) {
	float a = 1.0f, b = 3.0f;
	return a / b;
}

int main(void) {
	int ok = 1;

	ok &= isinf(over_float()) && over_float() > 0;
	ok &= isinf(over_double()) && over_double() > 0;

	float t = third();
	float t2 = (float)(1.0f / 3.0f);
	ok &= (t == t2);
	ok &= ((float)t == t);

	printf(ok ? "OK\n" : "FAIL\n");
	return 0;
}
