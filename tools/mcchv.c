#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include "libmcc.h"

#if defined(__STDC_NO_THREADS__) || \
		(defined(__has_include) && !__has_include(<threads.h>))
int main(void) {
	printf("mcchv: C11 threads unavailable; skipping\n");
	return 77;
}
#else

#include <threads.h>
#include <stdatomic.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

#define HV_BLOCK 64
#define HV_NBLOCKS (1u << 16)
#define HV_MAXPAT 16384
#define HV_HASHCAP (HV_MAXPAT * 4)
#define HV_TOPK 12
#define HV_PLANTED 24

typedef struct {
	uintmax_t weight;
	uint64_t hash;
	uint32_t id;
} HvPat;

typedef uint32_t (*HvKernel)(const unsigned char *blk, uint64_t h);

static unsigned char *hv_data;
static unsigned char *hv_store;
static uint32_t hv_npat;
static HvPat hv_perm[HV_MAXPAT];
static uint32_t hv_pos[HV_MAXPAT];
static int32_t hv_hmap[HV_HASHCAP];
static uint32_t hv_seq[HV_NBLOCKS];
static mtx_t hv_mtx;
static _Atomic(HvKernel) hv_kernel;
static atomic_int hv_stop;
static atomic_int hv_jit_gen;
static char hv_mccopts[2048];
static MCCState *hv_states[64];
static int hv_nstates;

static uint64_t hv_rng = 1;

static uint64_t hv_rand(void) {
	hv_rng ^= hv_rng << 13;
	hv_rng ^= hv_rng >> 7;
	hv_rng ^= hv_rng << 17;
	return hv_rng;
}

static uint64_t hv_hash(const unsigned char *p) {
	uint64_t h = 0xcbf29ce484222325u;
	for (int i = 0; i < HV_BLOCK; i++) {
		h ^= p[i];
		h *= 0x100000001b3u;
	}
	return h;
}

static uint64_t hv_now_ns(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000u + ts.tv_nsec;
}

static void hv_bump(uint32_t id) {
	uint32_t i = hv_pos[id];
	if (hv_perm[i].weight < UINTMAX_MAX)
		hv_perm[i].weight++;
	while (i > 0 && hv_perm[i].weight > hv_perm[i - 1].weight) {
		HvPat t = hv_perm[i - 1];
		hv_perm[i - 1] = hv_perm[i];
		hv_perm[i] = t;
		hv_pos[hv_perm[i - 1].id] = i - 1;
		hv_pos[hv_perm[i].id] = i;
		i--;
	}
}

static uint32_t hv_intern(const unsigned char *blk) {
	uint64_t h = hv_hash(blk);
	uint32_t slot = (uint32_t)(h % HV_HASHCAP);
	for (;;) {
		int32_t id = hv_hmap[slot];
		if (id < 0)
			break;
		if (hv_perm[hv_pos[id]].hash == h &&
				!memcmp(hv_store + (size_t)id * HV_BLOCK, blk, HV_BLOCK)) {
			hv_bump((uint32_t)id);
			return (uint32_t)id;
		}
		slot = (slot + 1) % HV_HASHCAP;
	}
	if (hv_npat >= HV_MAXPAT) {
		fprintf(stderr, "mcchv: pattern table full\n");
		exit(1);
	}
	uint32_t id = hv_npat++;
	memcpy(hv_store + (size_t)id * HV_BLOCK, blk, HV_BLOCK);
	hv_perm[id].weight = 1;
	hv_perm[id].hash = h;
	hv_perm[id].id = id;
	hv_pos[id] = id;
	hv_hmap[slot] = (int32_t)id;
	hv_bump(id);
	return id;
}

static uint32_t hv_lookup_baseline(const unsigned char *blk, uint64_t h) {
	for (uint32_t i = 0; i < hv_npat; i++) {
		if (hv_perm[i].hash == h &&
				!memcmp(hv_store + (size_t)hv_perm[i].id * HV_BLOCK, blk, HV_BLOCK))
			return hv_perm[i].id;
	}
	return UINT32_MAX;
}

static void hv_jit_error(void *opaque, const char *msg) {
	fprintf(opaque, "mcchv jit: %s\n", msg);
}

typedef struct {
	uint64_t hash;
	uint32_t id;
} HvLeaf;

static int hv_leaf_cmp(const void *a, const void *b) {
	const HvLeaf *x = a, *y = b;
	return x->hash < y->hash ? -1 : x->hash > y->hash ? 1 : 0;
}

static int hv_emit_symbolic;

static size_t hv_emit_one(char *src, size_t n, size_t cap, const HvLeaf *v) {
	if (hv_emit_symbolic)
		return n + (size_t)snprintf(src + n, cap - n,
																"if (h == 0x%" PRIx64 "ull && !hv_memcmp(blk, hv_pat_base + %zu, %d)) return %" PRIu32 "u; ",
																v->hash, (size_t)v->id * HV_BLOCK, HV_BLOCK, v->id);
	uintptr_t pat = (uintptr_t)(hv_store + (size_t)v->id * HV_BLOCK);
	return n + (size_t)snprintf(src + n, cap - n,
															"if (h == 0x%" PRIx64 "ull && !hv_memcmp(blk, (const void *)%" PRIuPTR "ul, %d)) return %" PRIu32 "u; ",
															v->hash, pat, HV_BLOCK, v->id);
}

