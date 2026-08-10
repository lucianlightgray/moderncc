#ifndef MCC_EFFECT_H
#define MCC_EFFECT_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MCC_EFFECT_MALLOC
#define MCC_EFFECT_MALLOC malloc
#endif
#ifndef MCC_EFFECT_REALLOC
#define MCC_EFFECT_REALLOC realloc
#endif
#ifndef MCC_EFFECT_FREE
#define MCC_EFFECT_FREE free
#endif

enum {
	MCC_EFFECT_OFF = 0,
	MCC_EFFECT_RECORD = 1,
	MCC_EFFECT_REPLAY = 2
};

enum {
	MCC_EFFECT_NONE = 0,
	MCC_EFFECT_LOAD = 1,
	MCC_EFFECT_STORE = 2,
	MCC_EFFECT_WRITE = 3,
	MCC_EFFECT_ASM = 4
};

enum {
	MCC_EFFECT_SPACE_REGION = 0,
	MCC_EFFECT_SPACE_FD = 1,
	MCC_EFFECT_SPACE_PORT = 2,
	MCC_EFFECT_SPACE_CELL = 3
};

#define MCC_EFFECT_F_VOLATILE 1u
#define MCC_EFFECT_F_UNDEF 2u
#define MCC_EFFECT_F_NOUNDO 4u
#define MCC_EFFECT_F_REQ (MCC_EFFECT_F_VOLATILE)

enum {
	MCC_EFFECT_OK = 0,
	MCC_EFFECT_DIV_MISSING,
	MCC_EFFECT_DIV_EXTRA,
	MCC_EFFECT_DIV_KIND,
	MCC_EFFECT_DIV_SPACE,
	MCC_EFFECT_DIV_CHAN,
	MCC_EFFECT_DIV_SITE,
	MCC_EFFECT_DIV_ADDR,
	MCC_EFFECT_DIV_WIDTH,
	MCC_EFFECT_DIV_FLAGS,
	MCC_EFFECT_DIV_VALUE,
	MCC_EFFECT_DIV_PAYLOAD,
	MCC_EFFECT_DIV_ORDER,
	MCC_EFFECT_DIV_CAPACITY,
	MCC_EFFECT_DIV_COUNT
};

typedef struct MccEffect {
	uint32_t kind;
	uint32_t space;
	int32_t chan;
	uint32_t site;
	uint32_t lane;
	uint32_t seq;
	int32_t addr;
	int32_t width;
	uint32_t flags;
	uint32_t pay;
	int32_t paylen;
	int64_t value;
	int64_t prev;
} MccEffect;

typedef void (*MccEffectUndoFn)(const MccEffect *e, const void *payload,
																void *user);

typedef struct MccEffectDiv {
	int code;
	uint32_t lane;
	uint32_t seq;
	int have_want;
	int have_got;
	MccEffect want;
	MccEffect got;
} MccEffectDiv;

typedef struct MccEffectLog {
	MccEffect *e;
	int32_t *link;
	int n;
	int cap;
	unsigned char *pbuf;
	int pn;
	int pcap;
	int mode;
	int want_prev;
	int32_t *cur;
	int32_t *head;
	int32_t *tail;
	uint32_t *nrec;
	uint32_t *nrep;
	int nlane;
	long consumed;
	long recorded;
	MccEffectDiv div;
} MccEffectLog;

static int mcc_effect_output_kind(uint32_t k) { return k != MCC_EFFECT_LOAD; }

static const char *mcc_effect_kind_name(uint32_t k) {
	switch (k) {
	case MCC_EFFECT_LOAD:
		return "load";
	case MCC_EFFECT_STORE:
		return "store";
	case MCC_EFFECT_WRITE:
		return "write";
	case MCC_EFFECT_ASM:
		return "asm";
	default:
		return "none";
	}
}

static const char *mcc_effect_space_name(uint32_t s) {
	switch (s) {
	case MCC_EFFECT_SPACE_REGION:
		return "region";
	case MCC_EFFECT_SPACE_FD:
		return "fd";
	case MCC_EFFECT_SPACE_PORT:
		return "port";
	case MCC_EFFECT_SPACE_CELL:
		return "cell";
	default:
		return "?";
	}
}

