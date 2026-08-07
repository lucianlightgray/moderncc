#ifndef MCC_GPU_H
#define MCC_GPU_H

/* The width ladder's GPU oracle, in two halves.
 *
 *   - The shader emitters, below, turn an arena subtree plus its live-in slot
 *     offsets into a compute kernel: SPIR-V on every host but Darwin, Metal
 *     Shading Language there.  They are header-only because they need the AST
 *     accessors and the ast_eval_slice width helpers already in the includer's
 *     scope, and because tools/spvgate.c wants the SPIR-V half on its own.
 *   - The device layer, src/mccgpu.c, dlopens Vulkan or Metal at first use and
 *     dispatches that kernel.  It touches no AST at all; `code` reaches it as
 *     bytes.
 *
 * Define MCC_GPU_EMITTER before including to get the emitter for this host's
 * shading language, or MCC_GPU_ORACLE to get the MccGpuCode glue that feeds
 * the device layer as well.  Either one requires mcc.h, mccast.h and
 * ast_eval_slice.h to have been included first; the declarations below need
 * none of that.  MCC_GPU_MALLOC/REALLOC/FREE override the emitters'
 * allocator, and MCC_GPU_LANG_MSL overrides the shading language.
 *
 * The Vulkan ABI subset the device layer binds is transcribed from the Khronos
 * Vulkan-Headers project; see src/mccgpu.LICENSE for provenance and license.
 */

/* stdio.h before mcchost.h, not after: mcchost.h declares host_fopen in terms
 * of FILE, and on MSVC it macro-defines vsnprintf, which <stdio.h> rejects if
 * it gets there second. */
#include <stdint.h>
#include <stdio.h>

#include "mcchost.h"

#ifndef MCC_GPU_LANG_MSL
#define MCC_GPU_LANG_MSL MCC_HOST_DARWIN
#endif

#define MCC_GPU_LOCAL_SIZE 64

#if MCC_GPU_LANG_MSL
#define MCC_GPU_CODE_MAX 65536
#define MCC_GPU_CODE_SUFFIX "metal"
#define MCC_GPU_CODE_UNIT 1
#else
#define MCC_GPU_CODE_MAX 8192
#define MCC_GPU_CODE_SUFFIX "spv"
#define MCC_GPU_CODE_UNIT 4
#endif

typedef struct MccGpuCode {
	void *p;
	int n;
} MccGpuCode;

typedef struct MccGpuStats {
	int tried;
	int ok;
	const char *name;
	long dispatches;
	long lanes;
} MccGpuStats;

/* `n` counts MCC_GPU_CODE_UNIT-sized units of `code`. */
int mcc_gpu_dispatch(const void *code, int n, const int32_t *in, int ntuple,
										 int nlive, int32_t *out);
void mcc_gpu_quiesce(void);
void mcc_gpu_stats(MccGpuStats *out);

#endif /* MCC_GPU_H */

/* The two sections below carry their own guards rather than riding on
 * MCC_GPU_H: in an amalgamated build mccgpu.c and mccast.c include this header
 * asking for different halves of it, in whichever order libmcc.c lists them. */

#if defined(MCC_GPU_ORACLE) && !defined(MCC_GPU_EMITTER)
#define MCC_GPU_EMITTER 1
#endif

#if defined(MCC_GPU_EMITTER) && !defined(MCC_GPU_EMITTER_PROVIDED)
#define MCC_GPU_EMITTER_PROVIDED 1

#include <stdlib.h>
#include <string.h>

#ifndef MCC_GPU_MALLOC
#define MCC_GPU_MALLOC malloc
#endif
#ifndef MCC_GPU_REALLOC
#define MCC_GPU_REALLOC realloc
#endif
#ifndef MCC_GPU_FREE
#define MCC_GPU_FREE free
#endif

#if MCC_GPU_LANG_MSL

#include <stdarg.h>
#include <stdio.h>

#define MSL_MALLOC MCC_GPU_MALLOC
#define MSL_REALLOC MCC_GPU_REALLOC
#define MSL_FREE MCC_GPU_FREE

#define MSL_LOCAL_SIZE MCC_GPU_LOCAL_SIZE
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

#else /* !MCC_GPU_LANG_MSL */

#define SPV_MALLOC MCC_GPU_MALLOC
#define SPV_REALLOC MCC_GPU_REALLOC
#define SPV_FREE MCC_GPU_FREE

#define SPV_MAGIC 0x07230203u
#define SPV_VERSION 0x00010300u

enum {
	SpvOpEntryPoint = 15, SpvOpExecutionMode = 16, SpvOpCapability = 17,
	SpvOpMemoryModel = 14, SpvOpTypeVoid = 19, SpvOpTypeBool = 20,
	SpvOpTypeInt = 21, SpvOpTypeVector = 23, SpvOpTypeRuntimeArray = 29,
	SpvOpTypeStruct = 30, SpvOpTypePointer = 32, SpvOpTypeFunction = 33,
	SpvOpConstant = 43, SpvOpFunction = 54, SpvOpFunctionEnd = 56,
	SpvOpVariable = 59, SpvOpLoad = 61, SpvOpStore = 62, SpvOpAccessChain = 65,
	SpvOpDecorate = 71, SpvOpMemberDecorate = 72, SpvOpCompositeExtract = 81,
	SpvOpSNegate = 126, SpvOpIAdd = 128, SpvOpISub = 130, SpvOpIMul = 132,
	SpvOpUDiv = 134, SpvOpSDiv = 135, SpvOpUMod = 137, SpvOpSRem = 138,
	SpvOpSelect = 169, SpvOpIEqual = 170, SpvOpINotEqual = 171,
	SpvOpUGreaterThan = 172, SpvOpSGreaterThan = 173,
	SpvOpUGreaterThanEqual = 174, SpvOpSGreaterThanEqual = 175,
	SpvOpULessThan = 176, SpvOpSLessThan = 177, SpvOpULessThanEqual = 178,
	SpvOpSLessThanEqual = 179, SpvOpShiftRightLogical = 194,
	SpvOpShiftRightArithmetic = 195, SpvOpShiftLeftLogical = 196,
	SpvOpBitwiseOr = 197, SpvOpBitwiseXor = 198, SpvOpBitwiseAnd = 199,
	SpvOpBitcast = 124, SpvOpSMulExtended = 152,
	SpvOpLogicalOr = 166, SpvOpLogicalAnd = 167, SpvOpLogicalNot = 168,
	SpvOpNot = 200, SpvOpPhi = 245, SpvOpSelectionMerge = 247, SpvOpLabel = 248,
	SpvOpBranch = 249, SpvOpBranchConditional = 250, SpvOpReturn = 253
};

