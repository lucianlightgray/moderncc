/*
 * k-nucleotide — oligonucleotide frequencies, in-process sequence, fork-join.
 *
 * Ported from The Computer Language Benchmarks Game entry
 * vendor/plb/bench/algorithm/knucleotide/1.c (Contributed by Jeremy Zerfas).
 * The original reads the ">THREE" polynucleotide from a FASTA file on stdin,
 * which is not self-contained. This port instead GENERATES that sequence in
 * process with the standard CLBG fasta linear-congruential generator
 * (seed 42, IM=139968, IA=3877, IC=29573 — the same one the fasta problem
 * uses) and the HomoSapiens frequency table: sequence ONE uses no random
 * draws, sequence TWO consumes 3*fn draws, and sequence THREE (length 5*fn,
 * fn = N*KNUC_FASTA_SCALE) is what we count. Advancing the LCG past the TWO
 * draws before generating THREE reproduces the CLBG THREE stream exactly.
 *
 * The seven output items — k=1 frequencies, k=2 frequencies, and the counts of
 * GGT, GGTA, GGTATT, GGTATTTTAATT, GGTATTTTAATTTATAGT — are data-independent,
 * so each runs on one of NT C11 <threads.h> workers (fork-join, nsieve-style
 * batching), writing into its own result slot; main then prints the slots in
 * the original fixed order. Percentages are formatted with integer arithmetic
 * (round half up) rather than printf %f so the bytes are identical across every
 * toolchain, and the per-slot combine makes -DNT=1 and -DNT=4 byte-identical.
 *
 * The k-mers are packed 2 bits per base into an integer key and counted in a
 * simple open-addressing (linear-probing) hash table that quadruples on load.
 *
 * Work scale: THREE length is 5 * N * KNUC_FASTA_SCALE bases (N = argv[1],
 * default 2000). KNUC_FASTA_SCALE is tuned so N=2000 runs in a few tens of ms.
 */
#include <threads.h>

extern void *malloc(__SIZE_TYPE__);
extern void free(void *);
extern int atoi(const char *);
extern int printf(const char *, ...);

#ifndef NT
#define NT 4
#endif

#ifndef KNUC_FASTA_SCALE
#define KNUC_FASTA_SCALE 200
#endif

static const double CUM[4] = {
	0.3029549426680,
	0.5009432431601,
	0.6984905497992,
	1.0
};

struct hcell {
	long long key;
	long val;
};

struct ht {
	long cap;
	long mask;
	long limit;
	long count;
	struct hcell *cells;
};

static struct ht *ht_new(long cap) {
	struct ht *h = malloc(sizeof(struct ht));
	h->cap = cap;
	h->mask = cap - 1;
	h->limit = cap * 3 / 4;
	h->count = 0;
	h->cells = malloc((__SIZE_TYPE__)cap * sizeof(struct hcell));
	for (long i = 0; i < cap; i++) {
		h->cells[i].key = -1;
		h->cells[i].val = 0;
	}
	return h;
}

static void ht_free(struct ht *h) {
	free(h->cells);
	free(h);
}

static long ht_index(long long key, long mask) {
	unsigned long long h = (unsigned long long)key;
	h ^= h >> 33;
	h *= 0xff51afd7ed558ccdULL;
	h ^= h >> 33;
	return (long)(h & (unsigned long long)mask);
}

static void ht_grow(struct ht *h) {
	long old_cap = h->cap;
	struct hcell *old = h->cells;
	long new_cap = old_cap * 4;
	struct hcell *nc = malloc((__SIZE_TYPE__)new_cap * sizeof(struct hcell));
	for (long i = 0; i < new_cap; i++) {
		nc[i].key = -1;
		nc[i].val = 0;
	}
	long new_mask = new_cap - 1;
	for (long i = 0; i < old_cap; i++) {
		if (old[i].key >= 0) {
			long idx = ht_index(old[i].key, new_mask);
			while (nc[idx].key >= 0)
				idx = (idx + 1) & new_mask;
			nc[idx] = old[i];
		}
	}
	free(old);
	h->cells = nc;
	h->cap = new_cap;
	h->mask = new_mask;
	h->limit = new_cap * 3 / 4;
}

static void ht_add(struct ht *h, long long key) {
	long idx = ht_index(key, h->mask);
	struct hcell *c = h->cells;
	while (c[idx].key != key) {
		if (c[idx].key < 0) {
			if (h->count >= h->limit) {
				ht_grow(h);
				ht_add(h, key);
				return;
			}
			c[idx].key = key;
			c[idx].val = 0;
			h->count++;
			break;
		}
		idx = (idx + 1) & h->mask;
	}
	c[idx].val++;
}

static long ht_get(struct ht *h, long long key) {
	long idx = ht_index(key, h->mask);
	struct hcell *c = h->cells;
	while (c[idx].key != key) {
		if (c[idx].key < 0)
			return 0;
		idx = (idx + 1) & h->mask;
	}
	return c[idx].val;
}

