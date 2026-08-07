#ifndef MCC_MSL_PROVIDED
#define MCC_MSL_PROVIDED 1

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MSL_REALLOC
#define MSL_REALLOC realloc
#endif
#ifndef MSL_MALLOC
#define MSL_MALLOC malloc
#endif
#ifndef MSL_FREE
#define MSL_FREE free
#endif

#define MSL_LOCAL_SIZE 64
#define MSL_MAX_CONST 512
#define MSL_LINE_MAX 512

enum {
	MslOpAdd = 1, MslOpSub, MslOpMul, MslOpUDiv, MslOpSDiv, MslOpUMod,
	MslOpSRem, MslOpShl, MslOpShr, MslOpSar, MslOpAnd, MslOpOr, MslOpXor,
	MslOpEq, MslOpNe, MslOpULt, MslOpUGe, MslOpULe, MslOpUGt, MslOpSLt,
	MslOpSGe, MslOpSLe, MslOpSGt
};

typedef struct MslBuf {
	char *s;
	int n, cap;
} MslBuf;

typedef struct MslMod {
	MslBuf decls, body;
	uint32_t next_id;
	uint32_t def;
	int32_t cval[MSL_MAX_CONST];
	uint32_t cid[MSL_MAX_CONST];
	int ncached;
	int indent;
	int failed;
} MslMod;

static void mslb_need(MslBuf *b, int extra) {
	if (b->n + extra + 1 > b->cap) {
		int want = b->cap ? b->cap : 1024;
		while (want < b->n + extra + 1)
			want *= 2;
		b->s = (char *)MSL_REALLOC(b->s, (size_t)want);
		b->cap = want;
	}
}

static void mslb_puts(MslBuf *b, const char *s) {
	int len = (int)strlen(s);
	mslb_need(b, len);
	memcpy(b->s + b->n, s, (size_t)len);
	b->n += len;
	b->s[b->n] = 0;
}

static uint32_t msl_id(MslMod *m) { return m->next_id++; }

static void msl_indent(MslMod *m) {
	int i;
	for (i = 0; i < m->indent; i++)
		mslb_puts(&m->body, "\t");
}

static void msl_line(MslMod *m, const char *fmt, ...) {
	char tmp[MSL_LINE_MAX];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(tmp, sizeof tmp, fmt, ap);
	va_end(ap);
	msl_indent(m);
	mslb_puts(&m->body, tmp);
	mslb_puts(&m->body, "\n");
}

static uint32_t msl_iv(MslMod *m, const char *fmt, ...) {
	uint32_t id = msl_id(m);
	char tmp[MSL_LINE_MAX], out[MSL_LINE_MAX];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(tmp, sizeof tmp, fmt, ap);
	va_end(ap);
	snprintf(out, sizeof out, "int v%u = %s;", id, tmp);
	msl_indent(m);
	mslb_puts(&m->body, out);
	mslb_puts(&m->body, "\n");
	return id;
}

static uint32_t msl_bv(MslMod *m, const char *fmt, ...) {
	uint32_t id = msl_id(m);
	char tmp[MSL_LINE_MAX], out[MSL_LINE_MAX];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(tmp, sizeof tmp, fmt, ap);
	va_end(ap);
	snprintf(out, sizeof out, "bool b%u = %s;", id, tmp);
	msl_indent(m);
	mslb_puts(&m->body, out);
	mslb_puts(&m->body, "\n");
	return id;
}

static uint32_t msl_const(MslMod *m, int32_t v) {
	char tmp[MSL_LINE_MAX];
	uint32_t id;
	int i;
	for (i = 0; i < m->ncached; i++)
		if (m->cval[i] == v)
			return m->cid[i];
	if (m->ncached == MSL_MAX_CONST) {
		m->failed = 1;
		return m->cid[0];
	}
	id = msl_id(m);
	snprintf(tmp, sizeof tmp, "\tint v%u = as_type<int>(0x%08xu);\n", id,
					 (unsigned)v);
	mslb_puts(&m->decls, tmp);
	m->cval[m->ncached] = v;
	m->cid[m->ncached] = id;
	m->ncached++;
	return id;
}

static uint32_t msl_uconst(MslMod *m, uint32_t v) {
	return msl_const(m, (int32_t)v);
}

static uint32_t msl_true(MslMod *m) { return msl_bv(m, "true"); }

