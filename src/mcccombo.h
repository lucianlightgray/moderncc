#ifndef MCC_COMBO_H
#define MCC_COMBO_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef uint64_t combo_u64;

#include "algorithms/lzss.h"
#include "algorithms/lzw.h"
#include "algorithms/rle.h"

#define COMBO_MAX 16

#define COMBO_REJECT ((long)-1)
typedef long (*ComboScoreFn)(const int *sel, int k, void *user);

#define COMBO_WALK_LINEAR 0
#define COMBO_WALK_DFS 1
#define COMBO_WALK_BFS 2
#define COMBO_WALK_PRODUCT 3

static const char *combo_walk_name(int w) {
	switch (w) {
	case COMBO_WALK_LINEAR:
		return "linear";
	case COMBO_WALK_DFS:
		return "dfs";
	case COMBO_WALK_BFS:
		return "bfs";
	case COMBO_WALK_PRODUCT:
		return "product";
	default:
		return "?";
	}
}

typedef struct ComboSpec {
	int nitems;
	int min_k, max_k;
	int ordered;
	int walk;
	long budget;
	ComboScoreFn score;
	void (*visit)(const int *sel, int k, int depth, int walk, void *user);
	void *user;
} ComboSpec;

typedef struct ComboBest {
	int sel[COMBO_MAX];
	int k;
	long score;
	long evaluated;
	int exhausted;
} ComboBest;

static int combo_next_perm(int *a, int k) {
	int i = k - 2, j = k - 1, t;
	while (i >= 0 && a[i] >= a[i + 1])
		i--;
	if (i < 0)
		return 0;
	while (a[j] <= a[i])
		j--;
	t = a[i], a[i] = a[j], a[j] = t;
	for (i++, j = k - 1; i < j; i++, j--)
		t = a[i], a[i] = a[j], a[j] = t;
	return 1;
}

static int combo_emit(const ComboSpec *s, ComboBest *best, const int *members, int k,
											long *evaluated) {
	int buf[COMBO_MAX], i;
	if (k < s->min_k || k > s->max_k)
		return 1;
	for (i = 0; i < k; i++)
		buf[i] = members[i];
	do {
		long sc;
		if (s->budget && *evaluated >= s->budget) {
			best->exhausted = 0;
			return 0;
		}
		if (s->visit)
			s->visit(buf, k, k, s->walk, s->user);
		sc = s->score(buf, k, s->user);
		(*evaluated)++;
		if (sc != COMBO_REJECT && (best->k == 0 || sc < best->score)) {
			best->score = sc;
			best->k = k;
			for (i = 0; i < k; i++)
				best->sel[i] = buf[i];
		}
		if (!s->ordered)
			break;
	} while (combo_next_perm(buf, k));
	return 1;
}

static int combo_dfs(const ComboSpec *s, ComboBest *best, int *prefix, int plen,
										 int start, int n, long *evaluated) {
	int i;
	if (!combo_emit(s, best, prefix, plen, evaluated))
		return 0;
	for (i = start; i < n; i++) {
		prefix[plen] = i;
		if (!combo_dfs(s, best, prefix, plen + 1, i + 1, n, evaluated))
			return 0;
	}
	return 1;
}

static int combo_bfs(const ComboSpec *s, ComboBest *best, int n, long *evaluated) {
	int comb[COMBO_MAX], size, i, j, hi = s->max_k;
	if (hi > n)
		hi = n;
	for (size = s->min_k < 1 ? 1 : s->min_k; size <= hi; size++) {
		for (i = 0; i < size; i++)
			comb[i] = i;
		for (;;) {
			if (!combo_emit(s, best, comb, size, evaluated))
				return 0;
			i = size - 1;
			while (i >= 0 && comb[i] == n - size + i)
				i--;
			if (i < 0)
				break;
			comb[i]++;
			for (j = i + 1; j < size; j++)
				comb[j] = comb[j - 1] + 1;
		}
	}
	return 1;
}

static int combo_product_deepen(const ComboSpec *s, ComboBest *best, int *prefix,
																int plen, int start, int n, long *evaluated) {
	int i;
	for (i = start; i < n; i++) {
		prefix[plen] = i;
		if (!combo_emit(s, best, prefix, plen + 1, evaluated))
			return 0;
		if (!combo_product_deepen(s, best, prefix, plen + 1, i + 1, n, evaluated))
			return 0;
	}
	return 1;
}

static int combo_product(const ComboSpec *s, ComboBest *best, int n, long *evaluated) {
	int prefix[COMBO_MAX], i;
	for (i = 0; i < n; i++) {
		prefix[0] = i;
		if (!combo_emit(s, best, prefix, 1, evaluated))
			return 0;
	}
	for (i = 0; i < n; i++) {
		prefix[0] = i;
		if (!combo_product_deepen(s, best, prefix, 1, i + 1, n, evaluated))
			return 0;
	}
	return 1;
}