static const char *mcc_effect_div_name(int c) {
	switch (c) {
	case MCC_EFFECT_OK:
		return "ok";
	case MCC_EFFECT_DIV_MISSING:
		return "missing";
	case MCC_EFFECT_DIV_EXTRA:
		return "extra";
	case MCC_EFFECT_DIV_KIND:
		return "kind";
	case MCC_EFFECT_DIV_SPACE:
		return "space";
	case MCC_EFFECT_DIV_CHAN:
		return "chan";
	case MCC_EFFECT_DIV_SITE:
		return "site";
	case MCC_EFFECT_DIV_ADDR:
		return "addr";
	case MCC_EFFECT_DIV_WIDTH:
		return "width";
	case MCC_EFFECT_DIV_FLAGS:
		return "flags";
	case MCC_EFFECT_DIV_VALUE:
		return "value";
	case MCC_EFFECT_DIV_PAYLOAD:
		return "payload";
	case MCC_EFFECT_DIV_ORDER:
		return "order";
	case MCC_EFFECT_DIV_CAPACITY:
		return "capacity";
	default:
		return "?";
	}
}

static void mcc_effect_log_init(MccEffectLog *L) {
	if (!L)
		return;
	memset(L, 0, sizeof *L);
}

static void mcc_effect_log_free(MccEffectLog *L) {
	if (!L)
		return;
	MCC_EFFECT_FREE(L->e);
	MCC_EFFECT_FREE(L->link);
	MCC_EFFECT_FREE(L->pbuf);
	MCC_EFFECT_FREE(L->cur);
	MCC_EFFECT_FREE(L->head);
	MCC_EFFECT_FREE(L->tail);
	MCC_EFFECT_FREE(L->nrec);
	MCC_EFFECT_FREE(L->nrep);
	memset(L, 0, sizeof *L);
}

static void mcc_effect_log_clear(MccEffectLog *L) {
	int i;
	if (!L)
		return;
	L->n = 0;
	L->pn = 0;
	L->consumed = 0;
	L->recorded = 0;
	memset(&L->div, 0, sizeof L->div);
	for (i = 0; i < L->nlane; i++) {
		L->cur[i] = -1;
		L->head[i] = -1;
		L->tail[i] = -1;
		L->nrec[i] = 0;
		L->nrep[i] = 0;
	}
}

static int mcc_effect_lanes(MccEffectLog *L, uint32_t lane) {
	int want, i, old;
	void *p;
	if (!L)
		return 0;
	if ((int64_t)lane < (int64_t)L->nlane)
		return 1;
	if (lane > 0x00ffffffu)
		return 0;
	old = L->nlane;
	want = old ? old : 8;
	while ((uint32_t)want <= lane)
		want *= 2;
	p = MCC_EFFECT_REALLOC(L->cur, (size_t)want * sizeof *L->cur);
	if (!p)
		return 0;
	L->cur = (int32_t *)p;
	p = MCC_EFFECT_REALLOC(L->head, (size_t)want * sizeof *L->head);
	if (!p)
		return 0;
	L->head = (int32_t *)p;
	p = MCC_EFFECT_REALLOC(L->tail, (size_t)want * sizeof *L->tail);
	if (!p)
		return 0;
	L->tail = (int32_t *)p;
	p = MCC_EFFECT_REALLOC(L->nrec, (size_t)want * sizeof *L->nrec);
	if (!p)
		return 0;
	L->nrec = (uint32_t *)p;
	p = MCC_EFFECT_REALLOC(L->nrep, (size_t)want * sizeof *L->nrep);
	if (!p)
		return 0;
	L->nrep = (uint32_t *)p;
	for (i = old; i < want; i++) {
		L->cur[i] = -1;
		L->head[i] = -1;
		L->tail[i] = -1;
		L->nrec[i] = 0;
		L->nrep[i] = 0;
	}
	L->nlane = want;
	return 1;
}

static int mcc_effect_room(MccEffectLog *L) {
	int want;
	void *p;
	if (L->n < L->cap)
		return 1;
	want = L->cap ? L->cap * 2 : 64;
	if (want > (1 << 26))
		return 0;
	p = MCC_EFFECT_REALLOC(L->e, (size_t)want * sizeof *L->e);
	if (!p)
		return 0;
	L->e = (MccEffect *)p;
	p = MCC_EFFECT_REALLOC(L->link, (size_t)want * sizeof *L->link);
	if (!p)
		return 0;
	L->link = (int32_t *)p;
	L->cap = want;
	return 1;
}