static void msl_def_and(MslMod *m, uint32_t *def, uint32_t ok) {
	*def = msl_bv(m, "b%u && b%u", *def, ok);
}

static uint32_t msl_not(MslMod *m, uint32_t b) { return msl_bv(m, "!b%u", b); }

static uint32_t msl_or(MslMod *m, uint32_t a, uint32_t b) {
	return msl_bv(m, "b%u || b%u", a, b);
}

static uint32_t msl_bool_of(MslMod *m, uint32_t v) {
	return msl_bv(m, "v%u != 0", v);
}

static uint32_t msl_int_of_bool(MslMod *m, uint32_t b) {
	return msl_iv(m, "b%u ? 1 : 0", b);
}

static uint32_t msl_load_live(MslMod *m, uint32_t base, int k) {
	if (k)
		return msl_iv(m, "inb[v%u + %d]", base, k);
	return msl_iv(m, "inb[v%u]", base);
}

static uint32_t msl_fit(MslMod *m, uint32_t v, int t) {
	int bt = t & VT_BTYPE;
	int uns = (t & VT_UNSIGNED) != 0;
	switch (bt) {
	case VT_BOOL:
		return msl_int_of_bool(m, msl_bool_of(m, v));
	case VT_BYTE:
		if (uns)
			return msl_iv(m, "v%u & 0xFF", v);
		return msl_iv(m, "mcc_sar(mcc_shl(v%u, 24), 24)", v);
	case VT_SHORT:
		if (uns)
			return msl_iv(m, "v%u & 0xFFFF", v);
		return msl_iv(m, "mcc_sar(mcc_shl(v%u, 16), 16)", v);
	default:
		return v;
	}
}

static uint32_t msl_main_begin(MslMod *m, int nlive) {
	uint32_t gi = msl_iv(m, "as_type<int>(gid)");
	uint32_t nl = msl_const(m, nlive);
	m->def = msl_true(m);
	return msl_iv(m, "mcc_mul(v%u, v%u)", gi, nl);
}

static uint32_t msl_lane(MslMod *m, uint32_t base, int nlive) {
	uint32_t nl = msl_const(m, nlive);
	return msl_iv(m, "mcc_sdiv(v%u, v%u)", base, nl);
}

static void msl_main_end(MslMod *m, uint32_t lane, uint32_t val) {
	uint32_t c2 = msl_const(m, 2);
	uint32_t c1 = msl_const(m, 1);
	uint32_t two = msl_iv(m, "mcc_mul(v%u, v%u)", lane, c2);
	uint32_t one = msl_iv(m, "mcc_add(v%u, v%u)", two, c1);
	uint32_t d = msl_int_of_bool(m, m->def);
	msl_line(m, "outb[v%u] = v%u;", two, val);
	msl_line(m, "outb[v%u] = v%u;", one, d);
}

static void msl_def_addsub(MslMod *m, uint32_t *def, int is_sub, uint32_t a,
													 uint32_t b, uint32_t r) {
	uint32_t x = msl_iv(m, "v%u ^ v%u", a, r);
	uint32_t y = is_sub ? msl_iv(m, "v%u ^ v%u", a, b)
										 : msl_iv(m, "v%u ^ v%u", b, r);
	uint32_t t = is_sub ? msl_iv(m, "v%u & v%u", y, x)
										 : msl_iv(m, "v%u & v%u", x, y);
	msl_def_and(m, def, msl_bv(m, "v%u >= 0", t));
}

static void msl_def_mul(MslMod *m, uint32_t *def, uint32_t a, uint32_t b,
												uint32_t r) {
	uint32_t lo = msl_iv(m, "mcc_mul(v%u, v%u)", a, b);
	uint32_t hi = msl_iv(m, "mulhi(v%u, v%u)", a, b);
	uint32_t sign = msl_iv(m, "mcc_sar(v%u, 31)", lo);
	msl_def_and(m, def, msl_bv(m, "v%u == v%u", hi, sign));
	(void)r;
}

static uint32_t msl_guard_div(MslMod *m, uint32_t *def, int uns, uint32_t a,
															uint32_t b) {
	uint32_t bad = msl_bv(m, "v%u == 0", b);
	if (!uns) {
		uint32_t amin =
				msl_bv(m, "v%u == v%u", a, msl_const(m, (int32_t)0x80000000));
		uint32_t bneg = msl_bv(m, "v%u == v%u", b, msl_const(m, -1));
		bad = msl_or(m, bad, msl_bv(m, "b%u && b%u", amin, bneg));
	}
	msl_def_and(m, def, msl_not(m, bad));
	return msl_iv(m, "b%u ? 1 : v%u", bad, b);
}

