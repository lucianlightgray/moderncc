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

#define MCC_GPU_IN_SLOTS 2
#define MCC_GPU_OUT_SLOTS 3

#if MCC_GPU_LANG_MSL
#define MCC_GPU_CODE_MAX 65536
#define MCC_GPU_CODE_SUFFIX "metal"
#define MCC_GPU_CODE_UNIT 1
#else
#define MCC_GPU_CODE_MAX 16384
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
int mcc_gpu_dispatch2_ro_in(const void *ca, int na, const void *cb, int nb,
														const int32_t *in, int ntuple, int nlive,
														int32_t *oa, int32_t *ob);
void mcc_gpu_quiesce(void);
/* Frame dispatch: `inout` is both seeded into and read back out of the device
 * frame, so a kernel that stores to local slots can hand its results back. */
int mcc_gpu_dispatch_rw(const void *code, int n, int32_t *inout, int ntuple,
												int nslot);
/* As above, plus the per-lane value/defined slots for a run ending in Return. */
int mcc_gpu_dispatch_rw2(const void *code, int n, int32_t *inout, int ntuple,
												 int nslot, int32_t *out);
/* Nonzero while the device may still be dispatched to. Cleared when a dispatch
 * is stranded, i.e. abandoned with its command buffer still pending. */
int mcc_gpu_alive(void);
/* Host view of the shared device address space (binding 2): globals image,
 * heap, and the printf ring. Offset 0 is reserved as NULL. Valid between
 * dispatches only. */
int mcc_gpu_mem(void **base, unsigned long *size);
unsigned long mcc_gpu_host_import_align(const char **why);
int mcc_gpu_mem_import(void *base, unsigned long size);
/* How many dispatches have been abandoned with resources deliberately leaked
 * rather than freed under a live command buffer. */
long mcc_gpu_stranded(void);
void mcc_gpu_stats(MccGpuStats *out);
int mcc_gpu_f64(void);

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

static long mcc_gpu_bail_emit;

static int mcc_gpu_op_is_cmp(int op) {
	switch (op) {
	case TOK_EQ: case TOK_NE: case TOK_ULT: case TOK_UGE: case TOK_ULE:
	case TOK_UGT: case TOK_LE: case TOK_GE: case TOK_LT: case TOK_GT:
		return 1;
	default:
		return 0;
	}
}

static int mcc_gpu_vwt(AstArena *a, AstLocal n) {
	if (n == AST_NONE)
		return VT_INT;
	switch (ast_kind(a, n)) {
	case AST_Literal:
	case AST_Ref:
	case AST_Load:
	case AST_Convert:
		return ast_type_t(a, n);
	case AST_Unary: {
		int32_t mo, madd;
		int at;
		if (ast_eval_slice_member_off(a, n, &mo))
			return ast_type_t(a, n);
		if (ast_eval_slice_arrow(a, n, &mo, &madd, &at))
			return at;
		if (ast_op(a, n) == '!')
			return VT_INT;
		return ast_eval_slice_promote(ast_eval_slice_wtype(a, n));
	}
	case AST_Binary: {
		int bop = ast_op(a, n);
		if (bop == TOK_LAND || bop == TOK_LOR || mcc_gpu_op_is_cmp(bop))
			return VT_INT;
		if (ast_nchild(a, n) != 2)
			return VT_INT;
		return ast_eval_slice_binop_wtype(a, n);
	}
	case AST_If:
		if (ast_nchild(a, n) != 3)
			return VT_INT;
		return ast_eval_slice_uac(mcc_gpu_vwt(a, ast_child(a, n, 1)),
															mcc_gpu_vwt(a, ast_child(a, n, 2)));
	default:
		return VT_INT;
	}
}

static void mcc_gpu_vw(AstArena *a, AstLocal n, int *w64, int *uns) {
	int t = mcc_gpu_vwt(a, n);
	*w64 = ast_eval_slice_is64(t);
	*uns = (t & VT_UNSIGNED) != 0;
}

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

typedef struct MslV {
	uint32_t id;
	int w64;
	int uns;
	int f64;
} MslV;