enum {
	SpvDecBlock = 2, SpvDecArrayStride = 6, SpvDecBuiltIn = 11,
	SpvDecBinding = 33, SpvDecDescriptorSet = 34, SpvDecOffset = 35
};

enum { SpvStorageInput = 1, SpvStorageStorageBuffer = 12, SpvStorageFunction = 7 };
enum { SpvBuiltInGlobalInvocationId = 28 };
enum { SpvExecModelGLCompute = 5, SpvExecModeLocalSize = 17 };
enum { SpvCapShader = 1 };

#define SPV_LOCAL_SIZE MCC_GPU_LOCAL_SIZE
#define SPV_MAX_CONST 512

typedef struct SpvWords {
	uint32_t *w;
	int n, cap;
} SpvWords;

typedef struct SpvMod {
	SpvWords pre, types, body;
	uint32_t next_id;
	uint32_t id_void, id_fnvoid, id_bool, id_int, id_uint, id_v3uint;
	uint32_t id_ptr_in_v3uint, id_gid;
	uint32_t id_rt, id_buf, id_ptr_buf, id_ptr_sb_int, id_pair;
	uint32_t id_in, id_out, id_main, id_nlive;
	uint32_t cur_label;
	uint32_t def;
	int32_t cval[SPV_MAX_CONST];
	uint32_t cid[SPV_MAX_CONST];
	int ncached;
	int failed;
} SpvMod;

static void spvw_put(SpvWords *b, uint32_t v) {
	if (b->n == b->cap) {
		b->cap = b->cap ? b->cap * 2 : 256;
		b->w = (uint32_t *)SPV_REALLOC(b->w, (size_t)b->cap * sizeof *b->w);
	}
	b->w[b->n++] = v;
}

static void spvw_op(SpvWords *b, int opcode, int nwords) {
	spvw_put(b, ((uint32_t)nwords << 16) | (uint32_t)opcode);
}

static void spvw_str(SpvWords *b, const char *s) {
	size_t len = strlen(s), i = 0;
	uint32_t acc = 0;
	int k = 0;
	for (i = 0; i <= len; i++) {
		acc |= (uint32_t)(unsigned char)s[i] << (8 * k);
		if (++k == 4) {
			spvw_put(b, acc);
			acc = 0;
			k = 0;
		}
	}
	if (k)
		spvw_put(b, acc);
}

static int spv_str_words(const char *s) {
	return (int)(strlen(s) / 4 + 1);
}

static uint32_t spv_id(SpvMod *m) { return m->next_id++; }

static uint32_t spv_const(SpvMod *m, int32_t v) {
	int i;
	for (i = 0; i < m->ncached; i++)
		if (m->cval[i] == v)
			return m->cid[i];
	if (m->ncached == SPV_MAX_CONST) {
		m->failed = 1;
		return m->cid[0];
	}
	uint32_t id = spv_id(m);
	spvw_op(&m->types, SpvOpConstant, 4);
	spvw_put(&m->types, m->id_int);
	spvw_put(&m->types, id);
	spvw_put(&m->types, (uint32_t)v);
	m->cval[m->ncached] = v;
	m->cid[m->ncached] = id;
	m->ncached++;
	return id;
}

static uint32_t spv_uconst(SpvMod *m, uint32_t v) {
	return spv_const(m, (int32_t)v);
}

static uint32_t spv_emit2(SpvMod *m, int opcode, uint32_t rtype, uint32_t a) {
	uint32_t id = spv_id(m);
	spvw_op(&m->body, opcode, 4);
	spvw_put(&m->body, rtype);
	spvw_put(&m->body, id);
	spvw_put(&m->body, a);
	return id;
}

static uint32_t spv_emit3(SpvMod *m, int opcode, uint32_t rtype, uint32_t a,
													uint32_t b) {
	uint32_t id = spv_id(m);
	spvw_op(&m->body, opcode, 5);
	spvw_put(&m->body, rtype);
	spvw_put(&m->body, id);
	spvw_put(&m->body, a);
	spvw_put(&m->body, b);
	return id;
}

static uint32_t spv_label(SpvMod *m) {
	uint32_t id = spv_id(m);
	spvw_op(&m->body, SpvOpLabel, 2);
	spvw_put(&m->body, id);
	m->cur_label = id;
	return id;
}

static void spv_label_at(SpvMod *m, uint32_t id) {
	spvw_op(&m->body, SpvOpLabel, 2);
	spvw_put(&m->body, id);
	m->cur_label = id;
}

