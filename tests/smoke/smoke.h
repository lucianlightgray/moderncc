#ifndef MCC_SMOKE_H
#define MCC_SMOKE_H

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

typedef unsigned long long SmBits;

typedef enum { SM_E_A = 0, SM_E_B = 1, SM_E_MAX = 2147483647 } SmEnum;

#define SM_SIGFPE 8

#if defined __i386__ || defined __x86_64__
#define SM_DIV_TRAPS 1
#else
#define SM_DIV_TRAPS 0
#endif

#define SM_ENC_INT(v) ((SmBits)(long long)(int)(v))

#define SM_LONG_WIDTH ((int)(8 * sizeof(long)))

#define SM_ITYPES(X) \
	X(BOOL, _Bool, 1) \
	X(CH, char, 8) \
	X(SC, signed char, 8) \
	X(UC, unsigned char, 8) \
	X(SS, short, 16) \
	X(US, unsigned short, 16) \
	X(SI, int, 32) \
	X(UI, unsigned int, 32) \
	X(SL, long, SM_LONG_WIDTH) \
	X(UL, unsigned long, SM_LONG_WIDTH) \
	X(SLL, long long, 64) \
	X(ULL, unsigned long long, 64) \
	X(EN, SmEnum, 32)

#define SM_CTY_BOOL _Bool
#define SM_CTY_CH char
#define SM_CTY_SC signed char
#define SM_CTY_UC unsigned char
#define SM_CTY_SS short
#define SM_CTY_US unsigned short
#define SM_CTY_SI int
#define SM_CTY_UI unsigned int
#define SM_CTY_SL long
#define SM_CTY_UL unsigned long
#define SM_CTY_SLL long long
#define SM_CTY_ULL unsigned long long
#define SM_CTY_EN SmEnum

#define SM_ENC_BOOL(v) ((SmBits)(_Bool)(v))
#define SM_ENC_CH(v) ((SmBits)(long long)(char)(v))
#define SM_ENC_SC(v) ((SmBits)(long long)(signed char)(v))
#define SM_ENC_UC(v) ((SmBits)(unsigned char)(v))
#define SM_ENC_SS(v) ((SmBits)(long long)(short)(v))
#define SM_ENC_US(v) ((SmBits)(unsigned short)(v))
#define SM_ENC_SI(v) ((SmBits)(long long)(int)(v))
#define SM_ENC_UI(v) ((SmBits)(unsigned int)(v))
#define SM_ENC_SL(v) ((SmBits)(long long)(long)(v))
#define SM_ENC_UL(v) ((SmBits)(unsigned long)(v))
#define SM_ENC_SLL(v) ((SmBits)(long long)(v))
#define SM_ENC_ULL(v) ((SmBits)(unsigned long long)(v))
#define SM_ENC_EN(v) ((SmBits)(long long)(int)(SmEnum)(v))

#define SM_OPS(X, A) \
	X(A, ADD) X(A, SUB) X(A, MUL) X(A, DIV) X(A, REM) \
	X(A, AND) X(A, OR) X(A, XOR) X(A, SHL) X(A, SHR) \
	X(A, LT) X(A, LE) X(A, GT) X(A, GE) X(A, EQ) X(A, NE) \
	X(A, LAND) X(A, LOR) X(A, NEG) X(A, COM) X(A, NOT) X(A, SEL) X(A, ABS) \
	X(A, ABSC) X(A, LABSC) X(A, LLABSC)

#define SM_OP_NEEDS_NZ_B(op) ((op) == SM_O_DIV || (op) == SM_O_REM)

#define SM_SOPS(X, A) \
	X(A, PREINC) X(A, PREDEC) X(A, POSTINC) X(A, POSTDEC) \
	X(A, ADDASG) X(A, SUBASG) X(A, SHLASG) X(A, SHRASG) X(A, MULASG)

