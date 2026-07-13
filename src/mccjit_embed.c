#ifdef MCC_EMBED_JIT

#include "mcc.h"
#include "mccast.h"
#include "mccgate.h"
#include "algorithms/jit.h"

#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#if defined(__GNUC__) || defined(__clang__)
#define MCCJIT_LOCAL __attribute__((visibility("hidden")))
#else
#define MCCJIT_LOCAL
#endif

#define MCCJIT_INTENT_MAGIC 0x314a434dul
#define MCCJIT_INTENT_FORMAT 3u

#define MCCJIT_ROLE_PLAIN 0u
#define MCCJIT_ROLE_NAMED 1u
#define MCCJIT_ROLE_PTR 2u
#define MCCJIT_ROLE_FUNC 3u

static unsigned mccjit_role_for_base(int t) {
	switch (t & VT_BTYPE) {
	case VT_PTR:
		return MCCJIT_ROLE_PTR;
	case VT_FUNC:
		return MCCJIT_ROLE_FUNC;
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

	if (do_spec && param_index >= 0 && (uint32_t)param_index < it.nparam)
		mccjit_ast_spec_fold(it.arena, (int)it.param_off[param_index], const_val);

	sym = mccjit_rebuild_sym(&it);
	if (sym)
		ast_reemit_extern(sym, it.arena);
	mcc_exit_state(js);

	if (sym && mcc_relocate(js) == 0)
		entry = mcc_get_symbol(js, it.fn_name);

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

static void mccjit_boot_swap_run(void **slot, const void *blob, unsigned long len,
																 unsigned long max_duration, const char *mode,
																 const struct timespec *t0, int timed) {
	void *variant = NULL;
	void *entry = NULL;
	int over = 0;
	int skipped = 0;
	if (timed && max_duration && mccjit_elapsed(t0) > (double)max_duration) {
		skipped = 1;
	} else {
		variant = mcc_jit_recompile_blob(blob, (size_t)len);
		entry = variant ? mccjit_make_trampoline(variant) : NULL;
		if (timed && max_duration && entry &&
				mccjit_elapsed(t0) > (double)max_duration) {
			over = 1;
			entry = NULL;
		}
	}
	if (getenv("MCC_JIT_VERBOSE")) {
		int probe = variant ? ((int (*)(int))variant)(7) : -1;
		fprintf(stderr,
						"mccjit-boot[%s]: slot=%p old=%p blob=%p len=%lu variant=%p entry=%p probe(7)=%d %s\n",
						mode, (void *)slot, slot ? *slot : (void *)0, blob, len, variant, entry,
						probe,
						skipped ? "budget-skip"
										: over ? "over-budget-kept-aot" : entry ? "swapped" : "kept-aot");
	}
	if (entry)
		mcc_jit_publish(slot, entry);
}

void mccjit_boot_swap(void **slot, const void *blob, unsigned long len) {
	mccjit_boot_swap_run(slot, blob, len, 0, "sync", NULL, 0);
}

typedef struct MccjitSwapJob {
	void **slot;
	const void *blob;
	unsigned long len;
	unsigned long max_duration;
	struct timespec start;
	int timed;
} MccjitSwapJob;

static pthread_mutex_t mccjit_swap_lock = PTHREAD_MUTEX_INITIALIZER;

static void *mccjit_swap_worker(void *p) {
	MccjitSwapJob job = *(MccjitSwapJob *)p;
	mcc_free(p);
	pthread_mutex_lock(&mccjit_swap_lock);
	mccjit_boot_swap_run(job.slot, job.blob, job.len, job.max_duration, "async",
											 &job.start, job.timed);
	pthread_mutex_unlock(&mccjit_swap_lock);
	return NULL;
}

void mccjit_boot_swap_async(void **slot, const void *blob, unsigned long len,
														unsigned long max_duration) {
	MccjitSwapJob *job = mcc_malloc(sizeof *job);
	if (job) {
		pthread_t th;
		job->slot = slot;
		job->blob = blob;
		job->len = len;
		job->max_duration = max_duration;
		job->timed =
				(max_duration != 0) && (clock_gettime(CLOCK_MONOTONIC, &job->start) == 0);
		if (pthread_create(&th, NULL, mccjit_swap_worker, job) == 0) {
			pthread_detach(th);
			return;
		}
		mcc_free(job);
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
	if (!s1 || !s1->embed_jit || !mccjit_embed_fns)
		return;
	if (s1->output_type == MCC_OUTPUT_MEMORY)
		return;
	cstr_new(&cs);
	if (s1->jit_threads > 0)
		cstr_printf(&cs,
								"extern void mccjit_boot_swap_async(void**, const void*, unsigned long, unsigned long);\n");
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
		if (s1->jit_threads > 0)
			cstr_printf(&cs,
									"__attribute__((constructor)) static void __mccjit_boot_%s(void){"
									"mccjit_boot_swap_async(&__mccjit_slot_%s, __mccjit_blob_%s, %luUL, %luUL);}\n",
									e->name, e->name, e->name, (unsigned long)e->len,
									(unsigned long)s1->jit_max_duration);
		else
			cstr_printf(&cs,
									"__attribute__((constructor)) static void __mccjit_boot_%s(void){"
									"mccjit_boot_swap(&__mccjit_slot_%s, __mccjit_blob_%s, %luUL);}\n",
									e->name, e->name, e->name, (unsigned long)e->len);
	}
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

#define MCCJIT_KGC_ARITY 4
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
} MccjitKgc;

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
	if (k->memoize_ok && mccjit_kgc_contains(k, tuple))
		return (int64_t)vf((int)x);
	bval = (int64_t)bf((int)x);
	vval = (int64_t)vf((int)x);
	if (vval == bval) {
		if (k->memoize_ok)
			mccjit_kgc_insert(k, tuple);
		return bval;
	}
	if (flagged)
		*flagged = 1;
	return bval;
}

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
