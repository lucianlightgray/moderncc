/*
 * lru — LRU cache driven by a linear-congruential PRNG.
 *
 * Ported from The Computer Language Benchmarks Game lru problem
 * (vendor/plb/bench/algorithm/lru/, logic mirrored from 1.py). Each iteration
 * draws a key from one PRNG and put()s it, then draws a key from a second PRNG
 * and get()s it, counting hits and misses. The cache is a single shared,
 * recency-ordered structure that every operation mutates, and both PRNGs are
 * strictly sequential (seed[i+1] = (A*seed[i] + C) % M) — so the whole driver
 * is inherently serial. <threads.h> is still included and the NT macro honored
 * (the runner builds a -DNT=1 serial reference of every kernel), but no threads
 * are spawned: concurrent access would race the shared cache and reorder draws.
 *
 * The cache is an intrusive doubly-linked recency list indexed directly by key.
 * Keys are bounded to [0, mod) with mod = CACHE_SIZE*10, so a per-key node id is
 * the key itself: next[]/prev[] hold the recency order (front = least-recently
 * used, evicted first; back = most-recently used), present[] the membership,
 * with index `mod` as the list sentinel. get/put move touched keys to the back;
 * put evicts the front when the cache is full — matching OrderedDict semantics.
 *
 * Work scale: N = argv[1] (default 2000); the driver runs N*1000 operations
 * (default 2,000,000) against a CACHE_SIZE=1000 cache over a 10000-key space.
 * (The key space avoids large powers of two: this LCG's low-order bits have a
 * short period, and a mod like 10240 collapses the hit rate to zero.) Output is
 * the deterministic hit and miss counts, identical for any NT.
 *
 * <threads.h> leads and libc is hand-declared rather than #included: mcc's
 * cooperative backend (-DMCC_THREADS_COOP) defines its own once_flag, which
 * collides with the one glibc's <stdlib.h> pulls in.
 */
#include <threads.h>

extern void *malloc(__SIZE_TYPE__);
extern void free(void *);
extern int atoi(const char *);
extern int printf(const char *, ...);

#ifndef NT
#define NT 4
#endif

#define CACHE_SIZE 1000
#define LCG_A 1103515245ULL
#define LCG_C 12345ULL
#define LCG_M (1ULL << 31)

static int *nxt;
static int *prv;
static unsigned char *present;
static int sentinel;
static int cap;
static int count;

static void lru_init(int mod, int size) {
	sentinel = mod;
	cap = size;
	count = 0;
	nxt = malloc((__SIZE_TYPE__)(mod + 1) * sizeof(int));
	prv = malloc((__SIZE_TYPE__)(mod + 1) * sizeof(int));
	present = malloc((__SIZE_TYPE__)mod);
	for (int i = 0; i < mod; i++)
		present[i] = 0;
	nxt[sentinel] = sentinel;
	prv[sentinel] = sentinel;
}

static void unlink_key(int k) {
	nxt[prv[k]] = nxt[k];
	prv[nxt[k]] = prv[k];
}

static void insert_back(int k) {
	prv[k] = prv[sentinel];
	nxt[k] = sentinel;
	nxt[prv[sentinel]] = k;
	prv[sentinel] = k;
}

static void lru_put(int k) {
	if (present[k]) {
		unlink_key(k);
		insert_back(k);
		return;
	}
	if (count == cap) {
		int old = nxt[sentinel];
		unlink_key(old);
		present[old] = 0;
		count--;
	}
	present[k] = 1;
	insert_back(k);
	count++;
}

static int lru_get(int k) {
	if (!present[k])
		return 0;
	unlink_key(k);
	insert_back(k);
	return 1;
}

int main(int argc, char **argv) {
	int n = argc > 1 ? atoi(argv[1]) : 2000;
	if (n < 1)
		n = 1;
	long ops = (long)n * 1000;
	int mod = CACHE_SIZE * 10;

	lru_init(mod, CACHE_SIZE);

	unsigned long long s0 = 0, s1 = 1;
	unsigned long long hit = 0, missed = 0;

	for (long i = 0; i < ops; i++) {
		s0 = (LCG_A * s0 + LCG_C) % LCG_M;
		int k0 = (int)(s0 % (unsigned long long)mod);
		lru_put(k0);
		s1 = (LCG_A * s1 + LCG_C) % LCG_M;
		int k1 = (int)(s1 % (unsigned long long)mod);
		if (lru_get(k1))
			hit++;
		else
			missed++;
	}

	printf("hit=%llu missed=%llu\n", hit, missed);

	free(nxt);
	free(prv);
	free(present);
	return 0;
}
