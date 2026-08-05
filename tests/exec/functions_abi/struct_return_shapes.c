extern int printf(const char *, ...);

static int fails;

struct S1 {
	char a;
	char b;
};

static struct S1 m1(void) {
	struct S1 s;
	s.a = (char)1;
	s.b = (char)2;
	return s;
}

struct S2 {
	char a;
	short b;
};

static struct S2 m2(void) {
	struct S2 s;
	s.a = (char)1;
	s.b = (short)3;
	return s;
}

struct S3 {
	char a;
	int b;
};

static struct S3 m3(void) {
	struct S3 s;
	s.a = (char)1;
	s.b = (int)4;
	return s;
}

struct S4 {
	char a;
	long b;
};

static struct S4 m4(void) {
	struct S4 s;
	s.a = (char)1;
	s.b = (long)5;
	return s;
}

struct S5 {
	char a;
	float b;
};

static struct S5 m5(void) {
	struct S5 s;
	s.a = (char)1;
	s.b = (float)6;
	return s;
}

struct S6 {
	char a;
	double b;
};

static struct S6 m6(void) {
	struct S6 s;
	s.a = (char)1;
	s.b = (double)7;
	return s;
}

struct S7 {
	short a;
	char b;
};

static struct S7 m7(void) {
	struct S7 s;
	s.a = (short)2;
	s.b = (char)2;
	return s;
}

struct S8 {
	short a;
	short b;
};

static struct S8 m8(void) {
	struct S8 s;
	s.a = (short)2;
	s.b = (short)3;
	return s;
}

struct S9 {
	short a;
	int b;
};

static struct S9 m9(void) {
	struct S9 s;
	s.a = (short)2;
	s.b = (int)4;
	return s;
}

struct S10 {
	short a;
	long b;
};

static struct S10 m10(void) {
	struct S10 s;
	s.a = (short)2;
	s.b = (long)5;
	return s;
}

struct S11 {
	short a;
	float b;
};

static struct S11 m11(void) {
	struct S11 s;
	s.a = (short)2;
	s.b = (float)6;
	return s;
}

struct S12 {
	short a;
	double b;
};

static struct S12 m12(void) {
	struct S12 s;
	s.a = (short)2;
	s.b = (double)7;
	return s;
}

struct S13 {
	int a;
	char b;
};

static struct S13 m13(void) {
	struct S13 s;
	s.a = (int)3;
	s.b = (char)2;
	return s;
}

struct S14 {
	int a;
	short b;
};

static struct S14 m14(void) {
	struct S14 s;
	s.a = (int)3;
	s.b = (short)3;
	return s;
}

struct S15 {
	int a;
	int b;
};

static struct S15 m15(void) {
	struct S15 s;
	s.a = (int)3;
	s.b = (int)4;
	return s;
}

struct S16 {
	int a;
	long b;
};

static struct S16 m16(void) {
	struct S16 s;
	s.a = (int)3;
	s.b = (long)5;
	return s;
}

struct S17 {
	int a;
	float b;
};

static struct S17 m17(void) {
	struct S17 s;
	s.a = (int)3;
	s.b = (float)6;
	return s;
}

struct S18 {
	int a;
	double b;
};

static struct S18 m18(void) {
	struct S18 s;
	s.a = (int)3;
	s.b = (double)7;
	return s;
}

struct S19 {
	long a;
	char b;
};

static struct S19 m19(void) {
	struct S19 s;
	s.a = (long)4;
	s.b = (char)2;
	return s;
}

struct S20 {
	long a;
	short b;
};

static struct S20 m20(void) {
	struct S20 s;
	s.a = (long)4;
	s.b = (short)3;
	return s;
}

struct S21 {
	long a;
	int b;
};

static struct S21 m21(void) {
	struct S21 s;
	s.a = (long)4;
	s.b = (int)4;
	return s;
}

struct S22 {
	long a;
	long b;
};

static struct S22 m22(void) {
	struct S22 s;
	s.a = (long)4;
	s.b = (long)5;
	return s;
}

struct S23 {
	long a;
	float b;
};

static struct S23 m23(void) {
	struct S23 s;
	s.a = (long)4;
	s.b = (float)6;
	return s;
}

struct S24 {
	long a;
	double b;
};

static struct S24 m24(void) {
	struct S24 s;
	s.a = (long)4;
	s.b = (double)7;
	return s;
}

struct S25 {
	float a;
	char b;
};

static struct S25 m25(void) {
	struct S25 s;
	s.a = (float)5;
	s.b = (char)2;
	return s;
}

struct S26 {
	float a;
	short b;
};

static struct S26 m26(void) {
	struct S26 s;
	s.a = (float)5;
	s.b = (short)3;
	return s;
}

struct S27 {
	float a;
	int b;
};

static struct S27 m27(void) {
	struct S27 s;
	s.a = (float)5;
	s.b = (int)4;
	return s;
}

struct S28 {
	float a;
	long b;
};

static struct S28 m28(void) {
	struct S28 s;
	s.a = (float)5;
	s.b = (long)5;
	return s;
}

struct S29 {
	float a;
	float b;
};

static struct S29 m29(void) {
	struct S29 s;
	s.a = (float)5;
	s.b = (float)6;
	return s;
}

struct S30 {
	float a;
	double b;
};

static struct S30 m30(void) {
	struct S30 s;
	s.a = (float)5;
	s.b = (double)7;
	return s;
}

struct S31 {
	double a;
	char b;
};

static struct S31 m31(void) {
	struct S31 s;
	s.a = (double)6;
	s.b = (char)2;
	return s;
}

struct S32 {
	double a;
	short b;
};

