#include <complex.h>
#include <math.h>
extern int printf(const char *, ...);

static int isnan_(double x) {
	return x != x;
}
static int isinf_(double x) {
	return !isnan_(x) && ((x - x) != (x - x));
}

int main(void) {
	int ok = 1;
	double inf = 1.0;
	inf = inf / (inf - 1.0);

	double _Complex a = CMPLX(1, 2), b = CMPLX(3, 4), z;
	z = a * b;
	if (!(creal(z) == -5 && cimag(z) == 10))
		ok = 0;

	double _Complex c = CMPLX(11, -2);
	z = c / b;
	if (!(creal(z) == 1 && cimag(z) == -2))
		ok = 0;

	double _Complex one = CMPLX(1, 0), infc = CMPLX(inf, inf);
	z = one / infc;
	if (!(creal(z) == 0 && cimag(z) == 0))
		ok = 0;

	double _Complex inf0 = CMPLX(inf, 0);
	z = a * inf0;
	if (!isinf_(creal(z)))
		ok = 0;

	double _Complex zero = CMPLX(0, 0);
	z = one / zero;
	if (!isinf_(creal(z)))
		ok = 0;

	float _Complex fa = CMPLXF(1, 2), fb = CMPLXF(3, 4), fz;
	fz = fa * fb;
	if (!(crealf(fz) == -5 && cimagf(fz) == 10))
		ok = 0;

	long double _Complex la = CMPLXL(1, 2), lb = CMPLXL(3, 4), lz;
	lz = la * lb;
	if (!(creall(lz) == -5 && cimagl(lz) == 10))
		ok = 0;

	double _Complex k = CMPLX(3.0, -4.0);
	if (!(creal(k) == 3.0 && cimag(k) == -4.0))
		ok = 0;

	double _Complex nzz = CMPLX(-0.0, -0.0);
	if (!(signbit(creal(nzz)) && signbit(cimag(nzz))))
		ok = 0;
	double _Complex pzz = CMPLX(0.0, 0.0);
	if (!(!signbit(creal(pzz)) && !signbit(cimag(pzz))))
		ok = 0;
	double _Complex mzz = CMPLX(-0.0, 0.0);
	if (!(signbit(creal(mzz)) && !signbit(cimag(mzz))))
		ok = 0;

	double _Complex kinf = CMPLX(inf, -inf);
	if (!(isinf_(creal(kinf)) && creal(kinf) > 0 && isinf_(cimag(kinf)) &&
				cimag(kinf) < 0))
		ok = 0;

	double _Complex u = CMPLX(3.0, 5.0);
	double _Complex ui = u * I;
	if (!(creal(ui) == -5.0 && cimag(ui) == 3.0))
		ok = 0;
	double _Complex udi = u / I;
	if (!(creal(udi) == 5.0 && cimag(udi) == -3.0))
		ok = 0;
	double _Complex ur = u * 2.0;
	if (!(creal(ur) == 6.0 && cimag(ur) == 10.0))
		ok = 0;
	double _Complex urd = u / 2.0;
	if (!(creal(urd) == 1.5 && cimag(urd) == 2.5))
		ok = 0;

	double _Complex ii = I * I;
	if (!(creal(ii) == -1.0 && cimag(ii) == 0.0))
		ok = 0;

	double _Complex w = CMPLX(2.0, 3.0);
	double _Complex cw = CMPLX(creal(w), -cimag(w));
	if (!(creal(cw) == 2.0 && cimag(cw) == -3.0))
		ok = 0;
	double _Complex sumw = w + cw;
	if (!(creal(sumw) == 4.0 && cimag(sumw) == 0.0))
		ok = 0;
	double _Complex difw = w - cw;
	if (!(creal(difw) == 0.0 && cimag(difw) == 6.0))
		ok = 0;
	double _Complex mulw = w * cw;
	if (!(creal(mulw) == 13.0 && cimag(mulw) == 0.0))
		ok = 0;

	double _Complex az = CMPLX(1.0, 2.0), bz = CMPLX(3.0, 4.0);
	double _Complex prod = az * bz;
	double _Complex expect = CMPLX(1.0 * 3.0 - 2.0 * 4.0, 1.0 * 4.0 + 2.0 * 3.0);
	if (!(creal(prod) == creal(expect) && cimag(prod) == cimag(expect)))
		ok = 0;

	float _Complex fc = CMPLXF(1.5f, -2.25f);
	double _Complex dc = fc;
	if (!(creal(dc) == 1.5 && cimag(dc) == -2.25))
		ok = 0;
	long double _Complex lc = dc;
	if (!(creall(lc) == 1.5L && cimagl(lc) == -2.25L))
		ok = 0;
	double _Complex backc = lc;
	if (!(creal(backc) == 1.5 && cimag(backc) == -2.25))
		ok = 0;
	float _Complex backf = lc;
	if (!(crealf(backf) == 1.5f && cimagf(backf) == -2.25f))
		ok = 0;

	double _Complex nz = CMPLX(-0.0, 0.0);
	double _Complex pz = CMPLX(0.0, 0.0);
	double _Complex sz = nz + pz;
	if (signbit(creal(sz)))
		ok = 0;
	double _Complex sz2 = pz + pz;
	if (signbit(creal(sz2)) || signbit(cimag(sz2)))
		ok = 0;

	double _Complex infr = CMPLX(inf, 0.0);
	double _Complex mres = infr * CMPLX(2.0, 0.0);
	if (!isinf_(creal(mres)))
		ok = 0;

	double _Complex e1 = CMPLX(1.0, 2.0), e2 = CMPLX(1.0, 2.0);
	double _Complex e3 = CMPLX(1.0, 3.0);
	if (!(e1 == e2))
		ok = 0;
	if (!(e1 != e3))
		ok = 0;
	if (e1 == e3)
		ok = 0;
	if (e1 != e2)
		ok = 0;

	printf(ok ? "OK\n" : "FAIL\n");
	return ok ? 0 : 1;
}
