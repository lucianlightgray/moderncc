#ifndef MCC_COMBO_H
#define MCC_COMBO_H

/*
 * mcccombo — a wrapper around the recurring pattern in mcc's optimizer: "try
 * permutations of formulaic combinations, score each, keep the best, and memoize
 * the winner by a key->value cache." Three concerns the search and disk memo
 * currently hand-roll are factored here into one reusable, header-only module:
 *
 *   1. combo_run   — enumerate combinations (subsets) x permutations (orderings) of
 *                    a set of pluggable "formulas" (fold-gate strategies, forecast
 *                    predictors, compression codecs, ...), scoring each candidate
 *                    through a caller callback and keeping the lowest score.
 *   2. codecs      — the src/algorithms compressors as a first-class formula family:
 *                    they can BE the thing being permuted (combo_pipeline_search
 *                    finds the best codec chain), AND they compress cached values.
 *   3. ComboMemo   — a key->value cache whose values are stored best-of-3 compressed
 *                    with refcount + LFU size-cap eviction. A hit decompresses and
 *                    returns the value instead of recomputing it: the compression
 *                    algorithms and the cache-hit key->value optimization are the
 *                    same mechanism, which is exactly why they live together here.
 *
 * The AST fold search (ast_strategies subset lattice) and the -O4 disk memo are two
 * instantiations of this concept; this wrapper is the substrate they can be
 * refactored onto. No libc beyond memcpy, no heap: callers own all buffers.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef uint64_t combo_u64;

#include "algorithms/lzss.h"
#include "algorithms/lzw.h"
#include "algorithms/rle.h"

/* ------------------------------------------------------------------ formulas */

#define COMBO_MAX 16 /* subset enumeration is 2^n; cap matches the AST search */

/* Score a candidate. `sel` is the length-`k` ordered list of formula indices in
 * application order; return a score (lower is better) or COMBO_REJECT to discard. */
#define COMBO_REJECT ((long)-1)
typedef long (*ComboScoreFn)(const int *sel, int k, void *user);

typedef struct ComboSpec {
	int nitems;         /* number of formulas to choose from (<= COMBO_MAX) */
	int min_k, max_k;   /* inclusive bounds on selection size */
	int ordered;        /* 0: combinations only; 1: also every ordering (permutations) */
	long budget;        /* max candidates to score (0 = whole space) */
	ComboScoreFn score;
	void *user;
} ComboSpec;

typedef struct ComboBest {
	int sel[COMBO_MAX];
	int k;
	long score;
	long evaluated;     /* candidates actually scored */
	int exhausted;      /* 1 iff the whole space fit within budget */
} ComboBest;

/* Lexicographic next permutation of the k ints in a[]; 0 when a[] is the last one. */
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

/* Enumerate combinations x (optionally) permutations, scoring each; fills `best`.
 * Returns 1 if any candidate was accepted. */
static int combo_run(const ComboSpec *s, ComboBest *best) {
	unsigned mask, full;
	int members[COMBO_MAX], k, i, n = s->nitems;
	long evaluated = 0;
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
				members[k++] = i; /* ascending, so the first ordering is canonical */
		if (k < s->min_k || k > s->max_k)
			continue;
		do {
			long sc;
			if (s->budget && evaluated >= s->budget) {
				best->exhausted = 0;
				goto done;
			}
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

/* -------------------------------------------------------------------- codecs */

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

#define COMBO_STORED 0xff /* value kept verbatim: no codec beat it */

/* Best-of-3 single-shot compress of in[0..n) into out[0..cap). Returns the codec id
 * (0..COMBO_NCODEC-1) or COMBO_STORED, and sets *olen. -1 if it will not fit. */
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

/* ------------------------------------------------- codec-pipeline (perm*comb) */

typedef struct ComboPipeCtx {
	const unsigned char *data;
	long n;
	unsigned char *a, *b; /* two ping-pong scratch buffers, each of `cap` bytes */
	long cap;
} ComboPipeCtx;

/* Score a codec chain by the size it compresses ctx->data to (rejecting overflow). */
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

/* Run a codec chain forward; sets *outp to the buffer holding the result. */
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

/* Reverse a codec chain (decoders in reverse order); sets *outp to the result. */
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

/* Find the codec chain (up to `maxdepth` stages) that compresses data smallest. */
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
	spec.ordered = 1; /* order matters: rle-then-lzw != lzw-then-rle */
	spec.budget = 0;
	spec.score = combo_pipe_score;
	spec.user = &ctx;
	return combo_run(&spec, best);
}

/* --------------------------------------------------- key->value compressed memo */

#ifndef COMBO_MEMO_CAP
#define COMBO_MEMO_CAP 256
#endif
#ifndef COMBO_VAL_MAX
#define COMBO_VAL_MAX 4096
#endif

typedef struct ComboMemoRec {
	combo_u64 key;
	int codec;         /* how val is stored: 0..COMBO_NCODEC-1 or COMBO_STORED */
	long rawlen, vlen; /* decompressed length, stored (compressed) length */
	unsigned refcount;
	unsigned char val[COMBO_VAL_MAX];
} ComboMemoRec;

typedef struct ComboMemo {
	ComboMemoRec rec[COMBO_MEMO_CAP];
	int n;
	combo_u64 bytes;    /* sum of stored vlen — the quantity the cap bounds */
	combo_u64 cap_bytes;
	unsigned char scratch[COMBO_VAL_MAX * 2 + 64];
} ComboMemo;

static void combo_memo_init(ComboMemo *m, combo_u64 cap_bytes) {
	m->n = 0;
	m->bytes = 0;
	m->cap_bytes = cap_bytes;
}

/* 64-bit FNV-1a — a convenience key hash for callers with raw-byte keys. */
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

/* Drop the lowest-refcount record (LFU); returns 0 if none to drop. */
static int combo_memo_evict_one(ComboMemo *m) {
	int i, lo = -1;
	if (m->n == 0)
		return 0;
	for (i = 0; i < m->n; i++)
		if (lo < 0 || m->rec[i].refcount < m->rec[lo].refcount)
			lo = i;
	m->bytes -= (combo_u64)m->rec[lo].vlen;
	m->rec[lo] = m->rec[m->n - 1]; /* swap-remove */
	m->n--;
	return 1;
}

/* Insert/refresh key -> val (compressed best-of-3). Evicts by LFU until the stored
 * bytes are under cap_bytes. Returns the codec used, or -1 if val cannot fit. */
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

/* Cache hit: decompress key's value into out; bumps refcount. Returns the raw length,
 * or -1 on miss / undersized out. */
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
