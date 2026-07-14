#ifdef MCC_EMBED_JIT

#include "mcc.h"
#include "mccast.h"
#include "mccgate.h"
#include "algorithms/jit.h"

#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#if defined(__GNUC__) || defined(__clang__)
#define MCCJIT_LOCAL __attribute__((visibility("hidden")))
#else
#define MCCJIT_LOCAL
#endif

#define MCCJIT_INTENT_MAGIC 0x314a434dul
#define MCCJIT_INTENT_FORMAT 4u

#define MCCJIT_ROLE_PLAIN 0u
#define MCCJIT_ROLE_NAMED 1u
#define MCCJIT_ROLE_PTR 2u
#define MCCJIT_ROLE_FUNC 3u
#define MCCJIT_ROLE_STRUCT 4u

static unsigned mccjit_role_for_base(int t) {
	switch (t & VT_BTYPE) {
	case VT_PTR:
		return MCCJIT_ROLE_PTR;
	case VT_FUNC:
		return MCCJIT_ROLE_FUNC;
	case VT_STRUCT:
		return MCCJIT_ROLE_STRUCT;
	default:
		return MCCJIT_ROLE_PLAIN;
	}
}

void ast_reemit_extern(Sym *sym, AstArena *ast);
int mccjit_ast_spec_fold(AstArena *ast, int off, int64_t val);
void mcc_jit_publish(void **slot, void *variant);

MCCJIT_LOCAL unsigned char *mccjit_last_blob;
MCCJIT_LOCAL size_t mccjit_last_len;
MCCJIT_LOCAL MCCState *mccjit_last_state;
MCCJIT_LOCAL int mccjit_last_purity;

#define MCCJIT_KGC_MAXARG 6

MCCJIT_LOCAL uint32_t mccjit_last_nparam;
MCCJIT_LOCAL uint32_t mccjit_last_param_t[MCCJIT_KGC_MAXARG];
MCCJIT_LOCAL int mccjit_last_ret_wide;
MCCJIT_LOCAL int mccjit_last_kgc_ok;
MCCJIT_LOCAL int mccjit_last_allfp;

static int mccjit_type_wide(int t) {
	switch (t & VT_BTYPE) {
	case VT_LLONG:
	case VT_PTR:
	case VT_FUNC:
		return 1;
	case VT_INT:
		return (t & VT_LONG) ? 1 : 0;
	default:
		return 0;
	}
}

static int mccjit_type_gp(int t) {
	switch (t & VT_BTYPE) {
	case VT_BOOL:
	case VT_BYTE:
	case VT_SHORT:
	case VT_INT:
	case VT_LLONG:
	case VT_PTR:
	case VT_FUNC:
		return 1;
	default:
		return 0;
	}
}

MCCJIT_LOCAL uint64_t mccjit_salt_witness(void) {
	uint64_t h = 0xcbf29ce484222325ull;
	const char *s;
	(void)s;
#ifdef MCC_VERSION_STR
	for (s = MCC_VERSION_STR; *s; s++)
		h = (h ^ (unsigned char)*s) * 0x100000001b3ull;
#endif
#ifdef MCC_CONFIG_TRIPLET
	for (s = MCC_CONFIG_TRIPLET; *s; s++)
		h = (h ^ (unsigned char)*s) * 0x100000001b3ull;
#endif
	return h;
}

typedef struct MccjitBuf {
	unsigned char *data;
	size_t len;
	size_t cap;
	int oom;
} MccjitBuf;

MCCJIT_LOCAL void mccjit_buf_init(MccjitBuf *b) {
	b->data = NULL;
	b->len = 0;
	b->cap = 0;
	b->oom = 0;
}

MCCJIT_LOCAL void mccjit_buf_free(MccjitBuf *b) {
	mcc_free(b->data);
	b->data = NULL;
	b->len = b->cap = 0;
}

static void mccjit_buf_put(MccjitBuf *b, const void *p, size_t n) {
	if (b->oom)
		return;
	if (b->len + n > b->cap) {
		size_t ncap = b->cap ? b->cap * 2 : 256;
		unsigned char *nd;
		while (ncap < b->len + n)
			ncap *= 2;
		nd = mcc_realloc(b->data, ncap);
		if (!nd) {
			b->oom = 1;
			return;
		}
		b->data = nd;
		b->cap = ncap;
	}
	memcpy(b->data + b->len, p, n);
	b->len += n;
}

static void mccjit_put_u8(MccjitBuf *b, uint8_t v) {
	mccjit_buf_put(b, &v, 1);
}
static void mccjit_put_u16(MccjitBuf *b, uint16_t v) {
	unsigned char t[2] = {(unsigned char)v, (unsigned char)(v >> 8)};
	mccjit_buf_put(b, t, 2);
}
static void mccjit_put_u32(MccjitBuf *b, uint32_t v) {
	unsigned char t[4];
	int i;
	for (i = 0; i < 4; i++)
		t[i] = (unsigned char)(v >> (i * 8));
	mccjit_buf_put(b, t, 4);
}
static void mccjit_put_u64(MccjitBuf *b, uint64_t v) {
	unsigned char t[8];
	int i;
	for (i = 0; i < 8; i++)
		t[i] = (unsigned char)(v >> (i * 8));
	mccjit_buf_put(b, t, 8);
}
static void mccjit_put_str(MccjitBuf *b, const char *s) {
	uint32_t n = s ? (uint32_t)strlen(s) : 0;
	mccjit_put_u32(b, n);
	if (n)
		mccjit_buf_put(b, s, n);
}

typedef struct MccjitReader {
	const unsigned char *data;
	size_t len;
	size_t pos;
	int err;
} MccjitReader;

static int mccjit_have(MccjitReader *r, size_t n) {
	if (r->err || r->pos + n > r->len) {
		r->err = 1;
		return 0;
	}
	return 1;
}
static uint8_t mccjit_get_u8(MccjitReader *r) {
	uint8_t v = 0;
	if (mccjit_have(r, 1)) {
		v = r->data[r->pos];
		r->pos += 1;
	}
	return v;
}
static uint16_t mccjit_get_u16(MccjitReader *r) {
	uint16_t v = 0;
	if (mccjit_have(r, 2)) {
		v = (uint16_t)(r->data[r->pos] | (r->data[r->pos + 1] << 8));
		r->pos += 2;
	}
	return v;
}
static uint32_t mccjit_get_u32(MccjitReader *r) {
	uint32_t v = 0;
	int i;
	if (mccjit_have(r, 4)) {
		for (i = 0; i < 4; i++)
			v |= (uint32_t)r->data[r->pos + i] << (i * 8);
		r->pos += 4;
	}
	return v;
}
static uint64_t mccjit_get_u64(MccjitReader *r) {
	uint64_t v = 0;
	int i;
	if (mccjit_have(r, 8)) {
		for (i = 0; i < 8; i++)
			v |= (uint64_t)r->data[r->pos + i] << (i * 8);
		r->pos += 8;
	}
	return v;
}
static char *mccjit_get_str(MccjitReader *r) {
	uint32_t n = mccjit_get_u32(r);
	char *s;
	if (r->err || r->pos + n > r->len) {
		r->err = 1;
		return NULL;
	}
	s = mcc_malloc(n + 1);
	if (!s) {
		r->err = 1;
		return NULL;
	}
	if (n)
		memcpy(s, r->data + r->pos, n);
	s[n] = 0;
	r->pos += n;
	return s;
}

typedef struct MccjitHandles {
	uint64_t *raw;
	int64_t *token_v;
	uint8_t *role;
	uint32_t count;
	uint32_t cap;
	int oom;
} MccjitHandles;

static void mccjit_handles_init(MccjitHandles *h) {
	h->raw = NULL;
	h->token_v = NULL;
	h->role = NULL;
	h->count = 0;
	h->cap = 0;
	h->oom = 0;
}
static void mccjit_handles_free(MccjitHandles *h) {
	mcc_free(h->raw);
	mcc_free(h->token_v);
	mcc_free(h->role);
	mccjit_handles_init(h);
}

static uint32_t mccjit_handles_intern(MccjitHandles *h, uint64_t raw, unsigned role) {
	uint32_t i;
	if (raw == 0)
		return 0;
	for (i = 0; i < h->count; i++)
		if (h->raw[i] == raw) {
			if (role != MCCJIT_ROLE_PLAIN && h->role[i] == MCCJIT_ROLE_PLAIN)
				h->role[i] = (uint8_t)role;
			return i + 1;
		}
	if (h->count == h->cap) {
		uint32_t ncap = h->cap ? h->cap * 2 : 16;
		uint64_t *nr = mcc_realloc(h->raw, ncap * sizeof *nr);
		int64_t *nv = mcc_realloc(h->token_v, ncap * sizeof *nv);
		uint8_t *no = mcc_realloc(h->role, ncap * sizeof *no);
		if (nr)
			h->raw = nr;
		if (nv)
			h->token_v = nv;
		if (no)
			h->role = no;
		if (!nr || !nv || !no) {
			h->oom = 1;
			return 0;
		}
		h->cap = ncap;
	}
	h->raw[h->count] = raw;
	h->token_v[h->count] = (int64_t)(intptr_t)((Sym *)(uintptr_t)raw)->v;
	h->role[h->count] = (uint8_t)role;
	h->count++;
	return h->count;
}

static void mccjit_handles_expand(MccjitHandles *h, uint32_t i) {
	Sym *s = (Sym *)(uintptr_t)h->raw[i];
	Sym *p;
	if (!s)
		return;
	switch (h->role[i]) {
	case MCCJIT_ROLE_PTR:
		if (s->type.ref)
			mccjit_handles_intern(h, (uint64_t)(uintptr_t)s->type.ref,
														mccjit_role_for_base(s->type.t));
		break;
	case MCCJIT_ROLE_FUNC:
		if (s->type.ref)
			mccjit_handles_intern(h, (uint64_t)(uintptr_t)s->type.ref,
														mccjit_role_for_base(s->type.t));
		for (p = s->next; p; p = p->next)
			if (p->type.ref)
				mccjit_handles_intern(h, (uint64_t)(uintptr_t)p->type.ref,
															mccjit_role_for_base(p->type.t));
		break;
	case MCCJIT_ROLE_NAMED:
		if (s->type.ref)
			mccjit_handles_intern(h, (uint64_t)(uintptr_t)s->type.ref,
														mccjit_role_for_base(s->type.t));
		break;
	case MCCJIT_ROLE_STRUCT:
		for (p = s->next; p; p = p->next)
			if (p->type.ref)
				mccjit_handles_intern(h, (uint64_t)(uintptr_t)p->type.ref,
															mccjit_role_for_base(p->type.t));
		break;
	default:
		break;
	}
}

static void mccjit_emit_type_record(MccjitBuf *buf, MccjitHandles *h, uint32_t i) {
	Sym *s = (Sym *)(uintptr_t)h->raw[i];
	unsigned role = h->role[i];
	mccjit_put_u8(buf, (uint8_t)role);
	switch (role) {
	case MCCJIT_ROLE_NAMED:
	case MCCJIT_ROLE_PTR:
		mccjit_put_u32(buf, (uint32_t)s->type.t);
		mccjit_put_u32(buf, s->type.ref
													 ? mccjit_handles_intern(
																 h, (uint64_t)(uintptr_t)s->type.ref,
																 mccjit_role_for_base(s->type.t))
													 : 0);
		break;
	case MCCJIT_ROLE_FUNC: {
		Sym *p;
		uint32_t np = 0;
		mccjit_put_u32(buf, (uint32_t)s->type.t);
		mccjit_put_u32(buf, s->type.ref
													 ? mccjit_handles_intern(
																 h, (uint64_t)(uintptr_t)s->type.ref,
																 mccjit_role_for_base(s->type.t))
													 : 0);
		mccjit_put_u32(buf, (uint32_t)s->f.func_type);
		mccjit_put_u32(buf, (uint32_t)s->f.func_call);
		for (p = s->next; p; p = p->next)
			np++;
		mccjit_put_u32(buf, np);
		for (p = s->next; p; p = p->next) {
			mccjit_put_u32(buf, (uint32_t)p->type.t);
			mccjit_put_u32(buf, p->type.ref
														 ? mccjit_handles_intern(
																	 h, (uint64_t)(uintptr_t)p->type.ref,
																	 mccjit_role_for_base(p->type.t))
														 : 0);
		}
		break;
	}
	case MCCJIT_ROLE_STRUCT: {
		Sym *p;
		uint32_t nf = 0;
		mccjit_put_u32(buf, (uint32_t)s->type.t);
		mccjit_put_u32(buf, (uint32_t)s->c);
		mccjit_put_u32(buf, (uint32_t)s->r);
		for (p = s->next; p; p = p->next)
			nf++;
		mccjit_put_u32(buf, nf);
		for (p = s->next; p; p = p->next) {
			int ft = (p->v & ~(SYM_FIELD | SYM_STRUCT));
			const char *fn = (ft >= TOK_IDENT && ft < SYM_FIRST_ANOM)
													 ? get_tok_str(ft, NULL)
													 : "";
			mccjit_put_str(buf, fn);
			mccjit_put_u32(buf, (uint32_t)p->c);
			mccjit_put_u32(buf, (uint32_t)p->type.t);
			mccjit_put_u32(buf, p->type.ref
														 ? mccjit_handles_intern(
																	 h, (uint64_t)(uintptr_t)p->type.ref,
																	 mccjit_role_for_base(p->type.t))
														 : 0);
		}
		break;
	}
	default:
		break;
	}
}

MCCJIT_LOCAL int mccjit_intent_serialize(const AstArena *a, Sym *sym, MccjitBuf *buf) {
	MccjitHandles handles;
	AstLocal count, n;
	uint32_t k;
	if (!a || !buf)
		return -1;
	count = ast_count(a);
	mccjit_handles_init(&handles);

	for (n = 0; n < count; n++) {
		uint64_t tref = ast_type_ref(a, n);
		uint64_t nsym = ast_sym(a, n);
		if (tref)
			mccjit_handles_intern(&handles, tref,
														mccjit_role_for_base(ast_type_t(a, n)));
		if (nsym)
			mccjit_handles_intern(&handles, nsym,
														(ast_op(a, n) & VT_SYM) ? MCCJIT_ROLE_NAMED
																										: MCCJIT_ROLE_PLAIN);
	}
	for (k = 0; k < handles.count; k++)
		mccjit_handles_expand(&handles, k);
	if (handles.oom) {
		mccjit_handles_free(&handles);
		return -1;
	}

	mccjit_put_u32(buf, MCCJIT_INTENT_MAGIC);
	mccjit_put_u32(buf, MCCJIT_INTENT_FORMAT);
	mccjit_put_u64(buf, mccjit_salt_witness());
	mccjit_put_u64(buf, sym ? (uint64_t)(int64_t)sym->v : 0);
	mccjit_put_u32(buf, (uint32_t)count);
	mccjit_put_u32(buf, (uint32_t)ast_root(a));

	mccjit_put_u32(buf, handles.count);
	for (k = 0; k < handles.count; k++) {
		int64_t tv = handles.token_v[k];
		mccjit_put_u64(buf, handles.raw[k]);
		mccjit_put_u64(buf, (uint64_t)tv);
		if (tv >= TOK_IDENT && tv < SYM_FIRST_ANOM)
			mccjit_put_str(buf, get_tok_str((int)tv, NULL));
		else
			mccjit_put_str(buf, "");
		mccjit_emit_type_record(buf, &handles, k);
	}

	for (n = 0; n < count; n++) {
		uint32_t nc = ast_nchild(a, n), i;
		uint64_t nsym = ast_sym(a, n);
		uint64_t tref = ast_type_ref(a, n);
		mccjit_put_u16(buf, ast_kind(a, n));
		mccjit_put_u32(buf, (uint32_t)ast_op(a, n));
		mccjit_put_u32(buf, (uint32_t)ast_type_t(a, n));
		mccjit_put_u64(buf, ast_ival(a, n));
		mccjit_put_u64(buf, ast_fbits(a, n));
		mccjit_put_u32(buf, nsym ? mccjit_handles_intern(
															 &handles, nsym,
															 (ast_op(a, n) & VT_SYM) ? MCCJIT_ROLE_NAMED
																											 : MCCJIT_ROLE_PLAIN)
														 : 0);
		mccjit_put_u32(buf, tref ? mccjit_handles_intern(
															 &handles, tref, mccjit_role_for_base(ast_type_t(a, n)))
														 : 0);
		mccjit_put_u64(buf, ast_cst(a, n));
		mccjit_put_u32(buf, nc);
		for (i = 0; i < nc; i++)
			mccjit_put_u32(buf, (uint32_t)ast_child(a, n, i));
	}

	{
		Sym *sig = sym ? sym->type.ref : NULL;
		Sym *p;
		uint32_t np = 0;
		const char *fname = (sym && sym->v >= TOK_IDENT && sym->v < SYM_FIRST_ANOM)
														? get_tok_str(sym->v, NULL)
														: "";
		mccjit_put_str(buf, fname);
		mccjit_put_u32(buf, sig ? (uint32_t)sig->type.t : (uint32_t)VT_INT);
		mccjit_put_u32(buf, sig ? (uint32_t)sig->f.func_type : (uint32_t)FUNC_NEW);
		for (p = sig ? sig->next : NULL; p; p = p->next)
			np++;
		mccjit_put_u32(buf, np);
		for (p = sig ? sig->next : NULL; p; p = p->next) {
			const char *pn = (p->v >= TOK_IDENT && p->v < SYM_FIRST_ANOM)
													 ? get_tok_str(p->v, NULL)
													 : "";
			Sym *ls = (p->v >= TOK_IDENT && p->v < SYM_FIRST_ANOM) ? sym_find(p->v) : NULL;
			int64_t poff = (ls && (ls->r & VT_VALMASK) == VT_LOCAL) ? (int64_t)ls->c
																															: (int64_t)p->c;
			mccjit_put_u32(buf, (uint32_t)p->type.t);
			mccjit_put_u64(buf, (uint64_t)poff);
			mccjit_put_str(buf, pn);
		}
	}

	mccjit_handles_free(&handles);
	return buf->oom ? -1 : 0;
}

typedef struct MccjitTypeRec {
	uint8_t role;
	uint8_t building;
	uint8_t done;
	uint32_t a;
	uint32_t b;
	uint32_t c;
	uint32_t d;
	uint32_t nparam;
	uint32_t *pt;
	uint32_t *pr;
	uint32_t *foff;
	char **fnm;
	struct Sym *built;
} MccjitTypeRec;

typedef struct MccjitIntent {
	AstArena *arena;
	uint64_t salt;
	int64_t anchor_sym_v;
	uint32_t handle_count;
	uint64_t *handle_raw;
	int64_t *handle_token_v;
	char **handle_name;
	MccjitTypeRec *recs;
	int has_external;
	char *fn_name;
	uint32_t ret_type_t;
	uint32_t func_type;
	uint32_t nparam;
	uint32_t *param_type_t;
	int64_t *param_off;
	char **param_name;
} MccjitIntent;

MCCJIT_LOCAL void mccjit_intent_release(MccjitIntent *it) {
	uint32_t i;
	if (!it)
		return;
	if (it->arena)
		ast_arena_free(it->arena);
	if (it->handle_name)
		for (i = 0; i < it->handle_count; i++)
			mcc_free(it->handle_name[i]);
	if (it->param_name)
		for (i = 0; i < it->nparam; i++)
			mcc_free(it->param_name[i]);
	if (it->recs)
		for (i = 0; i < it->handle_count; i++) {
			uint32_t j;
			if (it->recs[i].fnm)
				for (j = 0; j < it->recs[i].nparam; j++)
					mcc_free(it->recs[i].fnm[j]);
			mcc_free(it->recs[i].fnm);
			mcc_free(it->recs[i].foff);
			mcc_free(it->recs[i].pt);
			mcc_free(it->recs[i].pr);
		}
	mcc_free(it->recs);
	mcc_free(it->handle_raw);
	mcc_free(it->handle_token_v);
	mcc_free(it->handle_name);
	mcc_free(it->fn_name);
	mcc_free(it->param_type_t);
	mcc_free(it->param_off);
	mcc_free(it->param_name);
	it->arena = NULL;
	it->handle_raw = NULL;
	it->handle_token_v = NULL;
	it->handle_name = NULL;
	it->recs = NULL;
	it->fn_name = NULL;
	it->param_type_t = NULL;
	it->param_off = NULL;
	it->param_name = NULL;
}