static uint32_t spv_emit4(SpvMod *m, int opcode, uint32_t rtype, uint32_t a,
													uint32_t b, uint32_t c) {
	uint32_t id = spv_id(m);
	spvw_op(&m->body, opcode, 6);
	spvw_put(&m->body, rtype);
	spvw_put(&m->body, id);
	spvw_put(&m->body, a);
	spvw_put(&m->body, b);
	spvw_put(&m->body, c);
	return id;
}

static uint32_t spv_true(SpvMod *m) {
	return spv_emit3(m, SpvOpIEqual, m->id_bool, spv_const(m, 0),
									 spv_const(m, 0));
}

static void spv_def_and(SpvMod *m, uint32_t *def, uint32_t ok) {
	*def = spv_emit3(m, SpvOpLogicalAnd, m->id_bool, *def, ok);
}

static uint32_t spv_not(SpvMod *m, uint32_t b) {
	return spv_emit2(m, SpvOpLogicalNot, m->id_bool, b);
}

static uint32_t spv_or(SpvMod *m, uint32_t a, uint32_t b) {
	return spv_emit3(m, SpvOpLogicalOr, m->id_bool, a, b);
}

static uint32_t spv_bool_of(SpvMod *m, uint32_t v) {
	return spv_emit3(m, SpvOpINotEqual, m->id_bool, v, spv_const(m, 0));
}

static uint32_t spv_int_of_bool(SpvMod *m, uint32_t b) {
	uint32_t id = spv_id(m);
	spvw_op(&m->body, SpvOpSelect, 6);
	spvw_put(&m->body, m->id_int);
	spvw_put(&m->body, id);
	spvw_put(&m->body, b);
	spvw_put(&m->body, spv_const(m, 1));
	spvw_put(&m->body, spv_const(m, 0));
	return id;
}

static void spv_module_begin(SpvMod *m, int nlive) {
	memset(m, 0, sizeof *m);
	m->next_id = 1;
	m->id_void = spv_id(m);
	m->id_fnvoid = spv_id(m);
	m->id_bool = spv_id(m);
	m->id_int = spv_id(m);
	m->id_uint = spv_id(m);
	m->id_v3uint = spv_id(m);
	m->id_ptr_in_v3uint = spv_id(m);
	m->id_gid = spv_id(m);
	m->id_rt = spv_id(m);
	m->id_buf = spv_id(m);
	m->id_ptr_buf = spv_id(m);
	m->id_ptr_sb_int = spv_id(m);
	m->id_pair = spv_id(m);
	m->id_in = spv_id(m);
	m->id_out = spv_id(m);
	m->id_main = spv_id(m);
	m->id_nlive = (uint32_t)nlive;

	spvw_op(&m->pre, SpvOpCapability, 2);
	spvw_put(&m->pre, SpvCapShader);
	spvw_op(&m->pre, SpvOpMemoryModel, 3);
	spvw_put(&m->pre, 0);
	spvw_put(&m->pre, 1);
	spvw_op(&m->pre, SpvOpEntryPoint, 4 + spv_str_words("main"));
	spvw_put(&m->pre, SpvExecModelGLCompute);
	spvw_put(&m->pre, m->id_main);
	spvw_str(&m->pre, "main");
	spvw_put(&m->pre, m->id_gid);
	spvw_op(&m->pre, SpvOpExecutionMode, 6);
	spvw_put(&m->pre, m->id_main);
	spvw_put(&m->pre, SpvExecModeLocalSize);
	spvw_put(&m->pre, SPV_LOCAL_SIZE);
	spvw_put(&m->pre, 1);
	spvw_put(&m->pre, 1);

	spvw_op(&m->pre, SpvOpDecorate, 4);
	spvw_put(&m->pre, m->id_gid);
	spvw_put(&m->pre, SpvDecBuiltIn);
	spvw_put(&m->pre, SpvBuiltInGlobalInvocationId);
	spvw_op(&m->pre, SpvOpDecorate, 4);
	spvw_put(&m->pre, m->id_rt);
	spvw_put(&m->pre, SpvDecArrayStride);
	spvw_put(&m->pre, 4);
	spvw_op(&m->pre, SpvOpDecorate, 3);
	spvw_put(&m->pre, m->id_buf);
	spvw_put(&m->pre, SpvDecBlock);
	spvw_op(&m->pre, SpvOpMemberDecorate, 5);
	spvw_put(&m->pre, m->id_buf);
	spvw_put(&m->pre, 0);
	spvw_put(&m->pre, SpvDecOffset);
	spvw_put(&m->pre, 0);
	spvw_op(&m->pre, SpvOpDecorate, 4);
	spvw_put(&m->pre, m->id_in);
	spvw_put(&m->pre, SpvDecDescriptorSet);
	spvw_put(&m->pre, 0);
	spvw_op(&m->pre, SpvOpDecorate, 4);
	spvw_put(&m->pre, m->id_in);
	spvw_put(&m->pre, SpvDecBinding);
	spvw_put(&m->pre, 0);
	spvw_op(&m->pre, SpvOpDecorate, 4);
	spvw_put(&m->pre, m->id_out);
	spvw_put(&m->pre, SpvDecDescriptorSet);
	spvw_put(&m->pre, 0);
	spvw_op(&m->pre, SpvOpDecorate, 4);
	spvw_put(&m->pre, m->id_out);
	spvw_put(&m->pre, SpvDecBinding);
	spvw_put(&m->pre, 1);

	spvw_op(&m->types, SpvOpTypeVoid, 2);
	spvw_put(&m->types, m->id_void);
	spvw_op(&m->types, SpvOpTypeFunction, 3);
	spvw_put(&m->types, m->id_fnvoid);
	spvw_put(&m->types, m->id_void);
	spvw_op(&m->types, SpvOpTypeBool, 2);
	spvw_put(&m->types, m->id_bool);
	spvw_op(&m->types, SpvOpTypeInt, 4);
	spvw_put(&m->types, m->id_int);
	spvw_put(&m->types, 32);
	spvw_put(&m->types, 1);
	spvw_op(&m->types, SpvOpTypeInt, 4);
	spvw_put(&m->types, m->id_uint);
	spvw_put(&m->types, 32);
	spvw_put(&m->types, 0);
	spvw_op(&m->types, SpvOpTypeVector, 4);
	spvw_put(&m->types, m->id_v3uint);
	spvw_put(&m->types, m->id_uint);
	spvw_put(&m->types, 3);
	spvw_op(&m->types, SpvOpTypePointer, 4);
	spvw_put(&m->types, m->id_ptr_in_v3uint);
	spvw_put(&m->types, SpvStorageInput);
	spvw_put(&m->types, m->id_v3uint);
	spvw_op(&m->types, SpvOpVariable, 4);
	spvw_put(&m->types, m->id_ptr_in_v3uint);
	spvw_put(&m->types, m->id_gid);
	spvw_put(&m->types, SpvStorageInput);
	spvw_op(&m->types, SpvOpTypeRuntimeArray, 3);
	spvw_put(&m->types, m->id_rt);
	spvw_put(&m->types, m->id_int);
	spvw_op(&m->types, SpvOpTypeStruct, 3);
	spvw_put(&m->types, m->id_buf);
	spvw_put(&m->types, m->id_rt);
	spvw_op(&m->types, SpvOpTypePointer, 4);
	spvw_put(&m->types, m->id_ptr_buf);
	spvw_put(&m->types, SpvStorageStorageBuffer);
	spvw_put(&m->types, m->id_buf);
	spvw_op(&m->types, SpvOpVariable, 4);
	spvw_put(&m->types, m->id_ptr_buf);
	spvw_put(&m->types, m->id_in);
	spvw_put(&m->types, SpvStorageStorageBuffer);
	spvw_op(&m->types, SpvOpVariable, 4);
	spvw_put(&m->types, m->id_ptr_buf);
	spvw_put(&m->types, m->id_out);
	spvw_put(&m->types, SpvStorageStorageBuffer);
	spvw_op(&m->types, SpvOpTypePointer, 4);
	spvw_put(&m->types, m->id_ptr_sb_int);
	spvw_put(&m->types, SpvStorageStorageBuffer);
	spvw_put(&m->types, m->id_int);
	spvw_op(&m->types, SpvOpTypeStruct, 4);
	spvw_put(&m->types, m->id_pair);
	spvw_put(&m->types, m->id_int);
	spvw_put(&m->types, m->id_int);
}

