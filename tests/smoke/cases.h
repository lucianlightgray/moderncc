#ifndef MCC_SMOKE_CASES_H
#define MCC_SMOKE_CASES_H

#include "smoke.h"

static const SmRow sm_rows[] = {

		SM_ROW("neg.si.min", SI, NEG, INT_MIN, 0, 0, SM_ENC_SI(INT_MIN))
		SM_ROW("neg.si.max", SI, NEG, INT_MAX, 0, 0, SM_ENC_SI(-INT_MAX))
		SM_ROW("neg.si.minp1", SI, NEG, INT_MIN + 1, 0, 0, SM_ENC_SI(INT_MAX))
		SM_ROW("neg.sll.min", SLL, NEG, LLONG_MIN, 0, 0, SM_ENC_SLL(LLONG_MIN))
		SM_ROW("neg.sll.max", SLL, NEG, LLONG_MAX, 0, 0, SM_ENC_SLL(-LLONG_MAX))
		SM_ROW("neg.sl.min", SL, NEG, LONG_MIN, 0, 0, SM_ENC_SL(LONG_MIN))
		SM_ROW("neg.ss.min", SS, NEG, SHRT_MIN, 0, 0, SM_ENC_SS(SHRT_MIN))
		SM_ROW("neg.sc.min", SC, NEG, SCHAR_MIN, 0, 0, SM_ENC_SC(SCHAR_MIN))
		SM_ROW("neg.ui.high", UI, NEG, 0x80000000u, 0, 0, SM_ENC_UI(0x80000000u))
		SM_ROW("neg.ui.one", UI, NEG, 1u, 0, 0, SM_ENC_UI(UINT_MAX))
		SM_ROW("neg.ull.one", ULL, NEG, 1ull, 0, 0, SM_ENC_ULL(ULLONG_MAX))
		SM_ROW("neg.bool.one", BOOL, NEG, 1, 0, 0, SM_ENC_BOOL(1))

		SM_ROW("abs.si.min", SI, ABS, INT_MIN, 0, 0, SM_ENC_SI(INT_MIN))
		SM_ROW("abs.si.minp1", SI, ABS, INT_MIN + 1, 0, 0, SM_ENC_SI(INT_MAX))
		SM_ROW("abs.sll.min", SLL, ABS, LLONG_MIN, 0, 0, SM_ENC_SLL(LLONG_MIN))
		SM_ROW("abs.ss.min", SS, ABS, SHRT_MIN, 0, 0, SM_ENC_SS(SHRT_MIN))
		SM_ROW_NF("absc.si.min", SI, ABSC, INT_MIN, 0, 0, SM_ENC_SI(INT_MIN))
		SM_ROW_NF("absc.si.neg", SI, ABSC, -7, 0, 0, SM_ENC_SI(7))
		SM_ROW_NF("llabsc.sll.min", SLL, LLABSC, LLONG_MIN, 0, 0, SM_ENC_SLL(LLONG_MIN))
		SM_ROW_NF("llabsc.sll.neg", SLL, LLABSC, -7ll, 0, 0, SM_ENC_SLL(7))
		SM_ROW_NF("labsc.sl.min", SL, LABSC, LONG_MIN, 0, 0, SM_ENC_SL(LONG_MIN))

		SM_ROW("div.si.minm1p", SI, DIV, INT_MIN + 1, -1, 0, SM_ENC_SI(INT_MAX))
		SM_ROW("div.si.maxm1", SI, DIV, INT_MAX, -1, 0, SM_ENC_SI(-INT_MAX))
		SM_ROW("rem.si.minm1p", SI, REM, INT_MIN + 1, -1, 0, SM_ENC_SI(0))
		SM_ROW("div.sll.minm1p", SLL, DIV, LLONG_MIN + 1, -1ll, 0, SM_ENC_SLL(LLONG_MAX))
		SM_ROW("div.si.hi_as_signed", SI, DIV, (int)0x80000000, 2, 0, SM_ENC_SI(-1073741824))
		SM_ROW("div.ui.hi_as_unsigned", UI, DIV, 0x80000000u, 2u, 0, SM_ENC_UI(1073741824u))
		SM_ROW("rem.si.hi_as_signed", SI, REM, (int)0x80000000, 3, 0, SM_ENC_SI(-2))
		SM_ROW("rem.ui.hi_as_unsigned", UI, REM, 0x80000000u, 3u, 0, SM_ENC_UI(2u))
		SM_ROW("div.si.negtrunc", SI, DIV, -7, 2, 0, SM_ENC_SI(-3))
		SM_ROW("rem.si.negsign", SI, REM, -7, 2, 0, SM_ENC_SI(-1))
		SM_ROW("rem.si.negsign2", SI, REM, 7, -2, 0, SM_ENC_SI(1))
		SM_ROW("div.ull.hi", ULL, DIV, 0x8000000000000000ull, 2ull, 0, SM_ENC_ULL(0x4000000000000000ull))
		SM_ROW("div.sll.hi", SLL, DIV, LLONG_MIN, 2ll, 0, SM_ENC_SLL(-4611686018427387904ll))

		SM_ROW("shl.si.0", SI, SHL, 1, 0, 0, SM_ENC_SI(1))
		SM_ROW("shl.si.wm1", SI, SHL, 1, 31, 0, SM_ENC_SI(INT_MIN))
		SM_ROW("shl.si.w", SI, SHL, 1, 32, 0, SM_ENC_SI(1))
		SM_ROW("shl.si.wp1", SI, SHL, 1, 33, 0, SM_ENC_SI(2))
		SM_ROW("shr.si.0", SI, SHR, -1, 0, 0, SM_ENC_SI(-1))
		SM_ROW("shr.si.wm1", SI, SHR, -1, 31, 0, SM_ENC_SI(-1))
		SM_ROW("shr.si.w", SI, SHR, -1, 32, 0, SM_ENC_SI(-1))
		SM_ROW("shr.si.wp1", SI, SHR, -1, 33, 0, SM_ENC_SI(-1))
		SM_ROW("shr.si.min.1", SI, SHR, INT_MIN, 1, 0, SM_ENC_SI(-1073741824))
		SM_ROW("shr.si.min.31", SI, SHR, INT_MIN, 31, 0, SM_ENC_SI(-1))
		SM_ROW("shr.si.min.30", SI, SHR, INT_MIN, 30, 0, SM_ENC_SI(-2))
		SM_ROW("shr.ui.hi.31", UI, SHR, 0x80000000u, 31, 0, SM_ENC_UI(1u))
		SM_ROW("shl.sll.0", SLL, SHL, 1ll, 0, 0, SM_ENC_SLL(1))
		SM_ROW("shl.sll.wm1", SLL, SHL, 1ll, 63, 0, SM_ENC_SLL(LLONG_MIN))
		SM_ROW("shl.sll.w", SLL, SHL, 1ll, 64, 0, SM_ENC_SLL(1))
		SM_ROW("shl.sll.wp1", SLL, SHL, 1ll, 65, 0, SM_ENC_SLL(2))
		SM_ROW("shr.sll.min.63", SLL, SHR, LLONG_MIN, 63, 0, SM_ENC_SLL(-1))
		SM_ROW("shr.ull.hi.63", ULL, SHR, 0x8000000000000000ull, 63, 0, SM_ENC_ULL(1ull))
		SM_ROW("shl.ss.15", SS, SHL, 1, 15, 0, SM_ENC_SS(SHRT_MIN))
		SM_ROW("shl.ss.16", SS, SHL, 1, 16, 0, SM_ENC_SS(0))
		SM_ROW("shr.ss.min.15", SS, SHR, SHRT_MIN, 15, 0, SM_ENC_SS(-1))
		SM_ROW("shl.sc.7", SC, SHL, 1, 7, 0, SM_ENC_SC(SCHAR_MIN))
		SM_ROW("shr.sc.min.7", SC, SHR, SCHAR_MIN, 7, 0, SM_ENC_SC(-1))
		SM_ROW("shr.uc.hi.7", UC, SHR, 0x80u, 7, 0, SM_ENC_UC(1))
		SM_ROW("shl.bool.1", BOOL, SHL, 1, 1, 0, SM_ENC_BOOL(1))

		SM_ROW_NF("shl.si.neg1", SI, SHL, 1, -1, 0, SM_ENC_SI(INT_MIN))
		SM_ROW_NF("shr.si.neg1", SI, SHR, -1, -1, 0, SM_ENC_SI(-1))
		SM_ROW_NF("shl.sll.neg1", SLL, SHL, 1ll, -1, 0, SM_ENC_SLL(LLONG_MIN))
		SM_ROW_NF("shr.sll.neg1", SLL, SHR, LLONG_MIN, -1, 0, SM_ENC_SLL(-1))
		SM_ROW_NF("shl.si.neg32", SI, SHL, 1, -32, 0, SM_ENC_SI(1))

		SM_ROW("add.si.maxp1", SI, ADD, INT_MAX, 1, 0, SM_ENC_SI(INT_MIN))
		SM_ROW("add.si.minm1", SI, ADD, INT_MIN, -1, 0, SM_ENC_SI(INT_MAX))
		SM_ROW("sub.si.min1", SI, SUB, INT_MIN, 1, 0, SM_ENC_SI(INT_MAX))
		SM_ROW("mul.si.min.m1", SI, MUL, INT_MIN, -1, 0, SM_ENC_SI(INT_MIN))
		SM_ROW("mul.si.max.2", SI, MUL, INT_MAX, 2, 0, SM_ENC_SI(-2))
		SM_ROW("add.sll.maxp1", SLL, ADD, LLONG_MAX, 1ll, 0, SM_ENC_SLL(LLONG_MIN))
		SM_ROW("mul.sll.min.m1", SLL, MUL, LLONG_MIN, -1ll, 0, SM_ENC_SLL(LLONG_MIN))
		SM_ROW("add.ss.maxp1", SS, ADD, SHRT_MAX, 1, 0, SM_ENC_SS(SHRT_MIN))
		SM_ROW("add.sc.maxp1", SC, ADD, SCHAR_MAX, 1, 0, SM_ENC_SC(SCHAR_MIN))
		SM_ROW("add.ui.maxp1", UI, ADD, UINT_MAX, 1u, 0, SM_ENC_UI(0))
		SM_ROW("sub.ui.zerom1", UI, SUB, 0u, 1u, 0, SM_ENC_UI(UINT_MAX))
		SM_ROW("add.ull.maxp1", ULL, ADD, ULLONG_MAX, 1ull, 0, SM_ENC_ULL(0))
		SM_ROW("add.uc.maxp1", UC, ADD, UCHAR_MAX, 1, 0, SM_ENC_UC(0))
		SM_ROW("add.us.maxp1", US, ADD, USHRT_MAX, 1, 0, SM_ENC_US(0))
		SM_ROW("add.en.max", EN, ADD, SM_E_MAX, 1, 0, SM_ENC_EN(INT_MIN))

		SM_ROW("com.si.zero", SI, COM, 0, 0, 0, SM_ENC_SI(-1))
		SM_ROW("com.si.min", SI, COM, INT_MIN, 0, 0, SM_ENC_SI(INT_MAX))
		SM_ROW("com.ui.zero", UI, COM, 0u, 0, 0, SM_ENC_UI(UINT_MAX))
		SM_ROW("com.uc.zero", UC, COM, 0, 0, 0, SM_ENC_UC(UCHAR_MAX))
		SM_ROW("com.bool.zero", BOOL, COM, 0, 0, 0, SM_ENC_BOOL(1))
		SM_ROW("not.si.min", SI, NOT, INT_MIN, 0, 0, SM_ENC_INT(0))
		SM_ROW("not.si.zero", SI, NOT, 0, 0, 0, SM_ENC_INT(1))

		SM_ROW("lt.si.min.max", SI, LT, INT_MIN, INT_MAX, 0, SM_ENC_INT(1))
		SM_ROW("gt.si.min.max", SI, GT, INT_MIN, INT_MAX, 0, SM_ENC_INT(0))
		SM_ROW("lt.ui.hi.one", UI, LT, 0x80000000u, 1u, 0, SM_ENC_INT(0))
		SM_ROW("lt.si.hi.one", SI, LT, (int)0x80000000, 1, 0, SM_ENC_INT(1))
		SM_ROW("le.ull.max.max", ULL, LE, ULLONG_MAX, ULLONG_MAX, 0, SM_ENC_INT(1))
		SM_ROW("ge.sll.min.min", SLL, GE, LLONG_MIN, LLONG_MIN, 0, SM_ENC_INT(1))
		SM_ROW("eq.bool.2.1", BOOL, EQ, 2, 1, 0, SM_ENC_INT(1))
		SM_ROW("ne.ch.hi.neg", CH, NE, 0x80, -128, 0, SM_ENC_INT(0))

		SM_ROW("sel.si.min", SI, SEL, 1, INT_MIN, INT_MAX, SM_ENC_SI(INT_MIN))
		SM_ROW("sel.si.zero", SI, SEL, 0, INT_MIN, INT_MAX, SM_ENC_SI(INT_MAX))
		SM_ROW("sel.ull.hi", ULL, SEL, 1ull, ULLONG_MAX, 0ull, SM_ENC_ULL(ULLONG_MAX))
		SM_ROW("sel.sll.min", SLL, SEL, 0ll, LLONG_MAX, LLONG_MIN, SM_ENC_SLL(LLONG_MIN))
		SM_ROW("sel.uc.hi", UC, SEL, 1, 0x80, 0x7f, SM_ENC_UC(0x80))
		SM_ROW("sel.en.max", EN, SEL, 1, SM_E_MAX, SM_E_A, SM_ENC_EN(SM_E_MAX))

		SM_ROW("land.si.min", SI, LAND, INT_MIN, INT_MIN, 0, SM_ENC_INT(1))
		SM_ROW("lor.si.zero", SI, LOR, 0, 0, 0, SM_ENC_INT(0))
		SM_ROW("land.ull.hi", ULL, LAND, 0x8000000000000000ull, 1ull, 0, SM_ENC_INT(1))

		SM_ROW("and.si.min.max", SI, AND, INT_MIN, INT_MAX, 0, SM_ENC_SI(0))
		SM_ROW("or.si.min.max", SI, OR, INT_MIN, INT_MAX, 0, SM_ENC_SI(-1))
		SM_ROW("xor.si.min.min", SI, XOR, INT_MIN, INT_MIN, 0, SM_ENC_SI(0))
		SM_ROW("xor.ull.max", ULL, XOR, ULLONG_MAX, ULLONG_MAX, 0, SM_ENC_ULL(0))
		SM_ROW("and.ch.hi", CH, AND, 0x80, 0xff, 0, SM_ENC_CH(-128))

		SM_ROW("bool.from.256", BOOL, ADD, 256, 0, 0, SM_ENC_BOOL(1))
		SM_ROW("bool.from.0", BOOL, ADD, 0, 0, 0, SM_ENC_BOOL(0))
		SM_ROW("bool.mul", BOOL, MUL, 1, 1, 0, SM_ENC_BOOL(1))
		SM_ROW("bool.sub", BOOL, SUB, 1, 1, 0, SM_ENC_BOOL(0))
};

