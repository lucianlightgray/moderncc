#ifndef MCC_SMOKE_FCASES_H
#define MCC_SMOKE_FCASES_H

#include <float.h>
#include <string.h>
#include <stdio.h>

#include "smoke.h"

#if defined __SIZEOF_INT256__
#define SMF_HAVE_I256 1
#else
#define SMF_HAVE_I256 0
#endif

#if defined __SIZEOF_INT128__
#define SMF_HAVE_I128 1
#else
#define SMF_HAVE_I128 0
#endif

#if defined __i386__ || defined __x86_64__
#define SMF_FTOI_INDEFINITE 1
#else
#define SMF_FTOI_INDEFINITE 0
#endif

#define SMF_FTYPES(X) \
	X(F16, _Float16, 2) \
	X(F32, float, 4) \
	X(F64, double, 8) \
	X(F80, long double, 10)

#define SMF_FTYPES2(X) \
	X(F16, _Float16, 2) \
	X(F32, float, 4) \
	X(F64, double, 8) \
	X(F80, long double, 10)

#define SMF_CTY_F16 _Float16
#define SMF_CTY_F32 float
#define SMF_CTY_F64 double
#define SMF_CTY_F80 long double

#define SMF_MAXV_F16 65504.0
#define SMF_MINV_F16 6.103515625e-05
#define SMF_TMINV_F16 5.9604644775390625e-08
#define SMF_EPSV_F16 9.765625e-04

#define SMF_MAXV_F32 FLT_MAX
#define SMF_MINV_F32 FLT_MIN
#define SMF_TMINV_F32 FLT_TRUE_MIN
#define SMF_EPSV_F32 FLT_EPSILON

#define SMF_MAXV_F64 DBL_MAX
#define SMF_MINV_F64 DBL_MIN
#define SMF_TMINV_F64 DBL_TRUE_MIN
#define SMF_EPSV_F64 DBL_EPSILON

#define SMF_MAXV_F80 LDBL_MAX
#define SMF_MINV_F80 LDBL_MIN
#define SMF_TMINV_F80 LDBL_TRUE_MIN
#define SMF_EPSV_F80 LDBL_EPSILON

#define SMF_OPS(X, A) \
	X(A, FADD) X(A, FSUB) X(A, FMUL) X(A, FDIV) \
	X(A, FNEG) X(A, FSEL) X(A, FLT) X(A, FEQ) X(A, FABS) \
	X(A, FSELI) X(A, FSELZ) X(A, FSELMIXL) X(A, FSELMIXR) X(A, FSELCMP) \
	X(A, FMULADD) X(A, FDIVSEL) X(A, FMIN3) X(A, FNEST) X(A, FSCALE) \
	X(A, FSUBABS) X(A, FSELIC) X(A, FSELMIXB) X(A, FSELMIXN) X(A, FCMPMIX) \
	X(A, FSELIR) X(A, FSELZR) X(A, FSELIL) X(A, FSELZL) X(A, FSELIX) \
	X(A, FSELT) X(A, FRECEQ) X(A, FNESELF)

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
#define SMF_EXPR_FSELIR(T, a, b, c) ((T)(a) <= (T)(b) ? 0 : (T)(a) - (T)(b))
#define SMF_EXPR_FSELZR(T, a, b, c) ((T)(a) <= (T)(b) ? 0.0 : (T)(a) - (T)(b))
#define SMF_EXPR_FSELIL(T, a, b, c) ((T)(a) > (T)(b) ? 0 : (T)(a) - (T)(b))
#define SMF_EXPR_FSELZL(T, a, b, c) ((T)(a) > (T)(b) ? 0.0 : (T)(a) - (T)(b))
#define SMF_EXPR_FSELIX(T, a, b, c) \
	((T)(((T)(a) > (T)(b) ? (T)(a) - (T)(b) : 0) * (T)2))
#define SMF_EXPR_FSELT(T, a, b, c) ((T)(a) > (T)(b) ? (T)(a) - (T)(b) : (T)0)
#define SMF_EXPR_FRECEQ(T, a, b, c) \
	((T)((T)((T)1 / (T)(a)) == (T)((T)1 / (T)(b))))
#define SMF_EXPR_FNESELF(T, a, b, c) ((T)((T)(a) != (T)(a)))

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

#define SMC_OPS(X, A) \
	X(A, CADD) X(A, CSUB) X(A, CMUL) X(A, CSEL) \
	X(A, CNEG) X(A, CCONJ) X(A, CSCALE) X(A, CSELR) \
	X(A, CDIV) X(A, CEQ) X(A, CNE) X(A, CMULADD) \
	X(A, CDIVSEL) X(A, CSELMIX) X(A, CABS2) X(A, CIMUL)

enum {
#define SMC_O_ROW(a, op) SMC_O_##op,
	SMC_OPS(SMC_O_ROW, _)
#undef SMC_O_ROW
			SMC_O_COUNT
};

#define SMC_CTY_C32 float
#define SMC_CTY_C64 double
#define SMC_CTY_C80 long double

#if !defined(__APPLE__) && defined(__FLT16_MANT_DIG__)
#define SMC_HAVE_C16 1
#else
#define SMC_HAVE_C16 0
#endif

