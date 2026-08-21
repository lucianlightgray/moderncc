/*
 * binary-trees, fork-join threading.
 *
 * Ported from The Computer Language Benchmarks Game binary-trees problem
 * (vendor/plb/bench/algorithm/binarytrees/2.c by Jeremy Zerfas). The original
 * uses the Apache Portable Runtime memory pools and OpenMP; this self-contained
 * port drops APR entirely (plain malloc/free per node, recursive free) and
 * replaces the OpenMP regions with C11 <threads.h>.
 *
 * The independent phases are: the stretch tree (depth max+1), the long-lived
 * tree (depth max), and one bounded-allocation job per depth in
 * {min, min+2, ..., max}. These jobs are packed into a fixed array and handed
 * to NT workers round-robin; each worker writes its results into the job's own
 * slot, so main() reduces/prints them in the original depth order and the
 * output is byte-identical for any NT (serial -DNT=1 included).
 *
 * Work scale: argv[1] = N (run.sh default 2000). binary-trees cost is 2^depth,
 * so N maps to a max depth = 10 + N/300, clamped to [6, 16]; the default N=2000
 * gives depth 16, runtime comparable to the spectral-norm kernels.
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

struct tree_node {
	struct tree_node *left;
	struct tree_node *right;
};

static struct tree_node *create_tree(int depth) {
	struct tree_node *node = malloc(sizeof(struct tree_node));
	if (depth > 0) {
		node->left = create_tree(depth - 1);
		node->right = create_tree(depth - 1);
	} else {
		node->left = 0;
		node->right = 0;
	}
	return node;
}

static long check_tree(const struct tree_node *node) {
	if (node->left && node->right)
		return check_tree(node->left) + check_tree(node->right) + 1;
	return 1;
}

static void free_tree(struct tree_node *node) {
	if (node->left)
		free_tree(node->left);
	if (node->right)
		free_tree(node->right);
	free(node);
}

enum { KIND_STRETCH, KIND_DEPTH, KIND_LONGLIVED };

struct job {
	int kind;
	int depth;
	long iterations;
	long result;
};

static void run_job(struct job *j) {
	if (j->kind == KIND_DEPTH) {
		long sum = 0;
		for (long i = 0; i < j->iterations; i++) {
			struct tree_node *t = create_tree(j->depth);
			sum += check_tree(t);
			free_tree(t);
		}
		j->result = sum;
	} else {
		struct tree_node *t = create_tree(j->depth);
		j->result = check_tree(t);
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

	printf("stretch tree of depth %d\t check: %ld\n", jobs[0].depth,
	       jobs[0].result);
	for (int d = MIN_DEPTH, i = 0; d <= max_depth; d += 2, i++) {
		struct job *j = &jobs[2 + i];
		printf("%ld\t trees of depth %d\t check: %ld\n", j->iterations, d,
		       j->result);
	}
	printf("long lived tree of depth %d\t check: %ld\n", jobs[1].depth,
	       jobs[1].result);
	return 0;
}