static int mcc_effect_payroom(MccEffectLog *L, int add) {
	int want;
	void *p;
	if (add <= 0)
		return 1;
	if (L->pn + add <= L->pcap)
		return 1;
	want = L->pcap ? L->pcap : 256;
	while (want < L->pn + add) {
		if (want > (1 << 28))
			return 0;
		want *= 2;
	}
	p = MCC_EFFECT_REALLOC(L->pbuf, (size_t)want);
	if (!p)
		return 0;
	L->pbuf = (unsigned char *)p;
	L->pcap = want;
	return 1;
}

static const unsigned char *mcc_effect_payload(const MccEffectLog *L,
																							 const MccEffect *e) {
	if (!L || !e || e->paylen <= 0)
		return NULL;
	if ((int64_t)e->pay + e->paylen > (int64_t)L->pn)
		return NULL;
	return L->pbuf + e->pay;
}

static void mcc_effect_fail(MccEffectLog *L, int code, const MccEffect *got,
														const MccEffect *want) {
	if (!L || L->div.code)
		return;
	L->div.code = code;
	L->div.lane = got ? got->lane : (want ? want->lane : 0);
	L->div.seq = want ? want->seq : (got ? got->seq : 0);
	if (got) {
		L->div.got = *got;
		L->div.have_got = 1;
	}
	if (want) {
		L->div.want = *want;
		L->div.have_want = 1;
	}
}

static int mcc_effect_record(MccEffectLog *L, MccEffect *e,
														 const void *payload, int paylen) {
	int32_t i;
	if (!L || !e)
		return 0;
	if (paylen < 0)
		paylen = 0;
	if (!mcc_effect_lanes(L, e->lane) || !mcc_effect_room(L) ||
			!mcc_effect_payroom(L, paylen)) {
		mcc_effect_fail(L, MCC_EFFECT_DIV_CAPACITY, e, NULL);
		return 0;
	}
	i = (int32_t)L->n;
	e->seq = L->nrec[e->lane]++;
	e->pay = (uint32_t)L->pn;
	e->paylen = paylen;
	if (paylen && payload) {
		memcpy(L->pbuf + L->pn, payload, (size_t)paylen);
		L->pn += paylen;
	} else {
		e->paylen = 0;
	}
	L->e[i] = *e;
	L->link[i] = -1;
	if (L->tail[e->lane] >= 0)
		L->link[L->tail[e->lane]] = i;
	else
		L->head[e->lane] = i;
	L->tail[e->lane] = i;
	L->n++;
	L->recorded++;
	return 1;
}

static void mcc_effect_relink(MccEffectLog *L) {
	int i;
	if (!L)
		return;
	for (i = 0; i < L->nlane; i++) {
		L->head[i] = -1;
		L->tail[i] = -1;
		L->cur[i] = -1;
		L->nrep[i] = 0;
	}
	for (i = 0; i < L->n; i++) {
		uint32_t ln = L->e[i].lane;
		if (!mcc_effect_lanes(L, ln))
			continue;
		L->link[i] = -1;
		if (L->tail[ln] >= 0)
			L->link[L->tail[ln]] = i;
		else
			L->head[ln] = i;
		L->tail[ln] = i;
	}
	for (i = 0; i < L->nlane; i++)
		L->cur[i] = L->head[i];
}

static void mcc_effect_replay_begin(MccEffectLog *L) {
	if (!L)
		return;
	memset(&L->div, 0, sizeof L->div);
	L->consumed = 0;
	mcc_effect_relink(L);
	L->mode = MCC_EFFECT_REPLAY;
}