static struct ht *count_kmers(const unsigned char *seq, long len, int klen) {
	struct ht *h = ht_new(256);
	unsigned long long mask = (((unsigned long long)1 << (2 * klen)) - 1);
	long long code = 0;
	for (long i = 0; i < len; i++) {
		code = ((code << 2) | seq[i]) & (long long)mask;
		if (i >= klen - 1)
			ht_add(h, code);
	}
	return h;
}

static int code_of(char ch) {
	switch (ch) {
	case 'A': return 0;
	case 'C': return 1;
	case 'G': return 2;
	case 'T': return 3;
	}
	return 0;
}

struct job {
	int klen;
	int is_count;
	const char *pat;
	const unsigned char *seq;
	long len;
	long total;
	long distinct;
	long long *keys;
	long *vals;
	long count;
};

static int worker(void *arg) {
	struct job *j = arg;
	struct ht *h = count_kmers(j->seq, j->len, j->klen);
	if (j->is_count) {
		long long key = 0;
		for (int i = 0; j->pat[i]; i++)
			key = (key << 2) | code_of(j->pat[i]);
		j->count = ht_get(h, key);
	} else {
		long m = h->count;
		long long *keys = malloc((__SIZE_TYPE__)m * sizeof(long long));
		long *vals = malloc((__SIZE_TYPE__)m * sizeof(long));
		long idx = 0, total = 0;
		for (long i = 0; i < h->cap; i++) {
			if (h->cells[i].key >= 0) {
				keys[idx] = h->cells[i].key;
				vals[idx] = h->cells[i].val;
				total += h->cells[i].val;
				idx++;
			}
		}
		for (long a = 1; a < idx; a++) {
			long long kk = keys[a];
			long vv = vals[a];
			long b = a - 1;
			while (b >= 0 && (vals[b] < vv || (vals[b] == vv && keys[b] > kk))) {
				keys[b + 1] = keys[b];
				vals[b + 1] = vals[b];
				b--;
			}
			keys[b + 1] = kk;
			vals[b + 1] = vv;
		}
		j->keys = keys;
		j->vals = vals;
		j->distinct = idx;
		j->total = total;
	}
	ht_free(h);
	return 0;
}

static void print_freq(struct job *j) {
	for (long i = 0; i < j->distinct; i++) {
		char lab[8];
		long long k = j->keys[i];
		for (int p = j->klen - 1; p >= 0; p--) {
			lab[p] = "ACGT"[k & 3];
			k >>= 2;
		}
		lab[j->klen] = 0;
		unsigned long long v = (unsigned long long)j->vals[i];
		unsigned long long t = (unsigned long long)j->total;
		unsigned long long r = (200000ULL * v + t) / (2ULL * t);
		printf("%s %llu.%03llu\n", lab, r / 1000ULL, r % 1000ULL);
	}
}

int main(int argc, char **argv) {
	int n = argc > 1 ? atoi(argv[1]) : 2000;
	if (n < 1)
		n = 1;
	long fn = (long)n * KNUC_FASTA_SCALE;
	long len = 5 * fn;

	unsigned char *seq = malloc((__SIZE_TYPE__)len);
	unsigned lcg = 42;
	for (long i = 0; i < 3 * fn; i++)
		lcg = (lcg * 3877u + 29573u) % 139968u;
	for (long i = 0; i < len; i++) {
		lcg = (lcg * 3877u + 29573u) % 139968u;
		double x = (double)lcg / 139968.0;
		int c = 0;
		while (c < 3 && CUM[c] <= x)
			c++;
		seq[i] = (unsigned char)c;
	}

	enum { NJOB = 7 };
	struct job jobs[NJOB];
	static const char *pats[NJOB] = {
		0, 0, "GGT", "GGTA", "GGTATT", "GGTATTTTAATT", "GGTATTTTAATTTATAGT"
	};
	static const int klens[NJOB] = { 1, 2, 3, 4, 6, 12, 18 };
	static const int iscnt[NJOB] = { 0, 0, 1, 1, 1, 1, 1 };
	for (int i = 0; i < NJOB; i++) {
		jobs[i].klen = klens[i];
		jobs[i].is_count = iscnt[i];
		jobs[i].pat = pats[i];
		jobs[i].seq = seq;
		jobs[i].len = len;
		jobs[i].keys = 0;
		jobs[i].vals = 0;
	}

	thrd_t th[NJOB];
	int t = 0;
	while (t < NJOB) {
		int batch = NT < NJOB - t ? NT : NJOB - t;
		if (batch < 1)
			batch = 1;
		for (int k = 0; k < batch; k++)
			thrd_create(&th[t + k], worker, &jobs[t + k]);
		for (int k = 0; k < batch; k++)
			thrd_join(th[t + k], 0);
		t += batch;
	}

	print_freq(&jobs[0]);
	printf("\n");
	print_freq(&jobs[1]);
	printf("\n");
	for (int i = 2; i < NJOB; i++)
		printf("%ld\t%s\n", jobs[i].count, jobs[i].pat);

	for (int i = 0; i < NJOB; i++) {
		free(jobs[i].keys);
		free(jobs[i].vals);
	}
	free(seq);
	return 0;
}