static const SmMixRow sm_mix_rows[] = {

		SM_MIX("uac.si_ui.lt.m1.1", SI_UI, LT, -1, 1u, SM_ENC_INT(0))
		SM_MIX("uac.si_ui.gt.m1.1", SI_UI, GT, -1, 1u, SM_ENC_INT(1))
		SM_MIX("uac.si_ui.eq.m1.max", SI_UI, EQ, -1, UINT_MAX, SM_ENC_INT(1))
		SM_MIX("uac.si_ui.lt.min.hi", SI_UI, LT, INT_MIN, 0x80000000u, SM_ENC_INT(0))
		SM_MIX("uac.si_ui.ge.min.hi", SI_UI, GE, INT_MIN, 0x80000000u, SM_ENC_INT(1))
		SM_MIX("uac.si_ui.add.m1.1", SI_UI, ADD, -1, 1u, SM_ENC_UI(0))
		SM_MIX("uac.si_ui.sub.0.1", SI_UI, SUB, 0, 1u, SM_ENC_UI(UINT_MAX))
		SM_MIX("uac.si_ui.mul.m1.2", SI_UI, MUL, -1, 2u, SM_ENC_UI(UINT_MAX - 1))

		SM_MIX("uac.si_ull.lt.m1.1", SI_ULL, LT, -1, 1ull, SM_ENC_INT(0))
		SM_MIX("uac.si_ull.eq.m1.max", SI_ULL, EQ, -1, ULLONG_MAX, SM_ENC_INT(1))
		SM_MIX("uac.si_ull.add.m1.1", SI_ULL, ADD, -1, 1ull, SM_ENC_ULL(0))

		SM_MIX("uac.sll_ull.lt.min.hi", SLL_ULL, LT, LLONG_MIN, 0x8000000000000000ull, SM_ENC_INT(0))
		SM_MIX("uac.sll_ull.eq.min.hi", SLL_ULL, EQ, LLONG_MIN, 0x8000000000000000ull, SM_ENC_INT(1))
		SM_MIX("uac.sll_ull.gt.m1.0", SLL_ULL, GT, -1ll, 0ull, SM_ENC_INT(1))

		SM_MIX("uac.sc_uc.lt.m1.1", SC_UC, LT, -1, 1, SM_ENC_INT(1))
		SM_MIX("uac.sc_uc.add.min.max", SC_UC, ADD, SCHAR_MIN, UCHAR_MAX, SM_ENC_SI(127))
		SM_MIX("uac.ss_us.lt.m1.1", SS_US, LT, -1, 1, SM_ENC_INT(1))
		SM_MIX("uac.ss_us.add.min.max", SS_US, ADD, SHRT_MIN, USHRT_MAX, SM_ENC_SI(32767))

		SM_MIX("uac.si_sll.lt.min.min", SI_SLL, LT, INT_MIN, (long long)INT_MIN, SM_ENC_INT(0))
		SM_MIX("uac.si_sll.mul.max.2", SI_SLL, MUL, INT_MAX, 2ll, SM_ENC_SLL(4294967294ll))
		SM_MIX("uac.ui_sll.lt.max.m1", UI_SLL, LT, UINT_MAX, -1ll, SM_ENC_INT(0))
		SM_MIX("uac.ui_sll.add.max.1", UI_SLL, ADD, UINT_MAX, 1ll, SM_ENC_SLL(4294967296ll))
		SM_MIX("uac.ui_ull.lt.max.max", UI_ULL, LT, UINT_MAX, ULLONG_MAX, SM_ENC_INT(1))

		SM_MIX("uac.ch_si.lt.hi.0", CH_SI, LT, 0x80, 0, SM_ENC_INT(1))
		SM_MIX("uac.bool_si.eq.2.1", BOOL_SI, EQ, 2, 1, SM_ENC_INT(1))
		SM_MIX("uac.en_ui.lt.max.hi", EN_UI, LT, SM_E_MAX, 0x80000000u, SM_ENC_INT(1))
		SM_MIX("uac.sl_ul.lt.m1.1", SL_UL, LT, -1l, 1ul, SM_ENC_INT(0))
		SM_MIX("uac.sl_ul.add.m1.1", SL_UL, ADD, -1l, 1ul, SM_ENC_UL(0))

		SM_MIX("uac.si_ui.shl.m1.1", SI_UI, SHL, -1, 1u, SM_ENC_UI(0xfffffffeu))
		SM_MIX("uac.si_ui.shr.m1.1", SI_UI, SHR, -1, 1u, SM_ENC_UI(0xffffffffu))
		SM_MIX("uac.sll_ull.shr.min.1", SLL_ULL, SHR, LLONG_MIN, 1ull, SM_ENC_ULL(0xc000000000000000ull))
};