static uint32_t spv_load_live(SpvMod *m, uint32_t base, int k) {
	uint32_t idx = base;
	if (k) {
		idx = spv_emit3(m, SpvOpIAdd, m->id_int, base, spv_const(m, k));
	}
	uint32_t p = spv_id(m);
	spvw_op(&m->body, SpvOpAccessChain, 5 + 1);
	spvw_put(&m->body, m->id_ptr_sb_int);
	spvw_put(&m->body, p);
	spvw_put(&m->body, m->id_in);
	spvw_put(&m->body, spv_const(m, 0));
	spvw_put(&m->body, idx);
	return spv_emit2(m, SpvOpLoad, m->id_int, p);
}

static uint32_t spv_fit(SpvMod *m, uint32_t v, int t) {
	int bt = t & VT_BTYPE;
	int uns = (t & VT_UNSIGNED) != 0;
	switch (bt) {
	case VT_BOOL:
		return spv_int_of_bool(m, spv_bool_of(m, v));
	case VT_BYTE:
		if (uns)
			return spv_emit3(m, SpvOpBitwiseAnd, m->id_int, v, spv_const(m, 0xFF));
		{
			uint32_t s = spv_emit3(m, SpvOpShiftLeftLogical, m->id_int, v,
														 spv_uconst(m, 24));
			return spv_emit3(m, SpvOpShiftRightArithmetic, m->id_int, s,
											 spv_uconst(m, 24));
		}
	case VT_SHORT:
		if (uns)
			return spv_emit3(m, SpvOpBitwiseAnd, m->id_int, v, spv_const(m, 0xFFFF));
		{
			uint32_t s = spv_emit3(m, SpvOpShiftLeftLogical, m->id_int, v,
														 spv_uconst(m, 16));
			return spv_emit3(m, SpvOpShiftRightArithmetic, m->id_int, s,
											 spv_uconst(m, 16));
		}
	default:
		return v;
	}
}

static uint32_t spv_main_begin(SpvMod *m, int nlive) {
	spvw_op(&m->body, SpvOpFunction, 5);
	spvw_put(&m->body, m->id_void);
	spvw_put(&m->body, m->id_main);
	spvw_put(&m->body, 0);
	spvw_put(&m->body, m->id_fnvoid);
	spv_label(m);
	uint32_t g = spv_emit2(m, SpvOpLoad, m->id_v3uint, m->id_gid);
	uint32_t gx = spv_id(m);
	spvw_op(&m->body, SpvOpCompositeExtract, 5);
	spvw_put(&m->body, m->id_uint);
	spvw_put(&m->body, gx);
	spvw_put(&m->body, g);
	spvw_put(&m->body, 0);
	uint32_t gi = spv_emit2(m, SpvOpBitcast, m->id_int, gx);
	m->def = spv_true(m);
	return spv_emit3(m, SpvOpIMul, m->id_int, gi, spv_const(m, nlive));
}