#define SM_SEXPR_PREINC(T, E, v, b) (++v, E(v))
#define SM_SEXPR_PREDEC(T, E, v, b) (--v, E(v))
#define SM_SEXPR_POSTINC(T, E, v, b) (v++, E(v))
#define SM_SEXPR_POSTDEC(T, E, v, b) (v--, E(v))
#define SM_SEXPR_ADDASG(T, E, v, b) (v += (T)(b), E(v))
#define SM_SEXPR_SUBASG(T, E, v, b) (v -= (T)(b), E(v))
#define SM_SEXPR_SHLASG(T, E, v, b) (v <<= (int)(b), E(v))
#define SM_SEXPR_SHRASG(T, E, v, b) (v >>= (int)(b), E(v))
#define SM_SEXPR_MULASG(T, E, v, b) (v *= (T)(b), E(v))

#define SM_EXPR_ADD(T, E, a, b, c) E((T)(a) + (T)(b))
#define SM_EXPR_SUB(T, E, a, b, c) E((T)(a) - (T)(b))
#define SM_EXPR_MUL(T, E, a, b, c) E((T)(a) * (T)(b))
#define SM_EXPR_DIV(T, E, a, b, c) E((T)(a) / (T)(b))
#define SM_EXPR_REM(T, E, a, b, c) E((T)(a) % (T)(b))
#define SM_EXPR_AND(T, E, a, b, c) E((T)(a) & (T)(b))
#define SM_EXPR_OR(T, E, a, b, c) E((T)(a) | (T)(b))
#define SM_EXPR_XOR(T, E, a, b, c) E((T)(a) ^ (T)(b))
#define SM_EXPR_SHL(T, E, a, b, c) E((T)(a) << (int)(b))
#define SM_EXPR_SHR(T, E, a, b, c) E((T)(a) >> (int)(b))
#define SM_EXPR_LT(T, E, a, b, c) SM_ENC_INT((T)(a) < (T)(b))
#define SM_EXPR_LE(T, E, a, b, c) SM_ENC_INT((T)(a) <= (T)(b))
#define SM_EXPR_GT(T, E, a, b, c) SM_ENC_INT((T)(a) > (T)(b))
#define SM_EXPR_GE(T, E, a, b, c) SM_ENC_INT((T)(a) >= (T)(b))
#define SM_EXPR_EQ(T, E, a, b, c) SM_ENC_INT((T)(a) == (T)(b))
#define SM_EXPR_NE(T, E, a, b, c) SM_ENC_INT((T)(a) != (T)(b))
#define SM_EXPR_LAND(T, E, a, b, c) SM_ENC_INT((T)(a) && (T)(b))
#define SM_EXPR_LOR(T, E, a, b, c) SM_ENC_INT((T)(a) || (T)(b))
#define SM_EXPR_NEG(T, E, a, b, c) E(-(T)(a))
#define SM_EXPR_COM(T, E, a, b, c) E(~(T)(a))
#define SM_EXPR_NOT(T, E, a, b, c) SM_ENC_INT(!(T)(a))
#define SM_EXPR_SEL(T, E, a, b, c) E((T)(a) ? (T)(b) : (T)(c))
#define SM_EXPR_ABS(T, E, a, b, c) E((T)(a) < (T)0 ? -(T)(a) : (T)(a))
#define SM_EXPR_ABSC(T, E, a, b, c) SM_ENC_INT(abs((int)(T)(a)))
#define SM_EXPR_LABSC(T, E, a, b, c) SM_ENC_SL(labs((long)(T)(a)))
#define SM_EXPR_LLABSC(T, E, a, b, c) SM_ENC_SLL(llabs((long long)(T)(a)))

#define SM_MIXPAIRS(X) \
	X(SI_UI, int, unsigned int, SM_ENC_UI) \
	X(SI_ULL, int, unsigned long long, SM_ENC_ULL) \
	X(SLL_ULL, long long, unsigned long long, SM_ENC_ULL) \
	X(SC_UC, signed char, unsigned char, SM_ENC_SI) \
	X(SS_US, short, unsigned short, SM_ENC_SI) \
	X(SI_SLL, int, long long, SM_ENC_SLL) \
	X(UI_SLL, unsigned int, long long, SM_ENC_SLL) \
	X(UI_ULL, unsigned int, unsigned long long, SM_ENC_ULL) \
	X(CH_SI, char, int, SM_ENC_SI) \
	X(BOOL_SI, _Bool, int, SM_ENC_SI) \
	X(EN_UI, SmEnum, unsigned int, SM_ENC_UI) \
	X(SL_UL, long, unsigned long, SM_ENC_UL)

