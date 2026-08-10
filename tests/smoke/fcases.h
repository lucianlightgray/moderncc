#ifndef MCC_SMOKE_FCASES_H
#define MCC_SMOKE_FCASES_H

#include <string.h>
#include <stdio.h>

#include "smoke.h"

#if defined __SIZEOF_INT256__
#define SMF_HAVE_I256 1
#else
#define SMF_HAVE_I256 0
#endif

#if defined __SIZEOF_INT128__ || defined __MCC__
#define SMF_HAVE_I128 1
#else
#define SMF_HAVE_I128 0
#endif

#define SMF_FTYPES(X) \
	X(F16, _Float16, 2) \
	X(F32, float, 4) \
	X(F64, double, 8) \
	X(F80, long double, 10)

#define SMF_CTY_F16 _Float16
#define SMF_CTY_F32 float
#define SMF_CTY_F64 double
#define SMF_CTY_F80 long double

#define SMF_OPS(X, A) \
	X(A, FADD) X(A, FSUB) X(A, FMUL) X(A, FDIV) \
	X(A, FNEG) X(A, FSEL) X(A, FLT) X(A, FEQ) X(A, FABS) \
	X(A, FSELI) X(A, FSELZ) X(A, FSELMIXL) X(A, FSELMIXR) X(A, FSELCMP) \
	X(A, FMULADD) X(A, FDIVSEL) X(A, FMIN3) X(A, FNEST) X(A, FSCALE) \
	X(A, FSUBABS) X(A, FSELIC) X(A, FSELMIXB) X(A, FSELMIXN) X(A, FCMPMIX)

#define SMF_EXPR_FADD(T, a, b, c) ((T)(a) + (T)(b))
#define SMF_EXPR_FSUB(T, a, b, c) ((T)(a) - (T)(b))
#define SMF_EXPR_FMUL(T, a, b, c) ((T)(a) * (T)(b))
#define SMF_EXPR_FDIV(T, a, b, c) ((T)(a) / (T)(b))
#define SMF_EXPR_FNEG(T, a, b, c) (-(T)(a))
#define SMF_EXPR_FSEL(T, a, b, c) ((T)(a) != (T)0 ? (T)(b) : (T)(c))
#define SMF_EXPR_FLT(T, a, b, c) ((T)((T)(a) < (T)(b)))
#define SMF_EXPR_FEQ(T, a, b, c) ((T)((T)(a) == (T)(b)))
#define SMF_EXPR_FABS(T, a, b, c) ((T)(a) < (T)0 ? -(T)(a) : (T)(a))
#define SMF_EXPR_FSELI(T, a, b, c) ((T)(a) > (T)(b) ? (T)(a) - (T)(b) : 0)
#define SMF_EXPR_FSELZ(T, a, b, c) ((T)(a) > (T)(b) ? (T)(a) - (T)(b) : 0.0)
#define SMF_EXPR_FSELMIXL(T, a, b, c) ((T)(a) != (T)0 ? (int)(T)(b) : (T)(c))
#define SMF_EXPR_FSELMIXR(T, a, b, c) ((T)(a) != (T)0 ? (T)(b) : (int)(T)(c))
#define SMF_EXPR_FSELCMP(T, a, b, c) ((T)(a) > (T)(b) ? 1 : (T)(c))
#define SMF_EXPR_FMULADD(T, a, b, c) ((T)((T)(a) * (T)(b) + (T)(c)))
#define SMF_EXPR_FDIVSEL(T, a, b, c) ((T)(b) != (T)0 ? (T)((T)(a) / (T)(b)) : (T)(c))
#define SMF_EXPR_FMIN3(T, a, b, c) \
	((T)(a) < (T)(b) ? ((T)(a) < (T)(c) ? (T)(a) : (T)(c)) \
									 : ((T)(b) < (T)(c) ? (T)(b) : (T)(c)))
#define SMF_EXPR_FNEST(T, a, b, c) \
	((T)(a) > (T)0 ? ((T)(b) > (T)0 ? (T)(b) : (T)(c)) : (T)(a))
#define SMF_EXPR_FSCALE(T, a, b, c) \
	((T)((T)((T)(a) * (T)2) - (T)((T)(b) / (T)2) + (T)(c)))
#define SMF_EXPR_FSUBABS(T, a, b, c) \
	((T)((T)(a) - (T)(b)) < (T)0 ? (T)((T)(b) - (T)(a)) : (T)((T)(a) - (T)(b)))
#define SMF_EXPR_FSELIC(T, a, b, c) ((int)((T)(a) != (T)0) ? (T)(b) : (T)(c))
#define SMF_EXPR_FSELMIXB(T, a, b, c) ((T)(a) < (T)(b) ? (int)(T)(c) : (T)(b))
#define SMF_EXPR_FSELMIXN(T, a, b, c) ((T)(a) == (T)(b) ? (T)(c) : (long)(T)(a))
#define SMF_EXPR_FCMPMIX(T, a, b, c) \
	((T)((int)((T)(a) < (T)(b)) + (int)((T)(b) < (T)(c))))