static size_t hv_emit_tree(char *src, size_t n, size_t cap,
													 const HvLeaf *v, uint32_t lo, uint32_t hi,
													 uint32_t leaf_cut) {
	if (lo >= hi)
		return n + (size_t)snprintf(src + n, cap - n,
																"return hv_baseline_lookup(blk, h);");
	if (hi - lo <= leaf_cut) {
		for (uint32_t i = lo; i < hi; i++)
			n = hv_emit_one(src, n, cap, &v[i]);
		return n + (size_t)snprintf(src + n, cap - n,
																"return hv_baseline_lookup(blk, h);");
	}
	uint32_t mid = lo + (hi - lo) / 2;
	n += (size_t)snprintf(src + n, cap - n, "if (h < 0x%" PRIx64 "ull) { ",
												v[mid].hash);
	n = hv_emit_tree(src, n, cap, v, lo, mid, leaf_cut);
	n += (size_t)snprintf(src + n, cap - n, " } else { ");
	n = hv_emit_tree(src, n, cap, v, mid, hi, leaf_cut);
	n += (size_t)snprintf(src + n, cap - n, " }");
	return n;
}

static char *hv_kernel_source(const HvLeaf *leaves, uint32_t nleaf,
															const HvLeaf *hot, uint32_t nhot,
															uint32_t leaf_cut, size_t *codebytes,
															int symbolic) {
	size_t cap = (size_t)nleaf * 220 + (size_t)nhot * 220 + 65536;
	char *src = malloc(cap);
	size_t n = 0;
	if (!src)
		return NULL;
	if (leaf_cut < 1)
		leaf_cut = 1;
	hv_emit_symbolic = symbolic;
	n += (size_t)snprintf(src + n, cap - n,
												"extern unsigned hv_baseline_lookup(const unsigned char *, unsigned long long);\n"
												"extern int hv_memcmp(const void *, const void *, unsigned long);\n");
	if (symbolic)
		n += (size_t)snprintf(src + n, cap - n,
													"extern const unsigned char hv_pat_base[];\n");
	n += (size_t)snprintf(src + n, cap - n,
												"unsigned hv_kernel_jit(const unsigned char *blk, unsigned long long h) {\n");
	for (uint32_t i = 0; i < nhot; i++)
		n = hv_emit_one(src, n, cap, &hot[i]);
	n = hv_emit_tree(src, n, cap, leaves, 0, nleaf, leaf_cut);
	n += (size_t)snprintf(src + n, cap - n, "\n}\n");
	hv_emit_symbolic = 0;
	if (codebytes)
		*codebytes = n;
	return src;
}

static HvKernel hv_jit_build(const HvLeaf *leaves, uint32_t nleaf,
														 const HvLeaf *hot, uint32_t nhot,
														 uint32_t leaf_cut, size_t *codebytes,
														 MCCState **st_out) {
	char *src = hv_kernel_source(leaves, nleaf, hot, nhot, leaf_cut,
															 codebytes, 0);
	if (!src)
		return NULL;

	MCCState *s = mcc_new();
	if (!s) {
		free(src);
		return NULL;
	}
	mcc_set_error_func(s, stderr, hv_jit_error);
	if (hv_mccopts[0])
		mcc_set_options(s, hv_mccopts);
	mcc_set_options(s, "-nostdlib");
	mcc_set_output_type(s, MCC_OUTPUT_MEMORY);
	mcc_add_symbol(s, "hv_baseline_lookup", (const void *)hv_lookup_baseline);
	mcc_add_symbol(s, "hv_memcmp", (const void *)memcmp);
	if (mcc_compile_string(s, src) < 0 || mcc_relocate(s) < 0) {
		mcc_delete(s);
		free(src);
		return NULL;
	}
	free(src);
	HvKernel fn = (HvKernel)mcc_get_symbol(s, "hv_kernel_jit");
	if (!fn) {
		mcc_delete(s);
		return NULL;
	}
	if (st_out)
		*st_out = s;
	else if (hv_nstates < (int)(sizeof hv_states / sizeof hv_states[0]))
		hv_states[hv_nstates++] = s;
	else
		mcc_delete(s);
	return fn;
}

static int hv_optimizer(void *arg) {
	uint32_t last_n = 0;
	(void)arg;
	while (!atomic_load(&hv_stop)) {
		uint32_t np;
		mtx_lock(&hv_mtx);
		np = hv_npat;
		mtx_unlock(&hv_mtx);
		if (np > 0 && np != last_n) {
			HvLeaf *leaves = malloc((size_t)np * sizeof *leaves);
			if (leaves) {
				mtx_lock(&hv_mtx);
				for (uint32_t i = 0; i < np; i++) {
					leaves[i].hash = hv_perm[i].hash;
					leaves[i].id = hv_perm[i].id;
				}
				mtx_unlock(&hv_mtx);
				qsort(leaves, np, sizeof *leaves, hv_leaf_cmp);
				HvKernel fn = hv_jit_build(leaves, np, NULL, 0, 1, NULL, NULL);
				free(leaves);
				if (fn) {
					atomic_store(&hv_kernel, fn);
					atomic_fetch_add(&hv_jit_gen, 1);
					last_n = np;
				}
			}
		}
		thrd_yield();
		struct timespec ts = {0, 5000000};
		thrd_sleep(&ts, NULL);
	}
	return 0;
}

