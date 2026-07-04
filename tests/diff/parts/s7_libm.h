void s7_6_fenv_test(void) {
	feclearexcept(FE_ALL_EXCEPT);
	printf("fenv clear: %d\n", fetestexcept(FE_ALL_EXCEPT) == 0);
	feraiseexcept(FE_INVALID);
	printf("fenv raise inv: %d\n", (fetestexcept(FE_INVALID) & FE_INVALID) != 0);
	feclearexcept(FE_INVALID);
	printf("fenv cleared inv: %d\n", (fetestexcept(FE_INVALID) & FE_INVALID) == 0);
	int prev = fegetround();
	printf("fenv round set: %d\n", fesetround(FE_TOWARDZERO) == 0 && fegetround() == FE_TOWARDZERO);
	fesetround(prev);
	printf("fenv round restored: %d\n", fegetround() == prev);
	feclearexcept(FE_ALL_EXCEPT);
}

void s7_1_complex_libm_test(void) {
	double complex z = 3.0 + 4.0 * I;
	printf("cabs: %d\n", cabs(z) == 5.0);
	printf("carg: %d\n", carg(I) > 1.57 && carg(I) < 1.58);
	printf("cexp0: %d\n", creal(cexp(0.0 * I)) == 1.0 && cimag(cexp(0.0 * I)) == 0.0);
	double complex s = csqrt(-1.0 + 0.0 * I);
	printf("csqrt-1: %d\n", creal(s) == 0.0 && cimag(s) == 1.0);
	printf("conj: %d\n", creal(conj(z)) == 3.0 && cimag(conj(z)) == -4.0);
	printf("cproj: %d\n", creal(cproj(z)) == 3.0);
}

void s7_23_tgmath_eval_test(void) {
	double d = 4.0;
	float f = 4.0f;
	double complex c = 3.0 + 4.0 * I;
	printf("tg sqrt d: %d\n", sqrt(d) == 2.0);
	printf("tg sqrt f: %d\n", sqrt(f) == 2.0f);
	printf("tg pow: %d\n", pow(d, 3.0) == 64.0);
	printf("tg cabs(complex via sqrt->real): %d\n", (double)cabs(c) == 5.0);
	printf("tg fabs: %d\n", fabs(-2.5) == 2.5);
}