static const SmTrapRow sm_trap_rows[] = {

		SM_TRAP("trap.div.si.min.m1", SI, DIV, INT_MIN, -1, 0, INT_MIN)
		SM_TRAP("trap.rem.si.min.m1", SI, REM, INT_MIN, -1, 0, 0)
		SM_TRAP("trap.div.sll.min.m1", SLL, DIV, LLONG_MIN, -1ll, 0, LLONG_MIN)
		SM_TRAP("trap.rem.sll.min.m1", SLL, REM, LLONG_MIN, -1ll, 0, 0ll)
		SM_TRAP("trap.div.sl.min.m1", SL, DIV, LONG_MIN, -1l, 0, LONG_MIN)
		SM_TRAP("trap.div.si.zero", SI, DIV, 1, 0, 0, 0)
		SM_TRAP("trap.rem.si.zero", SI, REM, 1, 0, 0, 1)
		SM_TRAP("trap.div.ui.zero", UI, DIV, 1u, 0u, 0, 0u)
		SM_TRAP("trap.div.ull.zero", ULL, DIV, 1ull, 0ull, 0, 0ull)
		SM_TRAP("trap.div.sll.zero", SLL, DIV, 1ll, 0ll, 0, 0ll)
};

typedef struct { int x, y, z; } SmPt;