static Sym *mccjit_build_rec(MccjitIntent *it, uint32_t id1) {
	uint32_t i;
	MccjitTypeRec *r;
	Sym *res = NULL;
	if (!id1 || id1 > it->handle_count || !it->recs)
		return NULL;
	i = id1 - 1;
	r = &it->recs[i];
	if (r->done)
		return r->built;
	if (r->building)
		return NULL;
	r->building = 1;
	switch (r->role) {
	case MCCJIT_ROLE_PTR: {
		CType pc;
		pc.t = (int)r->a;
		pc.ref = mccjit_build_rec(it, r->b);
		res = sym_push(SYM_FIELD, &pc, 0, -1);
		break;
	}
	case MCCJIT_ROLE_FUNC: {
		Sym *sr = sym_push2(&global_stack, SYM_FIELD, 0, 0);
		Sym *first = NULL, **plast = &first, *p;
		uint32_t k;
		sr->type.t = (int)r->a;
		sr->type.ref = mccjit_build_rec(it, r->b);
		sr->f.func_call = (int)r->d;
		sr->f.func_type = r->c ? (int)r->c : FUNC_NEW;
		sr->f.func_args = r->nparam;
		for (k = 0; k < r->nparam; k++) {
			p = sym_push2(&global_stack, SYM_FIELD, (int)r->pt[k], 0);
			p->type.ref = mccjit_build_rec(it, r->pr[k]);
			p->r = VT_LOCAL | VT_LVAL;
			p->a.inited = 1;
			*plast = p;
			plast = &p->next;
		}
		sr->next = first;
		res = sr;
		break;
	}
	case MCCJIT_ROLE_STRUCT: {
		Sym *first = NULL, **plast = &first, *p;
		uint32_t k;
		Sym *ss = sym_push2(&global_stack, anon_sym++ | SYM_STRUCT, (int)r->a, (int)r->c);
		ss->r = (unsigned short)r->d;
		r->built = ss;
		r->done = 1;
		r->building = 0;
		res = ss;
		for (k = 0; k < r->nparam; k++) {
			const char *fn = it->recs[i].fnm ? it->recs[i].fnm[k] : NULL;
			int ftok = (fn && fn[0]) ? (tok_alloc(fn, (int)strlen(fn))->tok | SYM_FIELD)
															 : (anon_sym++ | SYM_FIELD);
			p = sym_push2(&global_stack, ftok, (int)r->pt[k], (int)r->foff[k]);
			p->type.ref = mccjit_build_rec(it, r->pr[k]);
			*plast = p;
			plast = &p->next;
		}
		ss->next = first;
		break;
	}
	case MCCJIT_ROLE_NAMED: {
		CType ct;
		const char *nm = it->handle_name ? it->handle_name[i] : NULL;
		if (nm && nm[0]) {
			ct.t = (int)r->a;
			ct.ref = mccjit_build_rec(it, r->b);
			res = external_global_sym(tok_alloc(nm, (int)strlen(nm))->tok, &ct);
		}
		break;
	}
	default:
		break;
	}
	r->built = res;
	r->done = 1;
	r->building = 0;
	return res;
}

MCCJIT_LOCAL int mccjit_intent_deserialize(const void *buf, size_t len,
																					 MccjitIntent *out) {
	MccjitReader r;
	AstArena *a;
	uint32_t count, i, hc;
	uint32_t *nc_of = NULL;
	uint32_t **kids = NULL;
	int rc = -1;

	if (!buf || !out)
		return -1;
	memset(out, 0, sizeof *out);
	r.data = buf;
	r.len = len;
	r.pos = 0;
	r.err = 0;

	if (mccjit_get_u32(&r) != MCCJIT_INTENT_MAGIC)
		return -1;
	if (mccjit_get_u32(&r) != MCCJIT_INTENT_FORMAT)
		return -1;
	out->salt = mccjit_get_u64(&r);
	out->anchor_sym_v = (int64_t)mccjit_get_u64(&r);
	count = mccjit_get_u32(&r);
	(void)mccjit_get_u32(&r);

	hc = mccjit_get_u32(&r);
	if (r.err)
		return -1;
	if (hc) {
		out->handle_raw = mcc_malloc(hc * sizeof *out->handle_raw);
		out->handle_token_v = mcc_malloc(hc * sizeof *out->handle_token_v);
		out->handle_name = mcc_mallocz(hc * sizeof *out->handle_name);
		out->recs = mcc_mallocz(hc * sizeof *out->recs);
		if (!out->handle_raw || !out->handle_token_v || !out->handle_name ||
				!out->recs)
			goto done;
	}
	out->handle_count = hc;
	for (i = 0; i < hc; i++) {
		uint64_t raw = mccjit_get_u64(&r);
		int64_t tv = (int64_t)mccjit_get_u64(&r);
		char *nm = mccjit_get_str(&r);
		MccjitTypeRec *rec = &out->recs[i];
		if (r.err)
			goto done;
		out->handle_raw[i] = raw;
		out->handle_name[i] = nm;
		if (nm && nm[0] && mcc_state)
			tv = tok_alloc(nm, (int)strlen(nm))->tok;
		out->handle_token_v[i] = tv;
		rec->role = mccjit_get_u8(&r);
		if (rec->role == MCCJIT_ROLE_NAMED)
			out->has_external = 1;
		switch (rec->role) {
		case MCCJIT_ROLE_NAMED:
		case MCCJIT_ROLE_PTR:
			rec->a = mccjit_get_u32(&r);
			rec->b = mccjit_get_u32(&r);
			break;
		case MCCJIT_ROLE_FUNC: {
			uint32_t k;
			rec->a = mccjit_get_u32(&r);
			rec->b = mccjit_get_u32(&r);
			rec->c = mccjit_get_u32(&r);
			rec->d = mccjit_get_u32(&r);
			rec->nparam = mccjit_get_u32(&r);
			if (r.err)
				goto done;
			if (rec->nparam) {
				rec->pt = mcc_mallocz(rec->nparam * sizeof *rec->pt);
				rec->pr = mcc_mallocz(rec->nparam * sizeof *rec->pr);
				if (!rec->pt || !rec->pr)
					goto done;
			}
			for (k = 0; k < rec->nparam; k++) {
				rec->pt[k] = mccjit_get_u32(&r);
				rec->pr[k] = mccjit_get_u32(&r);
			}
			break;
		}
		case MCCJIT_ROLE_STRUCT: {
			uint32_t k;
			rec->a = mccjit_get_u32(&r);
			rec->c = mccjit_get_u32(&r);
			rec->d = mccjit_get_u32(&r);
			rec->nparam = mccjit_get_u32(&r);
			if (r.err)
				goto done;
			if (rec->nparam) {
				rec->pt = mcc_mallocz(rec->nparam * sizeof *rec->pt);
				rec->pr = mcc_mallocz(rec->nparam * sizeof *rec->pr);
				rec->foff = mcc_mallocz(rec->nparam * sizeof *rec->foff);
				rec->fnm = mcc_mallocz(rec->nparam * sizeof *rec->fnm);
				if (!rec->pt || !rec->pr || !rec->foff || !rec->fnm)
					goto done;
			}
			for (k = 0; k < rec->nparam; k++) {
				rec->fnm[k] = mccjit_get_str(&r);
				rec->foff[k] = mccjit_get_u32(&r);
				rec->pt[k] = mccjit_get_u32(&r);
				rec->pr[k] = mccjit_get_u32(&r);
			}
			break;
		}
		default:
			break;
		}
		if (r.err)
			goto done;
	}
	if (r.err)
		goto done;

	if (mcc_state)
		for (i = 0; i < hc; i++)
			mccjit_build_rec(out, i + 1);

	a = ast_arena_new();
	if (!a)
		goto done;
	out->arena = a;

	if (count) {
		nc_of = mcc_mallocz((count) * (sizeof *nc_of));
		kids = mcc_mallocz((count) * (sizeof *kids));
		if (!nc_of || !kids)
			goto done;
	}

	for (i = 0; i < count; i++) {
		uint16_t kind = mccjit_get_u16(&r);
		int32_t op = (int32_t)mccjit_get_u32(&r);
		int32_t type_t = (int32_t)mccjit_get_u32(&r);
		uint64_t ival = mccjit_get_u64(&r);
		uint64_t fbits = mccjit_get_u64(&r);
		uint32_t sym_id = mccjit_get_u32(&r);
		uint32_t typeref_id = mccjit_get_u32(&r);
		uint64_t cst = mccjit_get_u64(&r);
		uint32_t nc = mccjit_get_u32(&r), j;
		Sym *sym_new = (sym_id && sym_id <= hc) ? mccjit_build_rec(out, sym_id) : NULL;
		Sym *tref_new =
				(typeref_id && typeref_id <= hc) ? mccjit_build_rec(out, typeref_id) : NULL;
		uint64_t sym_raw = (uint64_t)(uintptr_t)sym_new;
		uint64_t tref_raw = (uint64_t)(uintptr_t)tref_new;
		AstLocal node;
		if (r.err)
			goto done;
		if ((op & VT_VALMASK) == VT_LOCAL && !(op & VT_SYM))
			sym_raw = 0;
		node = ast_node(a, kind);
		ast_set_op(a, node, op);
		ast_set_type(a, node, type_t, tref_raw);
		ast_set_ival(a, node, ival);
		ast_set_fbits(a, node, fbits);
		ast_set_sym(a, node, sym_raw);
		ast_set_cst(a, node, cst);
		nc_of[i] = nc;
		if (nc) {
			kids[i] = mcc_malloc(nc * sizeof **kids);
			if (!kids[i])
				goto done;
			for (j = 0; j < nc; j++)
				kids[i][j] = mccjit_get_u32(&r);
		}
	}
	if (r.err)
		goto done;

	for (i = 0; i < count; i++) {
		uint32_t j;
		for (j = 0; j < nc_of[i]; j++)
			if (kids[i][j] < count)
				ast_add_child(a, i, kids[i][j]);
	}

	out->fn_name = mccjit_get_str(&r);
	out->ret_type_t = mccjit_get_u32(&r);
	out->func_type = mccjit_get_u32(&r);
	out->nparam = mccjit_get_u32(&r);
	if (r.err)
		goto done;
	if (out->nparam) {
		out->param_type_t = mcc_mallocz(out->nparam * sizeof *out->param_type_t);
		out->param_off = mcc_mallocz(out->nparam * sizeof *out->param_off);
		out->param_name = mcc_mallocz(out->nparam * sizeof *out->param_name);
		if (!out->param_type_t || !out->param_off || !out->param_name)
			goto done;
	}
	for (i = 0; i < out->nparam; i++) {
		out->param_type_t[i] = mccjit_get_u32(&r);
		out->param_off[i] = (int64_t)mccjit_get_u64(&r);
		out->param_name[i] = mccjit_get_str(&r);
		if (r.err)
			goto done;
	}
	rc = 0;

done:
	if (kids)
		for (i = 0; i < count; i++)
			mcc_free(kids[i]);
	mcc_free(kids);
	mcc_free(nc_of);
	if (rc != 0)
		mccjit_intent_release(out);
	return rc;
}

MCCJIT_LOCAL Sym *mccjit_rebuild_sym(const MccjitIntent *it) {
	CType functype;
	Sym *sr, *first = NULL, **plast = &first, *p;
	uint32_t i;
	if (!it || !it->fn_name || !it->fn_name[0])
		return NULL;

	sr = sym_push2(&global_stack, SYM_FIELD, 0, 0);
	sr->type.t = (int)it->ret_type_t;
	sr->type.ref = NULL;
	sr->f.func_call = FUNC_CDECL;
	sr->f.func_type = it->func_type ? (int)it->func_type : FUNC_NEW;
	sr->f.func_args = it->nparam;

	for (i = 0; i < it->nparam; i++) {
		const char *pn = it->param_name ? it->param_name[i] : NULL;
		int ptok = (pn && pn[0]) ? tok_alloc(pn, (int)strlen(pn))->tok : SYM_FIELD;
		p = sym_push2(&global_stack, ptok, (int)it->param_type_t[i], 0);
		p->type.ref = NULL;
		p->r = VT_LOCAL | VT_LVAL;
		p->a.inited = 1;
		*plast = p;
		plast = &p->next;
	}
	sr->next = first;

	functype.t = VT_FUNC;
	functype.ref = sr;
	return external_global_sym(tok_alloc(it->fn_name, (int)strlen(it->fn_name))->tok,
														 &functype);
}

static void mccjit_perf_map_emit(MCCState *js, const char *name, void *addr) {
	char path[64];
	FILE *f;
	size_t size = 0;
	int si;
	if (!getenv("MCC_JIT_PERF_MAP") || !addr || !name || !name[0] || !js ||
			!js->symtab)
		return;
	si = find_elf_sym(js->symtab, name);
	if (si > 0)
		size = (size_t)((ElfSym *)js->symtab->data)[si].st_size;
	if (!size)
		size = 16;
	snprintf(path, sizeof path, "/tmp/perf-%d.map", (int)getpid());
	f = fopen(path, "a");
	if (!f)
		return;
	fprintf(f, "%lx %lx %s\n", (unsigned long)(uintptr_t)addr,
					(unsigned long)size, name);
	fclose(f);
}

static void *mccjit_recompile_common(const void *buf, size_t len, int do_spec,
																		 int param_index, int64_t const_val) {
	MccjitIntent it;
	MCCState *js;
	Sym *sym;
	void *entry = NULL;

	js = mcc_new();
	if (!js)
		return NULL;
	js->optimize = 0;
	js->nostdlib = 1;
	mcc_set_output_type(js, MCC_OUTPUT_MEMORY);

	mcc_enter_state(js);
	mccpp_new(js);
	mccgen_init(js);
	anon_sym = SYM_FIRST_ANOM;
	funcname = "";
	func_ind = -1;

	if (mccjit_intent_deserialize(buf, len, &it) != 0) {
		mcc_exit_state(js);
		mcc_delete(js);
		return NULL;
	}

	if (it.has_external)
		js->nostdlib = 0;

	mccjit_last_purity = ast_fn_purity(it.arena);

	{
		uint32_t qi;
		int allfp;
		mccjit_last_nparam = it.nparam;
		mccjit_last_ret_wide = mccjit_type_wide((int)it.ret_type_t);
		/* K4A all-double (SSE) class: 1-6 double params + a double return. */
		allfp = (it.nparam >= 1 && it.nparam <= MCCJIT_KGC_MAXARG &&
						 ((int)it.ret_type_t & VT_BTYPE) == VT_DOUBLE);
		for (qi = 0; allfp && qi < it.nparam && qi < MCCJIT_KGC_MAXARG; qi++)
			if (((int)it.param_type_t[qi] & VT_BTYPE) != VT_DOUBLE)
				allfp = 0;
		mccjit_last_allfp = allfp;
		mccjit_last_kgc_ok = 1;
		if (it.func_type == FUNC_ELLIPSIS)
			mccjit_last_kgc_ok = 0;
		if (it.nparam < 1 || it.nparam > MCCJIT_KGC_MAXARG)
			mccjit_last_kgc_ok = 0;
		if (!allfp &&
				(!mccjit_type_gp((int)it.ret_type_t) || (it.ret_type_t & VT_BITFIELD)))
			mccjit_last_kgc_ok = 0;
		for (qi = 0; qi < MCCJIT_KGC_MAXARG; qi++)
			mccjit_last_param_t[qi] = 0;
		for (qi = 0; qi < it.nparam && qi < MCCJIT_KGC_MAXARG; qi++) {
			mccjit_last_param_t[qi] = it.param_type_t[qi];
			if (!allfp && (!mccjit_type_gp((int)it.param_type_t[qi]) ||
										 (it.param_type_t[qi] & VT_BITFIELD)))
				mccjit_last_kgc_ok = 0;
		}
	}

	if (do_spec && param_index >= 0 && (uint32_t)param_index < it.nparam)
		mccjit_ast_spec_fold(it.arena, (int)it.param_off[param_index], const_val);

	sym = mccjit_rebuild_sym(&it);
	if (sym)
		ast_reemit_extern(sym, it.arena);
	mcc_exit_state(js);

	if (sym && mcc_relocate(js) == 0)
		entry = mcc_get_symbol(js, it.fn_name);

	if (entry)
		mccjit_perf_map_emit(js, it.fn_name, entry);

	mccjit_intent_release(&it);

	if (entry) {
		mccjit_last_state = js;
	} else {
		mcc_delete(js);
	}
	return entry;
}

MCCJIT_LOCAL void *mcc_jit_recompile_blob(const void *buf, size_t len) {
	return mccjit_recompile_common(buf, len, 0, -1, 0);
}

MCCJIT_LOCAL void *mcc_jit_recompile_blob_spec(const void *buf, size_t len,
																							 int param_index, int64_t const_val) {
	return mccjit_recompile_common(buf, len, 1, param_index, const_val);
}

MCCJIT_LOCAL void *mcc_jit_recompile(Sym *sym, const void *ctxkey) {
	(void)sym;
	(void)ctxkey;
	if (!mccjit_last_blob)
		return NULL;
	return mcc_jit_recompile_blob(mccjit_last_blob, mccjit_last_len);
}

void mccjit_embed_stash_leaf(AstArena *ast, Sym *sym) {
	MccjitBuf b;
	if (!ast || !sym)
		return;
	mccjit_buf_init(&b);
	if (mccjit_intent_serialize(ast, sym, &b) != 0) {
		mccjit_buf_free(&b);
		return;
	}
	mcc_free(mccjit_last_blob);
	mccjit_last_blob = b.data;
	mccjit_last_len = b.len;
}

typedef struct MccjitEmbedFn {
	char *name;
	unsigned char *blob;
	size_t len;
	struct MccjitEmbedFn *next;
} MccjitEmbedFn;

MCCJIT_LOCAL MccjitEmbedFn *mccjit_embed_fns;

void mccjit_embed_note(const char *name, AstArena *ast, Sym *sym) {
	MccjitBuf b;
	MccjitEmbedFn *e;
	if (!name || !name[0] || !ast || !sym)
		return;
	for (e = mccjit_embed_fns; e; e = e->next)
		if (!strcmp(e->name, name))
			return;
	mccjit_buf_init(&b);
	if (mccjit_intent_serialize(ast, sym, &b) != 0) {
		mccjit_buf_free(&b);
		return;
	}
	e = mcc_mallocz(sizeof *e);
	if (!e) {
		mccjit_buf_free(&b);
		return;
	}
	e->name = mcc_strdup(name);
	e->blob = b.data;
	e->len = b.len;
	e->next = mccjit_embed_fns;
	mccjit_embed_fns = e;
}

#if defined(__x86_64__)
#include <sys/mman.h>
static void *mccjit_make_trampoline(void *variant) {
	unsigned char *p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
													MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		return variant;
	p[0] = 0xc9;
	p[1] = 0x48;
	p[2] = 0xb8;
	memcpy(p + 3, &variant, 8);
	p[11] = 0xff;
	p[12] = 0xe0;
	return p;
}
#else
static void *mccjit_make_trampoline(void *variant) { return variant; }
#endif

static double mccjit_elapsed(const struct timespec *t0) {
	struct timespec t1;
	if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0)
		return -1.0;
	return (double)(t1.tv_sec - t0->tv_sec) +
				 (double)(t1.tv_nsec - t0->tv_nsec) / 1000000000.0;
}

static void *mccjit_make_kgc_stub_n(void *variant, void *baseline, int memoize_ok,
																		const uint32_t *param_t, uint32_t nargs,
																		int ret_wide);