#if SMC_HAVE_C16
#define SMC_CTY_C16 _Float16
enum { SMC_T_C32, SMC_T_C64, SMC_T_C80, SMC_T_C16, SMC_T_COUNT };
#else
enum { SMC_T_C32, SMC_T_C64, SMC_T_C80, SMC_T_COUNT };
#endif

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
		SMF_ROW("f32.add.negzero", F32, FADD, -0.0f, 0.0f, 0.0f, 0.0f)
		SMF_ROW("f32.add.negzero2", F32, FADD, -0.0f, -0.0f, 0.0f, -0.0f)
		SMF_ROW("f80.add.negzero2", F80, FADD, -0.0L, -0.0L, 0.0L, -0.0L)
		SMF_ROW("f16.add.negzero2", F16, FADD, -0.0, -0.0, 0.0, -0.0)
		SMF_ROW("f80.mul.negzero", F80, FMUL, -1.0L, 0.0L, 0.0L, -0.0L)
		SMF_ROW("f16.mul.negzero", F16, FMUL, -1.0, 0.0, 0.0, -0.0)
		SMF_ROW("f32.sub.negzero", F32, FSUB, 0.0f, 0.0f, 0.0f, 0.0f)
		SMF_ROW("f64.receq.zeros", F64, FRECEQ, 0.0, -0.0, 0.0, 0.0)
		SMF_ROW("f64.receq.same", F64, FRECEQ, 0.0, 0.0, 0.0, 1.0)
		SMF_ROW("f32.receq.zeros", F32, FRECEQ, 0.0f, -0.0f, 0.0f, 0.0f)
		SMF_ROW("f80.receq.zeros", F80, FRECEQ, 0.0L, -0.0L, 0.0L, 0.0L)
		SMF_ROW("f16.receq.zeros", F16, FRECEQ, 0.0, -0.0, 0.0, 0.0)
		SMF_ROW("f64.neself.finite", F64, FNESELF, 1.5, 0.0, 0.0, 0.0)
		SMF_ROW("f32.neself.finite", F32, FNESELF, 1.5f, 0.0f, 0.0f, 0.0f)
		SMF_ROW("f80.neself.finite", F80, FNESELF, 1.5L, 0.0L, 0.0L, 0.0L)
		SMF_ROW("f16.neself.finite", F16, FNESELF, 1.5, 0.0, 0.0, 0.0)

		SMF_ROW("f64.lt.negzero", F64, FLT, -0.0, 0.0, 0.0, 0.0)
		SMF_ROW("f64.eq.negzero", F64, FEQ, -0.0, 0.0, 0.0, 1.0)
		SMF_ROW("f32.eq.negzero", F32, FEQ, -0.0f, 0.0f, 0.0f, 1.0f)
		SMF_ROW("f80.eq.negzero", F80, FEQ, -0.0L, 0.0L, 0.0L, 1.0L)
		SMF_ROW("f16.eq.negzero", F16, FEQ, -0.0, 0.0, 0.0, 1.0)
		SMF_ROW("f32.lt.negzero", F32, FLT, -0.0f, 0.0f, 0.0f, 0.0f)
		SMF_ROW("f80.lt.negzero", F80, FLT, -0.0L, 0.0L, 0.0L, 0.0L)
		SMF_ROW("f16.lt.negzero", F16, FLT, -0.0, 0.0, 0.0, 0.0)

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

		SMF_ROW("f64.tern.int0.rev.taken", F64, FSELIR, 5.5, 2.25, 0.0, 3.25)
		SMF_ROW("f64.tern.int0.rev.else", F64, FSELIR, 2.25, 5.5, 0.0, 0.0)
		SMF_ROW("f64.tern.dbl0.rev.taken", F64, FSELZR, 5.5, 2.25, 0.0, 3.25)
		SMF_ROW("f64.tern.dbl0.rev.else", F64, FSELZR, 2.25, 5.5, 0.0, 0.0)
		SMF_ROW("f64.tern.int0.left", F64, FSELIL, 5.5, 2.25, 0.0, 0.0)
		SMF_ROW("f64.tern.int0.left.else", F64, FSELIL, 2.25, 5.5, 0.0, -3.25)
		SMF_ROW("f64.tern.dbl0.left", F64, FSELZL, 5.5, 2.25, 0.0, 0.0)
		SMF_ROW("f64.tern.dbl0.left.else", F64, FSELZL, 2.25, 5.5, 0.0, -3.25)
		SMF_ROW("f64.tern.typed0", F64, FSELT, 5.5, 2.25, 0.0, 3.25)
		SMF_ROW("f64.tern.typed0.else", F64, FSELT, 2.25, 5.5, 0.0, 0.0)
		SMF_ROW("f64.tern.scaled", F64, FSELIX, 5.5, 2.25, 0.0, 6.5)
		SMF_ROW("f64.tern.scaled.else", F64, FSELIX, 2.25, 5.5, 0.0, 0.0)
		SMF_ROW("f64.tern.int0.negzero", F64, FSELI, 2.25, 2.25, 0.0, 0.0)
		SMF_ROW("f64.tern.dbl0.negzero", F64, FSELZ, 2.25, 2.25, 0.0, 0.0)

		SMF_ROW("f32.tern.int0.rev.taken", F32, FSELIR, 5.5f, 2.25f, 0.0f, 3.25f)
		SMF_ROW("f32.tern.int0.rev.else", F32, FSELIR, 2.25f, 5.5f, 0.0f, 0.0f)
		SMF_ROW("f32.tern.dbl0.rev.taken", F32, FSELZR, 5.5f, 2.25f, 0.0f, 3.25f)
		SMF_ROW("f32.tern.dbl0.rev.else", F32, FSELZR, 2.25f, 5.5f, 0.0f, 0.0f)
		SMF_ROW("f32.tern.int0.left", F32, FSELIL, 5.5f, 2.25f, 0.0f, 0.0f)
		SMF_ROW("f32.tern.int0.left.else", F32, FSELIL, 2.25f, 5.5f, 0.0f, -3.25f)
		SMF_ROW("f32.tern.dbl0.left", F32, FSELZL, 5.5f, 2.25f, 0.0f, 0.0f)
		SMF_ROW("f32.tern.dbl0.left.else", F32, FSELZL, 2.25f, 5.5f, 0.0f, -3.25f)
		SMF_ROW("f32.tern.typed0", F32, FSELT, 5.5f, 2.25f, 0.0f, 3.25f)
		SMF_ROW("f32.tern.typed0.else", F32, FSELT, 2.25f, 5.5f, 0.0f, 0.0f)
		SMF_ROW("f32.tern.scaled", F32, FSELIX, 5.5f, 2.25f, 0.0f, 6.5f)
		SMF_ROW("f32.tern.scaled.else", F32, FSELIX, 2.25f, 5.5f, 0.0f, 0.0f)

		SMF_ROW("f80.tern.int0.rev.taken", F80, FSELIR, 5.5L, 2.25L, 0.0L, 3.25L)
		SMF_ROW("f80.tern.int0.rev.else", F80, FSELIR, 2.25L, 5.5L, 0.0L, 0.0L)
		SMF_ROW("f80.tern.dbl0.rev.taken", F80, FSELZR, 5.5L, 2.25L, 0.0L, 3.25L)
		SMF_ROW("f80.tern.dbl0.rev.else", F80, FSELZR, 2.25L, 5.5L, 0.0L, 0.0L)
		SMF_ROW("f80.tern.int0.left", F80, FSELIL, 5.5L, 2.25L, 0.0L, 0.0L)
		SMF_ROW("f80.tern.int0.left.else", F80, FSELIL, 2.25L, 5.5L, 0.0L, -3.25L)
		SMF_ROW("f80.tern.dbl0.left", F80, FSELZL, 5.5L, 2.25L, 0.0L, 0.0L)
		SMF_ROW("f80.tern.dbl0.left.else", F80, FSELZL, 2.25L, 5.5L, 0.0L, -3.25L)
		SMF_ROW("f80.tern.typed0", F80, FSELT, 5.5L, 2.25L, 0.0L, 3.25L)
		SMF_ROW("f80.tern.typed0.else", F80, FSELT, 2.25L, 5.5L, 0.0L, 0.0L)
		SMF_ROW("f80.tern.scaled", F80, FSELIX, 5.5L, 2.25L, 0.0L, 6.5L)
		SMF_ROW("f80.tern.dbl0.frac", F80, FSELZ, 1.5L, 1.25L, 0.0L, 0.25L)

		SMF_ROW("f16.tern.int0.rev.taken", F16, FSELIR, 5.5, 2.25, 0.0, 3.25)
		SMF_ROW("f16.tern.int0.rev.else", F16, FSELIR, 2.25, 5.5, 0.0, 0.0)
		SMF_ROW("f16.tern.dbl0.rev.taken", F16, FSELZR, 5.5, 2.25, 0.0, 3.25)
		SMF_ROW("f16.tern.dbl0.rev.else", F16, FSELZR, 2.25, 5.5, 0.0, 0.0)
		SMF_ROW("f16.tern.int0.left", F16, FSELIL, 5.5, 2.25, 0.0, 0.0)
		SMF_ROW("f16.tern.int0.left.else", F16, FSELIL, 2.25, 5.5, 0.0, -3.25)
		SMF_ROW("f16.tern.dbl0.left", F16, FSELZL, 5.5, 2.25, 0.0, 0.0)
		SMF_ROW("f16.tern.dbl0.left.else", F16, FSELZL, 2.25, 5.5, 0.0, -3.25)
		SMF_ROW("f16.tern.typed0", F16, FSELT, 5.5, 2.25, 0.0, 3.25)
		SMF_ROW("f16.tern.typed0.else", F16, FSELT, 2.25, 5.5, 0.0, 0.0)
		SMF_ROW("f16.tern.scaled", F16, FSELIX, 5.5, 2.25, 0.0, 6.5)
		SMF_ROW("f16.tern.dbl0.frac", F16, FSELZ, 1.5, 1.25, 0.0, 0.25)

		SMF_ROW("f64.denorm.add", F64, FADD, 4.9406564584124654e-324,
						4.9406564584124654e-324, 0.0, 9.8813129168249309e-324)
		SMF_ROW("f32.denorm.add", F32, FADD, 1.4012984643e-45f, 1.4012984643e-45f,
						0.0f, 2.8025969286e-45f)
		SMF_ROW("f64.denorm.boundary", F64, FSUB, 2.2250738585072014e-308,
						4.9406564584124654e-324, 0.0, 2.225073858507201e-308)
		SMF_ROW("f64.denorm.tonormal", F64, FADD, 2.225073858507201e-308,
						4.9406564584124654e-324, 0.0, 2.2250738585072014e-308)
		SMF_ROW("f64.denorm.halve", F64, FDIV, 4.9406564584124654e-324, 2.0, 0.0,
						0.0)
		SMF_ROW("f64.denorm.mul2", F64, FMUL, 4.9406564584124654e-324, 2.0, 0.0,
						9.8813129168249309e-324)
		SMF_ROW("f32.denorm.boundary", F32, FSUB, 1.17549435082228751e-38f,
						1.40129846432481707e-45f, 0.0f, 1.17549421069244108e-38f)
		SMF_ROW("f32.denorm.halve", F32, FDIV, 1.40129846432481707e-45f, 2.0f, 0.0f,
						0.0f)
		SMF_ROW("f16.denorm.min", F16, FADD, 0.0, 5.9604644775390625e-08, 0.0,
						5.9604644775390625e-08)
		SMF_ROW("f16.denorm.halve", F16, FDIV, 5.9604644775390625e-08, 2.0, 0.0, 0.0)
		SMF_ROW("f16.denorm.boundary", F16, FSUB, 6.103515625e-05,
						5.9604644775390625e-08, 0.0, 6.097555160522461e-05)
		SMF_ROW("f16.denorm.tonormal", F16, FADD, 6.097555160522461e-05,
						5.9604644775390625e-08, 0.0, 6.103515625e-05)
		SMF_ROW("f80.denorm.halve", F80, FDIV, 3.64519953188247460253e-4951L, 2.0L,
						0.0L, 0.0L)
		SMF_ROW("f80.denorm.mul2", F80, FMUL, 3.64519953188247460253e-4951L, 2.0L,
						0.0L, 7.29039906376494920506e-4951L)

		SMF_ROW("f16.round.half.even", F16, FADD, 2048.0, 1.0, 0.0, 2048.0)
		SMF_ROW("f16.round.half.up", F16, FADD, 2048.0, 3.0, 0.0, 2052.0)
		SMF_ROW("f16.round.eps", F16, FADD, 1.0, 9.765625e-04, 0.0, 1.0009765625)
		SMF_ROW("f16.round.halfeps", F16, FADD, 1.0, 4.8828125e-04, 0.0, 1.0)
		SMF_ROW("f32.round.half.even", F32, FADD, 16777216.0f, 1.0f, 0.0f,
						16777216.0f)
		SMF_ROW("f32.round.half.up", F32, FADD, 16777216.0f, 3.0f, 0.0f, 16777220.0f)
		SMF_ROW("f32.round.eps", F32, FADD, 1.0f, 1.1920928955078125e-07f, 0.0f,
						1.00000011920928955f)
		SMF_ROW("f32.round.halfeps", F32, FADD, 1.0f, 5.9604644775390625e-08f, 0.0f,
						1.0f)
		SMF_ROW("f64.round.half.even", F64, FADD, 9007199254740992.0, 1.0, 0.0,
						9007199254740992.0)
		SMF_ROW("f64.round.half.up", F64, FADD, 9007199254740992.0, 3.0, 0.0,
						9007199254740996.0)
		SMF_ROW("f64.round.eps", F64, FADD, 1.0, 2.220446049250313e-16, 0.0,
						1.0000000000000002)
		SMF_ROW("f64.round.halfeps", F64, FADD, 1.0, 1.1102230246251565e-16, 0.0,
						1.0)
		SMF_ROW("f80.round.half.even", F80, FADD, 18446744073709551616.0L, 1.0L,
						0.0L, 18446744073709551616.0L)
		SMF_ROW("f80.round.eps", F80, FADD, 1.0L, 1.0842021724855044340e-19L, 0.0L,
						1.00000000000000000010842021724855044340L)
		SMF_ROW("f80.round.halfeps", F80, FADD, 1.0L, 5.4210108624275221700e-20L,
						0.0L, 1.0L)

		SMF_ROW("f16.narrow.max", F16, FADD, 65504.0, 0.0, 0.0, 65504.0)
		SMF_ROW("f16.narrow.belowmax", F16, FADD, 65472.0, 0.0, 0.0, 65472.0)
		SMF_ROW("f16.narrow.tie", F16, FADD, 32752.0, 0.0, 0.0, 32752.0)
		SMF_ROW("f16.narrow.small", F16, FMUL, 5.9604644775390625e-08, 0.5, 0.0, 0.0)
		SMF_ROW("f16.narrow.subhalf", F16, FMUL, 1.19209289550781250e-07, 0.5, 0.0,
						5.9604644775390625e-08)
		SMF_ROW("f32.narrow.f64max", F32, FADD, 3.4028234663852886e38f, 0.0f, 0.0f,
						3.4028234663852886e38f)
		SMF_ROW("f80.narrow.f64max", F80, FADD, 1.7976931348623157e308L, 0.0L, 0.0L,
						1.7976931348623157e308L)

		SMF_ROW("f64.scale.exact", F64, FSCALE, 1.5, 1.0, 0.25, 2.75)
		SMF_ROW("f32.scale.exact", F32, FSCALE, 1.5f, 1.0f, 0.25f, 2.75f)
		SMF_ROW("f80.scale.exact", F80, FSCALE, 1.5L, 1.0L, 0.25L, 2.75L)
		SMF_ROW("f16.scale.exact", F16, FSCALE, 1.5, 1.0, 0.25, 2.75)
		SMF_ROW("f64.muladd.exact", F64, FMULADD, 2.25, 4.0, 0.5, 9.5)
		SMF_ROW("f32.muladd.exact", F32, FMULADD, 2.25f, 4.0f, 0.5f, 9.5f)
		SMF_ROW("f80.muladd.exact", F80, FMULADD, 2.25L, 4.0L, 0.5L, 9.5L)
		SMF_ROW("f16.muladd.exact", F16, FMULADD, 2.25, 4.0, 0.5, 9.5)
		SMF_ROW_NF("f16.muladd.doubleround", F16, FMULADD, 2.25, 255.0, 0.5, 574.5)
		SMF_ROW_NF("f16.muladd.doubleround.den", F16, FMULADD,
							 5.9604644775390625e-08, 0.5, 5.9604644775390625e-08,
							 5.9604644775390625e-08)
		SMF_ROW_NF("f16.scale.doubleround", F16, FSCALE, 1023.5, 1.0, 0.25, 2046.0)
};

