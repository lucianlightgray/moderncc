/* An INDEPENDENT oracle for ast_eval_binop(), which is N7's stated fix.
 *
 * N7: the slice evaluator's arithmetic is unobservable. `ast_eval_binop` is
 * reached 66,436,580 times compiling tests/smoke/subject.c at -O4, and
 * injecting `r = s + 1` into its 32-bit signed `+` arm left EVERYTHING
 * byte-identical: pairs=624 certified=532 differ=0, all 1782 --dump rows on all
 * six engines, the sweep digest, and eleven smoke cells. The reason is
 * structural rather than an oversight in the checks: `differ` compares two
 * arenas that are BOTH evaluated by ast_eval_binop, so a shared fault cancels
 * by construction. Self-comparison cannot see it. A wrong answer 66 million
 * times per compile changed nothing observable.
 *
 * So the oracle has to come from outside that function. This computes each
 * result a DIFFERENT WAY -- in 128-bit arithmetic, with range and definedness
 * decided explicitly from the value rather than from the same overflow idiom
 * the implementation uses -- and compares. It links no part of the compiler:
 * src/ast_eval_slice.h is self-contained (limits.h, stdint.h, and its own TOK_*
 * fallbacks), so this is a direct unit differential over the real function, not
 * a copy of it.
 *
 * Contract being pinned: ast_eval_binop returns 1 and writes *out when the
 * operation is defined and representable, and returns 0 when it is not
 * (signed overflow, division by zero, INT_MIN/-1, out-of-range shift). The
 * result is narrowed to the operand width and signedness on the way out.
 */

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h> /* the header's f64 bit-punning uses memcpy */

/* The arithmetic half of the header still uses a handful of VT_* in its type
 * predicates. These are the real values from src/mcc.h. None is reachable from
 * ast_eval_binop(), which takes is64/is_unsigned as plain ints -- they exist so
 * the file compiles standalone, and are listed openly rather than hidden behind
 * a compiler header, so this test builds with plain gcc and links no part of
 * mcc. */
#define VT_BTYPE 0x001f
#define VT_BITFIELD 0x0100
#define VT_BOOL 11
#define VT_BYTE 1
#define VT_CONST 0x0030
#define VT_DOUBLE 9
#define VT_FUNC 6
#define VT_INT 3
#define VT_LLONG 4
#define VT_LOCAL 0x0032
#define VT_LVAL 0x0100
#define VT_PTR 5
#define VT_SHORT 2
#define VT_STRUCT 7
#define VT_SYM 0x0200
#define VT_UNSIGNED 0x0020
#define VT_VALMASK 0x007f
#define VT_VOLATILE 0x0400
#define VT_ARRAY 0x0080
#define MCC_PTR_SIZE 8

/* Take only the arithmetic half of the header. Everything below its
 * AST_EVAL_SLICE_ARITH_ONLY guard is written against the AST types; everything
 * above needs nothing but <stdint.h>, which is what makes this unit
 * differential possible at all. */
#define AST_EVAL_SLICE_ARITH_ONLY 1

#include "ast_eval_slice.h"

static int fails;
static long checks;

static void fail(const char *what, int op, long long a, long long b, int is64,
								 int uns, long long got, long long want) {
	if (fails < 20)
		printf("FAIL %s op=%d(%c) a=%lld b=%lld is64=%d uns=%d got=%lld want=%lld\n",
					 what, op, (op >= 32 && op < 127) ? op : '?', a, b, is64, uns, got,
					 want);
	fails++;
}

/* The reference. Deliberately written from the C semantics rather than from the
 * implementation: compute wide, then decide representability by comparing
 * against the type's bounds. */
