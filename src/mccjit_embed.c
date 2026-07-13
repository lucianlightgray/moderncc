#ifdef MCC_EMBED_JIT

#include "mcc.h"
#include "mccast.h"
#include "mccgate.h"
#include "algorithms/jit.h"

#if defined(__GNUC__) || defined(__clang__)
#define MCCJIT_LOCAL __attribute__((visibility("hidden")))
#else
#define MCCJIT_LOCAL
#endif

#define MCCJIT_INTENT_MAGIC 0x314a434dul
#define MCCJIT_INTENT_FORMAT 2u

void ast_reemit_extern(Sym *sym, AstArena *ast);
int mccjit_ast_spec_fold(AstArena *ast, int off, int64_t val);
void mcc_jit_publish(void **slot, void *variant);

MCCJIT_LOCAL unsigned char *mccjit_last_blob;
MCCJIT_LOCAL size_t mccjit_last_len;
MCCJIT_LOCAL MCCState *mccjit_last_state;

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
	uint32_t count;
	uint32_t cap;
	int oom;
} MccjitHandles;

static void mccjit_handles_init(MccjitHandles *h) {
	h->raw = NULL;
	h->token_v = NULL;
	h->count = 0;
	h->cap = 0;
	h->oom = 0;
}
static void mccjit_handles_free(MccjitHandles *h) {
	mcc_free(h->raw);
	mcc_free(h->token_v);
	mccjit_handles_init(h);
}

static uint32_t mccjit_handles_intern(MccjitHandles *h, uint64_t raw) {
	uint32_t i;
	if (raw == 0)
		return 0;
	for (i = 0; i < h->count; i++)
		if (h->raw[i] == raw)
			return i + 1;
	if (h->count == h->cap) {
		uint32_t ncap = h->cap ? h->cap * 2 : 16;
		uint64_t *nr = mcc_realloc(h->raw, ncap * sizeof *nr);
		int64_t *nv = mcc_realloc(h->token_v, ncap * sizeof *nv);
		if (nr)
			h->raw = nr;
		if (nv)
			h->token_v = nv;
		if (!nr || !nv) {
			h->oom = 1;
			return 0;
		}
		h->cap = ncap;
	}
	h->raw[h->count] = raw;
	h->token_v[h->count] = (int64_t)(intptr_t)((Sym *)(uintptr_t)raw)->v;
	h->count++;
	return h->count;
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
		mccjit_handles_intern(&handles, ast_sym(a, n));
		mccjit_handles_intern(&handles, ast_type_ref(a, n));
	}
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
	}

	for (n = 0; n < count; n++) {
		uint32_t nc = ast_nchild(a, n), i;
		mccjit_put_u16(buf, ast_kind(a, n));
		mccjit_put_u32(buf, (uint32_t)ast_op(a, n));
		mccjit_put_u32(buf, (uint32_t)ast_type_t(a, n));
		mccjit_put_u64(buf, ast_ival(a, n));
		mccjit_put_u64(buf, ast_fbits(a, n));
		mccjit_put_u32(buf, mccjit_handles_intern(&handles, ast_sym(a, n)));
		mccjit_put_u32(buf, mccjit_handles_intern(&handles, ast_type_ref(a, n)));
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

typedef struct MccjitIntent {
	AstArena *arena;
	uint64_t salt;
	int64_t anchor_sym_v;
	uint32_t handle_count;
	uint64_t *handle_raw;
	int64_t *handle_token_v;
	char **handle_name;
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
	it->fn_name = NULL;
	it->param_type_t = NULL;
	it->param_off = NULL;
	it->param_name = NULL;
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
		if (!out->handle_raw || !out->handle_token_v || !out->handle_name)
			goto done;
	}
	out->handle_count = hc;
	for (i = 0; i < hc; i++) {
		uint64_t raw = mccjit_get_u64(&r);
		int64_t tv = (int64_t)mccjit_get_u64(&r);
		char *nm = mccjit_get_str(&r);
		if (r.err)
			goto done;
		out->handle_raw[i] = raw;
		out->handle_name[i] = nm;
		if (nm && nm[0] && mcc_state)
			tv = tok_alloc(nm, (int)strlen(nm))->tok;
		out->handle_token_v[i] = tv;
	}
	if (r.err)
		goto done;

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
		uint64_t sym_raw = (sym_id && sym_id <= hc) ? out->handle_raw[sym_id - 1] : 0;
		uint64_t tref_raw =
				(typeref_id && typeref_id <= hc) ? out->handle_raw[typeref_id - 1] : 0;
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