static uint32_t msl_guard_shift(MslMod *m, uint32_t *def, uint32_t b) {
	uint32_t lo = msl_bv(m, "v%u < 0", b);
	uint32_t hi = msl_bv(m, "v%u >= 32", b);
	uint32_t bad = msl_or(m, lo, hi);
	msl_def_and(m, def, msl_not(m, bad));
	return msl_iv(m, "b%u ? 0 : v%u", bad, b);
}

static uint32_t msl_signed_rem(MslMod *m, uint32_t a, uint32_t b) {
	uint32_t q = msl_iv(m, "mcc_sdiv(v%u, v%u)", a, b);
	uint32_t p = msl_iv(m, "mcc_mul(v%u, v%u)", b, q);
	return msl_iv(m, "mcc_sub(v%u, v%u)", a, p);
}

static uint32_t msl_arith(MslMod *m, int code, uint32_t a, uint32_t b) {
	switch (code) {
	case MslOpAdd: return msl_iv(m, "mcc_add(v%u, v%u)", a, b);
	case MslOpSub: return msl_iv(m, "mcc_sub(v%u, v%u)", a, b);
	case MslOpMul: return msl_iv(m, "mcc_mul(v%u, v%u)", a, b);
	case MslOpUDiv: return msl_iv(m, "mcc_udiv(v%u, v%u)", a, b);
	case MslOpSDiv: return msl_iv(m, "mcc_sdiv(v%u, v%u)", a, b);
	case MslOpUMod: return msl_iv(m, "mcc_umod(v%u, v%u)", a, b);
	case MslOpShl: return msl_iv(m, "mcc_shl(v%u, v%u)", a, b);
	case MslOpShr: return msl_iv(m, "mcc_shr(v%u, v%u)", a, b);
	case MslOpSar: return msl_iv(m, "mcc_sar(v%u, v%u)", a, b);
	case MslOpAnd: return msl_iv(m, "v%u & v%u", a, b);
	case MslOpOr: return msl_iv(m, "v%u | v%u", a, b);
	default: return msl_iv(m, "v%u ^ v%u", a, b);
	}
}

static uint32_t msl_cmp(MslMod *m, int code, uint32_t a, uint32_t b) {
	switch (code) {
	case MslOpEq: return msl_bv(m, "v%u == v%u", a, b);
	case MslOpNe: return msl_bv(m, "v%u != v%u", a, b);
	case MslOpULt:
		return msl_bv(m, "as_type<uint>(v%u) < as_type<uint>(v%u)", a, b);
	case MslOpUGe:
		return msl_bv(m, "as_type<uint>(v%u) >= as_type<uint>(v%u)", a, b);
	case MslOpULe:
		return msl_bv(m, "as_type<uint>(v%u) <= as_type<uint>(v%u)", a, b);
	case MslOpUGt:
		return msl_bv(m, "as_type<uint>(v%u) > as_type<uint>(v%u)", a, b);
	case MslOpSLt: return msl_bv(m, "v%u < v%u", a, b);
	case MslOpSGe: return msl_bv(m, "v%u >= v%u", a, b);
	case MslOpSLe: return msl_bv(m, "v%u <= v%u", a, b);
	default: return msl_bv(m, "v%u > v%u", a, b);
	}
}

static int msl_binop_code(int op, int uns, int *is_cmp) {
	*is_cmp = 0;
	switch (op) {
	case '+': return MslOpAdd;
	case '-': return MslOpSub;
	case '*': return MslOpMul;
	case '/': case TOK_PDIV: return uns ? MslOpUDiv : MslOpSDiv;
	case '%': return uns ? MslOpUMod : MslOpSRem;
	case TOK_UDIV: return MslOpUDiv;
	case TOK_UMOD: return MslOpUMod;
	case TOK_SHL: return MslOpShl;
	case TOK_SHR: return MslOpShr;
	case TOK_SAR: return MslOpSar;
	case '&': return MslOpAnd;
	case '|': return MslOpOr;
	case '^': return MslOpXor;
	default: break;
	}
	*is_cmp = 1;
	switch (op) {
	case TOK_EQ: return MslOpEq;
	case TOK_NE: return MslOpNe;
	case TOK_ULT: return MslOpULt;
	case TOK_UGE: return MslOpUGe;
	case TOK_ULE: return MslOpULe;
	case TOK_UGT: return MslOpUGt;
	case TOK_LE: return uns ? MslOpULe : MslOpSLe;
	case TOK_GE: return uns ? MslOpUGe : MslOpSGe;
	case TOK_LT: return uns ? MslOpULt : MslOpSLt;
	case TOK_GT: return uns ? MslOpUGt : MslOpSGt;
	default: break;
	}
	*is_cmp = -1;
	return 0;
}