static int mcc_effect_replay(MccEffectLog *L, MccEffect *req,
														 const void *payload, int paylen) {
	int32_t i;
	MccEffect *e;
	if (!L || !req)
		return 0;
	if (L->div.code)
		return 0;
	if ((int64_t)req->lane >= (int64_t)L->nlane || L->cur[req->lane] < 0) {
		mcc_effect_fail(L, MCC_EFFECT_DIV_MISSING, req, NULL);
		return 0;
	}
	i = L->cur[req->lane];
	e = &L->e[i];
	if (e->kind != req->kind) {
		mcc_effect_fail(L, MCC_EFFECT_DIV_KIND, req, e);
		return 0;
	}
	if (e->space != req->space) {
		mcc_effect_fail(L, MCC_EFFECT_DIV_SPACE, req, e);
		return 0;
	}
	if (e->chan != req->chan) {
		mcc_effect_fail(L, MCC_EFFECT_DIV_CHAN, req, e);
		return 0;
	}
	if (e->site != req->site) {
		mcc_effect_fail(L, MCC_EFFECT_DIV_SITE, req, e);
		return 0;
	}
	if (e->addr != req->addr) {
		mcc_effect_fail(L, MCC_EFFECT_DIV_ADDR, req, e);
		return 0;
	}
	if (e->width != req->width) {
		mcc_effect_fail(L, MCC_EFFECT_DIV_WIDTH, req, e);
		return 0;
	}
	if ((e->flags & MCC_EFFECT_F_REQ) != (req->flags & MCC_EFFECT_F_REQ)) {
		mcc_effect_fail(L, MCC_EFFECT_DIV_FLAGS, req, e);
		return 0;
	}
	if (mcc_effect_output_kind(req->kind)) {
		const unsigned char *p = mcc_effect_payload(L, e);
		if (e->paylen != (paylen < 0 ? 0 : paylen) ||
				(e->paylen && (!p || !payload ||
											 memcmp(p, payload, (size_t)e->paylen) != 0))) {
			mcc_effect_fail(L, MCC_EFFECT_DIV_PAYLOAD, req, e);
			return 0;
		}
		if (!e->paylen && e->value != req->value) {
			mcc_effect_fail(L, MCC_EFFECT_DIV_VALUE, req, e);
			return 0;
		}
	}
	if (e->seq != L->nrep[req->lane]) {
		mcc_effect_fail(L, MCC_EFFECT_DIV_ORDER, req, e);
		return 0;
	}
	req->seq = e->seq;
	req->value = e->value;
	req->flags = e->flags;
	req->prev = e->prev;
	req->pay = e->pay;
	req->paylen = e->paylen;
	L->cur[req->lane] = L->link[i];
	L->nrep[req->lane]++;
	L->consumed++;
	return 1;
}

static int mcc_effect_replay_end(MccEffectLog *L) {
	int i;
	if (!L)
		return 0;
	L->mode = MCC_EFFECT_OFF;
	if (L->div.code)
		return 0;
	for (i = 0; i < L->nlane; i++)
		if (L->cur[i] >= 0) {
			mcc_effect_fail(L, MCC_EFFECT_DIV_EXTRA, NULL, &L->e[L->cur[i]]);
			return 0;
		}
	return 1;
}

static int mcc_effect_diverged(const MccEffectLog *L) {
	return L && L->div.code != MCC_EFFECT_OK;
}

static int mcc_effect_str(const MccEffect *e, char *b, size_t n) {
	if (!b || !n)
		return 0;
	if (!e) {
		snprintf(b, n, "(none)");
		return 0;
	}
	if (e->paylen > 0)
		return snprintf(b, n,
										"lane=%lu seq=%lu %s %s:%ld off=%ld len=%ld flags=%lu",
										(unsigned long)e->lane, (unsigned long)e->seq,
										mcc_effect_kind_name(e->kind),
										mcc_effect_space_name(e->space), (long)e->chan,
										(long)e->addr, (long)e->paylen,
										(unsigned long)e->flags);
	if (e->kind == MCC_EFFECT_STORE)
		return snprintf(b, n,
										"lane=%lu seq=%lu store site=%lu addr=%ld w=%ld "
										"flags=%lu val=%lld prev=%lld",
										(unsigned long)e->lane, (unsigned long)e->seq,
										(unsigned long)e->site, (long)e->addr, (long)e->width,
										(unsigned long)e->flags, (long long)e->value,
										(long long)e->prev);
	return snprintf(b, n,
									"lane=%lu seq=%lu %s site=%lu addr=%ld w=%ld flags=%lu "
									"val=%lld",
									(unsigned long)e->lane, (unsigned long)e->seq,
									mcc_effect_kind_name(e->kind), (unsigned long)e->site,
									(long)e->addr, (long)e->width, (unsigned long)e->flags,
									(long long)e->value);
}