#define SM_MIXOPS(X, A) \
	X(A, LT) X(A, LE) X(A, GT) X(A, GE) X(A, EQ) X(A, NE) \
	X(A, ADD) X(A, SUB) X(A, MUL) X(A, AND) X(A, OR) X(A, XOR) X(A, SHL) X(A, SHR)

#define SM_MEXPR_LT(L, R, E, a, b) SM_ENC_INT((L)(a) < (R)(b))
#define SM_MEXPR_LE(L, R, E, a, b) SM_ENC_INT((L)(a) <= (R)(b))
#define SM_MEXPR_GT(L, R, E, a, b) SM_ENC_INT((L)(a) > (R)(b))
#define SM_MEXPR_GE(L, R, E, a, b) SM_ENC_INT((L)(a) >= (R)(b))
#define SM_MEXPR_EQ(L, R, E, a, b) SM_ENC_INT((L)(a) == (R)(b))
#define SM_MEXPR_NE(L, R, E, a, b) SM_ENC_INT((L)(a) != (R)(b))
#define SM_MEXPR_ADD(L, R, E, a, b) E((L)(a) + (R)(b))
#define SM_MEXPR_SUB(L, R, E, a, b) E((L)(a) - (R)(b))
#define SM_MEXPR_MUL(L, R, E, a, b) E((L)(a) * (R)(b))
#define SM_MEXPR_AND(L, R, E, a, b) E((L)(a) & (R)(b))
#define SM_MEXPR_OR(L, R, E, a, b) E((L)(a) | (R)(b))
#define SM_MEXPR_XOR(L, R, E, a, b) E((L)(a) ^ (R)(b))
#define SM_MEXPR_SHL(L, R, E, a, b) E((L)(a) << ((int)(b) & 31))
#define SM_MEXPR_SHR(L, R, E, a, b) E((L)(a) >> ((int)(b) & 31))

enum {
#define SM_T_ROW(tag, cty, w) SM_T_##tag,
	SM_ITYPES(SM_T_ROW)
#undef SM_T_ROW
			SM_T_COUNT
};

enum {
#define SM_O_ROW(a, op) SM_O_##op,
	SM_OPS(SM_O_ROW, _)
#undef SM_O_ROW
			SM_O_COUNT
};

enum {
#define SM_S_ROW(a, op) SM_S_##op,
	SM_SOPS(SM_S_ROW, _)
#undef SM_S_ROW
			SM_S_COUNT
};

enum {
#define SM_P_ROW(tag, l, r, e) SM_P_##tag,
	SM_MIXPAIRS(SM_P_ROW)
#undef SM_P_ROW
			SM_P_COUNT
};

enum {
#define SM_M_ROW(a, op) SM_M_##op,
	SM_MIXOPS(SM_M_ROW, _)
#undef SM_M_ROW
			SM_M_COUNT
};

#define SM_KEY(tag, op) ((tag)*SM_O_COUNT + (op))
#define SM_SKEY(tag, op) ((tag)*SM_S_COUNT + (op))
#define SM_MKEY(pair, op) ((pair)*SM_M_COUNT + (op))

#define SM_FOLD_CHECKED 1
#define SM_FOLD_SKIP 0

typedef struct
{
	const char *name;
	unsigned short tag;
	unsigned short op;
	unsigned short foldable;
	SmBits a, b, c;
	SmBits fold;
	SmBits want;
} SmRow;

typedef struct
{
	const char *name;
	unsigned short pair;
	unsigned short op;
	unsigned short foldable;
	SmBits a, b;
	SmBits fold;
	SmBits want;
} SmMixRow;

