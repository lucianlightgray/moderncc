#ifndef AST_EVAL_SLICE_PROVIDED
#define AST_EVAL_SLICE_PROVIDED 1

#include <limits.h>
#include <stdint.h>

#ifndef TOK_LAND
#define TOK_LAND 0x90
#endif
#ifndef TOK_LOR
#define TOK_LOR 0x91
#endif
#ifndef TOK_ULT
#define TOK_ULT 0x92
#endif
#ifndef TOK_UGE
#define TOK_UGE 0x93
#endif
#ifndef TOK_EQ
#define TOK_EQ 0x94
#endif
#ifndef TOK_NE
#define TOK_NE 0x95
#endif
#ifndef TOK_ULE
#define TOK_ULE 0x96
#endif
#ifndef TOK_UGT
#define TOK_UGT 0x97
#endif
#ifndef TOK_LT
#define TOK_LT 0x9c
#endif
#ifndef TOK_GE
#define TOK_GE 0x9d
#endif
#ifndef TOK_LE
#define TOK_LE 0x9e
#endif
#ifndef TOK_GT
#define TOK_GT 0x9f
#endif
#ifndef TOK_UDIV
#define TOK_UDIV 0x83
#endif
#ifndef TOK_UMOD
#define TOK_UMOD 0x84
#endif
#ifndef TOK_PDIV
#define TOK_PDIV 0x85
#endif
#ifndef TOK_SHL
#define TOK_SHL '<'
#endif
#ifndef TOK_SAR
#define TOK_SAR '>'
#endif
#ifndef TOK_SHR
#define TOK_SHR 0x8b
#endif

static int64_t ast_eval_narrow(int64_t x, int is64, int is_unsigned) {
	if (is64)
		return x;
	if (is_unsigned)
		return (int64_t)(uint32_t)x;
	return (int64_t)(int32_t)x;
}

static int ast_eval_binop(int op, int64_t a, int64_t b, int is64,
													int is_unsigned, int64_t *out) {
	a = ast_eval_narrow(a, is64, is_unsigned);
	b = ast_eval_narrow(b, is64, is_unsigned);
	uint64_t ua = (uint64_t)a, ub = (uint64_t)b;
	int64_t r;
	switch (op) {
	case '+':
		if (is_unsigned) {
			r = (int64_t)(ua + ub);
			break;
		}
		if (is64) {
			r = (int64_t)(ua + ub);
			if (((a ^ r) & (b ^ r)) < 0)
				return 0;
		} else {
			int64_t s = a + b;
			if (s < INT32_MIN || s > INT32_MAX)
				return 0;
			r = s;
		}
		break;
	case '-':
		if (is_unsigned) {
			r = (int64_t)(ua - ub);
			break;
		}
		if (is64) {
			r = (int64_t)(ua - ub);
			if (((a ^ b) & (a ^ r)) < 0)
				return 0;
		} else {
			int64_t s = a - b;
			if (s < INT32_MIN || s > INT32_MAX)
				return 0;
			r = s;
		}
		break;
	case '*':
		if (is_unsigned) {
			r = (int64_t)(ua * ub);
			break;
		}
		if (is64) {
			if (a != 0 && b != 0) {
				r = (int64_t)(ua * ub);
				if (r / a != b || (a == INT64_MIN && b == -1) ||
						(b == INT64_MIN && a == -1))
					return 0;
			} else {
				r = 0;
			}
		} else {
			int64_t s = a * b;
			if (s < INT32_MIN || s > INT32_MAX)
				return 0;
			r = s;
		}
		break;
	case '/':
	case '%':
	case TOK_PDIV:
	case TOK_UDIV:
	case TOK_UMOD: {
		if (b == 0)
			return 0;
		int is_mod = (op == '%' || op == TOK_UMOD);
		int uns = is_unsigned || op == TOK_UDIV || op == TOK_UMOD;
		if (uns) {
			if (is64) {
				r = (int64_t)(is_mod ? ua % ub : ua / ub);
			} else {
				uint32_t x = (uint32_t)ua, y = (uint32_t)ub;
				r = (int64_t)(uint32_t)(is_mod ? x % y : x / y);
			}
		} else {
			int64_t mn = is64 ? INT64_MIN : INT32_MIN;
			if (a == mn && b == -1)
				return 0;
			int64_t q = a / b;
			r = is_mod ? a - b * q : q;
		}
		break;
	}
	case TOK_SHL:
	case TOK_SHR:
	case TOK_SAR: {
		int width = is64 ? 64 : 32;
		if (b < 0 || b >= width)
			return 0;
		unsigned sh = (unsigned)b;
		if (op == TOK_SHL) {
			if (is64)
				r = (int64_t)(ua << sh);
			else
				r = (int64_t)(int32_t)((uint32_t)ua << sh);
		} else if (op == TOK_SHR) {
			if (is64)
				r = (int64_t)(ua >> sh);
			else
				r = (int64_t)(uint32_t)((uint32_t)ua >> sh);
		} else {
			if (is64)
				r = a >> sh;
			else
				r = (int64_t)((int32_t)a >> sh);
		}
		break;
	}
	case '&':
		r = (int64_t)(ua & ub);
		break;
	case '|':
		r = (int64_t)(ua | ub);
		break;
	case '^':
		r = (int64_t)(ua ^ ub);
		break;
	case TOK_EQ:
		r = (a == b);
		break;
	case TOK_NE:
		r = (a != b);
		break;
	case TOK_LT:
		r = (a < b);
		break;
	case TOK_GE:
		r = (a >= b);
		break;
	case TOK_LE:
		r = (a <= b);
		break;
	case TOK_GT:
		r = (a > b);
		break;
	case TOK_ULT:
		r = (ua < ub);
		break;
	case TOK_UGE:
		r = (ua >= ub);
		break;
	case TOK_ULE:
		r = (ua <= ub);
		break;
	case TOK_UGT:
		r = (ua > ub);
		break;
	case TOK_LAND:
		r = (a != 0 && b != 0);
		break;
	case TOK_LOR:
		r = (a != 0 || b != 0);
		break;
	default:
		return 0;
	}
	*out = ast_eval_narrow(r, is64, is_unsigned);
	return 1;
}