static int ref_binop(int op, int64_t a, int64_t b, int is64, int uns,
										 int64_t *out) {
	/* Narrow inputs the way the contract says, using an explicit mask/extend
	 * rather than the implementation's helper. */
	if (!is64) {
		uint32_t ta = (uint32_t)(uint64_t)a, tb = (uint32_t)(uint64_t)b;
		a = uns ? (int64_t)(uint64_t)ta : (int64_t)(int32_t)ta;
		b = uns ? (int64_t)(uint64_t)tb : (int64_t)(int32_t)tb;
	}

	__int128 wa = uns ? (__int128)(unsigned __int128)(uint64_t)a : (__int128)a;
	__int128 wb = uns ? (__int128)(unsigned __int128)(uint64_t)b : (__int128)b;
	__int128 w;
	int is_cmp = 0;

	switch (op) {
	case '+': w = wa + wb; break;
	case '-': w = wa - wb; break;
	case '*': w = wa * wb; break;
	case '/':
	case '%':
	case TOK_PDIV:
	case TOK_UDIV:
	case TOK_UMOD: {
		int u = uns || op == TOK_UDIV || op == TOK_UMOD;
		__int128 da = u ? (__int128)(uint64_t)(is64 ? (uint64_t)a : (uint32_t)a)
										: (__int128)a;
		__int128 db = u ? (__int128)(uint64_t)(is64 ? (uint64_t)b : (uint32_t)b)
										: (__int128)b;
		if (db == 0)
			return 0;
		/* The one signed case with no representable quotient. */
		if (!u) {
			__int128 lo = is64 ? (__int128)INT64_MIN : (__int128)INT32_MIN;
			if (da == lo && db == -1)
				return 0;
		}
		int mod = (op == '%' || op == TOK_UMOD);
		w = mod ? da % db : da / db;
		break;
	}
	case TOK_SHL:
	case TOK_SHR:
	case TOK_SAR: {
		int width = is64 ? 64 : 32;
		uint64_t sh = (uint64_t)b;
		if (b < 0 || sh >= (uint64_t)width)
			return 0;
		if (op == TOK_SHL) {
			/* Left shift WRAPS within the width and is never refused, signed or
			 * not. That is the contract this evaluator implements, and it is what
			 * gcc and clang fold `1 << 31` to as well. An earlier draft of this
			 * oracle refused it as C11 6.5.7p4 undefined behaviour and reported
			 * 11,417 disagreements -- all of them the oracle being stricter than
			 * the function, none of them a fault. Pinning the contract is the
			 * job here; changing it is not. */
			uint64_t uv = is64 ? (uint64_t)a : (uint32_t)a;
			uint64_t sv = is64 ? (uv << sh) : (uint64_t)(uint32_t)((uint32_t)uv << sh);
			w = (__int128)(uint64_t)sv;
		} else if (op == TOK_SAR) {
			/* SAR is the ARITHMETIC right shift: the opcode carries the
			 * signedness, so it sign-extends from the operand width whatever the
			 * is_unsigned flag says. (SHR is the logical one.) An earlier draft
			 * used the unsigned-narrowed operand here and reported every
			 * negative-input SAR as a mismatch -- the oracle misreading the
			 * opcode, not the function computing the wrong thing. */
			int64_t sa = is64 ? a : (int64_t)(int32_t)(uint32_t)(uint64_t)a;
			w = (__int128)(sa >> sh);
		} else { /* TOK_SHR: logical */
			uint64_t uv = is64 ? (uint64_t)a : (uint32_t)a;
			w = (__int128)(uint64_t)(uv >> sh);
		}
		break;
	}
	case '&': w = (__int128)(int64_t)((uint64_t)a & (uint64_t)b); break;
	case '|': w = (__int128)(int64_t)((uint64_t)a | (uint64_t)b); break;
	case '^': w = (__int128)(int64_t)((uint64_t)a ^ (uint64_t)b); break;
	case TOK_EQ: w = (a == b); is_cmp = 1; break;
	case TOK_NE: w = (a != b); is_cmp = 1; break;
	/* LT/GE/LE/GT take their signedness from the operand flag; the U-prefixed
	 * forms below are unconditionally unsigned. Both spellings exist precisely
	 * because the flag does not always decide, so an oracle that assumed either
	 * one universally would be wrong for half of them. */
	case TOK_LT: w = uns ? ((uint64_t)a < (uint64_t)b) : (a < b); is_cmp = 1; break;
	case TOK_GE: w = uns ? ((uint64_t)a >= (uint64_t)b) : (a >= b); is_cmp = 1; break;
	case TOK_LE: w = uns ? ((uint64_t)a <= (uint64_t)b) : (a <= b); is_cmp = 1; break;
	case TOK_GT: w = uns ? ((uint64_t)a > (uint64_t)b) : (a > b); is_cmp = 1; break;
	case TOK_ULT: w = ((uint64_t)a < (uint64_t)b); is_cmp = 1; break;
	case TOK_UGE: w = ((uint64_t)a >= (uint64_t)b); is_cmp = 1; break;
	case TOK_ULE: w = ((uint64_t)a <= (uint64_t)b); is_cmp = 1; break;
	case TOK_UGT: w = ((uint64_t)a > (uint64_t)b); is_cmp = 1; break;
	case TOK_LAND: w = (a != 0 && b != 0); is_cmp = 1; break;
	case TOK_LOR: w = (a != 0 || b != 0); is_cmp = 1; break;
	default: return 0;
	}

	if (!is_cmp && !uns) {
		/* Signed overflow is undefined and must be refused, not wrapped. Shifts
		 * and the bitwise ops are checked here too: for those the value is
		 * already in range by construction, so this only rejects the arithmetic
		 * that genuinely left it. */
		__int128 lo = is64 ? (__int128)INT64_MIN : (__int128)INT32_MIN;
		__int128 hi = is64 ? (__int128)INT64_MAX : (__int128)INT32_MAX;
		if (op == '+' || op == '-' || op == '*') {
			if (w < lo || w > hi)
				return 0;
		}
	}

	/* Narrow out, explicitly. */
	int64_t r;
	if (is64) {
		r = (int64_t)(uint64_t)(unsigned __int128)w;
	} else {
		uint32_t t = (uint32_t)(uint64_t)(unsigned __int128)w;
		r = uns ? (int64_t)(uint64_t)t : (int64_t)(int32_t)t;
	}
	*out = r;
	return 1;
}

