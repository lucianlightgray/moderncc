/*
 * merkle-trees, fork-join threading.
 *
 * Ported from The Computer Language Benchmarks Game merkletrees problem
 * (vendor/plb/bench/algorithm/merkletrees/, e.g. 1.go): build a full binary
 * tree, compute and cache each node's hash bottom-up, print root-hash stats,
 * and verify (Check) that every node holds a hash. The reference "hash" is a
 * plain integer sum; this port keeps it self-contained but uses a real 64-bit
 * integer mix (NO crypto library) so the hashing is non-trivial: a leaf hashes
 * its value, an interior node hashes the mix of its two child hashes.
 *
 * The independent phases are: the stretch tree (depth max+1), the long-lived
 * tree (depth max), and one iterated-build job per depth in
 * {min, min+2, ..., max}. These jobs are packed into a fixed array and handed
 * to NT C11 <threads.h> workers round-robin; each worker builds/hashes/checks
 * its trees and writes results into the job's own slot, so main() prints them
 * in the original depth order and the output is byte-identical for any NT
 * (serial -DNT=1 included).
 *
 * Work scale: argv[1] = N (run.sh default 2000). Cost is 2^depth, so N maps to
 * a max depth = 10 + N/300, clamped to [6, 16]; the default N=2000 gives depth
 * 16, runtime comparable to the spectral-norm kernels.
 *
 * <threads.h> must lead, and libc functions are declared by hand rather than
 * #included: mcc's cooperative backend (-DMCC_THREADS_COOP) defines its own
 * once_flag, which collides with the one glibc's headers pull in.
 */
#include <threads.h>

extern void *malloc(__SIZE_TYPE__);
extern void free(void *);
extern int atoi(const char *);
extern int printf(const char *, ...);

#ifndef NT
#define NT 4
#endif

#define MIN_DEPTH 4

struct node {
	struct node *left;
	struct node *right;
	unsigned long long hash;
	int has_hash;
	long long value;
};

static unsigned long long mixh(unsigned long long a, unsigned long long b) {
	unsigned long long x =
	    a * 0x100000001b3ULL + b * 0x9e3779b97f4a7c15ULL + 0xd1b54a32d192ed03ULL;
	x ^= x >> 33;
	x *= 0xff51afd7ed558ccdULL;
	x ^= x >> 29;
	x *= 0xc4ceb9fe1a85ec53ULL;
	x ^= x >> 32;
	return x;
}

static struct node *make_tree(int depth) {
	struct node *n = malloc(sizeof(struct node));
	n->has_hash = 0;
	n->hash = 0;
	if (depth <= 0) {
		n->left = 0;
		n->right = 0;
		n->value = 1;
	} else {
		n->left = make_tree(depth - 1);
		n->right = make_tree(depth - 1);
		n->value = 0;
	}
	return n;
}

static void cal_hash(struct node *n) {
	if (n->has_hash)
		return;
	if (!n->left) {
		n->hash = mixh((unsigned long long)n->value, 0);
	} else {
		cal_hash(n->left);
		cal_hash(n->right);
		n->hash = mixh(n->left->hash, n->right->hash);
	}
	n->has_hash = 1;
}

static int check_tree(const struct node *n) {
	if (!n->has_hash)
		return 0;
	if (!n->left)
		return 1;
	return check_tree(n->left) && check_tree(n->right);
}

static void free_tree(struct node *n) {
	if (n->left)
		free_tree(n->left);
	if (n->right)
		free_tree(n->right);
	free(n);
}

enum { KIND_STRETCH, KIND_DEPTH, KIND_LONGLIVED };

struct job {
	int kind;
	int depth;
	long iterations;
	unsigned long long root_hash;
	unsigned long long hash_sum;
	int ok;
};

static void run_job(struct job *j) {
	if (j->kind == KIND_DEPTH) {
		unsigned long long sum = 0;
		for (long i = 0; i < j->iterations; i++) {
			struct node *t = make_tree(j->depth);
			cal_hash(t);
			sum += t->hash;
			free_tree(t);
		}
		j->hash_sum = sum;
	} else {
		struct node *t = make_tree(j->depth);
		cal_hash(t);
		j->root_hash = t->hash;
		j->ok = check_tree(t);
		free_tree(t);
	}
}

struct worker_arg {
	struct job *jobs;
	int njobs;
	int wid;
	int nt;
};

static int worker(void *arg) {
	struct worker_arg *w = arg;
	for (int i = w->wid; i < w->njobs; i += w->nt)
		run_job(&w->jobs[i]);
	return 0;
}

int main(int argc, char **argv) {
	int N = argc > 1 ? atoi(argv[1]) : 2000;
	int max_depth = 10 + N / 300;
	if (max_depth > 16)
		max_depth = 16;
	if (max_depth < MIN_DEPTH + 2)
		max_depth = MIN_DEPTH + 2;

	int ndepths = (max_depth - MIN_DEPTH) / 2 + 1;
	int njobs = ndepths + 2;
	struct job jobs[64];

	jobs[0].kind = KIND_STRETCH;
	jobs[0].depth = max_depth + 1;
	jobs[0].iterations = 1;
	jobs[1].kind = KIND_LONGLIVED;
	jobs[1].depth = max_depth;
	jobs[1].iterations = 1;
	for (int d = MIN_DEPTH, i = 0; d <= max_depth; d += 2, i++) {
		struct job *j = &jobs[2 + i];
		j->kind = KIND_DEPTH;
		j->depth = d;
		j->iterations = (long)1 << (max_depth - d + MIN_DEPTH);
	}

	int nt = NT < njobs ? NT : njobs;
	if (nt < 1)
		nt = 1;
	struct worker_arg wargs[NT];
	thrd_t th[NT];
	int nj = 0;
	for (int k = 0; k < nt; k++) {
		wargs[k].jobs = jobs;
		wargs[k].njobs = njobs;
		wargs[k].wid = k;
		wargs[k].nt = nt;
		if (thrd_create(&th[nj], worker, &wargs[k]) == thrd_success)
			nj++;
		else
			worker(&wargs[k]);
	}
	for (int k = 0; k < nj; k++)
		thrd_join(th[k], 0);

	printf("stretch tree of depth %d\t root hash: %llu check: %s\n",
	       jobs[0].depth, jobs[0].root_hash, jobs[0].ok ? "true" : "false");
	for (int d = MIN_DEPTH, i = 0; d <= max_depth; d += 2, i++) {
		struct job *j = &jobs[2 + i];
		printf("%ld\t trees of depth %d\t root hash sum: %llu\n", j->iterations,
		       d, j->hash_sum);
	}
	printf("long lived tree of depth %d\t root hash: %llu check: %s\n",
	       jobs[1].depth, jobs[1].root_hash, jobs[1].ok ? "true" : "false");
	return 0;
}