static struct S32 m32(void) {
	struct S32 s;
	s.a = (double)6;
	s.b = (short)3;
	return s;
}

struct S33 {
	double a;
	int b;
};

static struct S33 m33(void) {
	struct S33 s;
	s.a = (double)6;
	s.b = (int)4;
	return s;
}

struct S34 {
	double a;
	long b;
};

static struct S34 m34(void) {
	struct S34 s;
	s.a = (double)6;
	s.b = (long)5;
	return s;
}

struct S35 {
	double a;
	float b;
};

static struct S35 m35(void) {
	struct S35 s;
	s.a = (double)6;
	s.b = (float)6;
	return s;
}

struct S36 {
	double a;
	double b;
};

static struct S36 m36(void) {
	struct S36 s;
	s.a = (double)6;
	s.b = (double)7;
	return s;
}

struct Nest {
	struct {
		int x;
	} a;
	struct {
		double y;
	} b;
};

static struct Nest mnest(void) {
	struct Nest s;
	s.a.x = 1;
	s.b.y = 2;
	return s;
}

int main(void) {
	{
		struct S1 v = m1();
		if ((long)v.a != 1 || (double)v.b != 2)
			fails++;
	}
	{
		struct S2 v = m2();
		if ((long)v.a != 1 || (double)v.b != 3)
			fails++;
	}
	{
		struct S3 v = m3();
		if ((long)v.a != 1 || (double)v.b != 4)
			fails++;
	}
	{
		struct S4 v = m4();
		if ((long)v.a != 1 || (double)v.b != 5)
			fails++;
	}
	{
		struct S5 v = m5();
		if ((long)v.a != 1 || (double)v.b != 6)
			fails++;
	}
	{
		struct S6 v = m6();
		if ((long)v.a != 1 || (double)v.b != 7)
			fails++;
	}
	{
		struct S7 v = m7();
		if ((long)v.a != 2 || (double)v.b != 2)
			fails++;
	}
	{
		struct S8 v = m8();
		if ((long)v.a != 2 || (double)v.b != 3)
			fails++;
	}
	{
		struct S9 v = m9();
		if ((long)v.a != 2 || (double)v.b != 4)
			fails++;
	}
	{
		struct S10 v = m10();
		if ((long)v.a != 2 || (double)v.b != 5)
			fails++;
	}
	{
		struct S11 v = m11();
		if ((long)v.a != 2 || (double)v.b != 6)
			fails++;
	}
	{
		struct S12 v = m12();
		if ((long)v.a != 2 || (double)v.b != 7)
			fails++;
	}
	{
		struct S13 v = m13();
		if ((long)v.a != 3 || (double)v.b != 2)
			fails++;
	}
	{
		struct S14 v = m14();
		if ((long)v.a != 3 || (double)v.b != 3)
			fails++;
	}
	{
		struct S15 v = m15();
		if ((long)v.a != 3 || (double)v.b != 4)
			fails++;
	}
	{
		struct S16 v = m16();
		if ((long)v.a != 3 || (double)v.b != 5)
			fails++;
	}
	{
		struct S17 v = m17();
		if ((long)v.a != 3 || (double)v.b != 6)
			fails++;
	}
	{
		struct S18 v = m18();
		if ((long)v.a != 3 || (double)v.b != 7)
			fails++;
	}
	{
		struct S19 v = m19();
		if ((long)v.a != 4 || (double)v.b != 2)
			fails++;
	}
	{
		struct S20 v = m20();
		if ((long)v.a != 4 || (double)v.b != 3)
			fails++;
	}
	{
		struct S21 v = m21();
		if ((long)v.a != 4 || (double)v.b != 4)
			fails++;
	}
	{
		struct S22 v = m22();
		if ((long)v.a != 4 || (double)v.b != 5)
			fails++;
	}
	{
		struct S23 v = m23();
		if ((long)v.a != 4 || (double)v.b != 6)
			fails++;
	}
	{
		struct S24 v = m24();
		if ((long)v.a != 4 || (double)v.b != 7)
			fails++;
	}
	{
		struct S25 v = m25();
		if ((long)v.a != 5 || (double)v.b != 2)
			fails++;
	}
	{
		struct S26 v = m26();
		if ((long)v.a != 5 || (double)v.b != 3)
			fails++;
	}
	{
		struct S27 v = m27();
		if ((long)v.a != 5 || (double)v.b != 4)
			fails++;
	}
	{
		struct S28 v = m28();
		if ((long)v.a != 5 || (double)v.b != 5)
			fails++;
	}
	{
		struct S29 v = m29();
		if ((long)v.a != 5 || (double)v.b != 6)
			fails++;
	}
	{
		struct S30 v = m30();
		if ((long)v.a != 5 || (double)v.b != 7)
			fails++;
	}
	{
		struct S31 v = m31();
		if ((long)v.a != 6 || (double)v.b != 2)
			fails++;
	}
	{
		struct S32 v = m32();
		if ((long)v.a != 6 || (double)v.b != 3)
			fails++;
	}
	{
		struct S33 v = m33();
		if ((long)v.a != 6 || (double)v.b != 4)
			fails++;
	}
	{
		struct S34 v = m34();
		if ((long)v.a != 6 || (double)v.b != 5)
			fails++;
	}
	{
		struct S35 v = m35();
		if ((long)v.a != 6 || (double)v.b != 6)
			fails++;
	}
	{
		struct S36 v = m36();
		if ((long)v.a != 6 || (double)v.b != 7)
			fails++;
	}
	{
		struct Nest v = mnest();
		if (v.a.x != 1 || v.b.y != 2)
			fails++;
	}
	printf("%s\n", fails == 0 ? "OK" : "FAIL");
	return 0;
}
