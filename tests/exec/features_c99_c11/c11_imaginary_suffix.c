#include <complex.h>
extern int printf(const char *, ...);

double _Complex g_di = 1.0i;
double _Complex g_dj = 2.0j;
double _Complex g_dI = 3.0I;
double _Complex g_dJ = 4.0J;

float _Complex g_fi = 1.0fi;
float _Complex g_if = 2.0if;
float _Complex g_iF = 3.0iF;
float _Complex g_jf = 4.0jf;
float _Complex g_Fj = 5.0Fj;
float _Complex g_IF = 6.0IF;

long double _Complex g_Li = 1.0Li;
long double _Complex g_iL = 2.0iL;

double _Complex g_int = 7i;
double _Complex g_intu = 8ui;
double _Complex g_intui = 9iu;
double _Complex g_intl = 10li;
double _Complex g_intil = 11il;

double _Complex g_hex = 0x1p1i;
double _Complex g_hexfi = 0x1p2fi;
double _Complex g_hexif = 0x1p3if;

double _Complex g_sum = 2.0 + 3.0iF;
double _Complex g_mul = 4.0 + 5.0 * _Complex_I;

static int eq(double _Complex z, double re, double im) {
	return __real__ z == re && __imag__ z == im;
}

static int eqf(float _Complex z, double re, double im) {
	return (double)__real__ z == re && (double)__imag__ z == im;
}

static int eql(long double _Complex z, double re, double im) {
	return (double)__real__ z == re && (double)__imag__ z == im;
}

static int local_scope(void) {
	double _Complex a = 1.0i;
	float _Complex b = 2.0if;
	long double _Complex c = 3.0iL;
	double _Complex d = 4i;
	double _Complex e = 5.0 + 6.0jf;
	return eq(a, 0, 1) && eqf(b, 0, 2) && eql(c, 0, 3) && eq(d, 0, 4) && eq(e, 5, 6);
}

int main(void) {
	int ok = 1;
	if (!eq(g_di, 0, 1) || !eq(g_dj, 0, 2) || !eq(g_dI, 0, 3) || !eq(g_dJ, 0, 4))
		ok = 0;
	if (!eqf(g_fi, 0, 1) || !eqf(g_if, 0, 2) || !eqf(g_iF, 0, 3))
		ok = 0;
	if (!eqf(g_jf, 0, 4) || !eqf(g_Fj, 0, 5) || !eqf(g_IF, 0, 6))
		ok = 0;
	if (!eql(g_Li, 0, 1) || !eql(g_iL, 0, 2))
		ok = 0;
	if (!eq(g_int, 0, 7) || !eq(g_intu, 0, 8) || !eq(g_intui, 0, 9))
		ok = 0;
	if (!eq(g_intl, 0, 10) || !eq(g_intil, 0, 11))
		ok = 0;
	if (!eq(g_hex, 0, 2) || !eq(g_hexfi, 0, 4) || !eq(g_hexif, 0, 8))
		ok = 0;
	if (!eq(g_sum, 2, 3) || !eq(g_mul, 4, 5))
		ok = 0;
	if (!local_scope())
		ok = 0;
	printf(ok ? "OK\n" : "FAIL\n");
	return ok ? 0 : 1;
}
