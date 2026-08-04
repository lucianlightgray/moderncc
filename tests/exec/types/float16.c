#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef unsigned short u16;
typedef unsigned int u32;

static u16 hb(_Float16 h) {
	u16 r;
	memcpy(&r, &h, 2);
	return r;
}

static _Float16 mkh(u16 b) {
	_Float16 h;
	memcpy(&h, &b, 2);
	return h;
}

static u32 fb(float f) {
	u32 b;
	memcpy(&b, &f, 4);
	return b;
}

static float mkf(u32 b) {
	float f;
	memcpy(&f, &b, 4);
	return f;
}

struct SH {
	_Float16 a;
};
struct SHH {
	_Float16 a, b;
};
struct SHF {
	_Float16 a;
	float b;
};
struct SHI {
	_Float16 a;
	int b;
};
struct SCH {
	char c;
	_Float16 a;
};

static _Float16 addh(_Float16 a, _Float16 b) { return a + b; }
static struct SHH pass2(struct SHH s) {
	s.a = s.a + (_Float16)1.0f16;
	return s;
}
static struct SHF passf(struct SHF s) {
	s.b = s.b + (float)s.a;
	return s;
}
static _Float16 vsum(int n, ...) {
	va_list ap;
	_Float16 t = (_Float16)0.0f16;
	int i;
	va_start(ap, n);
	for (i = 0; i < n; i++)
		t = t + va_arg(ap, _Float16);
	va_end(ap);
	return t;
}

static _Float16 garr[4] = {1.0f16, -2.5f16, 0.0f16, 65504.0f16};
static _Float16 gscalar = 1.5f16;
static struct SHF gsf = {2.5f16, 4.0f};

int ok = 1;

static void chk(int cond, const char *what) {
	if (!cond) {
		ok = 0;
		printf("FAIL %s\n", what);
	}
}

int main(void) {
	int i;
	static const u32 fin[] = {0x3F801000u, 0x3F802000u, 0x3F803000u, 0x477FE000u,
														0x477FF000u, 0x38800000u, 0x33800000u, 0x33000000u,
														0x32800000u, 0x387FC000u, 0x387FE000u, 0x7F800000u,
														0xFF800000u, 0x7FC00000u, 0x00000000u, 0x80000000u};
	static const u16 fout[] = {0x3C00, 0x3C01, 0x3C02, 0x7BFF, 0x7C00, 0x0400,
														 0x0001, 0x0000, 0x0000, 0x03FF, 0x0400, 0x7C00,
														 0xFC00, 0x7E00, 0x0000, 0x8000};
	static const u16 hin[] = {0x0001, 0x03FF, 0x0400, 0x7BFF, 0x7C00, 0xFC00, 0x3C00};
	static const u32 hout[] = {0x33800000u, 0x387FC000u, 0x38800000u, 0x477FE000u,
														 0x7F800000u, 0xFF800000u, 0x3F800000u};

	chk(sizeof(_Float16) == 2, "sizeof");
	chk(_Alignof(_Float16) == 2, "alignof");
	chk(__FLT16_MANT_DIG__ == 11, "__FLT16_MANT_DIG__");
	chk(__FLT16_MAX_EXP__ == 16, "__FLT16_MAX_EXP__");

	for (i = 0; i < (int)(sizeof(fin) / sizeof(fin[0])); i++)
		chk(hb((_Float16)mkf(fin[i])) == fout[i], "f32->f16 rounding");
	for (i = 0; i < (int)(sizeof(hin) / sizeof(hin[0])); i++)
		chk(fb((float)mkh(hin[i])) == hout[i], "f16->f32 widening");

	chk(hb((_Float16)(mkh(0x3C00) + mkh(0x4000))) == 0x4200, "add");
	chk(hb((_Float16)(mkh(0x4200) - mkh(0x3C00))) == 0x4000, "sub");
	chk(hb((_Float16)(mkh(0x4000) * mkh(0x4000))) == 0x4400, "mul");
	chk(hb((_Float16)(mkh(0x4400) / mkh(0x4000))) == 0x4000, "div");
	chk(hb((_Float16)(-mkh(0x3C00))) == 0xBC00, "neg");
	chk(hb((_Float16)(-mkh(0x0000))) == 0x8000, "neg zero");
	chk(mkh(0x3C00) < mkh(0x4000), "lt");
	chk(mkh(0x4000) > mkh(0x3C00), "gt");
	chk(mkh(0x3C00) == mkh(0x3C00), "eq");
	chk(!(mkh(0x7E00) == mkh(0x7E00)), "nan ne");

	chk(hb(gscalar) == 0x3E00, "static scalar init");
	chk(hb(garr[0]) == 0x3C00 && hb(garr[1]) == 0xC100 && hb(garr[2]) == 0x0000 &&
					hb(garr[3]) == 0x7BFF,
			"static array init");
	chk(hb(gsf.a) == 0x4100 && gsf.b == 4.0f, "static struct init");

	chk(sizeof(struct SH) == 2 && sizeof(struct SHH) == 4, "struct size");
	chk(sizeof(struct SHF) == 8 && sizeof(struct SHI) == 8, "struct size 2");
	chk(sizeof(struct SCH) == 4 && _Alignof(struct SCH) == 2, "struct align");
	chk(sizeof(garr) == 8, "array size");

	chk(hb(addh(mkh(0x3C00), mkh(0x4000))) == 0x4200, "byval scalar arg");
	{
		struct SHH s = {2.0f16, 3.0f16};
		struct SHH r = pass2(s);
		chk(hb(r.a) == 0x4200 && hb(r.b) == 0x4200, "byval struct");
	}
	{
		struct SHF s = {1.5f16, 2.0f};
		struct SHF r = passf(s);
		chk(hb(r.a) == 0x3E00 && r.b == 3.5f, "byval mixed struct");
	}
	chk(hb(vsum(3, (_Float16)1.0f16, (_Float16)2.0f16, (_Float16)4.0f16)) == 0x4700,
			"varargs");

	chk((int)(float)mkh(0x4200) == 3, "f16->int");
	chk((double)mkh(0x3C00) == 1.0, "f16->double");
	chk(hb((_Float16)3) == 0x4200, "int->f16");
	chk(hb((_Float16)3.0) == 0x4200, "double->f16");
	chk(hb((_Float16)3.0L) == 0x4200, "ldouble->f16");
	chk((long double)mkh(0x3C00) == 1.0L, "f16->ldouble");

	{
		_Float16 h = 1.0f16;
		h += (_Float16)1.0f16;
		chk(hb(h) == 0x4000, "compound add");
		h++;
		chk(hb(h) == 0x4200, "increment");
	}

	printf(ok ? "OK\n" : "FAIL\n");
	return 0;
}