static const SmCRow smc_rows[] = {

		SMC_ROW("c64.add", C64, CADD, 1.5, -2.5, 0.25, 4.0, 1.75, 1.5)
		SMC_ROW("c64.sub", C64, CSUB, 1.5, -2.5, 0.25, 4.0, 1.25, -6.5)
		SMC_ROW("c64.mul", C64, CMUL, 1.0, 2.0, 3.0, 4.0, -5.0, 10.0)
		SMC_ROW("c64.sel.negzero", C64, CSEL, 1.0, 0.0, -0.0, -0.0, -0.0, -0.0)
		SMC_ROW("c32.add", C32, CADD, 1.5, -2.5, 0.25, 4.0, 1.75, 1.5)
		SMC_ROW("c32.mul", C32, CMUL, 1.0, 2.0, 3.0, 4.0, -5.0, 10.0)

		SMC_ROW("c64.div.pow2", C64, CDIV, 1.0, 2.0, 2.0, 0.0, 0.5, 1.0)
		SMC_ROW("c64.div.imag", C64, CDIV, 1.0, 2.0, 0.0, 1.0, 2.0, -1.0)
		SMC_ROW("c64.div.big", C64, CDIV, 0x1p300, 0x1p300, 0x1p150, 0.0, 0x1p150,
						0x1p150)
		SMC_ROW("c64.div.den", C64, CDIV, 0x1p-1040, 0.0, 2.0, 0.0, 0x1p-1041, 0.0)
		SMC_ROW("c64.mul.den", C64, CMUL, 0x1p-1000, 0.0, 0x1p-40, 0.0, 0x1p-1040,
						0.0)
		SMC_ROW("c64.neg.zeros", C64, CNEG, 0.0, -0.0, 0.0, 0.0, -0.0, 0.0)
		SMC_ROW("c64.conj.zero", C64, CCONJ, 1.0, 0.0, 0.0, 0.0, 1.0, -0.0)
		SMC_ROW("c64.conj.negzero", C64, CCONJ, 1.0, -0.0, 0.0, 0.0, 1.0, 0.0)
		SMC_ROW("c64.eq.negzero", C64, CEQ, 0.0, -0.0, -0.0, 0.0, 1.0, 0.0)
		SMC_ROW("c64.ne.same", C64, CNE, 1.5, 2.5, 1.5, 2.5, 0.0, 0.0)
		SMC_ROW("c64.ne.imag", C64, CNE, 1.5, 2.5, 1.5, -2.5, 1.0, 0.0)
		SMC_ROW("c64.muladd", C64, CMULADD, 1.0, 2.0, 3.0, 4.0, -4.0, 12.0)
		SMC_ROW("c64.divsel.zero", C64, CDIVSEL, 1.5, -2.5, 0.0, 0.0, 1.5, -2.5)
		SMC_ROW("c64.divsel.live", C64, CDIVSEL, 1.0, 2.0, 2.0, 0.0, 0.5, 1.0)
		SMC_ROW("c64.selmix.taken", C64, CSELMIX, 5.5, 1.0, 2.25, 0.5, 3.25, 0.5)
		SMC_ROW("c64.selmix.else", C64, CSELMIX, 2.25, 1.0, 5.5, 0.5, 0.0, 0.0)
		SMC_ROW("c64.abs2", C64, CABS2, 3.0, 4.0, 0.0, 0.0, 25.0, 0.0)
		SMC_ROW("c64.imul", C64, CIMUL, 1.0, 2.0, 0.0, 0.0, -2.0, 1.0)
		SMC_ROW("c64.scale.negzero", C64, CSCALE, 1.0, 2.0, -0.0, 0.0, -0.0, -0.0)
		SMC_ROW("c64.selr.lt", C64, CSELR, 1.0, 2.0, 3.0, 4.0, 1.0, 2.0)
		SMC_ROW("c64.selr.ge", C64, CSELR, 3.0, 2.0, 1.0, 4.0, 1.0, 4.0)

		SMC_ROW("c32.sub", C32, CSUB, 1.5, -2.5, 0.25, 4.0, 1.25, -6.5)
		SMC_ROW("c32.div.pow2", C32, CDIV, 1.0, 2.0, 2.0, 0.0, 0.5, 1.0)
		SMC_ROW("c32.div.den", C32, CDIV, 0x1p-140, 0.0, 2.0, 0.0, 0x1p-141, 0.0)
		SMC_ROW("c32.neg.zeros", C32, CNEG, 0.0, -0.0, 0.0, 0.0, -0.0, 0.0)
		SMC_ROW("c32.conj.negzero", C32, CCONJ, 1.0, -0.0, 0.0, 0.0, 1.0, 0.0)
		SMC_ROW("c32.eq.negzero", C32, CEQ, 0.0, -0.0, -0.0, 0.0, 1.0, 0.0)
		SMC_ROW("c32.abs2", C32, CABS2, 3.0, 4.0, 0.0, 0.0, 25.0, 0.0)
		SMC_ROW("c32.imul", C32, CIMUL, 1.0, 2.0, 0.0, 0.0, -2.0, 1.0)
		SMC_ROW("c32.selmix.taken", C32, CSELMIX, 5.5, 1.0, 2.25, 0.5, 3.25, 0.5)
		SMC_ROW("c32.selmix.else", C32, CSELMIX, 2.25, 1.0, 5.5, 0.5, 0.0, 0.0)
		SMC_ROW("c32.muladd", C32, CMULADD, 1.0, 2.0, 3.0, 4.0, -4.0, 12.0)

		SMC_ROW("c80.add", C80, CADD, 1.5L, -2.5L, 0.25L, 4.0L, 1.75L, 1.5L)
		SMC_ROW("c80.mul", C80, CMUL, 1.0L, 2.0L, 3.0L, 4.0L, -5.0L, 10.0L)
		SMC_ROW("c80.div.pow2", C80, CDIV, 1.0L, 2.0L, 2.0L, 0.0L, 0.5L, 1.0L)
		SMC_ROW("c80.neg.zeros", C80, CNEG, 0.0L, -0.0L, 0.0L, 0.0L, -0.0L, 0.0L)
		SMC_ROW("c80.conj.negzero", C80, CCONJ, 1.0L, -0.0L, 0.0L, 0.0L, 1.0L, 0.0L)
		SMC_ROW("c80.eq.negzero", C80, CEQ, 0.0L, -0.0L, -0.0L, 0.0L, 1.0L, 0.0L)
		SMC_ROW("c80.abs2", C80, CABS2, 3.0L, 4.0L, 0.0L, 0.0L, 25.0L, 0.0L)
		SMC_ROW("c80.imul", C80, CIMUL, 1.0L, 2.0L, 0.0L, 0.0L, -2.0L, 1.0L)
		SMC_ROW("c80.selmix.taken", C80, CSELMIX, 5.5L, 1.0L, 2.25L, 0.5L, 3.25L,
						0.5L)
		SMC_ROW("c80.selmix.else", C80, CSELMIX, 2.25L, 1.0L, 5.5L, 0.5L, 0.0L, 0.0L)
		SMC_ROW("c80.muladd", C80, CMULADD, 1.0L, 2.0L, 3.0L, 4.0L, -4.0L, 12.0L)
		SMC_ROW("c80.scale.negzero", C80, CSCALE, 1.0L, 2.0L, -0.0L, 0.0L, -0.0L,
						-0.0L)
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

#if SMF_HAVE_I128
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
#endif

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

#define SMF_B_N 64

#define SMF_BDECL(tag, cty, w) static cty smf_b_##tag[SMF_B_N];
SMF_FTYPES(SMF_BDECL)
#undef SMF_BDECL

static _Float16 smf_snan_F16(void)
{
	unsigned short b = 0x7d00u;
	_Float16 v;
	memcpy(&v, &b, 2);
	return v;
}

static float smf_snan_F32(void)
{
	unsigned int b = 0x7fa00000u;
	float v;
	memcpy(&v, &b, 4);
	return v;
}

static double smf_snan_F64(void)
{
	SmBits b = 0x7ff4000000000000ull;
	double v;
	memcpy(&v, &b, 8);
	return v;
}

static long double smf_snan_F80(void)
{
#if LDBL_MANT_DIG == 64
	unsigned char b[sizeof(long double)];
	long double v;
	memset(b, 0, sizeof b);
	b[0] = 0x01;
	b[7] = 0x80;
	b[8] = 0xff;
	b[9] = 0x7f;
	memcpy(&v, b, sizeof v);
	return v;
#else
	return (long double)smf_snan_F64();
#endif
}

#define SMF_BFILL(TY) \
	static int smf_bfill_##TY(void) \
	{ \
		typedef SMF_CTY_##TY Cty; \
		Cty *v = smf_b_##TY; \
		volatile Cty z = (Cty)0; \
		volatile Cty on = (Cty)1; \
		volatile Cty tw = (Cty)2; \
		volatile Cty mx = (Cty)(SMF_MAXV_##TY); \
		volatile Cty mn = (Cty)(SMF_MINV_##TY); \
		volatile Cty tm = (Cty)(SMF_TMINV_##TY); \
		volatile Cty ep = (Cty)(SMF_EPSV_##TY); \
		volatile Cty t, u, w; \
		volatile long double q; \
		int k = 0; \
		v[k++] = z; \
		v[k++] = -z; \
		v[k++] = tm; \
		v[k++] = -tm; \
		t = tm * tw; \
		v[k++] = t; \
		u = t + tm; \
		v[k++] = u; \
		w = u + t; \
		v[k++] = w; \
		t = mn - tm; \
		v[k++] = t; \
		v[k++] = -t; \
		t = mn / tw; \
		v[k++] = t; \
		v[k++] = mn; \
		v[k++] = -mn; \
		t = mn + tm; \
		v[k++] = t; \
		t = mn * tw; \
		v[k++] = t; \
		u = t + mn; \
		v[k++] = u; \
		v[k++] = mx; \
		v[k++] = -mx; \
		t = on - ep; \
		u = mx * t; \
		v[k++] = u; \
		t = mx / tw; \
		v[k++] = t; \
		t = mx * tw; \
		v[k++] = t; \
		v[k++] = -t; \
		u = t - t; \
		v[k++] = u; \
		v[k++] = -u; \
		w = t / t; \
		v[k++] = w; \
		v[k++] = smf_snan_##TY(); \
		v[k++] = on; \
		v[k++] = -on; \
		t = on + ep; \
		v[k++] = t; \
		t = ep / tw; \
		u = on - t; \
		v[k++] = u; \
		v[k++] = tw; \
		v[k++] = -tw; \
		t = on / tw; \
		v[k++] = t; \
		v[k++] = -t; \
		t = on + tw; \
		v[k++] = t; \
		v[k++] = -t; \
		t = tw * tw; \
		u = on / t; \
		v[k++] = u; \
		t = on / ep; \
		v[k++] = t; \
		v[k++] = -t; \
		u = t * tw; \
		v[k++] = u; \
		w = u + on; \
		v[k++] = w; \
		w = u + tw; \
		v[k++] = w; \
		w = t + on; \
		v[k++] = w; \
		v[k++] = ep; \
		v[k++] = -ep; \
		t = ep / tw; \
		v[k++] = t; \
		q = 0.1L; \
		v[k++] = (Cty)q; \
		q = -0.1L; \
		v[k++] = (Cty)q; \
		q = 0.2L; \
		v[k++] = (Cty)q; \
		q = 1.5L; \
		v[k++] = (Cty)q; \
		q = -1.5L; \
		v[k++] = (Cty)q; \
		q = 2.25L; \
		v[k++] = (Cty)q; \
		q = -2.25L; \
		v[k++] = (Cty)q; \
		q = 5.5L; \
		v[k++] = (Cty)q; \
		q = 100.0L; \
		v[k++] = (Cty)q; \
		q = 255.0L; \
		v[k++] = (Cty)q; \
		q = 256.0L; \
		v[k++] = (Cty)q; \
		q = 1024.0L; \
		v[k++] = (Cty)q; \
		q = -1024.0L; \
		v[k++] = (Cty)q; \
		q = 32768.0L; \
		v[k++] = (Cty)q; \
		q = 65504.0L; \
		v[k++] = (Cty)q; \
		q = 65536.0L; \
		v[k++] = (Cty)q; \
		q = 1e-5L; \
		v[k++] = (Cty)q; \
		t = on + tw; \
		u = on / t; \
		v[k++] = u; \
		v[k++] = -u; \
		return k; \
	}

SMF_BFILL(F16)
SMF_BFILL(F32)
SMF_BFILL(F64)
SMF_BFILL(F80)

static int smf_bcorpus_ok;

static void smf_binit(void)
{
	int n16 = smf_bfill_F16();
	int n32 = smf_bfill_F32();
	int n64 = smf_bfill_F64();
	int n80 = smf_bfill_F80();
	smf_bcorpus_ok = n16 == SMF_B_N && n32 == SMF_B_N && n64 == SMF_B_N &&
									 n80 == SMF_B_N;
}

#define SMF_BARM(TY, OP) \
	case SMF_KEY(SMF_T_##TY, SMF_O_##OP): { \
		volatile SMF_CTY_##TY va = smf_b_##TY[i]; \
		volatile SMF_CTY_##TY vb = smf_b_##TY[j]; \
		volatile SMF_CTY_##TY vc = smf_b_##TY[k]; \
		SMF_CTY_##TY r = (SMF_CTY_##TY)(SMF_EXPR_##OP(SMF_CTY_##TY, va, vb, vc)); \
		n = (int)sizeof r > 10 ? 10 : (int)sizeof r; \
		memcpy(buf, &r, (size_t)n); \
		break; \
	}

#define SMF_BARMS_TY(tag, cty, w) SMF_OPS(SMF_BARM, tag)

static void smf_brun(int tag, int op, int i, int j, int k, SmBits *lo,
										 SmBits *hi)
{
	unsigned char buf[16];
	int n = 0;
	memset(buf, 0, sizeof buf);
	switch (SMF_KEY(tag, op)) {
		SMF_FTYPES(SMF_BARMS_TY)
	}
	(void)n;
	memcpy(lo, buf, 8);
	memcpy(hi, buf + 8, 8);
}

static SmBits smf_bhash(int tag, int op)
{
	SmBits h = 14695981039346656037ull, lo, hi;
	int i, j;
	for (i = 0; i < SMF_B_N; i++)
		for (j = 0; j < SMF_B_N; j++) {
			smf_brun(tag, op, i, j, (i + j + 1) % SMF_B_N, &lo, &hi);
			h = ((h ^ lo) * 1099511628211ull ^ hi) * 1099511628211ull;
		}
	return h;
}

static long smf_bsweep(SmBits *digest)
{
	int t, op;
	long n = 0;
	for (t = 0; t < SMF_T_COUNT; t++)
		for (op = 0; op < SMF_O_COUNT; op++) {
			SmBits h = smf_bhash(t, op);
			*digest = (*digest ^ (SmBits)(SMF_KEY(t, op) + 0x51ed)) * 1099511628211ull;
			*digest = (*digest ^ h) * 1099511628211ull;
			n += (long)SMF_B_N * SMF_B_N;
		}
	return n;
}

#define SMX_ITYPES(X) \
	X(SI, int) \
	X(UI, unsigned int) \
	X(SLL, long long) \
	X(ULL, unsigned long long)

#define SMX_F2I_TYPES(X) \
	X(SI, int) \
	X(UI, unsigned int) \
	X(SLL, long long)

enum {
#define SMX_I_ROW(tag, cty) SMX_I_##tag,
	SMX_ITYPES(SMX_I_ROW)
#undef SMX_I_ROW
			SMX_I_COUNT
};

enum {
#define SMX_F2I_ROW(tag, cty) SMX_F2I_##tag,
	SMX_F2I_TYPES(SMX_F2I_ROW)
#undef SMX_F2I_ROW
			SMX_F2I_COUNT
};

#define SMX_CTY_SI int
#define SMX_CTY_UI unsigned int
#define SMX_CTY_SLL long long
#define SMX_CTY_ULL unsigned long long

static const SmBits smx_icorpus[] = {
		0ull, 1ull, 0xffffffffffffffffull, 2ull, 3ull, 7ull, 127ull, 128ull,
		255ull, 256ull, 32767ull, 32768ull, 65535ull, 65536ull,
		0x7fffffull, 0x800000ull, 0x800001ull, 0xffffffull,
		0x1000000ull, 0x1000001ull, 0x7ffffffeull, 0x7fffffffull,
		0x80000000ull, 0x80000001ull, 0xfffffffeull,
		0x1fffffffffffffull, 0x20000000000000ull, 0x20000000000001ull,
		0x7ffffffffffffffeull, 0x7fffffffffffffffull, 0x8000000000000000ull,
		0x8000000000000001ull};

#define SMX_ICORPUS_N ((int)(sizeof smx_icorpus / sizeof smx_icorpus[0]))

#define SMX_F2F_ARM(ST, SCTY, DT, DCTY) \
	case (SMF_T_##ST) * SMF_T_COUNT + SMF_T_##DT: { \
		volatile SCTY sv = smf_b_##ST[i]; \
		DCTY dv = (DCTY)sv; \
		n = (int)sizeof dv > 10 ? 10 : (int)sizeof dv; \
		memcpy(buf, &dv, (size_t)n); \
		break; \
	}

#define SMX_F2F_IN_F16(dt, dcty, dw) SMX_F2F_ARM(F16, _Float16, dt, dcty)
#define SMX_F2F_IN_F32(dt, dcty, dw) SMX_F2F_ARM(F32, float, dt, dcty)
#define SMX_F2F_IN_F64(dt, dcty, dw) SMX_F2F_ARM(F64, double, dt, dcty)
#define SMX_F2F_IN_F80(dt, dcty, dw) SMX_F2F_ARM(F80, long double, dt, dcty)
#define SMX_F2F_OUT(st, scty, sw) SMF_FTYPES2(SMX_F2F_IN_##st)

static void smx_f2f(int s, int d, int i, SmBits *lo, SmBits *hi)
{
	unsigned char buf[16];
	int n = 0;
	memset(buf, 0, sizeof buf);
	switch (s * SMF_T_COUNT + d) {
		SMF_FTYPES(SMX_F2F_OUT)
	}
	(void)n;
	memcpy(lo, buf, 8);
	memcpy(hi, buf + 8, 8);
}

#define SMX_F2I_ARM(ST, SCTY, DT, DCTY) \
	case (SMF_T_##ST) * SMX_F2I_COUNT + SMX_F2I_##DT: { \
		volatile SCTY sv = smf_b_##ST[i]; \
		return (SmBits)(DCTY)sv; \
	}

#define SMX_F2I_IN_F16(dt, dcty) SMX_F2I_ARM(F16, _Float16, dt, dcty)
#define SMX_F2I_IN_F32(dt, dcty) SMX_F2I_ARM(F32, float, dt, dcty)
#define SMX_F2I_IN_F64(dt, dcty) SMX_F2I_ARM(F64, double, dt, dcty)
#define SMX_F2I_IN_F80(dt, dcty) SMX_F2I_ARM(F80, long double, dt, dcty)
#define SMX_F2I_OUT(st, scty, sw) SMX_F2I_TYPES(SMX_F2I_IN_##st)

static SmBits smx_f2i(int s, int t, int i)
{
	switch (s * SMX_F2I_COUNT + t) {
		SMF_FTYPES(SMX_F2I_OUT)
	}
	return 0;
}

#define SMX_I2F_ARM(ST, SCTY, DT, DCTY) \
	case (SMX_I_##ST) * SMF_T_COUNT + SMF_T_##DT: { \
		volatile SCTY sv = (SCTY)smx_icorpus[i]; \
		DCTY dv = (DCTY)sv; \
		n = (int)sizeof dv > 10 ? 10 : (int)sizeof dv; \
		memcpy(buf, &dv, (size_t)n); \
		break; \
	}

#define SMX_I2F_IN_SI(dt, dcty, dw) SMX_I2F_ARM(SI, int, dt, dcty)
#define SMX_I2F_IN_UI(dt, dcty, dw) SMX_I2F_ARM(UI, unsigned int, dt, dcty)
#define SMX_I2F_IN_SLL(dt, dcty, dw) SMX_I2F_ARM(SLL, long long, dt, dcty)
#define SMX_I2F_IN_ULL(dt, dcty, dw) \
	SMX_I2F_ARM(ULL, unsigned long long, dt, dcty)
#define SMX_I2F_OUT(st, scty) SMF_FTYPES2(SMX_I2F_IN_##st)

static void smx_i2f(int t, int d, int i, SmBits *lo, SmBits *hi)
{
	unsigned char buf[16];
	int n = 0;
	memset(buf, 0, sizeof buf);
	switch (t * SMF_T_COUNT + d) {
		SMX_ITYPES(SMX_I2F_OUT)
	}
	(void)n;
	memcpy(lo, buf, 8);
	memcpy(hi, buf + 8, 8);
}

static SmBits smx_f2f_hash(int s, int d)
{
	SmBits h = 14695981039346656037ull, lo, hi;
	int i;
	for (i = 0; i < SMF_B_N; i++) {
		smx_f2f(s, d, i, &lo, &hi);
		h = ((h ^ lo) * 1099511628211ull ^ hi) * 1099511628211ull;
	}
	return h;
}

static SmBits smx_f2i_hash(int s, int t)
{
	SmBits h = 14695981039346656037ull;
	int i;
	for (i = 0; i < SMF_B_N; i++)
		h = (h ^ smx_f2i(s, t, i)) * 1099511628211ull;
	return h;
}

static SmBits smx_i2f_hash(int t, int d)
{
	SmBits h = 14695981039346656037ull, lo, hi;
	int i;
	for (i = 0; i < SMX_ICORPUS_N; i++) {
		smx_i2f(t, d, i, &lo, &hi);
		h = ((h ^ lo) * 1099511628211ull ^ hi) * 1099511628211ull;
	}
	return h;
}

static long smx_sweep(SmBits *digest)
{
	int s, d, t;
	long n = 0;
	for (s = 0; s < SMF_T_COUNT; s++)
		for (d = 0; d < SMF_T_COUNT; d++) {
			*digest = (*digest ^ smx_f2f_hash(s, d)) * 1099511628211ull;
			n += SMF_B_N;
		}
	for (s = 0; s < SMF_T_COUNT; s++)
		for (t = 0; t < SMX_F2I_COUNT; t++) {
			*digest = (*digest ^ smx_f2i_hash(s, t)) * 1099511628211ull;
			n += SMF_B_N;
		}
	for (t = 0; t < SMX_I_COUNT; t++)
		for (d = 0; d < SMF_T_COUNT; d++) {
			*digest = (*digest ^ smx_i2f_hash(t, d)) * 1099511628211ull;
			n += SMX_ICORPUS_N;
		}
	return n;
}

enum {
	SMN_ZEQ, SMN_ZRECNE, SMN_ZSIGN, SMN_NANSELF, SMN_NANCMP, SMN_SNANQUIET,
	SMN_INFMAX, SMN_INFOVF, SMN_NEGINF, SMN_DENHALF, SMN_DENORDER, SMN_DENGAP,
	SMN_DENADD, SMN_EPSONE, SMN_NEXTHI, SMN_INTOVF, SMN_INTOK, SMN_TERNMIX,
	SMN_COUNT
};

static const char *const smn_kind_name[] = {
		"zeq",     "zrecne", "zsign",  "nanself", "nancmp", "snanquiet",
		"infmax",  "infovf", "neginf", "denhalf", "denorder", "dengap",
		"denadd",  "epsone", "nexthi", "intovf",  "intok",  "ternmix"};

#define SMN_BODY(TY) \
	static int smn_run_##TY(int kind) \
	{ \
		typedef SMF_CTY_##TY Cty; \
		volatile Cty z = smf_b_##TY[0]; \
		volatile Cty nz = smf_b_##TY[1]; \
		volatile Cty tm = smf_b_##TY[2]; \
		volatile Cty dmax = smf_b_##TY[7]; \
		volatile Cty mn = smf_b_##TY[10]; \
		volatile Cty mx = smf_b_##TY[15]; \
		volatile Cty nearmx = smf_b_##TY[17]; \
		volatile Cty inf = smf_b_##TY[19]; \
		volatile Cty ninf = smf_b_##TY[20]; \
		volatile Cty qn = smf_b_##TY[21]; \
		volatile Cty sn = smf_b_##TY[24]; \
		volatile Cty on = smf_b_##TY[25]; \
		volatile Cty one_up = smf_b_##TY[27]; \
		volatile Cty one_dn = smf_b_##TY[28]; \
		volatile Cty tw = smf_b_##TY[29]; \
		volatile Cty pow2 = smf_b_##TY[36]; \
		volatile Cty pow2n = smf_b_##TY[38]; \
		volatile Cty pow2n1 = smf_b_##TY[39]; \
		volatile Cty pow2p1 = smf_b_##TY[41]; \
		volatile Cty ep = smf_b_##TY[42]; \
		volatile Cty a, b; \
		switch (kind) { \
		case SMN_ZEQ: return z == nz; \
		case SMN_ZRECNE: a = on / z; b = on / nz; return a != b; \
		case SMN_ZSIGN: a = on / nz; return a < z; \
		case SMN_NANSELF: return qn != qn; \
		case SMN_NANCMP: return !((qn < on) || (qn > on) || (qn == on)); \
		case SMN_SNANQUIET: a = sn + z; return a != a; \
		case SMN_INFMAX: return inf > mx; \
		case SMN_INFOVF: a = mx * tw; return a == inf; \
		case SMN_NEGINF: a = ninf; return a < -mx; \
		case SMN_DENHALF: a = tm / tw; return a == z; \
		case SMN_DENORDER: return z < tm && tm < mn; \
		case SMN_DENGAP: return dmax < mn && dmax > z; \
		case SMN_DENADD: a = tm + tm; b = tm * tw; return a == b; \
		case SMN_EPSONE: a = on + ep; return a != on && one_up == a && one_dn < on; \
		case SMN_NEXTHI: return nearmx < mx && nearmx > z; \
		case SMN_INTOVF: a = pow2n + on; return a == pow2n && pow2n1 == a; \
		case SMN_INTOK: a = pow2 + on; return a != pow2 && pow2p1 == a; \
		case SMN_TERNMIX: \
			a = (Cty)(mx > mn ? mx - mn : 0); \
			b = (Cty)(mx > mn ? mx - mn : 0.0); \
			return a == b && a == (Cty)(mx > mn ? mx - mn : (Cty)0); \
		} \
		return -1; \
	}

SMN_BODY(F16)
SMN_BODY(F32)
SMN_BODY(F64)
SMN_BODY(F80)

static int smn_run(int tag, int kind)
{
	switch (tag) {
	case SMF_T_F16: return smn_run_F16(kind);
	case SMF_T_F32: return smn_run_F32(kind);
	case SMF_T_F64: return smn_run_F64(kind);
	default: return smn_run_F80(kind);
	}
}

typedef struct
{
	const char *name;
	int sel;
	SmBits want;
} SmXRow;

static SmBits smx_named(int which)
{
	volatile double big = 1e300;
	volatile double dmin = -2147483648.0;
	volatile double dllmin = -9223372036854775808.0;
	volatile double dfrac = -1.5;
	volatile float ffrac = 2.75f;
	volatile long double lmin = -9223372036854775808.0L;
	volatile int imin = -2147483647 - 1;
	volatile long long llmin = -9223372036854775807LL - 1;
	volatile unsigned long long ullmax = 18446744073709551615ull;
	switch (which) {
	case 0: return (SmBits)(long long)(int)dmin;
	case 1: return (SmBits)(long long)(long long)dllmin;
	case 2: return (SmBits)(long long)(int)dfrac;
	case 3: return (SmBits)(unsigned int)ffrac;
	case 4: return (SmBits)(long long)(long long)lmin;
	case 5: return (SmBits)(long long)(int)(double)imin;
	case 6: return (SmBits)(long long)(long long)(double)llmin;
	case 7: return (SmBits)(long long)(long long)(long double)llmin;
	case 8: return (SmBits)(long long)(long double)(long long)ullmax;
	case 9: return (SmBits)(long long)(long double)big;
	case 10: return (SmBits)(unsigned int)big;
	case 11: return (SmBits)(long long)(int)big;
	case 12: {
		volatile _Float16 h = (_Float16)big;
		unsigned short b;
		memcpy(&b, &h, 2);
		return (SmBits)b;
	}
	case 13: {
		volatile double d = 65520.0;
		volatile _Float16 h = (_Float16)d;
		unsigned short b;
		memcpy(&b, &h, 2);
		return (SmBits)b;
	}
	case 14: {
		volatile double d = 65519.0;
		volatile _Float16 h = (_Float16)d;
		unsigned short b;
		memcpy(&b, &h, 2);
		return (SmBits)b;
	}
	case 15: {
		volatile double d = 2.9802322387695312e-08;
		volatile _Float16 h = (_Float16)d;
		unsigned short b;
		memcpy(&b, &h, 2);
		return (SmBits)b;
	}
	case 16: {
		volatile double d = 4.4703484e-08;
		volatile _Float16 h = (_Float16)d;
		unsigned short b;
		memcpy(&b, &h, 2);
		return (SmBits)b;
	}
	case 17: {
		volatile float f = 16777217.0f;
		return (SmBits)(long long)(int)f;
	}
	case 18: {
		unsigned int u = (unsigned int)big;
		return (SmBits)u;
	}
	case 19: {
		volatile long long w = 0x123456789abcdefLL;
		return (SmBits)(unsigned int)w;
	}
	case 20: {
		volatile double d = 4294967296.0;
		unsigned int u = (unsigned int)d;
		return (SmBits)u;
	}
	}
	return 0;
}

static const SmXRow smx_rows[] = {
		{"x.int.from.intmin", 0, 0xffffffff80000000ull},
		{"x.llong.from.llmin", 1, 0x8000000000000000ull},
		{"x.int.from.negfrac", 2, 0xffffffffffffffffull},
		{"x.uint.from.f32", 3, 2ull},
		{"x.llong.from.ldbl.llmin", 4, 0x8000000000000000ull},
		{"x.int.roundtrip.intmin", 5, 0xffffffff80000000ull},
		{"x.llong.roundtrip.dbl", 6, 0x8000000000000000ull},
		{"x.llong.roundtrip.ldbl", 7, 0x8000000000000000ull},
		{"x.sll.roundtrip.ldbl.m1", 8, 0xffffffffffffffffull},
#if SMF_FTOI_INDEFINITE
		{"x.sll.from.ldbl.1e300", 9, 0x8000000000000000ull},
		{"x.uint.from.1e300", 10, 0x8000000000000000ull},
		{"x.uint.var.1e300", 18, 0ull},
		{"x.uint.from.sll", 19, 0x89abcdefull},
		{"x.uint.var.2p32", 20, 0ull},
		{"x.int.from.1e300", 11, 0xffffffff80000000ull},
#else
		{"x.sll.from.ldbl.1e300", 9, 0x7fffffffffffffffull},
		{"x.uint.from.1e300", 10, 0x00000000ffffffffull},
		{"x.uint.var.1e300", 18, 0x00000000ffffffffull},
		{"x.uint.from.sll", 19, 0x89abcdefull},
		{"x.uint.var.2p32", 20, 0x00000000ffffffffull},
		{"x.int.from.1e300", 11, 0x000000007fffffffull},
#endif
		{"x.f16.from.1e300", 12, 0x7c00ull},
		{"x.f16.from.65520", 13, 0x7c00ull},
		{"x.f16.from.65519", 14, 0x7bffull},
		{"x.f16.from.halfden", 15, 0ull},
		{"x.f16.from.den075", 16, 0x0001ull},
		{"x.int.from.f32.2p24p1", 17, 16777216ull}};

#define SMX_ROWS_N ((int)(sizeof smx_rows / sizeof smx_rows[0]))

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

static const char *const smx_int_name[] = {
#define SMX_IN(tag, cty) #tag,
		SMX_ITYPES(SMX_IN)
#undef SMX_IN
};

static long smf_sweep(SmBits *digest)
{
	int t, op, i, j;
	long n = 0;
	smf_binit();
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
	n += smf_bsweep(digest);
	n += smx_sweep(digest);
	return n;
}

#define SMC_MKFN(TAG, CTY) \
	static CTY _Complex smc_mk_##TAG(CTY re, CTY im) \
	{ \
		CTY p[2]; \
		CTY _Complex z; \
		p[0] = re; \
		p[1] = im; \
		memcpy(&z, p, sizeof z); \
		return z; \
	}

SMC_MKFN(C32, float)
SMC_MKFN(C64, double)
SMC_MKFN(C80, long double)
#if SMC_HAVE_C16
SMC_MKFN(C16, _Float16)
#endif

#define SMC_ARM_OPS(TAG, CTY) \
	switch (op) { \
	case SMC_O_CADD: r = x + y; break; \
	case SMC_O_CSUB: r = x - y; break; \
	case SMC_O_CMUL: r = x * y; break; \
	case SMC_O_CNEG: r = -x; break; \
	case SMC_O_CCONJ: r = smc_mk_##TAG(vr, -vi); break; \
	case SMC_O_CSCALE: r = x * wr; break; \
	case SMC_O_CSELR: r = (vr < wr) ? x : y; break; \
	case SMC_O_CDIV: r = x / y; break; \
	case SMC_O_CEQ: r = (CTY _Complex)(CTY)(x == y); break; \
	case SMC_O_CNE: r = (CTY _Complex)(CTY)(x != y); break; \
	case SMC_O_CMULADD: r = x * y + x; break; \
	case SMC_O_CDIVSEL: \
		r = (wr != (CTY)0 || wi != (CTY)0) ? x / y : x; \
		break; \
	case SMC_O_CSELMIX: r = (vr > wr) ? x - y : 0; break; \
	case SMC_O_CABS2: r = x * smc_mk_##TAG(vr, -vi); break; \
	case SMC_O_CIMUL: \
		r = smc_mk_##TAG(-(CTY)(__imag__ x), (CTY)(__real__ x)); \
		break; \
	default: r = (vr != (CTY)0) ? y : x; break; \
	}

#define SMC_BODY(TAG, CTY) \
	{ \
		volatile CTY vr = (CTY)(v[0]), vi = (CTY)(v[1]); \
		volatile CTY wr = (CTY)(v[2]), wi = (CTY)(v[3]); \
		CTY _Complex x = smc_mk_##TAG(vr, vi); \
		CTY _Complex y = smc_mk_##TAG(wr, wi); \
		CTY _Complex r; \
		SMC_ARM_OPS(TAG, CTY) \
		*rr = (long double)__real__ r; \
		*ri = (long double)__imag__ r; \
	}

static void smc_run(int tag, int op, const double *v, long double *rr,
										long double *ri)
{
	*rr = 0;
	*ri = 0;
	if (tag == SMC_T_C32) {
		SMC_BODY(C32, float)
	}
	if (tag == SMC_T_C64) {
		SMC_BODY(C64, double)
	}
	if (tag == SMC_T_C80) {
		SMC_BODY(C80, long double)
	}
#if SMC_HAVE_C16
	if (tag == SMC_T_C16) {
		SMC_BODY(C16, _Float16)
	}
#endif
}

#if SMC_HAVE_C16
static const int smc_ftag[] = {SMF_T_F32, SMF_T_F64, SMF_T_F80, SMF_T_F16};
#else
static const int smc_ftag[] = {SMF_T_F32, SMF_T_F64, SMF_T_F80};
#endif

#define SMC_CORPUS_N SMF_B_N

static double smc_at(int i)
{
	return smf_b_F64[i];
}

static SmBits smc_hash(int t, int op)
{
	SmBits h = 14695981039346656037ull, a1, a2, b1, b2;
	int i, j;
	for (i = 0; i < SMC_CORPUS_N; i++)
		for (j = 0; j < SMC_CORPUS_N; j++) {
			long double gr = 0, gi = 0;
			double cv[4];
			cv[0] = smc_at(i);
			cv[1] = smc_at(j);
			cv[2] = smc_at(j);
			cv[3] = smc_at((i + j + 1) % SMC_CORPUS_N);
			smc_run(t, op, cv, &gr, &gi);
			smf_enc(smc_ftag[t], gr, &a1, &a2);
			smf_enc(smc_ftag[t], gi, &b1, &b2);
			h = ((h ^ a1) * 1099511628211ull ^ a2) * 1099511628211ull;
			h = ((h ^ b1) * 1099511628211ull ^ b2) * 1099511628211ull;
		}
	return h;
}

static long smc_sweep(SmBits *digest)
{
	int t, op;
	long n = 0;
	for (t = 0; t < SMC_T_COUNT; t++)
		for (op = 0; op < SMC_O_COUNT; op++) {
			SmBits h = smc_hash(t, op);
			*digest = (*digest ^ (SmBits)(t * SMC_O_COUNT + op)) * 1099511628211ull;
			*digest = (*digest ^ h) * 1099511628211ull;
			n += (long)SMC_CORPUS_N * SMC_CORPUS_N;
		}
	return n;
}

#if SMC_HAVE_C16
static const char *const smc_type_name[] = {"C32", "C64", "C80", "C16"};
#else
static const char *const smc_type_name[] = {"C32", "C64", "C80"};
#endif

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
#define SMN_ROWS_N (SMF_T_COUNT * SMN_COUNT)

static int smf_rows_count(void)
{
	return SMF_ROWS_N + SMC_ROWS_N + SMW_ROWS_N + SMN_ROWS_N + SMX_ROWS_N;
}

static void smn_name(char *out, size_t n, int tag, int kind)
{
	snprintf(out, n, "n.%s.%s", smf_type_name[tag], smn_kind_name[kind]);
}

static long smf_rows_run(long *checks, long *failures, long *reported,
												 int poison)
{
	long n = 0;
	int i;
	smf_binit();
	(*checks)++;
	n++;
	if (!smf_bcorpus_ok) {
		(*failures)++;
		if ((*reported)++ < 40)
			printf("FAIL bcorpus fcorpus.size got=short want=%d\n", SMF_B_N);
	}
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
		long double gr = 0, gi = 0;
		SmBits a1, a2, b1, b2, c1, c2, d1, d2;
		double cv[4];
		volatile double t0 = r->ar, t1 = r->ai, t2 = r->br, t3 = r->bi;
		cv[0] = t0;
		cv[1] = t1;
		cv[2] = t2;
		cv[3] = t3;
		smc_run(r->tag, r->op, cv, &gr, &gi);
		smf_enc(smc_ftag[r->tag], gr, &a1, &a2);
		smf_enc(smc_ftag[r->tag], gi, &b1, &b2);
		smf_enc(smc_ftag[r->tag], r->wantr, &c1, &c2);
		smf_enc(smc_ftag[r->tag], r->wanti, &d1, &d2);
		(*checks)++;
		n++;
		if (a1 != c1 || a2 != c2 || b1 != d1 || b2 != d2) {
			(*failures)++;
			if ((*reported)++ < 40)
				printf("FAIL crun %s got=%016llx:%016llx want=%016llx:%016llx\n",
							 r->name, (unsigned long long)a1, (unsigned long long)b1,
							 (unsigned long long)c1, (unsigned long long)d1);
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
	{
		int t, kind;
		for (t = 0; t < SMF_T_COUNT; t++)
			for (kind = 0; kind < SMN_COUNT; kind++) {
				char nm[64];
				int got = smn_run(t, kind);
				smn_name(nm, sizeof nm, t, kind);
				(*checks)++;
				n++;
				if (got != 1) {
					(*failures)++;
					if ((*reported)++ < 40)
						printf("FAIL nrun %s got=%d want=1\n", nm, got);
				}
			}
	}
	for (i = 0; i < SMX_ROWS_N; i++) {
		SmBits got = smx_named(smx_rows[i].sel);
		(*checks)++;
		n++;
		if (got != smx_rows[i].want) {
			(*failures)++;
			if ((*reported)++ < 40)
				printf("FAIL xrun %s got=%016llx want=%016llx\n", smx_rows[i].name,
							 (unsigned long long)got,
							 (unsigned long long)smx_rows[i].want);
		}
	}
	return n;
}

static int smf_point_dump(const char *cat)
{
	int t, op, i, j, n = 0;
	char name[64];
	smf_binit();
	for (t = 0; t < SMF_T_COUNT; t++)
		for (op = 0; op < SMF_O_COUNT; op++) {
			SmBits lo, hi;
			sprintf(name, "fsweep.%s.%s", smf_type_name[t], smf_op_name[op]);
			if (strcmp(name, cat))
				continue;
			for (i = 0; i < SMF_CORPUS_N; i++)
				for (j = 0; j < SMF_CORPUS_N; j++) {
					smf_enc(t, smf_run(t, op, smf_corpus[i], smf_corpus[j],
														 smf_corpus[(i + j + 1) % SMF_CORPUS_N]),
									&lo, &hi);
					printf("P %s %d %d %016llx%016llx\n", name, i, j,
								 (unsigned long long)hi, (unsigned long long)lo);
					n++;
				}
		}
	for (t = 0; t < SMF_T_COUNT; t++)
		for (op = 0; op < SMF_O_COUNT; op++) {
			SmBits lo, hi;
			sprintf(name, "bsweep.%s.%s", smf_type_name[t], smf_op_name[op]);
			if (strcmp(name, cat))
				continue;
			for (i = 0; i < SMF_B_N; i++)
				for (j = 0; j < SMF_B_N; j++) {
					smf_brun(t, op, i, j, (i + j + 1) % SMF_B_N, &lo, &hi);
					printf("P %s %d %d %016llx%016llx\n", name, i, j,
								 (unsigned long long)hi, (unsigned long long)lo);
					n++;
				}
		}
	for (t = 0; t < SMC_T_COUNT; t++)
		for (op = 0; op < SMC_O_COUNT; op++) {
			SmBits a1, a2, b1, b2;
			sprintf(name, "csweep.%s.%s", smc_type_name[t], smc_op_name[op]);
			if (strcmp(name, cat))
				continue;
			for (i = 0; i < SMC_CORPUS_N; i++)
				for (j = 0; j < SMC_CORPUS_N; j++) {
					long double gr = 0, gi = 0;
					double cv[4];
					cv[0] = smc_at(i);
					cv[1] = smc_at(j);
					cv[2] = smc_at(j);
					cv[3] = smc_at((i + j + 1) % SMC_CORPUS_N);
					smc_run(t, op, cv, &gr, &gi);
					smf_enc(smc_ftag[t], gr, &a1, &a2);
					smf_enc(smc_ftag[t], gi, &b1, &b2);
					printf("P %s %d %d %016llx%016llx%016llx%016llx\n", name, i, j,
								 (unsigned long long)a2, (unsigned long long)a1,
								 (unsigned long long)b2, (unsigned long long)b1);
					n++;
				}
		}
	return n;
}

static void smf_row_dump(void)
{
	int i;
	smf_binit();
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
		long double gr = 0, gi = 0;
		SmBits a1, a2, b1, b2;
		double cv[4];
		volatile double t0 = r->ar, t1 = r->ai, t2 = r->br, t3 = r->bi;
		cv[0] = t0;
		cv[1] = t1;
		cv[2] = t2;
		cv[3] = t3;
		smc_run(r->tag, r->op, cv, &gr, &gi);
		smf_enc(smc_ftag[r->tag], gr, &a1, &a2);
		smf_enc(smc_ftag[r->tag], gi, &b1, &b2);
		printf("C %s 0 %016llx%016llx %016llx%016llx %016llx%016llx\n", r->name,
					 (unsigned long long)a2, (unsigned long long)a1,
					 (unsigned long long)b2, (unsigned long long)b1,
					 (unsigned long long)a2, (unsigned long long)a1);
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
		int t, kind;
		for (t = 0; t < SMF_T_COUNT; t++)
			for (kind = 0; kind < SMN_COUNT; kind++) {
				char nm[64];
				SmBits got = (SmBits)(long long)smn_run(t, kind);
				smn_name(nm, sizeof nm, t, kind);
				printf("N %s 0 %016llx %016llx %016llx\n", nm,
							 (unsigned long long)got, (unsigned long long)got, 1ull);
			}
	}
	for (i = 0; i < SMX_ROWS_N; i++) {
		SmBits got = smx_named(smx_rows[i].sel);
		printf("X %s 0 %016llx %016llx %016llx\n", smx_rows[i].name,
					 (unsigned long long)got, (unsigned long long)got,
					 (unsigned long long)smx_rows[i].want);
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
		int t, op;
		for (t = 0; t < SMF_T_COUNT; t++)
			for (op = 0; op < SMF_O_COUNT; op++) {
				SmBits h = smf_bhash(t, op);
				printf("V bsweep.%s.%s 0 %016llx %016llx %016llx\n", smf_type_name[t],
							 smf_op_name[op], (unsigned long long)h, (unsigned long long)h,
							 (unsigned long long)h);
			}
	}
	{
		int s, d, t;
		for (s = 0; s < SMF_T_COUNT; s++)
			for (d = 0; d < SMF_T_COUNT; d++) {
				SmBits h = smx_f2f_hash(s, d);
				printf("V xsweep.%s.%s 0 %016llx %016llx %016llx\n", smf_type_name[s],
							 smf_type_name[d], (unsigned long long)h, (unsigned long long)h,
							 (unsigned long long)h);
			}
		for (s = 0; s < SMF_T_COUNT; s++)
			for (t = 0; t < SMX_F2I_COUNT; t++) {
				SmBits h = smx_f2i_hash(s, t);
				printf("V xsweep.%s.%s 0 %016llx %016llx %016llx\n", smf_type_name[s],
							 smx_int_name[t], (unsigned long long)h, (unsigned long long)h,
							 (unsigned long long)h);
			}
		for (t = 0; t < SMX_I_COUNT; t++)
			for (d = 0; d < SMF_T_COUNT; d++) {
				SmBits h = smx_i2f_hash(t, d);
				printf("V xsweep.%s.%s 0 %016llx %016llx %016llx\n", smx_int_name[t],
							 smf_type_name[d], (unsigned long long)h, (unsigned long long)h,
							 (unsigned long long)h);
			}
	}
	{
		int t, op;
		for (t = 0; t < SMC_T_COUNT; t++)
			for (op = 0; op < SMC_O_COUNT; op++) {
				SmBits h = smc_hash(t, op);
				printf("V csweep.%s.%s 0 %016llx %016llx %016llx\n", smc_type_name[t],
							 smc_op_name[op], (unsigned long long)h, (unsigned long long)h,
							 (unsigned long long)h);
			}
	}
}

static void smf_digest_dump(void)
{
	int t, op, i, j;
	smf_binit();
	printf("D ext rows %d %d %d %d %d\n", SMF_ROWS_N, SMC_ROWS_N, SMW_ROWS_N,
				 SMN_ROWS_N, SMX_ROWS_N);
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
	for (t = 0; t < SMF_T_COUNT; t++)
		for (op = 0; op < SMF_O_COUNT; op++)
			printf("D bnd %s %s %016llx\n", smf_type_name[t], smf_op_name[op],
						 (unsigned long long)smf_bhash(t, op));
	for (t = 0; t < SMC_T_COUNT; t++)
		for (op = 0; op < SMC_O_COUNT; op++)
			printf("D cpx %s %s %016llx\n", smc_type_name[t], smc_op_name[op],
						 (unsigned long long)smc_hash(t, op));
}

#endif
