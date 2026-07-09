void s7_12_classify(void) {
	double dn = 0.0 / 0.0;
	double di = 1.0 / 0.0;
	printf("fpc normal=%d inf=%d nan=%d zero=%d\n",
				 fpclassify(1.0) == FP_NORMAL, fpclassify(di) == FP_INFINITE,
				 fpclassify(dn) == FP_NAN, fpclassify(0.0) == FP_ZERO);
	printf("fpc sub=%d\n", fpclassify(1e-310) == FP_SUBNORMAL);
	printf("isfinite %d %d %d\n", isfinite(1.0) != 0, isfinite(di) != 0, isfinite(dn) != 0);
	printf("isinf %d %d\n", isinf(di) != 0, isinf(1.0) != 0);
	printf("isnan %d %d\n", isnan(dn) != 0, isnan(1.0) != 0);
	printf("isnormal %d %d %d\n", isnormal(1.0) != 0, isnormal(0.0) != 0, isnormal(1e-310) != 0);
	printf("signbit %d %d %d\n", signbit(-3.0) != 0, signbit(3.0) != 0, signbit(-0.0) != 0);
}

void s7_12_trig_hyp(void) {
	printf("trig %d %d %d %d %d %d %d\n",
				 (int)acos(1.0), (int)asin(0.0), (int)atan(0.0), (int)atan2(0.0, 1.0),
				 (int)cos(0.0), (int)sin(0.0), (int)tan(0.0));
	printf("trigf %d %d\n", (int)cosf(0.0f), (int)sinf(0.0f));
	printf("trigl %d %d\n", (int)cosl(0.0L), (int)sinl(0.0L));
	printf("hyp %d %d %d %d %d %d\n",
				 (int)acosh(1.0), (int)asinh(0.0), (int)atanh(0.0),
				 (int)cosh(0.0), (int)sinh(0.0), (int)tanh(0.0));
}

void s7_12_explog(void) {
	int e;
	double ip;
	double fr;
	printf("exp %d %d %d\n", (int)exp(0.0), (int)exp2(3.0), (int)expm1(0.0));
	printf("log %d %d %d %d\n", (int)log(1.0), (int)log10(1000.0), (int)log1p(0.0), (int)log2(8.0));
	printf("expf %d logf %d\n", (int)expf(0.0f), (int)log2f(8.0f));
	fr = frexp(8.0, &e);
	printf("frexp %.1f %d\n", fr, e);
	printf("ldexp %d scalbn %d scalbln %d\n", (int)ldexp(1.0, 4), (int)scalbn(1.0, 3), (int)scalbln(1.0, 2));
	printf("ilogb %d logb %d\n", ilogb(8.0), (int)logb(8.0));
	fr = modf(3.5, &ip);
	printf("modf %.1f %.1f\n", fr, ip);
}

void s7_12_powabs(void) {
	printf("cbrt %d fabs %d hypot %d pow %d sqrt %d\n",
				 (int)cbrt(27.0), (int)fabs(-3.0), (int)hypot(3.0, 4.0),
				 (int)pow(2.0, 10.0), (int)sqrt(16.0));
	printf("sqrtf %d powl %d\n", (int)sqrtf(16.0f), (int)powl(2.0L, 10.0L));
	printf("erf %d erfc %d lgamma %d tgamma %d\n",
				 (int)erf(0.0), (int)erfc(0.0), (int)lgamma(1.0), (int)tgamma(5.0));
}

void s7_12_nearest_rem(void) {
	int q;
	printf("ceil %d floor %d round %d trunc %d\n",
				 (int)ceil(2.1), (int)floor(2.7), (int)round(2.5), (int)trunc(2.7));
	printf("nearbyint %d rint %d\n", (int)nearbyint(2.5), (int)rint(2.5));
	printf("lrint %ld llrint %lld\n", lrint(2.5), llrint(2.5));
	printf("lround %ld llround %lld\n", lround(2.5), llround(2.5));
	printf("fmod %d remainder %d\n", (int)fmod(10.0, 3.0), (int)remainder(10.0, 3.0));
	printf("remquo %d\n", (int)remquo(10.0, 3.0, &q));
}

void s7_12_manip_cmp(void) {
	double nan = 0.0 / 0.0;
	printf("copysign %d %d\n", (int)copysign(3.0, -1.0), (int)copysign(3.0, 1.0));
	printf("nextafter %d %d\n", nextafter(1.0, 2.0) > 1.0, nextafter(1.0, 0.0) < 1.0);
	printf("nexttoward %d\n", nexttoward(1.0, 2.0L) > 1.0);
	printf("fdim %d fmax %d fmin %d fma %d\n",
				 (int)fdim(5.0, 2.0), (int)fmax(1.0, 2.0), (int)fmin(1.0, 2.0), (int)fma(2.0, 3.0, 4.0));
	printf("fmax_nan %d fmin_nan %d\n", (int)fmax(nan, 5.0), (int)fmin(nan, 5.0));
	printf("isgreater %d %d\n", isgreater(2.0, 1.0), isgreater(1.0, 2.0));
	printf("isgreaterequal %d isless %d islessequal %d\n",
				 isgreaterequal(2.0, 2.0), isless(2.0, 1.0), islessequal(2.0, 2.0));
	printf("islessgreater %d isunordered %d %d\n",
				 islessgreater(2.0, 1.0), isunordered(2.0, nan), isunordered(2.0, 1.0));
}