typedef struct {
	uint32_t lo, hi;
	uint32_t bad;
} HvRange;

static int hv_sweep_worker(void *arg) {
	HvRange *r = arg;
	HvKernel k = atomic_load(&hv_kernel);
	uint32_t bad = 0;
	for (uint32_t i = r->lo; i < r->hi; i++) {
		const unsigned char *blk = hv_data + (size_t)i * HV_BLOCK;
		uint32_t id = k(blk, hv_hash(blk));
		if (id != hv_seq[i])
			bad++;
	}
	r->bad = bad;
	return 0;
}

static double hv_sweep(int workers, uint32_t *bad_out) {
	thrd_t th[64];
	HvRange rg[64];
	uint32_t per = HV_NBLOCKS / (uint32_t)workers;
	uint64_t t0 = hv_now_ns();
	for (int w = 0; w < workers; w++) {
		rg[w].lo = (uint32_t)w * per;
		rg[w].hi = w == workers - 1 ? HV_NBLOCKS : rg[w].lo + per;
		rg[w].bad = 0;
		thrd_create(&th[w], hv_sweep_worker, &rg[w]);
	}
	uint32_t bad = 0;
	for (int w = 0; w < workers; w++) {
		int res;
		thrd_join(th[w], &res);
		bad += rg[w].bad;
	}
	*bad_out = bad;
	return (double)(hv_now_ns() - t0) / 1e6;
}

static double hv_mean(const double *v, int n) {
	double s = 0;
	for (int i = 0; i < n; i++)
		s += v[i];
	return s / n;
}

static double hv_stdev(const double *v, int n, double m) {
	double s = 0;
	if (n < 2)
		return 0;
	for (int i = 0; i < n; i++)
		s += (v[i] - m) * (v[i] - m);
	return sqrt(s / (n - 1));
}

static double hv_welch_t(const double *a, int na, const double *b, int nb) {
	double ma = hv_mean(a, na), mb = hv_mean(b, nb);
	double sa = hv_stdev(a, na, ma), sb = hv_stdev(b, nb, mb);
	double se = sqrt(sa * sa / na + sb * sb / nb);
	if (se == 0)
		return ma == mb ? 0 : 1e9;
	return (ma - mb) / se;
}

static uint64_t hv_fold(uint64_t h, uint64_t v) {
	for (int i = 0; i < 8; i++) {
		h ^= (v >> (i * 8)) & 0xff;
		h *= 0x100000001b3u;
	}
	return h;
}

static uint64_t hv_fold_str(uint64_t h, const char *s) {
	for (; *s; s++) {
		h ^= (unsigned char)*s;
		h *= 0x100000001b3u;
	}
	return h;
}

static uint64_t hv_pattern_hash(void) {
	uint64_t h = 0xcbf29ce484222325u;
	for (uint32_t i = 0; i < hv_npat; i++) {
		h ^= hv_perm[i].hash;
		h *= 0x100000001b3u;
		h ^= (uint64_t)hv_perm[i].weight;
		h *= 0x100000001b3u;
	}
	return h;
}

static uint64_t hv_intention_key(int *via_ast) {
	uint64_t h = 0xcbf29ce484222325u;
	uint32_t np = hv_npat;
	HvLeaf *leaves = malloc((size_t)np * sizeof *leaves);
	*via_ast = 0;
	if (leaves) {
		for (uint32_t i = 0; i < np; i++) {
			leaves[i].hash = hv_perm[i].hash;
			leaves[i].id = hv_perm[i].id;
		}
		qsort(leaves, np, sizeof *leaves, hv_leaf_cmp);
		char *src = hv_kernel_source(leaves, np, NULL, 0, 1, NULL, 1);
		free(leaves);
		if (src) {
			MCCState *s = mcc_new();
			if (s) {
				mcc_set_error_func(s, stderr, hv_jit_error);
				if (hv_mccopts[0])
					mcc_set_options(s, hv_mccopts);
				mcc_set_options(s, "-nostdlib -O1");
				mcc_set_output_type(s, MCC_OUTPUT_MEMORY);
				if (mcc_compile_string(s, src) >= 0) {
					uint64_t ih = mcc_intention_hash(s);
					if (ih) {
						h = hv_fold(h, ih);
						*via_ast = 1;
					}
				}
				mcc_delete(s);
			}
			free(src);
		}
	}
	if (!*via_ast)
		h = hv_fold(h, hv_pattern_hash());
#ifdef MCC_VERSION
	h = hv_fold(h, (uint64_t)MCC_VERSION);
#endif
#ifdef MCC_CONFIG_TRIPLET
	h = hv_fold_str(h, MCC_CONFIG_TRIPLET);
#endif
	return h;
}