static int mcc_effect_div_str(const MccEffectLog *L, char *b, size_t n) {
	char w[160], g[160];
	if (!b || !n)
		return 0;
	if (!L || !L->div.code) {
		snprintf(b, n, "no divergence");
		return 0;
	}
	mcc_effect_str(L->div.have_want ? &L->div.want : NULL, w, sizeof w);
	mcc_effect_str(L->div.have_got ? &L->div.got : NULL, g, sizeof g);
	return snprintf(b, n,
									"effect divergence (%s) at lane=%lu seq=%lu: executor "
									"performed [%s], replay asked for [%s]",
									mcc_effect_div_name(L->div.code),
									(unsigned long)L->div.lane, (unsigned long)L->div.seq, w, g);
}

static void mcc_effect_dump(const MccEffectLog *L, FILE *f, int max) {
	char b[160];
	int i;
	if (!L || !f)
		return;
	for (i = 0; i < L->n && (max <= 0 || i < max); i++) {
		mcc_effect_str(&L->e[i], b, sizeof b);
		fprintf(f, "  effect[%d] %s\n", i, b);
	}
}

static uint64_t mcc_effect_hash(const MccEffectLog *L) {
	uint64_t h = 1469598103934665603ull;
	int i, k;
	if (!L)
		return 0;
	for (i = 0; i < L->n; i++) {
		const MccEffect *e = &L->e[i];
		const unsigned char *p = mcc_effect_payload(L, e);
		uint64_t f[10];
		int j;
		f[0] = e->kind;
		f[1] = e->space;
		f[2] = (uint64_t)(uint32_t)e->chan;
		f[3] = e->site;
		f[4] = e->lane;
		f[5] = e->seq;
		f[6] = (uint64_t)(uint32_t)e->addr;
		f[7] = (uint64_t)(uint32_t)e->width;
		f[8] = (uint64_t)e->value ^ ((uint64_t)e->flags << 32);
		f[9] = (uint64_t)e->prev ^ ((uint64_t)(uint32_t)e->paylen << 32);
		for (k = 0; k < 10; k++) {
			int s;
			for (s = 0; s < 64; s += 8) {
				h ^= (f[k] >> s) & 0xffu;
				h *= 1099511628211ull;
			}
		}
		for (j = 0; p && j < e->paylen; j++) {
			h ^= p[j];
			h *= 1099511628211ull;
		}
	}
	return h;
}

static int mcc_effect_mark(const MccEffectLog *L) { return L ? L->n : 0; }

static int mcc_effect_rewindable(const MccEffectLog *L, int mark) {
	int i;
	if (!L)
		return 1;
	if (mark < 0)
		mark = 0;
	for (i = mark; i < L->n; i++)
		if (mcc_effect_output_kind(L->e[i].kind) &&
				(L->e[i].flags & MCC_EFFECT_F_NOUNDO))
			return 0;
	return 1;
}

static long mcc_effect_undo_to(const MccEffectLog *L, int mark,
															 MccEffectUndoFn fn, void *user) {
	long done = 0;
	int i;
	if (!L || !fn)
		return 0;
	if (mark < 0)
		mark = 0;
	for (i = L->n - 1; i >= mark; i--) {
		const MccEffect *e = &L->e[i];
		if (!mcc_effect_output_kind(e->kind))
			continue;
		if (e->flags & MCC_EFFECT_F_NOUNDO)
			continue;
		fn(e, mcc_effect_payload(L, e), user);
		done++;
	}
	return done;
}

static long mcc_effect_undo(const MccEffectLog *L, MccEffectUndoFn fn,
														void *user) {
	return mcc_effect_undo_to(L, 0, fn, user);
}

#define MCC_EFFECT_MAXLIVE 8

enum {
	MCC_PROV_UNKNOWN = 0,
	MCC_PROV_LITERAL = 1,
	MCC_PROV_COMPUTED = 2,
	MCC_PROV_LOAD = 3,
	MCC_PROV_PARAM = 4,
	MCC_PROV_ANON = 5
};