static void spv_store_at(SpvMod *m, uint32_t idx, uint32_t val) {
	uint32_t p = spv_id(m);
	spvw_op(&m->body, SpvOpAccessChain, 6);
	spvw_put(&m->body, m->id_ptr_sb_int);
	spvw_put(&m->body, p);
	spvw_put(&m->body, m->id_out);
	spvw_put(&m->body, spv_const(m, 0));
	spvw_put(&m->body, idx);
	spvw_op(&m->body, SpvOpStore, 3);
	spvw_put(&m->body, p);
	spvw_put(&m->body, val);
}

static void spv_main_end(SpvMod *m, uint32_t lane, uint32_t val) {
	uint32_t two = spv_emit3(m, SpvOpIMul, m->id_int, lane, spv_const(m, 2));
	uint32_t one = spv_emit3(m, SpvOpIAdd, m->id_int, two, spv_const(m, 1));
	spv_store_at(m, two, val);
	spv_store_at(m, one, spv_int_of_bool(m, m->def));
	spvw_op(&m->body, SpvOpReturn, 1);
	spvw_op(&m->body, SpvOpFunctionEnd, 1);
}

static uint32_t spv_unsigned_binop(SpvMod *m, int opcode, uint32_t a,
																	 uint32_t b) {
	uint32_t ua = spv_emit2(m, SpvOpBitcast, m->id_uint, a);
	uint32_t ub = spv_emit2(m, SpvOpBitcast, m->id_uint, b);
	uint32_t r = spv_emit3(m, opcode, m->id_uint, ua, ub);
	return spv_emit2(m, SpvOpBitcast, m->id_int, r);
}

static void spv_def_addsub(SpvMod *m, uint32_t *def, int is_sub, uint32_t a,
													 uint32_t b, uint32_t r) {
	uint32_t x = spv_emit3(m, SpvOpBitwiseXor, m->id_int, a, r);
	uint32_t y = is_sub ? spv_emit3(m, SpvOpBitwiseXor, m->id_int, a, b)
											: spv_emit3(m, SpvOpBitwiseXor, m->id_int, b, r);
	uint32_t t = is_sub ? spv_emit3(m, SpvOpBitwiseAnd, m->id_int, y, x)
											: spv_emit3(m, SpvOpBitwiseAnd, m->id_int, x, y);
	spv_def_and(m, def,
							spv_emit3(m, SpvOpSGreaterThanEqual, m->id_bool, t,
												spv_const(m, 0)));
}

static void spv_def_mul(SpvMod *m, uint32_t *def, uint32_t a, uint32_t b,
												uint32_t r) {
	uint32_t wide = spv_emit3(m, SpvOpSMulExtended, m->id_pair, a, b);
	uint32_t lo = spv_id(m), hi = spv_id(m), sign;
	spvw_op(&m->body, SpvOpCompositeExtract, 5);
	spvw_put(&m->body, m->id_int);
	spvw_put(&m->body, lo);
	spvw_put(&m->body, wide);
	spvw_put(&m->body, 0);
	spvw_op(&m->body, SpvOpCompositeExtract, 5);
	spvw_put(&m->body, m->id_int);
	spvw_put(&m->body, hi);
	spvw_put(&m->body, wide);
	spvw_put(&m->body, 1);
	sign = spv_emit3(m, SpvOpShiftRightArithmetic, m->id_int, lo,
									 spv_uconst(m, 31));
	spv_def_and(m, def, spv_emit3(m, SpvOpIEqual, m->id_bool, hi, sign));
	(void)r;
}

static uint32_t spv_guard_div(SpvMod *m, uint32_t *def, int uns, uint32_t a,
															uint32_t b) {
	uint32_t bad = spv_emit3(m, SpvOpIEqual, m->id_bool, b, spv_const(m, 0));
	if (!uns) {
		uint32_t amin = spv_emit3(m, SpvOpIEqual, m->id_bool, a,
															spv_const(m, (int32_t)0x80000000));
		uint32_t bneg =
				spv_emit3(m, SpvOpIEqual, m->id_bool, b, spv_const(m, -1));
		bad = spv_or(m, bad,
								 spv_emit3(m, SpvOpLogicalAnd, m->id_bool, amin, bneg));
	}
	spv_def_and(m, def, spv_not(m, bad));
	return spv_emit4(m, SpvOpSelect, m->id_int, bad, spv_const(m, 1), b);
}

static uint32_t spv_guard_shift(SpvMod *m, uint32_t *def, uint32_t b) {
	uint32_t lo = spv_emit3(m, SpvOpSLessThan, m->id_bool, b, spv_const(m, 0));
	uint32_t hi =
			spv_emit3(m, SpvOpSGreaterThanEqual, m->id_bool, b, spv_const(m, 32));
	uint32_t bad = spv_or(m, lo, hi);
	spv_def_and(m, def, spv_not(m, bad));
	return spv_emit4(m, SpvOpSelect, m->id_int, bad, spv_const(m, 0), b);
}

static uint32_t spv_signed_rem(SpvMod *m, uint32_t a, uint32_t b) {
	uint32_t q = spv_emit3(m, SpvOpSDiv, m->id_int, a, b);
	uint32_t p = spv_emit3(m, SpvOpIMul, m->id_int, b, q);
	return spv_emit3(m, SpvOpISub, m->id_int, a, p);
}