static void *mccjit_make_kgc_stub_fp(void *variant, void *baseline,
																		 int memoize_ok, uint32_t nargs);

static void mccjit_boot_swap_run(void **slot, const void *blob, unsigned long len,
																 unsigned long max_duration, const char *mode,
																 const struct timespec *t0, int timed) {
	void *variant = NULL;
	void *baseline = NULL;
	void *aot_init = slot ? *slot : NULL;
	void *entry = NULL;
	int over = 0;
	int skipped = 0;
	int routed = 0;
	int no_kgc = getenv("MCC_JIT_NO_KGC") != NULL;
	int spec_wrong = getenv("MCC_JIT_SPEC_WRONG") != NULL;
	if (timed && max_duration && mccjit_elapsed(t0) > (double)max_duration) {
		skipped = 1;
	} else {
		variant = spec_wrong
									? mcc_jit_recompile_blob_spec(blob, (size_t)len, 0, 7)
									: mcc_jit_recompile_blob(blob, (size_t)len);
		if (variant && !no_kgc && mccjit_last_kgc_ok) {
			int memoize_ok;
			uint32_t nargs = mccjit_last_nparam;
			int ret_wide = mccjit_last_ret_wide;
			int all_fp = mccjit_last_allfp;
			uint32_t ptypes[MCCJIT_KGC_MAXARG];
			uint32_t qi;
			for (qi = 0; qi < MCCJIT_KGC_MAXARG; qi++)
				ptypes[qi] = mccjit_last_param_t[qi];
			baseline = mcc_jit_recompile_blob(blob, (size_t)len);
			memoize_ok = (mccjit_last_purity == AST_PURITY_TIER0);
			if (baseline) {
				entry = all_fp ? mccjit_make_kgc_stub_fp(variant, baseline, memoize_ok,
																								 nargs)
											 : mccjit_make_kgc_stub_n(variant, baseline, memoize_ok,
																								ptypes, nargs, ret_wide);
				if (entry)
					routed = 1;
			}
		}
		if (!entry && no_kgc)
			entry = variant ? mccjit_make_trampoline(variant) : NULL;
		if (timed && max_duration && entry &&
				mccjit_elapsed(t0) > (double)max_duration) {
			over = 1;
			entry = NULL;
		}
	}
	if (getenv("MCC_JIT_VERBOSE")) {
		int probeable = variant && mccjit_last_nparam == 1 &&
										!mccjit_type_wide((int)mccjit_last_param_t[0]);
		int probe = probeable ? ((int (*)(int))variant)(7) : -1;
		fprintf(stderr,
						"mccjit-boot[%s]: slot=%p aot=%p blob=%p len=%lu variant=%p baseline=%p entry=%p route=%s np=%u probe(7)=%d %s\n",
						mode, (void *)slot, aot_init, blob, len, variant, baseline, entry,
						routed ? "kgc" : "direct", mccjit_last_nparam, probe,
						skipped				 ? "budget-skip"
						: over					 ? "over-budget-kept-aot"
						: entry					 ? "swapped"
						: (variant && !no_kgc && !mccjit_last_kgc_ok)
								? "refused-unverified"
								: "kept-aot");
	}
	if (entry)
		mcc_jit_publish(slot, entry);
}

static void *mccjit_lazy_build(const void *blob, unsigned long len, int *routed) {
	int no_kgc = getenv("MCC_JIT_NO_KGC") != NULL;
	int spec_wrong = getenv("MCC_JIT_SPEC_WRONG") != NULL;
	void *variant = spec_wrong
											? mcc_jit_recompile_blob_spec(blob, (size_t)len, 0, 7)
											: mcc_jit_recompile_blob(blob, (size_t)len);
	void *entry = NULL;
	if (routed)
		*routed = 0;
	if (variant && !no_kgc && mccjit_last_kgc_ok) {
		uint32_t nargs = mccjit_last_nparam;
		int ret_wide = mccjit_last_ret_wide;
		int all_fp = mccjit_last_allfp;
		uint32_t ptypes[MCCJIT_KGC_MAXARG];
		uint32_t qi;
		void *baseline;
		int memoize_ok;
		for (qi = 0; qi < MCCJIT_KGC_MAXARG; qi++)
			ptypes[qi] = mccjit_last_param_t[qi];
		baseline = mcc_jit_recompile_blob(blob, (size_t)len);
		memoize_ok = (mccjit_last_purity == AST_PURITY_TIER0);
		if (baseline) {
			entry = all_fp
									? mccjit_make_kgc_stub_fp(variant, baseline, memoize_ok, nargs)
									: mccjit_make_kgc_stub_n(variant, baseline, memoize_ok, ptypes,
																					 nargs, ret_wide);
			if (entry && routed)
				*routed = 1;
		}
	}
	if (!entry && no_kgc)
		entry = variant ? mccjit_make_trampoline(variant) : NULL;
	return entry;
}

#define MCCJIT_PROFILE_SAMPLES 8

typedef struct MccjitCounterState {
	void **slot;
	const void *blob;
	unsigned long len;
	void *baseline;
	long threshold;
	long count;
	void *promoted;
	int building;
	long argseen;
	int nsample;
	int64_t argmin[MCCJIT_KGC_MAXARG];
	int64_t argmax[MCCJIT_KGC_MAXARG];
	int64_t sample[MCCJIT_PROFILE_SAMPLES][MCCJIT_KGC_MAXARG];
	pthread_mutex_t lock;
} MccjitCounterState;

/* J6A jit-profile: runtime live-in capture riding the D5 hot counter. The
   counter stub spills the 6 GP arg registers and hands their address to the
   tick as `regs`; regs[MCCJIT_KGC_MAXARG-1-i] is param i (rdi..r9 pushed in
   order, so the pointer walks r9..rdi). We accumulate a per-param min/max
   range (feeds dispatch mode 5's range guard + J7A) and a small ring of real
   observed tuples (the safe live-in set for the K5 promotion benchmark). */
static void mccjit_counter_capture(MccjitCounterState *st, const int64_t *regs) {
	int i;
	for (i = 0; i < MCCJIT_KGC_MAXARG; i++) {
		int64_t v = regs[MCCJIT_KGC_MAXARG - 1 - i];
		if (st->argseen == 0) {
			st->argmin[i] = v;
			st->argmax[i] = v;
		} else {
			if (v < st->argmin[i])
				st->argmin[i] = v;
			if (v > st->argmax[i])
				st->argmax[i] = v;
		}
	}
	if (st->nsample < MCCJIT_PROFILE_SAMPLES) {
		for (i = 0; i < MCCJIT_KGC_MAXARG; i++)
			st->sample[st->nsample][i] = regs[MCCJIT_KGC_MAXARG - 1 - i];
		st->nsample++;
	}
	st->argseen++;
}

/* J7A value-range speculation: the actionable fact from J6A's captured range
   is the collapsed case — a param observed to hold a single value over the
   whole profile (argmin==argmax) is a speculative constant. We only consider
   the first `nargs` params (the counter also captures unused arg registers,
   whose "range" is meaningless). Returns 1 + the param index/value to fold. */
static int mccjit_profile_pick_const(const MccjitCounterState *st, uint32_t nargs,
																		 long min_samples, int *pidx, int64_t *pval) {
	uint32_t i;
	if (!st || nargs == 0 || st->argseen < min_samples)
		return 0;
	for (i = 0; i < nargs && i < MCCJIT_KGC_MAXARG; i++) {
		if (st->argmin[i] == st->argmax[i]) {
			if (pidx)
				*pidx = (int)i;
			if (pval)
				*pval = st->argmin[i];
			return 1;
		}
	}
	return 0;
}

/* Build the hot variant, speculating on the J6A profile: if a param is
   observed constant, const-specialize on it (mode-4 fold). The speculation is
   guarded downstream by the KGC differential verify (a wrong fold returns the
   baseline result) and the K1C poison policy (a frequently-wrong fold is
   discarded), so it needs no static soundness proof here. Falls back to the
   plain recompile when the profile shows no constant param. */
MCCJIT_LOCAL void *mccjit_recompile_profiled(const void *blob, size_t len,
																						 const MccjitCounterState *st,
																						 uint32_t nargs, long min_samples) {
	int pidx = -1;
	int64_t pval = 0;
	if (mccjit_profile_pick_const(st, nargs, min_samples, &pidx, &pval))
		return mcc_jit_recompile_blob_spec(blob, len, pidx, pval);
	return mcc_jit_recompile_blob(blob, (size_t)len);
}

typedef struct MccjitSwapJob {
	void (*run)(struct MccjitSwapJob *);
	void **slot;
	const void *blob;
	unsigned long len;
	unsigned long max_duration;
	struct timespec start;
	int timed;
	MccjitCounterState *cst;
	struct MccjitSwapJob *next;
} MccjitSwapJob;

static pthread_mutex_t mccjit_swap_lock = PTHREAD_MUTEX_INITIALIZER;

static struct {
	MccjitSwapJob *head, *tail;
	pthread_mutex_t qlock;
	pthread_cond_t qcond;
	int started;
	int nworkers;
} mccjit_pool = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER,
								 0, 0};

static pthread_once_t mccjit_fork_once = PTHREAD_ONCE_INIT;

static void mccjit_atfork_prepare(void) {
	pthread_mutex_lock(&mccjit_pool.qlock);
	pthread_mutex_lock(&mccjit_swap_lock);
}

static void mccjit_atfork_parent(void) {
	pthread_mutex_unlock(&mccjit_swap_lock);
	pthread_mutex_unlock(&mccjit_pool.qlock);
}

static void mccjit_atfork_child(void) {
	mccjit_pool.head = mccjit_pool.tail = NULL;
	mccjit_pool.started = 0;
	mccjit_pool.nworkers = 0;
	pthread_cond_init(&mccjit_pool.qcond, NULL);
	pthread_mutex_unlock(&mccjit_swap_lock);
	pthread_mutex_unlock(&mccjit_pool.qlock);
}

static void mccjit_fork_setup(void) {
	pthread_atfork(mccjit_atfork_prepare, mccjit_atfork_parent,
								 mccjit_atfork_child);
}

static void *mccjit_pool_worker(void *arg) {
	(void)arg;
	for (;;) {
		MccjitSwapJob *job;
		pthread_mutex_lock(&mccjit_pool.qlock);
		while (!mccjit_pool.head)
			pthread_cond_wait(&mccjit_pool.qcond, &mccjit_pool.qlock);
		job = mccjit_pool.head;
		mccjit_pool.head = job->next;
		if (!mccjit_pool.head)
			mccjit_pool.tail = NULL;
		pthread_mutex_unlock(&mccjit_pool.qlock);
		pthread_mutex_lock(&mccjit_swap_lock);
		job->run(job);
		pthread_mutex_unlock(&mccjit_swap_lock);
		mcc_free(job);
	}
	return NULL;
}

static int mccjit_pool_start(unsigned long workers) {
	int n;
	pthread_once(&mccjit_fork_once, mccjit_fork_setup);
	pthread_mutex_lock(&mccjit_pool.qlock);
	if (!mccjit_pool.started) {
		int want = (int)workers;
		int i;
		if (want < 1)
			want = 1;
		mccjit_pool.started = 1;
		for (i = 0; i < want; i++) {
			pthread_t th;
			if (pthread_create(&th, NULL, mccjit_pool_worker, NULL) != 0)
				break;
			pthread_detach(th);
			mccjit_pool.nworkers++;
		}
		if (getenv("MCC_JIT_VERBOSE"))
			fprintf(stderr, "mccjit-pool[start]: requested=%d live=%d\n", want,
							mccjit_pool.nworkers);
	}
	n = mccjit_pool.nworkers;
	pthread_mutex_unlock(&mccjit_pool.qlock);
	return n;
}

static int mccjit_pool_ready(void) {
	return mccjit_pool.nworkers > 0;
}

static void mccjit_pool_enqueue(MccjitSwapJob *job) {
	pthread_mutex_lock(&mccjit_pool.qlock);
	job->next = NULL;
	if (mccjit_pool.tail)
		mccjit_pool.tail->next = job;
	else
		mccjit_pool.head = job;
	mccjit_pool.tail = job;
	pthread_cond_signal(&mccjit_pool.qcond);
	pthread_mutex_unlock(&mccjit_pool.qlock);
}

static void mccjit_job_run_eager(MccjitSwapJob *job) {
	mccjit_boot_swap_run(job->slot, job->blob, job->len, job->max_duration,
											 "async", &job->start, job->timed);
}

static void mccjit_job_run_lazy(MccjitSwapJob *job) {
	MccjitCounterState *st = job->cst;
	int routed = 0;
	void *entry = mccjit_lazy_build(st->blob, st->len, &routed);
	pthread_mutex_lock(&st->lock);
	if (entry) {
		st->promoted = entry;
		mcc_jit_publish(st->slot, entry);
	}
	st->building = 0;
	pthread_mutex_unlock(&st->lock);
	if (getenv("MCC_JIT_VERBOSE"))
		fprintf(stderr,
						"mccjit-lazy[promote-async]: slot=%p entry=%p route=%s %s\n",
						(void *)st->slot, entry, routed ? "kgc" : "direct",
						entry ? "promoted" : "build-failed");
}

static void *mccjit_counter_tick(MccjitCounterState *st, const int64_t *regs) {
	long n;
	void *target;
	int verbose = getenv("MCC_JIT_VERBOSE") != NULL;
	pthread_mutex_lock(&st->lock);
	n = ++st->count;
	if (regs && !st->promoted)
		mccjit_counter_capture(st, regs);
	if (st->promoted) {
		target = st->promoted;
	} else if (n < st->threshold) {
		if (verbose && n == 1)
			fprintf(stderr,
							"mccjit-lazy[cold]: slot=%p call=%ld<threshold=%ld running baseline=%p\n",
							(void *)st->slot, n, st->threshold, st->baseline);
		target = st->baseline;
	} else if (mccjit_pool_ready()) {
		if (!st->building) {
			MccjitSwapJob *job = mcc_malloc(sizeof *job);
			if (job) {
				job->run = mccjit_job_run_lazy;
				job->cst = st;
				st->building = 1;
				mccjit_pool_enqueue(job);
				if (verbose)
					fprintf(stderr,
									"mccjit-lazy[promote-async]: slot=%p hot after %ld calls -> queued\n",
									(void *)st->slot, n);
			}
		}
		target = st->baseline;
	} else {
		int routed = 0;
		void *entry = mccjit_lazy_build(st->blob, st->len, &routed);
		if (entry) {
			st->promoted = entry;
			mcc_jit_publish(st->slot, entry);
			target = entry;
			if (verbose)
				fprintf(stderr,
								"mccjit-lazy[promote]: slot=%p hot after %ld calls -> entry=%p route=%s\n",
								(void *)st->slot, n, entry, routed ? "kgc" : "direct");
		} else {
			target = st->baseline;
			if (verbose)
				fprintf(stderr,
								"mccjit-lazy[promote]: slot=%p build failed, staying cold\n",
								(void *)st->slot);
		}
	}
	pthread_mutex_unlock(&st->lock);
	return target;
}

