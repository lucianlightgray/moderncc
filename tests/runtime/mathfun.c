#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 1500000;
	int i;
	double acc = 0.0;
	float facc = 0.0f;
	for (i = 1; i <= n; i++) {
		double x = i * 1e-4;
		double y = (i % 977) * 0.125 - 60.0;
		float fx = (float)(i % 613) * 0.25f - 76.0f;
		acc += sqrt(x) + fabs(y);
		acc += copysign(x, y) + (y < 0.0 ? -y : y);
		acc += floor(y * 0.5) + ceil(y * 0.25);
		acc += fmin(x, 3.0) + fmax(y, -1.0);
		acc += sqrt(fabs(y) + 1.0) * 0.5;
		facc += sqrtf(fabsf(fx)) + copysignf(1.0f, fx);
		facc += fminf(fx, 2.0f) + fmaxf(fx, -2.0f);
	}
	printf("mathfun %.9f %.6f\n", acc, (double)facc);
	return 0;
}