typedef struct MccEffectKey {
	uint64_t slice;
	int32_t off[MCC_EFFECT_MAXLIVE];
	unsigned char prov[MCC_EFFECT_MAXLIVE];
	int nlive;
	int nopaque;
	int sealed;
	uint64_t loghash;
	long nrec;
} MccEffectKey;

static const char *mcc_effect_prov_name(int p) {
	switch (p) {
	case MCC_PROV_LITERAL:
		return "literal";
	case MCC_PROV_COMPUTED:
		return "computed";
	case MCC_PROV_LOAD:
		return "load";
	case MCC_PROV_PARAM:
		return "param";
	case MCC_PROV_ANON:
		return "anonymous-call";
	default:
		return "unknown";
	}
}

static int mcc_effect_prov_opaque(int p) {
	return p == MCC_PROV_UNKNOWN || p == MCC_PROV_ANON;
}

static void mcc_effect_key_init(MccEffectKey *k, uint64_t slice) {
	if (!k)
		return;
	memset(k, 0, sizeof *k);
	k->slice = slice;
}

static int mcc_effect_key_live(MccEffectKey *k, int32_t off, int prov) {
	if (!k || k->nlive >= MCC_EFFECT_MAXLIVE)
		return 0;
	k->off[k->nlive] = off;
	k->prov[k->nlive] = (unsigned char)prov;
	k->nlive++;
	if (mcc_effect_prov_opaque(prov))
		k->nopaque++;
	return 1;
}

static int mcc_effect_traceable(const MccEffectKey *k) {
	return k && k->nlive > 0 && k->nopaque == 0;
}

static int mcc_effect_effectful(const MccEffectKey *k) {
	return k && k->nrec != 0;
}

static int mcc_effect_memoisable(const MccEffectKey *k) {
	return mcc_effect_traceable(k) && k->sealed && !mcc_effect_effectful(k);
}

static int mcc_effect_key_seal(MccEffectKey *k, const MccEffectLog *L) {
	if (!k)
		return 0;
	k->loghash = mcc_effect_hash(L);
	k->nrec = L ? (long)L->n : 0;
	k->sealed = 1;
	return 1;
}

static int mcc_effect_key_match(const MccEffectKey *k, const MccEffectLog *L) {
	if (!k || !k->sealed)
		return 0;
	if (k->nrec != (L ? (long)L->n : 0))
		return 0;
	return k->loghash == mcc_effect_hash(L);
}

static int mcc_effect_key_eq(const MccEffectKey *a, const MccEffectKey *b) {
	int i;
	if (!a || !b)
		return 0;
	if (a->slice != b->slice || a->nlive != b->nlive)
		return 0;
	for (i = 0; i < a->nlive; i++)
		if (a->off[i] != b->off[i] || a->prov[i] != b->prov[i])
			return 0;
	return 1;
}

static uint64_t mcc_effect_key_hash(const MccEffectKey *k) {
	uint64_t h = 1469598103934665603ull;
	int i, s;
	if (!k)
		return 0;
	for (s = 0; s < 64; s += 8) {
		h ^= (k->slice >> s) & 0xffu;
		h *= 1099511628211ull;
	}
	for (i = 0; i < k->nlive; i++) {
		uint32_t u = (uint32_t)k->off[i];
		for (s = 0; s < 32; s += 8) {
			h ^= (u >> s) & 0xffu;
			h *= 1099511628211ull;
		}
		h ^= k->prov[i];
		h *= 1099511628211ull;
	}
	return h;
}

#define MCC_EFFECT_PERTURB_COUNT 14

static int mcc_effect_perturb_pair(const MccEffectLog *L, int *pa, int *pb) {
	int i, j;
	for (i = 0; i < L->n; i++)
		for (j = i + 1; j < L->n; j++)
			if (L->e[i].lane == L->e[j].lane) {
				*pa = i;
				*pb = j;
				return 1;
			}
	return 0;
}

static int mcc_effect_perturb_kind(const MccEffectLog *L, uint32_t kind) {
	int i;
	for (i = 0; i < L->n; i++)
		if (L->e[i].kind == kind)
			return i;
	return -1;
}