static int combo_walk_run(const ComboSpec *s, ComboBest *best) {
	int prefix[COMBO_MAX], n = s->nitems;
	long evaluated = 0;
	if (n > COMBO_MAX)
		n = COMBO_MAX;
	best->k = 0;
	best->score = 0;
	best->exhausted = 1;
	switch (s->walk) {
	case COMBO_WALK_DFS:
		combo_dfs(s, best, prefix, 0, 0, n, &evaluated);
		break;
	case COMBO_WALK_BFS:
		combo_bfs(s, best, n, &evaluated);
		break;
	case COMBO_WALK_PRODUCT:
		combo_product(s, best, n, &evaluated);
		break;
	}
	best->evaluated = evaluated;
	return best->k > 0;
}

static int combo_run(const ComboSpec *s, ComboBest *best) {
	unsigned mask, full;
	int members[COMBO_MAX], k, i, n = s->nitems;
	long evaluated = 0;
	if (s->walk != COMBO_WALK_LINEAR)
		return combo_walk_run(s, best);
	if (n > COMBO_MAX)
		n = COMBO_MAX;
	full = (n >= 31) ? 0x7fffffffu : ((1u << n) - 1u);
	best->k = 0;
	best->score = 0;
	best->exhausted = 1;
	for (mask = 1; mask <= full; mask++) {
		k = 0;
		for (i = 0; i < n; i++)
			if (mask & (1u << i))
				members[k++] = i;
		if (k < s->min_k || k > s->max_k)
			continue;
		do {
			long sc;
			if (s->budget && evaluated >= s->budget) {
				best->exhausted = 0;
				goto done;
			}
			if (s->visit)
				s->visit(members, k, k, s->walk, s->user);
			sc = s->score(members, k, s->user);
			evaluated++;
			if (sc != COMBO_REJECT && (best->k == 0 || sc < best->score)) {
				best->score = sc;
				best->k = k;
				for (i = 0; i < k; i++)
					best->sel[i] = members[i];
			}
			if (!s->ordered)
				break;
		} while (combo_next_perm(members, k));
	}
done:
	best->evaluated = evaluated;
	return best->k > 0;
}

typedef struct ComboCodec {
	const char *name;
	long (*enc)(const unsigned char *, long, unsigned char *, long);
	long (*dec)(const unsigned char *, long, unsigned char *, long);
} ComboCodec;

#define COMBO_NCODEC 3
static const ComboCodec combo_codecs[COMBO_NCODEC] = {
	{"rle", rle_compress, rle_decompress},
	{"lzss", lzss_compress, lzss_decompress},
	{"lzw", lzw_compress, lzw_decompress},
};

#define COMBO_STORED 0xff

static long combo_pack(const unsigned char *in, long n, unsigned char *out, long cap,
											 unsigned char *scratch, long scap, long *olen) {
	long best = n, l;
	int codec = COMBO_STORED, c;
	if (n > cap)
		return -1;
	memcpy(out, in, (size_t)n);
	for (c = 0; c < COMBO_NCODEC; c++) {
		l = combo_codecs[c].enc(in, n, scratch, scap);
		if (l >= 0 && l < best && l <= cap) {
			best = l;
			codec = c;
			memcpy(out, scratch, (size_t)l);
		}
	}
	*olen = best;
	return codec;
}

static long combo_unpack(int codec, const unsigned char *in, long n, unsigned char *out,
												 long cap) {
	if (codec == COMBO_STORED) {
		if (n > cap)
			return -1;
		memcpy(out, in, (size_t)n);
		return n;
	}
	if (codec < 0 || codec >= COMBO_NCODEC)
		return -1;
	return combo_codecs[codec].dec(in, n, out, cap);
}

typedef struct ComboPipeCtx {
	const unsigned char *data;
	long n;
	unsigned char *a, *b;
	long cap;
} ComboPipeCtx;

static long combo_pipe_score(const int *sel, int k, void *user) {
	ComboPipeCtx *c = (ComboPipeCtx *)user;
	const unsigned char *src = c->data;
	long slen = c->n;
	int i;
	for (i = 0; i < k; i++) {
		unsigned char *dst = (i & 1) ? c->b : c->a;
		long r = combo_codecs[sel[i]].enc(src, slen, dst, c->cap);
		if (r < 0)
			return COMBO_REJECT;
		src = dst;
		slen = r;
	}
	return slen;
}

static long combo_pipe_apply(const int *sel, int k, const unsigned char *in, long n,
														 unsigned char *a, unsigned char *b, long cap,
														 unsigned char **outp) {
	const unsigned char *src = in;
	long slen = n;
	int i;
	for (i = 0; i < k; i++) {
		unsigned char *dst = (i & 1) ? b : a;
		long r = combo_codecs[sel[i]].enc(src, slen, dst, cap);
		if (r < 0)
			return -1;
		src = dst;
		slen = r;
	}
	*outp = (k == 0) ? (unsigned char *)in : (((k - 1) & 1) ? b : a);
	return slen;
}