static int hv_cache_path(char *buf, int cap, uint64_t key) {
	const char *env = getenv("MCCHV_CACHE_DIR");
	char dir[3072];
	if (env && env[0]) {
		snprintf(dir, sizeof dir, "%s", env);
		mkdir(dir, 0755);
	} else if (mcc_cache_dir(dir, sizeof dir) != 0) {
		return -1;
	}
	snprintf(buf, cap, "%s/mcchv-%016" PRIx64 ".cache", dir, key);
	return 0;
}

#define HV_CACHE_MAGIC 0x6d6363687643u
#define HV_CACHE_FMT 2u
#define HV_CACHE_OBS 64u

typedef struct {
	uint32_t nhot, cut;
	double obj;
} HvObs;

typedef struct {
	uint64_t magic;
	uint32_t fmt;
	uint32_t nobs;
	uint64_t intention;
	uint32_t nhot;
	uint32_t leaf_cut;
	double cpu_ms;
	uint64_t code_bytes;
	uint64_t candidates;
	uint64_t next_lin;
	uint64_t rng;
	HvObs obs[HV_CACHE_OBS];
} HvCacheEntry;

static int hv_cache_read(uint64_t key, HvCacheEntry *e) {
	char path[4096];
	FILE *f;
	int ok;
	if (hv_cache_path(path, sizeof path, key) != 0)
		return 0;
	f = fopen(path, "rb");
	if (!f)
		return 0;
	ok = fread(e, sizeof *e, 1, f) == 1 && e->magic == HV_CACHE_MAGIC &&
			 e->fmt == HV_CACHE_FMT && e->intention == key &&
			 e->nobs <= HV_CACHE_OBS && e->leaf_cut >= 1 && e->leaf_cut <= 8;
	fclose(f);
	return ok;
}

static long hv_cache_write(uint64_t key, const HvCacheEntry *e) {
	char path[4096], lockp[4160], tmpp[4160];
	HvCacheEntry out, b;
	FILE *f;
	int lockfd;
	long sz = -1;
	if (hv_cache_path(path, sizeof path, key) != 0)
		return -1;
	snprintf(lockp, sizeof lockp, "%s.lock", path);
	snprintf(tmpp, sizeof tmpp, "%s.tmp", path);
	lockfd = open(lockp, O_CREAT | O_RDWR, 0644);
	if (lockfd >= 0)
		flock(lockfd, LOCK_EX);
	out = *e;
	if (hv_cache_read(key, &b)) {
		uint64_t cand = b.candidates > out.candidates ? b.candidates
																									: out.candidates;
		uint64_t nlin = b.next_lin > out.next_lin ? b.next_lin : out.next_lin;
		if (b.cpu_ms > 0 && (out.cpu_ms <= 0 || b.cpu_ms < out.cpu_ms))
			out = b;
		out.candidates = cand;
		out.next_lin = nlin;
	}
	f = fopen(tmpp, "wb");
	if (f) {
		if (fwrite(&out, sizeof out, 1, f) == 1) {
			fflush(f);
			fsync(fileno(f));
			sz = (long)sizeof out;
		}
		fclose(f);
		if (sz > 0 && rename(tmpp, path) != 0)
			sz = -1;
	}
	if (lockfd >= 0) {
		flock(lockfd, LOCK_UN);
		close(lockfd);
	}
	return sz;
}

static double hv_measure(HvKernel fn, int workers, int reps) {
	HvKernel prev = atomic_load(&hv_kernel);
	double best = 1e300;
	uint32_t bad = 0;
	atomic_store(&hv_kernel, fn);
	for (int r = 0; r < reps; r++) {
		double ms = hv_sweep(workers, &bad);
		if (bad) {
			atomic_store(&hv_kernel, prev);
			return -1;
		}
		if (ms < best)
			best = ms;
	}
	atomic_store(&hv_kernel, prev);
	return best;
}

typedef struct {
	HvLeaf *leaves;
	uint32_t np;
	HvLeaf *hot;
	int workers;
	MCCState *bst;
	double base_cpu;
	size_t base_mem;
	HvKernel winner;
	MCCState *winner_st;
	uint32_t best_nhot, best_cut;
	double best_cpu;
	size_t best_mem;
} HvSearchCtx;

static double hv_eval_cand(HvSearchCtx *c, uint32_t nhot, uint32_t cut) {
	size_t mem = 0;
	MCCState *st = NULL;
	HvKernel fn = hv_jit_build(c->leaves, c->np, c->hot, nhot, cut, &mem, &st);
	if (!fn)
		return -1;
	double cpu = hv_measure(fn, c->workers, 3);
	if (cpu < 0) {
		mcc_delete(st);
		return -1;
	}
	int better = cpu < c->best_cpu && mem <= c->best_mem;
	if (!better && cpu <= c->best_cpu && mem < c->best_mem)
		better = 1;
	if (better) {
		if (c->winner_st && c->winner_st != c->bst)
			mcc_delete(c->winner_st);
		c->winner = fn;
		c->winner_st = st;
		c->best_nhot = nhot;
		c->best_cut = cut;
		c->best_cpu = cpu;
		c->best_mem = mem;
	} else {
		mcc_delete(st);
	}
	return cpu / (c->base_cpu > 0 ? c->base_cpu : 1.0) +
				 (double)mem / (c->base_mem ? c->base_mem : 1);
}