#ifndef AST_EVAL_SLICE_KERNEL_ONLY

static int ast_eval_slice_env(const int32_t *off, const int64_t *val, int n,
															int32_t o, int64_t *out) {
	for (int i = 0; i < n; i++)
		if (off[i] == o) {
			*out = val[i];
			return 1;
		}
	return 0;
}

static int ast_eval_slice_intt(int t) {
	int bt = t & VT_BTYPE;
	return bt == VT_BOOL || bt == VT_BYTE || bt == VT_SHORT || bt == VT_INT ||
				 bt == VT_LLONG || bt == VT_PTR;
}

static int ast_eval_slice_is64(int t) {
	int bt = t & VT_BTYPE;
	return bt == VT_LLONG || (MCC_PTR_SIZE == 8 && bt == VT_PTR);
}

static int64_t ast_eval_slice_fit(int64_t x, int t) {
	int bt = t & VT_BTYPE;
	int uns = (t & VT_UNSIGNED) != 0;
	switch (bt) {
	case VT_BOOL:
		return x != 0;
	case VT_BYTE:
		return uns ? (int64_t)(uint8_t)x : (int64_t)(int8_t)x;
	case VT_SHORT:
		return uns ? (int64_t)(uint16_t)x : (int64_t)(int16_t)x;
	case VT_INT:
		return uns ? (int64_t)(uint32_t)x : (int64_t)(int32_t)x;
	default:
		if (MCC_PTR_SIZE == 8 && bt == VT_PTR)
			return x;
		if (bt == VT_PTR)
			return uns ? (int64_t)(uint32_t)x : (int64_t)(int32_t)x;
		return x;
	}
}