static int mcc_effect_perturb_payload(const MccEffectLog *L) {
	int i;
	for (i = 0; i < L->n; i++)
		if (L->e[i].paylen > 0)
			return i;
	return -1;
}

static int mcc_effect_perturb(MccEffectLog *L, int which, char *what,
															size_t nwhat) {
	int k, a, b;
	if (!L || L->n < 1)
		return 0;
	k = L->n / 2;
	switch (which) {
	case 0:
		L->e[k].addr += 4;
		snprintf(what, nwhat, "entry %d moved to a different address", k);
		return 1;
	case 1: {
		int i = mcc_effect_perturb_kind(L, MCC_EFFECT_LOAD);
		if (i < 0)
			return 0;
		L->e[i].value ^= 1;
		snprintf(what, nwhat, "entry %d observed one bit differently", i);
		return 1;
	}
	case 2:
		L->e[k].kind =
				L->e[k].kind == MCC_EFFECT_LOAD ? MCC_EFFECT_STORE : MCC_EFFECT_LOAD;
		snprintf(what, nwhat, "entry %d changed kind", k);
		return 1;
	case 3:
		L->e[k].site += 1;
		snprintf(what, nwhat, "entry %d attributed to a different site", k);
		return 1;
	case 4:
		L->e[k].width = L->e[k].width == 8 ? 4 : 8;
		snprintf(what, nwhat, "entry %d changed width", k);
		return 1;
	case 5:
		memmove(&L->e[k], &L->e[k + 1], (size_t)(L->n - k - 1) * sizeof *L->e);
		L->n--;
		snprintf(what, nwhat, "entry %d dropped", k);
		return 1;
	case 6:
		if (!mcc_effect_room(L))
			return 0;
		memmove(&L->e[k + 1], &L->e[k], (size_t)(L->n - k) * sizeof *L->e);
		L->n++;
		snprintf(what, nwhat, "entry %d duplicated", k);
		return 1;
	case 7: {
		MccEffect t;
		if (!mcc_effect_perturb_pair(L, &a, &b))
			return 0;
		t = L->e[a];
		L->e[a] = L->e[b];
		L->e[b] = t;
		snprintf(what, nwhat, "entries %d and %d of one lane reordered", a, b);
		return 1;
	}
	case 8:
		L->e[k].lane += 1;
		snprintf(what, nwhat, "entry %d reattributed to another lane", k);
		return 1;
	case 9:
		L->n--;
		snprintf(what, nwhat, "the last entry truncated");
		return 1;
	case 10: {
		int i = mcc_effect_perturb_kind(L, MCC_EFFECT_STORE);
		if (i < 0)
			return 0;
		L->e[i].prev ^= 1;
		snprintf(what, nwhat, "entry %d remembers a different prior value", i);
		return 1;
	}
	case 11:
		L->e[k].chan += 1;
		snprintf(what, nwhat, "entry %d moved to a different channel", k);
		return 1;
	case 12: {
		int i = mcc_effect_perturb_payload(L);
		if (i < 0 || !L->pbuf)
			return 0;
		L->pbuf[L->e[i].pay] ^= 1;
		snprintf(what, nwhat, "entry %d carries one perturbed payload byte", i);
		return 1;
	}
	case 13:
		L->e[k].space += 1;
		snprintf(what, nwhat, "entry %d moved to a different address space", k);
		return 1;
	default:
		return 0;
	}
}

static int mcc_effect_shuffle_lanes(MccEffectLog *L) {
	MccEffect *t;
	int i, w = 0, changed = 0;
	uint32_t ln;
	if (!L || L->n < 2)
		return 0;
	t = (MccEffect *)MCC_EFFECT_MALLOC((size_t)L->n * sizeof *t);
	if (!t)
		return 0;
	for (ln = (uint32_t)L->nlane; ln-- > 0;)
		for (i = 0; i < L->n; i++)
			if (L->e[i].lane == ln)
				t[w++] = L->e[i];
	if (w == L->n) {
		for (i = 0; i < L->n; i++)
			if (t[i].lane != L->e[i].lane || t[i].seq != L->e[i].seq) {
				changed = 1;
				break;
			}
		memcpy(L->e, t, (size_t)L->n * sizeof *t);
	}
	MCC_EFFECT_FREE(t);
	return changed;
}

#endif