static const double *hv_objkey;

static int hv_idx_cmp(const void *a, const void *b) {
	double x = hv_objkey[*(const int *)a], y = hv_objkey[*(const int *)b];
	return x < y ? -1 : x > y ? 1 : 0;
}

static void hv_parzen(const uint32_t *vals, const int *idx, int n0, int n1,
											uint32_t vmin, uint32_t vmax, double *dens) {
	uint32_t D = vmax - vmin + 1;
	double s = 0;
	for (uint32_t d = 0; d < D; d++)
		dens[d] = 1.0 / D;
	for (int i = n0; i < n1; i++) {
		uint32_t v = vals[idx[i]] - vmin;
		dens[v] += 1.0;
		if (v > 0)
			dens[v - 1] += 0.25;
		if (v + 1 < D)
			dens[v + 1] += 0.25;
	}
	for (uint32_t d = 0; d < D; d++)
		s += dens[d];
	for (uint32_t d = 0; d < D; d++)
		dens[d] /= s;
}

static uint32_t hv_sample_dens(const double *dens, uint32_t D, uint32_t vmin) {
	double r = (double)(hv_rand() % 1000000) / 1000000.0;
	double acc = 0;
	for (uint32_t d = 0; d < D; d++) {
		acc += dens[d];
		if (r <= acc)
			return vmin + d;
	}
	return vmin + D - 1;
}

static uintmax_t hv_search_tpe(HvSearchCtx *c, uint64_t deadline,
															 uint32_t nhot_max, HvCacheEntry *e,
															 int warm) {
	enum { HV_TPE_SEED = 8, HV_TPE_EI = 24 };
	uintmax_t cand = 0;
	int hcap = 256, hn = 0;
	uint32_t *h_nhot = malloc((size_t)hcap * sizeof *h_nhot);
	uint32_t *h_cut = malloc((size_t)hcap * sizeof *h_cut);
	double *h_obj = malloc((size_t)hcap * sizeof *h_obj);
	int *idx = malloc((size_t)hcap * sizeof *idx);
	char seen[65][9];
	double gn[65], bn[65], gc[8], bc[8];
	if (!h_nhot || !h_cut || !h_obj || !idx) {
		free(h_nhot);
		free(h_cut);
		free(h_obj);
		free(idx);
		return 0;
	}
	memset(seen, 0, sizeof seen);
	if (warm) {
		for (uint32_t i = 0; i < e->nobs && hn < hcap; i++) {
			uint32_t nh = e->obs[i].nhot, ct = e->obs[i].cut;
			if (nh > nhot_max || ct < 1 || ct > 8)
				continue;
			h_nhot[hn] = nh;
			h_cut[hn] = ct;
			h_obj[hn] = e->obs[i].obj;
			hn++;
			seen[nh][ct] = 1;
		}
	}
	for (int s = hn; s < HV_TPE_SEED && hv_now_ns() < deadline; s++) {
		uint32_t nhot = (uint32_t)(hv_rand() % (nhot_max + 1));
		uint32_t cut = 1 + (uint32_t)(hv_rand() % 8);
		double obj = hv_eval_cand(c, nhot, cut);
		cand++;
		if (obj < 0)
			continue;
		h_nhot[hn] = nhot;
		h_cut[hn] = cut;
		h_obj[hn] = obj;
		hn++;
		seen[nhot][cut] = 1;
	}
	while (hv_now_ns() < deadline) {
		uint32_t sel_nhot, sel_cut;
		if (hn < 2) {
			sel_nhot = (uint32_t)(hv_rand() % (nhot_max + 1));
			sel_cut = 1 + (uint32_t)(hv_rand() % 8);
		} else {
			int ngood = (int)(0.25 * hn);
			if (ngood < 1)
				ngood = 1;
			for (int i = 0; i < hn; i++)
				idx[i] = i;
			hv_objkey = h_obj;
			qsort(idx, (size_t)hn, sizeof *idx, hv_idx_cmp);
			hv_parzen(h_nhot, idx, 0, ngood, 0, nhot_max, gn);
			hv_parzen(h_nhot, idx, ngood, hn, 0, nhot_max, bn);
			hv_parzen(h_cut, idx, 0, ngood, 1, 8, gc);
			hv_parzen(h_cut, idx, ngood, hn, 1, 8, bc);
			double best_score = 0;
			int sel_new = 0, have = 0;
			sel_nhot = 0;
			sel_cut = 1;
			for (int k = 0; k < HV_TPE_EI; k++) {
				uint32_t nh = hv_sample_dens(gn, nhot_max + 1, 0);
				uint32_t ct = hv_sample_dens(gc, 8, 1);
				double score = gn[nh] * gc[ct - 1] /
											 (bn[nh] * bc[ct - 1] + 1e-12);
				int isnew = !seen[nh][ct];
				int take = !have || (isnew && !sel_new) ||
									 (isnew == sel_new && score > best_score);
				if (take) {
					have = 1;
					sel_new = isnew;
					best_score = score;
					sel_nhot = nh;
					sel_cut = ct;
				}
			}
		}
		double obj = hv_eval_cand(c, sel_nhot, sel_cut);
		cand++;
		if (obj < 0)
			continue;
		if (hn == hcap) {
			int ncap = hcap * 2;
			uint32_t *a = realloc(h_nhot, (size_t)ncap * sizeof *a);
			uint32_t *b = realloc(h_cut, (size_t)ncap * sizeof *b);
			double *o = realloc(h_obj, (size_t)ncap * sizeof *o);
			int *ix = realloc(idx, (size_t)ncap * sizeof *ix);
			if (!a || !b || !o || !ix) {
				h_nhot = a ? a : h_nhot;
				h_cut = b ? b : h_cut;
				h_obj = o ? o : h_obj;
				idx = ix ? ix : idx;
				break;
			}
			h_nhot = a;
			h_cut = b;
			h_obj = o;
			idx = ix;
			hcap = ncap;
		}
		h_nhot[hn] = sel_nhot;
		h_cut[hn] = sel_cut;
		h_obj[hn] = obj;
		hn++;
		seen[sel_nhot][sel_cut] = 1;
	}
	e->nobs = 0;
	if (hn > 0) {
		for (int i = 0; i < hn; i++)
			idx[i] = i;
		hv_objkey = h_obj;
		qsort(idx, (size_t)hn, sizeof *idx, hv_idx_cmp);
		uint32_t keep = hn < (int)HV_CACHE_OBS ? (uint32_t)hn : HV_CACHE_OBS;
		for (uint32_t i = 0; i < keep; i++) {
			e->obs[i].nhot = h_nhot[idx[i]];
			e->obs[i].cut = h_cut[idx[i]];
			e->obs[i].obj = h_obj[idx[i]];
		}
		e->nobs = keep;
	}
	e->rng = hv_rng;
	free(h_nhot);
	free(h_cut);
	free(h_obj);
	free(idx);
	return cand;
}