typedef struct
{
	const char *name;
	unsigned short tag;
	unsigned short op;
	SmBits a, b, c;
	int sig;
	SmBits nofault;
} SmTrapRow;

#define SM_ROW(nm, TY, OP, A, B, C, WANT) \
	{ nm, SM_T_##TY, SM_O_##OP, SM_FOLD_CHECKED, \
		SM_ENC_##TY(A), SM_ENC_##TY(B), SM_ENC_##TY(C), \
		SM_EXPR_##OP(SM_CTY_##TY, SM_ENC_##TY, A, B, C), \
		(SmBits)(WANT) },

#define SM_ROW_NF(nm, TY, OP, A, B, C, WANT) \
	{ nm, SM_T_##TY, SM_O_##OP, SM_FOLD_SKIP, \
		SM_ENC_##TY(A), SM_ENC_##TY(B), SM_ENC_##TY(C), \
		(SmBits)(WANT), (SmBits)(WANT) },

#define SM_MIX(nm, PAIR, OP, A, B, WANT) \
	{ nm, SM_P_##PAIR, SM_M_##OP, SM_FOLD_CHECKED, \
		(SmBits)(long long)(A), (SmBits)(long long)(B), \
		SM_MEXPR_##OP(SM_LTY_##PAIR, SM_RTY_##PAIR, SM_MENC_##PAIR, A, B), \
		(SmBits)(WANT) },

#define SM_SROW(nm, TY, OP, A, B, WANT) \
	{ nm, SM_T_##TY, SM_S_##OP, SM_FOLD_SKIP, \
		SM_ENC_##TY(A), SM_ENC_##TY(B), 0, (SmBits)(WANT), (SmBits)(WANT) },

#define SM_TRAP(nm, TY, OP, A, B, C, NF) \
	{ nm, SM_T_##TY, SM_O_##OP, \
		SM_ENC_##TY(A), SM_ENC_##TY(B), SM_ENC_##TY(C), SM_SIGFPE, \
		SM_ENC_##TY(NF) },

#define SM_LTY_SI_UI int
#define SM_RTY_SI_UI unsigned int
#define SM_MENC_SI_UI SM_ENC_UI
#define SM_LTY_SI_ULL int
#define SM_RTY_SI_ULL unsigned long long
#define SM_MENC_SI_ULL SM_ENC_ULL
#define SM_LTY_SLL_ULL long long
#define SM_RTY_SLL_ULL unsigned long long
#define SM_MENC_SLL_ULL SM_ENC_ULL
#define SM_LTY_SC_UC signed char
#define SM_RTY_SC_UC unsigned char
#define SM_MENC_SC_UC SM_ENC_SI
#define SM_LTY_SS_US short
#define SM_RTY_SS_US unsigned short
#define SM_MENC_SS_US SM_ENC_SI
#define SM_LTY_SI_SLL int
#define SM_RTY_SI_SLL long long
#define SM_MENC_SI_SLL SM_ENC_SLL
#define SM_LTY_UI_SLL unsigned int
#define SM_RTY_UI_SLL long long
#define SM_MENC_UI_SLL SM_ENC_SLL
#define SM_LTY_UI_ULL unsigned int
#define SM_RTY_UI_ULL unsigned long long
#define SM_MENC_UI_ULL SM_ENC_ULL
#define SM_LTY_CH_SI char
#define SM_RTY_CH_SI int
#define SM_MENC_CH_SI SM_ENC_SI
#define SM_LTY_BOOL_SI _Bool
#define SM_RTY_BOOL_SI int
#define SM_MENC_BOOL_SI SM_ENC_SI
#define SM_LTY_EN_UI SmEnum
#define SM_RTY_EN_UI unsigned int
#define SM_MENC_EN_UI SM_ENC_UI
#define SM_LTY_SL_UL long
#define SM_RTY_SL_UL unsigned long
#define SM_MENC_SL_UL SM_ENC_UL

#endif