typedef struct MslMod {
	MslBuf decls, body;
	uint32_t next_id;
	uint32_t def;
	uint32_t lane;
	int32_t cval[MSL_MAX_CONST];
	uint32_t cid[MSL_MAX_CONST];
	int ncached;
	int indent;
	int failed;
	/* Set by the store helpers. Selects the kernel signature in
	 * msl_module_finish; see msl_kernel_ro/msl_kernel_rw. */
	int wrote_in;
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

static uint32_t msl_pv(MslMod *m, const char *fmt, ...) {
	uint32_t id = msl_id(m);
	char tmp[MSL_LINE_MAX], out[MSL_LINE_MAX];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(tmp, sizeof tmp, fmt, ap);
	va_end(ap);
	snprintf(out, sizeof out, "int2 p%u = %s;", id, tmp);
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

static MslV msl_mk(uint32_t id, int w64, int uns) {
	MslV v;
	v.id = id;
	v.w64 = w64;
	v.uns = uns;
	v.f64 = 0;
	return v;
}

static MslV msl_mkf(uint32_t id) {
	MslV v;
	v.id = id;
	v.w64 = 1;
	v.uns = 0;
	v.f64 = 1;
	return v;
}


static void msl_widen(MslMod *m, MslV *v) {
	if (v->w64)
		return;
	v->id = v->uns ? msl_pv(m, "int2(v%u, 0)", v->id)
								 : msl_pv(m, "int2(v%u, mcc_sar(v%u, 31))", v->id, v->id);
	v->w64 = 1;
}

static MslV msl_f64_of(MslMod *m, MslV v) {
	if (v.f64)
		return v;
	msl_widen(m, &v);
	return msl_mkf(v.id);
}

static uint32_t msl_lo(MslMod *m, MslV v) {
	return v.w64 ? msl_iv(m, "p%u.x", v.id) : v.id;
}

static uint32_t msl_pair(MslMod *m, MslV v) {
	msl_widen(m, &v);
	return v.id;
}

static MslV msl_const64(MslMod *m, int64_t x) {
	uint32_t lo = msl_const(m, (int32_t)(uint32_t)(uint64_t)x);
	uint32_t hi = msl_const(m, (int32_t)(uint32_t)((uint64_t)x >> 32));
	return msl_mk(msl_pv(m, "int2(v%u, v%u)", lo, hi), 1, 0);
}

static uint32_t msl_bool_of_v(MslMod *m, MslV v) {
	if (v.f64)
		return msl_bv(m, "(p%u.x | (p%u.y & 0x7fffffff)) != 0", v.id, v.id);
	if (!v.w64)
		return msl_bool_of(m, v.id);
	return msl_bv(m, "mcc64_nz(p%u)", v.id);
}

static uint32_t msl_load_at(MslMod *m, uint32_t idx) {
	return msl_iv(m, "inb[v%u]", idx);
}

/* k is a WORD offset here, matching spv_load_live. It used to be a slot -- the
 * body doubled it -- while the SPIR-V twin added it directly, so the two arms
 * disagreed on what the same argument meant. The store helpers below index the
 * same way, and a slot-vs-word mix-up there would land every high word on top
 * of the next slot's low word. */
static uint32_t msl_load_live(MslMod *m, uint32_t base, int k) {
	if (k)
		return msl_iv(m, "inb[v%u + %d]", base, k);
	return msl_load_at(m, base);
}

static MslV msl_load_live_v(MslMod *m, uint32_t base, int k, int w64, int uns) {
	if (!w64)
		return msl_mk(msl_load_live(m, base, 2 * k), 0, uns);
	return msl_mk(msl_pv(m, "int2(inb[v%u + %d], inb[v%u + %d])", base, 2 * k,
											 base, 2 * k + 1),
								1, uns);
}

/* The store counterpart of msl_load_at, and the twin of spv_store_at_in. The
 * input buffer is the device frame: making it writable is what lets a run of
 * statements lower as one kernel instead of one kernel per expression. Setting
 * wrote_in is what drops `const` from buffer 0 in the emitted signature, and it
 * must be set here rather than by the caller -- a module that stores through
 * any path at all needs the read-write signature. */
static void msl_store_at_in(MslMod *m, uint32_t idx, uint32_t val) {
	m->wrote_in = 1;
	msl_line(m, "inb[v%u] = v%u;", idx, val);
}

/* k is a WORD offset, as in msl_load_live. */
static void msl_store_live(MslMod *m, uint32_t base, int k, uint32_t val) {
	m->wrote_in = 1;
	if (k)
		msl_line(m, "inb[v%u + %d] = v%u;", base, k, val);
	else
		msl_line(m, "inb[v%u] = v%u;", base, val);
}

static void msl_store_live_v(MslMod *m, uint32_t base, int k, MslV v) {
	if (!v.w64) {
		/* The high word follows the value's own signedness, exactly as
		 * spv_store_live_v does: an unsigned 32-bit value zero-extends and a
		 * signed one sign-extends. Sign-extending unconditionally stored -2 into
		 * an `unsigned int` slot as -2 where the CPU has 4294967294. */
		uint32_t hi = v.uns ? msl_const(m, 0) : msl_iv(m, "mcc_sar(v%u, 31)", v.id);
		msl_store_live(m, base, 2 * k, v.id);
		msl_store_live(m, base, 2 * k + 1, hi);
		return;
	}
	{
		/* Named before the stores: msl_iv emits on evaluation and C leaves
		 * argument order unspecified, so inlining these would leave the emitted
		 * line order up to the host compiler. */
		uint32_t lo = msl_iv(m, "p%u.x", v.id);
		uint32_t hi = msl_iv(m, "p%u.y", v.id);
		msl_store_live(m, base, 2 * k, lo);
		msl_store_live(m, base, 2 * k + 1, hi);
	}
}

static uint32_t msl_slot_at(MslMod *m, uint32_t base, int k0, uint32_t elem,
														int half) {
	uint32_t s = msl_iv(m, "mcc_add(v%u, v%u)", elem, msl_const(m, k0));
	uint32_t w = msl_iv(m, "mcc_mul(v%u, v%u)", s,
											msl_const(m, MCC_GPU_IN_SLOTS));
	uint32_t i = msl_iv(m, "mcc_add(v%u, v%u)", base, w);
	if (half)
		i = msl_iv(m, "mcc_add(v%u, v%u)", i, msl_const(m, half));
	return i;
}

static uint32_t msl_dyn_elem(MslMod *m, MslV idx, const AstEvalSliceIdx *ix) {
	uint32_t lo = msl_lo(m, idx);
	uint32_t ok =
			msl_bv(m, "as_type<uint>(v%u) < %uu", lo, (unsigned)ix->nelem);
	msl_def_and(m, &m->def, ok);
	return msl_iv(m, "v%u & %d", lo, ix->nspan - 1);
}

static MslV msl_load_live_dv(MslMod *m, uint32_t base, int k0, uint32_t elem,
														 int w64, int uns) {
	if (!w64)
		return msl_mk(msl_load_at(m, msl_slot_at(m, base, k0, elem, 0)), 0, uns);
	{
		uint32_t lo = msl_load_at(m, msl_slot_at(m, base, k0, elem, 0));
		uint32_t hi = msl_load_at(m, msl_slot_at(m, base, k0, elem, 1));
		return msl_mk(msl_pv(m, "int2(v%u, v%u)", lo, hi), 1, uns);
	}
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
	uint32_t nl = msl_const(m, nlive * MCC_GPU_IN_SLOTS);
	m->def = msl_true(m);
	m->lane = gi;
	return msl_iv(m, "mcc_mul(v%u, v%u)", gi, nl);
}

static void msl_main_end(MslMod *m, uint32_t lane, MslV val) {
	uint32_t cn = msl_const(m, MCC_GPU_OUT_SLOTS);
	uint32_t c1 = msl_const(m, 1);
	uint32_t c2 = msl_const(m, 2);
	uint32_t o0 = msl_iv(m, "mcc_mul(v%u, v%u)", lane, cn);
	uint32_t o1 = msl_iv(m, "mcc_add(v%u, v%u)", o0, c1);
	uint32_t o2 = msl_iv(m, "mcc_add(v%u, v%u)", o0, c2);
	uint32_t p = msl_pair(m, val);
	uint32_t d = msl_int_of_bool(m, m->def);
	msl_line(m, "outb[v%u] = p%u.x;", o0, p);
	msl_line(m, "outb[v%u] = p%u.y;", o1, p);
	msl_line(m, "outb[v%u] = v%u;", o2, d);
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

static MslV msl_fit_v(MslMod *m, MslV v, int t) {
	int uns = (t & VT_UNSIGNED) != 0;
	if (ast_eval_slice_f64t(t))
		return msl_f64_of(m, v);
	if (ast_eval_slice_is64(t)) {
		msl_widen(m, &v);
		v.uns = uns;
		return v;
	}
	if ((t & VT_BTYPE) == VT_BOOL)
		return msl_mk(msl_int_of_bool(m, msl_bool_of_v(m, v)), 0, uns);
	return msl_mk(msl_fit(m, msl_lo(m, v), t), 0, uns);
}

static void msl_def_addsub64(MslMod *m, uint32_t *def, int is_sub, MslV a,
														 MslV b, MslV r) {
	if (is_sub)
		msl_def_and(m, def,
								msl_bv(m, "mcc64_subok(p%u, p%u, p%u)", a.id, b.id, r.id));
	else
		msl_def_and(m, def,
								msl_bv(m, "mcc64_addok(p%u, p%u, p%u)", a.id, b.id, r.id));
}

static uint32_t msl_guard_div64(MslMod *m, uint32_t *def, int uns, MslV a,
																MslV b) {
	uint32_t bad = msl_bv(m, "!mcc64_nz(p%u)", b.id);
	if (!uns) {
		MslV mn = msl_const64(m, (int64_t)((uint64_t)1 << 63));
		MslV n1 = msl_const64(m, -1);
		uint32_t amin = msl_bv(m, "mcc64_eq(p%u, p%u)", a.id, mn.id);
		uint32_t bneg = msl_bv(m, "mcc64_eq(p%u, p%u)", b.id, n1.id);
		bad = msl_or(m, bad, msl_bv(m, "b%u && b%u", amin, bneg));
	}
	msl_def_and(m, def, msl_not(m, bad));
	return msl_pv(m, "b%u ? int2(1, 0) : p%u", bad, b.id);
}

static uint32_t msl_guard_shift64(MslMod *m, uint32_t *def, MslV b) {
	uint32_t bad =
			msl_bv(m, "p%u.y != 0 || as_type<uint>(p%u.x) >= 64u", b.id, b.id);
	msl_def_and(m, def, msl_not(m, bad));
	return msl_iv(m, "b%u ? 0 : (p%u.x & 63)", bad, b.id);
}

static MslV msl_sdiv64(MslMod *m, MslV a, MslV b) {
	uint32_t sa = msl_bv(m, "p%u.y < 0", a.id);
	uint32_t sb = msl_bv(m, "p%u.y < 0", b.id);
	uint32_t na = msl_pv(m, "b%u ? mcc64_neg(p%u) : p%u", sa, a.id, a.id);
	uint32_t nb = msl_pv(m, "b%u ? mcc64_neg(p%u) : p%u", sb, b.id, b.id);
	uint32_t q = msl_pv(m, "mcc64_udiv(p%u, p%u)", na, nb);
	uint32_t s = msl_bv(m, "b%u != b%u", sa, sb);
	return msl_mk(msl_pv(m, "b%u ? mcc64_neg(p%u) : p%u", s, q, q), 1, 0);
}

static void msl_def_mul64(MslMod *m, uint32_t *def, MslV a, MslV b, MslV r) {
	MslV mn = msl_const64(m, (int64_t)((uint64_t)1 << 63));
	MslV n1 = msl_const64(m, -1);
	uint32_t az = msl_bv(m, "!mcc64_nz(p%u)", a.id);
	uint32_t bz = msl_bv(m, "!mcc64_nz(p%u)", b.id);
	uint32_t ag = msl_pv(m, "b%u ? int2(1, 0) : p%u", az, a.id);
	MslV q = msl_sdiv64(m, r, msl_mk(ag, 1, 0));
	uint32_t ne = msl_bv(m, "!mcc64_eq(p%u, p%u)", q.id, b.id);
	uint32_t k1 = msl_bv(m, "mcc64_eq(p%u, p%u) && mcc64_eq(p%u, p%u)", a.id,
											 mn.id, b.id, n1.id);
	uint32_t k2 = msl_bv(m, "mcc64_eq(p%u, p%u) && mcc64_eq(p%u, p%u)", b.id,
											 mn.id, a.id, n1.id);
	uint32_t bad = msl_bv(m, "!b%u && !b%u && (b%u || b%u || b%u)", az, bz, ne,
												k1, k2);
	msl_def_and(m, def, msl_not(m, bad));
}

static MslV msl_arith64(MslMod *m, int code, MslV a, MslV b, uint32_t sh,
												int uns) {
	switch (code) {
	case MslOpAdd:
		return msl_mk(msl_pv(m, "mcc64_add(p%u, p%u)", a.id, b.id), 1, uns);
	case MslOpSub:
		return msl_mk(msl_pv(m, "mcc64_sub(p%u, p%u)", a.id, b.id), 1, uns);
	case MslOpMul:
		return msl_mk(msl_pv(m, "mcc64_mul(p%u, p%u)", a.id, b.id), 1, uns);
	case MslOpShl:
		return msl_mk(msl_pv(m, "mcc64_shl(p%u, as_type<uint>(v%u))", a.id, sh), 1,
									uns);
	case MslOpShr:
		return msl_mk(msl_pv(m, "mcc64_shr(p%u, as_type<uint>(v%u))", a.id, sh), 1,
									uns);
	case MslOpSar:
		return msl_mk(msl_pv(m, "mcc64_sar(p%u, as_type<uint>(v%u))", a.id, sh), 1,
									uns);
	case MslOpAnd:
		return msl_mk(msl_pv(m, "p%u & p%u", a.id, b.id), 1, uns);
	case MslOpOr:
		return msl_mk(msl_pv(m, "p%u | p%u", a.id, b.id), 1, uns);
	default:
		return msl_mk(msl_pv(m, "p%u ^ p%u", a.id, b.id), 1, uns);
	}
}

static uint32_t msl_cmp64(MslMod *m, int code, MslV a, MslV b) {
	switch (code) {
	case MslOpEq: return msl_bv(m, "mcc64_eq(p%u, p%u)", a.id, b.id);
	case MslOpNe: return msl_bv(m, "!mcc64_eq(p%u, p%u)", a.id, b.id);
	case MslOpULt: return msl_bv(m, "mcc64_ult(p%u, p%u)", a.id, b.id);
	case MslOpUGe: return msl_bv(m, "!mcc64_ult(p%u, p%u)", a.id, b.id);
	case MslOpULe: return msl_bv(m, "!mcc64_ult(p%u, p%u)", b.id, a.id);
	case MslOpUGt: return msl_bv(m, "mcc64_ult(p%u, p%u)", b.id, a.id);
	case MslOpSLt: return msl_bv(m, "mcc64_slt(p%u, p%u)", a.id, b.id);
	case MslOpSGe: return msl_bv(m, "!mcc64_slt(p%u, p%u)", a.id, b.id);
	case MslOpSLe: return msl_bv(m, "!mcc64_slt(p%u, p%u)", b.id, a.id);
	default: return msl_bv(m, "mcc64_slt(p%u, p%u)", b.id, a.id);
	}
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

static int msl_f64_binop_code(int op) {
	switch (op) {
	case '+':
	case '-':
	case '*':
	case TOK_EQ:
	case TOK_NE:
	case TOK_LT:
	case TOK_LE:
	case TOK_GT:
	case TOK_GE:
		return op;
	default:
		return 0;
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
										int nenv, uint32_t base, MslV *out);

static int msl_branch_pair(MslMod *m, AstArena *a, AstLocal n,
													 const int32_t *off, int nenv, uint32_t base,
													 MslV *out) {
	AstLocal cn = ast_child(a, n, 0), tn = ast_child(a, n, 1);
	AstLocal en = ast_child(a, n, 2);
	MslV cv, tv, ev;
	uint32_t cb, res, dres, def_in;
	int w64, uns;
	int ft = ast_eval_slice_ftype(a, n);
	mcc_gpu_vw(a, n, &w64, &uns);
	if (ft) {
		w64 = 1;
		uns = 0;
	}
	if (!msl_expr(m, a, cn, off, nenv, base, &cv))
		return 0;
	cb = msl_bool_of_v(m, cv);
	def_in = m->def;
	res = msl_id(m);
	dres = msl_id(m);
	if (w64)
		msl_line(m, "int2 p%u;", res);
	else
		msl_line(m, "int v%u;", res);
	msl_line(m, "bool b%u;", dres);
	msl_line(m, "if (b%u) {", cb);
	m->indent++;
	m->def = def_in;
	if (!msl_expr(m, a, tn, off, nenv, base, &tv))
		return 0;
	if (w64)
		msl_line(m, "p%u = p%u;", res, msl_pair(m, tv));
	else
		msl_line(m, "v%u = v%u;", res, tv.id);
	msl_line(m, "b%u = b%u;", dres, m->def);
	m->indent--;
	msl_line(m, "} else {");
	m->indent++;
	m->def = def_in;
	if (!msl_expr(m, a, en, off, nenv, base, &ev))
		return 0;
	if (w64)
		msl_line(m, "p%u = p%u;", res, msl_pair(m, ev));
	else
		msl_line(m, "v%u = v%u;", res, ev.id);
	msl_line(m, "b%u = b%u;", dres, m->def);
	m->indent--;
	msl_line(m, "}");
	m->def = dres;
	*out = ft ? msl_mkf(res) : msl_mk(res, w64, uns);
	return 1;
}

static int msl_logical(MslMod *m, AstArena *a, AstLocal n, int want,
											 const int32_t *off, int nenv, uint32_t base,
											 MslV *out, uint32_t k) {
	uint32_t nc = ast_nchild(a, n);
	uint32_t cb, res, dres, def_in, stopv;
	MslV cv, rest;
	if (k == nc) {
		*out = msl_mk(msl_const(m, want ? 1 : 0), 0, 0);
		return 1;
	}
	if (!msl_expr(m, a, ast_child(a, n, k), off, nenv, base, &cv))
		return 0;
	cb = msl_bool_of_v(m, cv);
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
	msl_line(m, "v%u = v%u;", res, rest.id);
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
	*out = msl_mk(res, 0, 0);
	return 1;
}

static int msl_konst(MslMod *m, AstArena *a, AstLocal n, int t, MslV *out) {
	int64_t x;
	int uns = (t & VT_UNSIGNED) != 0;
	if (ast_eval_slice_f64t(t)) {
		*out = msl_mkf(msl_const64(m, (int64_t)ast_ival(a, n)).id);
		return 1;
	}
	x = ast_eval_slice_fit((int64_t)ast_ival(a, n), t);
	if (ast_eval_slice_is64(t)) {
		*out = msl_const64(m, x);
		out->uns = uns;
		return 1;
	}
	*out = msl_mk(msl_const(m, (int32_t)x), 0, uns);
	return 1;
}

static int msl_expr(MslMod *m, AstArena *a, AstLocal n, const int32_t *off,
										int nenv, uint32_t base, MslV *out) {
	if (n == AST_NONE || m->failed)
		return 0;
	switch (ast_kind(a, n)) {
	case AST_Bailout: {
		int t = ast_type_t(a, n);
		if (ast_bad_type(t) || is_float(t) || !ast_eval_slice_intt(t))
			return 0;
		msl_def_and(m, &m->def, msl_bv(m, "false"));
		mcc_gpu_bail_emit++;
		if (ast_eval_slice_is64(t)) {
			*out = msl_const64(m, 0);
			out->uns = (t & VT_UNSIGNED) != 0;
			return 1;
		}
		*out = msl_mk(msl_const(m, 0), 0, (t & VT_UNSIGNED) != 0);
		return 1;
	}
	case AST_Literal: {
		int t = ast_type_t(a, n);
		if (ast_bad_type(t))
			return 0;
		if ((ast_op(a, n) & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
			return 0;
		if (!ast_eval_slice_f64t(t) && (is_float(t) || !ast_eval_slice_intt(t)))
			return 0;
		return msl_konst(m, a, n, t, out);
	}
	case AST_Ref: {
		int r = ast_op(a, n);
		int t = ast_type_t(a, n);
		int32_t go;
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) {
			int k;
			if (!ast_bad_type(t) && ast_eval_slice_f64t(t)) {
				if (!msl_env_index(off, nenv, (int32_t)(int64_t)ast_ival(a, n), &k))
					return 0;
				*out = msl_mkf(msl_load_live_v(m, base, k, 1, 0).id);
				return 1;
			}
			if (!ast_eval_slice_intt(t) || is_float(t))
				return 0;
			if (!msl_env_index(off, nenv, (int32_t)(int64_t)ast_ival(a, n), &k))
				return 0;
			/* Narrow to the ref's own type; msl_load_live_v carries only a width
			 * flag and cannot deliver VT_BOOL/BYTE/SHORT on its own. */
			*out = msl_fit_v(m, msl_load_live_v(m, base, k, ast_eval_slice_is64(t),
																					(t & VT_UNSIGNED) != 0),
											 t);
			return 1;
		}
		if ((r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
			if (ast_bad_type(t))
				return 0;
			if (!ast_eval_slice_f64t(t) && (is_float(t) || !ast_eval_slice_intt(t)))
				return 0;
			return msl_konst(m, a, n, t, out);
		}
		if (!(t & VT_ARRAY) && ast_eval_slice_globl(a, n, &go)) {
			int k;
			if (!msl_env_index(off, nenv, go, &k))
				return 0;
			*out = msl_fit_v(m, msl_load_live_v(m, base, k, ast_eval_slice_is64(t),
																					(t & VT_UNSIGNED) != 0),
											 t);
			return 1;
		}
		return 0;
	}
	case AST_Load: {
		AstLocal c = ast_first_child(a, n);
		AstEvalSliceIdx ix;
		int t, k;
		int32_t fo;
		if (c == AST_NONE)
			return 0;
		t = ast_type_t(a, n);
		if (!ast_bad_type(t) && ast_eval_slice_f64t(t) &&
				ast_eval_slice_frame_off(a, c, &fo, 0)) {
			if (!msl_env_index(off, nenv, fo, &k))
				return 0;
			*out = msl_mkf(msl_load_live_v(m, base, k, 1, 0).id);
			return 1;
		}
		if (ast_eval_slice_intt(t) && !is_float(t) &&
				ast_eval_slice_frame_off(a, c, &fo, 0)) {
			if (!msl_env_index(off, nenv, fo, &k))
				return 0;
			*out = msl_fit_v(m, msl_load_live_v(m, base, k, ast_eval_slice_is64(t),
																					(t & VT_UNSIGNED) != 0),
											 t);
			return 1;
		}
		if (ast_eval_slice_dynidx(a, c, &ix)) {
			MslV iv;
			uint32_t elem;
			if (ast_eval_slice_ext(&ix))
				return 0;
			if (!msl_env_index(off, nenv, ix.base, &k))
				return 0;
			if (!msl_expr(m, a, ix.idx, off, nenv, base, &iv))
				return 0;
			elem = msl_dyn_elem(m, iv, &ix);
			*out = msl_fit_v(
					m,
					msl_load_live_dv(m, base, k, elem,
													 ast_eval_slice_is64(ix.etype) ||
															 ast_eval_slice_f64t(ix.etype),
													 (ix.etype & VT_UNSIGNED) != 0),
					ix.etype);
			return 1;
		}
		return 0;
	}
	case AST_Convert: {
		int t = ast_type_t(a, n);
		AstLocal c = ast_first_child(a, n);
		MslV v;
		if (c == AST_NONE || is_float(t) || is_float(ast_type_t(a, c)))
			return 0;
		if (ast_eval_slice_ftype(a, c))
			return 0;
		if (ast_bad_type(t) || !ast_eval_slice_intt(t))
			return 0;
		if (!msl_expr(m, a, c, off, nenv, base, &v))
			return 0;
		*out = msl_fit_v(m, v, t);
		return 1;
	}
	case AST_Unary: {
		int uop = ast_op(a, n);
		int t = ast_eval_slice_promote(ast_eval_slice_wtype(a, n));
		AstLocal c = ast_first_child(a, n);
		MslV v;
		int ft, is64, uns;
		int32_t mo;
		if (c == AST_NONE)
			return 0;
		if (ast_eval_slice_member_off(a, n, &mo)) {
			int mt = ast_type_t(a, n);
			int k;
			if (!msl_env_index(off, nenv, mo, &k))
				return 0;
			*out = msl_fit_v(m, msl_load_live_v(m, base, k, ast_eval_slice_is64(mt),
																					(mt & VT_UNSIGNED) != 0),
											 mt);
			return 1;
		}
		if (uop != '-' && uop != TOK_NEG && uop != '~' && uop != '!')
			return 0;
		ft = ast_eval_slice_ftype(a, c);
		if (ft && uop == '~')
			return 0;
		if (!ft && !t)
			return 0;
		if (!msl_expr(m, a, c, off, nenv, base, &v))
			return 0;
		if (ft) {
			MslV fv = msl_f64_of(m, v);
			if (uop == '!') {
				*out = msl_mk(msl_int_of_bool(m, msl_not(m, msl_bool_of_v(m, fv))), 0,
											0);
				return 1;
			}
			*out = msl_mkf(msl_pv(m, "int2(p%u.x, p%u.y ^ as_type<int>(0x80000000u))",
														fv.id, fv.id));
			return 1;
		}
		is64 = ast_eval_slice_is64(t);
		uns = (t & VT_UNSIGNED) != 0;
		if (uop == '!') {
			uint32_t z = v.w64 ? msl_bv(m, "!mcc64_nz(p%u)", v.id)
												 : msl_bv(m, "v%u == 0", v.id);
			*out = msl_mk(msl_int_of_bool(m, z), 0, 0);
			return 1;
		}
		if (uop == '~') {
			if (is64) {
				msl_widen(m, &v);
				*out = msl_mk(msl_pv(m, "~p%u", v.id), 1, uns);
			} else {
				*out = msl_mk(msl_fit(m, msl_iv(m, "~v%u", msl_lo(m, v)), t), 0, uns);
			}
			return 1;
		}
		if (is64) {
			MslV z = msl_const64(m, 0), r;
			msl_widen(m, &v);
			r = msl_arith64(m, MslOpSub, z, v, 0, uns);
			if (!uns)
				msl_def_addsub64(m, &m->def, 1, z, v, r);
			*out = r;
		} else {
			uint32_t z = msl_const(m, 0);
			uint32_t lo = msl_lo(m, v);
			uint32_t r = msl_iv(m, "mcc_sub(v%u, v%u)", z, lo);
			if (!uns)
				msl_def_addsub(m, &m->def, 1, z, lo, r);
			*out = msl_mk(r, 0, uns);
		}
		return 1;
	}
	case AST_Binary: {
		int bop = ast_op(a, n);
		AstLocal x, y;
		int xt, wt, xft, uns, is_cmp, code, is64;
		MslV lv, rv;
		if (bop == TOK_LAND || bop == TOK_LOR)
			return msl_logical(m, a, n, bop == TOK_LAND, off, nenv, base, out, 0);
		if (ast_nchild(a, n) != 2)
			return 0;
		x = ast_child(a, n, 0);
		y = ast_child(a, n, 1);
		xft = ast_eval_slice_ftype(a, x);
		if (xft) {
			MslV fl, fr;
			uint32_t cb;
			int fcode = msl_f64_binop_code(bop);
			if (!ast_eval_slice_ftype(a, y) || !fcode)
				return 0;
			if (!msl_expr(m, a, x, off, nenv, base, &fl))
				return 0;
			if (!msl_expr(m, a, y, off, nenv, base, &fr))
				return 0;
			fl = msl_f64_of(m, fl);
			fr = msl_f64_of(m, fr);
			if (fcode == '+' || fcode == '-') {
				*out = msl_mkf(msl_pv(m, "mccf64_addsub(p%u, p%u, %uu)", fl.id,
															fr.id, fcode == '-' ? 1u : 0u));
				return 1;
			}
			if (fcode == '*') {
				*out = msl_mkf(msl_pv(m, "mccf64_mul(p%u, p%u)", fl.id, fr.id));
				return 1;
			}
			switch (fcode) {
			case TOK_EQ:
				cb = msl_bv(m, "mccf64_cmp(p%u, p%u) == 0", fl.id, fr.id);
				break;
			case TOK_NE:
				cb = msl_bv(m, "mccf64_cmp(p%u, p%u) != 0", fl.id, fr.id);
				break;
			case TOK_LT:
				cb = msl_bv(m, "mccf64_cmp(p%u, p%u) < 0", fl.id, fr.id);
				break;
			case TOK_LE:
				cb = msl_bv(m, "mccf64_cmp(p%u, p%u) <= 0", fl.id, fr.id);
				break;
			case TOK_GT:
				cb = msl_bv(m, "mccf64_cmp(p%u, p%u) < 0", fr.id, fl.id);
				break;
			default:
				cb = msl_bv(m, "mccf64_cmp(p%u, p%u) <= 0", fr.id, fl.id);
				break;
			}
			*out = msl_mk(msl_int_of_bool(m, cb), 0, 0);
			return 1;
		}
		xt = ast_eval_slice_wtype(a, x);
		wt = ast_eval_slice_binop_wtype(a, n);
		if (!xt || !wt || is_float(ast_type_t(a, x)) || is_float(ast_type_t(a, y)))
			return 0;
		if (ast_eval_slice_ftype(a, y))
			return 0;
		if (!msl_expr(m, a, x, off, nenv, base, &lv))
			return 0;
		if (!msl_expr(m, a, y, off, nenv, base, &rv))
			return 0;
		uns = (wt & VT_UNSIGNED) != 0;
		is64 = ast_eval_slice_is64(wt);
		code = msl_binop_code(bop, uns, &is_cmp);
		if (is_cmp < 0)
			return 0;
		if (!is64) {
			uint32_t l = msl_lo(m, lv), r = msl_lo(m, rv), res;
			if (is_cmp) {
				*out = msl_mk(msl_int_of_bool(m, msl_cmp(m, code, l, r)), 0, 0);
				return 1;
			}
			if (code == MslOpUDiv || code == MslOpUMod) {
				uint32_t d = msl_guard_div(m, &m->def, 1, l, r);
				*out = msl_mk(msl_arith(m, code, l, d), 0, uns);
				return 1;
			}
			if (code == MslOpSRem) {
				uint32_t d = msl_guard_div(m, &m->def, 0, l, r);
				*out = msl_mk(msl_signed_rem(m, l, d), 0, uns);
				return 1;
			}
			if (code == MslOpSDiv) {
				uint32_t d = msl_guard_div(m, &m->def, 0, l, r);
				*out = msl_mk(msl_arith(m, code, l, d), 0, uns);
				return 1;
			}
			if (code == MslOpShl || code == MslOpShr || code == MslOpSar) {
				uint32_t sh = msl_guard_shift(m, &m->def, r);
				*out = msl_mk(msl_arith(m, code, l, sh), 0, uns);
				return 1;
			}
			res = msl_arith(m, code, l, r);
			if (!uns && code == MslOpAdd)
				msl_def_addsub(m, &m->def, 0, l, r, res);
			else if (!uns && code == MslOpSub)
				msl_def_addsub(m, &m->def, 1, l, r, res);
			else if (!uns && code == MslOpMul)
				msl_def_mul(m, &m->def, l, r, res);
			*out = msl_mk(res, 0, uns);
			return 1;
		}
		msl_widen(m, &lv);
		msl_widen(m, &rv);
		if (is_cmp) {
			*out = msl_mk(msl_int_of_bool(m, msl_cmp64(m, code, lv, rv)), 0, 0);
			return 1;
		}
		if (code == MslOpUDiv || code == MslOpUMod) {
			MslV d = msl_mk(msl_guard_div64(m, &m->def, 1, lv, rv), 1, 1);
			MslV q = msl_mk(msl_pv(m, "mcc64_udiv(p%u, p%u)", lv.id, d.id), 1, uns);
			if (code == MslOpUMod) {
				uint32_t pr = msl_pv(m, "mcc64_mul(p%u, p%u)", d.id, q.id);
				*out = msl_mk(msl_pv(m, "mcc64_sub(p%u, p%u)", lv.id, pr), 1, uns);
			} else {
				*out = q;
			}
			return 1;
		}
		if (code == MslOpSDiv || code == MslOpSRem) {
			MslV d = msl_mk(msl_guard_div64(m, &m->def, 0, lv, rv), 1, 0);
			MslV q = msl_sdiv64(m, lv, d);
			if (code == MslOpSRem) {
				uint32_t pr = msl_pv(m, "mcc64_mul(p%u, p%u)", d.id, q.id);
				*out = msl_mk(msl_pv(m, "mcc64_sub(p%u, p%u)", lv.id, pr), 1, uns);
			} else {
				q.uns = uns;
				*out = q;
			}
			return 1;
		}
		if (code == MslOpShl || code == MslOpShr || code == MslOpSar) {
			uint32_t sh = msl_guard_shift64(m, &m->def, rv);
			*out = msl_arith64(m, code, lv, rv, sh, uns);
			return 1;
		}
		*out = msl_arith64(m, code, lv, rv, 0, uns);
		if (!uns && code == MslOpAdd)
			msl_def_addsub64(m, &m->def, 0, lv, rv, *out);
		else if (!uns && code == MslOpSub)
			msl_def_addsub64(m, &m->def, 1, lv, rv, *out);
		else if (!uns && code == MslOpMul)
			msl_def_mul64(m, &m->def, lv, rv, *out);
		return 1;
	}
	case AST_If: {
		if (ast_nchild(a, n) != 3)
			return 0;
		return msl_branch_pair(m, a, n, off, nenv, base, out);
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
		"static inline int2 mcc64_add(int2 a, int2 b) {\n"
		"\tuint al = as_type<uint>(a.x), bl = as_type<uint>(b.x);\n"
		"\tuint lo = al + bl;\n"
		"\tuint hi = as_type<uint>(a.y) + as_type<uint>(b.y) + (lo < al ? 1u : 0u);\n"
		"\treturn int2(as_type<int>(lo), as_type<int>(hi));\n"
		"}\n"
		"static inline int2 mcc64_sub(int2 a, int2 b) {\n"
		"\tuint al = as_type<uint>(a.x), bl = as_type<uint>(b.x);\n"
		"\tuint lo = al - bl;\n"
		"\tuint hi = as_type<uint>(a.y) - as_type<uint>(b.y) - (al < bl ? 1u : 0u);\n"
		"\treturn int2(as_type<int>(lo), as_type<int>(hi));\n"
		"}\n"
		"static inline int2 mcc64_neg(int2 a) {\n"
		"\treturn mcc64_sub(int2(0, 0), a);\n"
		"}\n"
		"static inline int2 mcc64_mul(int2 a, int2 b) {\n"
		"\tuint al = as_type<uint>(a.x), ah = as_type<uint>(a.y);\n"
		"\tuint bl = as_type<uint>(b.x), bh = as_type<uint>(b.y);\n"
		"\tuint lo = al * bl;\n"
		"\tuint hi = mulhi(al, bl) + al * bh + ah * bl;\n"
		"\treturn int2(as_type<int>(lo), as_type<int>(hi));\n"
		"}\n"
		"static inline int2 mcc64_shl(int2 a, uint s) {\n"
		"\tuint al = as_type<uint>(a.x), ah = as_type<uint>(a.y);\n"
		"\tuint s1 = s & 31u, n1 = 31u - s1;\n"
		"\tbool big = (s & 32u) != 0u;\n"
		"\tuint lo = big ? 0u : (al << s1);\n"
		"\tuint hi = big ? (al << s1) : ((ah << s1) | ((al >> 1) >> n1));\n"
		"\treturn int2(as_type<int>(lo), as_type<int>(hi));\n"
		"}\n"
		"static inline int2 mcc64_shr(int2 a, uint s) {\n"
		"\tuint al = as_type<uint>(a.x), ah = as_type<uint>(a.y);\n"
		"\tuint s1 = s & 31u, n1 = 31u - s1;\n"
		"\tbool big = (s & 32u) != 0u;\n"
		"\tuint lo = big ? (ah >> s1) : ((al >> s1) | ((ah << 1) << n1));\n"
		"\tuint hi = big ? 0u : (ah >> s1);\n"
		"\treturn int2(as_type<int>(lo), as_type<int>(hi));\n"
		"}\n"
		"static inline int2 mcc64_sar(int2 a, uint s) {\n"
		"\tuint al = as_type<uint>(a.x), ah = as_type<uint>(a.y);\n"
		"\tint ih = a.y;\n"
		"\tuint s1 = s & 31u, n1 = 31u - s1;\n"
		"\tbool big = (s & 32u) != 0u;\n"
		"\tuint sh = as_type<uint>(ih >> int(s1));\n"
		"\tuint lo = big ? sh : ((al >> s1) | ((ah << 1) << n1));\n"
		"\tuint hi = big ? as_type<uint>(ih >> 31) : sh;\n"
		"\treturn int2(as_type<int>(lo), as_type<int>(hi));\n"
		"}\n"
		"static inline bool mcc64_nz(int2 a) { return (a.x | a.y) != 0; }\n"
		"static inline bool mcc64_eq(int2 a, int2 b) {\n"
		"\treturn a.x == b.x && a.y == b.y;\n"
		"}\n"
		"static inline bool mcc64_ult(int2 a, int2 b) {\n"
		"\tuint ah = as_type<uint>(a.y), bh = as_type<uint>(b.y);\n"
		"\treturn ah < bh ||\n"
		"\t\t\t (ah == bh && as_type<uint>(a.x) < as_type<uint>(b.x));\n"
		"}\n"
		"static inline bool mcc64_slt(int2 a, int2 b) {\n"
		"\treturn a.y < b.y ||\n"
		"\t\t\t (a.y == b.y && as_type<uint>(a.x) < as_type<uint>(b.x));\n"
		"}\n"
		"static inline bool mcc64_addok(int2 a, int2 b, int2 r) {\n"
		"\treturn ((a.y ^ r.y) & (b.y ^ r.y)) >= 0;\n"
		"}\n"
		"static inline bool mcc64_subok(int2 a, int2 b, int2 r) {\n"
		"\treturn ((a.y ^ b.y) & (a.y ^ r.y)) >= 0;\n"
		"}\n"
		"static inline int2 mcc64_udiv(int2 a, int2 b) {\n"
		"\tuint al = as_type<uint>(a.x), ah = as_type<uint>(a.y);\n"
		"\tuint bl = as_type<uint>(b.x), bh = as_type<uint>(b.y);\n"
		"\tuint rl = 0u, rh = 0u, ql = 0u, qh = 0u;\n"
		"\tfor (uint i = 0u; i < 64u; i++) {\n"
		"\t\tuint msb = ah >> 31;\n"
		"\t\tuint nal = al << 1, nah = (ah << 1) | (al >> 31);\n"
		"\t\tuint nrl = (rl << 1) | msb, nrh = (rh << 1) | (rl >> 31);\n"
		"\t\tbool ge = nrh > bh || (nrh == bh && nrl >= bl);\n"
		"\t\tuint brw = nrl < bl ? 1u : 0u;\n"
		"\t\tuint dl = nrl - bl, dh = nrh - bh - brw;\n"
		"\t\tuint nql = (ql << 1) | (ge ? 1u : 0u), nqh = (qh << 1) | (ql >> 31);\n"
		"\t\trl = ge ? dl : nrl;\n"
		"\t\trh = ge ? dh : nrh;\n"
		"\t\tql = nql;\n"
		"\t\tqh = nqh;\n"
		"\t\tal = nal;\n"
		"\t\tah = nah;\n"
		"\t}\n"
		"\treturn int2(as_type<int>(ql), as_type<int>(qh));\n"
		"}\n"
		"static inline int2 mccf64_round(uint s, int ex, uint rh, uint rl) {\n"
		"\tif (ex >= 0x7ff)\n"
		"\t\treturn int2(0, as_type<int>((s << 31) | 0x7ff00000u));\n"
		"\tif (ex <= 0) {\n"
		"\t\tuint sh = uint(1 - ex);\n"
		"\t\tif (sh < 32u) {\n"
		"\t\t\tuint lost = rl << (32u - sh);\n"
		"\t\t\trl = (rl >> sh) | (rh << (32u - sh)) | (lost != 0u ? 1u : 0u);\n"
		"\t\t\trh = rh >> sh;\n"
		"\t\t} else if (sh < 63u) {\n"
		"\t\t\tuint s2 = sh - 32u;\n"
		"\t\t\tuint lost = rl | (s2 != 0u ? (rh << (32u - s2)) : 0u);\n"
		"\t\t\trl = (rh >> s2) | (lost != 0u ? 1u : 0u);\n"
		"\t\t\trh = 0u;\n"
		"\t\t} else {\n"
		"\t\t\trl = (rh | rl) != 0u ? 1u : 0u;\n"
		"\t\t\trh = 0u;\n"
		"\t\t}\n"
		"\t\tex = 1;\n"
		"\t}\n"
		"\tuint rb = rl & 0x3ffu;\n"
		"\tuint ml = (rl >> 10) | (rh << 22);\n"
		"\tuint mh = rh >> 10;\n"
		"\tuint inc = (rb > 0x200u || (rb == 0x200u && (ml & 1u) != 0u)) ? 1u : 0u;\n"
		"\tuint pl = ml + inc;\n"
		"\tuint ph = mh + (pl < ml ? 1u : 0u) + (uint(ex - 1) << 20) + (s << 31);\n"
		"\treturn int2(as_type<int>(pl), as_type<int>(ph));\n"
		"}\n"
		"static inline int2 mccf64_addsub(int2 A, int2 B, uint flip) {\n"
		"\tuint ah = as_type<uint>(A.y), al = as_type<uint>(A.x);\n"
		"\tuint bh = as_type<uint>(B.y), bl = as_type<uint>(B.x);\n"
		"\tuint sa = ah >> 31, sb = (bh >> 31) ^ flip;\n"
		"\tuint ea = (ah >> 20) & 0x7ffu, eb = (bh >> 20) & 0x7ffu;\n"
		"\tuint mah = ah & 0xfffffu, mal = al;\n"
		"\tuint mbh = bh & 0xfffffu, mbl = bl;\n"
		"\tif (ea == 0x7ffu || eb == 0x7ffu) {\n"
		"\t\tbool na = ea == 0x7ffu && (mah | mal) != 0u;\n"
		"\t\tbool nb = eb == 0x7ffu && (mbh | mbl) != 0u;\n"
		"\t\tif (na && (mah & 0x80000u) == 0u)\n"
		"\t\t\treturn int2(as_type<int>(al), as_type<int>(ah | 0x80000u));\n"
		"\t\tif (nb && (mbh & 0x80000u) == 0u)\n"
		"\t\t\treturn int2(as_type<int>(bl), as_type<int>(bh | 0x80000u));\n"
		"\t\tif (na)\n"
		"\t\t\treturn A;\n"
		"\t\tif (nb)\n"
		"\t\t\treturn B;\n"
		"\t\tif (ea == 0x7ffu && eb == 0x7ffu && sa != sb)\n"
		"\t\t\treturn int2(0, as_type<int>(0x7ff80000u));\n"
		"\t\tuint si = ea == 0x7ffu ? sa : sb;\n"
		"\t\treturn int2(0, as_type<int>((si << 31) | 0x7ff00000u));\n"
		"\t}\n"
		"\tint exa = ea == 0u ? 1 : int(ea), exb = eb == 0u ? 1 : int(eb);\n"
		"\tif (ea != 0u)\n"
		"\t\tmah |= 0x100000u;\n"
		"\tif (eb != 0u)\n"
		"\t\tmbh |= 0x100000u;\n"
		"\tuint gah = (mah << 10) | (mal >> 22), gal = mal << 10;\n"
		"\tuint gbh = (mbh << 10) | (mbl >> 22), gbl = mbl << 10;\n"
		"\tif (exa < exb ||\n"
		"\t\t\t(exa == exb && (gah < gbh || (gah == gbh && gal < gbl)))) {\n"
		"\t\tint ti = exa; exa = exb; exb = ti;\n"
		"\t\tuint t = gah; gah = gbh; gbh = t;\n"
		"\t\tt = gal; gal = gbl; gbl = t;\n"
		"\t\tt = sa; sa = sb; sb = t;\n"
		"\t}\n"
		"\tuint d = uint(exa - exb);\n"
		"\tif (d != 0u) {\n"
		"\t\tif (d < 32u) {\n"
		"\t\t\tuint lost = gbl << (32u - d);\n"
		"\t\t\tgbl = (gbl >> d) | (gbh << (32u - d)) | (lost != 0u ? 1u : 0u);\n"
		"\t\t\tgbh = gbh >> d;\n"
		"\t\t} else if (d < 63u) {\n"
		"\t\t\tuint sh = d - 32u;\n"
		"\t\t\tuint lost = gbl | (sh != 0u ? (gbh << (32u - sh)) : 0u);\n"
		"\t\t\tgbl = (gbh >> sh) | (lost != 0u ? 1u : 0u);\n"
		"\t\t\tgbh = 0u;\n"
		"\t\t} else {\n"
		"\t\t\tgbl = (gbh | gbl) != 0u ? 1u : 0u;\n"
		"\t\t\tgbh = 0u;\n"
		"\t\t}\n"
		"\t}\n"
		"\tuint rh, rl;\n"
		"\tint ex = exa;\n"
		"\tif (sa == sb) {\n"
		"\t\trl = gal + gbl;\n"
		"\t\trh = gah + gbh + (rl < gal ? 1u : 0u);\n"
		"\t\tif ((rh & 0x80000000u) != 0u) {\n"
		"\t\t\tuint j = rl & 1u;\n"
		"\t\t\trl = (rl >> 1) | (rh << 31) | j;\n"
		"\t\t\trh = rh >> 1;\n"
		"\t\t\tex += 1;\n"
		"\t\t}\n"
		"\t} else {\n"
		"\t\tif (gah == gbh && gal == gbl)\n"
		"\t\t\treturn int2(0, 0);\n"
		"\t\trl = gal - gbl;\n"
		"\t\trh = gah - gbh - (gal < gbl ? 1u : 0u);\n"
		"\t\tuint k = rh != 0u ? (clz(rh) - 1u) : (31u + clz(rl));\n"
		"\t\tif (k != 0u) {\n"
		"\t\t\tif (k < 32u) {\n"
		"\t\t\t\trh = (rh << k) | (rl >> (32u - k));\n"
		"\t\t\t\trl = rl << k;\n"
		"\t\t\t} else {\n"
		"\t\t\t\trh = rl << (k - 32u);\n"
		"\t\t\t\trl = 0u;\n"
		"\t\t\t}\n"
		"\t\t\tex -= int(k);\n"
		"\t\t}\n"
		"\t}\n"
		"\treturn mccf64_round(sa, ex, rh, rl);\n"
		"}\n"
		"static inline int2 mccf64_mul(int2 A, int2 B) {\n"
		"\tuint ah = as_type<uint>(A.y), al = as_type<uint>(A.x);\n"
		"\tuint bh = as_type<uint>(B.y), bl = as_type<uint>(B.x);\n"
		"\tuint sa = ah >> 31, sb = bh >> 31, s = sa ^ sb;\n"
		"\tuint ea = (ah >> 20) & 0x7ffu, eb = (bh >> 20) & 0x7ffu;\n"
		"\tuint mah = ah & 0xfffffu, mal = al;\n"
		"\tuint mbh = bh & 0xfffffu, mbl = bl;\n"
		"\tif (ea == 0x7ffu || eb == 0x7ffu) {\n"
		"\t\tbool na = ea == 0x7ffu && (mah | mal) != 0u;\n"
		"\t\tbool nb = eb == 0x7ffu && (mbh | mbl) != 0u;\n"
		"\t\tif (na && (mah & 0x80000u) == 0u)\n"
		"\t\t\treturn int2(as_type<int>(al), as_type<int>(ah | 0x80000u));\n"
		"\t\tif (nb && (mbh & 0x80000u) == 0u)\n"
		"\t\t\treturn int2(as_type<int>(bl), as_type<int>(bh | 0x80000u));\n"
		"\t\tif (na)\n"
		"\t\t\treturn A;\n"
		"\t\tif (nb)\n"
		"\t\t\treturn B;\n"
		"\t\tif ((ea == 0x7ffu && eb == 0u && (mbh | mbl) == 0u) ||\n"
		"\t\t\t\t(eb == 0x7ffu && ea == 0u && (mah | mal) == 0u))\n"
		"\t\t\treturn int2(0, as_type<int>(0x7ff80000u));\n"
		"\t\treturn int2(0, as_type<int>((s << 31) | 0x7ff00000u));\n"
		"\t}\n"
		"\tif ((ea == 0u && (mah | mal) == 0u) || (eb == 0u && (mbh | mbl) == 0u))\n"
		"\t\treturn int2(0, as_type<int>(s << 31));\n"
		"\tint exa, exb;\n"
		"\tif (ea == 0u) {\n"
		"\t\tuint k = mah != 0u ? (clz(mah) - 11u) : (21u + clz(mal));\n"
		"\t\tif (k < 32u) {\n"
		"\t\t\tmah = (mah << k) | (mal >> 1 >> (31u - k));\n"
		"\t\t\tmal = mal << k;\n"
		"\t\t} else {\n"
		"\t\t\tmah = mal << (k - 32u);\n"
		"\t\t\tmal = 0u;\n"
		"\t\t}\n"
		"\t\texa = 1 - int(k);\n"
		"\t} else {\n"
		"\t\texa = int(ea);\n"
		"\t\tmah |= 0x100000u;\n"
		"\t}\n"
		"\tif (eb == 0u) {\n"
		"\t\tuint k = mbh != 0u ? (clz(mbh) - 11u) : (21u + clz(mbl));\n"
		"\t\tif (k < 32u) {\n"
		"\t\t\tmbh = (mbh << k) | (mbl >> 1 >> (31u - k));\n"
		"\t\t\tmbl = mbl << k;\n"
		"\t\t} else {\n"
		"\t\t\tmbh = mbl << (k - 32u);\n"
		"\t\t\tmbl = 0u;\n"
		"\t\t}\n"
		"\t\texb = 1 - int(k);\n"
		"\t} else {\n"
		"\t\texb = int(eb);\n"
		"\t\tmbh |= 0x100000u;\n"
		"\t}\n"
		"\tuint p00l = mal * mbl, p00h = mulhi(mal, mbl);\n"
		"\tuint p01l = mal * mbh, p01h = mulhi(mal, mbh);\n"
		"\tuint p10l = mah * mbl, p10h = mulhi(mah, mbl);\n"
		"\tuint p11l = mah * mbh, p11h = mulhi(mah, mbh);\n"
		"\tuint r0 = p00l;\n"
		"\tuint r1 = p00h + p01l;\n"
		"\tuint c1 = r1 < p00h ? 1u : 0u;\n"
		"\tuint t = r1 + p10l;\n"
		"\tc1 += t < r1 ? 1u : 0u;\n"
		"\tr1 = t;\n"
		"\tuint r2 = p01h + p10h;\n"
		"\tuint c2 = r2 < p01h ? 1u : 0u;\n"
		"\tt = r2 + p11l;\n"
		"\tc2 += t < r2 ? 1u : 0u;\n"
		"\tr2 = t;\n"
		"\tt = r2 + c1;\n"
		"\tc2 += t < r2 ? 1u : 0u;\n"
		"\tr2 = t;\n"
		"\tuint r3 = p11h + c2;\n"
		"\tuint big = (r3 & 0x200u) != 0u ? 1u : 0u;\n"
		"\tuint sh = 10u + big;\n"
		"\tuint rl = (r1 >> sh) | (r2 << (32u - sh));\n"
		"\tuint rh = (r2 >> sh) | (r3 << (32u - sh));\n"
		"\tuint lost = r0 | (r1 << (32u - sh));\n"
		"\trl |= lost != 0u ? 1u : 0u;\n"
		"\tint ex = exa + exb - 0x3ff + int(big);\n"
		"\treturn mccf64_round(s, ex, rh, rl);\n"
		"}\n"
		"static inline int mccf64_cmp(int2 a, int2 b) {\n"
		"\tbool na = (a.y & 0x7ff00000) == 0x7ff00000 &&\n"
		"\t\t\t\t\t\t((a.y & 0x000fffff) | a.x) != 0;\n"
		"\tbool nb = (b.y & 0x7ff00000) == 0x7ff00000 &&\n"
		"\t\t\t\t\t\t((b.y & 0x000fffff) | b.x) != 0;\n"
		"\tif (na || nb)\n"
		"\t\treturn 2;\n"
		"\tif ((a.x | (a.y & 0x7fffffff)) == 0)\n"
		"\t\ta = int2(0, 0);\n"
		"\tif ((b.x | (b.y & 0x7fffffff)) == 0)\n"
		"\t\tb = int2(0, 0);\n"
		"\tuint ah = a.y < 0 ? ~as_type<uint>(a.y)\n"
		"\t\t\t\t\t\t\t\t\t\t: (as_type<uint>(a.y) ^ 0x80000000u);\n"
		"\tuint al = a.y < 0 ? ~as_type<uint>(a.x) : as_type<uint>(a.x);\n"
		"\tuint bh = b.y < 0 ? ~as_type<uint>(b.y)\n"
		"\t\t\t\t\t\t\t\t\t\t: (as_type<uint>(b.y) ^ 0x80000000u);\n"
		"\tuint bl = b.y < 0 ? ~as_type<uint>(b.x) : as_type<uint>(b.x);\n"
		"\tif (ah != bh)\n"
		"\t\treturn ah < bh ? -1 : 1;\n"
		"\tif (al != bl)\n"
		"\t\treturn al < bl ? -1 : 1;\n"
		"\treturn 0;\n"
		"}\n"
		"\n";

/* Buffer 0 is read-only unless the module actually stores through it. Dropping
 * `const` unconditionally would make inb and outb mutually aliasable to the
 * Metal compiler and perturb codegen for every expression kernel, and the only
 * green evidence this backend has is mslgate's per-value differential. The
 * signature is therefore chosen per module from MslMod.wrote_in, so a module
 * that stores nothing emits the byte-identical text it emitted before. */
static const char msl_kernel_ro[] =
		"kernel void mcc_main(device const int *inb [[buffer(0)]],\n"
		"                     device int *outb [[buffer(1)]],\n"
		"                     uint gid [[thread_position_in_grid]]) {\n";
static const char msl_kernel_rw[] =
		"kernel void mcc_main(device int *inb [[buffer(0)]],\n"
		"                     device int *outb [[buffer(1)]],\n"
		"                     uint gid [[thread_position_in_grid]]) {\n";

static void msl_module_begin(MslMod *m, int nlive) {
	memset(m, 0, sizeof *m);
	m->next_id = 1;
	m->indent = 1;
	(void)nlive;
}

static char *msl_module_finish(MslMod *m, int *nbytes) {
	const char *kh = m->wrote_in ? msl_kernel_rw : msl_kernel_ro;
	int pre = (int)(sizeof msl_prelude - 1);
	int khn = (int)strlen(kh);
	int total = pre + khn + m->decls.n + m->body.n + 2;
	char *s = (char *)MSL_MALLOC((size_t)total + 1);
	int i = 0;
	memcpy(s + i, msl_prelude, (size_t)pre);
	i += pre;
	memcpy(s + i, kh, (size_t)khn);
	i += khn;
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
	SpvOpTypeInt = 21, SpvOpTypeFloat = 22,
	SpvOpTypeVector = 23, SpvOpTypeRuntimeArray = 29,
	SpvOpFNegate = 127, SpvOpFAdd = 129, SpvOpFSub = 131, SpvOpFMul = 133,
	SpvOpFOrdEqual = 180, SpvOpFOrdNotEqual = 182, SpvOpFOrdLessThan = 184,
	SpvOpFOrdGreaterThan = 186, SpvOpFOrdLessThanEqual = 188,
	SpvOpFOrdGreaterThanEqual = 190, SpvOpFUnordNotEqual = 183,
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
	SpvOpBitcast = 124, SpvOpSMulExtended = 152, SpvOpUMulExtended = 151,
	SpvOpFunctionParameter = 55, SpvOpFunctionCall = 57,
	SpvOpCompositeConstruct = 80, SpvOpCopyObject = 83,
	SpvOpLoopMerge = 246, SpvOpReturnValue = 254,
	SpvOpLogicalNotEqual = 165,
	SpvOpLogicalOr = 166, SpvOpLogicalAnd = 167, SpvOpLogicalNot = 168,
	SpvOpNot = 200, SpvOpPhi = 245, SpvOpSelectionMerge = 247, SpvOpLabel = 248,
	SpvOpBranch = 249, SpvOpBranchConditional = 250, SpvOpReturn = 253,
	SpvOpAtomicAnd = 240, SpvOpAtomicOr = 241
};

enum {
	SpvDecBlock = 2, SpvDecArrayStride = 6, SpvDecBuiltIn = 11,
	SpvDecNoContraction = 42,
	SpvDecBinding = 33, SpvDecDescriptorSet = 34, SpvDecOffset = 35
};

enum { SpvStorageInput = 1, SpvStorageStorageBuffer = 12, SpvStorageFunction = 7 };
enum { SpvBuiltInGlobalInvocationId = 28 };
enum { SpvExecModelGLCompute = 5, SpvExecModeLocalSize = 17 };
enum { SpvCapShader = 1, SpvCapFloat64 = 10 };

#define SPV_LOCAL_SIZE MCC_GPU_LOCAL_SIZE
#define SPV_MAX_CONST 512

typedef struct SpvWords {
	uint32_t *w;
	int n, cap;
} SpvWords;

typedef struct SpvV {
	uint32_t id;
	int w64;
	int uns;
	int f64;
} SpvV;

typedef struct SpvMod {
	SpvWords caps, pre, types, body;
	uint32_t next_id;
	uint32_t id_void, id_fnvoid, id_bool, id_int, id_uint, id_v3uint;
	uint32_t id_ptr_in_v3uint, id_gid;
	uint32_t id_rt, id_buf, id_ptr_buf, id_ptr_sb_int, id_pair;
	uint32_t id_u2, id_upair, id_fn_u2, id_udiv;
	uint32_t id_in, id_out, id_mem, id_main, id_nlive;
	uint32_t cur_label;
	uint32_t def;
	uint32_t lane;
	int32_t cval[SPV_MAX_CONST];
	uint32_t cid[SPV_MAX_CONST];
	int ncached;
	uint32_t ucval[SPV_MAX_CONST];
	uint32_t ucid[SPV_MAX_CONST];
	int nucached;
	uint32_t id_f64;
	uint64_t fcval[SPV_MAX_CONST];
	uint32_t fcid[SPV_MAX_CONST];
	int nfcached;
	int used_f64;
	int failed;
	int64_t mem_base;
	uint32_t mem_nbyte;
	int mem_used;
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

static uint32_t spv_uintc(SpvMod *m, uint32_t v) {
	int i;
	uint32_t id;
	for (i = 0; i < m->nucached; i++)
		if (m->ucval[i] == v)
			return m->ucid[i];
	if (m->nucached == SPV_MAX_CONST) {
		m->failed = 1;
		return m->ucid[0];
	}
	id = spv_id(m);
	spvw_op(&m->types, SpvOpConstant, 4);
	spvw_put(&m->types, m->id_uint);
	spvw_put(&m->types, id);
	spvw_put(&m->types, v);
	m->ucval[m->nucached] = v;
	m->ucid[m->nucached] = id;
	m->nucached++;
	return id;
}

static uint32_t spv_f64_type(SpvMod *m) {
	if (!m->id_f64) {
		m->id_f64 = spv_id(m);
		m->used_f64 = 1;
		spvw_op(&m->caps, SpvOpCapability, 2);
		spvw_put(&m->caps, SpvCapFloat64);
		spvw_op(&m->types, SpvOpTypeFloat, 3);
		spvw_put(&m->types, m->id_f64);
		spvw_put(&m->types, 64);
	}
	return m->id_f64;
}

static uint32_t spv_f64_const(SpvMod *m, uint64_t bits) {
	int i;
	uint32_t id, ft;
	for (i = 0; i < m->nfcached; i++)
		if (m->fcval[i] == bits)
			return m->fcid[i];
	if (m->nfcached == SPV_MAX_CONST) {
		m->failed = 1;
		return m->fcid[0];
	}
	ft = spv_f64_type(m);
	id = spv_id(m);
	spvw_op(&m->types, SpvOpConstant, 5);
	spvw_put(&m->types, ft);
	spvw_put(&m->types, id);
	spvw_put(&m->types, (uint32_t)(bits & 0xFFFFFFFFu));
	spvw_put(&m->types, (uint32_t)(bits >> 32));
	m->fcval[m->nfcached] = bits;
	m->fcid[m->nfcached] = id;
	m->nfcached++;
	return id;
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

static uint32_t spv_ex(SpvMod *m, uint32_t rtype, uint32_t comp, uint32_t idx) {
	uint32_t id = spv_id(m);
	spvw_op(&m->body, SpvOpCompositeExtract, 5);
	spvw_put(&m->body, rtype);
	spvw_put(&m->body, id);
	spvw_put(&m->body, comp);
	spvw_put(&m->body, idx);
	return id;
}

static uint32_t spv_u2(SpvMod *m, uint32_t lo, uint32_t hi) {
	uint32_t id = spv_id(m);
	spvw_op(&m->body, SpvOpCompositeConstruct, 5);
	spvw_put(&m->body, m->id_u2);
	spvw_put(&m->body, id);
	spvw_put(&m->body, lo);
	spvw_put(&m->body, hi);
	return id;
}

static uint32_t spv_lo(SpvMod *m, uint32_t p) {
	return spv_ex(m, m->id_uint, p, 0);
}

static uint32_t spv_hi(SpvMod *m, uint32_t p) {
	return spv_ex(m, m->id_uint, p, 1);
}

static uint32_t spv_f64_unpack(SpvMod *m, uint32_t pair) {
	uint32_t ft = spv_f64_type(m);
	return spv_emit2(m, SpvOpBitcast, ft, pair);
}

static uint32_t spv_f64_pack(SpvMod *m, uint32_t fid) {
	return spv_emit2(m, SpvOpBitcast, m->id_u2, fid);
}

static uint32_t spv_f64_arith(SpvMod *m, int opcode, uint32_t a, uint32_t b) {
	uint32_t ft = spv_f64_type(m);
	uint32_t r = spv_emit3(m, opcode, ft, a, b);
	spvw_op(&m->pre, SpvOpDecorate, 3);
	spvw_put(&m->pre, r);
	spvw_put(&m->pre, SpvDecNoContraction);
	return r;
}

/* Not OpFNegate: negation must be the raw sign-bit flip on every device, and
 * on NaNs the hardware is allowed to canonicalize instead -- measured on an
 * RTX 2060, OpFNegate returns a NaN input unchanged where the CPU reference
 * flips the sign, one bit per 65,812-point case. The integer XOR is the same
 * lowering the MSL arm's bits-pair negate uses, exact everywhere. */
static uint32_t spv_f64_neg(SpvMod *m, uint32_t a) {
	uint32_t p = spv_f64_pack(m, a);
	uint32_t hi = spv_emit3(m, SpvOpBitwiseXor, m->id_uint, spv_hi(m, p),
													spv_uintc(m, 0x80000000u));
	return spv_f64_unpack(m, spv_u2(m, spv_lo(m, p), hi));
}

static uint32_t spv_uop(SpvMod *m, int op, uint32_t a, uint32_t b) {
	return spv_emit3(m, op, m->id_uint, a, b);
}

static uint32_t spv_ucmp(SpvMod *m, int op, uint32_t a, uint32_t b) {
	return spv_emit3(m, op, m->id_bool, a, b);
}

static uint32_t spv_usel(SpvMod *m, uint32_t c, uint32_t x, uint32_t y) {
	return spv_emit4(m, SpvOpSelect, m->id_uint, c, x, y);
}

static uint32_t spv_and(SpvMod *m, uint32_t a, uint32_t b) {
	return spv_emit3(m, SpvOpLogicalAnd, m->id_bool, a, b);
}

static void spv_emit_udiv64(SpvMod *m) {
	uint32_t pa = spv_id(m), pb = spv_id(m);
	uint32_t l_head = spv_id(m), l_body = spv_id(m), l_cont = spv_id(m);
	uint32_t l_merge = spv_id(m);
	uint32_t ph[7], nx[7], iv[7], res[7];
	uint32_t al, ah, bl, bh, z, c1, c31, entry, cond;
	int i;
	spvw_op(&m->body, SpvOpFunction, 5);
	spvw_put(&m->body, m->id_u2);
	spvw_put(&m->body, m->id_udiv);
	spvw_put(&m->body, 0);
	spvw_put(&m->body, m->id_fn_u2);
	spvw_op(&m->body, SpvOpFunctionParameter, 3);
	spvw_put(&m->body, m->id_u2);
	spvw_put(&m->body, pa);
	spvw_op(&m->body, SpvOpFunctionParameter, 3);
	spvw_put(&m->body, m->id_u2);
	spvw_put(&m->body, pb);
	entry = spv_label(m);
	al = spv_lo(m, pa);
	ah = spv_hi(m, pa);
	bl = spv_lo(m, pb);
	bh = spv_hi(m, pb);
	z = spv_uintc(m, 0);
	c1 = spv_uintc(m, 1);
	c31 = spv_uintc(m, 31);
	iv[0] = al;
	iv[1] = ah;
	for (i = 2; i < 7; i++)
		iv[i] = z;
	for (i = 0; i < 7; i++) {
		ph[i] = spv_id(m);
		nx[i] = spv_id(m);
	}
	spvw_op(&m->body, SpvOpBranch, 2);
	spvw_put(&m->body, l_head);

	spv_label_at(m, l_head);
	for (i = 0; i < 7; i++) {
		spvw_op(&m->body, SpvOpPhi, 7);
		spvw_put(&m->body, m->id_uint);
		spvw_put(&m->body, ph[i]);
		spvw_put(&m->body, iv[i]);
		spvw_put(&m->body, entry);
		spvw_put(&m->body, nx[i]);
		spvw_put(&m->body, l_cont);
	}
	cond = spv_ucmp(m, SpvOpULessThan, ph[6], spv_uintc(m, 64));
	spvw_op(&m->body, SpvOpLoopMerge, 4);
	spvw_put(&m->body, l_merge);
	spvw_put(&m->body, l_cont);
	spvw_put(&m->body, 0);
	spvw_op(&m->body, SpvOpBranchConditional, 4);
	spvw_put(&m->body, cond);
	spvw_put(&m->body, l_body);
	spvw_put(&m->body, l_merge);

	spv_label_at(m, l_body);
	{
		uint32_t msb = spv_uop(m, SpvOpShiftRightLogical, ph[1], c31);
		uint32_t nal = spv_uop(m, SpvOpShiftLeftLogical, ph[0], c1);
		uint32_t nah =
				spv_uop(m, SpvOpBitwiseOr,
								spv_uop(m, SpvOpShiftLeftLogical, ph[1], c1),
								spv_uop(m, SpvOpShiftRightLogical, ph[0], c31));
		uint32_t nrl = spv_uop(m, SpvOpBitwiseOr,
													 spv_uop(m, SpvOpShiftLeftLogical, ph[2], c1), msb);
		uint32_t nrh =
				spv_uop(m, SpvOpBitwiseOr,
								spv_uop(m, SpvOpShiftLeftLogical, ph[3], c1),
								spv_uop(m, SpvOpShiftRightLogical, ph[2], c31));
		uint32_t gt = spv_ucmp(m, SpvOpUGreaterThan, nrh, bh);
		uint32_t eqh = spv_ucmp(m, SpvOpIEqual, nrh, bh);
		uint32_t gel = spv_ucmp(m, SpvOpUGreaterThanEqual, nrl, bl);
		uint32_t ge = spv_or(m, gt, spv_and(m, eqh, gel));
		uint32_t brw =
				spv_usel(m, spv_ucmp(m, SpvOpULessThan, nrl, bl), c1, z);
		uint32_t dl = spv_uop(m, SpvOpISub, nrl, bl);
		uint32_t dh =
				spv_uop(m, SpvOpISub, spv_uop(m, SpvOpISub, nrh, bh), brw);
		uint32_t nql = spv_uop(m, SpvOpBitwiseOr,
													 spv_uop(m, SpvOpShiftLeftLogical, ph[4], c1),
													 spv_usel(m, ge, c1, z));
		uint32_t nqh =
				spv_uop(m, SpvOpBitwiseOr,
								spv_uop(m, SpvOpShiftLeftLogical, ph[5], c1),
								spv_uop(m, SpvOpShiftRightLogical, ph[4], c31));
		res[0] = nal;
		res[1] = nah;
		res[2] = spv_usel(m, ge, dl, nrl);
		res[3] = spv_usel(m, ge, dh, nrh);
		res[4] = nql;
		res[5] = nqh;
		res[6] = spv_uop(m, SpvOpIAdd, ph[6], c1);
		for (i = 0; i < 7; i++) {
			spvw_op(&m->body, SpvOpCopyObject, 4);
			spvw_put(&m->body, m->id_uint);
			spvw_put(&m->body, nx[i]);
			spvw_put(&m->body, res[i]);
		}
	}
	spvw_op(&m->body, SpvOpBranch, 2);
	spvw_put(&m->body, l_cont);
	spv_label_at(m, l_cont);
	spvw_op(&m->body, SpvOpBranch, 2);
	spvw_put(&m->body, l_head);
	spv_label_at(m, l_merge);
	{
		uint32_t q = spv_u2(m, ph[4], ph[5]);
		spvw_op(&m->body, SpvOpReturnValue, 2);
		spvw_put(&m->body, q);
	}
	spvw_op(&m->body, SpvOpFunctionEnd, 1);
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
	m->id_u2 = spv_id(m);
	m->id_upair = spv_id(m);
	m->id_fn_u2 = spv_id(m);
	m->id_udiv = spv_id(m);
	m->id_in = spv_id(m);
	m->id_out = spv_id(m);
	m->id_mem = spv_id(m);
	m->id_main = spv_id(m);
	m->id_nlive = (uint32_t)nlive;

	spvw_op(&m->caps, SpvOpCapability, 2);
	spvw_put(&m->caps, SpvCapShader);
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
	spvw_op(&m->pre, SpvOpDecorate, 4);
	spvw_put(&m->pre, m->id_mem);
	spvw_put(&m->pre, SpvDecDescriptorSet);
	spvw_put(&m->pre, 0);
	spvw_op(&m->pre, SpvOpDecorate, 4);
	spvw_put(&m->pre, m->id_mem);
	spvw_put(&m->pre, SpvDecBinding);
	spvw_put(&m->pre, 2);

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
	spvw_op(&m->types, SpvOpVariable, 4);
	spvw_put(&m->types, m->id_ptr_buf);
	spvw_put(&m->types, m->id_mem);
	spvw_put(&m->types, SpvStorageStorageBuffer);
	spvw_op(&m->types, SpvOpTypePointer, 4);
	spvw_put(&m->types, m->id_ptr_sb_int);
	spvw_put(&m->types, SpvStorageStorageBuffer);
	spvw_put(&m->types, m->id_int);
	spvw_op(&m->types, SpvOpTypeStruct, 4);
	spvw_put(&m->types, m->id_pair);
	spvw_put(&m->types, m->id_int);
	spvw_put(&m->types, m->id_int);
	spvw_op(&m->types, SpvOpTypeVector, 4);
	spvw_put(&m->types, m->id_u2);
	spvw_put(&m->types, m->id_uint);
	spvw_put(&m->types, 2);
	spvw_op(&m->types, SpvOpTypeStruct, 4);
	spvw_put(&m->types, m->id_upair);
	spvw_put(&m->types, m->id_uint);
	spvw_put(&m->types, m->id_uint);
	spvw_op(&m->types, SpvOpTypeFunction, 5);
	spvw_put(&m->types, m->id_fn_u2);
	spvw_put(&m->types, m->id_u2);
	spvw_put(&m->types, m->id_u2);
	spvw_put(&m->types, m->id_u2);
	spv_emit_udiv64(m);
}

static uint32_t spv_load_at(SpvMod *m, uint32_t idx) {
	uint32_t p = spv_id(m);
	spvw_op(&m->body, SpvOpAccessChain, 5 + 1);
	spvw_put(&m->body, m->id_ptr_sb_int);
	spvw_put(&m->body, p);
	spvw_put(&m->body, m->id_in);
	spvw_put(&m->body, spv_const(m, 0));
	spvw_put(&m->body, idx);
	return spv_emit2(m, SpvOpLoad, m->id_int, p);
}

static uint32_t spv_load_live(SpvMod *m, uint32_t base, int k) {
	uint32_t idx = base;
	if (k)
		idx = spv_emit3(m, SpvOpIAdd, m->id_int, base, spv_const(m, k));
	return spv_load_at(m, idx);
}

/* The store counterpart of spv_load_live. The input buffer is a plain
 * StorageBuffer with no NonWritable decoration, so it is already writable --
 * this makes it the device frame, which is what lets a run of statements lower
 * as one kernel instead of one kernel per expression. */
static void spv_store_at_in(SpvMod *m, uint32_t idx, uint32_t val) {
	uint32_t p = spv_id(m);
	spvw_op(&m->body, SpvOpAccessChain, 5 + 1);
	spvw_put(&m->body, m->id_ptr_sb_int);
	spvw_put(&m->body, p);
	spvw_put(&m->body, m->id_in);
	spvw_put(&m->body, spv_const(m, 0));
	spvw_put(&m->body, idx);
	spvw_op(&m->body, SpvOpStore, 3);
	spvw_put(&m->body, p);
	spvw_put(&m->body, val);
}

static void spv_store_live(SpvMod *m, uint32_t base, int k, uint32_t val) {
	uint32_t idx = base;
	if (k)
		idx = spv_emit3(m, SpvOpIAdd, m->id_int, base, spv_const(m, k));
	spv_store_at_in(m, idx, val);
}

static SpvV spv_mk(uint32_t id, int w64, int uns) {
	SpvV v;
	v.id = id;
	v.w64 = w64;
	v.uns = uns;
	v.f64 = 0;
	return v;
}

static void spv_widen(SpvMod *m, SpvV *v) {
	uint32_t lo, hi;
	if (v->f64) {
		v->id = spv_f64_pack(m, v->id);
		v->f64 = 0;
		v->w64 = 1;
		return;
	}
	if (v->w64)
		return;
	lo = spv_emit2(m, SpvOpBitcast, m->id_uint, v->id);
	hi = v->uns ? spv_uintc(m, 0)
							: spv_emit2(m, SpvOpBitcast, m->id_uint,
													spv_emit3(m, SpvOpShiftRightArithmetic, m->id_int,
																		v->id, spv_uconst(m, 31)));
	v->id = spv_u2(m, lo, hi);
	v->w64 = 1;
}

static uint32_t spv_val_lo(SpvMod *m, SpvV v) {
	return v.w64 ? spv_emit2(m, SpvOpBitcast, m->id_int, spv_lo(m, v.id)) : v.id;
}

static uint32_t spv_pair(SpvMod *m, SpvV v) {
	if (v.f64)
		return spv_f64_pack(m, v.id);
	spv_widen(m, &v);
	return v.id;
}

static SpvV spv_mkf(uint32_t id) {
	SpvV v;
	v.id = id;
	v.w64 = 1;
	v.uns = 0;
	v.f64 = 1;
	return v;
}

static SpvV spv_f64_of(SpvMod *m, SpvV v) {
	if (v.f64)
		return v;
	spv_widen(m, &v);
	return spv_mkf(spv_f64_unpack(m, v.id));
}

static SpvV spv_const64(SpvMod *m, int64_t x) {
	uint32_t lo = spv_uintc(m, (uint32_t)(uint64_t)x);
	uint32_t hi = spv_uintc(m, (uint32_t)((uint64_t)x >> 32));
	return spv_mk(spv_u2(m, lo, hi), 1, 0);
}

static uint32_t spv_bool_of_v(SpvMod *m, SpvV v) {
	uint32_t o;
	if (v.f64)
		return spv_emit3(m, SpvOpFUnordNotEqual, m->id_bool, v.id,
										 spv_f64_const(m, 0));
	if (!v.w64)
		return spv_bool_of(m, v.id);
	o = spv_uop(m, SpvOpBitwiseOr, spv_lo(m, v.id), spv_hi(m, v.id));
	return spv_ucmp(m, SpvOpINotEqual, o, spv_uintc(m, 0));
}

static SpvV spv_add64(SpvMod *m, SpvV a, SpvV b, int uns) {
	uint32_t al = spv_lo(m, a.id), ah = spv_hi(m, a.id);
	uint32_t bl = spv_lo(m, b.id), bh = spv_hi(m, b.id);
	uint32_t rl = spv_uop(m, SpvOpIAdd, al, bl);
	uint32_t c = spv_usel(m, spv_ucmp(m, SpvOpULessThan, rl, al),
												spv_uintc(m, 1), spv_uintc(m, 0));
	uint32_t rh = spv_uop(m, SpvOpIAdd, spv_uop(m, SpvOpIAdd, ah, bh), c);
	return spv_mk(spv_u2(m, rl, rh), 1, uns);
}

static SpvV spv_sub64(SpvMod *m, SpvV a, SpvV b, int uns) {
	uint32_t al = spv_lo(m, a.id), ah = spv_hi(m, a.id);
	uint32_t bl = spv_lo(m, b.id), bh = spv_hi(m, b.id);
	uint32_t rl = spv_uop(m, SpvOpISub, al, bl);
	uint32_t w = spv_usel(m, spv_ucmp(m, SpvOpULessThan, al, bl),
												spv_uintc(m, 1), spv_uintc(m, 0));
	uint32_t rh = spv_uop(m, SpvOpISub, spv_uop(m, SpvOpISub, ah, bh), w);
	return spv_mk(spv_u2(m, rl, rh), 1, uns);
}

static SpvV spv_mul64(SpvMod *m, SpvV a, SpvV b, int uns) {
	uint32_t al = spv_lo(m, a.id), ah = spv_hi(m, a.id);
	uint32_t bl = spv_lo(m, b.id), bh = spv_hi(m, b.id);
	uint32_t wide = spv_emit3(m, SpvOpUMulExtended, m->id_upair, al, bl);
	uint32_t rl = spv_ex(m, m->id_uint, wide, 0);
	uint32_t rh = spv_uop(m, SpvOpIAdd,
												spv_uop(m, SpvOpIAdd, spv_ex(m, m->id_uint, wide, 1),
																spv_uop(m, SpvOpIMul, al, bh)),
												spv_uop(m, SpvOpIMul, ah, bl));
	return spv_mk(spv_u2(m, rl, rh), 1, uns);
}

static SpvV spv_shift64(SpvMod *m, int kind, SpvV a, uint32_t s, int uns) {
	uint32_t al = spv_lo(m, a.id), ah = spv_hi(m, a.id);
	uint32_t c0 = spv_uintc(m, 0), c1 = spv_uintc(m, 1);
	uint32_t c31 = spv_uintc(m, 31), c32 = spv_uintc(m, 32);
	uint32_t s1 = spv_uop(m, SpvOpBitwiseAnd, s, c31);
	uint32_t n1 = spv_uop(m, SpvOpISub, c31, s1);
	uint32_t big = spv_ucmp(m, SpvOpINotEqual,
													spv_uop(m, SpvOpBitwiseAnd, s, c32), c0);
	uint32_t lo, hi;
	if (kind == SpvOpShiftLeftLogical) {
		uint32_t l1 = spv_uop(m, SpvOpShiftLeftLogical, al, s1);
		uint32_t h1 = spv_uop(
				m, SpvOpBitwiseOr, spv_uop(m, SpvOpShiftLeftLogical, ah, s1),
				spv_uop(m, SpvOpShiftRightLogical,
								spv_uop(m, SpvOpShiftRightLogical, al, c1), n1));
		lo = spv_usel(m, big, c0, l1);
		hi = spv_usel(m, big, l1, h1);
	} else if (kind == SpvOpShiftRightLogical) {
		uint32_t l1 = spv_uop(
				m, SpvOpBitwiseOr, spv_uop(m, SpvOpShiftRightLogical, al, s1),
				spv_uop(m, SpvOpShiftLeftLogical,
								spv_uop(m, SpvOpShiftLeftLogical, ah, c1), n1));
		uint32_t h1 = spv_uop(m, SpvOpShiftRightLogical, ah, s1);
		lo = spv_usel(m, big, h1, l1);
		hi = spv_usel(m, big, c0, h1);
	} else {
		uint32_t ih = spv_emit2(m, SpvOpBitcast, m->id_int, ah);
		uint32_t sh = spv_emit2(
				m, SpvOpBitcast, m->id_uint,
				spv_emit3(m, SpvOpShiftRightArithmetic, m->id_int, ih, s1));
		uint32_t sg = spv_emit2(
				m, SpvOpBitcast, m->id_uint,
				spv_emit3(m, SpvOpShiftRightArithmetic, m->id_int, ih, c31));
		uint32_t l1 = spv_uop(
				m, SpvOpBitwiseOr, spv_uop(m, SpvOpShiftRightLogical, al, s1),
				spv_uop(m, SpvOpShiftLeftLogical,
								spv_uop(m, SpvOpShiftLeftLogical, ah, c1), n1));
		lo = spv_usel(m, big, sh, l1);
		hi = spv_usel(m, big, sg, sh);
	}
	return spv_mk(spv_u2(m, lo, hi), 1, uns);
}

static SpvV spv_bit64(SpvMod *m, int op, SpvV a, SpvV b, int uns) {
	uint32_t lo = spv_uop(m, op, spv_lo(m, a.id), spv_lo(m, b.id));
	uint32_t hi = spv_uop(m, op, spv_hi(m, a.id), spv_hi(m, b.id));
	return spv_mk(spv_u2(m, lo, hi), 1, uns);
}

static uint32_t spv_eq64(SpvMod *m, SpvV a, SpvV b) {
	return spv_and(m, spv_ucmp(m, SpvOpIEqual, spv_lo(m, a.id), spv_lo(m, b.id)),
								 spv_ucmp(m, SpvOpIEqual, spv_hi(m, a.id), spv_hi(m, b.id)));
}

static uint32_t spv_lt64(SpvMod *m, SpvV a, SpvV b, int sgn) {
	uint32_t ah = spv_hi(m, a.id), bh = spv_hi(m, b.id);
	uint32_t hlt = sgn ? spv_emit3(m, SpvOpSLessThan, m->id_bool,
																 spv_emit2(m, SpvOpBitcast, m->id_int, ah),
																 spv_emit2(m, SpvOpBitcast, m->id_int, bh))
										 : spv_ucmp(m, SpvOpULessThan, ah, bh);
	uint32_t heq = spv_ucmp(m, SpvOpIEqual, ah, bh);
	uint32_t llt =
			spv_ucmp(m, SpvOpULessThan, spv_lo(m, a.id), spv_lo(m, b.id));
	return spv_or(m, hlt, spv_and(m, heq, llt));
}

static uint32_t spv_cmp64(SpvMod *m, int code, SpvV a, SpvV b) {
	switch (code) {
	case SpvOpIEqual: return spv_eq64(m, a, b);
	case SpvOpINotEqual: return spv_not(m, spv_eq64(m, a, b));
	case SpvOpULessThan: return spv_lt64(m, a, b, 0);
	case SpvOpUGreaterThanEqual: return spv_not(m, spv_lt64(m, a, b, 0));
	case SpvOpULessThanEqual: return spv_not(m, spv_lt64(m, b, a, 0));
	case SpvOpUGreaterThan: return spv_lt64(m, b, a, 0);
	case SpvOpSLessThan: return spv_lt64(m, a, b, 1);
	case SpvOpSGreaterThanEqual: return spv_not(m, spv_lt64(m, a, b, 1));
	case SpvOpSLessThanEqual: return spv_not(m, spv_lt64(m, b, a, 1));
	default: return spv_lt64(m, b, a, 1);
	}
}

static SpvV spv_sel64(SpvMod *m, uint32_t c, SpvV x, SpvV y) {
	uint32_t lo = spv_usel(m, c, spv_lo(m, x.id), spv_lo(m, y.id));
	uint32_t hi = spv_usel(m, c, spv_hi(m, x.id), spv_hi(m, y.id));
	return spv_mk(spv_u2(m, lo, hi), 1, 0);
}

static SpvV spv_neg64(SpvMod *m, SpvV a) {
	return spv_sub64(m, spv_const64(m, 0), a, 0);
}

static uint32_t spv_sign64(SpvMod *m, SpvV a) {
	return spv_emit3(m, SpvOpSLessThan, m->id_bool,
									 spv_emit2(m, SpvOpBitcast, m->id_int, spv_hi(m, a.id)),
									 spv_const(m, 0));
}

static SpvV spv_udiv64(SpvMod *m, SpvV a, SpvV b, int uns) {
	uint32_t id = spv_id(m);
	spvw_op(&m->body, SpvOpFunctionCall, 6);
	spvw_put(&m->body, m->id_u2);
	spvw_put(&m->body, id);
	spvw_put(&m->body, m->id_udiv);
	spvw_put(&m->body, a.id);
	spvw_put(&m->body, b.id);
	return spv_mk(id, 1, uns);
}

static SpvV spv_sdiv64(SpvMod *m, SpvV a, SpvV b, int uns) {
	uint32_t sa = spv_sign64(m, a), sb = spv_sign64(m, b);
	SpvV na = spv_sel64(m, sa, spv_neg64(m, a), a);
	SpvV nb = spv_sel64(m, sb, spv_neg64(m, b), b);
	SpvV q = spv_udiv64(m, na, nb, 0);
	uint32_t s = spv_emit3(m, SpvOpLogicalNotEqual, m->id_bool, sa, sb);
	SpvV r = spv_sel64(m, s, spv_neg64(m, q), q);
	r.uns = uns;
	return r;
}

static SpvV spv_rem64(SpvMod *m, SpvV a, SpvV b, SpvV q, int uns) {
	return spv_sub64(m, a, spv_mul64(m, b, q, uns), uns);
}

static void spv_def_addsub64(SpvMod *m, uint32_t *def, int is_sub, SpvV a,
														 SpvV b, SpvV r) {
	uint32_t ah = spv_hi(m, a.id), bh = spv_hi(m, b.id), rh = spv_hi(m, r.id);
	uint32_t x = spv_uop(m, SpvOpBitwiseXor, ah, rh);
	uint32_t y = is_sub ? spv_uop(m, SpvOpBitwiseXor, ah, bh)
											: spv_uop(m, SpvOpBitwiseXor, bh, rh);
	uint32_t t = is_sub ? spv_uop(m, SpvOpBitwiseAnd, y, x)
											: spv_uop(m, SpvOpBitwiseAnd, x, y);
	spv_def_and(m, def,
							spv_emit3(m, SpvOpSGreaterThanEqual, m->id_bool,
												spv_emit2(m, SpvOpBitcast, m->id_int, t),
												spv_const(m, 0)));
}

static void spv_def_mul64(SpvMod *m, uint32_t *def, SpvV a, SpvV b, SpvV r) {
	SpvV mn = spv_const64(m, (int64_t)((uint64_t)1 << 63));
	SpvV n1 = spv_const64(m, -1);
	SpvV one = spv_const64(m, 1);
	uint32_t az = spv_not(m, spv_bool_of_v(m, a));
	uint32_t bz = spv_not(m, spv_bool_of_v(m, b));
	SpvV ag = spv_sel64(m, az, one, a);
	SpvV q = spv_sdiv64(m, r, ag, 0);
	uint32_t ne = spv_not(m, spv_eq64(m, q, b));
	uint32_t k1 = spv_and(m, spv_eq64(m, a, mn), spv_eq64(m, b, n1));
	uint32_t k2 = spv_and(m, spv_eq64(m, b, mn), spv_eq64(m, a, n1));
	uint32_t any = spv_or(m, ne, spv_or(m, k1, k2));
	uint32_t bad = spv_and(m, spv_and(m, spv_not(m, az), spv_not(m, bz)), any);
	spv_def_and(m, def, spv_not(m, bad));
}

static SpvV spv_guard_div64(SpvMod *m, uint32_t *def, int uns, SpvV a, SpvV b) {
	uint32_t bad = spv_not(m, spv_bool_of_v(m, b));
	uint32_t lo, hi;
	if (!uns) {
		SpvV mn = spv_const64(m, (int64_t)((uint64_t)1 << 63));
		SpvV n1 = spv_const64(m, -1);
		bad = spv_or(m, bad, spv_and(m, spv_eq64(m, a, mn), spv_eq64(m, b, n1)));
	}
	spv_def_and(m, def, spv_not(m, bad));
	lo = spv_usel(m, bad, spv_uintc(m, 1), spv_lo(m, b.id));
	hi = spv_usel(m, bad, spv_uintc(m, 0), spv_hi(m, b.id));
	return spv_mk(spv_u2(m, lo, hi), 1, uns);
}

static uint32_t spv_guard_shift64(SpvMod *m, uint32_t *def, SpvV b) {
	uint32_t lo = spv_lo(m, b.id), hi = spv_hi(m, b.id);
	uint32_t bad =
			spv_or(m, spv_ucmp(m, SpvOpINotEqual, hi, spv_uintc(m, 0)),
						 spv_ucmp(m, SpvOpUGreaterThanEqual, lo, spv_uintc(m, 64)));
	spv_def_and(m, def, spv_not(m, bad));
	return spv_usel(m, bad, spv_uintc(m, 0),
									spv_uop(m, SpvOpBitwiseAnd, lo, spv_uintc(m, 63)));
}

static SpvV spv_load_live_v(SpvMod *m, uint32_t base, int k, int w64, int uns) {
	if (!w64)
		return spv_mk(spv_load_live(m, base, 2 * k), 0, uns);
	{
		uint32_t lo = spv_emit2(m, SpvOpBitcast, m->id_uint,
														spv_load_live(m, base, 2 * k));
		uint32_t hi = spv_emit2(m, SpvOpBitcast, m->id_uint,
														spv_load_live(m, base, 2 * k + 1));
		return spv_mk(spv_u2(m, lo, hi), 1, uns);
	}
}

static void spv_store_live_v(SpvMod *m, uint32_t base, int k, SpvV v) {
	if (v.f64)
		v = spv_mk(spv_f64_pack(m, v.id), 1, 0);
	if (!v.w64) {
		/* The high word follows the value's own signedness, exactly as spv_widen
		 * does: an unsigned 32-bit value zero-extends and a signed one
		 * sign-extends. Sign-extending unconditionally stored -2 into an
		 * `unsigned int` slot as -2 where the CPU has 4294967294. */
		spv_store_live(m, base, 2 * k, v.id);
		spv_store_live(m, base, 2 * k + 1,
									 v.uns ? spv_const(m, 0)
												 : spv_emit3(m, SpvOpShiftRightArithmetic, m->id_int,
																		 v.id, spv_const(m, 31)));
		return;
	}
	spv_store_live(m, base, 2 * k,
								 spv_emit2(m, SpvOpBitcast, m->id_int, spv_lo(m, v.id)));
	spv_store_live(m, base, 2 * k + 1,
								 spv_emit2(m, SpvOpBitcast, m->id_int, spv_hi(m, v.id)));
}

/* B1. An element resolved at run time: `base + (slot0 + elem) * IN_SLOTS`.
 * Because the frame gives every array element its own 8-byte slot, this reaches
 * exactly the same cell layout the constant path uses, and the lo/hi pair of a
 * 64-bit element still cannot reach a neighbour. */
static uint32_t spv_slot_at(SpvMod *m, uint32_t base, int k0, uint32_t elem,
														int half) {
	uint32_t s = spv_emit3(m, SpvOpIAdd, m->id_int, elem, spv_const(m, k0));
	uint32_t w = spv_emit3(m, SpvOpIMul, m->id_int, s,
												 spv_const(m, MCC_GPU_IN_SLOTS));
	uint32_t i = spv_emit3(m, SpvOpIAdd, m->id_int, base, w);
	if (half)
		i = spv_emit3(m, SpvOpIAdd, m->id_int, i, spv_const(m, half));
	return i;
}

/* Three instructions, no branch, and the only ones the bound costs: compare,
 * poison, mask. J3b requires that an out-of-range device access be impossible
 * rather than merely detected, so the index is masked into the object's own
 * padded span -- the worst case then rewrites another element of the same
 * array, and the run carrying it is discarded because `def` is already false.
 * The reference reaches the identical verdict in ast_eval_slice_idx_ok, or the
 * two executors would differ precisely at the boundary. */
static uint32_t spv_dyn_elem(SpvMod *m, SpvV idx, const AstEvalSliceIdx *ix) {
	uint32_t u = spv_emit2(m, SpvOpBitcast, m->id_uint, spv_val_lo(m, idx));
	uint32_t ok = spv_ucmp(m, SpvOpULessThan, u, spv_uintc(m, (uint32_t)ix->nelem));
	uint32_t masked;
	spv_def_and(m, &m->def, ok);
	masked = spv_uop(m, SpvOpBitwiseAnd, u,
									 spv_uintc(m, (uint32_t)(ix->nspan - 1)));
	return spv_emit2(m, SpvOpBitcast, m->id_int, masked);
}

static SpvV spv_load_live_dv(SpvMod *m, uint32_t base, int k0, uint32_t elem,
														 int w64, int uns) {
	if (!w64)
		return spv_mk(spv_load_at(m, spv_slot_at(m, base, k0, elem, 0)), 0, uns);
	{
		uint32_t lo = spv_emit2(m, SpvOpBitcast, m->id_uint,
														spv_load_at(m, spv_slot_at(m, base, k0, elem, 0)));
		uint32_t hi = spv_emit2(m, SpvOpBitcast, m->id_uint,
														spv_load_at(m, spv_slot_at(m, base, k0, elem, 1)));
		return spv_mk(spv_u2(m, lo, hi), 1, uns);
	}
}

static void spv_store_live_dv(SpvMod *m, uint32_t base, int k0, uint32_t elem,
															SpvV v) {
	if (v.f64)
		v = spv_mk(spv_f64_pack(m, v.id), 1, 0);
	if (!v.w64) {
		spv_store_at_in(m, spv_slot_at(m, base, k0, elem, 0), v.id);
		spv_store_at_in(m, spv_slot_at(m, base, k0, elem, 1),
										v.uns ? spv_const(m, 0)
													: spv_emit3(m, SpvOpShiftRightArithmetic, m->id_int,
																			v.id, spv_const(m, 31)));
		return;
	}
	spv_store_at_in(m, spv_slot_at(m, base, k0, elem, 0),
									spv_emit2(m, SpvOpBitcast, m->id_int, spv_lo(m, v.id)));
	spv_store_at_in(m, spv_slot_at(m, base, k0, elem, 1),
									spv_emit2(m, SpvOpBitcast, m->id_int, spv_hi(m, v.id)));
}

/* ---- byte-addressed access to a storage region ------------------------- *
 *
 * The device half of ast_eval_slice_bytes_load / _store, and the reason it is
 * written as a region rather than as "the frame": a region is a storage-buffer
 * variable, a word index where the region starts, and how many bytes of it are
 * addressable. One lane's frame is a region whose base is the lane's own slice
 * of the input buffer; a heap shared by every lane and the host is a region
 * whose base is a constant in a different binding. Nothing below distinguishes
 * them, so the second one needs no new emitter code -- only a different
 * SpvRegion.
 *
 * Per-width is the whole point. The two-word store that the dense-slot path
 * uses is only sound because slots are disjoint 8-byte cells; the moment two
 * objects of different widths sit next to each other -- which is what a heap
 * is -- writing a sign-extended high word past a 32-bit value overwrites its
 * neighbour. So a store here writes exactly as many bytes as the type has, and
 * a sub-word store is a read-modify-write of the containing word because
 * SPIR-V has no 8-bit storage without StorageBuffer8BitAccess. */
static uint32_t spv_fit(SpvMod *m, uint32_t v, int t);

typedef struct SpvRegion {
	uint32_t var;
	uint32_t base;
	uint32_t nbyte;
	int shared;
} SpvRegion;

static SpvRegion spv_region(uint32_t var, uint32_t base, uint32_t nbyte) {
	SpvRegion r;
	r.var = var;
	r.base = base;
	r.nbyte = nbyte;
	r.shared = 0;
	return r;
}

static SpvRegion spv_region_shared(uint32_t var, uint32_t base,
																	 uint32_t nbyte) {
	SpvRegion r = spv_region(var, base, nbyte);
	r.shared = 1;
	return r;
}

static uint32_t spv_word_at(SpvMod *m, uint32_t var, uint32_t idx) {
	uint32_t p = spv_id(m);
	spvw_op(&m->body, SpvOpAccessChain, 6);
	spvw_put(&m->body, m->id_ptr_sb_int);
	spvw_put(&m->body, p);
	spvw_put(&m->body, var);
	spvw_put(&m->body, spv_const(m, 0));
	spvw_put(&m->body, idx);
	return spv_emit2(m, SpvOpLoad, m->id_int, p);
}

static uint32_t spv_word_ptr(SpvMod *m, uint32_t var, uint32_t idx) {
	uint32_t p = spv_id(m);
	spvw_op(&m->body, SpvOpAccessChain, 6);
	spvw_put(&m->body, m->id_ptr_sb_int);
	spvw_put(&m->body, p);
	spvw_put(&m->body, var);
	spvw_put(&m->body, spv_const(m, 0));
	spvw_put(&m->body, idx);
	return p;
}

static void spv_word_rmw_atomic(SpvMod *m, uint32_t var, uint32_t idx,
																uint32_t keep, uint32_t put) {
	uint32_t scope = spv_const(m, 1);
	uint32_t sem = spv_const(m, 0);
	uint32_t clr = spv_emit2(m, SpvOpBitcast, m->id_int,
													 spv_emit2(m, SpvOpNot, m->id_uint, keep));
	uint32_t set = spv_emit2(m, SpvOpBitcast, m->id_int, put);
	uint32_t p = spv_word_ptr(m, var, idx);
	uint32_t r = spv_id(m);
	spvw_op(&m->body, SpvOpAtomicAnd, 7);
	spvw_put(&m->body, m->id_int);
	spvw_put(&m->body, r);
	spvw_put(&m->body, p);
	spvw_put(&m->body, scope);
	spvw_put(&m->body, sem);
	spvw_put(&m->body, clr);
	r = spv_id(m);
	spvw_op(&m->body, SpvOpAtomicOr, 7);
	spvw_put(&m->body, m->id_int);
	spvw_put(&m->body, r);
	spvw_put(&m->body, p);
	spvw_put(&m->body, scope);
	spvw_put(&m->body, sem);
	spvw_put(&m->body, set);
}

static void spv_word_set(SpvMod *m, uint32_t var, uint32_t idx, uint32_t val) {
	uint32_t p = spv_id(m);
	spvw_op(&m->body, SpvOpAccessChain, 6);
	spvw_put(&m->body, m->id_ptr_sb_int);
	spvw_put(&m->body, p);
	spvw_put(&m->body, var);
	spvw_put(&m->body, spv_const(m, 0));
	spvw_put(&m->body, idx);
	spvw_op(&m->body, SpvOpStore, 3);
	spvw_put(&m->body, p);
	spvw_put(&m->body, val);
}

/* Four instructions, no branch: range, alignment, poison, replace. The
 * replacement is 0 rather than a masked offset because a region is not
 * required to be a power of two in size, and masking a 48-byte region would
 * reach byte 63 -- into whatever follows it. 0 is in range for every region
 * that can hold the access at all, so this cannot leave the region no matter
 * what the offset was, which is what J3b's "no PageFault by construction"
 * needs. The reference reaches the identical verdict and the identical
 * replacement in ast_eval_slice_addr_ok / _fix. */
static uint32_t spv_region_addr(SpvMod *m, const SpvRegion *r, uint32_t byteoff,
																int width) {
	uint32_t u = spv_emit2(m, SpvOpBitcast, m->id_uint, byteoff);
	uint32_t last = spv_uop(m, SpvOpISub, r->nbyte, spv_uintc(m, (uint32_t)width));
	/* nbyte - width wraps when the region is smaller than the access, which
	 * would read as "everything is in range". The reference refuses that case
	 * outright, so the device has to as well. */
	uint32_t big = spv_ucmp(m, SpvOpUGreaterThanEqual, r->nbyte,
													spv_uintc(m, (uint32_t)width));
	uint32_t inr = spv_and(m, big, spv_ucmp(m, SpvOpULessThanEqual, u, last));
	uint32_t ok = inr;
	if (width > 1) {
		uint32_t al = spv_ucmp(
				m, SpvOpIEqual,
				spv_uop(m, SpvOpBitwiseAnd, u, spv_uintc(m, (uint32_t)(width - 1))),
				spv_uintc(m, 0));
		ok = spv_and(m, inr, al);
	}
	spv_def_and(m, &m->def, ok);
	return spv_usel(m, ok, u, spv_uintc(m, 0));
}

static uint32_t spv_region_word(SpvMod *m, const SpvRegion *r, uint32_t uoff,
																int plus) {
	uint32_t w = spv_uop(m, SpvOpShiftRightLogical, uoff, spv_uintc(m, 2));
	uint32_t i = spv_emit3(m, SpvOpIAdd, m->id_int, r->base,
												 spv_emit2(m, SpvOpBitcast, m->id_int, w));
	if (plus)
		i = spv_emit3(m, SpvOpIAdd, m->id_int, i, spv_const(m, plus));
	return i;
}

static SpvV spv_load_region(SpvMod *m, const SpvRegion *r, uint32_t byteoff,
														int t) {
	int width = ast_eval_slice_tsize(t);
	int uns = (t & VT_UNSIGNED) != 0;
	uint32_t uoff, lo, sh;
	if (width <= 0)
		return spv_mk(spv_const(m, 0), 0, 0);
	uoff = spv_region_addr(m, r, byteoff, width);
	if (width == 8) {
		uint32_t a = spv_emit2(m, SpvOpBitcast, m->id_uint,
													 spv_word_at(m, r->var, spv_region_word(m, r, uoff, 0)));
		uint32_t b = spv_emit2(m, SpvOpBitcast, m->id_uint,
													 spv_word_at(m, r->var, spv_region_word(m, r, uoff, 1)));
		return spv_mk(spv_u2(m, a, b), 1, uns);
	}
	lo = spv_word_at(m, r->var, spv_region_word(m, r, uoff, 0));
	if (width == 4)
		return spv_mk(lo, 0, uns);
	sh = spv_uop(m, SpvOpShiftLeftLogical,
							 spv_uop(m, SpvOpBitwiseAnd, uoff, spv_uintc(m, 3)),
							 spv_uintc(m, 3));
	lo = spv_emit3(m, SpvOpShiftRightLogical, m->id_int, lo,
								 spv_emit2(m, SpvOpBitcast, m->id_int, sh));
	if ((t & VT_BTYPE) == VT_BOOL)
		lo = spv_emit3(m, SpvOpBitwiseAnd, m->id_int, lo, spv_const(m, 0xFF));
	return spv_mk(spv_fit(m, lo, t), 0, uns);
}

static void spv_store_region(SpvMod *m, const SpvRegion *r, uint32_t byteoff,
														 SpvV v, int t) {
	int width = ast_eval_slice_tsize(t);
	uint32_t uoff, sh, keep;
	if (width <= 0)
		return;
	uoff = spv_region_addr(m, r, byteoff, width);
	if (width == 8) {
		spv_widen(m, &v);
		spv_word_set(m, r->var, spv_region_word(m, r, uoff, 0),
								 spv_emit2(m, SpvOpBitcast, m->id_int, spv_lo(m, v.id)));
		spv_word_set(m, r->var, spv_region_word(m, r, uoff, 1),
								 spv_emit2(m, SpvOpBitcast, m->id_int, spv_hi(m, v.id)));
		return;
	}
	if (width == 4) {
		spv_word_set(m, r->var, spv_region_word(m, r, uoff, 0), spv_val_lo(m, v));
		return;
	}
	sh = spv_uop(m, SpvOpShiftLeftLogical,
							 spv_uop(m, SpvOpBitwiseAnd, uoff, spv_uintc(m, 3)),
							 spv_uintc(m, 3));
	keep = spv_uop(m, SpvOpShiftLeftLogical,
								 spv_uintc(m, width == 1 ? 0xFFu : 0xFFFFu), sh);
	spv_word_rmw_atomic(
			m, r->var, spv_region_word(m, r, uoff, 0), keep,
			spv_uop(m, SpvOpBitwiseAnd,
							spv_uop(m, SpvOpShiftLeftLogical,
											spv_emit2(m, SpvOpBitcast, m->id_uint, spv_val_lo(m, v)),
											sh),
							keep));
}

static int spv_mem_region(SpvMod *m, SpvRegion *r) {
	if (!m->mem_nbyte)
		return 0;
	*r = spv_region_shared(m->id_mem, spv_const(m, 0),
												 spv_uintc(m, m->mem_nbyte));
	m->mem_used = 1;
	return 1;
}

static uint32_t spv_mem_off(SpvMod *m, SpvV p) {
	SpvV d;
	spv_widen(m, &p);
	d = spv_sub64(m, p, spv_const64(m, m->mem_base), 1);
	spv_def_and(m, &m->def,
							spv_ucmp(m, SpvOpIEqual, spv_hi(m, d.id), spv_uintc(m, 0)));
	return spv_emit2(m, SpvOpBitcast, m->id_int, spv_lo(m, d.id));
}

static uint32_t spv_ext_off(SpvMod *m, SpvV p, uint32_t elem,
														const AstEvalSliceIdx *ix) {
	uint32_t b = spv_mem_off(m, p);
	uint32_t d = spv_emit3(m, SpvOpIMul, m->id_int, elem,
												 spv_const(m, ix->esize));
	return spv_emit3(m, SpvOpIAdd, m->id_int, b, d);
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

static SpvV spv_fit_v(SpvMod *m, SpvV v, int t) {
	int uns = (t & VT_UNSIGNED) != 0;
	if (ast_eval_slice_f64t(t))
		return spv_f64_of(m, v);
	if (ast_eval_slice_is64(t)) {
		spv_widen(m, &v);
		v.uns = uns;
		return v;
	}
	if ((t & VT_BTYPE) == VT_BOOL)
		return spv_mk(spv_int_of_bool(m, spv_bool_of_v(m, v)), 0, uns);
	return spv_mk(spv_fit(m, spv_val_lo(m, v), t), 0, uns);
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
	m->lane = gi;
	return spv_emit3(m, SpvOpIMul, m->id_int, gi,
									 spv_const(m, nlive * MCC_GPU_IN_SLOTS));
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

static void spv_main_end(SpvMod *m, uint32_t lane, SpvV val) {
	uint32_t o0 = spv_emit3(m, SpvOpIMul, m->id_int, lane,
													spv_const(m, MCC_GPU_OUT_SLOTS));
	uint32_t o1 = spv_emit3(m, SpvOpIAdd, m->id_int, o0, spv_const(m, 1));
	uint32_t o2 = spv_emit3(m, SpvOpIAdd, m->id_int, o0, spv_const(m, 2));
	uint32_t p = spv_pair(m, val);
	spv_store_at(m, o0,
							 spv_emit2(m, SpvOpBitcast, m->id_int, spv_lo(m, p)));
	spv_store_at(m, o1,
							 spv_emit2(m, SpvOpBitcast, m->id_int, spv_hi(m, p)));
	spv_store_at(m, o2, spv_int_of_bool(m, m->def));
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

static int spv_f64_binop_code(int op) {
	switch (op) {
	case '+':
		return SpvOpFAdd;
	case '-':
		return SpvOpFSub;
	case '*':
		return SpvOpFMul;
	case TOK_EQ:
		return SpvOpFOrdEqual;
	case TOK_NE:
		return SpvOpFUnordNotEqual;
	case TOK_LT:
		return SpvOpFOrdLessThan;
	case TOK_LE:
		return SpvOpFOrdLessThanEqual;
	case TOK_GT:
		return SpvOpFOrdGreaterThan;
	case TOK_GE:
		return SpvOpFOrdGreaterThanEqual;
	default:
		return 0;
	}
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
										int nenv, uint32_t base, SpvV *out);

static int spv_branch_pair(SpvMod *m, AstArena *a, AstLocal n,
													 const int32_t *off, int nenv, uint32_t base,
													 SpvV *out) {
	AstLocal cn = ast_child(a, n, 0), tn = ast_child(a, n, 1);
	AstLocal en = ast_child(a, n, 2);
	SpvV cv, tv, ev;
	int w64, uns;
	int ft = ast_eval_slice_ftype(a, n);
	mcc_gpu_vw(a, n, &w64, &uns);
	if (ft) {
		w64 = 1;
		uns = 0;
	}
	if (!spv_expr(m, a, cn, off, nenv, base, &cv))
		return 0;
	uint32_t cb = spv_bool_of_v(m, cv);
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
	if (!spv_expr(m, a, tn, off, nenv, base, &tv))
		return 0;
	uint32_t tid = w64 ? spv_pair(m, tv) : tv.id;
	uint32_t def_then = m->def;
	uint32_t from_then = m->cur_label;
	spvw_op(&m->body, SpvOpBranch, 2);
	spvw_put(&m->body, l_merge);

	spv_label_at(m, l_else);
	m->def = def_in;
	if (!spv_expr(m, a, en, off, nenv, base, &ev))
		return 0;
	uint32_t eid = w64 ? spv_pair(m, ev) : ev.id;
	uint32_t def_else = m->def;
	uint32_t from_else = m->cur_label;
	spvw_op(&m->body, SpvOpBranch, 2);
	spvw_put(&m->body, l_merge);

	spv_label_at(m, l_merge);
	uint32_t phi = spv_id(m);
	spvw_op(&m->body, SpvOpPhi, 7);
	spvw_put(&m->body, w64 ? m->id_u2 : m->id_int);
	spvw_put(&m->body, phi);
	spvw_put(&m->body, tid);
	spvw_put(&m->body, from_then);
	spvw_put(&m->body, eid);
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
	*out = ft ? spv_mkf(spv_f64_unpack(m, phi)) : spv_mk(phi, w64, uns);
	return 1;
}

static int spv_logical(SpvMod *m, AstArena *a, AstLocal n, int want,
											 const int32_t *off, int nenv, uint32_t base,
											 SpvV *out, uint32_t k) {
	uint32_t nc = ast_nchild(a, n);
	SpvV cv, rest;
	if (k == nc) {
		*out = spv_mk(spv_const(m, want ? 1 : 0), 0, 0);
		return 1;
	}
	if (!spv_expr(m, a, ast_child(a, n, k), off, nenv, base, &cv))
		return 0;
	uint32_t cb = spv_bool_of_v(m, cv);
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
	spvw_put(&m->body, rest.id);
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
	*out = spv_mk(phi, 0, 0);
	return 1;
}

static int spv_konst(SpvMod *m, AstArena *a, AstLocal n, int t, SpvV *out) {
	int64_t x;
	int uns = (t & VT_UNSIGNED) != 0;
	if (ast_eval_slice_f64t(t)) {
		*out = spv_mkf(spv_f64_const(m, ast_ival(a, n)));
		return 1;
	}
	x = ast_eval_slice_fit((int64_t)ast_ival(a, n), t);
	if (ast_eval_slice_is64(t)) {
		*out = spv_const64(m, x);
		out->uns = uns;
		return 1;
	}
	*out = spv_mk(spv_const(m, (int32_t)x), 0, uns);
	return 1;
}

#define MCC_GPU_REFUSE_KINDS 64
static long mcc_gpu_refuse_kind[MCC_GPU_REFUSE_KINDS];
static long mcc_gpu_refuse_total;
#define MCC_GPU_REFUSE_OPS 24
static int mcc_gpu_refuse_opv[MCC_GPU_REFUSE_OPS];
static long mcc_gpu_refuse_op[MCC_GPU_REFUSE_OPS];
static int mcc_gpu_refuse_opn;

static int spv_refuse(AstArena *a, AstLocal n) {
	unsigned k = (unsigned)ast_kind(a, n);
	unsigned o = (unsigned)ast_op(a, n);
	mcc_gpu_refuse_total++;
	if (k < MCC_GPU_REFUSE_KINDS)
		mcc_gpu_refuse_kind[k]++;
	{
		int i;
		for (i = 0; i < mcc_gpu_refuse_opn; i++)
			if (mcc_gpu_refuse_opv[i] == (int)o)
				break;
		if (i == mcc_gpu_refuse_opn && mcc_gpu_refuse_opn < MCC_GPU_REFUSE_OPS)
			mcc_gpu_refuse_opv[mcc_gpu_refuse_opn++] = (int)o;
		if (i < MCC_GPU_REFUSE_OPS)
			mcc_gpu_refuse_op[i]++;
	}
	return 0;
}
static int spv_expr(SpvMod *m, AstArena *a, AstLocal n, const int32_t *off,
										int nenv, uint32_t base, SpvV *out) {
	if (n == AST_NONE || m->failed)
		return spv_refuse(a, n);
	switch (ast_kind(a, n)) {
	case AST_Bailout: {
		int t = ast_type_t(a, n);
		if (ast_bad_type(t) || is_float(t) || !ast_eval_slice_intt(t))
			return spv_refuse(a, n);
		spv_def_and(m, &m->def, spv_not(m, spv_true(m)));
		mcc_gpu_bail_emit++;
		if (ast_eval_slice_is64(t)) {
			*out = spv_const64(m, 0);
			out->uns = (t & VT_UNSIGNED) != 0;
			return 1;
		}
		*out = spv_mk(spv_const(m, 0), 0, (t & VT_UNSIGNED) != 0);
		return 1;
	}
	case AST_Literal: {
		int t = ast_type_t(a, n);
		if (ast_bad_type(t))
			return spv_refuse(a, n);
		if ((ast_op(a, n) & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
			return spv_refuse(a, n);
		if (!ast_eval_slice_f64t(t) && (is_float(t) || !ast_eval_slice_intt(t)))
			return spv_refuse(a, n);
		return spv_konst(m, a, n, t, out);
	}
	case AST_Ref: {
		int r = ast_op(a, n);
		int t = ast_type_t(a, n);
		int32_t go;
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) {
			int k;
			if (!ast_bad_type(t) && ast_eval_slice_f64t(t)) {
				if (!spv_env_index(off, nenv, (int32_t)(int64_t)ast_ival(a, n), &k))
					return spv_refuse(a, n);
				*out = spv_mkf(
						spv_f64_unpack(m, spv_load_live_v(m, base, k, 1, 0).id));
				return 1;
			}
			if (!ast_eval_slice_intt(t) || is_float(t))
				return spv_refuse(a, n);
			if (!spv_env_index(off, nenv, (int32_t)(int64_t)ast_ival(a, n), &k))
				return spv_refuse(a, n);
			/* Narrow to the ref's own type. spv_load_live_v takes only a width
			 * flag, so it can deliver 32 or 64 bits but never VT_BOOL/BYTE/SHORT,
			 * and a byte live-in holding 1000 arrived as 1000 rather than -24. */
			*out = spv_fit_v(m, spv_load_live_v(m, base, k, ast_eval_slice_is64(t),
																					(t & VT_UNSIGNED) != 0),
											 t);
			return 1;
		}
		if ((r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
			if (ast_bad_type(t))
				return spv_refuse(a, n);
			if (!ast_eval_slice_f64t(t) && (is_float(t) || !ast_eval_slice_intt(t)))
				return spv_refuse(a, n);
			return spv_konst(m, a, n, t, out);
		}
		if (!(t & VT_ARRAY) && ast_eval_slice_globl(a, n, &go)) {
			int k;
			if (!spv_env_index(off, nenv, go, &k))
				return spv_refuse(a, n);
			*out = spv_fit_v(m, spv_load_live_v(m, base, k, ast_eval_slice_is64(t),
																					(t & VT_UNSIGNED) != 0),
											 t);
			return 1;
		}
		return spv_refuse(a, n);
	}
	case AST_Load: {
		AstLocal c = ast_first_child(a, n);
		int t = ast_type_t(a, n);
		AstEvalSliceIdx ix;
		SpvRegion mr;
		int32_t fo;
		int k, et;
		if (c == AST_NONE)
			return spv_refuse(a, n);
		/* A local, or a constant offset from one via `.field`/`&`. Resolved
		 * host-side, so the device still sees a constant OpAccessChain. */
		if (!ast_bad_type(t) && ast_eval_slice_f64t(t) &&
				ast_eval_slice_frame_off(a, c, &fo, 0)) {
			if (!spv_env_index(off, nenv, fo, &k))
				return spv_refuse(a, n);
			*out = spv_mkf(spv_f64_unpack(m, spv_load_live_v(m, base, k, 1, 0).id));
			return 1;
		}
		if (ast_eval_slice_intt(t) && !is_float(t) &&
				ast_eval_slice_frame_off(a, c, &fo, 0)) {
			if (!spv_env_index(off, nenv, fo, &k))
				return spv_refuse(a, n);
			*out = spv_fit_v(m, spv_load_live_v(m, base, k, ast_eval_slice_is64(t),
																					(t & VT_UNSIGNED) != 0),
											 t);
			return 1;
		}
		if (ast_eval_slice_dynidx(a, c, &ix)) {
			SpvV iv;
			uint32_t elem;
			if (!spv_env_index(off, nenv, ix.base, &k))
				return spv_refuse(a, n);
			if (!spv_expr(m, a, ix.idx, off, nenv, base, &iv))
				return spv_refuse(a, n);
			elem = spv_dyn_elem(m, iv, &ix);
			if (ast_eval_slice_ext(&ix)) {
				if (!spv_mem_region(m, &mr))
					return spv_refuse(a, n);
				*out = spv_fit_v(
						m,
						spv_load_region(m, &mr,
														spv_ext_off(m, spv_load_live_v(m, base, k, 1, 0),
																				elem, &ix),
														ix.etype),
						ix.etype);
				return 1;
			}
			*out = spv_fit_v(
					m,
					spv_load_live_dv(m, base, k, elem,
													 ast_eval_slice_is64(ix.etype) ||
															 ast_eval_slice_f64t(ix.etype),
													 (ix.etype & VT_UNSIGNED) != 0),
					ix.etype);
			return 1;
		}
		if (ast_eval_slice_deref(a, n, &fo, &et)) {
			if (!spv_env_index(off, nenv, fo, &k))
				return spv_refuse(a, n);
			if (!spv_mem_region(m, &mr))
				return spv_refuse(a, n);
			*out = spv_load_region(
					m, &mr, spv_mem_off(m, spv_load_live_v(m, base, k, 1, 0)), et);
			return 1;
		}
		return spv_refuse(a, n);
	}
	case AST_Convert: {
		int t = ast_type_t(a, n);
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || is_float(t) || is_float(ast_type_t(a, c)))
			return spv_refuse(a, n);
		if (ast_eval_slice_ftype(a, c))
			return spv_refuse(a, n);
		if (ast_bad_type(t) || !ast_eval_slice_intt(t))
			return spv_refuse(a, n);
		SpvV v;
		if (!spv_expr(m, a, c, off, nenv, base, &v))
			return spv_refuse(a, n);
		*out = spv_fit_v(m, v, t);
		return 1;
	}
	case AST_Unary: {
		int uop = ast_op(a, n);
		int t = ast_eval_slice_promote(ast_eval_slice_wtype(a, n));
		AstLocal c = ast_first_child(a, n);
		int ft;
		int32_t mo;
		if (c == AST_NONE)
			return spv_refuse(a, n);
		if (ast_eval_slice_member_off(a, n, &mo)) {
			int mt = ast_type_t(a, n);
			int k;
			if (!spv_env_index(off, nenv, mo, &k))
				return spv_refuse(a, n);
			*out = spv_fit_v(m, spv_load_live_v(m, base, k, ast_eval_slice_is64(mt),
																					(mt & VT_UNSIGNED) != 0),
											 mt);
			return 1;
		}
		{
			int32_t pfo, madd;
			int at, k;
			SpvRegion ar;
			if (ast_eval_slice_arrow(a, n, &pfo, &madd, &at)) {
				if (!spv_env_index(off, nenv, pfo, &k))
					return spv_refuse(a, n);
				if (!spv_mem_region(m, &ar))
					return spv_refuse(a, n);
				*out = spv_load_region(
						m, &ar,
						spv_emit3(m, SpvOpIAdd, m->id_int,
											spv_mem_off(m, spv_load_live_v(m, base, k, 1, 0)),
											spv_const(m, madd)),
						at);
				return 1;
			}
		}
		if (uop != '-' && uop != TOK_NEG && uop != '~' && uop != '!')
			return spv_refuse(a, n);
		ft = ast_eval_slice_ftype(a, c);
		if (ft && uop == '~')
			return spv_refuse(a, n);
		if (!ft && !t)
			return spv_refuse(a, n);
		SpvV v;
		if (!spv_expr(m, a, c, off, nenv, base, &v))
			return spv_refuse(a, n);
		if (ft) {
			SpvV fv = spv_f64_of(m, v);
			if (uop == '!') {
				*out = spv_mk(spv_int_of_bool(m, spv_not(m, spv_bool_of_v(m, fv))), 0,
											0);
				return 1;
			}
			*out = spv_mkf(spv_f64_neg(m, fv.id));
			return 1;
		}
		int is64 = ast_eval_slice_is64(t);
		int uns = (t & VT_UNSIGNED) != 0;
		if (uop == '!') {
			*out = spv_mk(spv_int_of_bool(m, spv_not(m, spv_bool_of_v(m, v))), 0, 0);
			return 1;
		}
		if (uop == '~') {
			if (is64) {
				spv_widen(m, &v);
				*out = spv_mk(spv_u2(m, spv_emit2(m, SpvOpNot, m->id_uint,
																					spv_lo(m, v.id)),
														 spv_emit2(m, SpvOpNot, m->id_uint,
																			 spv_hi(m, v.id))),
											1, uns);
			} else {
				*out = spv_mk(
						spv_fit(m, spv_emit2(m, SpvOpNot, m->id_int, spv_val_lo(m, v)), t),
						0, uns);
			}
			return 1;
		}
		if (is64) {
			SpvV z = spv_const64(m, 0), r;
			spv_widen(m, &v);
			r = spv_sub64(m, z, v, uns);
			if (!uns)
				spv_def_addsub64(m, &m->def, 1, z, v, r);
			*out = r;
		} else {
			uint32_t z = spv_const(m, 0);
			uint32_t lo = spv_val_lo(m, v);
			uint32_t r = spv_emit3(m, SpvOpISub, m->id_int, z, lo);
			if (!uns)
				spv_def_addsub(m, &m->def, 1, z, lo, r);
			*out = spv_mk(r, 0, uns);
		}
		return 1;
	}
	case AST_Binary: {
		int bop = ast_op(a, n);
		if (bop == TOK_LAND || bop == TOK_LOR)
			return spv_logical(m, a, n, bop == TOK_LAND, off, nenv, base, out, 0);
		if (ast_nchild(a, n) != 2)
			return spv_refuse(a, n);
		AstLocal x = ast_child(a, n, 0), y = ast_child(a, n, 1);
		int xft = ast_eval_slice_ftype(a, x);
		if (xft) {
			SpvV fl, fr;
			int fcode = spv_f64_binop_code(bop);
			if (!ast_eval_slice_ftype(a, y) || !fcode)
				return spv_refuse(a, n);
			if (!spv_expr(m, a, x, off, nenv, base, &fl))
				return spv_refuse(a, n);
			if (!spv_expr(m, a, y, off, nenv, base, &fr))
				return spv_refuse(a, n);
			fl = spv_f64_of(m, fl);
			fr = spv_f64_of(m, fr);
			if (fcode == SpvOpFAdd || fcode == SpvOpFSub || fcode == SpvOpFMul) {
				*out = spv_mkf(spv_f64_arith(m, fcode, fl.id, fr.id));
				return 1;
			}
			*out = spv_mk(
					spv_int_of_bool(m, spv_emit3(m, fcode, m->id_bool, fl.id, fr.id)), 0,
					0);
			return 1;
		}
		int xt = ast_eval_slice_wtype(a, x);
		int wt = ast_eval_slice_binop_wtype(a, n);
		if (!xt || !wt || is_float(ast_type_t(a, x)) || is_float(ast_type_t(a, y)))
			return spv_refuse(a, n);
		if (ast_eval_slice_ftype(a, y))
			return spv_refuse(a, n);
		SpvV lv, rv;
		if (!spv_expr(m, a, x, off, nenv, base, &lv))
			return spv_refuse(a, n);
		if (!spv_expr(m, a, y, off, nenv, base, &rv))
			return spv_refuse(a, n);
		int uns = (wt & VT_UNSIGNED) != 0;
		int is64 = ast_eval_slice_is64(wt);
		int is_cmp;
		int code = spv_binop_code(bop, uns, &is_cmp);
		if (is_cmp < 0)
			return spv_refuse(a, n);
		if (!is64) {
			uint32_t l = spv_val_lo(m, lv), r = spv_val_lo(m, rv), res;
			if (is_cmp) {
				*out = spv_mk(spv_int_of_bool(m, spv_emit3(m, code, m->id_bool, l, r)),
											0, 0);
				return 1;
			}
			if (code == SpvOpUDiv || code == SpvOpUMod) {
				uint32_t d = spv_guard_div(m, &m->def, 1, l, r);
				*out = spv_mk(spv_unsigned_binop(m, code, l, d), 0, uns);
				return 1;
			}
			if (code == SpvOpSRem) {
				uint32_t d = spv_guard_div(m, &m->def, 0, l, r);
				*out = spv_mk(spv_signed_rem(m, l, d), 0, uns);
				return 1;
			}
			if (code == SpvOpSDiv) {
				uint32_t d = spv_guard_div(m, &m->def, 0, l, r);
				*out = spv_mk(spv_emit3(m, code, m->id_int, l, d), 0, uns);
				return 1;
			}
			if (code == SpvOpShiftLeftLogical || code == SpvOpShiftRightLogical ||
					code == SpvOpShiftRightArithmetic) {
				uint32_t sh = spv_guard_shift(m, &m->def, r);
				*out = spv_mk(spv_emit3(m, code, m->id_int, l, sh), 0, uns);
				return 1;
			}
			res = spv_emit3(m, code, m->id_int, l, r);
			if (!uns && code == SpvOpIAdd)
				spv_def_addsub(m, &m->def, 0, l, r, res);
			else if (!uns && code == SpvOpISub)
				spv_def_addsub(m, &m->def, 1, l, r, res);
			else if (!uns && code == SpvOpIMul)
				spv_def_mul(m, &m->def, l, r, res);
			*out = spv_mk(res, 0, uns);
			return 1;
		}
		spv_widen(m, &lv);
		spv_widen(m, &rv);
		if (is_cmp) {
			*out = spv_mk(spv_int_of_bool(m, spv_cmp64(m, code, lv, rv)), 0, 0);
			return 1;
		}
		if (code == SpvOpUDiv || code == SpvOpUMod) {
			SpvV d = spv_guard_div64(m, &m->def, 1, lv, rv);
			SpvV q = spv_udiv64(m, lv, d, uns);
			*out = code == SpvOpUMod ? spv_rem64(m, lv, d, q, uns) : q;
			return 1;
		}
		if (code == SpvOpSDiv || code == SpvOpSRem) {
			SpvV d = spv_guard_div64(m, &m->def, 0, lv, rv);
			SpvV q = spv_sdiv64(m, lv, d, uns);
			*out = code == SpvOpSRem ? spv_rem64(m, lv, d, q, uns) : q;
			return 1;
		}
		if (code == SpvOpShiftLeftLogical || code == SpvOpShiftRightLogical ||
				code == SpvOpShiftRightArithmetic) {
			uint32_t sh = spv_guard_shift64(m, &m->def, rv);
			*out = spv_shift64(m, code, lv, sh, uns);
			return 1;
		}
		if (code == SpvOpBitwiseAnd || code == SpvOpBitwiseOr ||
				code == SpvOpBitwiseXor) {
			*out = spv_bit64(m, code, lv, rv, uns);
			return 1;
		}
		if (code == SpvOpIAdd) {
			*out = spv_add64(m, lv, rv, uns);
			if (!uns)
				spv_def_addsub64(m, &m->def, 0, lv, rv, *out);
			return 1;
		}
		if (code == SpvOpISub) {
			*out = spv_sub64(m, lv, rv, uns);
			if (!uns)
				spv_def_addsub64(m, &m->def, 1, lv, rv, *out);
			return 1;
		}
		*out = spv_mul64(m, lv, rv, uns);
		if (!uns)
			spv_def_mul64(m, &m->def, lv, rv, *out);
		return 1;
	}
	case AST_If: {
		if (ast_nchild(a, n) != 3)
			return spv_refuse(a, n);
		return spv_branch_pair(m, a, n, off, nenv, base, out);
	}
	default:
		return spv_refuse(a, n);
	}
}

static uint32_t *spv_module_finish(SpvMod *m, int *nwords) {
	int total = 5 + m->caps.n + m->pre.n + m->types.n + m->body.n;
	uint32_t *w = (uint32_t *)SPV_MALLOC((size_t)total * sizeof *w);
	int i = 0;
	w[i++] = SPV_MAGIC;
	w[i++] = SPV_VERSION;
	w[i++] = 0;
	w[i++] = m->next_id;
	w[i++] = 0;
	memcpy(w + i, m->caps.w, (size_t)m->caps.n * sizeof *w);
	i += m->caps.n;
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
	SPV_FREE(m->caps.w);
	SPV_FREE(m->pre.w);
	SPV_FREE(m->types.w);
	SPV_FREE(m->body.w);
}

#endif /* MCC_GPU_LANG_MSL */

#include "mccfmt.h"

#endif /* MCC_GPU_EMITTER */

#if defined(MCC_GPU_ORACLE) && !defined(MCC_GPU_ORACLE_PROVIDED)
#define MCC_GPU_ORACLE_PROVIDED 1

#include <stdio.h>

#if MCC_GPU_LANG_MSL

static int mcc_gpu_emit(AstArena *a, AstLocal root, const int32_t *off, int n,
												MccGpuCode *c) {
	MslMod m;
	uint32_t base;
	MslV val;
	char *src;
	int nb = 0;
	msl_module_begin(&m, n);
	base = msl_main_begin(&m, n);
	if (!msl_expr(&m, a, root, off, n, base, &val) || m.failed) {
		msl_module_free(&m);
		return 0;
	}
	msl_main_end(&m, m.lane, val);
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
	uint32_t base;
	SpvV val;
	uint32_t *code;
	int nwords = 0;
	spv_module_begin(&m, n);
	base = spv_main_begin(&m, n);
	if (!spv_expr(&m, a, root, off, n, base, &val) || m.failed) {
		spv_module_free(&m);
		return 0;
	}
	spv_main_end(&m, m.lane, val);
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

static int mcc_gpu_run2(const MccGpuCode *a, const MccGpuCode *b,
												const int32_t *in, int ntuple, int nlive, int32_t *oa,
												int32_t *ob) {
	return mcc_gpu_dispatch2_ro_in(a->p, a->n, b->p, b->n, in, ntuple, nlive, oa,
																 ob);
}

#endif /* MCC_GPU_ORACLE */