static uintmax_t hv_search(double seconds, int workers, int use_tpe,
													 uint64_t seed, uint32_t *best_nhot,
													 uint32_t *best_cut, double *best_cpu,
													 size_t *best_mem, double *base_cpu,
													 size_t *base_mem, HvCacheEntry *e, int warm) {
	uint32_t np = hv_npat;
	HvLeaf *leaves = malloc((size_t)np * sizeof *leaves);
	HvLeaf hot[64];
	uint32_t nhot_max = np < 64 ? np : 64;
	if (!leaves)
		return 0;
	for (uint32_t i = 0; i < np; i++) {
		leaves[i].hash = hv_perm[i].hash;
		leaves[i].id = hv_perm[i].id;
		if (i < nhot_max) {
			hot[i].hash = hv_perm[i].hash;
			hot[i].id = hv_perm[i].id;
		}
	}
	qsort(leaves, np, sizeof *leaves, hv_leaf_cmp);

	size_t bmem = 0;
	MCCState *bst = NULL;
	HvKernel bfn = hv_jit_build(leaves, np, NULL, 0, 1, &bmem, &bst);
	double bcpu = bfn ? hv_measure(bfn, workers, 3) : -1;
	HvSearchCtx ctx = {leaves, np, hot, workers, bst, bcpu, bmem,
										 bfn, bst, 0, 1, bcpu, bmem};

	if (warm && e->leaf_cut >= 1) {
		uint32_t wnhot = e->nhot > nhot_max ? nhot_max : e->nhot;
		size_t wmem = 0;
		MCCState *wst = NULL;
		HvKernel wfn = hv_jit_build(leaves, np, hot, wnhot, e->leaf_cut,
																&wmem, &wst);
		double wcpu = wfn ? hv_measure(wfn, workers, 3) : -1;
		if (wfn && wcpu >= 0 &&
				(wcpu < ctx.best_cpu || (wcpu <= ctx.best_cpu && wmem < ctx.best_mem))) {
			ctx.winner = wfn;
			ctx.winner_st = wst;
			ctx.best_nhot = wnhot;
			ctx.best_cut = e->leaf_cut;
			ctx.best_cpu = wcpu;
			ctx.best_mem = wmem;
			atomic_store(&hv_kernel, ctx.winner);
		} else if (wst) {
			mcc_delete(wst);
		}
	}

	uint64_t deadline = hv_now_ns() + (uint64_t)(seconds * 1e9);
	uintmax_t cand;
	if (use_tpe) {
		hv_rng = warm && e->rng ? e->rng
														: seed ? seed ^ 0x9e3779b97f4a7c15u : 1;
		cand = hv_search_tpe(&ctx, deadline, nhot_max, e, warm);
	} else {
		uint64_t cur = warm ? e->next_lin : 0;
		cand = 0;
		for (; hv_now_ns() < deadline; cand++, cur++) {
			uint32_t nhot = (uint32_t)(cur % (nhot_max + 1));
			uint32_t cut = 1 + (uint32_t)((cur / (nhot_max + 1)) % 8);
			hv_eval_cand(&ctx, nhot, cut);
		}
		e->next_lin = cur;
	}
	e->nhot = ctx.best_nhot;
	e->leaf_cut = ctx.best_cut;
	e->cpu_ms = ctx.best_cpu;
	e->code_bytes = ctx.best_mem;

	if (ctx.winner)
		atomic_store(&hv_kernel, ctx.winner);
	if (ctx.winner_st && hv_nstates < (int)(sizeof hv_states / sizeof hv_states[0]))
		hv_states[hv_nstates++] = ctx.winner_st;
	if (bst && bst != ctx.winner_st &&
			hv_nstates < (int)(sizeof hv_states / sizeof hv_states[0]))
		hv_states[hv_nstates++] = bst;
	*best_nhot = ctx.best_nhot;
	*best_cut = ctx.best_cut;
	*best_cpu = ctx.best_cpu;
	*best_mem = ctx.best_mem;
	*base_cpu = bcpu;
	*base_mem = bmem;
	free(leaves);
	return cand;
}

