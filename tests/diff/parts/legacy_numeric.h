void float_test(void) {
#if !defined(__arm__) || defined(__ARM_PCS_VFP) || defined __ANDROID__
	volatile float fa, fb;
	volatile double da, db;
	int a;
	unsigned int b;
	static double nan2 = 0.0 / 0.0;
	static double inf1 = 1.0 / 0.0;
	static double inf2 = 1e5000;
	volatile LONG_DOUBLE la;

	printf("sizeof(float) = %d\n", sizeof(float));
	printf("sizeof(double) = %d\n", sizeof(double));
	printf("sizeof(long double) = %d\n", sizeof(LONG_DOUBLE));
	ftest();
	dtest();
	ldtest();
	printf("%f %f %f\n", ftab1[0], ftab1[1], ftab1[2]);
	printf("%f %f %f\n", 2.12, .5, 2.3e10);

	da = 123;
	printf("da=%f\n", da);
	fa = 123;
	printf("fa=%f\n", fa);
	a = 4000000000;
	da = a;
	printf("da = %f\n", da);
	b = 4000000000;
	db = b;
	printf("db = %f\n", db);
	printf("nan != nan = %d, inf1 = %f, inf2 = %f\n", nan2 != nan2, inf1, inf2);
	da = 0x0.88p-1022;
	la = da;
	printf("da subnormal = %a\n", da);
	printf("da subnormal = %.40g\n", da);
	printf("la subnormal = %La\n", la);
	printf("la subnormal = %.40Lg\n", la);
	da /= 2;
	la = da;
	printf("da/2 subnormal = %a\n", da);
	printf("da/2 subnormal = %.40g\n", da);
	printf("la/2 subnormal = %La\n", la);
	printf("la/2 subnormal = %.40Lg\n", la);
	fa = 0x0.88p-126f;
	la = fa;
	printf("fa subnormal = %a\n", fa);
	printf("fa subnormal = %.40g\n", fa);
	printf("la subnormal = %La\n", la);
	printf("la subnormal = %.40Lg\n", la);
	fa /= 2;
	la = fa;
	printf("fa/2 subnormal = %a\n", fa);
	printf("fa/2 subnormal = %.40g\n", fa);
	printf("la/2 subnormal = %La\n", la);
	printf("la/2 subnormal = %.40Lg\n", la);
#endif
}

int fib(int n) {
	if (n <= 2)
		return 1;
	else
		return fib(n - 1) + fib(n - 2);
}

#if __GNUC__ == 3 || __GNUC__ == 4
#define aligned_function 0
#else
void __attribute__((aligned(16))) aligned_function(int i) {
}
#endif

void funcptr_test() {
	void (*func)(int);
	int a;
	struct {
		int dummy;
		void (*func)(int);
	} st1;
	long diff;

	func = &num;
	(*func)(12345);
	func = num;
	a = 1;
	a = 1;
	func(12345);

	st1.func = num;
	st1.func(12346);
	printf("sizeof1 = %d\n", sizeof(funcptr_test));
	printf("sizeof2 = %d\n", sizeof funcptr_test);
	printf("sizeof3 = %d\n", sizeof(&funcptr_test));
	printf("sizeof4 = %d\n", sizeof &funcptr_test);
	a = 0;
	func = num + a;
	diff = func - num;
	func(42);
	(func + diff)(42);
	(num + a)(43);

	func = aligned_function;
	printf("aligned_function (should be zero): %d\n", ((int)(uintptr_t)func) & 15);
}

void lloptest(long long a, long long b) {
	unsigned long long ua, ub;

	ua = a;
	ub = b;

	printf("arith: " LONG_LONG_FORMAT " " LONG_LONG_FORMAT " " LONG_LONG_FORMAT "\n",
				 a + b,
				 a - b,
				 a * b);

	if (b != 0) {
		printf("arith1: " LONG_LONG_FORMAT " " LONG_LONG_FORMAT "\n",
					 a / b,
					 a % b);
	}

	printf("bin: " LONG_LONG_FORMAT " " LONG_LONG_FORMAT " " LONG_LONG_FORMAT "\n",
				 a & b,
				 a | b,
				 a ^ b);

	printf("test: %d %d %d %d %d %d\n",
				 a == b,
				 a != b,
				 a<b,
					 a>
						 b,
				 a >= b,
				 a <= b);

	printf("utest: %d %d %d %d %d %d\n",
				 ua == ub,
				 ua != ub,
				 ua<ub,
						ua>
						 ub,
				 ua >= ub,
				 ua <= ub);

	a++;
	b++;
	printf("arith2: " LONG_LONG_FORMAT " " LONG_LONG_FORMAT "\n", a, b);
	printf("arith2: " LONG_LONG_FORMAT " " LONG_LONG_FORMAT "\n", a++, b++);
	printf("arith2: " LONG_LONG_FORMAT " " LONG_LONG_FORMAT "\n", --a, --b);
	printf("arith2: " LONG_LONG_FORMAT " " LONG_LONG_FORMAT "\n", a, b);
	b = ub = 0;
	printf("not: %d %d %d %d\n", !a, !ua, !b, !ub);
}