#if defined(__x86_64__)
static void *mccjit_make_counter_stub(MccjitCounterState *st) {
	void *tick = (void *)mccjit_counter_tick;
	unsigned char *p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
													MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	size_t o = 0;
	if (p == MAP_FAILED)
		return NULL;
	p[o++] = 0x57;
	p[o++] = 0x56;
	p[o++] = 0x52;
	p[o++] = 0x51;
	p[o++] = 0x41;
	p[o++] = 0x50;
	p[o++] = 0x41;
	p[o++] = 0x51;
	p[o++] = 0x48;
	p[o++] = 0x89;
	p[o++] = 0xe6;
	p[o++] = 0x48;
	p[o++] = 0xbf;
	memcpy(p + o, &st, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0xb8;
	memcpy(p + o, &tick, 8);
	o += 8;
	p[o++] = 0xff;
	p[o++] = 0xd0;
	p[o++] = 0x41;
	p[o++] = 0x59;
	p[o++] = 0x41;
	p[o++] = 0x58;
	p[o++] = 0x59;
	p[o++] = 0x5a;
	p[o++] = 0x5e;
	p[o++] = 0x5f;
	p[o++] = 0xff;
	p[o++] = 0xe0;
	return p;
}
#else
static void *mccjit_make_counter_stub(MccjitCounterState *st) {
	(void)st;
	return NULL;
}
#endif

static int mccjit_lazy_install(void **slot, const void *blob, unsigned long len) {
	void *baseline = slot ? *slot : NULL;
	long threshold = 1000;
	const char *e = getenv("MCC_JIT_HOT_THRESHOLD");
	MccjitCounterState *st;
	void *stub;
	if (e && e[0]) {
		long v = strtol(e, NULL, 10);
		if (v > 0)
			threshold = v;
	}
	st = mcc_mallocz(sizeof *st);
	if (!st)
		return -1;
	st->slot = slot;
	st->blob = blob;
	st->len = len;
	st->baseline = baseline;
	st->threshold = threshold;
	st->count = 0;
	st->promoted = NULL;
	pthread_mutex_init(&st->lock, NULL);
	stub = mccjit_make_counter_stub(st);
	if (getenv("MCC_JIT_VERBOSE"))
		fprintf(stderr,
						"mccjit-lazy[install]: slot=%p baseline=%p blob=%p len=%lu threshold=%ld stub=%p\n",
						(void *)slot, baseline, blob, len, threshold, stub);
	if (!stub) {
		mcc_free(st);
		return -1;
	}
	mcc_jit_publish(slot, stub);
	return 0;
}

static int mccjit_lazy_enabled(void) {
	const char *e = getenv("MCC_JIT_LAZY");
	return e && e[0] && e[0] != '0';
}

static int mccjit_probe_exec_mem(void) {
	void *p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
								 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		return 0;
#if defined(__x86_64__)
	{
		static const unsigned char code[] = {0xb8, 0x5a, 0x5a, 0x00, 0x00, 0xc3};
		int got;
		memcpy(p, code, sizeof code);
		got = ((int (*)(void))p)();
		munmap(p, 4096);
		return got == 0x5a5a;
	}
#else
	munmap(p, 4096);
	return 1;
#endif
}

static int mccjit_feasible_flag;
static pthread_once_t mccjit_feasible_once = PTHREAD_ONCE_INIT;

static void mccjit_feasible_probe(void) {
	mccjit_feasible_flag = mccjit_probe_exec_mem();
	if (!mccjit_feasible_flag && getenv("MCC_JIT_VERBOSE"))
		fprintf(stderr,
						"mccjit: executable-memory probe failed — JIT disabled, running "
						"AOT baseline\n");
}

static int mccjit_feasible(void) {
	if (getenv("MCC_JIT_FORCE_INFEASIBLE"))
		return 0;
	pthread_once(&mccjit_feasible_once, mccjit_feasible_probe);
	return mccjit_feasible_flag;
}

void mccjit_boot_swap(void **slot, const void *blob, unsigned long len) {
	if (!mccjit_feasible())
		return;
	if (mccjit_lazy_enabled() && mccjit_lazy_install(slot, blob, len) == 0)
		return;
	mccjit_boot_swap_run(slot, blob, len, 0, "sync", NULL, 0);
}

void mccjit_boot_swap_async(void **slot, const void *blob, unsigned long len,
														unsigned long max_duration, unsigned long workers) {
	MccjitSwapJob *job;
	int nw;
	if (!mccjit_feasible())
		return;
	nw = mccjit_pool_start(workers);
	if (mccjit_lazy_enabled() && mccjit_lazy_install(slot, blob, len) == 0)
		return;
	job = (nw > 0) ? mcc_malloc(sizeof *job) : NULL;
	if (job) {
		job->run = mccjit_job_run_eager;
		job->slot = slot;
		job->blob = blob;
		job->len = len;
		job->max_duration = max_duration;
		job->timed =
				(max_duration != 0) && (clock_gettime(CLOCK_MONOTONIC, &job->start) == 0);
		mccjit_pool_enqueue(job);
		return;
	}
	{
		struct timespec t0;
		int timed =
				(max_duration != 0) && (clock_gettime(CLOCK_MONOTONIC, &t0) == 0);
		mccjit_boot_swap_run(slot, blob, len, max_duration, "sync-fallback", &t0,
												 timed);
	}
}

int mccjit_embed_have_fns(void) {
	return mccjit_embed_fns != NULL;
}

void mccjit_embed_finalize(MCCState *s1) {
	MccjitEmbedFn *e, *nx;
	CString cs;
	int n = 0;
	int async = 0;
	if (!s1 || !s1->embed_jit || !mccjit_embed_fns)
		return;
	async = s1->jit_threads > 0;
	if (s1->output_type == MCC_OUTPUT_MEMORY) {
		mcc_add_symbol(s1, "mccjit_boot_swap", (void *)mccjit_boot_swap);
		mcc_add_symbol(s1, "mccjit_boot_swap_async",
									 (void *)mccjit_boot_swap_async);
	}
	cstr_new(&cs);
	if (async)
		cstr_printf(&cs,
								"extern void mccjit_boot_swap_async(void**, const void*, unsigned long, unsigned long, unsigned long);\n");
	else
		cstr_printf(&cs,
								"extern void mccjit_boot_swap(void**, const void*, unsigned long);\n");
	for (e = mccjit_embed_fns; e; e = e->next) {
		int off = data_section->data_offset;
		unsigned char *p = section_ptr_add(data_section, e->len ? e->len : 1);
		char blobname[256];
		if (e->len)
			memcpy(p, e->blob, e->len);
		snprintf(blobname, sizeof blobname, "__mccjit_blob_%s", e->name);
		set_global_sym(s1, blobname, data_section, off);
		cstr_printf(&cs, "extern unsigned char __mccjit_blob_%s[];\n", e->name);
		cstr_printf(&cs, "extern void *__mccjit_slot_%s;\n", e->name);
		n++;
	}
	cstr_printf(&cs,
							"static struct __mccjit_reg { void **slot; const unsigned char *blob; "
							"unsigned long len; } __mccjit_registry[] = {\n");
	for (e = mccjit_embed_fns; e; e = e->next)
		cstr_printf(&cs, "{&__mccjit_slot_%s, __mccjit_blob_%s, %luUL},\n", e->name,
								e->name, (unsigned long)e->len);
	cstr_printf(&cs, "};\n");
	cstr_printf(
			&cs,
			"__attribute__((constructor)) static void __mccjit_boot_all(void){\nint __i;\nfor(__i=0;__i<%d;__i++)\n",
			n);
	if (async)
		cstr_printf(&cs,
								"mccjit_boot_swap_async(__mccjit_registry[__i].slot, __mccjit_registry[__i].blob, __mccjit_registry[__i].len, %luUL, %luUL);\n}\n",
								(unsigned long)s1->jit_max_duration,
								(unsigned long)s1->jit_threads);
	else
		cstr_printf(&cs,
								"mccjit_boot_swap(__mccjit_registry[__i].slot, __mccjit_registry[__i].blob, __mccjit_registry[__i].len);\n}\n");
	mcc_compile_string(s1, cs.data);
	cstr_free(&cs);
	for (e = mccjit_embed_fns; e; e = nx) {
		nx = e->next;
		mcc_free(e->name);
		mcc_free(e->blob);
		mcc_free(e);
	}
	mccjit_embed_fns = NULL;
}

int mccjit_selftest(void) {
	static const char src[] = "int f(int x){return x*2+1;}";
	MCCState *s1;
	int (*aotf)(int) = NULL;
	int (*jitf)(int) = NULL;
	int inputs[4] = {5, 0, -3, 100};
	int i, fails = 0;
	void *slot = NULL, *v1 = NULL, *v2 = NULL, *vspec = NULL;
	MCCState *v1state = NULL, *v2state = NULL, *vspecstate = NULL;

	mcc_free(mccjit_last_blob);
	mccjit_last_blob = NULL;
	mccjit_last_len = 0;
	mccjit_last_state = NULL;

	s1 = mcc_new();
	if (!s1) {
		printf("mccjit-selftest: mcc_new failed\n");
		return 1;
	}
	s1->optimize = 1;
	s1->nostdlib = 1;
	mcc_free(s1->jit_functions);
	s1->jit_functions = mcc_strdup("f");
	mcc_set_output_type(s1, MCC_OUTPUT_MEMORY);

	if (mcc_compile_string(s1, src) != 0) {
		printf("mccjit-selftest: state1 compile failed\n");
		mcc_delete(s1);
		return 1;
	}
	if (!mccjit_last_blob) {
		printf("mccjit-selftest: no intent blob stashed for 'f' (not faithful?)\n");
		mcc_delete(s1);
		return 1;
	}
	printf("mccjit-selftest: stashed leaf-int intent = %lu bytes\n",
				 (unsigned long)mccjit_last_len);

	if (mcc_relocate(s1) == 0)
		aotf = (int (*)(int))mcc_get_symbol(s1, "f");

	jitf = (int (*)(int))mcc_jit_recompile_blob(mccjit_last_blob, mccjit_last_len);
	if (!jitf) {
		printf("mccjit-selftest: cross-session recompile returned NULL\n");
		mcc_delete(s1);
		return 1;
	}
	v1state = mccjit_last_state;

	for (i = 0; i < 4; i++) {
		int x = inputs[i];
		int got = jitf(x);
		int want = x * 2 + 1;
		int aot = aotf ? aotf(x) : want;
		int ok = (got == want) && (got == aot);
		printf("mccjit-selftest: f(%d) jit=%d expect=%d aot=%d %s\n", x, got, want,
					 aot, ok ? "OK" : "FAIL");
		if (!ok)
			fails++;
	}

	v1 = (void *)jitf;
	slot = v1;
	printf("mccjit-selftest: hotswap slot init -> v1=%p\n", slot);
	for (i = 0; i < 4; i++) {
		int x = inputs[i];
		int got = ((int (*)(int))slot)(x);
		int want = x * 2 + 1;
		int ok = (got == want);
		printf("mccjit-selftest: slot(v1) f(%d)=%d expect=%d %s\n", x, got, want,
					 ok ? "OK" : "FAIL");
		if (!ok)
			fails++;
	}

	v2 = mcc_jit_recompile_blob(mccjit_last_blob, mccjit_last_len);
	if (!v2) {
		printf("mccjit-selftest: v2 recompile returned NULL\n");
		if (v1state)
			mcc_delete(v1state);
		mcc_delete(s1);
		return 1;
	}
	v2state = mccjit_last_state;
	mcc_jit_publish(&slot, v2);
	printf("mccjit-selftest: published v2=%p into slot (was v1=%p)\n", v2, v1);
	if (slot != v2) {
		printf("mccjit-selftest: slot did not observe v2 after publish\n");
		fails++;
	}
	for (i = 0; i < 4; i++) {
		int x = inputs[i];
		int gv1 = ((int (*)(int))v1)(x);
		int gv2 = ((int (*)(int))slot)(x);
		int want = x * 2 + 1;
		int ok = (gv2 == want) && (gv1 == gv2);
		printf("mccjit-selftest: swap f(%d) v1=%d v2=%d expect=%d %s\n", x, gv1, gv2,
					 want, ok ? "OK" : "FAIL");
		if (!ok)
			fails++;
	}

	vspec = mcc_jit_recompile_blob_spec(mccjit_last_blob, mccjit_last_len, 0, 7);
	if (!vspec) {
		printf("mccjit-selftest: specialized recompile returned NULL\n");
		fails++;
	} else {
		int sval = ((int (*)(int))vspec)(7);
		int sfold = ((int (*)(int))vspec)(5);
		int ok = (sval == 15) && (sfold == 15);
		vspecstate = mccjit_last_state;
		printf("mccjit-selftest: spec[x==7] f(7)=%d expect=15; f(5)=%d (folded=%s) %s\n",
					 sval, sfold, sfold == 15 ? "const" : "live", ok ? "OK" : "FAIL");
		if (!ok)
			fails++;
	}

	if (vspecstate)
		mcc_delete(vspecstate);
	if (v2state)
		mcc_delete(v2state);
	if (v1state)
		mcc_delete(v1state);
	mccjit_last_state = NULL;
	mcc_delete(s1);
	printf("mccjit-selftest: %s (%d failure%s)\n", fails ? "FAIL" : "PASS", fails,
				 fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static unsigned char *mccjit_stash_one(const char *src, const char *fn,
																			 int nostdlib, size_t *out_len,
																			 MCCState **out_state) {
	MCCState *s1;
	unsigned char *blob = NULL;
	*out_len = 0;
	*out_state = NULL;
	mcc_free(mccjit_last_blob);
	mccjit_last_blob = NULL;
	mccjit_last_len = 0;
	s1 = mcc_new();
	if (!s1)
		return NULL;
	s1->optimize = 1;
	s1->nostdlib = nostdlib;
	mcc_free(s1->jit_functions);
	s1->jit_functions = mcc_strdup(fn);
	mcc_set_output_type(s1, MCC_OUTPUT_MEMORY);
	if (mcc_compile_string(s1, src) != 0) {
		mcc_delete(s1);
		return NULL;
	}
	if (mccjit_last_blob) {
		blob = mcc_malloc(mccjit_last_len ? mccjit_last_len : 1);
		if (blob) {
			memcpy(blob, mccjit_last_blob, mccjit_last_len);
			*out_len = mccjit_last_len;
		}
	}
	*out_state = s1;
	return blob;
}

int mccjit_selftest_stage2(void) {
	static const char src_g[] =
			"int g(int *p, int x){ return p ? *p + x : -1; }";
	static const char src_h[] =
			"int abs(int); int h(int x){ return abs(x) + 1; }";
	int fails = 0;
	unsigned char *blob;
	size_t blen;
	MCCState *s1;

	printf("mccjit-selftest-stage2: begin\n");

	blob = mccjit_stash_one(src_g, "g", 1, &blen, &s1);
	if (!s1) {
		printf("mccjit-selftest-stage2: g compile setup failed\n");
		return 1;
	}
	if (!blob) {
		printf("mccjit-selftest-stage2: no intent blob stashed for 'g'\n");
		mcc_delete(s1);
		fails++;
	} else {
		int (*aotg)(int *, int) = NULL;
		int (*jitg)(int *, int);
		MCCState *js;
		int cells[3] = {41, -8, 0};
		int i;
		printf("mccjit-selftest-stage2: stashed pointer-param intent = %lu bytes\n",
					 (unsigned long)blen);
		if (mcc_relocate(s1) == 0)
			aotg = (int (*)(int *, int))mcc_get_symbol(s1, "g");
		jitg = (int (*)(int *, int))mcc_jit_recompile_blob(blob, blen);
		if (!jitg) {
			printf("mccjit-selftest-stage2: g recompile returned NULL\n");
			fails++;
		} else {
			js = mccjit_last_state;
			for (i = 0; i < 3; i++) {
				int x = cells[i] + i * 7;
				int got = jitg(&cells[i], x);
				int want = cells[i] + x;
				int aot = aotg ? aotg(&cells[i], x) : want;
				int ok = (got == want) && (got == aot);
				printf("mccjit-selftest-stage2: g(&%d,%d) jit=%d expect=%d aot=%d %s\n",
							 cells[i], x, got, want, aot, ok ? "OK" : "FAIL");
				if (!ok)
					fails++;
			}
			{
				int got = jitg((int *)0, 99);
				int aot = aotg ? aotg((int *)0, 99) : -1;
				int ok = (got == -1) && (got == aot);
				printf("mccjit-selftest-stage2: g(NULL,99) jit=%d expect=-1 aot=%d %s\n",
							 got, aot, ok ? "OK" : "FAIL");
				if (!ok)
					fails++;
			}
			if (js)
				mcc_delete(js);
			mccjit_last_state = NULL;
		}
		mcc_free(blob);
		mcc_delete(s1);
	}

	blob = mccjit_stash_one(src_h, "h", 0, &blen, &s1);
	if (!s1) {
		printf("mccjit-selftest-stage2: h compile setup failed\n");
		return fails + 1;
	}
	if (!blob) {
		printf("mccjit-selftest-stage2: no intent blob stashed for 'h'\n");
		mcc_delete(s1);
		fails++;
	} else {
		int (*aoth)(int) = NULL;
		int (*jith)(int);
		MCCState *js;
		int inputs[4] = {5, -12, 0, 40};
		int i;
		printf("mccjit-selftest-stage2: stashed call-bearing intent = %lu bytes\n",
					 (unsigned long)blen);
		if (mcc_relocate(s1) == 0)
			aoth = (int (*)(int))mcc_get_symbol(s1, "h");
		jith = (int (*)(int))mcc_jit_recompile_blob(blob, blen);
		if (!jith) {
			printf("mccjit-selftest-stage2: h recompile returned NULL (callee unbound?)\n");
			fails++;
		} else {
			js = mccjit_last_state;
			for (i = 0; i < 4; i++) {
				int x = inputs[i];
				int got = jith(x);
				int want = (x < 0 ? -x : x) + 1;
				int aot = aoth ? aoth(x) : want;
				int ok = (got == want) && (got == aot);
				printf("mccjit-selftest-stage2: h(%d) jit=%d expect=%d aot=%d %s\n", x,
							 got, want, aot, ok ? "OK" : "FAIL");
				if (!ok)
					fails++;
			}
			if (js)
				mcc_delete(js);
			mccjit_last_state = NULL;
		}
		mcc_free(blob);
		mcc_delete(s1);
	}

	printf("mccjit-selftest-stage2: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

struct MccjitTestS {
	int a;
	int b;
};

struct MccjitTestT {
	int *p;
	int k;
};

int mccjit_selftest_struct(void) {
	static const char src_f[] =
			"struct S{int a; int b;}; int f(struct S*s){return s->a*3 + s->b;}";
	static const char src_i[] =
			"struct S{int a; int b;}; int fi(struct S*s){return s[1].a*3 + s[0].b;}";
	static const char src_g[] =
			"struct T{int *p; int k;}; int g(struct T*t){return *t->p + t->k;}";
	int fails = 0;
	unsigned char *blob;
	size_t blen;
	MCCState *s1;

	printf("mccjit-selftest-struct: begin\n");

	blob = mccjit_stash_one(src_f, "f", 1, &blen, &s1);
	if (!s1) {
		printf("mccjit-selftest-struct: f compile setup failed\n");
		return 1;
	}
	if (!blob) {
		printf("mccjit-selftest-struct: no intent blob stashed for 'f'\n");
		mcc_delete(s1);
		fails++;
	} else {
		int (*aotf)(struct MccjitTestS *) = NULL;
		int (*jitf)(struct MccjitTestS *);
		MCCState *js;
		struct MccjitTestS cells[3] = {{7, 5}, {-2, 9}, {0, -11}};
		int i;
		printf("mccjit-selftest-struct: stashed struct-param intent = %lu bytes\n",
					 (unsigned long)blen);
		if (mcc_relocate(s1) == 0)
			aotf = (int (*)(struct MccjitTestS *))mcc_get_symbol(s1, "f");
		jitf = (int (*)(struct MccjitTestS *))mcc_jit_recompile_blob(blob, blen);
		if (!jitf) {
			printf("mccjit-selftest-struct: f recompile returned NULL\n");
			fails++;
		} else {
			js = mccjit_last_state;
			for (i = 0; i < 3; i++) {
				int got = jitf(&cells[i]);
				int want = cells[i].a * 3 + cells[i].b;
				int aot = aotf ? aotf(&cells[i]) : want;
				int ok = (got == want) && (got == aot);
				printf("mccjit-selftest-struct: f({%d,%d}) jit=%d expect=%d aot=%d %s\n",
							 cells[i].a, cells[i].b, got, want, aot, ok ? "OK" : "FAIL");
				if (!ok)
					fails++;
			}
			if (js)
				mcc_delete(js);
			mccjit_last_state = NULL;
		}
		mcc_free(blob);
		mcc_delete(s1);
	}

	blob = mccjit_stash_one(src_i, "fi", 1, &blen, &s1);
	if (!s1) {
		printf("mccjit-selftest-struct: fi compile setup failed\n");
		return fails + 1;
	}
	if (!blob) {
		printf("mccjit-selftest-struct: no intent blob stashed for 'fi'\n");
		mcc_delete(s1);
		fails++;
	} else {
		int (*aotfi)(struct MccjitTestS *) = NULL;
		int (*jitfi)(struct MccjitTestS *);
		MCCState *js;
		struct MccjitTestS cells[6] = {{7, 5}, {-2, 9}, {0, -11},
																	 {3, 4},  {8, -1}, {6, 2}};
		int i;
		printf("mccjit-selftest-struct: stashed struct-index intent = %lu bytes\n",
					 (unsigned long)blen);
		if (mcc_relocate(s1) == 0)
			aotfi = (int (*)(struct MccjitTestS *))mcc_get_symbol(s1, "fi");
		jitfi = (int (*)(struct MccjitTestS *))mcc_jit_recompile_blob(blob, blen);
		if (!jitfi) {
			printf("mccjit-selftest-struct: fi recompile returned NULL\n");
			fails++;
		} else {
			js = mccjit_last_state;
			for (i = 0; i + 1 < 6; i++) {
				int got = jitfi(&cells[i]);
				int want = cells[i + 1].a * 3 + cells[i].b;
				int aot = aotfi ? aotfi(&cells[i]) : want;
				int ok = (got == want) && (got == aot);
				printf("mccjit-selftest-struct: fi(&cells[%d]) jit=%d expect=%d aot=%d %s\n",
							 i, got, want, aot, ok ? "OK" : "FAIL");
				if (!ok)
					fails++;
			}
			if (js)
				mcc_delete(js);
			mccjit_last_state = NULL;
		}
		mcc_free(blob);
		mcc_delete(s1);
	}

	blob = mccjit_stash_one(src_g, "g", 1, &blen, &s1);
	if (!s1) {
		printf("mccjit-selftest-struct: g compile setup failed\n");
		return fails + 1;
	}
	if (!blob) {
		printf("mccjit-selftest-struct: no intent blob stashed for 'g'\n");
		mcc_delete(s1);
		fails++;
	} else {
		int (*aotg)(struct MccjitTestT *) = NULL;
		int (*jitg)(struct MccjitTestT *);
		MCCState *js;
		int slots[3] = {10, -4, 100};
		struct MccjitTestT recs[3];
		int i;
		for (i = 0; i < 3; i++) {
			recs[i].p = &slots[i];
			recs[i].k = i * 3 - 1;
		}
		printf("mccjit-selftest-struct: stashed pointer-field intent = %lu bytes\n",
					 (unsigned long)blen);
		if (mcc_relocate(s1) == 0)
			aotg = (int (*)(struct MccjitTestT *))mcc_get_symbol(s1, "g");
		jitg = (int (*)(struct MccjitTestT *))mcc_jit_recompile_blob(blob, blen);
		if (!jitg) {
			printf("mccjit-selftest-struct: g recompile returned NULL\n");
			fails++;
		} else {
			js = mccjit_last_state;
			for (i = 0; i < 3; i++) {
				int got = jitg(&recs[i]);
				int want = slots[i] + recs[i].k;
				int aot = aotg ? aotg(&recs[i]) : want;
				int ok = (got == want) && (got == aot);
				printf("mccjit-selftest-struct: g({*%d,%d}) jit=%d expect=%d aot=%d %s\n",
							 slots[i], recs[i].k, got, want, aot, ok ? "OK" : "FAIL");
				if (!ok)
					fails++;
			}
			if (js)
				mcc_delete(js);
			mccjit_last_state = NULL;
		}
		mcc_free(blob);
		mcc_delete(s1);
	}

	printf("mccjit-selftest-struct: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

#define MCCJIT_KGC_ARITY 6
#define MCCJIT_KGC_MAGIC 0x43474b4dul

typedef struct MccjitKgcHdr {
	uint32_t magic;
	uint32_t arity;
	uint64_t salt;
	uint64_t count;
	uint64_t cap;
} MccjitKgcHdr;

typedef struct MccjitKgc {
	int fd;
	int anon;
	char *path;
	void *map;
	size_t map_len;
	MccjitKgcHdr *hdr;
	int64_t *tuples;
	uint32_t arity;
	int memoize_ok;
	int ret_wide;
	uint64_t hits;
	uint64_t misses;
	int poisoned;
	pthread_mutex_t lock;
} MccjitKgc;

static int mccjit_poison_min(void) {
	const char *e = getenv("MCC_JIT_POISON_MIN");
	if (e && e[0]) {
		long v = strtol(e, NULL, 10);
		if (v > 0)
			return (int)v;
	}
	return 8;
}

static int mccjit_poison_pct(void) {
	const char *e = getenv("MCC_JIT_POISON_PCT");
	if (e && e[0]) {
		long v = strtol(e, NULL, 10);
		if (v > 0 && v <= 100)
			return (int)v;
	}
	return 50;
}

#define MCCJIT_KGC_MAX ((uint64_t)1 << 16)

static size_t mccjit_kgc_bytes(uint64_t cap, uint32_t arity) {
	return sizeof(MccjitKgcHdr) + (size_t)cap * arity * sizeof(int64_t);
}

static void mccjit_kgc_bind(MccjitKgc *k) {
	k->hdr = (MccjitKgcHdr *)k->map;
	k->tuples = (int64_t *)((char *)k->map + sizeof(MccjitKgcHdr));
}

static int mccjit_kgc_map_shared(MccjitKgc *k, size_t bytes) {
	void *m = mmap(0, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, k->fd, 0);
	if (m == MAP_FAILED)
		return -1;
	k->map = m;
	k->map_len = bytes;
	mccjit_kgc_bind(k);
	return 0;
}

static void mccjit_kgc_init_hdr(MccjitKgc *k, uint64_t salt, uint64_t cap) {
	k->hdr->magic = MCCJIT_KGC_MAGIC;
	k->hdr->arity = k->arity;
	k->hdr->salt = salt;
	k->hdr->count = 0;
	k->hdr->cap = cap;
}

static int mccjit_kgc_open(MccjitKgc *k, const char *path, uint64_t salt,
													 uint32_t arity) {
	uint64_t initcap = 64;
	size_t initbytes;
	memset(k, 0, sizeof *k);
	k->fd = -1;
	k->memoize_ok = 1;
	pthread_mutex_init(&k->lock, NULL);
	if (arity == 0 || arity > MCCJIT_KGC_ARITY)
		return -1;
	k->arity = arity;
	initbytes = mccjit_kgc_bytes(initcap, arity);
	if (!path) {
		void *m = mmap(0, initbytes, PROT_READ | PROT_WRITE,
									 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (m == MAP_FAILED)
			return -1;
		k->anon = 1;
		k->map = m;
		k->map_len = initbytes;
		mccjit_kgc_bind(k);
		mccjit_kgc_init_hdr(k, salt, initcap);
		return 0;
	}
	k->path = mcc_strdup(path);
	k->fd = open(path, O_RDWR | O_CREAT, 0644);
	if (k->fd < 0)
		return -1;
	{
		struct stat st;
		MccjitKgcHdr peek;
		int valid = 0;
		if (fstat(k->fd, &st) == 0 && (size_t)st.st_size >= sizeof(MccjitKgcHdr) &&
				pread(k->fd, &peek, sizeof peek, 0) == (ssize_t)sizeof peek &&
				peek.magic == MCCJIT_KGC_MAGIC && peek.salt == salt &&
				peek.arity == arity && peek.cap >= 1 && peek.count <= peek.cap &&
				(size_t)st.st_size >= mccjit_kgc_bytes(peek.cap, arity)) {
			if (mccjit_kgc_map_shared(k, mccjit_kgc_bytes(peek.cap, arity)) == 0)
				valid = 1;
		}
		if (!valid) {
			if (ftruncate(k->fd, (off_t)initbytes) != 0 ||
					mccjit_kgc_map_shared(k, initbytes) != 0) {
				close(k->fd);
				k->fd = -1;
				return -1;
			}
			mccjit_kgc_init_hdr(k, salt, initcap);
			msync(k->map, k->map_len, MS_SYNC);
		}
	}
	return 0;
}

static void mccjit_kgc_close(MccjitKgc *k) {
	if (!k)
		return;
	if (k->map && k->map != MAP_FAILED) {
		if (!k->anon)
			msync(k->map, k->map_len, MS_SYNC);
		munmap(k->map, k->map_len);
	}
	if (k->fd >= 0)
		close(k->fd);
	mcc_free(k->path);
	memset(k, 0, sizeof *k);
	k->fd = -1;
}

static int mccjit_kgc_cmp(const int64_t *a, const int64_t *b, uint32_t arity) {
	uint32_t i;
	for (i = 0; i < arity; i++) {
		if (a[i] < b[i])
			return -1;
		if (a[i] > b[i])
			return 1;
	}
	return 0;
}

static uint64_t mccjit_kgc_lower(const MccjitKgc *k, const int64_t *tuple,
																 int *found) {
	uint64_t lo = 0, hi = k->hdr->count;
	*found = 0;
	while (lo < hi) {
		uint64_t mid = lo + (hi - lo) / 2;
		int c = mccjit_kgc_cmp(k->tuples + mid * k->arity, tuple, k->arity);
		if (c < 0) {
			lo = mid + 1;
		} else if (c > 0) {
			hi = mid;
		} else {
			*found = 1;
			return mid;
		}
	}
	return lo;
}

static int mccjit_kgc_contains(const MccjitKgc *k, const int64_t *tuple) {
	int found;
	mccjit_kgc_lower(k, tuple, &found);
	return found;
}

static int mccjit_kgc_grow(MccjitKgc *k, uint64_t need) {
	uint64_t ncap = k->hdr->cap ? k->hdr->cap : 1;
	size_t nbytes;
	while (ncap < need)
		ncap *= 2;
	nbytes = mccjit_kgc_bytes(ncap, k->arity);
	if (k->anon) {
		void *nm = mmap(0, nbytes, PROT_READ | PROT_WRITE,
										MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (nm == MAP_FAILED)
			return -1;
		memcpy(nm, k->map, k->map_len);
		munmap(k->map, k->map_len);
		k->map = nm;
		k->map_len = nbytes;
		mccjit_kgc_bind(k);
		k->hdr->cap = ncap;
		return 0;
	}
	munmap(k->map, k->map_len);
	k->map = NULL;
	if (ftruncate(k->fd, (off_t)nbytes) != 0)
		return -1;
	if (mccjit_kgc_map_shared(k, nbytes) != 0)
		return -1;
	k->hdr->cap = ncap;
	return 0;
}

static int mccjit_kgc_insert(MccjitKgc *k, const int64_t *tuple) {
	int found;
	uint64_t at = mccjit_kgc_lower(k, tuple, &found);
	int64_t *dst;
	if (found)
		return 0;
	if (k->hdr->count >= MCCJIT_KGC_MAX)
		return 0;
	if (k->hdr->count + 1 > k->hdr->cap &&
			mccjit_kgc_grow(k, k->hdr->count + 1) != 0)
		return -1;
	dst = k->tuples + at * k->arity;
	if (at < k->hdr->count)
		memmove(dst + k->arity, dst,
						(size_t)(k->hdr->count - at) * k->arity * sizeof(int64_t));
	memcpy(dst, tuple, (size_t)k->arity * sizeof(int64_t));
	k->hdr->count++;
	if (!k->anon)
		msync(k->map, k->map_len, MS_SYNC);
	return 1;
}

static int64_t mccjit_kgc_call1(MccjitKgc *k, void *variant, void *baseline,
																int64_t x, int *flagged) {
	int (*vf)(int) = (int (*)(int))variant;
	int (*bf)(int) = (int (*)(int))baseline;
	int64_t tuple[MCCJIT_KGC_ARITY];
	int64_t bval, vval;
	uint32_t i;
	for (i = 0; i < MCCJIT_KGC_ARITY; i++)
		tuple[i] = 0;
	tuple[0] = x;
	pthread_mutex_lock(&k->lock);
	if (k->memoize_ok && mccjit_kgc_contains(k, tuple)) {
		pthread_mutex_unlock(&k->lock);
		return (int64_t)vf((int)x);
	}
	bval = (int64_t)bf((int)x);
	vval = (int64_t)vf((int)x);
	if (vval == bval) {
		if (k->memoize_ok)
			mccjit_kgc_insert(k, tuple);
		pthread_mutex_unlock(&k->lock);
		return bval;
	}
	pthread_mutex_unlock(&k->lock);
	if (flagged)
		*flagged = 1;
	return bval;
}

static int64_t mccjit_invoke(void *fn, const int64_t *a, uint32_t n, int wide) {
	switch (n) {
	case 1:
		return wide ? (int64_t)((long (*)(long))fn)(a[0])
								: (int64_t)((int (*)(long))fn)(a[0]);
	case 2:
		return wide ? (int64_t)((long (*)(long, long))fn)(a[0], a[1])
								: (int64_t)((int (*)(long, long))fn)(a[0], a[1]);
	case 3:
		return wide ? (int64_t)((long (*)(long, long, long))fn)(a[0], a[1], a[2])
								: (int64_t)((int (*)(long, long, long))fn)(a[0], a[1], a[2]);
	case 4:
		return wide
							 ? (int64_t)((long (*)(long, long, long, long))fn)(a[0], a[1], a[2],
																																 a[3])
							 : (int64_t)((int (*)(long, long, long, long))fn)(a[0], a[1], a[2],
																															 a[3]);
	case 5:
		return wide ? (int64_t)((long (*)(long, long, long, long, long))fn)(
											a[0], a[1], a[2], a[3], a[4])
								: (int64_t)((int (*)(long, long, long, long, long))fn)(
											a[0], a[1], a[2], a[3], a[4]);
	case 6:
		return wide ? (int64_t)((long (*)(long, long, long, long, long, long))fn)(
											a[0], a[1], a[2], a[3], a[4], a[5])
								: (int64_t)((int (*)(long, long, long, long, long, long))fn)(
											a[0], a[1], a[2], a[3], a[4], a[5]);
	default:
		return 0;
	}
}

/* K4A: scalar all-double marshalling (SSE class). Args come in via a spilled
   xmm0-7 double vector; the return is a double (xmm0). Fixed C signatures let
   the compiler place args in xmm and read the xmm0 return, so no hand-emitted
   argument classifier is needed for the all-FP case. */
static double mccjit_invoke_fp(void *fn, const double *a, uint32_t n) {
	switch (n) {
	case 1:
		return ((double (*)(double))fn)(a[0]);
	case 2:
		return ((double (*)(double, double))fn)(a[0], a[1]);
	case 3:
		return ((double (*)(double, double, double))fn)(a[0], a[1], a[2]);
	case 4:
		return ((double (*)(double, double, double, double))fn)(a[0], a[1], a[2],
																														a[3]);
	case 5:
		return ((double (*)(double, double, double, double, double))fn)(
				a[0], a[1], a[2], a[3], a[4]);
	case 6:
		return ((double (*)(double, double, double, double, double, double))fn)(
				a[0], a[1], a[2], a[3], a[4], a[5]);
	default:
		return 0.0;
	}
}

static volatile int64_t mccjit_bench_sink;

static long mccjit_bench_iters(void) {
	const char *e = getenv("MCC_JIT_BENCH_ITERS");
	if (e && e[0]) {
		long v = strtol(e, NULL, 10);
		if (v > 0)
			return v;
	}
	return 100000;
}

static int mccjit_bench_margin_pct(void) {
	const char *e = getenv("MCC_JIT_BENCH_MARGIN_PCT");
	if (e && e[0]) {
		long v = strtol(e, NULL, 10);
		if (v >= 0 && v <= 100)
			return (int)v;
	}
	return 6;
}

static double mccjit_bench_run(void *fn, const int64_t *tuples, uint32_t ntuples,
															uint32_t nargs, int wide, uint32_t reps) {
	struct timespec t0;
	int64_t sink = 0;
	uint32_t r, i;
	if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0)
		return 1e300;
	for (r = 0; r < reps; r++)
		for (i = 0; i < ntuples; i++)
			sink += mccjit_invoke(fn, tuples + (size_t)i * MCCJIT_KGC_ARITY, nargs, wide);
	mccjit_bench_sink ^= sink;
	return mccjit_elapsed(&t0);
}

/* K5/L4A/L5A promotion scorer: best-of-3 wall-clock benchmark of a candidate
   vs the incumbent over a caller-supplied live-in set (the real observed
   distribution, provided by J6A once it lands). Returns 1 only if the
   candidate is faster by more than a hysteresis margin — the incumbent wins
   ties (L5A). Work is bounded by a deterministic inner-iteration cap, not
   wall-clock, so the decision is reproducible. */
static int mccjit_bench_pair(void *cand, void *incumbent, const int64_t *tuples,
														 uint32_t ntuples, uint32_t nargs, int wide) {
	double cb = 1e300, ib = 1e300;
	uint32_t k, reps;
	long iters;
	int margin;
	if (!cand || !incumbent || !tuples || ntuples == 0 || nargs == 0)
		return 1;
	iters = mccjit_bench_iters();
	margin = mccjit_bench_margin_pct();
	reps = (uint32_t)(iters / (long)ntuples);
	if (reps < 1)
		reps = 1;
	for (k = 0; k < 3; k++) {
		double c = mccjit_bench_run(cand, tuples, ntuples, nargs, wide, reps);
		double i2 = mccjit_bench_run(incumbent, tuples, ntuples, nargs, wide, reps);
		if (c < cb)
			cb = c;
		if (i2 < ib)
			ib = i2;
	}
	return cb * (100.0 + (double)margin) < ib * 100.0;
}

static int64_t mccjit_kgc_calln(MccjitKgc *k, void *variant, void *baseline,
																const int64_t *argv, uint32_t nargs,
																int *flagged) {
	int wide = k->ret_wide;
	int64_t tuple[MCCJIT_KGC_ARITY];
	int64_t bval, vval;
	uint32_t i;
	for (i = 0; i < MCCJIT_KGC_ARITY; i++)
		tuple[i] = 0;
	for (i = 0; i < nargs && i < MCCJIT_KGC_ARITY; i++)
		tuple[i] = argv[i];
	pthread_mutex_lock(&k->lock);
	if (k->poisoned) {
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke(baseline, argv, nargs, wide);
	}
	if (k->memoize_ok && mccjit_kgc_contains(k, tuple)) {
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke(variant, argv, nargs, wide);
	}
	bval = mccjit_invoke(baseline, argv, nargs, wide);
	vval = mccjit_invoke(variant, argv, nargs, wide);
	if (vval == bval) {
		k->hits++;
		if (k->memoize_ok)
			mccjit_kgc_insert(k, tuple);
		pthread_mutex_unlock(&k->lock);
		return bval;
	}
	k->misses++;
	{
		uint64_t total = k->hits + k->misses;
		if (total >= (uint64_t)mccjit_poison_min() &&
				k->misses * 100 >= total * (uint64_t)mccjit_poison_pct())
			k->poisoned = 1;
	}
	pthread_mutex_unlock(&k->lock);
	if (flagged)
		*flagged = 1;
	return bval;
}

/* All-double differential verify. Args/return are doubles; the memo key and
   the mismatch check use the raw bit pattern (a faithful recompile is
   bit-identical, and treating +0/-0 or a NaN-bit difference as a mismatch is
   the conservative-correct choice — it just returns the baseline). */
static double mccjit_kgc_calln_fp(MccjitKgc *k, void *variant, void *baseline,
																	const double *argv, uint32_t nargs,
																	int *flagged) {
	int64_t tuple[MCCJIT_KGC_ARITY];
	double bval, vval;
	uint64_t bbits = 0, vbits = 0;
	uint32_t i;
	for (i = 0; i < MCCJIT_KGC_ARITY; i++)
		tuple[i] = 0;
	for (i = 0; i < nargs && i < MCCJIT_KGC_ARITY; i++)
		memcpy(&tuple[i], &argv[i], sizeof tuple[i]);
	pthread_mutex_lock(&k->lock);
	if (k->poisoned) {
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke_fp(baseline, argv, nargs);
	}
	if (k->memoize_ok && mccjit_kgc_contains(k, tuple)) {
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke_fp(variant, argv, nargs);
	}
	bval = mccjit_invoke_fp(baseline, argv, nargs);
	vval = mccjit_invoke_fp(variant, argv, nargs);
	memcpy(&bbits, &bval, sizeof bbits);
	memcpy(&vbits, &vval, sizeof vbits);
	if (vbits == bbits) {
		k->hits++;
		if (k->memoize_ok)
			mccjit_kgc_insert(k, tuple);
		pthread_mutex_unlock(&k->lock);
		return bval;
	}
	k->misses++;
	{
		uint64_t total = k->hits + k->misses;
		if (total >= (uint64_t)mccjit_poison_min() &&
				k->misses * 100 >= total * (uint64_t)mccjit_poison_pct())
			k->poisoned = 1;
	}
	pthread_mutex_unlock(&k->lock);
	if (flagged)
		*flagged = 1;
	return bval;
}

#if defined(__x86_64__)
static void *mccjit_make_kgc_stub_fp(void *variant, void *baseline,
																		 int memoize_ok, uint32_t nargs) {
	unsigned char *p;
	MccjitKgc *kgc;
	int *flag;
	void *calln = (void *)mccjit_kgc_calln_fp;
	void *fp;
	size_t o = 0;
	uint32_t i;
	if (nargs < 1 || nargs > MCCJIT_KGC_MAXARG)
		return NULL;
	kgc = mcc_mallocz(sizeof *kgc);
	if (!kgc)
		return NULL;
	if (mccjit_kgc_open(kgc, NULL, mccjit_salt_witness(), nargs) != 0) {
		mcc_free(kgc);
		return NULL;
	}
	kgc->memoize_ok = memoize_ok;
	kgc->ret_wide = 1;
	p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
					 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) {
		mccjit_kgc_close(kgc);
		mcc_free(kgc);
		return NULL;
	}
	flag = (int *)(p + 256);
	*flag = 0;
	fp = flag;
	p[o++] = 0xc9;
	p[o++] = 0x55;
	p[o++] = 0x48;
	p[o++] = 0x83;
	p[o++] = 0xec;
	p[o++] = 0x40;
	for (i = 0; i < nargs; i++) {
		unsigned char disp = (unsigned char)(i * 8);
		p[o++] = 0xf2;
		p[o++] = 0x0f;
		p[o++] = 0x11;
		p[o++] = (unsigned char)(0x44 + i * 8);
		p[o++] = 0x24;
		p[o++] = disp;
	}
	p[o++] = 0x48;
	p[o++] = 0xbf;
	memcpy(p + o, &kgc, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0xbe;
	memcpy(p + o, &variant, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0xba;
	memcpy(p + o, &baseline, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0x89;
	p[o++] = 0xe1;
	p[o++] = 0x41;
	p[o++] = 0xb8;
	memcpy(p + o, &nargs, 4);
	o += 4;
	p[o++] = 0x49;
	p[o++] = 0xb9;
	memcpy(p + o, &fp, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0xb8;
	memcpy(p + o, &calln, 8);
	o += 8;
	p[o++] = 0xff;
	p[o++] = 0xd0;
	p[o++] = 0x48;
	p[o++] = 0x83;
	p[o++] = 0xc4;
	p[o++] = 0x40;
	p[o++] = 0x5d;
	p[o++] = 0xc3;
	return p;
}

static void *mccjit_make_kgc_stub_n(void *variant, void *baseline, int memoize_ok,
																		const uint32_t *param_t, uint32_t nargs,
																		int ret_wide) {
	static const unsigned char mov64_pre[MCCJIT_KGC_MAXARG][4] = {
			{0x48, 0x89, 0x7c, 0x24}, {0x48, 0x89, 0x74, 0x24},
			{0x48, 0x89, 0x54, 0x24}, {0x48, 0x89, 0x4c, 0x24},
			{0x4c, 0x89, 0x44, 0x24}, {0x4c, 0x89, 0x4c, 0x24}};
	static const unsigned char movsxd[MCCJIT_KGC_MAXARG][3] = {
			{0x48, 0x63, 0xc7}, {0x48, 0x63, 0xc6}, {0x48, 0x63, 0xc2},
			{0x48, 0x63, 0xc1}, {0x49, 0x63, 0xc0}, {0x49, 0x63, 0xc1}};
	unsigned char *p;
	MccjitKgc *kgc;
	int *flag;
	void *calln = (void *)mccjit_kgc_calln;
	void *fp;
	size_t o = 0;
	uint32_t i;
	if (nargs < 1 || nargs > MCCJIT_KGC_MAXARG)
		return NULL;
	kgc = mcc_mallocz(sizeof *kgc);
	if (!kgc)
		return NULL;
	if (mccjit_kgc_open(kgc, NULL, mccjit_salt_witness(), nargs) != 0) {
		mcc_free(kgc);
		return NULL;
	}
	kgc->memoize_ok = memoize_ok;
	kgc->ret_wide = ret_wide;
	p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
					 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) {
		mccjit_kgc_close(kgc);
		mcc_free(kgc);
		return NULL;
	}
	flag = (int *)(p + 256);
	*flag = 0;
	fp = flag;
	p[o++] = 0xc9;
	p[o++] = 0x55;
	p[o++] = 0x48;
	p[o++] = 0x83;
	p[o++] = 0xec;
	p[o++] = 0x40;
	for (i = 0; i < nargs; i++) {
		unsigned char disp = (unsigned char)(i * 8);
		if (mccjit_type_wide((int)param_t[i])) {
			memcpy(p + o, mov64_pre[i], 4);
			o += 4;
			p[o++] = disp;
		} else {
			memcpy(p + o, movsxd[i], 3);
			o += 3;
			p[o++] = 0x48;
			p[o++] = 0x89;
			p[o++] = 0x44;
			p[o++] = 0x24;
			p[o++] = disp;
		}
	}
	p[o++] = 0x48;
	p[o++] = 0xbf;
	memcpy(p + o, &kgc, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0xbe;
	memcpy(p + o, &variant, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0xba;
	memcpy(p + o, &baseline, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0x89;
	p[o++] = 0xe1;
	p[o++] = 0x41;
	p[o++] = 0xb8;
	memcpy(p + o, &nargs, 4);
	o += 4;
	p[o++] = 0x49;
	p[o++] = 0xb9;
	memcpy(p + o, &fp, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0xb8;
	memcpy(p + o, &calln, 8);
	o += 8;
	p[o++] = 0xff;
	p[o++] = 0xd0;
	p[o++] = 0x48;
	p[o++] = 0x83;
	p[o++] = 0xc4;
	p[o++] = 0x40;
	p[o++] = 0x5d;
	p[o++] = 0xc3;
	return p;
}
#else
static void *mccjit_make_kgc_stub_n(void *variant, void *baseline, int memoize_ok,
																		const uint32_t *param_t, uint32_t nargs,
																		int ret_wide) {
	(void)variant;
	(void)baseline;
	(void)memoize_ok;
	(void)param_t;
	(void)nargs;
	(void)ret_wide;
	return NULL;
}
static void *mccjit_make_kgc_stub_fp(void *variant, void *baseline,
																		 int memoize_ok, uint32_t nargs) {
	(void)variant;
	(void)baseline;
	(void)memoize_ok;
	(void)nargs;
	return NULL;
}
#endif

int mccjit_selftest_kgc(void) {
	static const char src[] = "int f(int x){return x*2+1;}";
	MCCState *s1;
	int (*baseline)(int) = NULL;
	void *variant = NULL;
	MCCState *vstate = NULL;
	MccjitKgc kgc;
	int fails = 0;
	int variant_flagged = 0;
	int64_t inputs[6] = {7, 7, 5, 0, 3, 100};
	int i;

	mcc_free(mccjit_last_blob);
	mccjit_last_blob = NULL;
	mccjit_last_len = 0;
	mccjit_last_state = NULL;

	printf("mccjit-selftest-kgc: begin (arity=%d salt=%016llx)\n", MCCJIT_KGC_ARITY,
				 (unsigned long long)mccjit_salt_witness());

	s1 = mcc_new();
	if (!s1) {
		printf("mccjit-selftest-kgc: mcc_new failed\n");
		return 1;
	}
	s1->optimize = 1;
	s1->nostdlib = 1;
	mcc_free(s1->jit_functions);
	s1->jit_functions = mcc_strdup("f");
	mcc_set_output_type(s1, MCC_OUTPUT_MEMORY);
	if (mcc_compile_string(s1, src) != 0 || !mccjit_last_blob) {
		printf("mccjit-selftest-kgc: compile/stash failed\n");
		mcc_delete(s1);
		return 1;
	}
	if (mcc_relocate(s1) == 0)
		baseline = (int (*)(int))mcc_get_symbol(s1, "f");
	if (!baseline) {
		printf("mccjit-selftest-kgc: no AOT baseline entry for f\n");
		mcc_delete(s1);
		return 1;
	}
	variant = mcc_jit_recompile_blob_spec(mccjit_last_blob, mccjit_last_len, 0, 7);
	if (!variant) {
		printf("mccjit-selftest-kgc: wrongly-specialized variant recompile NULL\n");
		mcc_delete(s1);
		return 1;
	}
	vstate = mccjit_last_state;
	printf("mccjit-selftest-kgc: baseline f=%p variant spec[x==7]=%p v(0)=%d v(7)=%d\n",
				 (void *)baseline, variant, ((int (*)(int))variant)(0),
				 ((int (*)(int))variant)(7));

	if (mccjit_kgc_open(&kgc, NULL, mccjit_salt_witness(), 1) != 0) {
		printf("mccjit-selftest-kgc: kgc open (anon) failed\n");
		if (vstate)
			mcc_delete(vstate);
		mcc_delete(s1);
		return 1;
	}

	printf("mccjit-selftest-kgc:    x  path  variant  baseline  returned  flagged  ok\n");
	for (i = 0; i < 6; i++) {
		int64_t x = inputs[i];
		int64_t tuple[MCCJIT_KGC_ARITY];
		int hit, flagged = 0, ok;
		int64_t returned, want = x * 2 + 1;
		int vv = ((int (*)(int))variant)((int)x);
		int bv = baseline((int)x);
		uint32_t j;
		for (j = 0; j < MCCJIT_KGC_ARITY; j++)
			tuple[j] = 0;
		tuple[0] = x;
		hit = mccjit_kgc_contains(&kgc, tuple);
		returned = mccjit_kgc_call1(&kgc, variant, baseline, x, &flagged);
		if (flagged)
			variant_flagged = 1;
		ok = (returned == want);
		if (i == 1 && !hit)
			ok = 0;
		if (x != 7 && !flagged)
			ok = 0;
		if (x == 7 && flagged)
			ok = 0;
		printf("mccjit-selftest-kgc: %4lld  %-4s  %7d  %8d  %8lld  %7d  %s\n",
					 (long long)x, hit ? "HIT" : "MISS", vv, bv, (long long)returned,
					 flagged, ok ? "OK" : "FAIL");
		if (!ok)
			fails++;
	}
	printf("mccjit-selftest-kgc: variant flagged-unsound=%d (expected 1)\n",
				 variant_flagged);
	if (!variant_flagged)
		fails++;
	{
		int64_t t7[MCCJIT_KGC_ARITY];
		uint32_t j;
		for (j = 0; j < MCCJIT_KGC_ARITY; j++)
			t7[j] = 0;
		t7[0] = 7;
		if (!mccjit_kgc_contains(&kgc, t7)) {
			printf("mccjit-selftest-kgc: x=7 not cached after verify\n");
			fails++;
		}
	}
	mccjit_kgc_close(&kgc);

	{
		char path[] = "/tmp/mccjit_kgc_XXXXXX";
		int fd = mkstemp(path);
		uint64_t salt = mccjit_salt_witness();
		int64_t vals[5] = {100, -7, 42, 7, 3};
		int j;
		MccjitKgc p;
		uint64_t saved = 0;
		if (fd >= 0)
			close(fd);
		if (fd < 0 || mccjit_kgc_open(&p, path, salt, 1) != 0) {
			printf("mccjit-selftest-kgc: persistence open failed\n");
			fails++;
		} else {
			for (j = 0; j < 5; j++) {
				int64_t t[MCCJIT_KGC_ARITY];
				uint32_t m;
				for (m = 0; m < MCCJIT_KGC_ARITY; m++)
					t[m] = 0;
				t[0] = vals[j];
				mccjit_kgc_insert(&p, t);
			}
			saved = p.hdr->count;
			mccjit_kgc_close(&p);
			printf("mccjit-selftest-kgc: persistence wrote %llu tuples, closed\n",
						 (unsigned long long)saved);
			if (mccjit_kgc_open(&p, path, salt, 1) != 0) {
				printf("mccjit-selftest-kgc: persistence reopen failed\n");
				fails++;
			} else {
				int survived = (p.hdr->count == saved);
				for (j = 0; j < 5; j++) {
					int64_t t[MCCJIT_KGC_ARITY];
					uint32_t m;
					for (m = 0; m < MCCJIT_KGC_ARITY; m++)
						t[m] = 0;
					t[0] = vals[j];
					if (!mccjit_kgc_contains(&p, t))
						survived = 0;
				}
				printf("mccjit-selftest-kgc: reopened count=%llu survived=%s\n",
							 (unsigned long long)p.hdr->count, survived ? "yes" : "no");
				if (!survived)
					fails++;
				mccjit_kgc_close(&p);
			}
			if (mccjit_kgc_open(&p, path, salt ^ 0xdeadbeefull, 1) == 0) {
				printf("mccjit-selftest-kgc: stale-salt reopen count=%llu (expect 0 reset)\n",
							 (unsigned long long)p.hdr->count);
				if (p.hdr->count != 0)
					fails++;
				mccjit_kgc_close(&p);
			}
			unlink(path);
		}
	}

	if (vstate)
		mcc_delete(vstate);
	mccjit_last_state = NULL;
	mcc_delete(s1);
	printf("mccjit-selftest-kgc: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static int mccjit_classify_blob(const void *buf, size_t len) {
	MccjitIntent it;
	MCCState *js;
	int purity;
	js = mcc_new();
	if (!js)
		return -1;
	js->optimize = 0;
	js->nostdlib = 1;
	mcc_set_output_type(js, MCC_OUTPUT_MEMORY);
	mcc_enter_state(js);
	mccpp_new(js);
	mccgen_init(js);
	anon_sym = SYM_FIRST_ANOM;
	funcname = "";
	func_ind = -1;
	if (mccjit_intent_deserialize(buf, len, &it) != 0) {
		mcc_exit_state(js);
		mcc_delete(js);
		return -1;
	}
	purity = ast_fn_purity(it.arena);
	mccjit_intent_release(&it);
	mcc_exit_state(js);
	mcc_delete(js);
	return purity;
}

static const char *mccjit_purity_name(int p) {
	switch (p) {
	case AST_PURITY_TIER0:
		return "TIER0";
	case AST_PURITY_TIER1:
		return "TIER1";
	case AST_PURITY_IMPURE:
		return "IMPURE";
	default:
		return "ERR";
	}
}

int mccjit_selftest_purity(void) {
	static const struct {
		const char *src;
		const char *fn;
		int nostdlib;
		int want;
	} cases[4] = {
			{"int f(int x){return x*2+1;}", "f", 1, AST_PURITY_TIER0},
			{"int g(int *p, int x){return p ? *p + x : -1;}", "g", 1,
			 AST_PURITY_TIER1},
			{"int s(int *p, int x){*p = x; return x;}", "s", 1, AST_PURITY_IMPURE},
			{"int abs(int); int h(int x){return abs(x) + 1;}", "h", 1,
			 AST_PURITY_IMPURE},
	};
	int fails = 0;
	int i;

	printf("mccjit-selftest-purity: begin\n");
	printf("mccjit-selftest-purity: classify  fn  got       want      ok\n");
	for (i = 0; i < 4; i++) {
		unsigned char *blob;
		size_t blen;
		MCCState *s1;
		int got, ok;
		blob = mccjit_stash_one(cases[i].src, cases[i].fn, cases[i].nostdlib, &blen,
														&s1);
		if (!s1 || !blob) {
			printf("mccjit-selftest-purity: %s stash failed\n", cases[i].fn);
			if (s1)
				mcc_delete(s1);
			mcc_free(blob);
			fails++;
			continue;
		}
		got = mccjit_classify_blob(blob, blen);
		ok = (got == cases[i].want);
		printf("mccjit-selftest-purity:           %-3s %-9s %-9s %s\n", cases[i].fn,
					 mccjit_purity_name(got), mccjit_purity_name(cases[i].want),
					 ok ? "OK" : "FAIL");
		if (!ok)
			fails++;
		mcc_free(blob);
		mcc_delete(s1);
	}

	{
		static const struct {
			const char *src;
			const char *fn;
			int want;
		} wire[2] = {
				{"int f(int x){return x*2+1;}", "f", AST_PURITY_TIER0},
				{"int g(int *p, int x){return p ? *p + x : -1;}", "g",
				 AST_PURITY_TIER1},
		};
		int w;
		printf("mccjit-selftest-purity: recompile-path wiring (mccjit_last_purity "
					 "set by mcc_jit_recompile_blob):\n");
		for (w = 0; w < 2; w++) {
			unsigned char *blob;
			size_t blen;
			MCCState *s1;
			void *rj;
			int tier, ok;
			blob = mccjit_stash_one(wire[w].src, wire[w].fn, 1, &blen, &s1);
			if (!s1 || !blob) {
				if (s1)
					mcc_delete(s1);
				mcc_free(blob);
				fails++;
				continue;
			}
			rj = mcc_jit_recompile_blob(blob, blen);
			tier = mccjit_last_purity;
			if (rj && mccjit_last_state)
				mcc_delete(mccjit_last_state);
			mccjit_last_state = NULL;
			ok = (tier == wire[w].want);
			printf("mccjit-selftest-purity:           %-3s -> %-6s memoize_ok=%d %s\n",
						 wire[w].fn, mccjit_purity_name(tier), tier == AST_PURITY_TIER0,
						 ok ? "OK" : "FAIL");
			if (!ok)
				fails++;
			mcc_free(blob);
			mcc_delete(s1);
		}
	}

	{
		static const char src_t[] = "int *gp; int t(int x){return *gp + x;}";
		static const char src_tv[] = "int tv(int x){return x + 10;}";
		unsigned char *tblob = NULL, *vblob = NULL;
		size_t tlen = 0, vlen = 0;
		MCCState *st = NULL, *sv = NULL;
		int (*baseline)(int) = NULL;
		int (*variant)(int) = NULL;
		int **gpp = NULL;
		int cell = 0;
		int tier = -1;

		tblob = mccjit_stash_one(src_t, "t", 1, &tlen, &st);
		if (st && tblob && mcc_relocate(st) == 0) {
			baseline = (int (*)(int))mcc_get_symbol(st, "t");
			gpp = (int **)mcc_get_symbol(st, "gp");
		}
		vblob = mccjit_stash_one(src_tv, "tv", 1, &vlen, &sv);
		if (sv && vblob && mcc_relocate(sv) == 0)
			variant = (int (*)(int))mcc_get_symbol(sv, "tv");

		if (tblob)
			tier = mccjit_classify_blob(tblob, tlen);

		printf("mccjit-selftest-purity: gating demo tier(t)=%s (memoize_ok=%d)\n",
					 mccjit_purity_name(tier), tier == AST_PURITY_TIER0);
		if (tier != AST_PURITY_TIER1)
			fails++;

		if (gpp)
			*gpp = &cell;

		if (!baseline || !variant || !gpp) {
			printf("mccjit-selftest-purity: gating demo setup failed "
						 "(baseline=%p variant=%p gpp=%p)\n",
						 (void *)baseline, (void *)variant, (void *)gpp);
			fails++;
		} else {
			MccjitKgc ka, kb;
			int oa = mccjit_kgc_open(&ka, NULL, mccjit_salt_witness(), 1);
			int ob = mccjit_kgc_open(&kb, NULL, mccjit_salt_witness(), 1);
			if (oa != 0 || ob != 0) {
				printf("mccjit-selftest-purity: kgc open failed\n");
				fails++;
			} else {
				int64_t r1, r2, r3, r4;
				int f1 = 0, f2 = 0, f3 = 0, f4 = 0;

				ka.memoize_ok = 1;
				kb.memoize_ok = (tier == AST_PURITY_TIER0);

				printf("mccjit-selftest-purity: --- gating demo: t is %s, correct "
							 "treatment memoize_ok=%d ---\n",
							 mccjit_purity_name(tier), kb.memoize_ok);
				printf("mccjit-selftest-purity: t(x)=*gp+x ; "
							 "variant=speculative t (assumes *gp==10)\n");

				cell = 10;
				r1 = mccjit_kgc_call1(&ka, (void *)variant, (void *)baseline, 5, &f1);
				cell = 100;
				r2 = mccjit_kgc_call1(&ka, (void *)variant, (void *)baseline, 5, &f2);
				printf("mccjit-selftest-purity: [memoize_ok=1] t(5) *gp=10 ->%lld ; "
							 "*gp=100 (repeat)->%lld flagged=%d  (unsound: stale, want 105)\n",
							 (long long)r1, (long long)r2, f2);

				cell = 10;
				r3 = mccjit_kgc_call1(&kb, (void *)variant, (void *)baseline, 5, &f3);
				cell = 100;
				r4 = mccjit_kgc_call1(&kb, (void *)variant, (void *)baseline, 5, &f4);
				printf("mccjit-selftest-purity: [memoize_ok=0] t(5) *gp=10 ->%lld ; "
							 "*gp=100 (repeat)->%lld flagged=%d  (sound: re-differentiated)\n",
							 (long long)r3, (long long)r4, f4);

				if (r1 != 15) {
					printf("mccjit-selftest-purity: setup r1 expected 15\n");
					fails++;
				}
				if (r2 != 15) {
					printf("mccjit-selftest-purity: memoize_ok=1 did not fast-path stale\n");
					fails++;
				}
				if (r4 != 105 || !f4) {
					printf("mccjit-selftest-purity: memoize_ok=0 did not re-differentiate "
								 "(r4=%lld f4=%d, want 105/1)\n",
								 (long long)r4, f4);
					fails++;
				}
				mccjit_kgc_close(&ka);
				mccjit_kgc_close(&kb);
			}
		}

		mcc_free(tblob);
		mcc_free(vblob);
		if (st)
			mcc_delete(st);
		if (sv)
			mcc_delete(sv);
	}

	mcc_free(mccjit_last_blob);
	mccjit_last_blob = NULL;
	mccjit_last_len = 0;
	mccjit_last_state = NULL;
	printf("mccjit-selftest-purity: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_lazy(void) {
	static const char src[] = "int f(int x){return x*2+1;}";
	unsigned char *blob;
	size_t blen;
	MCCState *s1;
	int (*baseline)(int) = NULL;
	MCCState *bstate = NULL;
	void *slot = NULL;
	MccjitCounterState st;
	long threshold = 8;
	int inputs[6] = {5, 0, -3, 100, 7, -40};
	int fails = 0;
	long c;
	int i;

	printf("mccjit-selftest-lazy: begin (threshold=%ld)\n", threshold);

	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) {
		printf("mccjit-selftest-lazy: stash failed\n");
		if (s1)
			mcc_delete(s1);
		mcc_free(blob);
		return 1;
	}
	baseline = (int (*)(int))mcc_jit_recompile_blob(blob, blen);
	bstate = mccjit_last_state;
	mccjit_last_state = NULL;
	if (!baseline) {
		printf("mccjit-selftest-lazy: baseline recompile returned NULL\n");
		mcc_free(blob);
		mcc_delete(s1);
		return 1;
	}

	slot = (void *)baseline;
	memset(&st, 0, sizeof st);
	st.slot = &slot;
	st.blob = blob;
	st.len = blen;
	st.baseline = (void *)baseline;
	st.threshold = threshold;
	st.count = 0;
	st.promoted = NULL;
	pthread_mutex_init(&st.lock, NULL);

	printf("mccjit-selftest-lazy: cold phase (calls 1..%ld run baseline):\n",
				 threshold - 1);
	for (c = 1; c < threshold; c++) {
		void *t = mccjit_counter_tick(&st, NULL);
		int x = inputs[(c - 1) % 6];
		int got = ((int (*)(int))t)(x);
		int want = x * 2 + 1;
		int ok = (t == (void *)baseline) && (got == want) && (st.promoted == NULL) &&
						 (slot == (void *)baseline);
		printf("mccjit-selftest-lazy: cold call %ld path=%s f(%d)=%d expect=%d %s\n", c,
					 t == (void *)baseline ? "baseline" : "other", x, got, want,
					 ok ? "OK" : "FAIL");
		if (!ok)
			fails++;
	}

	{
		void *t = mccjit_counter_tick(&st, NULL);
		int ok = (st.promoted != NULL) && (t == st.promoted) &&
						 (slot == st.promoted) && (t != (void *)baseline);
		printf("mccjit-selftest-lazy: PROMOTE at call %ld promoted=%p slot=%p %s\n",
					 st.count, st.promoted, slot, ok ? "OK" : "FAIL");
		if (!ok)
			fails++;
	}

	for (i = 0; i < 3; i++) {
		void *t = mccjit_counter_tick(&st, NULL);
		int ok = (t == st.promoted) && (slot == st.promoted);
		printf("mccjit-selftest-lazy: hot call %ld path=promoted stable=%s %s\n",
					 st.count, t == st.promoted ? "yes" : "no", ok ? "OK" : "FAIL");
		if (!ok)
			fails++;
	}

	{
		int (*v)(int) = (int (*)(int))mcc_jit_recompile_blob(blob, blen);
		MCCState *vstate = mccjit_last_state;
		mccjit_last_state = NULL;
		if (!v) {
			printf("mccjit-selftest-lazy: post-promote variant recompile NULL\n");
			fails++;
		} else {
			for (i = 0; i < 6; i++) {
				int x = inputs[i];
				int got = v(x);
				int want = x * 2 + 1;
				int ok = (got == want);
				printf("mccjit-selftest-lazy: post-promote variant f(%d)=%d expect=%d %s\n",
							 x, got, want, ok ? "OK" : "FAIL");
				if (!ok)
					fails++;
			}
		}
		if (vstate)
			mcc_delete(vstate);
	}

	pthread_mutex_destroy(&st.lock);
	if (bstate)
		mcc_delete(bstate);
	mccjit_last_state = NULL;
	mcc_free(blob);
	mcc_delete(s1);
	printf("mccjit-selftest-lazy: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static void mccjit_pool_nap(void) {
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 1000000;
	nanosleep(&ts, NULL);
}

int mccjit_selftest_pool(void) {
	static const char src[] = "int f(int x){return x*2+1;}";
	static void *slot_a;
	static void *slot_b;
	static MccjitCounterState st;
	unsigned char *blob;
	size_t blen;
	MCCState *s1;
	int (*baseline)(int) = NULL;
	int inputs[6] = {5, 0, -3, 100, 7, -40};
	int fails = 0;
	int i;
	int nw;

	printf("mccjit-selftest-pool: begin\n");

	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) {
		printf("mccjit-selftest-pool: stash failed\n");
		if (s1)
			mcc_delete(s1);
		mcc_free(blob);
		return 1;
	}
	baseline = (int (*)(int))mcc_jit_recompile_blob(blob, blen);
	mccjit_last_state = NULL;
	if (!baseline) {
		printf("mccjit-selftest-pool: baseline recompile returned NULL\n");
		return 1;
	}

	nw = mccjit_pool_start(2);
	printf("mccjit-selftest-pool: pool workers=%d\n", nw);
	if (nw <= 0) {
		printf("mccjit-selftest-pool: pool start failed FAIL\n");
		return 1;
	}

	slot_a = (void *)baseline;
	{
		MccjitSwapJob *job = mcc_malloc(sizeof *job);
		void *pub = (void *)baseline;
		int spins = 0;
		if (!job) {
			printf("mccjit-selftest-pool: eager job alloc failed FAIL\n");
			fails++;
		} else {
			job->run = mccjit_job_run_eager;
			job->slot = &slot_a;
			job->blob = blob;
			job->len = blen;
			job->max_duration = 0;
			job->timed = 0;
			mccjit_pool_enqueue(job);
			while (spins++ < 5000) {
				pub = __atomic_load_n(&slot_a, __ATOMIC_ACQUIRE);
				if (pub != (void *)baseline)
					break;
				mccjit_pool_nap();
			}
			if (pub == (void *)baseline) {
				printf("mccjit-selftest-pool: eager async never published (timeout) FAIL\n");
				fails++;
			} else {
				for (i = 0; i < 6; i++) {
					int x = inputs[i];
					int got = ((int (*)(int))pub)(x);
					int want = x * 2 + 1;
					int ok = got == want;
					printf("mccjit-selftest-pool: eager pub f(%d)=%d expect=%d %s\n", x, got,
								 want, ok ? "OK" : "FAIL");
					if (!ok)
						fails++;
				}
			}
		}
	}

	{
		long threshold = 4;
		long c;
		void *promoted = NULL;
		int spins = 0;
		int building = 0;
		slot_b = (void *)baseline;
		memset(&st, 0, sizeof st);
		st.slot = &slot_b;
		st.blob = blob;
		st.len = blen;
		st.baseline = (void *)baseline;
		st.threshold = threshold;
		pthread_mutex_init(&st.lock, NULL);

		for (c = 1; c < threshold; c++) {
			void *t = mccjit_counter_tick(&st, NULL);
			int ok = (t == (void *)baseline);
			if (!ok) {
				printf("mccjit-selftest-pool: cold call %ld not baseline FAIL\n", c);
				fails++;
			}
		}
		{
			void *t = mccjit_counter_tick(&st, NULL);
			pthread_mutex_lock(&st.lock);
			building = st.building;
			promoted = st.promoted;
			pthread_mutex_unlock(&st.lock);
			printf("mccjit-selftest-pool: cross call %ld path=%s building=%d %s\n",
						 st.count, t == (void *)baseline ? "baseline" : "other", building,
						 (t == (void *)baseline) ? "OK" : "FAIL");
			if (t != (void *)baseline)
				fails++;
		}
		while (spins++ < 5000) {
			pthread_mutex_lock(&st.lock);
			promoted = st.promoted;
			pthread_mutex_unlock(&st.lock);
			if (promoted)
				break;
			mccjit_pool_nap();
		}
		if (!promoted) {
			printf("mccjit-selftest-pool: async promote never landed (timeout) FAIL\n");
			fails++;
		} else {
			printf("mccjit-selftest-pool: PROMOTE-ASYNC promoted=%p slot=%p %s\n",
						 promoted, slot_b, (slot_b == promoted) ? "OK" : "FAIL");
			if (slot_b != promoted)
				fails++;
			for (i = 0; i < 6; i++) {
				void *t = mccjit_counter_tick(&st, NULL);
				int x = inputs[i];
				int got = ((int (*)(int))t)(x);
				int want = x * 2 + 1;
				int ok = (t == promoted) && (got == want);
				printf("mccjit-selftest-pool: promoted f(%d)=%d expect=%d %s\n", x, got,
							 want, ok ? "OK" : "FAIL");
				if (!ok)
					fails++;
			}
		}
	}

	printf("mccjit-selftest-pool: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_eligibility(void) {
	static const struct {
		const char *src;
		const char *fn;
		int eligible;
		const char *why;
	} cases[] = {
			{"int f(int x){return x*2+1;}", "f", 1, "GP-int arg + int return"},
			{"long f(long a, long b){return a+b;}", "f", 1, "two GP args"},
			{"int f(int *p){return *p;}", "f", 1, "pointer arg"},
			{"int f(int a,int b,int c,int d,int e,int g){return a+b+c+d+e+g;}", "f", 1,
			 "six GP args (ABI max)"},
			{"double f(int x){return x*1.5;}", "f", 0, "FP return"},
			{"int f(double x){return (int)x;}", "f", 0, "FP arg"},
			{"struct S{int a;int b;}; struct S f(int x){struct S s; s.a=x; s.b=-x; return s;}",
			 "f", 0, "struct-by-value return"},
			{"struct S{int a;int b;}; int f(struct S s){return s.a+s.b;}", "f", 0,
			 "struct-by-value arg"},
			{"void f(int x){(void)x;}", "f", 0, "void return (not verifiable)"},
			{"int f(void){return 42;}", "f", 0, "zero args"},
			{"int f(int a,int b,int c,int d,int e,int g,int h){return a+h;}", "f", 0,
			 "seven args (over ABI max)"},
			{"int f(int x, ...){return x;}", "f", 0, "variadic"},
	};
	int n = (int)(sizeof cases / sizeof cases[0]);
	int fails = 0;
	int i;

	printf("mccjit-selftest-eligibility: begin (%d cases)\n", n);

	for (i = 0; i < n; i++) {
		size_t blen = 0;
		MCCState *st = NULL;
		unsigned char *blob =
				mccjit_stash_one(cases[i].src, cases[i].fn, 1, &blen, &st);
		int selected = (blob != NULL);
		int compiled = (st != NULL);
		int ok;
		if (!compiled) {
			printf("mccjit-selftest-eligibility: [%s] compile FAILED (setup)\n",
						 cases[i].why);
			fails++;
		} else {
			ok = (selected == cases[i].eligible);
			printf(
					"mccjit-selftest-eligibility: %-28s want=%s got=%s %s\n", cases[i].why,
					cases[i].eligible ? "jit" : "refuse", selected ? "jit" : "refuse",
					ok ? "OK" : "FAIL");
			if (!ok)
				fails++;
		}
		mcc_free(blob);
		if (st)
			mcc_delete(st);
	}

	mcc_free(mccjit_last_blob);
	mccjit_last_blob = NULL;
	mccjit_last_len = 0;
	mccjit_last_state = NULL;

	printf("mccjit-selftest-eligibility: %s (%d failure%s)\n",
				 fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_fork(void) {
	static const char src[] = "int f(int x){return x*2+1;}";
	unsigned char *blob;
	size_t blen;
	MCCState *s1;
	void *baseline;
	int nw;
	int fails = 0;
	pid_t pid;

	printf("mccjit-selftest-fork: begin\n");

	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) {
		printf("mccjit-selftest-fork: stash failed\n");
		if (s1)
			mcc_delete(s1);
		mcc_free(blob);
		return 1;
	}
	baseline = mcc_jit_recompile_blob(blob, blen);
	if (!baseline) {
		printf("mccjit-selftest-fork: baseline recompile NULL\n");
		mcc_free(blob);
		mcc_delete(s1);
		return 1;
	}
	mccjit_last_state = NULL;

	nw = mccjit_pool_start(2);
	printf("mccjit-selftest-fork: parent pool workers=%d started=%d\n", nw,
				 mccjit_pool.started);
	if (nw <= 0 || !mccjit_pool.started) {
		printf("mccjit-selftest-fork: pool failed to start FAIL\n");
		return 1;
	}

	fflush(stdout);
	pid = fork();
	if (pid == 0) {
		int cf = 0;
		if (mccjit_pool.started != 0 || mccjit_pool.nworkers != 0) {
			fprintf(stderr,
							"mccjit-selftest-fork[child]: phantom pool NOT reset "
							"(started=%d nworkers=%d) FAIL\n",
							mccjit_pool.started, mccjit_pool.nworkers);
			cf++;
		}
		if (((int (*)(int))baseline)(9) != 19) {
			fprintf(stderr,
							"mccjit-selftest-fork[child]: installed variant f(9)!=19 FAIL\n");
			cf++;
		}
		if (mccjit_pool_start(2) <= 0) {
			fprintf(stderr,
							"mccjit-selftest-fork[child]: pool locks unusable post-fork "
							"(deadlock/start failure) FAIL\n");
			cf++;
		}
		fflush(stderr);
		_exit(cf ? 1 : 0);
	}

	if (pid < 0) {
		printf("mccjit-selftest-fork: fork() failed FAIL\n");
		fails++;
	} else {
		int status = 0;
		while (waitpid(pid, &status, 0) < 0)
			;
		{
			int child_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
			printf(
					"mccjit-selftest-fork: child pool reset + variant runs after fork: %s\n",
					child_ok ? "OK" : "FAIL");
			if (!child_ok)
				fails++;
		}
		if (mccjit_pool.started != 1 || mccjit_pool.nworkers != nw ||
				((int (*)(int))baseline)(9) != 19) {
			printf("mccjit-selftest-fork: parent pool broken after fork "
						 "(started=%d nworkers=%d) FAIL\n",
						 mccjit_pool.started, mccjit_pool.nworkers);
			fails++;
		} else {
			printf("mccjit-selftest-fork: parent pool intact after fork OK\n");
		}
	}

	mcc_free(blob);
	mcc_delete(s1);
	printf("mccjit-selftest-fork: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_observability(void) {
	static const char src[] = "int f(int x){return x*2+1;}";
	int fails = 0;
	char path[64];
	unsigned char *blob;
	size_t blen;
	MCCState *s1;

	printf("mccjit-selftest-observability: begin\n");

	if (!mccjit_feasible()) {
		printf("mccjit-selftest-observability: exec-mem probe infeasible on this "
					 "host FAIL\n");
		fails++;
	} else {
		printf("mccjit-selftest-observability: exec-mem probe feasible OK\n");
	}

	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) {
		printf("mccjit-selftest-observability: stash failed\n");
		if (s1)
			mcc_delete(s1);
		mcc_free(blob);
		return fails + 1;
	}

	{
		void *baseline = mcc_jit_recompile_blob(blob, blen);
		MCCState *bstate = mccjit_last_state;
		void *slot = baseline;
		mccjit_last_state = NULL;
		if (!baseline) {
			printf("mccjit-selftest-observability: baseline recompile NULL FAIL\n");
			fails++;
		} else {
			setenv("MCC_JIT_FORCE_INFEASIBLE", "1", 1);
			mccjit_boot_swap(&slot, blob, blen);
			unsetenv("MCC_JIT_FORCE_INFEASIBLE");
			if (slot != baseline) {
				printf("mccjit-selftest-observability: infeasible boot swapped anyway "
							 "(no silent fallback) FAIL\n");
				fails++;
			} else {
				printf("mccjit-selftest-observability: infeasible -> kept AOT baseline "
							 "OK\n");
			}
		}
		if (bstate)
			mcc_delete(bstate);
		mccjit_last_state = NULL;
	}

	{
		void *v;
		MCCState *vstate;
		FILE *f;
		int found = 0;
		snprintf(path, sizeof path, "/tmp/perf-%d.map", (int)getpid());
		remove(path);
		setenv("MCC_JIT_PERF_MAP", "1", 1);
		v = mcc_jit_recompile_blob(blob, blen);
		vstate = mccjit_last_state;
		mccjit_last_state = NULL;
		unsetenv("MCC_JIT_PERF_MAP");
		if (!v) {
			printf("mccjit-selftest-observability: perf-map recompile NULL FAIL\n");
			fails++;
		}
		f = fopen(path, "r");
		if (f) {
			char line[256];
			while (fgets(line, sizeof line, f)) {
				unsigned long a = 0, sz = 0;
				char nm[128] = {0};
				if (sscanf(line, "%lx %lx %127s", &a, &sz, nm) == 3 &&
						!strcmp(nm, "f") && a == (unsigned long)(uintptr_t)v && sz > 0)
					found = 1;
			}
			fclose(f);
		}
		printf("mccjit-selftest-observability: perf-map %s (%s addr=%p) %s\n",
					 found ? "line for 'f' present" : "line MISSING", path, v,
					 found ? "OK" : "FAIL");
		if (!found)
			fails++;
		remove(path);
		if (vstate)
			mcc_delete(vstate);
		mccjit_last_state = NULL;
	}

	mcc_free(blob);
	mcc_delete(s1);
	printf("mccjit-selftest-observability: %s (%d failure%s)\n",
				 fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_liverun(const char *libpath, const char *incpath) {
	static const char src[] =
			"int f(int x){return x*3+7;}\nint main(void){return f(11);}\n";
	MCCState *s;
	char *av[] = {"liverun", NULL};
	char path[64];
	int fails = 0;
	int rc;
	int perf_found = 0;

	printf("mccjit-selftest-liverun: begin\n");

	setenv("MCC_AST_JIT_DISPATCH", "6", 1);
	setenv("MCC_JIT_PERF_MAP", "1", 1);
	snprintf(path, sizeof path, "/tmp/perf-%d.map", (int)getpid());
	remove(path);

	s = mcc_new();
	if (!s) {
		printf("mccjit-selftest-liverun: mcc_new failed\n");
		unsetenv("MCC_AST_JIT_DISPATCH");
		unsetenv("MCC_JIT_PERF_MAP");
		return 1;
	}
	if (libpath)
		mcc_set_lib_path(s, libpath);
	if (incpath)
		mcc_add_include_path(s, incpath);
	s->optimize = 1;
	s->embed_jit = 1;
	s->jit_threads = 0;
	mcc_free(s->jit_functions);
	s->jit_functions = mcc_strdup("f");
	mcc_set_output_type(s, MCC_OUTPUT_MEMORY);

	if (mcc_compile_string(s, src) != 0) {
		printf("mccjit-selftest-liverun: compile failed FAIL\n");
		fails++;
		rc = -1;
	} else {
		rc = mcc_run(s, 1, av);
	}
	unsetenv("MCC_AST_JIT_DISPATCH");
	unsetenv("MCC_JIT_PERF_MAP");

	printf("mccjit-selftest-liverun: main() returned %d (expect 40) %s\n", rc,
				 rc == 40 ? "OK" : "FAIL");
	if (rc != 40)
		fails++;

	{
		FILE *pf = fopen(path, "r");
		if (pf) {
			char line[256];
			while (fgets(line, sizeof line, pf)) {
				char nm[128] = {0};
				unsigned long a = 0, sz = 0;
				if (sscanf(line, "%lx %lx %127s", &a, &sz, nm) == 3 &&
						!strcmp(nm, "f"))
					perf_found = 1;
			}
			fclose(pf);
		}
	}
	printf("mccjit-selftest-liverun: live recompile %s during .init_array ctor %s\n",
				 perf_found ? "fired" : "did NOT fire", perf_found ? "OK" : "FAIL");
	if (!perf_found)
		fails++;
	remove(path);

	mcc_delete(s);
	printf("mccjit-selftest-liverun: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_poison(void) {
	static const char src[] = "int f(int x){return x*2+1;}";
	unsigned char *blob;
	size_t blen;
	MCCState *s1;
	int (*baseline)(int) = NULL;
	void *variant = NULL;
	MCCState *vstate = NULL;
	MccjitKgc kgc;
	int fails = 0;
	int i;
	int flagged_calls = 0;
	int min;
	int poison_at = -1;

	printf("mccjit-selftest-poison: begin\n");

	setenv("MCC_JIT_POISON_MIN", "4", 1);
	setenv("MCC_JIT_POISON_PCT", "50", 1);
	min = mccjit_poison_min();

	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) {
		printf("mccjit-selftest-poison: stash failed\n");
		unsetenv("MCC_JIT_POISON_MIN");
		unsetenv("MCC_JIT_POISON_PCT");
		if (s1)
			mcc_delete(s1);
		mcc_free(blob);
		return 1;
	}
	if (mcc_relocate(s1) == 0)
		baseline = (int (*)(int))mcc_get_symbol(s1, "f");
	variant = mcc_jit_recompile_blob_spec(blob, blen, 0, 7);
	vstate = mccjit_last_state;
	mccjit_last_state = NULL;
	if (!baseline || !variant) {
		printf("mccjit-selftest-poison: baseline/variant build failed FAIL\n");
		fails++;
	} else if (mccjit_kgc_open(&kgc, NULL, mccjit_salt_witness(), 1) != 0) {
		printf("mccjit-selftest-poison: kgc open failed FAIL\n");
		fails++;
	} else {
		for (i = 0; i < 10; i++) {
			int64_t args[1];
			int flagged = 0;
			int64_t r;
			args[0] = 3;
			r = mccjit_kgc_calln(&kgc, variant, baseline, args, 1, &flagged);
			if (kgc.poisoned && poison_at < 0)
				poison_at = i;
			if (flagged)
				flagged_calls++;
			if (r != 7) {
				printf("mccjit-selftest-poison: call %d returned %lld (expect baseline "
							 "7) FAIL\n",
							 i, (long long)r);
				fails++;
			}
		}
		printf(
				"mccjit-selftest-poison: mismatches-flagged=%d (expect %d) poisoned=%d "
				"first-poison-call=%d %s\n",
				flagged_calls, min, kgc.poisoned, poison_at,
				(flagged_calls == min && kgc.poisoned) ? "OK" : "FAIL");
		if (flagged_calls != min || !kgc.poisoned)
			fails++;
		if (kgc.hits != 0 || kgc.misses != (uint64_t)min) {
			printf("mccjit-selftest-poison: counters hits=%llu misses=%llu "
						 "(expect 0/%d) FAIL\n",
						 (unsigned long long)kgc.hits, (unsigned long long)kgc.misses, min);
			fails++;
		}
		mccjit_kgc_close(&kgc);
	}

	unsetenv("MCC_JIT_POISON_MIN");
	unsetenv("MCC_JIT_POISON_PCT");
	if (vstate)
		mcc_delete(vstate);
	mccjit_last_state = NULL;
	mcc_free(blob);
	mcc_delete(s1);
	printf("mccjit-selftest-poison: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static long mccjit_bench_fast_fn(long x) {
	long s = 0;
	int i;
	for (i = 0; i < 30; i++)
		s += (x ^ (long)i) * 2654435761L;
	return s;
}

static long mccjit_bench_slow_fn(long x) {
	long s = 0;
	int i;
	for (i = 0; i < 900; i++)
		s += (x ^ (long)i) * 2654435761L;
	return s;
}

int mccjit_selftest_bench(void) {
	int64_t tuples[4 * MCCJIT_KGC_ARITY];
	uint32_t nt = 4, i, j;
	int fails = 0;
	int r_win, r_lose, r_tie;

	printf("mccjit-selftest-bench: begin\n");
	setenv("MCC_JIT_BENCH_ITERS", "3000", 1);
	setenv("MCC_JIT_BENCH_MARGIN_PCT", "10", 1);
	for (i = 0; i < nt; i++)
		for (j = 0; j < MCCJIT_KGC_ARITY; j++)
			tuples[i * MCCJIT_KGC_ARITY + j] = (int64_t)(i * 7 + 1);

	r_win = mccjit_bench_pair((void *)mccjit_bench_fast_fn,
													 (void *)mccjit_bench_slow_fn, tuples, nt, 1, 1);
	r_lose = mccjit_bench_pair((void *)mccjit_bench_slow_fn,
														(void *)mccjit_bench_fast_fn, tuples, nt, 1, 1);
	r_tie = mccjit_bench_pair((void *)mccjit_bench_slow_fn,
													 (void *)mccjit_bench_slow_fn, tuples, nt, 1, 1);
	unsetenv("MCC_JIT_BENCH_ITERS");
	unsetenv("MCC_JIT_BENCH_MARGIN_PCT");

	printf("mccjit-selftest-bench: faster candidate promoted=%d (expect 1) %s\n",
				 r_win, r_win == 1 ? "OK" : "FAIL");
	if (r_win != 1)
		fails++;
	printf("mccjit-selftest-bench: slower candidate promoted=%d (expect 0) %s\n",
				 r_lose, r_lose == 0 ? "OK" : "FAIL");
	if (r_lose != 0)
		fails++;
	printf(
			"mccjit-selftest-bench: equal candidate promoted=%d (expect 0, incumbent-wins-tie) %s\n",
			r_tie, r_tie == 0 ? "OK" : "FAIL");
	if (r_tie != 0)
		fails++;

	printf("mccjit-selftest-bench: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_fparg(const char *libpath, const char *incpath) {
	static const char src[] = "double f(double a, double b){return a*b + a;}";
	static const char src_g[] = "double g(double a, double b){return a*b + b;}";
	unsigned char *blob;
	size_t blen;
	MCCState *s1;
	double (*baseline)(double, double) = NULL;
	double (*variant)(double, double) = NULL;
	MCCState *bstate = NULL, *vstate = NULL;
	int fails = 0;
	int allfp_seen;
	int i;

	printf("mccjit-selftest-fparg: begin\n");

	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) {
		printf("mccjit-selftest-fparg: stash failed\n");
		if (s1)
			mcc_delete(s1);
		mcc_free(blob);
		return 1;
	}
	baseline = (double (*)(double, double))mcc_jit_recompile_blob(blob, blen);
	allfp_seen = mccjit_last_allfp;
	bstate = mccjit_last_state;
	mccjit_last_state = NULL;
	if (!baseline) {
		printf("mccjit-selftest-fparg: baseline recompile NULL FAIL\n");
		mcc_free(blob);
		mcc_delete(s1);
		return 1;
	}
	printf("mccjit-selftest-fparg: all-double signature detected=%d (expect 1) %s\n",
				 allfp_seen, allfp_seen ? "OK" : "FAIL");
	if (!allfp_seen)
		fails++;
	if (baseline(3.0, 4.0) != 15.0 || baseline(2.5, 2.0) != 7.5) {
		printf("mccjit-selftest-fparg: baseline miscomputes FAIL\n");
		fails++;
	}

	variant = (double (*)(double, double))mcc_jit_recompile_blob(blob, blen);
	vstate = mccjit_last_state;
	mccjit_last_state = NULL;

	/* FP differential-verify marshalling (mccjit_invoke_fp + bitwise compare),
	   driven directly — a faithful variant must agree with the baseline and not
	   flag; a divergent one (g) must return the baseline and flag. */
	if (variant) {
		MccjitKgc kgc;
		if (mccjit_kgc_open(&kgc, NULL, mccjit_salt_witness(), 2) == 0) {
			static const struct {
				double a, b, want;
			} cs[5] = {{3, 4, 15},	 {2.5, 2, 7.5}, {-1, 5, -6},
								 {0, 9, 0}, {1.5, 1.5, 3.75}};
			int okall = 1;
			for (i = 0; i < 5; i++) {
				double a2[2];
				int flagged = 0;
				double r;
				a2[0] = cs[i].a;
				a2[1] = cs[i].b;
				r = mccjit_kgc_calln_fp(&kgc, (void *)variant, (void *)baseline, a2, 2,
																&flagged);
				if (r != cs[i].want || flagged)
					okall = 0;
			}
			printf("mccjit-selftest-fparg: faithful FP verify (5 cases) %s\n",
						 okall ? "OK" : "FAIL");
			if (!okall)
				fails++;
			mccjit_kgc_close(&kgc);
		}
	}

	{
		unsigned char *gblob;
		size_t glen;
		MCCState *sg;
		gblob = mccjit_stash_one(src_g, "g", 1, &glen, &sg);
		if (sg && gblob) {
			double (*gv)(double, double) =
					(double (*)(double, double))mcc_jit_recompile_blob(gblob, glen);
			MCCState *gstate = mccjit_last_state;
			mccjit_last_state = NULL;
			if (gv) {
				MccjitKgc kgc;
				if (mccjit_kgc_open(&kgc, NULL, mccjit_salt_witness(), 2) == 0) {
					double args[2];
					int flagged = 0;
					double r;
					args[0] = 3.0;
					args[1] = 4.0;
					r = mccjit_kgc_calln_fp(&kgc, (void *)gv, (void *)baseline, args, 2,
																	&flagged);
					printf("mccjit-selftest-fparg: mismatch f-vs-g returned=%g flagged=%d "
								 "(expect 15,1) %s\n",
								 r, flagged, (r == 15.0 && flagged) ? "OK" : "FAIL");
					if (r != 15.0 || !flagged)
						fails++;
					mccjit_kgc_close(&kgc);
				}
			}
			if (gstate)
				mcc_delete(gstate);
		}
		mcc_free(gblob);
		if (sg)
			mcc_delete(sg);
	}

	/* End-to-end: a -run program self-recompiles its all-double f and calls it
	   through the mode-6 dispatch slot -> the hand-emitted FP stub (movsd spill
	   + calln_fp). This exercises the stub via its correct entry (jmp *slot);
	   direct-calling a dispatch stub corrupts the frame. */
	{
		static const char prog[] =
				"double f(double a, double b){return a*b + a;}\n"
				"int main(void){ double r = f(3.0, 4.0); return (int)r; }\n";
		MCCState *rs;
		char *av[] = {"fparg", NULL};
		char path[64];
		int rc = -1;
		int swapped = 0;
		setenv("MCC_AST_JIT_DISPATCH", "6", 1);
		setenv("MCC_JIT_PERF_MAP", "1", 1);
		snprintf(path, sizeof path, "/tmp/perf-%d.map", (int)getpid());
		remove(path);
		rs = mcc_new();
		if (rs) {
			FILE *pf;
			if (libpath)
				mcc_set_lib_path(rs, libpath);
			if (incpath)
				mcc_add_include_path(rs, incpath);
			rs->optimize = 1;
			rs->embed_jit = 1;
			rs->jit_threads = 0;
			mcc_free(rs->jit_functions);
			rs->jit_functions = mcc_strdup("f");
			mcc_set_output_type(rs, MCC_OUTPUT_MEMORY);
			if (mcc_compile_string(rs, prog) == 0)
				rc = mcc_run(rs, 1, av);
			pf = fopen(path, "r");
			if (pf) {
				char line[256];
				while (fgets(line, sizeof line, pf)) {
					char nm[128] = {0};
					unsigned long a = 0, sz = 0;
					if (sscanf(line, "%lx %lx %127s", &a, &sz, nm) == 3 &&
							!strcmp(nm, "f"))
						swapped = 1;
				}
				fclose(pf);
			}
			mcc_delete(rs);
		}
		remove(path);
		unsetenv("MCC_AST_JIT_DISPATCH");
		unsetenv("MCC_JIT_PERF_MAP");
		printf("mccjit-selftest-fparg: end-to-end dispatch main()=%d (expect 15) "
					 "fp-stub-swapped=%d %s\n",
					 rc, swapped, (rc == 15 && swapped) ? "OK" : "FAIL");
		if (rc != 15 || !swapped)
			fails++;
	}

	if (vstate)
		mcc_delete(vstate);
	if (bstate)
		mcc_delete(bstate);
	mccjit_last_state = NULL;
	mcc_free(blob);
	mcc_delete(s1);
	printf("mccjit-selftest-fparg: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static long mccjit_profile_id1(long x) { return x; }
static long mccjit_profile_sum2(long a, long b) { return a + b; }

int mccjit_selftest_vrange(void) {
	static const char src[] = "int f(int a, int b){return a*100 + b;}";
	unsigned char *blob;
	size_t blen;
	MCCState *s1;
	int (*baseline)(int, int) = NULL;
	MCCState *bstate = NULL;
	MccjitCounterState st;
	void *stub;
	int fails = 0;
	int i;
	int pidx = -1;
	int64_t pval = 0;

	printf("mccjit-selftest-vrange: begin\n");

	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) {
		printf("mccjit-selftest-vrange: stash failed\n");
		if (s1)
			mcc_delete(s1);
		mcc_free(blob);
		return 1;
	}
	baseline = (int (*)(int, int))mcc_jit_recompile_blob(blob, blen);
	bstate = mccjit_last_state;
	mccjit_last_state = NULL;
	if (!baseline) {
		printf("mccjit-selftest-vrange: baseline recompile NULL FAIL\n");
		mcc_free(blob);
		mcc_delete(s1);
		return 1;
	}

	memset(&st, 0, sizeof st);
	st.baseline = (void *)baseline;
	st.threshold = 1L << 30;
	pthread_mutex_init(&st.lock, NULL);
	stub = mccjit_make_counter_stub(&st);
	if (!stub) {
		printf("mccjit-selftest-vrange: counter stub NULL (non-x86_64?) SKIP\n");
		pthread_mutex_destroy(&st.lock);
		if (bstate)
			mcc_delete(bstate);
		mcc_free(blob);
		mcc_delete(s1);
		return 0;
	}
	for (i = 1; i <= 5; i++)
		((int (*)(int, int))stub)(7, i);

	if (!mccjit_profile_pick_const(&st, 2, 3, &pidx, &pval) || pidx != 0 ||
			pval != 7) {
		printf("mccjit-selftest-vrange: const-param detect pidx=%d pval=%lld "
					 "(expect 0,7) FAIL\n",
					 pidx, (long long)pval);
		fails++;
	} else {
		printf("mccjit-selftest-vrange: profile detected const param a==7 OK\n");
	}

	{
		int (*v)(int, int) =
				(int (*)(int, int))mccjit_recompile_profiled(blob, blen, &st, 2, 3);
		MCCState *vstate = mccjit_last_state;
		mccjit_last_state = NULL;
		if (!v) {
			printf("mccjit-selftest-vrange: profiled recompile NULL FAIL\n");
			fails++;
		} else {
			int in_domain = v(7, 5);
			int folded = v(999, 5);
			int base = baseline(7, 5);
			printf("mccjit-selftest-vrange: spec v(7,5)=%d v(999,5)=%d base(7,5)=%d\n",
						 in_domain, folded, base);
			if (in_domain != 705 || base != 705) {
				printf("mccjit-selftest-vrange: in-domain value wrong FAIL\n");
				fails++;
			}
			if (folded != 705) {
				printf("mccjit-selftest-vrange: a NOT folded (v(999,5)=%d, expect 705) "
							 "FAIL\n",
							 folded);
				fails++;
			} else {
				printf("mccjit-selftest-vrange: speculation folded a=7 (variant ignores "
							 "passed a) OK\n");
			}
		}
		if (vstate)
			mcc_delete(vstate);
		mccjit_last_state = NULL;
	}
	pthread_mutex_destroy(&st.lock);

	{
		MccjitCounterState st2;
		void *stub2;
		memset(&st2, 0, sizeof st2);
		st2.baseline = (void *)baseline;
		st2.threshold = 1L << 30;
		pthread_mutex_init(&st2.lock, NULL);
		stub2 = mccjit_make_counter_stub(&st2);
		if (stub2) {
			for (i = 0; i < 5; i++)
				((int (*)(int, int))stub2)(i + 1, i * 2);
			if (mccjit_profile_pick_const(&st2, 2, 3, NULL, NULL)) {
				printf("mccjit-selftest-vrange: false-positive const on varying params "
							 "FAIL\n");
				fails++;
			} else {
				int (*v2)(int, int) =
						(int (*)(int, int))mccjit_recompile_profiled(blob, blen, &st2, 2, 3);
				MCCState *v2state = mccjit_last_state;
				mccjit_last_state = NULL;
				if (v2 && v2(999, 5) == baseline(999, 5)) {
					printf("mccjit-selftest-vrange: no const param -> unspecialized "
								 "(v(999,5)=base) OK\n");
				} else {
					printf("mccjit-selftest-vrange: no-const case wrong FAIL\n");
					fails++;
				}
				if (v2state)
					mcc_delete(v2state);
				mccjit_last_state = NULL;
			}
		}
		pthread_mutex_destroy(&st2.lock);
	}

	if (bstate)
		mcc_delete(bstate);
	mccjit_last_state = NULL;
	mcc_free(blob);
	mcc_delete(s1);
	printf("mccjit-selftest-vrange: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_profile(void) {
	MccjitCounterState st;
	void *stub;
	int fails = 0;
	int64_t inputs[5] = {5, 0, -3, 100, 7};
	int i;

	printf("mccjit-selftest-profile: begin\n");

	memset(&st, 0, sizeof st);
	st.baseline = (void *)mccjit_profile_id1;
	st.threshold = 1L << 30;
	pthread_mutex_init(&st.lock, NULL);
	stub = mccjit_make_counter_stub(&st);
	if (!stub) {
		printf("mccjit-selftest-profile: counter stub NULL (non-x86_64?) SKIP\n");
		pthread_mutex_destroy(&st.lock);
		return 0;
	}
	for (i = 0; i < 5; i++) {
		long r = ((long (*)(long))stub)((long)inputs[i]);
		if (r != inputs[i]) {
			printf("mccjit-selftest-profile: stub(%lld) returned %ld (expect %lld) FAIL\n",
						 (long long)inputs[i], r, (long long)inputs[i]);
			fails++;
		}
	}
	printf("mccjit-selftest-profile: 1-arg range=[%lld,%lld] seen=%ld nsample=%d\n",
				 (long long)st.argmin[0], (long long)st.argmax[0], st.argseen,
				 st.nsample);
	if (st.argmin[0] != -3 || st.argmax[0] != 100 || st.argseen != 5) {
		printf("mccjit-selftest-profile: 1-arg range/seen mismatch FAIL\n");
		fails++;
	}
	if (st.nsample != 5) {
		printf("mccjit-selftest-profile: nsample=%d (expect 5) FAIL\n", st.nsample);
		fails++;
	} else {
		for (i = 0; i < 5; i++)
			if (st.sample[i][0] != inputs[i]) {
				printf("mccjit-selftest-profile: sample[%d][0]=%lld (expect %lld) FAIL\n",
							 i, (long long)st.sample[i][0], (long long)inputs[i]);
				fails++;
			}
	}
	pthread_mutex_destroy(&st.lock);

	memset(&st, 0, sizeof st);
	st.baseline = (void *)mccjit_profile_sum2;
	st.threshold = 1L << 30;
	pthread_mutex_init(&st.lock, NULL);
	stub = mccjit_make_counter_stub(&st);
	if (stub) {
		((long (*)(long, long))stub)(3, 7);
		((long (*)(long, long))stub)(-2, 20);
		printf("mccjit-selftest-profile: 2-arg p0=[%lld,%lld] p1=[%lld,%lld]\n",
					 (long long)st.argmin[0], (long long)st.argmax[0],
					 (long long)st.argmin[1], (long long)st.argmax[1]);
		if (st.argmin[0] != -2 || st.argmax[0] != 3 || st.argmin[1] != 7 ||
				st.argmax[1] != 20) {
			printf("mccjit-selftest-profile: 2-arg per-param range mismatch FAIL\n");
			fails++;
		}
	}
	pthread_mutex_destroy(&st.lock);

	printf("mccjit-selftest-profile: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

void mcc_jit_publish(void **slot, void *variant) {
	if (!slot)
		return;
	__atomic_store_n(slot, variant, __ATOMIC_RELEASE);
}

MCCJIT_LOCAL int mccjit_embed_active(void) {
	return 1;
}

int mccjit_embed_manifest(MCCState *s) {
	if (!s || !s->verbose)
		return mccjit_embed_active();
	printf("embed-jit manifest: functions=%s max-duration=%us%s\n",
				 s->jit_functions ? s->jit_functions : "main", s->jit_max_duration,
				 s->jit_max_duration == 0 ? " (unlimited)" : "");
	printf("embed-jit: Tier-B engine slice linked; graduated-records=%d salt=%016llx\n",
				 JIT_GRADUATED_COUNT, (unsigned long long)mccjit_salt_witness());
	return mccjit_embed_active();
}

#else
typedef int mccjit_embed_translation_unit_not_empty;
#endif /* MCC_EMBED_JIT */
