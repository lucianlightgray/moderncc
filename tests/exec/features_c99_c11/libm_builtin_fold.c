#include <errno.h>
#include <math.h>
#include <stdio.h>

static volatile double d625 = 6.25, dm625 = -6.25, d25 = 2.5, dm25 = -2.5,
											 d3 = 3.0, d7 = 7.0, d75 = 7.5, dm1 = -1.0;
static volatile float f625 = 6.25f, fm625 = -6.25f, f25 = 2.5f, fm25 = -2.5f,
											f3 = 3.0f, f7 = 7.0f, f75 = 7.5f;

int main(void) {
	int ok = 1;

	printf("%g %g %g %g\n", sqrt(6.25), fabs(-6.25), floor(2.5), ceil(2.5));
	printf("%g %g %g %g\n", trunc(2.5), copysign(3.0, 7.0), fmin(2.5, 7.5),
				 fmax(2.5, 7.5));
	printf("%g %g %g %g\n", (double)sqrtf(6.25f), (double)fabsf(-6.25f),
				 (double)floorf(2.5f), (double)ceilf(2.5f));
	printf("%g %g %g %g\n", (double)truncf(2.5f), (double)copysignf(3.0f, 7.0f),
				 (double)fminf(2.5f, 7.5f), (double)fmaxf(2.5f, 7.5f));

	if (sqrt(6.25) != sqrt(d625))
		ok = 0;
	if (fabs(-6.25) != fabs(dm625))
		ok = 0;
	if (floor(2.5) != floor(d25))
		ok = 0;
	if (ceil(2.5) != ceil(d25))
		ok = 0;
	if (trunc(2.5) != trunc(d25))
		ok = 0;
	if (trunc(-2.5) != trunc(dm25))
		ok = 0;
	if (copysign(3.0, 7.0) != copysign(d3, d7))
		ok = 0;
	if (fmin(2.5, 7.5) != fmin(d25, d75))
		ok = 0;
	if (fmax(2.5, 7.5) != fmax(d25, d75))
		ok = 0;

	if (sqrtf(6.25f) != sqrtf(f625))
		ok = 0;
	if (fabsf(-6.25f) != fabsf(fm625))
		ok = 0;
	if (floorf(2.5f) != floorf(f25))
		ok = 0;
	if (ceilf(2.5f) != ceilf(f25))
		ok = 0;
	if (truncf(2.5f) != truncf(f25))
		ok = 0;
	if (truncf(-2.5f) != truncf(fm25))
		ok = 0;
	if (copysignf(3.0f, 7.0f) != copysignf(f3, f7))
		ok = 0;
	if (fminf(2.5f, 7.5f) != fminf(f25, f75))
		ok = 0;
	if (fmaxf(2.5f, 7.5f) != fmaxf(f25, f75))
		ok = 0;

#if defined(math_errhandling) && defined(MATH_ERRNO)
	int check_errno = (math_errhandling & MATH_ERRNO) != 0;
#else
	int check_errno = 0;
#endif
	errno = 0;
	double sv = sqrt(dm1);
	if (sv == sv || (check_errno && errno != EDOM))
		ok = 0;
	errno = 0;
	double sc = sqrt(-1.0);
	if (sc == sc || (check_errno && errno != EDOM))
		ok = 0;

	printf("%s\n", ok ? "OK" : "FAIL");
	return 0;
}