static int spv_binop_code(int op, int uns, int *is_cmp) {
	*is_cmp = 0;
	switch (op) {
	case '+': return SpvOpIAdd;
	case '-': return SpvOpISub;
	case '*': return SpvOpIMul;
	case '/': case TOK_PDIV: return uns ? SpvOpUDiv : SpvOpSDiv;
	case '%': return uns ? SpvOpUMod : SpvOpSRem;
	case TOK_UDIV: return SpvOpUDiv;
	case TOK_UMOD: return SpvOpUMod;
	case TOK_SHL: return SpvOpShiftLeftLogical;
	case TOK_SHR: return SpvOpShiftRightLogical;
	case TOK_SAR: return SpvOpShiftRightArithmetic;
	case '&': return SpvOpBitwiseAnd;
	case '|': return SpvOpBitwiseOr;
	case '^': return SpvOpBitwiseXor;
	default: break;
	}
	*is_cmp = 1;
	switch (op) {
	case TOK_EQ: return SpvOpIEqual;
	case TOK_NE: return SpvOpINotEqual;
	case TOK_ULT: return SpvOpULessThan;
	case TOK_UGE: return SpvOpUGreaterThanEqual;
	case TOK_ULE: return SpvOpULessThanEqual;
	case TOK_UGT: return SpvOpUGreaterThan;
	case TOK_LE: return uns ? SpvOpULessThanEqual : SpvOpSLessThanEqual;
	case TOK_GE: return uns ? SpvOpUGreaterThanEqual : SpvOpSGreaterThanEqual;
	case TOK_LT: return uns ? SpvOpULessThan : SpvOpSLessThan;
	case TOK_GT: return uns ? SpvOpUGreaterThan : SpvOpSGreaterThan;
	default: break;
	}
	*is_cmp = -1;
	return 0;
}

static int spv_env_index(const int32_t *off, int nenv, int32_t want, int *out) {
	int i;
	for (i = 0; i < nenv; i++)
		if (off[i] == want) {
			*out = i;
			return 1;
		}
	return 0;
}

static int spv_expr(SpvMod *m, AstArena *a, AstLocal n, const int32_t *off,
										int nenv, uint32_t base, uint32_t *out);

static int spv_branch_pair(SpvMod *m, AstArena *a, AstLocal cn, AstLocal tn,
													 AstLocal en, const int32_t *off, int nenv,
													 uint32_t base, uint32_t *out) {
	uint32_t cv;
	if (!spv_expr(m, a, cn, off, nenv, base, &cv))
		return 0;
	uint32_t cb = spv_bool_of(m, cv);
	uint32_t l_then = spv_id(m), l_else = spv_id(m), l_merge = spv_id(m);
	spvw_op(&m->body, SpvOpSelectionMerge, 3);
	spvw_put(&m->body, l_merge);
	spvw_put(&m->body, 0);
	spvw_op(&m->body, SpvOpBranchConditional, 4);
	spvw_put(&m->body, cb);
	spvw_put(&m->body, l_then);
	spvw_put(&m->body, l_else);

	uint32_t def_in = m->def;
	spv_label_at(m, l_then);
	m->def = def_in;
	uint32_t tv;
	if (!spv_expr(m, a, tn, off, nenv, base, &tv))
		return 0;
	uint32_t def_then = m->def;
	uint32_t from_then = m->cur_label;
	spvw_op(&m->body, SpvOpBranch, 2);
	spvw_put(&m->body, l_merge);

	spv_label_at(m, l_else);
	m->def = def_in;
	uint32_t ev;
	if (!spv_expr(m, a, en, off, nenv, base, &ev))
		return 0;
	uint32_t def_else = m->def;
	uint32_t from_else = m->cur_label;
	spvw_op(&m->body, SpvOpBranch, 2);
	spvw_put(&m->body, l_merge);

	spv_label_at(m, l_merge);
	uint32_t phi = spv_id(m);
	spvw_op(&m->body, SpvOpPhi, 7);
	spvw_put(&m->body, m->id_int);
	spvw_put(&m->body, phi);
	spvw_put(&m->body, tv);
	spvw_put(&m->body, from_then);
	spvw_put(&m->body, ev);
	spvw_put(&m->body, from_else);
	uint32_t dphi = spv_id(m);
	spvw_op(&m->body, SpvOpPhi, 7);
	spvw_put(&m->body, m->id_bool);
	spvw_put(&m->body, dphi);
	spvw_put(&m->body, def_then);
	spvw_put(&m->body, from_then);
	spvw_put(&m->body, def_else);
	spvw_put(&m->body, from_else);
	m->def = dphi;
	*out = phi;
	return 1;
}