static int msl_env_index(const int32_t *off, int nenv, int32_t want, int *out) {
	int i;
	for (i = 0; i < nenv; i++)
		if (off[i] == want) {
			*out = i;
			return 1;
		}
	return 0;
}

static int msl_expr(MslMod *m, AstArena *a, AstLocal n, const int32_t *off,
										int nenv, uint32_t base, uint32_t *out);

static int msl_branch_pair(MslMod *m, AstArena *a, AstLocal cn, AstLocal tn,
													 AstLocal en, const int32_t *off, int nenv,
													 uint32_t base, uint32_t *out) {
	uint32_t cv, cb, res, dres, def_in, tv, ev;
	if (!msl_expr(m, a, cn, off, nenv, base, &cv))
		return 0;
	cb = msl_bool_of(m, cv);
	def_in = m->def;
	res = msl_id(m);
	dres = msl_id(m);
	msl_line(m, "int v%u;", res);
	msl_line(m, "bool b%u;", dres);
	msl_line(m, "if (b%u) {", cb);
	m->indent++;
	m->def = def_in;
	if (!msl_expr(m, a, tn, off, nenv, base, &tv))
		return 0;
	msl_line(m, "v%u = v%u;", res, tv);
	msl_line(m, "b%u = b%u;", dres, m->def);
	m->indent--;
	msl_line(m, "} else {");
	m->indent++;
	m->def = def_in;
	if (!msl_expr(m, a, en, off, nenv, base, &ev))
		return 0;
	msl_line(m, "v%u = v%u;", res, ev);
	msl_line(m, "b%u = b%u;", dres, m->def);
	m->indent--;
	msl_line(m, "}");
	m->def = dres;
	*out = res;
	return 1;
}

static int msl_logical(MslMod *m, AstArena *a, AstLocal n, int want,
											 const int32_t *off, int nenv, uint32_t base,
											 uint32_t *out, uint32_t k) {
	uint32_t nc = ast_nchild(a, n);
	uint32_t cv, cb, res, dres, def_in, rest, stopv;
	if (k == nc) {
		*out = msl_const(m, want ? 1 : 0);
		return 1;
	}
	if (!msl_expr(m, a, ast_child(a, n, k), off, nenv, base, &cv))
		return 0;
	cb = msl_bool_of(m, cv);
	def_in = m->def;
	res = msl_id(m);
	dres = msl_id(m);
	msl_line(m, "int v%u;", res);
	msl_line(m, "bool b%u;", dres);
	msl_line(m, "if (%sb%u) {", want ? "" : "!", cb);
	m->indent++;
	m->def = def_in;
	if (!msl_logical(m, a, n, want, off, nenv, base, &rest, k + 1))
		return 0;
	msl_line(m, "v%u = v%u;", res, rest);
	msl_line(m, "b%u = b%u;", dres, m->def);
	m->indent--;
	msl_line(m, "} else {");
	m->indent++;
	m->def = def_in;
	stopv = msl_const(m, want ? 0 : 1);
	msl_line(m, "v%u = v%u;", res, stopv);
	msl_line(m, "b%u = b%u;", dres, m->def);
	m->indent--;
	msl_line(m, "}");
	m->def = dres;
	*out = res;
	return 1;
}