void llshift(long long a, int b) {
	printf("shift: " LONG_LONG_FORMAT " " LONG_LONG_FORMAT " " LONG_LONG_FORMAT "\n",
				 (unsigned long long)a >> b,
				 a >> b,
				 a << b);
	printf("shiftc: " LONG_LONG_FORMAT " " LONG_LONG_FORMAT " " LONG_LONG_FORMAT "\n",
				 (unsigned long long)a >> 3,
				 a >> 3,
				 a << 3);
	printf("shiftc: " LONG_LONG_FORMAT " " LONG_LONG_FORMAT " " LONG_LONG_FORMAT "\n",
				 (unsigned long long)a >> 35,
				 a >> 35,
				 a << 35);
}

void llfloat(void) {
	float fa;
	double da;
	LONG_DOUBLE lda;
	long long la, lb, lc;
	unsigned long long ula, ulb, ulc;
	la = 0x12345678;
	ula = 0x72345678;
	la = (la << 20) | 0x12345;
	ula = ula << 33;
	printf("la=" LONG_LONG_FORMAT " ula=" ULONG_LONG_FORMAT "\n", la, ula);

	fa = la;
	da = la;
	lda = la;
	printf("lltof: %f %f %Lf\n", fa, da, lda);

	la = fa;
	lb = da;
	lc = lda;
	printf("ftoll: " LONG_LONG_FORMAT " " LONG_LONG_FORMAT " " LONG_LONG_FORMAT "\n", la, lb, lc);

	fa = ula;
	da = ula;
	lda = ula;
	printf("ulltof: %f %f %Lf\n", fa, da, lda);

	ula = fa;
	ulb = da;
	ulc = lda;
	printf("ftoull: " ULONG_LONG_FORMAT " " ULONG_LONG_FORMAT " " ULONG_LONG_FORMAT "\n", ula, ulb, ulc);
}

long long llfunc1(int a) {
	return a * 2;
}

struct S {
	int id;
	char item;
};

long long int value(struct S *v) {
	return ((long long int)v->item);
}

long long llfunc2(long long x, long long y, int z) {
	return x * y * z;
}

void check_opl_save_regs(char *a, long long b, int c) {
	*a = b < 0 && !c;
}

void longlong_test(void) {
	long long a, b, c;
	int ia;
	unsigned int ua;
	printf("sizeof(long long) = %d\n", sizeof(long long));
	ia = -1;
	ua = -2;
	a = ia;
	b = ua;
	printf(LONG_LONG_FORMAT " " LONG_LONG_FORMAT "\n", a, b);
	printf(LONG_LONG_FORMAT " " LONG_LONG_FORMAT " " LONG_LONG_FORMAT " " XLONG_LONG_FORMAT "\n",
				 (long long)1,
				 (long long)-2,
				 1LL,
				 0x1234567812345679);
	a = llfunc1(-3);
	printf(LONG_LONG_FORMAT "\n", a);

	lloptest(1000, 23);
	lloptest(0xff, 0x1234);
	b = 0x72345678 << 10;
	lloptest(-3, b);
	llshift(0x123, 5);
	llshift(-23, 5);
	b = 0x72345678LL << 10;
	llshift(b, 47);

	llfloat();
#if 1
	b = 0x12345678;
	a = -1;
	c = a + b;
	printf(XLONG_LONG_FORMAT "\n", c);
#endif

	{
		struct S a;

		a.item = 3;
		printf("%lld\n", value(&a));
	}
	lloptest(0x80000000, 0);

	{
		long long *p, v, **pp;
		v = 1;
		p = &v;
		p[0]++;
		printf("another long long spill test : %lld\n", *p);
		pp = &p;

		v = llfunc2(**pp, **pp, ia);
		printf("a long long function (arm-)reg-args test : %lld\n", v);
	}
	a = 68719476720LL;
	b = 4294967295LL;
	printf("%d %d %d %d\n", a > b, a < b, a >= b, a <= b);

	printf(LONG_LONG_FORMAT "\n", 0x123456789LLU);

	a = 0x123;
	long long *p = &a;
	llshift(*p, 5);

	unsigned long long u = 0x8000000000000001ULL;
	u = (unsigned)(u + 1);
	printf("long long u=" ULONG_LONG_FORMAT "\n", u);
	u = 0x11223344aa998877ULL;
	u = (unsigned)(int)(u + 1);
	printf("long long u=" ULONG_LONG_FORMAT "\n", u);

	char cc = 78;
	check_opl_save_regs(&cc, -1, 0);
	printf("check_opl_save_regs: %d\n", cc);
}