static const int OPS[] = {
		'+',      '-',      '*',      '/',      '%',      TOK_PDIV, TOK_UDIV,
		TOK_UMOD, TOK_SHL,  TOK_SHR,  TOK_SAR,  '&',      '|',      '^',
		TOK_EQ,   TOK_NE,   TOK_LT,   TOK_GE,   TOK_LE,   TOK_GT,   TOK_ULT,
		TOK_UGE,  TOK_ULE,  TOK_UGT,  TOK_LAND, TOK_LOR};

/* Edge cases first: every boundary an overflow check can be wrong about. */
static const int64_t VALS[] = {0,
															 1,
															 -1,
															 2,
															 -2,
															 3,
															 7,
															 31,
															 32,
															 33,
															 63,
															 64,
															 65,
															 -31,
															 -32,
															 -63,
															 -64,
															 100,
															 -100,
															 65535,
															 65536,
															 INT32_MAX,
															 INT32_MIN,
															 (int64_t)INT32_MAX + 1,
															 (int64_t)INT32_MIN - 1,
															 (int64_t)INT32_MAX / 2,
															 (int64_t)INT32_MIN / 2,
															 INT64_MAX,
															 INT64_MIN,
															 INT64_MAX / 2,
															 INT64_MIN / 2,
															 (int64_t)0xFFFFFFFFu,
															 (int64_t)0x80000000u,
															 (int64_t)0x7FFFFFFFu};

static uint64_t rng_state = 0x243F6A8885A308D3ull;
static uint64_t rng(void) {
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 7;
	rng_state ^= rng_state << 17;
	return rng_state;
}

static void one(int op, int64_t a, int64_t b, int is64, int uns) {
	int64_t got = 0, want = 0;
	int rg = ast_eval_binop(op, a, b, is64, uns, &got);
	int rw = ref_binop(op, a, b, is64, uns, &want);
	checks++;
	if (rg != rw) {
		fail(rg ? "evaluated where the oracle refuses"
						: "refused where the oracle evaluates",
				 op, (long long)a, (long long)b, is64, uns, rg, rw);
		return;
	}
	if (rg && got != want)
		fail("wrong value", op, (long long)a, (long long)b, is64, uns,
				 (long long)got, (long long)want);
}

int main(void) {
	const int nops = (int)(sizeof OPS / sizeof OPS[0]);
	const int nval = (int)(sizeof VALS / sizeof VALS[0]);

	for (int o = 0; o < nops; o++)
		for (int i = 0; i < nval; i++)
			for (int j = 0; j < nval; j++)
				for (int is64 = 0; is64 < 2; is64++)
					for (int uns = 0; uns < 2; uns++)
						one(OPS[o], VALS[i], VALS[j], is64, uns);

	/* Then random values, to reach the interior the edge list does not. */
	for (int o = 0; o < nops; o++)
		for (int n = 0; n < 4000; n++) {
			int64_t a = (int64_t)rng(), b = (int64_t)rng();
			if ((n & 3) == 0)
				b = (int64_t)(rng() % 96) - 16; /* shift-sized, incl. out of range */
			for (int is64 = 0; is64 < 2; is64++)
				for (int uns = 0; uns < 2; uns++)
					one(OPS[o], a, b, is64, uns);
		}

	/* Anti-vacuity: a harness that checked nothing would print OK. */
	if (checks < 100000) {
		printf("FAIL: only %ld check(s) ran; the oracle covered almost nothing\n",
					 checks);
		return 1;
	}
	printf(fails ? "FAIL %d of %ld\n" : "OK %d %ld\n", fails, checks);
	return fails ? 1 : 0;
}