static int ast_eval_slice_rec(AstArena *a, AstLocal n, const int32_t *off,
															const int64_t *val, int nenv, int64_t *out) {
	if (n == AST_NONE)
		return 0;
	switch (ast_kind(a, n)) {
	case AST_Literal: {
		int t = ast_type_t(a, n);
		if (ast_bad_type(t) || is_float(t) || !ast_eval_slice_intt(t))
			return 0;
		if ((ast_op(a, n) & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
			return 0;
		*out = ast_eval_slice_fit((int64_t)ast_ival(a, n), t);
		return 1;
	}
	case AST_Ref: {
		int r = ast_op(a, n);
		int t = ast_type_t(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) {
			int64_t v;
			if (!ast_eval_slice_env(off, val, nenv,
															(int32_t)(int64_t)ast_ival(a, n), &v))
				return 0;
			if (!ast_eval_slice_intt(t) || is_float(t))
				return 0;
			*out = v;
			return 1;
		}
		if ((r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
			if (ast_bad_type(t) || is_float(t) || !ast_eval_slice_intt(t))
				return 0;
			*out = ast_eval_slice_fit((int64_t)ast_ival(a, n), t);
			return 1;
		}
		return 0;
	}
	case AST_Load: {
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || ast_kind(a, c) != AST_Ref)
			return 0;
		int r = ast_op(a, c);
		int t = ast_type_t(a, n);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
			return 0;
		if (!ast_eval_slice_intt(t) || is_float(t))
			return 0;
		int64_t v;
		if (!ast_eval_slice_env(off, val, nenv,
														(int32_t)(int64_t)ast_ival(a, c), &v))
			return 0;
		*out = v;
		return 1;
	}
	case AST_Convert: {
		int t = ast_type_t(a, n);
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || is_float(t) || is_float(ast_type_t(a, c)))
			return 0;
		if (ast_bad_type(t) || !ast_eval_slice_intt(t))
			return 0;
		int64_t v;
		if (!ast_eval_slice_rec(a, c, off, val, nenv, &v))
			return 0;
		*out = ast_eval_slice_fit(v, t);
		return 1;
	}
	case AST_Unary: {
		int uop = ast_op(a, n);
		int t = ast_type_t(a, n);
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || ast_bad_type(t) || is_float(t) ||
				!ast_eval_slice_intt(t))
			return 0;
		if (uop != '-' && uop != TOK_NEG && uop != '~' && uop != '!')
			return 0;
		int64_t v;
		if (!ast_eval_slice_rec(a, c, off, val, nenv, &v))
			return 0;
		int is64 = ast_eval_slice_is64(t);
		int uns = (t & VT_UNSIGNED) != 0;
		if (uop == '!') {
			*out = (v == 0);
			return 1;
		}
		if (uop == '~') {
			*out = ast_eval_slice_fit(~v, t);
			return 1;
		}
		return ast_eval_binop('-', 0, v, is64, uns, out);
	}
	case AST_Binary: {
		int bop = ast_op(a, n);
		if (bop == TOK_LAND || bop == TOK_LOR) {
			int want = (bop == TOK_LAND);
			uint32_t nc = ast_nchild(a, n);
			for (uint32_t k = 0; k < nc; k++) {
				int64_t v;
				if (!ast_eval_slice_rec(a, ast_child(a, n, k), off, val, nenv, &v))
					return 0;
				if ((v != 0) != want) {
					*out = want ? 0 : 1;
					return 1;
				}
			}
			*out = want ? 1 : 0;
			return 1;
		}
		if (ast_nchild(a, n) != 2)
			return 0;
		AstLocal x = ast_child(a, n, 0), y = ast_child(a, n, 1);
		int xt = ast_type_t(a, x);
		if (ast_bad_type(xt) || is_float(xt) || is_float(ast_type_t(a, y)) ||
				!ast_eval_slice_intt(xt))
			return 0;
		int64_t lv, rv;
		if (!ast_eval_slice_rec(a, x, off, val, nenv, &lv))
			return 0;
		if (!ast_eval_slice_rec(a, y, off, val, nenv, &rv))
			return 0;
		int is64 = ast_eval_slice_is64(xt);
		int uns = (xt & VT_UNSIGNED) != 0;
		return ast_eval_binop(bop, lv, rv, is64, uns, out);
	}
	case AST_If: {
		if (ast_nchild(a, n) != 3)
			return 0;
		int64_t cv;
		if (!ast_eval_slice_rec(a, ast_child(a, n, 0), off, val, nenv, &cv))
			return 0;
		AstLocal taken = cv != 0 ? ast_child(a, n, 1) : ast_child(a, n, 2);
		return ast_eval_slice_rec(a, taken, off, val, nenv, out);
	}
	default:
		return 0;
	}
}

static int ast_eval_slice(AstArena *a, AstLocal node, const int32_t *off,
													const int64_t *val, int n, int64_t *out) {
	return ast_eval_slice_rec(a, node, off, val, n, out);
}