static int msl_expr(MslMod *m, AstArena *a, AstLocal n, const int32_t *off,
										int nenv, uint32_t base, uint32_t *out) {
	if (n == AST_NONE || m->failed)
		return 0;
	switch (ast_kind(a, n)) {
	case AST_Literal: {
		int t = ast_type_t(a, n);
		if (ast_bad_type(t) || is_float(t) || !ast_eval_slice_intt(t))
			return 0;
		if (ast_eval_slice_is64(t))
			return 0;
		if ((ast_op(a, n) & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
			return 0;
		*out = msl_const(m, (int32_t)ast_eval_slice_fit((int64_t)ast_ival(a, n), t));
		return 1;
	}
	case AST_Ref: {
		int r = ast_op(a, n);
		int t = ast_type_t(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) {
			int k;
			if (!ast_eval_slice_intt(t) || is_float(t) || ast_eval_slice_is64(t))
				return 0;
			if (!msl_env_index(off, nenv, (int32_t)(int64_t)ast_ival(a, n), &k))
				return 0;
			*out = msl_load_live(m, base, k);
			return 1;
		}
		if ((r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
			if (ast_bad_type(t) || is_float(t) || !ast_eval_slice_intt(t))
				return 0;
			if (ast_eval_slice_is64(t))
				return 0;
			*out =
					msl_const(m, (int32_t)ast_eval_slice_fit((int64_t)ast_ival(a, n), t));
			return 1;
		}
		return 0;
	}
	case AST_Load: {
		AstLocal c = ast_first_child(a, n);
		int r, t, k;
		if (c == AST_NONE || ast_kind(a, c) != AST_Ref)
			return 0;
		r = ast_op(a, c);
		t = ast_type_t(a, n);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
			return 0;
		if (!ast_eval_slice_intt(t) || is_float(t) || ast_eval_slice_is64(t))
			return 0;
		if (!msl_env_index(off, nenv, (int32_t)(int64_t)ast_ival(a, c), &k))
			return 0;
		*out = msl_load_live(m, base, k);
		return 1;
	}
	case AST_Convert: {
		int t = ast_type_t(a, n);
		AstLocal c = ast_first_child(a, n);
		uint32_t v;
		if (c == AST_NONE || is_float(t) || is_float(ast_type_t(a, c)))
			return 0;
		if (ast_bad_type(t) || !ast_eval_slice_intt(t) || ast_eval_slice_is64(t))
			return 0;
		if (!msl_expr(m, a, c, off, nenv, base, &v))
			return 0;
		*out = msl_fit(m, v, t);
		return 1;
	}
	case AST_Unary: {
		int uop = ast_op(a, n);
		int t = ast_eval_slice_wtype(a, n);
		AstLocal c = ast_first_child(a, n);
		uint32_t v;
		if (c == AST_NONE || !t || ast_eval_slice_is64(t))
			return 0;
		if (uop != '-' && uop != TOK_NEG && uop != '~' && uop != '!')
			return 0;
		if (!msl_expr(m, a, c, off, nenv, base, &v))
			return 0;
		if (uop == '!') {
			*out = msl_int_of_bool(m, msl_bv(m, "v%u == 0", v));
			return 1;
		}
		if (uop == '~') {
			*out = msl_fit(m, msl_iv(m, "~v%u", v), t);
			return 1;
		}
		{
			uint32_t z = msl_const(m, 0);
			uint32_t r = msl_iv(m, "mcc_sub(v%u, v%u)", z, v);
			if (!((t & VT_UNSIGNED) != 0))
				msl_def_addsub(m, &m->def, 1, z, v, r);
			*out = r;
		}
		return 1;
	}
	case AST_Binary: {
		int bop = ast_op(a, n);
		AstLocal x, y;
		int xt, uns, is_cmp, code;
		uint32_t lv, rv;
		if (bop == TOK_LAND || bop == TOK_LOR)
			return msl_logical(m, a, n, bop == TOK_LAND, off, nenv, base, out, 0);
		if (ast_nchild(a, n) != 2)
			return 0;
		x = ast_child(a, n, 0);
		y = ast_child(a, n, 1);
		xt = ast_eval_slice_wtype(a, x);
		if (!xt || is_float(ast_type_t(a, x)) || is_float(ast_type_t(a, y)))
			return 0;
		if (ast_eval_slice_is64(xt))
			return 0;
		if (!msl_expr(m, a, x, off, nenv, base, &lv))
			return 0;
		if (!msl_expr(m, a, y, off, nenv, base, &rv))
			return 0;
		uns = (xt & VT_UNSIGNED) != 0;
		code = msl_binop_code(bop, uns, &is_cmp);
		if (is_cmp < 0)
			return 0;
		if (is_cmp) {
			*out = msl_int_of_bool(m, msl_cmp(m, code, lv, rv));
			return 1;
		}
		if (code == MslOpUDiv || code == MslOpUMod) {
			uint32_t d = msl_guard_div(m, &m->def, 1, lv, rv);
			*out = msl_arith(m, code, lv, d);
			return 1;
		}
		if (code == MslOpSRem) {
			uint32_t d = msl_guard_div(m, &m->def, 0, lv, rv);
			*out = msl_signed_rem(m, lv, d);
			return 1;
		}
		if (code == MslOpSDiv) {
			uint32_t d = msl_guard_div(m, &m->def, 0, lv, rv);
			*out = msl_arith(m, code, lv, d);
			return 1;
		}
		if (code == MslOpShl || code == MslOpShr || code == MslOpSar) {
			uint32_t sh = msl_guard_shift(m, &m->def, rv);
			*out = msl_arith(m, code, lv, sh);
			return 1;
		}
		*out = msl_arith(m, code, lv, rv);
		if (!uns && code == MslOpAdd)
			msl_def_addsub(m, &m->def, 0, lv, rv, *out);
		else if (!uns && code == MslOpSub)
			msl_def_addsub(m, &m->def, 1, lv, rv, *out);
		else if (!uns && code == MslOpMul)
			msl_def_mul(m, &m->def, lv, rv, *out);
		return 1;
	}
	case AST_If: {
		if (ast_nchild(a, n) != 3)
			return 0;
		return msl_branch_pair(m, a, ast_child(a, n, 0), ast_child(a, n, 1),
													 ast_child(a, n, 2), off, nenv, base, out);
	}
	default:
		return 0;
	}
}

static const char msl_prelude[] =
		"#include <metal_stdlib>\n"
		"using namespace metal;\n"
		"\n"
		"static inline int mcc_add(int a, int b) {\n"
		"\treturn as_type<int>(as_type<uint>(a) + as_type<uint>(b));\n"
		"}\n"
		"static inline int mcc_sub(int a, int b) {\n"
		"\treturn as_type<int>(as_type<uint>(a) - as_type<uint>(b));\n"
		"}\n"
		"static inline int mcc_mul(int a, int b) {\n"
		"\treturn as_type<int>(as_type<uint>(a) * as_type<uint>(b));\n"
		"}\n"
		"static inline int mcc_shl(int a, int b) {\n"
		"\treturn as_type<int>(as_type<uint>(a) << as_type<uint>(b));\n"
		"}\n"
		"static inline int mcc_shr(int a, int b) {\n"
		"\treturn as_type<int>(as_type<uint>(a) >> as_type<uint>(b));\n"
		"}\n"
		"static inline int mcc_sar(int a, int b) { return a >> b; }\n"
		"static inline int mcc_sdiv(int a, int b) { return a / b; }\n"
		"static inline int mcc_udiv(int a, int b) {\n"
		"\treturn as_type<int>(as_type<uint>(a) / as_type<uint>(b));\n"
		"}\n"
		"static inline int mcc_umod(int a, int b) {\n"
		"\treturn as_type<int>(as_type<uint>(a) % as_type<uint>(b));\n"
		"}\n"
		"\n"
		"kernel void mcc_main(device const int *inb [[buffer(0)]],\n"
		"                     device int *outb [[buffer(1)]],\n"
		"                     uint gid [[thread_position_in_grid]]) {\n";

static void msl_module_begin(MslMod *m, int nlive) {
	memset(m, 0, sizeof *m);
	m->next_id = 1;
	m->indent = 1;
	(void)nlive;
}

static char *msl_module_finish(MslMod *m, int *nbytes) {
	int pre = (int)(sizeof msl_prelude - 1);
	int total = pre + m->decls.n + m->body.n + 2;
	char *s = (char *)MSL_MALLOC((size_t)total + 1);
	int i = 0;
	memcpy(s + i, msl_prelude, (size_t)pre);
	i += pre;
	if (m->decls.n) {
		memcpy(s + i, m->decls.s, (size_t)m->decls.n);
		i += m->decls.n;
	}
	if (m->body.n) {
		memcpy(s + i, m->body.s, (size_t)m->body.n);
		i += m->body.n;
	}
	s[i++] = '}';
	s[i++] = '\n';
	s[i] = 0;
	*nbytes = i;
	return s;
}

static void msl_module_free(MslMod *m) {
	MSL_FREE(m->decls.s);
	MSL_FREE(m->body.s);
}

#endif