static int spv_logical(SpvMod *m, AstArena *a, AstLocal n, int want,
											 const int32_t *off, int nenv, uint32_t base,
											 uint32_t *out, uint32_t k) {
	uint32_t nc = ast_nchild(a, n);
	if (k == nc) {
		*out = spv_const(m, want ? 1 : 0);
		return 1;
	}
	uint32_t cv;
	if (!spv_expr(m, a, ast_child(a, n, k), off, nenv, base, &cv))
		return 0;
	uint32_t cb = spv_bool_of(m, cv);
	uint32_t l_cont = spv_id(m), l_stop = spv_id(m), l_merge = spv_id(m);
	spvw_op(&m->body, SpvOpSelectionMerge, 3);
	spvw_put(&m->body, l_merge);
	spvw_put(&m->body, 0);
	spvw_op(&m->body, SpvOpBranchConditional, 4);
	spvw_put(&m->body, cb);
	spvw_put(&m->body, want ? l_cont : l_stop);
	spvw_put(&m->body, want ? l_stop : l_cont);

	uint32_t ldef_in = m->def;
	spv_label_at(m, l_cont);
	m->def = ldef_in;
	uint32_t rest;
	if (!spv_logical(m, a, n, want, off, nenv, base, &rest, k + 1))
		return 0;
	uint32_t def_cont = m->def;
	uint32_t from_cont = m->cur_label;
	spvw_op(&m->body, SpvOpBranch, 2);
	spvw_put(&m->body, l_merge);

	spv_label_at(m, l_stop);
	m->def = ldef_in;
	uint32_t stopv = spv_const(m, want ? 0 : 1);
	uint32_t def_stop = m->def;
	uint32_t from_stop = m->cur_label;
	spvw_op(&m->body, SpvOpBranch, 2);
	spvw_put(&m->body, l_merge);

	spv_label_at(m, l_merge);
	uint32_t phi = spv_id(m);
	spvw_op(&m->body, SpvOpPhi, 7);
	spvw_put(&m->body, m->id_int);
	spvw_put(&m->body, phi);
	spvw_put(&m->body, rest);
	spvw_put(&m->body, from_cont);
	spvw_put(&m->body, stopv);
	spvw_put(&m->body, from_stop);
	uint32_t ldphi = spv_id(m);
	spvw_op(&m->body, SpvOpPhi, 7);
	spvw_put(&m->body, m->id_bool);
	spvw_put(&m->body, ldphi);
	spvw_put(&m->body, def_cont);
	spvw_put(&m->body, from_cont);
	spvw_put(&m->body, def_stop);
	spvw_put(&m->body, from_stop);
	m->def = ldphi;
	*out = phi;
	return 1;
}

static int spv_expr(SpvMod *m, AstArena *a, AstLocal n, const int32_t *off,
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
		*out = spv_const(m, (int32_t)ast_eval_slice_fit((int64_t)ast_ival(a, n), t));
		return 1;
	}
	case AST_Ref: {
		int r = ast_op(a, n);
		int t = ast_type_t(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) {
			int k;
			if (!ast_eval_slice_intt(t) || is_float(t) || ast_eval_slice_is64(t))
				return 0;
			if (!spv_env_index(off, nenv, (int32_t)(int64_t)ast_ival(a, n), &k))
				return 0;
			*out = spv_load_live(m, base, k);
			return 1;
		}
		if ((r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
			if (ast_bad_type(t) || is_float(t) || !ast_eval_slice_intt(t))
				return 0;
			if (ast_eval_slice_is64(t))
				return 0;
			*out = spv_const(m, (int32_t)ast_eval_slice_fit((int64_t)ast_ival(a, n), t));
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
		if (!ast_eval_slice_intt(t) || is_float(t) || ast_eval_slice_is64(t))
			return 0;
		int k;
		if (!spv_env_index(off, nenv, (int32_t)(int64_t)ast_ival(a, c), &k))
			return 0;
		*out = spv_load_live(m, base, k);
		return 1;
	}
	case AST_Convert: {
		int t = ast_type_t(a, n);
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || is_float(t) || is_float(ast_type_t(a, c)))
			return 0;
		if (ast_bad_type(t) || !ast_eval_slice_intt(t) || ast_eval_slice_is64(t))
			return 0;
		uint32_t v;
		if (!spv_expr(m, a, c, off, nenv, base, &v))
			return 0;
		*out = spv_fit(m, v, t);
		return 1;
	}
	case AST_Unary: {
		int uop = ast_op(a, n);
		int t = ast_eval_slice_wtype(a, n);
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || !t || ast_eval_slice_is64(t))
			return 0;
		if (uop != '-' && uop != TOK_NEG && uop != '~' && uop != '!')
			return 0;
		uint32_t v;
		if (!spv_expr(m, a, c, off, nenv, base, &v))
			return 0;
		if (uop == '!') {
			uint32_t z = spv_emit3(m, SpvOpIEqual, m->id_bool, v, spv_const(m, 0));
			*out = spv_int_of_bool(m, z);
			return 1;
		}
		if (uop == '~') {
			*out = spv_fit(m, spv_emit2(m, SpvOpNot, m->id_int, v), t);
			return 1;
		}
		{
			uint32_t z = spv_const(m, 0);
			uint32_t r = spv_emit3(m, SpvOpISub, m->id_int, z, v);
			if (!((t & VT_UNSIGNED) != 0))
				spv_def_addsub(m, &m->def, 1, z, v, r);
			*out = r;
		}
		return 1;
	}
	case AST_Binary: {
		int bop = ast_op(a, n);
		if (bop == TOK_LAND || bop == TOK_LOR)
			return spv_logical(m, a, n, bop == TOK_LAND, off, nenv, base, out, 0);
		if (ast_nchild(a, n) != 2)
			return 0;
		AstLocal x = ast_child(a, n, 0), y = ast_child(a, n, 1);
		int xt = ast_eval_slice_wtype(a, x);
		if (!xt || is_float(ast_type_t(a, x)) || is_float(ast_type_t(a, y)))
			return 0;
		if (ast_eval_slice_is64(xt))
			return 0;
		uint32_t lv, rv;
		if (!spv_expr(m, a, x, off, nenv, base, &lv))
			return 0;
		if (!spv_expr(m, a, y, off, nenv, base, &rv))
			return 0;
		int uns = (xt & VT_UNSIGNED) != 0;
		int is_cmp;
		int code = spv_binop_code(bop, uns, &is_cmp);
		if (is_cmp < 0)
			return 0;
		if (is_cmp) {
			*out = spv_int_of_bool(m, spv_emit3(m, code, m->id_bool, lv, rv));
			return 1;
		}
		if (code == SpvOpUDiv || code == SpvOpUMod) {
			uint32_t d = spv_guard_div(m, &m->def, 1, lv, rv);
			*out = spv_unsigned_binop(m, code, lv, d);
			return 1;
		}
		if (code == SpvOpSRem) {
			uint32_t d = spv_guard_div(m, &m->def, 0, lv, rv);
			*out = spv_signed_rem(m, lv, d);
			return 1;
		}
		if (code == SpvOpSDiv) {
			uint32_t d = spv_guard_div(m, &m->def, 0, lv, rv);
			*out = spv_emit3(m, code, m->id_int, lv, d);
			return 1;
		}
		if (code == SpvOpShiftLeftLogical || code == SpvOpShiftRightLogical ||
				code == SpvOpShiftRightArithmetic) {
			uint32_t sh = spv_guard_shift(m, &m->def, rv);
			*out = spv_emit3(m, code, m->id_int, lv, sh);
			return 1;
		}
		*out = spv_emit3(m, code, m->id_int, lv, rv);
		if (!uns && code == SpvOpIAdd)
			spv_def_addsub(m, &m->def, 0, lv, rv, *out);
		else if (!uns && code == SpvOpISub)
			spv_def_addsub(m, &m->def, 1, lv, rv, *out);
		else if (!uns && code == SpvOpIMul)
			spv_def_mul(m, &m->def, lv, rv, *out);
		return 1;
	}
	case AST_If: {
		if (ast_nchild(a, n) != 3)
			return 0;
		return spv_branch_pair(m, a, ast_child(a, n, 0), ast_child(a, n, 1),
													 ast_child(a, n, 2), off, nenv, base, out);
	}
	default:
		return 0;
	}
}

