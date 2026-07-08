/* C11 §7.3.9.3 (CMPLX) + Annex G special-value edges for the <complex.h>
 * builder macros and cproj/cabs/conj. CMPLX(x,y) must build a complex with real
 * part x and imaginary part y *exactly*, even when a part is NaN or infinity
 * (unlike `x + y*I`, which would contaminate). cabs of anything with an infinite
 * part is +inf; cproj maps any infinite part to (+inf, copysign(0,imag)). All
 * checks are normalized to booleans (isnan/isinf may return any nonzero). */
#include <complex.h>
#include <math.h>
#include <stdio.h>

#define B(x) ((x) ? 1 : 0)

int main(void) {
	int ok = 1;

	/* CMPLX exactness with NaN/inf parts (double / float / long double). */
	double complex z = CMPLX(INFINITY, NAN);
	if (!(B(isinf(creal(z))) && creal(z) > 0 && B(isnan(cimag(z)))))
		ok = 0;
	double complex z2 = CMPLX(0.0, INFINITY);
	if (!(creal(z2) == 0.0 && B(isinf(cimag(z2)))))
		ok = 0;

	float complex zf = CMPLXF(INFINITY, NAN);
	if (!(B(isinf(crealf(zf))) && B(isnan(cimagf(zf)))))
		ok = 0;

	long double complex zl = CMPLXL(NAN, INFINITY);
	if (!(B(isnan(creall(zl))) && B(isinf(cimagl(zl)))))
		ok = 0;

	/* cabs: an infinite part forces +inf, even alongside a NaN part. */
	if (!(B(isinf(cabs(CMPLX(INFINITY, NAN)))) &&
				B(isinf(cabs(CMPLX(NAN, INFINITY))))))
		ok = 0;

	/* conj flips the imaginary sign exactly. */
	double complex c = conj(CMPLX(2.0, 3.0));
	if (!(creal(c) == 2.0 && cimag(c) == -3.0))
		ok = 0;

	/* cproj: any infinite part -> (+inf, copysign(0, imag)). */
	double complex p = cproj(CMPLX(NAN, INFINITY));
	if (!(B(isinf(creal(p))) && creal(p) > 0 && cimag(p) == 0.0))
		ok = 0;
	double complex p2 = cproj(CMPLX(1.0, -INFINITY));
	if (!(B(isinf(creal(p2))) && creal(p2) > 0 && signbit(cimag(p2))))
		ok = 0;

	printf(ok ? "OK\n" : "FAIL\n");
	return 0;
}