#define AST_EVAL_SLICE_MAXRET 64
#define AST_EVAL_SLICE_SAMPLE_CAP 8
#define AST_EVAL_SLICE_DOMAIN_CAP 4096

static int ast_eval_slice_returns(AstArena *a, AstLocal *out, int max) {
	int n = 0;
	AstLocal cnt = ast_count(a);
	for (AstLocal r = 0; r < cnt; r++) {
		if (ast_kind(a, r) != AST_Return || ast_nchild(a, r) != 1)
			continue;
		AstLocal v = ast_first_child(a, r);
		if (v != AST_NONE && n < max)
			out[n++] = v;
	}
	return n;
}

static int ast_eval_slice_env_ok(AstArena *base, AstArena *spec, const int32_t *off,
																 const int64_t *val, int nenv) {
	AstLocal brets[AST_EVAL_SLICE_MAXRET], srets[AST_EVAL_SLICE_MAXRET];
	int64_t bvals[AST_EVAL_SLICE_MAXRET];
	int nb = ast_eval_slice_returns(base, brets, AST_EVAL_SLICE_MAXRET);
	int ns = ast_eval_slice_returns(spec, srets, AST_EVAL_SLICE_MAXRET);
	int nbv = 0;
	for (int i = 0; i < nb; i++) {
		int64_t v;
		if (ast_eval_slice(base, brets[i], off, val, nenv, &v))
			bvals[nbv++] = v;
	}
	if (nbv == 0)
		return 1;
	for (int i = 0; i < ns; i++) {
		int64_t sv;
		if (!ast_eval_slice(spec, srets[i], off, val, nenv, &sv))
			continue;
		int found = 0;
		for (int j = 0; j < nbv; j++)
			if (bvals[j] == sv) {
				found = 1;
				break;
			}
		if (!found)
			return 0;
	}
	return 1;
}

static int ast_eval_slice_sound(AstArena *base, AstArena *spec, int mode,
																const int *offs, const int64_t *pvals,
																const int64_t *plos, const int64_t *phis,
																int npoff, int maxp) {
	if (npoff <= 0 || npoff > maxp || npoff > AST_EVAL_SLICE_MAXRET)
		return 1;
	int32_t soff[AST_EVAL_SLICE_MAXRET];
	int64_t sval[AST_EVAL_SLICE_MAXRET];
	for (int i = 0; i < npoff; i++)
		soff[i] = offs[i];
	if (mode == 4) {
		for (int i = 0; i < npoff; i++)
			sval[i] = pvals[i];
		return ast_eval_slice_env_ok(base, spec, soff, sval, npoff);
	}
	if (mode != 5)
		return 1;
	int64_t card = 1;
	for (int i = 0; i < npoff; i++) {
		if (plos[i] > phis[i])
			return 1;
		uint64_t w = (uint64_t)(phis[i] - plos[i]) + 1;
		if (w > (uint64_t)AST_EVAL_SLICE_DOMAIN_CAP)
			return 1;
		card *= (int64_t)w;
		if (card > AST_EVAL_SLICE_DOMAIN_CAP)
			return 1;
	}
	int k[AST_EVAL_SLICE_MAXRET];
	int64_t sp[AST_EVAL_SLICE_MAXRET][4];
	int64_t total = 1;
	for (int i = 0; i < npoff; i++) {
		int64_t lo = plos[i], hi = phis[i];
		int c = 0;
		sp[i][c++] = lo;
		if (hi != lo)
			sp[i][c++] = hi;
		if (hi - lo >= 2)
			sp[i][c++] = lo + (hi - lo) / 2;
		if (hi - lo >= 3)
			sp[i][c++] = lo + (hi - lo) / 3;
		k[i] = c;
		total *= c;
	}
	int lim = total < AST_EVAL_SLICE_SAMPLE_CAP ? (int)total : AST_EVAL_SLICE_SAMPLE_CAP;
	for (int s = 0; s < lim; s++) {
		int rem = s;
		for (int i = 0; i < npoff; i++) {
			int idx = rem % k[i];
			rem /= k[i];
			sval[i] = sp[i][idx];
		}
		if (!ast_eval_slice_env_ok(base, spec, soff, sval, npoff))
			return 0;
	}
	return 1;
}

#endif /* AST_EVAL_SLICE_KERNEL_ONLY */

#endif /* AST_EVAL_SLICE_PROVIDED */