static uint32_t *spv_module_finish(SpvMod *m, int *nwords) {
	int total = 5 + m->pre.n + m->types.n + m->body.n;
	uint32_t *w = (uint32_t *)SPV_MALLOC((size_t)total * sizeof *w);
	int i = 0;
	w[i++] = SPV_MAGIC;
	w[i++] = SPV_VERSION;
	w[i++] = 0;
	w[i++] = m->next_id;
	w[i++] = 0;
	memcpy(w + i, m->pre.w, (size_t)m->pre.n * sizeof *w);
	i += m->pre.n;
	memcpy(w + i, m->types.w, (size_t)m->types.n * sizeof *w);
	i += m->types.n;
	memcpy(w + i, m->body.w, (size_t)m->body.n * sizeof *w);
	i += m->body.n;
	*nwords = total;
	return w;
}

static void spv_module_free(SpvMod *m) {
	SPV_FREE(m->pre.w);
	SPV_FREE(m->types.w);
	SPV_FREE(m->body.w);
}

#endif /* MCC_GPU_LANG_MSL */
#endif /* MCC_GPU_EMITTER */

#if defined(MCC_GPU_ORACLE) && !defined(MCC_GPU_ORACLE_PROVIDED)
#define MCC_GPU_ORACLE_PROVIDED 1

#include <stdio.h>

#if MCC_GPU_LANG_MSL

static int mcc_gpu_emit(AstArena *a, AstLocal root, const int32_t *off, int n,
												MccGpuCode *c) {
	MslMod m;
	uint32_t base, val, lane;
	char *src;
	int nb = 0;
	msl_module_begin(&m, n);
	base = msl_main_begin(&m, n);
	if (!msl_expr(&m, a, root, off, n, base, &val) || m.failed) {
		msl_module_free(&m);
		return 0;
	}
	lane = msl_lane(&m, base, n);
	msl_main_end(&m, lane, val);
	src = msl_module_finish(&m, &nb);
	msl_module_free(&m);
	if (nb > MCC_GPU_CODE_MAX) {
		MCC_GPU_FREE(src);
		return 0;
	}
	c->p = src;
	c->n = nb;
	return 1;
}

#else /* !MCC_GPU_LANG_MSL */

static int mcc_gpu_emit(AstArena *a, AstLocal root, const int32_t *off, int n,
												MccGpuCode *c) {
	SpvMod m;
	uint32_t base, val, lane;
	uint32_t *code;
	int nwords = 0;
	spv_module_begin(&m, n);
	base = spv_main_begin(&m, n);
	if (!spv_expr(&m, a, root, off, n, base, &val) || m.failed) {
		spv_module_free(&m);
		return 0;
	}
	lane = spv_emit3(&m, SpvOpSDiv, m.id_int, base, spv_const(&m, n));
	spv_main_end(&m, lane, val);
	code = spv_module_finish(&m, &nwords);
	spv_module_free(&m);
	if (nwords > MCC_GPU_CODE_MAX) {
		MCC_GPU_FREE(code);
		return 0;
	}
	c->p = code;
	c->n = nwords;
	return 1;
}

#endif /* MCC_GPU_LANG_MSL */

static void mcc_gpu_code_free(MccGpuCode *c) {
	MCC_GPU_FREE(c->p);
	c->p = NULL;
	c->n = 0;
}

static void mcc_gpu_code_dump(const MccGpuCode *c, const char *path) {
	FILE *fp = fopen(path, "wb");
	if (fp) {
		fwrite(c->p, MCC_GPU_CODE_UNIT, (size_t)c->n, fp);
		fclose(fp);
	}
}

static int mcc_gpu_run(const MccGpuCode *c, const int32_t *in, int ntuple,
											 int nlive, int32_t *out) {
	return mcc_gpu_dispatch(c->p, c->n, in, ntuple, nlive, out);
}

#endif /* MCC_GPU_ORACLE */