static long combo_pipe_unapply(const int *sel, int k, const unsigned char *comp,
															 long clen, unsigned char *a, unsigned char *b,
															 long cap, unsigned char **outp) {
	const unsigned char *src = comp;
	long slen = clen;
	int i, stage = 0;
	for (i = k - 1; i >= 0; i--, stage++) {
		unsigned char *dst = (stage & 1) ? b : a;
		long r = combo_codecs[sel[i]].dec(src, slen, dst, cap);
		if (r < 0)
			return -1;
		src = dst;
		slen = r;
	}
	*outp = (k == 0) ? (unsigned char *)comp : (((k - 1) & 1) ? b : a);
	return slen;
}

static int combo_pipeline_search(const unsigned char *data, long n, int maxdepth,
																 unsigned char *a, unsigned char *b, long cap,
																 ComboBest *best) {
	ComboPipeCtx ctx;
	ComboSpec spec;
	ctx.data = data;
	ctx.n = n;
	ctx.a = a;
	ctx.b = b;
	ctx.cap = cap;
	spec.nitems = COMBO_NCODEC;
	spec.min_k = 1;
	spec.max_k = maxdepth < 1 ? 1 : maxdepth;
	spec.ordered = 1;
	spec.walk = COMBO_WALK_LINEAR;
	spec.budget = 0;
	spec.score = combo_pipe_score;
	spec.visit = NULL;
	spec.user = &ctx;
	return combo_run(&spec, best);
}

#ifndef COMBO_MEMO_CAP
#define COMBO_MEMO_CAP 256
#endif
#ifndef COMBO_VAL_MAX
#define COMBO_VAL_MAX 4096
#endif

typedef struct ComboMemoRec {
	combo_u64 key;
	int codec;
	long rawlen, vlen;
	unsigned refcount;
	unsigned char val[COMBO_VAL_MAX];
} ComboMemoRec;

typedef struct ComboMemo {
	ComboMemoRec rec[COMBO_MEMO_CAP];
	int n;
	combo_u64 bytes;
	combo_u64 cap_bytes;
	unsigned char scratch[COMBO_VAL_MAX * 2 + 64];
} ComboMemo;

static void combo_memo_init(ComboMemo *m, combo_u64 cap_bytes) {
	m->n = 0;
	m->bytes = 0;
	m->cap_bytes = cap_bytes;
}

static combo_u64 combo_hash(const unsigned char *p, long n) {
	combo_u64 h = 1469598103934665603ULL;
	long i;
	for (i = 0; i < n; i++) {
		h ^= (combo_u64)p[i];
		h *= 1099511628211ULL;
	}
	return h;
}

static int combo_memo_find(const ComboMemo *m, combo_u64 key) {
	int i;
	for (i = 0; i < m->n; i++)
		if (m->rec[i].key == key)
			return i;
	return -1;
}

static int combo_memo_evict_one(ComboMemo *m) {
	int i, lo = -1;
	if (m->n == 0)
		return 0;
	for (i = 0; i < m->n; i++)
		if (lo < 0 || m->rec[i].refcount < m->rec[lo].refcount)
			lo = i;
	m->bytes -= (combo_u64)m->rec[lo].vlen;
	m->rec[lo] = m->rec[m->n - 1];
	m->n--;
	return 1;
}

static int combo_memo_put(ComboMemo *m, combo_u64 key, const unsigned char *val,
													long n) {
	long vlen = 0, codec;
	int idx;
	if (n > COMBO_VAL_MAX)
		return -1;
	codec = combo_pack(val, n, m->scratch + COMBO_VAL_MAX, COMBO_VAL_MAX, m->scratch,
										 COMBO_VAL_MAX, &vlen);
	if (codec < 0)
		return -1;
	idx = combo_memo_find(m, key);
	if (idx < 0) {
		while (m->n >= COMBO_MEMO_CAP && combo_memo_evict_one(m))
			;
		if (m->n >= COMBO_MEMO_CAP)
			return -1;
		idx = m->n++;
		m->rec[idx].key = key;
		m->rec[idx].refcount = 0;
	} else {
		m->bytes -= (combo_u64)m->rec[idx].vlen;
	}
	m->rec[idx].codec = (int)codec;
	m->rec[idx].rawlen = n;
	m->rec[idx].vlen = vlen;
	memcpy(m->rec[idx].val, m->scratch + COMBO_VAL_MAX, (size_t)vlen);
	m->bytes += (combo_u64)vlen;
	while (m->bytes > m->cap_bytes && m->n > 1 && combo_memo_evict_one(m))
		;
	return (int)codec;
}

static long combo_memo_get(ComboMemo *m, combo_u64 key, unsigned char *out, long cap) {
	int idx = combo_memo_find(m, key);
	long r;
	if (idx < 0)
		return -1;
	r = combo_unpack(m->rec[idx].codec, m->rec[idx].val, m->rec[idx].vlen, out, cap);
	if (r >= 0)
		m->rec[idx].refcount++;
	return r;
}

#endif
