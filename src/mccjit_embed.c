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
#define MCCJIT_INTENT_FORMAT 1u

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
		mccjit_put_u64(buf, handles.raw[k]);
		mccjit_put_u64(buf, (uint64_t)handles.token_v[k]);
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
} MccjitIntent;

MCCJIT_LOCAL void mccjit_intent_release(MccjitIntent *it) {
	if (!it)
		return;
	if (it->arena)
		ast_arena_free(it->arena);
	mcc_free(it->handle_raw);
	mcc_free(it->handle_token_v);
	it->arena = NULL;
	it->handle_raw = NULL;
	it->handle_token_v = NULL;
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
		if (!out->handle_raw || !out->handle_token_v)
			goto done;
	}
	out->handle_count = hc;
	for (i = 0; i < hc; i++) {
		uint64_t raw = mccjit_get_u64(&r);
		int64_t tv = (int64_t)mccjit_get_u64(&r);
		out->handle_raw[i] = raw;
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

MCCJIT_LOCAL void *mcc_jit_recompile(Sym *sym, const void *ctxkey) {
	MCCState *js;
	void *entry = NULL;
	(void)sym;
	(void)ctxkey;

	js = mcc_new();
	if (!js)
		return NULL;
	mcc_set_output_type(js, MCC_OUTPUT_MEMORY);

	mcc_delete(js);
	return entry;
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