static const int sm_desig_a[12] = {[2 ... 5] = 7, [0] = 1, [11] = 9};
static const int sm_desig_b[8] = {[1] = 2, 3, 4, [6] = 5};
static const int sm_desig_c[6] = {[5] = 1, [0 ... 2] = 4, [3] = 8};
static const SmPt sm_desig_p[3] = {[1].y = 5, 6, [0].z = 7};
static const int sm_desig_m[3][3] = {[1][1] = 5, 6, [0][2] = 3};

typedef struct
{
	const char *name;
	const int *base;
	int idx;
	SmBits want;
} SmIRow;

#define SM_INIT(nm, ARR, IDX, WANT) \
	{ nm, (const int *)(ARR), (IDX), (SmBits)(long long)(WANT) },

static const SmIRow sm_init_rows[] = {

		SM_INIT("desig.a.0", sm_desig_a, 0, 1)
		SM_INIT("desig.a.1", sm_desig_a, 1, 0)
		SM_INIT("desig.a.2", sm_desig_a, 2, 7)
		SM_INIT("desig.a.5", sm_desig_a, 5, 7)
		SM_INIT("desig.a.6", sm_desig_a, 6, 0)
		SM_INIT("desig.a.11", sm_desig_a, 11, 9)
		SM_INIT("desig.b.0", sm_desig_b, 0, 0)
		SM_INIT("desig.b.1", sm_desig_b, 1, 2)
		SM_INIT("desig.b.2", sm_desig_b, 2, 3)
		SM_INIT("desig.b.3", sm_desig_b, 3, 4)
		SM_INIT("desig.b.4", sm_desig_b, 4, 0)
		SM_INIT("desig.b.6", sm_desig_b, 6, 5)
		SM_INIT("desig.c.0", sm_desig_c, 0, 4)
		SM_INIT("desig.c.2", sm_desig_c, 2, 4)
		SM_INIT("desig.c.3", sm_desig_c, 3, 8)
		SM_INIT("desig.c.4", sm_desig_c, 4, 0)
		SM_INIT("desig.c.5", sm_desig_c, 5, 1)
		SM_INIT("desig.p.0.x", sm_desig_p, 0, 0)
		SM_INIT("desig.p.0.z", sm_desig_p, 2, 7)
		SM_INIT("desig.p.1.y", sm_desig_p, 4, 5)
		SM_INIT("desig.p.1.z", sm_desig_p, 5, 6)
		SM_INIT("desig.m.0.2", sm_desig_m, 2, 3)
		SM_INIT("desig.m.1.1", sm_desig_m, 4, 5)
		SM_INIT("desig.m.1.2", sm_desig_m, 5, 6)
		SM_INIT("desig.m.2.0", sm_desig_m, 6, 0)
};

static const int sm_init_rows_count =
		(int)(sizeof sm_init_rows / sizeof sm_init_rows[0]);

static const int sm_rows_count = (int)(sizeof sm_rows / sizeof sm_rows[0]);
static const int sm_mix_rows_count = (int)(sizeof sm_mix_rows / sizeof sm_mix_rows[0]);
static const int sm_trap_rows_count = (int)(sizeof sm_trap_rows / sizeof sm_trap_rows[0]);

#endif