int main(int argc, char **argv) {
	uint64_t seed = 42;
	int passes = 6, workers = 4;
	double search_seconds = 0;
	int use_tpe = 0;
	const char *envsearch = getenv("MCCHV_SEARCH");
	if (envsearch && !strcmp(envsearch, "tpe"))
		use_tpe = 1;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
			seed = strtoull(argv[++i], NULL, 0);
		} else if (!strcmp(argv[i], "--search") && i + 1 < argc) {
			use_tpe = !strcmp(argv[++i], "tpe");
		} else if (!strcmp(argv[i], "--passes") && i + 1 < argc) {
			passes = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--workers") && i + 1 < argc) {
			workers = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--seconds") && i + 1 < argc) {
			search_seconds = atof(argv[++i]);
		} else if (!strncmp(argv[i], "-O", 2)) {
			int lvl = atoi(argv[i] + 2);
			if (lvl > 3)
				search_seconds = lvl > 28800 ? 28800 : lvl;
		} else {
			size_t l = strlen(hv_mccopts);
			snprintf(hv_mccopts + l, sizeof hv_mccopts - l, "%s\"%s\"",
							 l ? " " : "", argv[i]);
		}
	}
	if (passes < 2)
		passes = 2;
	if (passes > 32)
		passes = 32;
	if (workers < 1)
		workers = 1;
	if (workers > 32)
		workers = 32;
	hv_rng = seed ? seed : 1;

	hv_data = malloc((size_t)HV_NBLOCKS * HV_BLOCK);
	hv_store = malloc((size_t)HV_MAXPAT * HV_BLOCK);
	if (!hv_data || !hv_store)
		return 1;
	memset(hv_hmap, -1, sizeof hv_hmap);
	mtx_init(&hv_mtx, mtx_plain);
	atomic_store(&hv_kernel, hv_lookup_baseline);

	unsigned char planted[HV_PLANTED][HV_BLOCK];
	for (int p = 0; p < HV_PLANTED; p++)
		for (int b = 0; b < HV_BLOCK; b++)
			planted[p][b] = (unsigned char)hv_rand();
	double cum[HV_PLANTED];
	double zsum = 0;
	for (int p = 0; p < HV_PLANTED; p++) {
		zsum += 1.0 / (p + 1);
		cum[p] = zsum;
	}
	for (uint32_t i = 0; i < HV_NBLOCKS; i++) {
		unsigned char *blk = hv_data + (size_t)i * HV_BLOCK;
		if (hv_rand() % 100 < 85) {
			double r = (double)(hv_rand() % 1000000) / 1000000.0 * zsum;
			int p = 0;
			while (p < HV_PLANTED - 1 && cum[p] < r)
				p++;
			memcpy(blk, planted[p], HV_BLOCK);
		} else {
			for (int b = 0; b < HV_BLOCK; b++)
				blk[b] = (unsigned char)hv_rand();
		}
	}

	mtx_lock(&hv_mtx);
	for (uint32_t i = 0; i < HV_NBLOCKS; i++)
		hv_seq[i] = hv_intern(hv_data + (size_t)i * HV_BLOCK);
	mtx_unlock(&hv_mtx);

	unsigned char *replay = malloc((size_t)HV_NBLOCKS * HV_BLOCK);
	if (!replay)
		return 1;
	for (uint32_t i = 0; i < HV_NBLOCKS; i++)
		memcpy(replay + (size_t)i * HV_BLOCK,
					 hv_store + (size_t)hv_seq[i] * HV_BLOCK, HV_BLOCK);
	int replay_ok = !memcmp(replay, hv_data, (size_t)HV_NBLOCKS * HV_BLOCK);
	free(replay);
	size_t orig = (size_t)HV_NBLOCKS * HV_BLOCK;
	size_t comp = (size_t)hv_npat * HV_BLOCK + (size_t)HV_NBLOCKS * sizeof(uint32_t);
	printf("hv: replay %s, blocks=%u unique=%u compressed=%zu/%zu (%.2fx)\n",
				 replay_ok ? "OK" : "MISMATCH", HV_NBLOCKS, hv_npat, comp, orig,
				 (double)orig / (double)comp);
	printf("hv: top weights:");
	for (uint32_t i = 0; i < 5 && i < hv_npat; i++)
		printf(" #%" PRIu32 "=%ju", hv_perm[i].id, hv_perm[i].weight);
	printf(" (weight type: %zu-bit uintmax_t)\n", sizeof(uintmax_t) * 8);
	if (!replay_ok)
		return 1;

	double base_ms[32], jit_ms[32];
	uint32_t bad = 0;
	for (int p = 0; p < passes; p++) {
		base_ms[p] = hv_sweep(workers, &bad);
		if (bad) {
			printf("hv: baseline sweep mismatch (%u)\n", bad);
			return 1;
		}
	}

	thrd_t opt;
	thrd_create(&opt, hv_optimizer, NULL);
	int waited = 0;
	while (atomic_load(&hv_jit_gen) == 0 && waited < 10000) {
		struct timespec ts = {0, 1000000};
		thrd_sleep(&ts, NULL);
		waited++;
	}
	if (atomic_load(&hv_jit_gen) == 0) {
		printf("hv: jit never landed\n");
		atomic_store(&hv_stop, 1);
		thrd_join(opt, NULL);
		return 1;
	}
	printf("hv: jit swapped in (gen %d, binary hash tree over %u patterns, %d worker threads)\n",
				 atomic_load(&hv_jit_gen), hv_npat, workers);

	for (int p = 0; p < passes; p++) {
		jit_ms[p] = hv_sweep(workers, &bad);
		if (bad) {
			printf("hv: jit sweep mismatch (%u)\n", bad);
			atomic_store(&hv_stop, 1);
			thrd_join(opt, NULL);
			return 1;
		}
	}
	atomic_store(&hv_stop, 1);
	thrd_join(opt, NULL);

	double bm = hv_mean(base_ms, passes), bs = hv_stdev(base_ms, passes, bm);
	double jm = hv_mean(jit_ms, passes), js = hv_stdev(jit_ms, passes, jm);
	double t = hv_welch_t(base_ms, passes, jit_ms, passes);
	printf("hv: baseline %.2f±%.2fms  jit %.2f±%.2fms  speedup %.2fx  welch-t %.2f (%s)\n",
				 bm, bs, jm, js, jm > 0 ? bm / jm : 0, t,
				 t > 2.2 ? "significant" : "not significant");

	if (search_seconds > 0) {
		uint32_t bnhot = 0, bcut = 1;
		double bcpu = 0, base_cpu = 0;
		size_t bmem = 0, base_mem = 0;
		int via_ast = 0;
		uint64_t key = hv_intention_key(&via_ast);
		char cpath[4096];
		int cache_ok = hv_cache_path(cpath, sizeof cpath, key) == 0;
		HvCacheEntry ent;
		memset(&ent, 0, sizeof ent);
		int warm = cache_ok && hv_cache_read(key, &ent);
		uint64_t prev_cand = warm ? ent.candidates : 0;
		if (!cache_ok)
			printf("hv: cache disabled (no cache dir)\n");
		else
			printf("hv: %s\n", warm ? "cache hit (warm-start)"
															: "cache miss (cold)");
		printf("hv: -O search budget %.0fs, intention 0x%016" PRIx64 " (%s) search=%s\n",
					 search_seconds, key, via_ast ? "ast" : "pattern-table",
					 use_tpe ? "tpe" : "linear");
		if (warm)
			printf("hv: cache resume: %u observations, cursor %" PRIu64 ", best nhot=%u leaf_cut=%u\n",
						 ent.nobs, ent.next_lin, ent.nhot, ent.leaf_cut);
		uintmax_t n = hv_search(search_seconds, workers, use_tpe, seed, &bnhot,
														&bcut, &bcpu, &bmem, &base_cpu, &base_mem,
														&ent, warm);
		ent.magic = HV_CACHE_MAGIC;
		ent.fmt = HV_CACHE_FMT;
		ent.intention = key;
		ent.candidates = prev_cand + n;
		long csz = cache_ok ? hv_cache_write(key, &ent) : -1;
		printf("hv: searched %ju candidates via %s; best nhot=%u leaf_cut=%u\n",
					 n, use_tpe ? "tpe (Parzen surrogate)" : "linear 0..UINTMAX sweep",
					 bnhot, bcut);
		printf("hv:   cpu  %.3fms -> %.3fms (%.2fx)   mem(code) %zuB -> %zuB (%+.1f%%)\n",
					 base_cpu, bcpu, bcpu > 0 ? base_cpu / bcpu : 0, base_mem, bmem,
					 base_mem ? 100.0 * ((double)bmem - base_mem) / base_mem : 0);
		if (cache_ok)
			printf("hv:   cache file %ld bytes at intention key\n", csz);
	}

	for (int i = 0; i < hv_nstates; i++)
		mcc_delete(hv_states[i]);
	free(hv_data);
	free(hv_store);
	mtx_destroy(&hv_mtx);
	return 0;
}

#endif