enum {
#define SMF_T_ROW(tag, cty, w) SMF_T_##tag,
	SMF_FTYPES(SMF_T_ROW)
#undef SMF_T_ROW
			SMF_T_COUNT
};

enum {
#define SMF_O_ROW(a, op) SMF_O_##op,
	SMF_OPS(SMF_O_ROW, _)
#undef SMF_O_ROW
			SMF_O_COUNT
};

#define SMF_KEY(tag, op) ((tag)*SMF_O_COUNT + (op))

typedef struct
{
	const char *name;
	unsigned short tag;
	unsigned short op;
	unsigned short foldable;
	long double a, b, c, fold, want;
} SmFRow;

#define SMF_ROW(nm, TY, OP, A, B, C, WANT) \
	{ nm, SMF_T_##TY, SMF_O_##OP, SM_FOLD_CHECKED, \
		(long double)(A), (long double)(B), (long double)(C), \
		(long double)(SMF_EXPR_##OP(SMF_CTY_##TY, A, B, C)), \
		(long double)(WANT) },

#define SMF_ROW_NF(nm, TY, OP, A, B, C, WANT) \
	{ nm, SMF_T_##TY, SMF_O_##OP, SM_FOLD_SKIP, \
		(long double)(A), (long double)(B), (long double)(C), \
		(long double)(WANT), (long double)(WANT) },

typedef struct
{
	const char *name;
	unsigned short tag;
	unsigned short op;
	unsigned short foldable;
	double ar, ai, br, bi, foldr, foldi, wantr, wanti;
} SmCRow;

#define SMC_OPS(X, A) X(A, CADD) X(A, CSUB) X(A, CMUL) X(A, CSEL) \
	X(A, CNEG) X(A, CCONJ) X(A, CSCALE) X(A, CSELR)

enum {
#define SMC_O_ROW(a, op) SMC_O_##op,
	SMC_OPS(SMC_O_ROW, _)
#undef SMC_O_ROW
			SMC_O_COUNT
};

#define SMC_CTY_C32 float
#define SMC_CTY_C64 double

enum { SMC_T_C32, SMC_T_C64, SMC_T_COUNT };

#define SMC_EXPR_CADD(T, ar, ai, br, bi) \
	((T _Complex)((T)(ar) + (T)(br)) + \
	 (T _Complex)(__extension__ 1.0i) * (T _Complex)((T)(ai) + (T)(bi)))

#define SMC_MK(T, re, im) \
	((T _Complex)(T)(re) + (T _Complex)(__extension__ 1.0i) * (T _Complex)(T)(im))

#define SMC_ROW(nm, TY, OP, AR, AI, BR, BI, WR, WI) \
	{ nm, SMC_T_##TY, SMC_O_##OP, SM_FOLD_SKIP, \
		(double)(AR), (double)(AI), (double)(BR), (double)(BI), \
		(double)(WR), (double)(WI), (double)(WR), (double)(WI) },

static const SmFRow smf_rows[] = {

		SMF_ROW("f64.sel.true", F64, FSEL, 1.0, 3.5, -2.25, 3.5)
		SMF_ROW("f64.sel.false", F64, FSEL, 0.0, 3.5, -2.25, -2.25)
		SMF_ROW("f64.sel.negzero.cond", F64, FSEL, -0.0, 3.5, -2.25, -2.25)
		SMF_ROW("f64.sel.negzero.val", F64, FSEL, 1.0, -0.0, 0.0, -0.0)
		SMF_ROW("f64.sel.denorm", F64, FSEL, 1.0, 4.9406564584124654e-324, 1.0,
						4.9406564584124654e-324)
		SMF_ROW("f64.sel.max", F64, FSEL, 1.0, 1.7976931348623157e308, 0.0,
						1.7976931348623157e308)
		SMF_ROW("f32.sel.true", F32, FSEL, 1.0f, 3.5f, -2.25f, 3.5f)
		SMF_ROW("f32.sel.negzero.val", F32, FSEL, 1.0f, -0.0f, 0.0f, -0.0f)
		SMF_ROW("f32.sel.denorm", F32, FSEL, 1.0f, 1.4012984643e-45f, 1.0f,
						1.4012984643e-45f)
		SMF_ROW("f80.sel.true", F80, FSEL, 1.0L, 3.5L, -2.25L, 3.5L)
		SMF_ROW("f80.sel.negzero.val", F80, FSEL, 1.0L, -0.0L, 0.0L, -0.0L)
		SMF_ROW("f16.sel.true", F16, FSEL, 1.0, 3.5, -2.25, 3.5)
		SMF_ROW("f16.sel.negzero.val", F16, FSEL, 1.0, -0.0, 0.0, -0.0)

		SMF_ROW("f64.add.negzero", F64, FADD, -0.0, 0.0, 0.0, 0.0)
		SMF_ROW("f64.add.negzero2", F64, FADD, -0.0, -0.0, 0.0, -0.0)
		SMF_ROW("f64.sub.zero", F64, FSUB, 0.0, 0.0, 0.0, 0.0)
		SMF_ROW("f64.mul.negzero", F64, FMUL, -1.0, 0.0, 0.0, -0.0)
		SMF_ROW("f64.neg.zero", F64, FNEG, 0.0, 0.0, 0.0, -0.0)
		SMF_ROW("f64.abs.negzero", F64, FABS, -0.0, 0.0, 0.0, -0.0)
		SMF_ROW("f32.neg.zero", F32, FNEG, 0.0f, 0.0f, 0.0f, -0.0f)
		SMF_ROW("f80.neg.zero", F80, FNEG, 0.0L, 0.0L, 0.0L, -0.0L)
		SMF_ROW("f16.neg.zero", F16, FNEG, 0.0, 0.0, 0.0, -0.0)

		SMF_ROW("f64.lt.negzero", F64, FLT, -0.0, 0.0, 0.0, 0.0)
		SMF_ROW("f64.eq.negzero", F64, FEQ, -0.0, 0.0, 0.0, 1.0)
		SMF_ROW("f32.eq.negzero", F32, FEQ, -0.0f, 0.0f, 0.0f, 1.0f)

		SMF_ROW("f64.tern.int0.taken", F64, FSELI, 5.5, 2.25, 0.0, 3.25)
		SMF_ROW("f64.tern.int0.else", F64, FSELI, 2.25, 5.5, 0.0, 0.0)
		SMF_ROW("f64.tern.int0.frac", F64, FSELI, 1.5, 1.25, 0.0, 0.25)
		SMF_ROW("f64.tern.dbl0.taken", F64, FSELZ, 5.5, 2.25, 0.0, 3.25)
		SMF_ROW("f64.tern.dbl0.frac", F64, FSELZ, 1.5, 1.25, 0.0, 0.25)
		SMF_ROW("f64.tern.mixl", F64, FSELMIXL, 1.0, 3.75, 2.5, 3.0)
		SMF_ROW("f64.tern.mixr", F64, FSELMIXR, 0.0, 3.75, 2.5, 2.0)
		SMF_ROW("f64.tern.cmp", F64, FSELCMP, 5.5, 2.25, 7.25, 1.0)
		SMF_ROW("f64.tern.cmp.else", F64, FSELCMP, 1.0, 2.25, 7.25, 7.25)
		SMF_ROW("f32.tern.int0.frac", F32, FSELI, 1.5f, 1.25f, 0.0f, 0.25f)
		SMF_ROW("f32.tern.dbl0.frac", F32, FSELZ, 1.5f, 1.25f, 0.0f, 0.25f)
		SMF_ROW("f32.tern.mixl", F32, FSELMIXL, 1.0f, 3.75f, 2.5f, 3.0f)
		SMF_ROW("f32.tern.cmp", F32, FSELCMP, 5.5f, 2.25f, 7.25f, 1.0f)
		SMF_ROW("f80.tern.int0.frac", F80, FSELI, 1.5L, 1.25L, 0.0L, 0.25L)
		SMF_ROW("f80.tern.mixl", F80, FSELMIXL, 1.0L, 3.75L, 2.5L, 3.0L)
		SMF_ROW("f80.tern.cmp", F80, FSELCMP, 5.5L, 2.25L, 7.25L, 1.0L)
		SMF_ROW("f16.tern.int0.frac", F16, FSELI, 1.5, 1.25, 0.0, 0.25)
		SMF_ROW("f16.tern.mixl", F16, FSELMIXL, 1.0, 3.75, 2.5, 3.0)
		SMF_ROW("f16.tern.cmp", F16, FSELCMP, 5.5, 2.25, 7.25, 1.0)

		SMF_ROW("f64.denorm.add", F64, FADD, 4.9406564584124654e-324,
						4.9406564584124654e-324, 0.0, 9.8813129168249309e-324)
		SMF_ROW("f32.denorm.add", F32, FADD, 1.4012984643e-45f, 1.4012984643e-45f,
						0.0f, 2.8025969286e-45f)
};

static const SmCRow smc_rows[] = {

		SMC_ROW("c64.add", C64, CADD, 1.5, -2.5, 0.25, 4.0, 1.75, 1.5)
		SMC_ROW("c64.sub", C64, CSUB, 1.5, -2.5, 0.25, 4.0, 1.25, -6.5)
		SMC_ROW("c64.mul", C64, CMUL, 1.0, 2.0, 3.0, 4.0, -5.0, 10.0)
		SMC_ROW("c64.sel.negzero", C64, CSEL, 1.0, 0.0, -0.0, -0.0, -0.0, -0.0)
		SMC_ROW("c32.add", C32, CADD, 1.5, -2.5, 0.25, 4.0, 1.75, 1.5)
		SMC_ROW("c32.mul", C32, CMUL, 1.0, 2.0, 3.0, 4.0, -5.0, 10.0)
};

#if SMF_HAVE_I128
typedef unsigned __int128 SmU128;
typedef __int128 SmS128;
#endif

typedef struct
{
	const char *name;
	unsigned short tag;
	unsigned short op;
	unsigned short foldable;
	SmBits ahi, alo, bhi, blo, whi, wlo, fhi, flo;
} SmWRow;

enum { SMW_T_S128, SMW_T_U128, SMW_T_S256, SMW_T_U256, SMW_T_COUNT };
enum { SMW_O_ADD, SMW_O_SUB, SMW_O_MUL, SMW_O_NEG, SMW_O_SHL, SMW_O_SHR,
			 SMW_O_COUNT };

#define SMW_ROW(nm, TY, OP, AHI, ALO, BHI, BLO, WHI, WLO) \
	{ nm, SMW_T_##TY, SMW_O_##OP, SM_FOLD_SKIP, \
		(SmBits)(AHI), (SmBits)(ALO), (SmBits)(BHI), (SmBits)(BLO), \
		(SmBits)(WHI), (SmBits)(WLO), (SmBits)(WHI), (SmBits)(WLO) },

static const SmWRow smw_rows[] = {

		SMW_ROW("i128.neg.min", S128, NEG, 0x8000000000000000ull, 0ull, 0ull, 0ull,
						0x8000000000000000ull, 0ull)
		SMW_ROW("i128.neg.one", S128, NEG, 0ull, 1ull, 0ull, 0ull,
						0xffffffffffffffffull, 0xffffffffffffffffull)
		SMW_ROW("i128.add.maxp1", S128, ADD, 0x7fffffffffffffffull,
						0xffffffffffffffffull, 0ull, 1ull, 0x8000000000000000ull, 0ull)
		SMW_ROW("u128.add.maxp1", U128, ADD, 0xffffffffffffffffull,
						0xffffffffffffffffull, 0ull, 1ull, 0ull, 0ull)
		SMW_ROW("i128.mul.min.m1", S128, MUL, 0x8000000000000000ull, 0ull,
						0xffffffffffffffffull, 0xffffffffffffffffull,
						0x8000000000000000ull, 0ull)
		SMW_ROW("i128.shl.63", S128, SHL, 0ull, 1ull, 0ull, 63ull,
						0ull, 0x8000000000000000ull)
		SMW_ROW("i128.shl.127", S128, SHL, 0ull, 1ull, 0ull, 127ull,
						0x8000000000000000ull, 0ull)
		SMW_ROW("i128.shr.min.127", S128, SHR, 0x8000000000000000ull, 0ull, 0ull,
						127ull, 0xffffffffffffffffull, 0xffffffffffffffffull)
		SMW_ROW("u128.shr.hi.127", U128, SHR, 0x8000000000000000ull, 0ull, 0ull,
						127ull, 0ull, 1ull)
		SMW_ROW("i128.sub.min.1", S128, SUB, 0x8000000000000000ull, 0ull, 0ull, 1ull,
						0x7fffffffffffffffull, 0xffffffffffffffffull)

#if SMF_HAVE_I256
		SMW_ROW("i256.neg.one", S256, NEG, 0ull, 1ull, 0ull, 0ull,
						0xffffffffffffffffull, 0xffffffffffffffffull)
		SMW_ROW("i256.add.carry", S256, ADD, 0ull, 0xffffffffffffffffull, 0ull, 1ull,
						1ull, 0ull)
		SMW_ROW("u256.add.carry", U256, ADD, 0ull, 0xffffffffffffffffull, 0ull, 1ull,
						1ull, 0ull)
		SMW_ROW("i256.shl.64", S256, SHL, 0ull, 1ull, 0ull, 64ull, 1ull, 0ull)
		SMW_ROW("i256.mul", S256, MUL, 0ull, 0x100000000ull, 0ull, 0x100000000ull,
						1ull, 0ull)
		SMW_ROW("i256.sub.borrow", S256, SUB, 0ull, 0ull, 0ull, 1ull,
						0xffffffffffffffffull, 0xffffffffffffffffull)
#endif
};

static long smf_enc(int tag, long double v, SmBits *lo, SmBits *hi)
{
	unsigned char buf[16];
	int n = 0;
	memset(buf, 0, sizeof buf);
	switch (tag) {
	case SMF_T_F16: {
		_Float16 t = (_Float16)v;
		n = 2;
		memcpy(buf, &t, 2);
		break;
	}
	case SMF_T_F32: {
		float t = (float)v;
		n = 4;
		memcpy(buf, &t, 4);
		break;
	}
	case SMF_T_F64: {
		double t = (double)v;
		n = 8;
		memcpy(buf, &t, 8);
		break;
	}
	default: {
		long double t = v;
		n = (int)sizeof(long double) > 10 ? 10 : (int)sizeof(long double);
		memcpy(buf, &t, (size_t)n);
		break;
	}
	}
	memcpy(lo, buf, 8);
	memcpy(hi, buf + 8, 8);
	return n;
}

#define SMF_ARM(TY, OP) \
	case SMF_KEY(SMF_T_##TY, SMF_O_##OP): { \
		volatile SMF_CTY_##TY va = (SMF_CTY_##TY)(a); \
		volatile SMF_CTY_##TY vb = (SMF_CTY_##TY)(b); \
		volatile SMF_CTY_##TY vc = (SMF_CTY_##TY)(c); \
		return (long double)(SMF_EXPR_##OP(SMF_CTY_##TY, va, vb, vc)); \
	}

#define SMF_ARMS_TY(tag, cty, w) SMF_OPS(SMF_ARM, tag)

static long double smf_run(int tag, int op, long double a, long double b,
													 long double c)
{
	switch (SMF_KEY(tag, op)) {
		SMF_FTYPES(SMF_ARMS_TY)
	}
	return (long double)0;
}

static const long double smf_corpus[] = {
		0.0L, -0.0L, 1.0L, -1.0L, 0.5L, -0.5L, 2.25L, -2.25L, 1.5L, 5.5L,
		1e-30L, -1e-30L, 1e30L, -1e30L, 3.0L, -3.0L, 1024.0L, -1024.0L,
		0.25L, -0.25L, 0.75L, -0.75L, 0.125L, -0.125L, 0.0625L, -0.0625L,
		1.25L, -1.25L, 1.75L, -1.75L, 2.0L, -2.0L, 4.0L, -4.0L,
		6.0L, -6.0L, 7.5L, -7.5L, 8.0L, -8.0L, 12.0L, -12.0L,
		16.0L, -16.0L, 31.0L, -31.0L, 32.0L, -32.0L, 63.0L, -63.0L,
		64.0L, -64.0L, 100.0L, -100.0L, 127.0L, -127.0L, 128.0L, -128.0L,
		255.0L, -255.0L, 256.0L, -256.0L, 512.0L, -512.0L, 2048.0L, -2048.0L,
		4096.0L, -4096.0L, 32768.0L, -32768.0L, 65504.0L, -65504.0L,
		0.001953125L, -0.001953125L, 6.103515625e-05L, -6.103515625e-05L};

#define SMF_CORPUS_N ((int)(sizeof smf_corpus / sizeof smf_corpus[0]))

static long smf_sweep(SmBits *digest)
{
	int t, op, i, j;
	long n = 0;
	for (t = 0; t < SMF_T_COUNT; t++)
		for (op = 0; op < SMF_O_COUNT; op++)
			for (i = 0; i < SMF_CORPUS_N; i++)
				for (j = 0; j < SMF_CORPUS_N; j++) {
					SmBits lo, hi;
					smf_enc(t, smf_run(t, op, smf_corpus[i], smf_corpus[j],
														 smf_corpus[(i + j + 1) % SMF_CORPUS_N]),
									&lo, &hi);
					*digest = (*digest ^ (SmBits)SMF_KEY(t, op)) * 1099511628211ull;
					*digest = (*digest ^ lo) * 1099511628211ull;
					*digest = (*digest ^ hi) * 1099511628211ull;
					n++;
				}
	return n;
}

static const char *const smf_type_name[] = {
#define SMF_TN(tag, cty, w) #tag,
		SMF_FTYPES(SMF_TN)
#undef SMF_TN
};

static const char *const smf_op_name[] = {
#define SMF_ON(a, op) #op,
		SMF_OPS(SMF_ON, _)
#undef SMF_ON
};

static void smc_run(int tag, int op, double ar, double ai, double br, double bi,
										double *rr, double *ri)
{
	if (tag == SMC_T_C32) {
		volatile float _Complex x = (float)ar + (float _Complex)(__extension__ 1.0i) * (float)ai;
		volatile float _Complex y = (float)br + (float _Complex)(__extension__ 1.0i) * (float)bi;
		float _Complex r;
		switch (op) {
		case SMC_O_CADD: r = x + y; break;
		case SMC_O_CSUB: r = x - y; break;
		case SMC_O_CMUL: r = x * y; break;
		case SMC_O_CNEG: r = -x; break;
		case SMC_O_CCONJ:
			r = (float _Complex)(__real__ x) -
					(float _Complex)(__extension__ 1.0i) * (float)(__imag__ x);
			break;
		case SMC_O_CSCALE: r = x * (float)br; break;
		case SMC_O_CSELR: r = ((float)ar < (float)br) ? x : y; break;
		default: r = (ar != 0.0) ? y : x; break;
		}
		*rr = (double)__real__ r;
		*ri = (double)__imag__ r;
		return;
	}
	{
		volatile double _Complex x = ar + (double _Complex)(__extension__ 1.0i) * ai;
		volatile double _Complex y = br + (double _Complex)(__extension__ 1.0i) * bi;
		double _Complex r;
		switch (op) {
		case SMC_O_CADD: r = x + y; break;
		case SMC_O_CSUB: r = x - y; break;
		case SMC_O_CMUL: r = x * y; break;
		case SMC_O_CNEG: r = -x; break;
		case SMC_O_CCONJ:
			r = (double _Complex)(__real__ x) -
					(double _Complex)(__extension__ 1.0i) * (double)(__imag__ x);
			break;
		case SMC_O_CSCALE: r = x * br; break;
		case SMC_O_CSELR: r = (ar < br) ? x : y; break;
		default: r = (ar != 0.0) ? y : x; break;
		}
		*rr = __real__ r;
		*ri = __imag__ r;
	}
}

static const double smc_corpus[] = {
		0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 0.25, -0.25,
		0.75, -0.75, 1.25, -1.25, 1.5, -1.5, 1.75, -1.75,
		2.0, -2.0, 2.25, -2.25, 3.0, -3.0, 4.0, -4.0,
		5.5, -5.5, 6.0, -6.0, 7.5, -7.5, 8.0, -8.0,
		12.0, -12.0, 16.0, -16.0, 31.0, -31.0, 64.0, -64.0,
		128.0, -128.0, 256.0, -256.0, 1024.0, -1024.0, 4096.0, -4096.0};

#define SMC_CORPUS_N ((int)(sizeof smc_corpus / sizeof smc_corpus[0]))

static long smc_sweep(SmBits *digest)
{
	int t, op, i, j;
	long n = 0;
	for (t = 0; t < SMC_T_COUNT; t++)
		for (op = 0; op < SMC_O_COUNT; op++)
			for (i = 0; i < SMC_CORPUS_N; i++)
				for (j = 0; j < SMC_CORPUS_N; j++) {
					double gr = 0, gi = 0;
					SmBits a1, a2;
					smc_run(t, op, smc_corpus[i], smc_corpus[j], smc_corpus[j],
									smc_corpus[(i + j + 1) % SMC_CORPUS_N], &gr, &gi);
					memcpy(&a1, &gr, 8);
					memcpy(&a2, &gi, 8);
					*digest = (*digest ^ (SmBits)(t * SMC_O_COUNT + op)) *
										1099511628211ull;
					*digest = (*digest ^ a1) * 1099511628211ull;
					*digest = (*digest ^ a2) * 1099511628211ull;
					n++;
				}
	return n;
}

static const char *const smc_type_name[] = {"C32", "C64"};

static const char *const smc_op_name[] = {
#define SMC_ON(a, op) #op,
		SMC_OPS(SMC_ON, _)
#undef SMC_ON
};

static void smw_run(int tag, int op, SmBits ahi, SmBits alo, SmBits bhi,
										SmBits blo, SmBits *whi, SmBits *wlo)
{
	*whi = 0;
	*wlo = 0;
#if SMF_HAVE_I128
	if (tag == SMW_T_S128 || tag == SMW_T_U128) {
		volatile SmU128 a = ((SmU128)ahi << 64) | (SmU128)alo;
		volatile SmU128 b = ((SmU128)bhi << 64) | (SmU128)blo;
		SmU128 r = 0;
		int sh = (int)blo;
		if (tag == SMW_T_S128) {
			volatile SmS128 sa = (SmS128)a, sb = (SmS128)b;
			switch (op) {
			case SMW_O_ADD: r = (SmU128)(SmS128)(sa + sb); break;
			case SMW_O_SUB: r = (SmU128)(SmS128)(sa - sb); break;
			case SMW_O_MUL: r = (SmU128)(SmS128)(sa * sb); break;
			case SMW_O_NEG: r = (SmU128)(SmS128)(-sa); break;
			case SMW_O_SHL: r = (SmU128)(SmS128)(sa << sh); break;
			default: r = (SmU128)(SmS128)(sa >> sh); break;
			}
		} else {
			switch (op) {
			case SMW_O_ADD: r = a + b; break;
			case SMW_O_SUB: r = a - b; break;
			case SMW_O_MUL: r = a * b; break;
			case SMW_O_NEG: r = (SmU128)0 - a; break;
			case SMW_O_SHL: r = a << sh; break;
			default: r = a >> sh; break;
			}
		}
		*whi = (SmBits)(r >> 64);
		*wlo = (SmBits)r;
		return;
	}
#endif
#if SMF_HAVE_I256
	if (tag == SMW_T_S256 || tag == SMW_T_U256) {
		volatile unsigned __int256 a =
				((unsigned __int256)ahi << 64) | (unsigned __int256)alo;
		volatile unsigned __int256 b =
				((unsigned __int256)bhi << 64) | (unsigned __int256)blo;
		unsigned __int256 r = 0;
		int sh = (int)blo;
		if (tag == SMW_T_S256) {
			volatile __int256 sa = (__int256)a, sb = (__int256)b;
			switch (op) {
			case SMW_O_ADD: r = (unsigned __int256)(__int256)(sa + sb); break;
			case SMW_O_SUB: r = (unsigned __int256)(__int256)(sa - sb); break;
			case SMW_O_MUL: r = (unsigned __int256)(__int256)(sa * sb); break;
			case SMW_O_NEG: r = (unsigned __int256)(__int256)(-sa); break;
			case SMW_O_SHL: r = (unsigned __int256)(__int256)(sa << sh); break;
			default: r = (unsigned __int256)(__int256)(sa >> sh); break;
			}
		} else {
			switch (op) {
			case SMW_O_ADD: r = a + b; break;
			case SMW_O_SUB: r = a - b; break;
			case SMW_O_MUL: r = a * b; break;
			case SMW_O_NEG: r = (unsigned __int256)0 - a; break;
			case SMW_O_SHL: r = a << sh; break;
			default: r = a >> sh; break;
			}
		}
		*whi = (SmBits)(unsigned long long)(r >> 64);
		*wlo = (SmBits)(unsigned long long)r;
		return;
	}
#endif
	(void)op;
	(void)ahi;
	(void)alo;
	(void)bhi;
	(void)blo;
}

#define SMF_ROWS_N ((int)(sizeof smf_rows / sizeof smf_rows[0]))
#define SMC_ROWS_N ((int)(sizeof smc_rows / sizeof smc_rows[0]))
#define SMW_ROWS_N ((int)(sizeof smw_rows / sizeof smw_rows[0]))

static int smf_rows_count(void)
{
	return SMF_ROWS_N + SMC_ROWS_N + SMW_ROWS_N;
}

static long smf_rows_run(long *checks, long *failures, long *reported,
												 int poison)
{
	long n = 0;
	int i;
	for (i = 0; i < SMF_ROWS_N; i++) {
		const SmFRow *r = &smf_rows[i];
		SmBits wl, wh, gl, gh;
		long double want = r->want;
		if (poison && i == 0)
			want = want + (long double)1;
		smf_enc(r->tag, want, &wl, &wh);
		if (r->foldable) {
			smf_enc(r->tag, r->fold, &gl, &gh);
			(*checks)++;
			n++;
			if (gl != wl || gh != wh) {
				(*failures)++;
				if ((*reported)++ < 40)
					printf("FAIL ffold %s got=%016llx:%016llx want=%016llx:%016llx\n",
								 r->name, (unsigned long long)gh, (unsigned long long)gl,
								 (unsigned long long)wh, (unsigned long long)wl);
			}
		}
		smf_enc(r->tag, smf_run(r->tag, r->op, r->a, r->b, r->c), &gl, &gh);
		(*checks)++;
		n++;
		if (gl != wl || gh != wh) {
			(*failures)++;
			if ((*reported)++ < 40)
				printf("FAIL frun %s got=%016llx:%016llx want=%016llx:%016llx\n",
							 r->name, (unsigned long long)gh, (unsigned long long)gl,
							 (unsigned long long)wh, (unsigned long long)wl);
		}
	}
	for (i = 0; i < SMC_ROWS_N; i++) {
		const SmCRow *r = &smc_rows[i];
		double gr = 0, gi = 0;
		SmBits a1, a2, b1, b2;
		smc_run(r->tag, r->op, r->ar, r->ai, r->br, r->bi, &gr, &gi);
		memcpy(&a1, &gr, 8);
		memcpy(&a2, &gi, 8);
		{
			double wr = r->wantr, wi = r->wanti;
			if (r->tag == SMC_T_C32) {
				wr = (double)(float)wr;
				wi = (double)(float)wi;
			}
			memcpy(&b1, &wr, 8);
			memcpy(&b2, &wi, 8);
		}
		(*checks)++;
		n++;
		if (a1 != b1 || a2 != b2) {
			(*failures)++;
			if ((*reported)++ < 40)
				printf("FAIL crun %s got=%016llx:%016llx want=%016llx:%016llx\n",
							 r->name, (unsigned long long)a1, (unsigned long long)a2,
							 (unsigned long long)b1, (unsigned long long)b2);
		}
	}
	for (i = 0; i < SMW_ROWS_N; i++) {
		const SmWRow *r = &smw_rows[i];
		SmBits gh = 0, gl = 0;
		smw_run(r->tag, r->op, r->ahi, r->alo, r->bhi, r->blo, &gh, &gl);
		(*checks)++;
		n++;
		if (gh != r->whi || gl != r->wlo) {
			(*failures)++;
			if ((*reported)++ < 40)
				printf("FAIL wrun %s got=%016llx:%016llx want=%016llx:%016llx\n",
							 r->name, (unsigned long long)gh, (unsigned long long)gl,
							 (unsigned long long)r->whi, (unsigned long long)r->wlo);
		}
	}
	return n;
}

static void smf_row_dump(void)
{
	int i;
	for (i = 0; i < SMF_ROWS_N; i++) {
		const SmFRow *r = &smf_rows[i];
		SmBits fl, fh, rl, rh, wl, wh;
		smf_enc(r->tag, r->fold, &fl, &fh);
		smf_enc(r->tag, smf_run(r->tag, r->op, r->a, r->b, r->c), &rl, &rh);
		smf_enc(r->tag, r->want, &wl, &wh);
		printf("F %s %d %016llx%016llx %016llx%016llx %016llx%016llx\n", r->name,
					 (int)r->foldable, (unsigned long long)fh, (unsigned long long)fl,
					 (unsigned long long)rh, (unsigned long long)rl,
					 (unsigned long long)wh, (unsigned long long)wl);
	}
	for (i = 0; i < SMC_ROWS_N; i++) {
		const SmCRow *r = &smc_rows[i];
		double gr = 0, gi = 0;
		SmBits a1, a2;
		smc_run(r->tag, r->op, r->ar, r->ai, r->br, r->bi, &gr, &gi);
		memcpy(&a1, &gr, 8);
		memcpy(&a2, &gi, 8);
		printf("C %s %016llx %016llx\n", r->name, (unsigned long long)a1,
					 (unsigned long long)a2);
	}
	for (i = 0; i < SMW_ROWS_N; i++) {
		const SmWRow *r = &smw_rows[i];
		SmBits gh = 0, gl = 0;
		smw_run(r->tag, r->op, r->ahi, r->alo, r->bhi, r->blo, &gh, &gl);
		printf("W %s 0 %016llx%016llx %016llx%016llx %016llx%016llx\n", r->name,
					 (unsigned long long)r->whi, (unsigned long long)r->wlo,
					 (unsigned long long)gh, (unsigned long long)gl,
					 (unsigned long long)r->whi, (unsigned long long)r->wlo);
	}
	{
		int t, op, k, m;
		for (t = 0; t < SMF_T_COUNT; t++)
			for (op = 0; op < SMF_O_COUNT; op++) {
				SmBits h = 14695981039346656037ull, lo, hi;
				for (k = 0; k < SMF_CORPUS_N; k++)
					for (m = 0; m < SMF_CORPUS_N; m++) {
						smf_enc(t, smf_run(t, op, smf_corpus[k], smf_corpus[m],
															 smf_corpus[(k + m + 1) % SMF_CORPUS_N]),
										&lo, &hi);
						h = ((h ^ lo) * 1099511628211ull ^ hi) * 1099511628211ull;
					}
				printf("V fsweep.%s.%s 0 %016llx %016llx %016llx\n", smf_type_name[t],
							 smf_op_name[op], (unsigned long long)h, (unsigned long long)h,
							 (unsigned long long)h);
			}
	}
	{
		int t, op, k, m;
		for (t = 0; t < SMC_T_COUNT; t++)
			for (op = 0; op < SMC_O_COUNT; op++) {
				SmBits h = 14695981039346656037ull, a1, a2;
				for (k = 0; k < SMC_CORPUS_N; k++)
					for (m = 0; m < SMC_CORPUS_N; m++) {
						double gr = 0, gi = 0;
						smc_run(t, op, smc_corpus[k], smc_corpus[m], smc_corpus[m],
										smc_corpus[(k + m + 1) % SMC_CORPUS_N], &gr, &gi);
						memcpy(&a1, &gr, 8);
						memcpy(&a2, &gi, 8);
						h = ((h ^ a1) * 1099511628211ull ^ a2) * 1099511628211ull;
					}
				printf("V csweep.%s.%s 0 %016llx %016llx %016llx\n", smc_type_name[t],
							 smc_op_name[op], (unsigned long long)h, (unsigned long long)h,
							 (unsigned long long)h);
			}
	}
}

static void smf_digest_dump(void)
{
	int t, op, i, j;
	printf("D ext rows %d %d %d\n", SMF_ROWS_N, SMC_ROWS_N, SMW_ROWS_N);
	for (t = 0; t < SMF_T_COUNT; t++)
		for (op = 0; op < SMF_O_COUNT; op++) {
			SmBits h = 14695981039346656037ull, lo, hi;
			for (i = 0; i < SMF_CORPUS_N; i++)
				for (j = 0; j < SMF_CORPUS_N; j++) {
					smf_enc(t, smf_run(t, op, smf_corpus[i], smf_corpus[j],
														 smf_corpus[(i + j + 1) % SMF_CORPUS_N]),
									&lo, &hi);
					h = ((h ^ lo) * 1099511628211ull ^ hi) * 1099511628211ull;
				}
			printf("D flt %s %s %016llx\n", smf_type_name[t], smf_op_name[op],
						 (unsigned long long)h);
		}
}

#endif
