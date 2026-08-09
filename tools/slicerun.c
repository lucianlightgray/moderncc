#include "mcc.h"
#include "mccast.h"

ST_INLN int is_float(int t) {
	int bt = t & VT_BTYPE;
	return bt == VT_FLOAT || bt == VT_DOUBLE || bt == VT_LDOUBLE;
}

static int ast_bad_type(int tt) {
	int bt = tt & VT_BTYPE;
	return bt == VT_STRUCT || bt == VT_FUNC || tt == VT_VOID;
}

#include "ast_eval_slice.h"

#undef malloc
#undef realloc
#undef free
#undef strdup

#ifndef AST_EVAL_SLICE_PROVIDED
#error "slicerun needs the real ast_eval_slice; the mccast.c fallback stub returns 1 without writing *out, so the CPU runner would be comparing uninitialised memory against the device and every cell would pass."
#endif

#define MCC_GPU_EMITTER 1
#include "mccgpu.h"

#define MCC_SLICE_GPU 1
#include "mcctask.h"
#include "mccslice.h"
#include "slice_inline.h"

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <stdlib.h>

ST_FUNC void *host_dlopen(const char *name) {
	return dlopen(name, RTLD_GLOBAL | RTLD_LAZY);
}

ST_FUNC const char *host_dlerror(void) { return dlerror(); }

ST_FUNC void *host_dlsym(void *h, const char *symbol) {
	return dlsym(h, symbol);
}

ST_FUNC void *host_dlsym_process(const char *symbol) {
	return dlsym(RTLD_DEFAULT, symbol);
}

static int g_failures;
static int g_checks;

#define CHECK(cond, msg)                                            \
	do {                                                              \
		g_checks++;                                                     \
		if (!(cond)) {                                                  \
			fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, msg); \
			g_failures++;                                                 \
		}                                                               \
	} while (0)

/* The device is optional on a developer box and mandatory on a cell whose whole
 * purpose is to exercise it. Mirrors MCC_GPU_REQUIRED, and the zero-dispatch
 * tooth below is what stops "no device" from masquerading as "all green". */
static int g_have_device;
static int g_device_required;
static int g_device_or_skip;
static int g_mutate;
static char g_devname[256];

/* B1's object table: the byte extent and element type of every node, indexed by
 * node id, which is what the arena dump's 13th and 14th columns carry. It is a
 * hook rather than an arena field on purpose -- the compiler itself never runs
 * frames, so leaving the hook NULL there keeps every runtime-index shape
 * refused rather than resolved against information it does not have. One arena
 * is live at a time on both the suite and the corpus path, so a flat table set
 * before the scan and cleared after is the whole mechanism. */
static int32_t *g_obj_ext;
static int *g_obj_ety;
static long g_obj_n;

static int slicerun_obj(AstArena *a, AstLocal n, int32_t *extent, int *etype) {
	(void)a;
	if (!g_obj_ext || (long)n >= g_obj_n)
		return 0;
	if (g_obj_ext[n] <= 0 || !g_obj_ety[n])
		return 0;
	*extent = g_obj_ext[n];
	*etype = g_obj_ety[n];
	return 1;
}

static void slicerun_obj_reset(long n) {
	free(g_obj_ext);
	free(g_obj_ety);
	g_obj_ext = n > 0 ? (int32_t *)calloc((size_t)n, sizeof *g_obj_ext) : NULL;
	g_obj_ety = n > 0 ? (int *)calloc((size_t)n, sizeof *g_obj_ety) : NULL;
	g_obj_n = (g_obj_ext && g_obj_ety) ? n : 0;
}

static AstLocal mk_lit(AstArena *a, int64_t v, int type) {
	AstLocal n = ast_node(a, AST_Literal);
	ast_set_op(a, n, VT_CONST);
	ast_set_type(a, n, type, 0);
	ast_set_ival(a, n, (uint64_t)v);
	return n;
}

static AstLocal mk_ref(AstArena *a, int32_t off, int type) {
	AstLocal n = ast_node(a, AST_Ref);
	ast_set_op(a, n, VT_LOCAL);
	ast_set_type(a, n, type, 0);
	ast_set_ival(a, n, (uint64_t)(int64_t)off);
	return n;
}

static AstLocal mk_bin(AstArena *a, int op, AstLocal l, AstLocal r, int type) {
	AstLocal n = ast_node(a, AST_Binary);
	ast_set_op(a, n, op);
	ast_set_type(a, n, type, 0);
	ast_add_child(a, n, l);
	ast_add_child(a, n, r);
	return n;
}

/* 3 * x(-8) + x(-16) -- two live-ins, first encountered at -8. */
static AstLocal build_affine(AstArena *a) {
	return mk_bin(a, '+',
								mk_bin(a, '*', mk_ref(a, -8, VT_INT), mk_lit(a, 3, VT_INT),
											 VT_INT),
								mk_ref(a, -16, VT_INT), VT_INT);
}

/* ---------------------------------------------------------------- tasks -- */

typedef struct CountCtx {
	int limit;
	int seen;
	int order[16];
	int *log;
	int *logn;
	int id;
} CountCtx;

static int count_tick(MccTask *t) {
	CountCtx *c = (CountCtx *)t->ctx;
	if (c->log && *c->logn < 16)
		c->log[(*c->logn)++] = c->id;
	c->seen++;
	if (c->seen >= c->limit)
		return MCC_TASK_DONE;
	return MCC_TASK_YIELDED;
}

static void suite_task(void) {
	MccSched s;
	MccTask t1, t2;
	CountCtx c1, c2;
	int log[16], logn = 0;

	memset(&c1, 0, sizeof c1);
	c1.limit = 4;
	mcc_sched_init(&s);
	mcc_task_init(&t1, count_tick, &c1);
	mcc_sched_add(&s, &t1);

	CHECK(mcc_sched_pending(&s) == 1, "one task queued is one pending");
	CHECK(mcc_sched_run(&s, 0) == 0, "run to completion reports no task left");
	CHECK(t1.state == MCC_TASK_DONE, "the task reached DONE");
	CHECK(t1.ticks == 4, "a task yielding three times is ticked exactly four times");
	CHECK(c1.seen == 4, "the task body ran once per tick");
	CHECK(mcc_sched_pending(&s) == 0, "nothing is pending after completion");

	memset(&c1, 0, sizeof c1);
	memset(&c2, 0, sizeof c2);
	c1.limit = 3;
	c1.id = 1;
	c1.log = log;
	c1.logn = &logn;
	c2.limit = 3;
	c2.id = 2;
	c2.log = log;
	c2.logn = &logn;
	mcc_sched_init(&s);
	mcc_task_init(&t1, count_tick, &c1);
	mcc_task_init(&t2, count_tick, &c2);
	mcc_sched_add(&s, &t1);
	mcc_sched_add(&s, &t2);
	mcc_sched_run(&s, 0);
	CHECK(logn == 6, "two three-tick tasks tick six times in total");
	CHECK(log[0] == 1 && log[1] == 2 && log[2] == 1 && log[3] == 2 &&
					log[4] == 1 && log[5] == 2,
				"the scheduler round-robins rather than draining one task first");

	/* The quit flag is the whole point of a tick: it is observed between ticks,
	 * so a stop is exact and leaves resumable state rather than a killed thread
	 * mid-compile. This is L2' in miniature. */
	memset(&c1, 0, sizeof c1);
	memset(&c2, 0, sizeof c2);
	c1.limit = 5;
	c2.limit = 5;
	mcc_sched_init(&s);
	mcc_task_init(&t1, count_tick, &c1);
	mcc_task_init(&t2, count_tick, &c2);
	mcc_sched_add(&s, &t1);
	mcc_sched_add(&s, &t2);
	CHECK(mcc_sched_run(&s, 2) == 2, "a two-round budget leaves both tasks pending");
	CHECK(c1.seen == 2 && c2.seen == 2, "each task advanced exactly two ticks");
	CHECK(mcc_sched_pending(&s) == 2, "both tasks are still queued");
	mcc_sched_quit(&s);
	CHECK(mcc_sched_run(&s, 0) == 2, "a quit scheduler runs nothing and reports both");
	CHECK(c1.seen == 2, "quit is observed before the next tick, not during one");
	s.quit = 0;
	CHECK(mcc_sched_run(&s, 0) == 0, "resuming after quit drains the queue");
	CHECK(c1.seen == 5 && c2.seen == 5, "both tasks completed from where they stopped");
}

/* ----------------------------------------------------------- work items -- */

static void suite_work(void) {
	AstArena *a = ast_arena_new();
	AstLocal root = build_affine(a);
	MccSliceWork w;

	CHECK(mcc_slice_work_from_ast(a, root, &w) == 1, "an affine slice yields work");
	CHECK(w.nlive == 2, "the slice has two live-ins");
	CHECK(w.off[0] == -8 && w.off[1] == -16,
				"live-in offsets are in first-encounter order, which is the kernel ABI");
	CHECK(w.nodes == 5, "the work item records the slice node count");
	CHECK(w.root == root && w.a == a, "the work item points at the slice it came from");

	{
		AstLocal store = ast_node(a, AST_StoreVal);
		ast_add_child(a, store, mk_ref(a, -24, VT_INT));
		CHECK(mcc_slice_work_from_ast(a, store, &w) == 0,
					"a slice containing a store is not schedulable work");
	}
	{
		AstLocal f = mk_bin(a, '+', mk_ref(a, -32, VT_DOUBLE),
												mk_ref(a, -40, VT_DOUBLE), VT_DOUBLE);
		CHECK(mcc_slice_work_from_ast(a, f, &w) == 0,
					"a float slice is not schedulable work");
	}
	{
		AstLocal wide = mk_ref(a, -8, VT_INT);
		int i;
		for (i = 0; i < MCC_SLICE_MAXLIVE + 2; i++)
			wide = mk_bin(a, '+', wide, mk_ref(a, (int32_t)(-64 - 8 * i), VT_INT),
										VT_INT);
		CHECK(mcc_slice_work_from_ast(a, wide, &w) == 0,
					"a slice with more live-ins than the ABI carries is refused");
	}
	CHECK(mcc_slice_work_from_ast(a, AST_NONE, &w) == 0, "no node is no work");
	CHECK(mcc_slice_work_from_ast(NULL, root, &w) == 0, "no arena is no work");
	CHECK(mcc_slice_work_from_ast(a, root, NULL) == 0, "no sink is no work");

	ast_arena_free(a);
}

/* --------------------------------------------------------- the CPU runner -- */

static const int64_t AFFINE_IN[] = {1, 2, 5, 7, -3, 4, 0, 0, 100, -1};
static const int64_t AFFINE_EXPECT[] = {5, 22, -5, 0, 299};
#define AFFINE_N 5

static void suite_cpu(void) {
	AstArena *a = ast_arena_new();
	AstLocal root = build_affine(a);
	MccSliceWork w;
	int64_t out[AFFINE_N];
	unsigned char def[AFFINE_N];
	int i;

	CHECK(mcc_slice_work_from_ast(a, root, &w) == 1, "affine slice yields work");
	mcc_slice_work_bind(&w, AFFINE_IN, AFFINE_N, out, def);

	CHECK(mcc_slice_run_cpu(&w, 0) == MCC_TASK_DONE,
				"an unbudgeted CPU run completes in one tick");
	CHECK(w.done == AFFINE_N, "every tuple was evaluated");
	for (i = 0; i < AFFINE_N; i++) {
		CHECK(def[i] == 1, "each affine tuple is defined");
		CHECK(out[i] == AFFINE_EXPECT[i], "3*x0 + x1 has the expected value");
	}

	/* Resumability: the same batch, one tuple per tick. The values must not
	 * depend on how the work was carved up, or a step budget (C3b) changes
	 * results and the whole scheduling story is unsound. */
	memset(out, 0, sizeof out);
	memset(def, 0, sizeof def);
	mcc_slice_work_bind(&w, AFFINE_IN, AFFINE_N, out, def);
	for (i = 0; i < AFFINE_N - 1; i++)
		CHECK(mcc_slice_run_cpu(&w, 1) == MCC_TASK_YIELDED,
					"a budgeted CPU run yields with tuples outstanding");
	CHECK(mcc_slice_run_cpu(&w, 1) == MCC_TASK_DONE, "the last tuple completes it");
	CHECK(w.done == AFFINE_N, "a resumed run covers every tuple exactly once");
	for (i = 0; i < AFFINE_N; i++)
		CHECK(out[i] == AFFINE_EXPECT[i],
					"tick-by-tick results equal single-shot results");

	ast_arena_free(a);

	/* Undefinedness is a result, not an error: the runner must report it per
	 * tuple so a lane the device could not define is distinguishable from a
	 * lane that legitimately produced zero. */
	a = ast_arena_new();
	{
		AstLocal div = mk_bin(a, '/', mk_ref(a, -8, VT_INT), mk_ref(a, -16, VT_INT),
													VT_INT);
		static const int64_t din[] = {12, 4, 7, 0, -8, 2};
		int64_t dout[3];
		unsigned char ddef[3];
		CHECK(mcc_slice_work_from_ast(a, div, &w) == 1, "a division slice is work");
		mcc_slice_work_bind(&w, din, 3, dout, ddef);
		CHECK(mcc_slice_run_cpu(&w, 0) == MCC_TASK_DONE, "the division batch runs");
		CHECK(ddef[0] == 1 && dout[0] == 3, "12/4 is defined and is 3");
		CHECK(ddef[1] == 0, "division by zero is reported undefined, not trapped");
		CHECK(ddef[2] == 1 && dout[2] == -4, "-8/2 is defined and is -4");
	}
	ast_arena_free(a);
}

/* --------------------------------------------------------- the GPU runner -- */

static void suite_gpu(void) {
	AstArena *a;
	AstLocal root;
	MccSliceWork w;
	MccSliceKernel k;
	int64_t cout[AFFINE_N], gout[AFFINE_N];
	unsigned char cdef[AFFINE_N], gdef[AFFINE_N];
	int i;

	if (!g_have_device) {
		if (g_device_required) {
			fprintf(stderr, "FAIL slicerun: no usable device but a device is required\n");
			g_failures++;
		}
		return;
	}

	a = ast_arena_new();
	root = build_affine(a);
	CHECK(mcc_slice_work_from_ast(a, root, &w) == 1, "affine slice yields work");

	CHECK(mcc_slice_kernel_build(&w, &k) == 1, "the slice lowers to a device kernel");
	CHECK(k.code.p != NULL && k.code.n > 0, "the kernel carries emitted code");
	CHECK(k.nlive == w.nlive, "the kernel records the live-in count it was built for");
	CHECK(k.key != 0, "the kernel carries a cache key");

	mcc_slice_work_bind(&w, AFFINE_IN, AFFINE_N, cout, cdef);
	CHECK(mcc_slice_run_cpu(&w, 0) == MCC_TASK_DONE, "CPU reference runs");

	mcc_slice_work_bind(&w, AFFINE_IN, AFFINE_N, gout, gdef);
	CHECK(mcc_slice_run_gpu(&w, &k, 0) == MCC_TASK_DONE, "the device batch runs");
	CHECK(w.done == AFFINE_N, "the device covered every tuple");
	CHECK(mcc_slice_dispatches() > 0,
				"the device really dispatched; a zero here means this cell proves nothing");

	for (i = 0; i < AFFINE_N; i++) {
		CHECK(gdef[i] == cdef[i], "device and CPU agree on definedness per lane");
		CHECK(gout[i] == cout[i], "device and CPU agree on the value per lane");
		CHECK(gout[i] == AFFINE_EXPECT[i], "the device produced the expected value");
	}

	/* A second run of the same work must reuse the built kernel rather than
	 * re-emitting: S6's amortization is the difference between a bid that can
	 * win and one that cannot. */
	{
		long before = mcc_slice_emits();
		mcc_slice_work_bind(&w, AFFINE_IN, AFFINE_N, gout, gdef);
		CHECK(mcc_slice_run_gpu(&w, &k, 0) == MCC_TASK_DONE, "the kernel re-runs");
		CHECK(mcc_slice_emits() == before, "re-running a built kernel does not re-emit");
	}

	mcc_slice_kernel_free(&k);
	ast_arena_free(a);

	/* Undefinedness must cross the boundary intact, or a lane the device could
	 * not define reads back as a plausible wrong value. */
	a = ast_arena_new();
	{
		AstLocal div = mk_bin(a, '/', mk_ref(a, -8, VT_INT), mk_ref(a, -16, VT_INT),
													VT_INT);
		static const int64_t din[] = {12, 4, 7, 0, -8, 2};
		int64_t dc[3], dg[3];
		unsigned char dcd[3], dgd[3];
		MccSliceKernel dk;
		CHECK(mcc_slice_work_from_ast(a, div, &w) == 1, "a division slice is work");
		CHECK(mcc_slice_kernel_build(&w, &dk) == 1, "the division slice lowers");
		mcc_slice_work_bind(&w, din, 3, dc, dcd);
		mcc_slice_run_cpu(&w, 0);
		mcc_slice_work_bind(&w, din, 3, dg, dgd);
		CHECK(mcc_slice_run_gpu(&w, &dk, 0) == MCC_TASK_DONE, "the division batch runs");
		CHECK(dgd[1] == 0, "the device reports division by zero as undefined");
		CHECK(dgd[0] == dcd[0] && dgd[1] == dcd[1] && dgd[2] == dcd[2],
					"definedness agrees on every lane");
		CHECK(dg[0] == dc[0] && dg[2] == dc[2], "defined lanes agree in value");
		mcc_slice_kernel_free(&dk);
	}
	ast_arena_free(a);
}

/* ------------------------------------------------- 64-bit fidelity -------- */

#define VT_LL VT_LLONG
#define VT_ULL (VT_LLONG | VT_UNSIGNED)
#define VT_UINT (VT_INT | VT_UNSIGNED)

/* Full-width values, not rung-derived ones. Every 64-bit value the emitters see
 * today comes from spvgate, which builds inputs by shifting a 1..16-bit rung
 * value up by a constant -- so the lo/hi boundary is exercised only in the
 * patterns that construction happens to produce. These are the values that
 * break pair emulation: the 2^32 carry boundary in both directions, a low word
 * of all ones, the two extremes, and bit patterns with nothing in common
 * between the halves. */
static const int64_t W64[] = {
		0,
		1,
		-1,
		2,
		-2,
		(int64_t)0x000000007FFFFFFFll, /* INT32_MAX  */
		-(int64_t)0x0000000080000000ll, /* INT32_MIN */
		(int64_t)0x00000000FFFFFFFFll, /* low word all ones, high word clear */
		(int64_t)0x0000000100000000ll, /* 2^32: the carry boundary itself */
		(int64_t)0x0000000100000001ll,
		-(int64_t)0x0000000100000000ll,
		(int64_t)0x7FFFFFFFFFFFFFFFll, /* INT64_MAX */
		(int64_t)0x8000000000000000ull, /* INT64_MIN */
		(int64_t)0x123456789ABCDEF0ll,
		-(int64_t)0x123456789ABCDEF0ll,
		(int64_t)0xDEADBEEFCAFEBABEull};
#define W64_N ((int)(sizeof W64 / sizeof W64[0]))

static const int64_t W64_SHIFT[] = {0, 1, 31, 32, 33, 63};
#define W64_SHIFT_N ((int)(sizeof W64_SHIFT / sizeof W64_SHIFT[0]))

/* Ops actually exercised by a differential, so a case that silently stopped
 * running is a failed assertion rather than a quietly narrower matrix. N9 names
 * six opcodes with an MSL arm, a SPIR-V arm and a CPU arm and no coverage at
 * all; they are structurally unreachable from harvested arenas because gen_op
 * rewrites TOK_GE into TOK_UGE after the arena has already recorded the token,
 * so enumeration is the only way to reach them. */
static unsigned char g_op_seen[256];
static long g_op_tuples;

/* One binary op over a full cross product of hard values, CPU against device,
 * value and definedness both. */
static void w64_binop(const char *what, int op, int type, const int64_t *lhs,
											int nl, const int64_t *rhs, int nr) {
	AstArena *a = ast_arena_new();
	AstLocal root = mk_bin(a, op, mk_ref(a, -8, type), mk_ref(a, -16, type), type);
	MccSliceWork w;
	MccSliceKernel k;
	int64_t *in, *cout, *gout;
	unsigned char *cdef, *gdef;
	int n = nl * nr, i, j, bad = 0, cmp = 0, vac = 0;

	in = (int64_t *)malloc((size_t)n * 2 * sizeof *in);
	cout = (int64_t *)malloc((size_t)n * sizeof *cout);
	gout = (int64_t *)malloc((size_t)n * sizeof *gout);
	cdef = (unsigned char *)malloc((size_t)n);
	gdef = (unsigned char *)malloc((size_t)n);
	if (!in || !cout || !gout || !cdef || !gdef) {
		free(in); free(cout); free(gout); free(cdef); free(gdef);
		ast_arena_free(a);
		return;
	}
	for (i = 0; i < nl; i++)
		for (j = 0; j < nr; j++) {
			in[((long)i * nr + j) * 2] = lhs[i];
			in[((long)i * nr + j) * 2 + 1] = rhs[j];
		}

	if (!mcc_slice_work_from_ast(a, root, &w)) {
		fprintf(stderr, "FAIL %s: %s is not schedulable work\n", __func__, what);
		g_failures++;
		goto out;
	}
	g_checks++;
	mcc_slice_work_bind(&w, in, n, cout, cdef);
	if (mcc_slice_run_cpu(&w, 0) != MCC_TASK_DONE) {
		fprintf(stderr, "FAIL %s: %s CPU run did not complete\n", __func__, what);
		g_failures++;
		goto out;
	}
	if (!mcc_slice_kernel_build(&w, &k)) {
		fprintf(stderr, "FAIL %s: %s did not lower to a device kernel\n", __func__,
						what);
		g_failures++;
		goto out;
	}
	mcc_slice_work_bind(&w, in, n, gout, gdef);
	if (mcc_slice_run_gpu(&w, &k, 0) != MCC_TASK_DONE) {
		fprintf(stderr, "FAIL %s: %s device run did not complete\n", __func__, what);
		g_failures++;
		mcc_slice_kernel_free(&k);
		goto out;
	}
	mcc_slice_kernel_free(&k);

	for (i = 0; i < n; i++) {
		if (gdef[i] != cdef[i]) {
			if (bad < 4)
				fprintf(stderr,
								"FAIL %s: %s definedness a=%lld b=%lld cpu=%d gpu=%d\n",
								__func__, what, (long long)in[i * 2], (long long)in[i * 2 + 1],
								cdef[i], gdef[i]);
			bad++;
			continue;
		}
		if (!cdef[i]) {
			vac++;
			continue;
		}
		cmp++;
		if (gout[i] != cout[i]) {
			if (bad < 4)
				fprintf(stderr,
								"FAIL %s: %s a=%lld b=%lld cpu=%lld gpu=%lld\n", __func__,
								what, (long long)in[i * 2], (long long)in[i * 2 + 1],
								(long long)cout[i], (long long)gout[i]);
			bad++;
		}
	}
	g_checks++;
	if (bad)
		g_failures++;
	/* A case where every tuple was undefined compared nothing and would report
	 * clean. That is the vacuity the plan flags in spvgate; refuse it here. */
	g_checks++;
	if (!cmp) {
		fprintf(stderr, "FAIL %s: %s compared no defined tuple (%d vacuous)\n",
						__func__, what, vac);
		g_failures++;
	} else {
		g_op_seen[op & 0xff] = 1;
		g_op_tuples += cmp;
	}
out:
	free(in); free(cout); free(gout); free(cdef); free(gdef);
	ast_arena_free(a);
}

static void suite_wide64(void) {
	AstArena *a;
	MccSliceWork w;
	MccSliceKernel k;
	int64_t out[W64_N];
	unsigned char def[W64_N];
	int i;

	if (!g_have_device) {
		if (g_device_required) {
			fprintf(stderr, "FAIL slicerun: no usable device but a device is required\n");
			g_failures++;
		}
		return;
	}

	/* Fidelity at its purest: a bare 64-bit live-in, straight back out. This
	 * needs no oracle -- the expected value is the input, all 64 bits of it, and
	 * it is the one test that isolates the lo/hi packing from the arithmetic. */
	a = ast_arena_new();
	{
		AstLocal id = mk_ref(a, -8, VT_LL);
		CHECK(mcc_slice_work_from_ast(a, id, &w) == 1, "a bare 64-bit ref is work");
		CHECK(w.wtype == VT_LL, "the work item carries the declared 64-bit width");
		CHECK(mcc_slice_kernel_build(&w, &k) == 1, "a 64-bit ref lowers to a kernel");
		mcc_slice_work_bind(&w, W64, W64_N, out, def);
		CHECK(mcc_slice_run_gpu(&w, &k, 0) == MCC_TASK_DONE, "the 64-bit batch runs");
		for (i = 0; i < W64_N; i++) {
			CHECK(def[i] == 1, "a 64-bit identity is always defined");
			CHECK(out[i] == W64[i],
						"a 64-bit value survives the round trip with every bit intact");
		}
		mcc_slice_kernel_free(&k);
	}
	ast_arena_free(a);

	/* Sign extension is the half of the ABI the identity test cannot see: a
	 * 32-bit live-in widened to a 64-bit result must arrive sign-extended, and
	 * an unsigned one zero-extended. This is the exact divergence that produced
	 * the carry-in-the-high-word bug on the real corpus. */
	a = ast_arena_new();
	{
		static const int64_t narrow[] = {0, 1, -1, 2147483647, -2147483648, -12345};
		int64_t nout[6];
		unsigned char ndef[6];
		AstLocal cvt = ast_node(a, AST_Convert);
		ast_set_type(a, cvt, VT_LL, 0);
		ast_add_child(a, cvt, mk_ref(a, -8, VT_INT));
		CHECK(mcc_slice_work_from_ast(a, cvt, &w) == 1, "a widening convert is work");
		CHECK(mcc_slice_kernel_build(&w, &k) == 1, "a widening convert lowers");
		mcc_slice_work_bind(&w, narrow, 6, nout, ndef);
		CHECK(mcc_slice_run_gpu(&w, &k, 0) == MCC_TASK_DONE, "the convert batch runs");
		CHECK(nout[2] == -1, "int32 -1 sign-extends to int64 -1, not to 4294967295");
		CHECK(nout[4] == -2147483648ll, "INT32_MIN sign-extends");
		CHECK(nout[5] == -12345, "a negative int32 sign-extends");
		CHECK(nout[3] == 2147483647ll, "INT32_MAX widens unchanged");
		mcc_slice_kernel_free(&k);
	}
	ast_arena_free(a);

	/* A live-in is read *through a typed Ref*, and the value it yields is that
	 * type's value -- exactly as the Literal arm right beside it already does.
	 * The device narrows per Ref because spv_load_live_v is handed the ref's own
	 * type; the CPU evaluator returned the raw environment word. So an unsigned
	 * narrow live-in holding a negative host value diverged: the device saw
	 * 4294954951 and the CPU saw -12345, and both then widened *correctly* from
	 * different starting points. Found on the real corpus, not by construction. */
	a = ast_arena_new();
	{
		static const int64_t nv[] = {-12345, -1, 7, -2147483648ll};
		int64_t co[4], go[4];
		unsigned char cd[4], gd[4];
		AstLocal cvt = ast_node(a, AST_Convert);
		ast_set_type(a, cvt, VT_ULL, 0);
		ast_add_child(a, cvt, mk_ref(a, -8, VT_UINT));
		CHECK(mcc_slice_work_from_ast(a, cvt, &w) == 1, "u32->u64 convert is work");
		CHECK(mcc_slice_kernel_build(&w, &k) == 1, "u32->u64 convert lowers");
		mcc_slice_work_bind(&w, nv, 4, co, cd);
		CHECK(mcc_slice_run_cpu(&w, 0) == MCC_TASK_DONE, "the CPU runs it");
		mcc_slice_work_bind(&w, nv, 4, go, gd);
		CHECK(mcc_slice_run_gpu(&w, &k, 0) == MCC_TASK_DONE, "the device runs it");
		CHECK(co[0] == 4294954951ll,
					"an unsigned 32-bit live-in reads as its own type, not as the raw word");
		CHECK(co[1] == 4294967295ll, "u32 -1 reads as 4294967295");
		CHECK(co[3] == 2147483648ll, "u32 INT32_MIN reads as 2147483648");
		for (i = 0; i < 4; i++)
			CHECK(go[i] == co[i], "device and CPU agree on a narrow unsigned live-in");
		mcc_slice_kernel_free(&k);
	}
	ast_arena_free(a);

	/* Truncation the other way: the high word must be discarded, not folded in. */
	a = ast_arena_new();
	{
		int64_t tout[W64_N];
		unsigned char tdef[W64_N];
		AstLocal cvt = ast_node(a, AST_Convert);
		ast_set_type(a, cvt, VT_INT, 0);
		ast_add_child(a, cvt, mk_ref(a, -8, VT_LL));
		CHECK(mcc_slice_work_from_ast(a, cvt, &w) == 1, "a narrowing convert is work");
		CHECK(mcc_slice_kernel_build(&w, &k) == 1, "a narrowing convert lowers");
		mcc_slice_work_bind(&w, W64, W64_N, tout, tdef);
		CHECK(mcc_slice_run_gpu(&w, &k, 0) == MCC_TASK_DONE, "the truncate batch runs");
		for (i = 0; i < W64_N; i++)
			CHECK(tout[i] == (int64_t)(int32_t)W64[i],
						"int64 to int32 keeps the low word and re-signs it");
		mcc_slice_kernel_free(&k);
	}
	ast_arena_free(a);

	/* Every 64-bit op the emitters claim, over the full cross product of hard
	 * values: 256 tuples each, device against CPU, value and definedness. */
	w64_binop("ll-add", '+', VT_LL, W64, W64_N, W64, W64_N);
	w64_binop("ll-sub", '-', VT_LL, W64, W64_N, W64, W64_N);
	w64_binop("ll-mul", '*', VT_LL, W64, W64_N, W64, W64_N);
	w64_binop("ll-div", '/', VT_LL, W64, W64_N, W64, W64_N);
	w64_binop("ll-mod", '%', VT_LL, W64, W64_N, W64, W64_N);
	w64_binop("ll-and", '&', VT_LL, W64, W64_N, W64, W64_N);
	w64_binop("ll-or", '|', VT_LL, W64, W64_N, W64, W64_N);
	w64_binop("ll-xor", '^', VT_LL, W64, W64_N, W64, W64_N);
	w64_binop("ll-lt", TOK_LT, VT_LL, W64, W64_N, W64, W64_N);
	w64_binop("ll-le", TOK_LE, VT_LL, W64, W64_N, W64, W64_N);
	w64_binop("ll-gt", TOK_GT, VT_LL, W64, W64_N, W64, W64_N);
	w64_binop("ll-ge", TOK_GE, VT_LL, W64, W64_N, W64, W64_N);
	w64_binop("ll-eq", TOK_EQ, VT_LL, W64, W64_N, W64, W64_N);
	w64_binop("ll-ne", TOK_NE, VT_LL, W64, W64_N, W64, W64_N);

	w64_binop("ull-add", '+', VT_ULL, W64, W64_N, W64, W64_N);
	w64_binop("ull-sub", '-', VT_ULL, W64, W64_N, W64, W64_N);
	w64_binop("ull-mul", '*', VT_ULL, W64, W64_N, W64, W64_N);
	w64_binop("ull-div", '/', VT_ULL, W64, W64_N, W64, W64_N);
	w64_binop("ull-mod", '%', VT_ULL, W64, W64_N, W64, W64_N);
	w64_binop("ull-lt", TOK_ULT, VT_ULL, W64, W64_N, W64, W64_N);
	w64_binop("ull-ge", TOK_UGE, VT_ULL, W64, W64_N, W64, W64_N);
	w64_binop("ull-le", TOK_ULE, VT_ULL, W64, W64_N, W64, W64_N);
	w64_binop("ull-gt", TOK_UGT, VT_ULL, W64, W64_N, W64, W64_N);

	/* Shifts are separated because a hard value used as a shift count is almost
	 * always out of range, and a case where every tuple is undefined proves
	 * nothing. 32 is the count that breaks a naive pair implementation. */
	w64_binop("ll-shl", TOK_SHL, VT_LL, W64, W64_N, W64_SHIFT, W64_SHIFT_N);
	w64_binop("ll-sar", TOK_SAR, VT_LL, W64, W64_N, W64_SHIFT, W64_SHIFT_N);
	w64_binop("ull-shl", TOK_SHL, VT_ULL, W64, W64_N, W64_SHIFT, W64_SHIFT_N);
	w64_binop("ull-shr", TOK_SHR, VT_ULL, W64, W64_N, W64_SHIFT, W64_SHIFT_N);
}

/* ------------------------------------ the op matrix, and N9's six opcodes -- */

static const int64_t I32[] = {0,
															1,
															-1,
															2,
															-2,
															3,
															-7,
															(int64_t)0x7FFFFFFFll,  /* INT32_MAX */
															-(int64_t)0x80000000ll, /* INT32_MIN */
															(int64_t)0x00007FFFll,
															-(int64_t)0x00008000ll,
															(int64_t)0x0000FFFFll,
															(int64_t)0x00010000ll,
															(int64_t)0x55555555ll,
															-(int64_t)0x55555555ll,
															(int64_t)0x0F0F0F0Fll};
#define I32_N ((int)(sizeof I32 / sizeof I32[0]))

static const int64_t SH32[] = {0, 1, 15, 16, 17, 31};
#define SH32_N ((int)(sizeof SH32 / sizeof SH32[0]))

typedef struct OpRow {
	const char *name;
	int op;
	int type;
	int shifty;
} OpRow;

static const OpRow OPS[] = {
		/* 32-bit signed */
		{"i32-add", '+', VT_INT, 0},
		{"i32-sub", '-', VT_INT, 0},
		{"i32-mul", '*', VT_INT, 0},
		{"i32-div", '/', VT_INT, 0},
		{"i32-mod", '%', VT_INT, 0},
		{"i32-and", '&', VT_INT, 0},
		{"i32-or", '|', VT_INT, 0},
		{"i32-xor", '^', VT_INT, 0},
		{"i32-eq", TOK_EQ, VT_INT, 0},
		{"i32-ne", TOK_NE, VT_INT, 0},
		{"i32-lt", TOK_LT, VT_INT, 0},
		{"i32-le", TOK_LE, VT_INT, 0},
		{"i32-gt", TOK_GT, VT_INT, 0},
		{"i32-ge", TOK_GE, VT_INT, 0},
		{"i32-land", TOK_LAND, VT_INT, 0},
		{"i32-lor", TOK_LOR, VT_INT, 0},
		{"i32-shl", TOK_SHL, VT_INT, 1},
		{"i32-sar", TOK_SAR, VT_INT, 1},
		/* 32-bit unsigned */
		{"u32-add", '+', VT_UINT, 0},
		{"u32-sub", '-', VT_UINT, 0},
		{"u32-mul", '*', VT_UINT, 0},
		{"u32-div", '/', VT_UINT, 0},
		{"u32-mod", '%', VT_UINT, 0},
		{"u32-lt", TOK_LT, VT_UINT, 0},
		{"u32-ge", TOK_GE, VT_UINT, 0},
		{"u32-shl", TOK_SHL, VT_UINT, 1},
		{"u32-shr", TOK_SHR, VT_UINT, 1},
		/* N9's six, at 32 bits. These are type-independent opcodes: they mean
		 * "compare/divide these as unsigned" whatever the operand type says, which
		 * is exactly why gen_op can substitute them late and why no harvested
		 * arena ever contains one. */
		{"n9-i32-udiv", TOK_UDIV, VT_INT, 0},
		{"n9-i32-umod", TOK_UMOD, VT_INT, 0},
		{"n9-i32-pdiv", TOK_PDIV, VT_INT, 0},
		{"n9-i32-uge", TOK_UGE, VT_INT, 0},
		{"n9-i32-ule", TOK_ULE, VT_INT, 0},
		{"n9-i32-ugt", TOK_UGT, VT_INT, 0},
		{"n9-i32-ult", TOK_ULT, VT_INT, 0},
		{"n9-u32-udiv", TOK_UDIV, VT_UINT, 0},
		{"n9-u32-umod", TOK_UMOD, VT_UINT, 0},
		{"n9-u32-pdiv", TOK_PDIV, VT_UINT, 0},
		{"n9-u32-uge", TOK_UGE, VT_UINT, 0},
		{"n9-u32-ule", TOK_ULE, VT_UINT, 0},
		{"n9-u32-ugt", TOK_UGT, VT_UINT, 0}};
#define OPS_N ((int)(sizeof OPS / sizeof OPS[0]))

/* The same six at 64 bits, where the pair emulation has to get the high word
 * right as well. */
static const OpRow OPS64[] = {
		{"n9-i64-udiv", TOK_UDIV, VT_LL, 0}, {"n9-i64-umod", TOK_UMOD, VT_LL, 0},
		{"n9-i64-pdiv", TOK_PDIV, VT_LL, 0}, {"n9-i64-uge", TOK_UGE, VT_LL, 0},
		{"n9-i64-ule", TOK_ULE, VT_LL, 0},   {"n9-i64-ugt", TOK_UGT, VT_LL, 0},
		{"n9-i64-ult", TOK_ULT, VT_LL, 0},   {"n9-u64-udiv", TOK_UDIV, VT_ULL, 0},
		{"n9-u64-umod", TOK_UMOD, VT_ULL, 0}, {"n9-u64-pdiv", TOK_PDIV, VT_ULL, 0},
		{"i64-land", TOK_LAND, VT_LL, 0},    {"i64-lor", TOK_LOR, VT_LL, 0}};
#define OPS64_N ((int)(sizeof OPS64 / sizeof OPS64[0]))

static void suite_ops(void) {
	static const int N9[] = {TOK_UDIV, TOK_UMOD, TOK_PDIV,
													 TOK_UGE,  TOK_ULE,  TOK_UGT};
	static const char *N9NAME[] = {"TOK_UDIV", "TOK_UMOD", "TOK_PDIV",
																 "TOK_UGE",  "TOK_ULE",  "TOK_UGT"};
	int i;

	if (!g_have_device) {
		if (g_device_required) {
			fprintf(stderr, "FAIL slicerun: no usable device but a device is required\n");
			g_failures++;
		}
		return;
	}

	for (i = 0; i < OPS_N; i++)
		w64_binop(OPS[i].name, OPS[i].op, OPS[i].type, I32, I32_N,
							OPS[i].shifty ? SH32 : I32, OPS[i].shifty ? SH32_N : I32_N);
	for (i = 0; i < OPS64_N; i++)
		w64_binop(OPS64[i].name, OPS64[i].op, OPS64[i].type, W64, W64_N, W64,
							W64_N);

	/* The ratchet. Without this the matrix can silently shrink -- a case that
	 * stops lowering just stops being compared, and the cell still reads green. */
	for (i = 0; i < 6; i++) {
		g_checks++;
		if (!g_op_seen[N9[i] & 0xff]) {
			fprintf(stderr, "FAIL %s: %s was never exercised against the device\n",
							__func__, N9NAME[i]);
			g_failures++;
		}
	}
	fprintf(stderr, "slicerun: op matrix %d rows, %ld defined tuples compared\n",
					OPS_N + OPS64_N, g_op_tuples);
}

/* ------------------------------------------- both runners, one scheduler -- */

static void suite_sched(void) {
	AstArena *a = ast_arena_new();
	AstLocal root = build_affine(a);
	MccSliceWork cw, gw;
	MccSliceKernel k;
	MccSched s;
	MccTask ct, gt;
	int64_t cout[AFFINE_N], gout[AFFINE_N];
	unsigned char cdef[AFFINE_N], gdef[AFFINE_N];
	int i;

	CHECK(mcc_slice_work_from_ast(a, root, &cw) == 1, "affine slice yields work");
	gw = cw;
	mcc_slice_work_bind(&cw, AFFINE_IN, AFFINE_N, cout, cdef);
	mcc_slice_task_cpu(&ct, &cw, 2);

	mcc_sched_init(&s);
	mcc_sched_add(&s, &ct);

	if (g_have_device && mcc_slice_kernel_build(&gw, &k)) {
		mcc_slice_work_bind(&gw, AFFINE_IN, AFFINE_N, gout, gdef);
		mcc_slice_task_gpu(&gt, &gw, &k, 0);
		mcc_sched_add(&s, &gt);
	}

	CHECK(mcc_sched_run(&s, 0) == 0, "the scheduler drains both runners");
	CHECK(ct.state == MCC_TASK_DONE, "the CPU slice task completed");
	CHECK(ct.ticks == 3, "a five-tuple batch at a budget of two takes three ticks");
	for (i = 0; i < AFFINE_N; i++)
		CHECK(cout[i] == AFFINE_EXPECT[i], "scheduled CPU results are the expected ones");

	if (g_have_device && gw.ntuple) {
		CHECK(gt.state == MCC_TASK_DONE, "the device slice task completed");
		for (i = 0; i < AFFINE_N; i++)
			CHECK(gout[i] == cout[i] && gdef[i] == cdef[i],
						"a slice scheduled to the device matches the same slice on the CPU");
		mcc_slice_kernel_free(&k);
	}

	ast_arena_free(a);
}

/* ------------------------------------------------ device frame storage -- */

/* Store/Load are the dominant reason a lowerable subtree ends: measured over
 * 32,373 corpus nodes, Store 3.0% + StoreVal 3.0% + BasicBlock 3.0% + Invoke
 * 4.0% terminate subtrees, against a census that is 80% expression nodes. The
 * expressions are there; C statement boundaries chop them into 3-4 node pieces.
 * Giving the device a frame — storage it can load from and store to — is what
 * lets a run of statements lower as one kernel instead of N unrelated scraps.
 *
 * The frame is the input buffer, made read-write: slots are dense-indexed local
 * offsets, the host seeds every slot, the kernel reads and writes them in
 * place, and the host reads the whole frame back. No new binding, no ABI
 * version bump — the buffer was never decorated NonWritable. */

static AstLocal mk_store(AstArena *a, int32_t off, AstLocal val, int type) {
	AstLocal st = ast_node(a, AST_Store);
	ast_set_type(a, st, type, 0);
	ast_add_child(a, st, mk_ref(a, off, type));
	ast_add_child(a, st, val);
	return st;
}

static void suite_frame(void) {
	AstArena *a;
	MccSliceFrame fr;
	int64_t f[MCC_SLICE_MAXSLOT];
	int i;

	/* One statement: x(-8) = x(-8)*3 + x(-16) */
	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		ast_add_child(a, bb,
									mk_store(a, -8,
													 mk_bin(a, '+',
																	mk_bin(a, '*', mk_ref(a, -8, VT_INT),
																				 mk_lit(a, 3, VT_INT), VT_INT),
																	mk_ref(a, -16, VT_INT), VT_INT),
													 VT_INT));
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1,
					"a block of local stores is frame work");
		CHECK(fr.nslot == 2, "the slot map covers both locals");
		CHECK(fr.slot[0] == -8 && fr.slot[1] == -16,
					"slots are in first-encounter order, destination first");
		CHECK(fr.nstmt == 1, "one statement");
		f[0] = 5;
		f[1] = 7;
		CHECK(mcc_slice_frame_exec_cpu(&fr, f) == 1, "the CPU frame runs");
		CHECK(f[0] == 22, "5*3 + 7 = 22 written back to the destination slot");
		CHECK(f[1] == 7, "the untouched slot is unchanged");
	}
	ast_arena_free(a);

	/* Two statements, second reading what the first wrote -- the property a
	 * per-expression kernel cannot express at all. */
	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		ast_add_child(a, bb,
									mk_store(a, -24,
													 mk_bin(a, '+', mk_ref(a, -8, VT_INT),
																	mk_ref(a, -16, VT_INT), VT_INT),
													 VT_INT));
		ast_add_child(a, bb,
									mk_store(a, -8,
													 mk_bin(a, '*', mk_ref(a, -24, VT_INT),
																	mk_lit(a, 2, VT_INT), VT_INT),
													 VT_INT));
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1, "a two-statement run is work");
		CHECK(fr.nstmt == 2, "two statements");
		CHECK(fr.nslot == 3, "three distinct slots");
		f[0] = 0; f[1] = 0; f[2] = 0;
		for (i = 0; i < fr.nslot; i++)
			f[i] = fr.slot[i] == -8 ? 3 : fr.slot[i] == -16 ? 4 : 0;
		CHECK(mcc_slice_frame_exec_cpu(&fr, f) == 1, "the two-statement run executes");
		for (i = 0; i < fr.nslot; i++) {
			if (fr.slot[i] == -24)
				CHECK(f[i] == 7, "statement 1 wrote 3+4 to its slot");
			if (fr.slot[i] == -8)
				CHECK(f[i] == 14, "statement 2 read statement 1's result and doubled it");
		}
	}
	ast_arena_free(a);

	/* The device runs the same run over many independent frames, and every slot
	 * of every frame must match the CPU reference. This is the property the
	 * whole frame-storage idea rests on: statements sequenced on the device,
	 * with a later one observing an earlier one's store. */
	if (g_have_device) {
		a = ast_arena_new();
		{
			AstLocal bb = ast_node(a, AST_BasicBlock);
			MccSliceKernel k;
			int64_t cf[8 * MCC_SLICE_MAXSLOT], gf[8 * MCC_SLICE_MAXSLOT];
			int t, bad = 0;
			ast_add_child(a, bb,
										mk_store(a, -24,
														 mk_bin(a, '+', mk_ref(a, -8, VT_INT),
																		mk_ref(a, -16, VT_INT), VT_INT),
														 VT_INT));
			ast_add_child(a, bb,
										mk_store(a, -8,
														 mk_bin(a, '*', mk_ref(a, -24, VT_INT),
																		mk_ref(a, -8, VT_INT), VT_INT),
														 VT_INT));
			CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1, "the run is frame work");
			CHECK(mcc_slice_frame_kernel_build(&fr, &k) == 1,
						"the frame run lowers to a device kernel");
			for (t = 0; t < 8; t++)
				for (i = 0; i < fr.nslot; i++) {
					int64_t v = fr.slot[i] == -8 ? (t + 1) : fr.slot[i] == -16 ? (t * 3 - 4) : 0;
					cf[t * fr.nslot + i] = gf[t * fr.nslot + i] = v;
				}
			for (t = 0; t < 8; t++)
				CHECK(mcc_slice_frame_exec_cpu(&fr, cf + (long)t * fr.nslot) == 1,
							"the CPU reference runs each frame");
			CHECK(mcc_slice_run_frame_gpu(&fr, &k, gf, 8, NULL, NULL) == MCC_TASK_DONE,
						"the device runs eight frames in one dispatch");
			for (t = 0; t < 8 * fr.nslot; t++)
				if (cf[t] != gf[t])
					bad++;
			CHECK(bad == 0, "every slot of every frame matches the CPU reference");
			if (bad)
				for (t = 0; t < fr.nslot; t++)
					fprintf(stderr, "  slot %d off=%d cpu=%lld gpu=%lld\n", t, fr.slot[t],
									(long long)cf[t], (long long)gf[t]);
			mcc_slice_kernel_free(&k);
		}
		ast_arena_free(a);
	}

	/* A run ending in Return: stores into the frame, value into the out slots.
	 * This is what took corpus eligibility from 44 of 947 blocks to 255 --
	 * Return blocked more blocks on its own (353) than Invoke (294) or If (238). */
	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal r = ast_node(a, AST_Return);
		int64_t cf[4 * MCC_SLICE_MAXSLOT], gf[4 * MCC_SLICE_MAXSLOT];
		int64_t crv[4], grv[4];
		int cdf[4];
		unsigned char gdf[4];
		MccSliceKernel k;
		int t;
		ast_add_child(a, bb,
									mk_store(a, -24,
													 mk_bin(a, '+', mk_ref(a, -8, VT_INT),
																	mk_lit(a, 10, VT_INT), VT_INT),
													 VT_INT));
		ast_add_child(a, r,
									mk_bin(a, '*', mk_ref(a, -24, VT_INT),
												 mk_ref(a, -8, VT_INT), VT_INT));
		ast_add_child(a, bb, r);
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1, "a run ending in Return is work");
		CHECK(fr.ret != AST_NONE, "the run records its Return value");
		CHECK(fr.nstmt == 1, "the Return is not counted as a store");
		for (t = 0; t < 4; t++)
			for (i = 0; i < fr.nslot; i++) {
				int64_t v = fr.slot[i] == -8 ? (t + 2) : 0;
				cf[t * fr.nslot + i] = gf[t * fr.nslot + i] = v;
			}
		for (t = 0; t < 4; t++)
			CHECK(mcc_slice_frame_exec_cpu2(&fr, cf + (long)t * fr.nslot, &crv[t],
																			&cdf[t]) == 1,
						"the CPU reference runs the returning frame");
		CHECK(crv[0] == 24, "(2+10)*2 = 24 returned");
		CHECK(cdf[0] == 1, "and it is defined");
		if (g_have_device && mcc_slice_frame_kernel_build(&fr, &k)) {
			CHECK(mcc_slice_run_frame_gpu(&fr, &k, gf, 4, grv, gdf) == MCC_TASK_DONE,
						"the device runs the returning frame");
			for (t = 0; t < 4; t++) {
				CHECK(grv[t] == crv[t], "device and CPU agree on the returned value");
				CHECK((int)gdf[t] == cdf[t], "and on its definedness");
			}
			for (t = 0; t < 4 * fr.nslot; t++)
				CHECK(gf[t] == cf[t], "and on every frame slot");
			mcc_slice_kernel_free(&k);
		}
	}
	ast_arena_free(a);

	/* A statement-if inside a frame run. The two arms communicate through the
	 * frame, so the device needs SelectionMerge and two blocks of stores but no
	 * OpPhi for the value -- which is precisely why having a frame is what makes
	 * control flow tractable here at all. */
	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal iff = ast_node(a, AST_If);
		AstLocal thn = ast_node(a, AST_BasicBlock);
		AstLocal els = ast_node(a, AST_BasicBlock);
		MccSliceKernel k;
		int64_t cf[6 * MCC_SLICE_MAXSLOT], gf[6 * MCC_SLICE_MAXSLOT];
		int t, bad = 0;
		ast_set_op(a, iff, 0);
		ast_add_child(a, iff,
									mk_bin(a, TOK_LT, mk_ref(a, -8, VT_INT),
												 mk_lit(a, 0, VT_INT), VT_INT));
		ast_add_child(a, thn,
									mk_store(a, -16, mk_lit(a, 111, VT_INT), VT_INT));
		ast_add_child(a, els,
									mk_store(a, -16,
													 mk_bin(a, '*', mk_ref(a, -8, VT_INT),
																	mk_lit(a, 2, VT_INT), VT_INT),
													 VT_INT));
		ast_add_child(a, iff, thn);
		ast_add_child(a, iff, els);
		ast_add_child(a, bb, iff);
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1,
					"a block containing a statement-if is frame work");
		CHECK(fr.nctrl == 1, "the if is counted as control flow");
		CHECK(fr.nstmt == 2, "both arms' stores are counted");
		for (t = 0; t < 6; t++)
			for (i = 0; i < fr.nslot; i++) {
				int64_t v = fr.slot[i] == -8 ? (int64_t)(t - 3) : 0;
				cf[t * fr.nslot + i] = gf[t * fr.nslot + i] = v;
			}
		for (t = 0; t < 6; t++)
			CHECK(mcc_slice_frame_exec_cpu(&fr, cf + (long)t * fr.nslot) == 1,
						"the CPU takes a branch per frame");
		for (t = 0; t < 6; t++)
			for (i = 0; i < fr.nslot; i++)
				if (fr.slot[i] == -16) {
					int64_t x = (int64_t)(t - 3);
					CHECK(cf[t * fr.nslot + i] == (x < 0 ? 111 : x * 2),
								"the taken arm's store is the one observed");
				}
		if (g_have_device && mcc_slice_frame_kernel_build(&fr, &k)) {
			CHECK(mcc_slice_run_frame_gpu(&fr, &k, gf, 6, NULL, NULL) ==
								MCC_TASK_DONE,
						"the device runs six frames that diverge");
			for (t = 0; t < 6 * fr.nslot; t++)
				if (cf[t] != gf[t])
					bad++;
			CHECK(bad == 0, "device and CPU agree on every slot across the branch");
			mcc_slice_kernel_free(&k);
		}
	}
	ast_arena_free(a);

	/* A while loop. Loop-carried state lives in the frame, so the SPIR-V header
	 * needs phis only for the trip counter and the definedness flag -- never for
	 * the values. sum(-16) = 0; while (i(-8) < n(-24)) { sum += i; i += 1; } */
	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal lp = ast_node(a, AST_If);
		AstLocal body = ast_node(a, AST_BasicBlock);
		MccSliceKernel k;
		int64_t cf[5 * MCC_SLICE_MAXSLOT], gf[5 * MCC_SLICE_MAXSLOT];
		int t, bad = 0, si = -1, sn = -1, ss = -1;
		ast_set_op(a, lp, 2);
		ast_add_child(a, lp,
									mk_bin(a, TOK_LT, mk_ref(a, -8, VT_INT),
												 mk_ref(a, -24, VT_INT), VT_INT));
		ast_add_child(a, body,
									mk_store(a, -16,
													 mk_bin(a, '+', mk_ref(a, -16, VT_INT),
																	mk_ref(a, -8, VT_INT), VT_INT),
													 VT_INT));
		ast_add_child(a, body,
									mk_store(a, -8,
													 mk_bin(a, '+', mk_ref(a, -8, VT_INT),
																	mk_lit(a, 1, VT_INT), VT_INT),
													 VT_INT));
		ast_add_child(a, lp, body);
		ast_add_child(a, bb, lp);
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1, "a while loop is frame work");
		CHECK(fr.nloop == 1, "the loop is counted");
		for (i = 0; i < fr.nslot; i++) {
			if (fr.slot[i] == -8) si = i;
			if (fr.slot[i] == -24) sn = i;
			if (fr.slot[i] == -16) ss = i;
		}
		CHECK(si >= 0 && sn >= 0 && ss >= 0, "all three slots mapped");
		for (t = 0; t < 5; t++)
			for (i = 0; i < fr.nslot; i++)
				cf[t * fr.nslot + i] = gf[t * fr.nslot + i] =
						(i == sn) ? (int64_t)t : 0;
		for (t = 0; t < 5; t++)
			CHECK(mcc_slice_frame_exec_cpu(&fr, cf + (long)t * fr.nslot) == 1,
						"the CPU runs the loop");
		/* sum of 0..n-1 */
		for (t = 0; t < 5; t++)
			CHECK(cf[t * fr.nslot + ss] == (int64_t)(t * (t - 1) / 2),
						"the loop accumulated sum(0..n-1)");
		if (g_have_device && mcc_slice_frame_kernel_build(&fr, &k)) {
			CHECK(mcc_slice_run_frame_gpu(&fr, &k, gf, 5, NULL, NULL) ==
								MCC_TASK_DONE,
						"the device runs five loops with different trip counts");
			for (t = 0; t < 5 * fr.nslot; t++)
				if (cf[t] != gf[t])
					bad++;
			CHECK(bad == 0, "device and CPU agree on every slot after the loop");
			if (bad)
				for (t = 0; t < 5; t++)
					fprintf(stderr, "    n=%d cpu_sum=%lld gpu_sum=%lld\n", t,
									(long long)cf[t * fr.nslot + ss],
									(long long)gf[t * fr.nslot + ss]);
			mcc_slice_kernel_free(&k);
		}
	}
	ast_arena_free(a);

	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal lp = ast_node(a, AST_If);
		AstLocal incr = ast_node(a, AST_BasicBlock);
		AstLocal body = ast_node(a, AST_BasicBlock);
		MccSliceKernel k;
		int64_t cf[5 * MCC_SLICE_MAXSLOT], gf[5 * MCC_SLICE_MAXSLOT];
		int t, bad = 0, si = -1, sn = -1, ss = -1;
		ast_set_op(a, lp, 3);
		ast_add_child(a, lp,
									mk_bin(a, TOK_LT, mk_ref(a, -8, VT_INT),
												 mk_ref(a, -24, VT_INT), VT_INT));
		ast_add_child(a, incr,
									mk_store(a, -8,
													 mk_bin(a, '+', mk_ref(a, -8, VT_INT),
																	mk_lit(a, 1, VT_INT), VT_INT),
													 VT_INT));
		ast_add_child(a, body,
									mk_store(a, -16,
													 mk_bin(a, '+', mk_ref(a, -16, VT_INT),
																	mk_ref(a, -8, VT_INT), VT_INT),
													 VT_INT));
		ast_add_child(a, lp, incr);
		ast_add_child(a, lp, body);
		ast_add_child(a, bb, lp);
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1, "a for loop is frame work");
		CHECK(fr.nloop == 1, "the for loop is counted");
		for (i = 0; i < fr.nslot; i++) {
			if (fr.slot[i] == -8) si = i;
			if (fr.slot[i] == -24) sn = i;
			if (fr.slot[i] == -16) ss = i;
		}
		CHECK(si >= 0 && sn >= 0 && ss >= 0, "all three slots mapped");
		for (t = 0; t < 5; t++)
			for (i = 0; i < fr.nslot; i++)
				cf[t * fr.nslot + i] = gf[t * fr.nslot + i] =
						(i == sn) ? (int64_t)t : 0;
		for (t = 0; t < 5; t++)
			CHECK(mcc_slice_frame_exec_cpu(&fr, cf + (long)t * fr.nslot) == 1,
						"the CPU runs the for loop");
		for (t = 0; t < 5; t++)
			CHECK(cf[t * fr.nslot + ss] == (int64_t)(t * (t - 1) / 2),
						"the body ran before the increment, so sum is sum(0..n-1)");
		for (t = 0; t < 5; t++)
			CHECK(cf[t * fr.nslot + si] == (int64_t)t,
						"the induction variable ends at n");
		if (g_have_device && mcc_slice_frame_kernel_build(&fr, &k)) {
			CHECK(mcc_slice_run_frame_gpu(&fr, &k, gf, 5, NULL, NULL) ==
								MCC_TASK_DONE,
						"the device runs the for loop");
			for (t = 0; t < 5 * fr.nslot; t++)
				if (cf[t] != gf[t])
					bad++;
			CHECK(bad == 0, "device and CPU agree on every slot after the for loop");
			mcc_slice_kernel_free(&k);
		}
	}
	ast_arena_free(a);

	/* `x++` / `x--` as statements. Measured to unblock 96 more corpus blocks on
	 * their own, and they need no address space -- on an integer local the whole
	 * operation is frame[slot] +/- 1. */
	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal inc = ast_node(a, AST_Unary);
		AstLocal dec = ast_node(a, AST_Unary);
		MccSliceKernel k;
		int64_t cf[4 * MCC_SLICE_MAXSLOT], gf[4 * MCC_SLICE_MAXSLOT];
		int t, bad = 0;
		ast_set_op(a, inc, TOK_INC);
		ast_set_type(a, inc, VT_INT, 0);
		ast_add_child(a, inc, mk_ref(a, -8, VT_INT));
		ast_set_op(a, dec, TOK_DEC);
		ast_set_type(a, dec, VT_INT, 0);
		ast_add_child(a, dec, mk_ref(a, -16, VT_INT));
		ast_add_child(a, bb, inc);
		ast_add_child(a, bb, dec);
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1,
					"a block of ++/-- statements is frame work");
		CHECK(fr.nslot == 2 && fr.nstmt == 2, "both locals mapped, both counted");
		for (t = 0; t < 4; t++)
			for (i = 0; i < fr.nslot; i++)
				cf[t * fr.nslot + i] = gf[t * fr.nslot + i] = (int64_t)(t * 7 - 10);
		for (t = 0; t < 4; t++)
			CHECK(mcc_slice_frame_exec_cpu(&fr, cf + (long)t * fr.nslot) == 1,
						"the CPU runs ++/--");
		for (t = 0; t < 4; t++)
			for (i = 0; i < fr.nslot; i++) {
				int64_t was = (int64_t)(t * 7 - 10);
				CHECK(cf[t * fr.nslot + i] == (fr.slot[i] == -8 ? was + 1 : was - 1),
							"++ added one and -- subtracted one");
			}
		if (g_have_device && mcc_slice_frame_kernel_build(&fr, &k)) {
			CHECK(mcc_slice_run_frame_gpu(&fr, &k, gf, 4, NULL, NULL) ==
								MCC_TASK_DONE,
						"the device runs ++/--");
			for (t = 0; t < 4 * fr.nslot; t++)
				if (cf[t] != gf[t])
					bad++;
			CHECK(bad == 0, "device and CPU agree on every incremented slot");
			mcc_slice_kernel_free(&k);
		}
	}
	ast_arena_free(a);

	/* B1 -- a runtime index into a local array, on both sides of the store.
	 *
	 * `arr[i] = arr[j] * 2 + 1` over `int arr[4]` at frame offset -32, with i at
	 * -8 and j at -12. The address is not known until the kernel runs, so the
	 * device resolves a slot from a value in the frame; the object's element
	 * slots are contiguous by construction, which is what makes `slot0 + index`
	 * the right cell.
	 *
	 * Every index in the seed set is in range here, so this case is about the
	 * address arithmetic and not about the bound -- the bound gets its own case
	 * below. */
	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal arr = mk_ref(a, -32, VT_PTR | VT_ARRAY);
		AstLocal arr2 = mk_ref(a, -32, VT_PTR | VT_ARRAY);
		AstLocal ld = ast_node(a, AST_Load);
		AstLocal dst = ast_node(a, AST_Load);
		MccSliceKernel k;
		int64_t cf[4 * MCC_SLICE_MAXSLOT], gf[4 * MCC_SLICE_MAXSLOT];
		int64_t crv[4], grv[4];
		int cdf[4];
		unsigned char gdf[4];
		int t, bad = 0, si = -1, sj = -1, s0 = -1;
		slicerun_obj_reset(64);
		g_obj_ext[arr] = 16;
		g_obj_ety[arr] = VT_INT;
		g_obj_ext[arr2] = 16;
		g_obj_ety[arr2] = VT_INT;
		ast_add_child(a, ld, mk_bin(a, '+', arr2, mk_ref(a, -12, VT_INT), 0));
		ast_set_type(a, ld, 0, 0);
		ast_add_child(a, dst, mk_bin(a, '+', arr, mk_ref(a, -8, VT_INT), 0));
		ast_set_type(a, dst, 0, 0);
		{
			AstLocal st = ast_node(a, AST_Store);
			ast_add_child(a, st, dst);
			ast_add_child(a, st,
										mk_bin(a, '+',
													 mk_bin(a, '*', ld, mk_lit(a, 2, VT_INT), VT_INT),
													 mk_lit(a, 1, VT_INT), VT_INT));
			ast_add_child(a, bb, st);
		}
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1,
					"a runtime-indexed store into a local array is frame work");
		CHECK(fr.nidx == 2, "both the indexed load and the indexed store counted");
		CHECK(fr.nslot == 6,
					"four element slots plus the two index locals");
		for (i = 0; i < fr.nslot; i++) {
			if (fr.slot[i] == -8) si = i;
			if (fr.slot[i] == -12) sj = i;
			if (fr.slot[i] == -32) s0 = i;
		}
		CHECK(s0 == 0 && fr.slot[1] == -28 && fr.slot[2] == -24 &&
							fr.slot[3] == -20,
					"the array's element slots are consecutive and in order");
		for (t = 0; t < 4; t++)
			for (i = 0; i < fr.nslot; i++) {
				int64_t v = i == si ? (int64_t)(t % 4)
													 : i == sj ? (int64_t)((t + 1) % 4)
																		 : (int64_t)(10 * i + t);
				cf[t * fr.nslot + i] = gf[t * fr.nslot + i] = v;
			}
		for (t = 0; t < 4; t++)
			CHECK(mcc_slice_frame_exec_cpu2(&fr, cf + (long)t * fr.nslot, &crv[t],
																			&cdf[t]) == 1,
						"the CPU reference runs the indexed frame");
		for (t = 0; t < 4; t++)
			CHECK(cdf[t] == 1, "every index is in range, so the run is defined");
		for (t = 0; t < 4; t++) {
			int di = t % 4, sr = (t + 1) % 4;
			CHECK(cf[t * fr.nslot + di] == (int64_t)(10 * sr + t) * 2 + 1,
						"arr[i] took arr[j]*2 + 1 at the element the index selected");
		}
		if (g_have_device && mcc_slice_frame_kernel_build(&fr, &k)) {
			CHECK(mcc_slice_run_frame_gpu(&fr, &k, gf, 4, grv, gdf) == MCC_TASK_DONE,
						"the device runs the indexed frame");
			for (t = 0; t < 4 * fr.nslot; t++)
				if (cf[t] != gf[t])
					bad++;
			for (t = 0; t < 4; t++)
				if ((int)gdf[t] != cdf[t])
					bad++;
			CHECK(bad == 0,
						"device and CPU agree on every element slot and on the verdict");
			if (bad)
				for (t = 0; t < fr.nslot; t++)
					fprintf(stderr, "  slot %d off=%d cpu=%lld gpu=%lld\n", t, fr.slot[t],
									(long long)cf[t], (long long)gf[t]);
			mcc_slice_kernel_free(&k);
		}
		slicerun_obj_reset(0);
	}
	ast_arena_free(a);

	/* An index that leaves its object. J3b says a device PageFault is our own
	 * bug, so this must be impossible rather than merely detected: the index is
	 * masked into the object's own padded span and the run's definedness flag is
	 * cleared. Undefined is the only acceptable answer -- never a wrong one, and
	 * never a fault. The CPU reference reaches the same verdict AND writes the
	 * same masked element, so the two agree on the frame as well as the flag. */
	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal arr = mk_ref(a, -32, VT_PTR | VT_ARRAY);
		AstLocal dst = ast_node(a, AST_Load);
		MccSliceKernel k;
		int64_t cf[6 * MCC_SLICE_MAXSLOT], gf[6 * MCC_SLICE_MAXSLOT];
		int64_t crv[6], grv[6];
		int cdf[6];
		unsigned char gdf[6];
		static const int64_t IDX[6] = {0, 3, 4, -1, 99, 2};
		int t, bad = 0, si = -1;
		slicerun_obj_reset(64);
		g_obj_ext[arr] = 16;
		g_obj_ety[arr] = VT_INT;
		ast_add_child(a, dst, mk_bin(a, '+', arr, mk_ref(a, -8, VT_INT), 0));
		ast_set_type(a, dst, 0, 0);
		{
			AstLocal st = ast_node(a, AST_Store);
			ast_add_child(a, st, dst);
			ast_add_child(a, st, mk_lit(a, 777, VT_INT));
			ast_add_child(a, bb, st);
		}
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1,
					"an out-of-range-capable store is still frame work");
		for (i = 0; i < fr.nslot; i++)
			if (fr.slot[i] == -8)
				si = i;
		for (t = 0; t < 6; t++)
			for (i = 0; i < fr.nslot; i++)
				cf[t * fr.nslot + i] = gf[t * fr.nslot + i] = i == si ? IDX[t] : 0;
		for (t = 0; t < 6; t++)
			CHECK(mcc_slice_frame_exec_cpu2(&fr, cf + (long)t * fr.nslot, &crv[t],
																			&cdf[t]) == 1,
						"the CPU reference completes even when the index is wild");
		for (t = 0; t < 6; t++)
			CHECK(cdf[t] == (IDX[t] >= 0 && IDX[t] < 4),
						"in range is defined and out of range is undefined");
		for (t = 0; t < 6; t++) {
			int wrote = 0;
			for (i = 0; i < fr.nslot; i++)
				if (i != si && cf[t * fr.nslot + i] == 777)
					wrote++;
			CHECK(wrote == 1, "the masked store landed inside the array, exactly once");
		}
		if (g_have_device && mcc_slice_frame_kernel_build(&fr, &k)) {
			CHECK(mcc_slice_run_frame_gpu(&fr, &k, gf, 6, grv, gdf) == MCC_TASK_DONE,
						"the device survives every wild index");
			for (t = 0; t < 6 * fr.nslot; t++)
				if (cf[t] != gf[t])
					bad++;
			for (t = 0; t < 6; t++)
				if ((int)gdf[t] != cdf[t])
					bad++;
			CHECK(bad == 0,
						"device and CPU agree on the masked element and on undefinedness");
			mcc_slice_kernel_free(&k);
		}
		slicerun_obj_reset(0);
	}
	ast_arena_free(a);

	/* One slot per element rather than a byte-addressed region is what keeps the
	 * existing two-word store correct. Under byte addressing an int at -12 and an
	 * int at -8 are adjacent words, and spv_store_live_v's sign-extended high
	 * word would land on the neighbour. Here they are separate 8-byte slots, so
	 * this asserts the property the layout choice buys: a 32-bit store to one
	 * element does not disturb the next, and neither does one to a scalar local
	 * that sits at the adjacent frame offset. */
	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal arr = mk_ref(a, -12, VT_PTR | VT_ARRAY);
		AstLocal dst = ast_node(a, AST_Load);
		MccSliceKernel k;
		int64_t cf[3 * MCC_SLICE_MAXSLOT], gf[3 * MCC_SLICE_MAXSLOT];
		int t, bad = 0, sn = -1;
		slicerun_obj_reset(64);
		g_obj_ext[arr] = 8;
		g_obj_ety[arr] = VT_INT;
		ast_add_child(a, dst, mk_bin(a, '+', arr, mk_lit(a, 0, VT_INT), 0));
		ast_set_type(a, dst, 0, 0);
		{
			AstLocal st = ast_node(a, AST_Store);
			ast_add_child(a, st, dst);
			ast_add_child(a, st, mk_lit(a, -1, VT_INT));
			ast_add_child(a, bb, st);
		}
		ast_add_child(a, bb, mk_store(a, -4, mk_lit(a, 5, VT_INT), VT_INT));
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1,
					"an element store beside a scalar local is frame work");
		CHECK(fr.nslot == 3, "two element slots and the neighbouring scalar");
		CHECK(fr.slot[0] == -12 && fr.slot[1] == -8 && fr.slot[2] == -4,
					"the neighbour is a separate slot, not an adjacent word");
		for (i = 0; i < fr.nslot; i++)
			if (fr.slot[i] == -8)
				sn = i;
		for (t = 0; t < 3; t++)
			for (i = 0; i < fr.nslot; i++)
				cf[t * fr.nslot + i] = gf[t * fr.nslot + i] = 12345;
		for (t = 0; t < 3; t++)
			CHECK(mcc_slice_frame_exec_cpu(&fr, cf + (long)t * fr.nslot) == 1,
						"the CPU writes element 0 and the scalar");
		for (t = 0; t < 3; t++) {
			CHECK(cf[t * fr.nslot + 0] == -1, "element 0 took the -1");
			CHECK(cf[t * fr.nslot + sn] == 12345,
						"element 1 was not disturbed by the sign-extended high word");
			CHECK(cf[t * fr.nslot + 2] == 5, "the scalar local took its own store");
		}
		if (g_have_device && mcc_slice_frame_kernel_build(&fr, &k)) {
			CHECK(mcc_slice_run_frame_gpu(&fr, &k, gf, 3, NULL, NULL) ==
								MCC_TASK_DONE,
						"the device runs the adjacency case");
			for (t = 0; t < 3 * fr.nslot; t++)
				if (cf[t] != gf[t])
					bad++;
			CHECK(bad == 0, "the device disturbed no neighbour either");
			mcc_slice_kernel_free(&k);
		}
		slicerun_obj_reset(0);
	}
	ast_arena_free(a);

	/* Refusals: a store to something that is not a local frame slot, and a
	 * block containing a non-store statement. */
	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal st = ast_node(a, AST_Store);
		AstLocal g = ast_node(a, AST_Ref);
		ast_set_op(a, g, VT_CONST | VT_SYM);
		ast_set_type(a, g, VT_INT, 0);
		ast_add_child(a, st, g);
		ast_add_child(a, st, mk_lit(a, 1, VT_INT));
		ast_add_child(a, bb, st);
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 0,
					"a store to a global is not frame work");
	}
	ast_arena_free(a);
	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		ast_add_child(a, bb, ast_node(a, AST_Invoke));
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 0,
					"a block containing a call is not frame work");
	}
	ast_arena_free(a);
}

/* ---------------------------------------- per-width byte-addressed access -- */

/* The region layer, differentialled directly against its reference rather than
 * through a slice. It is tested on its own because it is the piece everything
 * later depends on: a heap shared between lanes and the host has objects of
 * different widths sitting next to each other by construction, so a store that
 * writes two words where the type has one is not an edge case there, it is the
 * normal case. The dense-slot frame does not need it -- one slot per element
 * keeps the cells disjoint -- which is exactly why it would otherwise ship
 * unexercised.
 *
 * Every lane owns a region of NSLOT*2 words of the input buffer, the same
 * storage the frame path uses; the only thing that makes it "the frame" rather
 * than "a heap" is which base is handed in, and nothing in the emitter below
 * knows the difference. */
#define BYTES_NSLOT 8
#define BYTES_NWORD (BYTES_NSLOT * MCC_GPU_IN_SLOTS)
#define BYTES_NBYTE (BYTES_NWORD * 4)

/* Load from the offset in word 0, store the loaded value at the offset in word
 * 1. Both offsets are read from the region at run time, so neither the address
 * nor its range is known at emit time. */
static int bytes_kernel_in(int t, int shared, MccGpuCode *out) {
	SpvMod m;
	SpvRegion r;
	SpvV v;
	uint32_t base, olo, ohi;
	uint32_t *code;
	int nw = 0;
	spv_module_begin(&m, BYTES_NSLOT);
	base = spv_main_begin(&m, BYTES_NSLOT);
	r = shared ? spv_region_shared(m.id_in, base, spv_uintc(&m, BYTES_NBYTE))
						 : spv_region(m.id_in, base, spv_uintc(&m, BYTES_NBYTE));
	olo = spv_load_at(&m, base);
	ohi = spv_emit3(&m, SpvOpIAdd, m.id_int, base, spv_const(&m, 1));
	ohi = spv_load_at(&m, ohi);
	v = spv_load_region(&m, &r, olo, t);
	if (mcc_slice_mutate) {
		uint32_t p = spv_pair(&m, v);
		uint32_t lo = spv_uop(&m, SpvOpBitwiseXor, spv_lo(&m, p), spv_uintc(&m, 1));
		v = spv_mk(spv_u2(&m, lo, spv_hi(&m, p)), 1, 0);
	}
	spv_store_region(&m, &r, ohi, v, t);
	if (m.failed) {
		spv_module_free(&m);
		return 0;
	}
	spv_main_end(&m, m.lane, spv_mk(spv_const(&m, 0), 0, 0));
	code = spv_module_finish(&m, &nw);
	spv_module_free(&m);
	if (!code || nw <= 0) {
		free(code);
		return 0;
	}
	out->p = code;
	out->n = nw;
	return 1;
}

static int bytes_kernel(int t, MccGpuCode *out) {
	return bytes_kernel_in(t, 0, out);
}

static void bytes_subword_shared(void) {
	static const int NARROW[] = {VT_BYTE, VT_BYTE | VT_UNSIGNED, VT_SHORT,
															 VT_SHORT | VT_UNSIGNED};
	static const int WIDE[] = {VT_INT, VT_INT | VT_UNSIGNED, VT_LLONG,
														 VT_LLONG | VT_UNSIGNED};
	MccGpuCode c;
	int i;
	for (i = 0; i < (int)(sizeof NARROW / sizeof *NARROW); i++) {
		if (bytes_kernel_in(NARROW[i], 1, &c)) {
			CHECK(0, "a shared region refuses a sub-word store, which would be a "
								"read-modify-write race between two lanes of one word");
			free(c.p);
		} else {
			CHECK(1, "a shared region refuses a sub-word store, which would be a "
								"read-modify-write race between two lanes of one word");
		}
		if (bytes_kernel_in(NARROW[i], 0, &c)) {
			CHECK(1, "and a lane-private region of the same width still emits");
			free(c.p);
		} else {
			CHECK(0, "and a lane-private region of the same width still emits");
		}
	}
	for (i = 0; i < (int)(sizeof WIDE / sizeof *WIDE); i++) {
		if (bytes_kernel_in(WIDE[i], 1, &c)) {
			CHECK(1, "a word-or-wider store into a shared region is whole-word and "
								"so is admitted");
			free(c.p);
		} else {
			CHECK(0, "a word-or-wider store into a shared region is whole-word and "
								"so is admitted");
		}
	}
}

static void suite_bytes(void) {
	/* Offsets chosen so that every width sees: aligned and in range, the last
	 * legal offset, one past it, a misaligned offset, a wildly out-of-range one,
	 * and a negative one. */
	static const int32_t OFF[] = {0,   1,   2,   4,   7,   8,   60,
																61,  62,  63,  64,  128, -1,  -4};
	static const int TY[] = {VT_BYTE, VT_BYTE | VT_UNSIGNED,
													 VT_SHORT, VT_SHORT | VT_UNSIGNED,
													 VT_INT,  VT_INT | VT_UNSIGNED,
													 VT_LLONG, VT_LLONG | VT_UNSIGNED};
	int noff = (int)(sizeof OFF / sizeof *OFF);
	int nty = (int)(sizeof TY / sizeof *TY);
	int ti, oi;

	/* The reference on its own first: a device is optional, the rule is not. */
	CHECK(ast_eval_slice_addr_ok(64, 60, 4) == 1, "the last legal word is in range");
	CHECK(ast_eval_slice_addr_ok(64, 61, 4) == 0, "one byte past it is not");
	CHECK(ast_eval_slice_addr_ok(64, 62, 4) == 0, "and neither is a misaligned word");
	CHECK(ast_eval_slice_addr_ok(64, 56, 8) == 1, "the last legal doubleword is in range");
	CHECK(ast_eval_slice_addr_ok(64, 60, 8) == 0, "a doubleword straddling the end is not");
	CHECK(ast_eval_slice_addr_ok(4, 0, 8) == 0, "nor one wider than the region itself");
	CHECK(ast_eval_slice_addr_ok(64, -4, 4) == 0, "a negative offset is out of range");
	CHECK(ast_eval_slice_addr_fix(64, 999, 4) == 0,
				"a rejected offset is replaced by one that cannot leave the region");
	{
		uint32_t w[2] = {0x11223344u, 0u};
		int ok = 0;
		ast_eval_slice_bytes_store(w, 8, 1, VT_BYTE, -1, &ok);
		CHECK(ok == 1 && w[0] == 0x1122FF44u,
					"a one-byte store rewrites one byte of its word and no other");
		CHECK(w[1] == 0, "and does not reach the next word at all");
		ast_eval_slice_bytes_store(w, 8, 4, VT_INT, -1, &ok);
		CHECK(w[0] == 0x1122FF44u, "a four-byte store does not reach backwards either");
	}
	bytes_subword_shared();

	if (!g_have_device)
		return;

	for (ti = 0; ti < nty; ti++) {
		MccGpuCode code;
		uint32_t *ref;
		int32_t *buf, *ob;
		int *cdef;
		int t = TY[ti], i, j, bad = 0, defbad = 0;
		if (!bytes_kernel(t, &code)) {
			CHECK(0, "the region kernel emits for every width");
			continue;
		}
		ref = (uint32_t *)malloc((size_t)noff * BYTES_NWORD * sizeof *ref);
		buf = (int32_t *)malloc((size_t)noff * BYTES_NWORD * sizeof *buf);
		ob = (int32_t *)malloc((size_t)noff * MCC_GPU_OUT_SLOTS * sizeof *ob);
		cdef = (int *)malloc((size_t)noff * sizeof *cdef);
		if (!ref || !buf || !ob || !cdef) {
			free(ref);
			free(buf);
			free(ob);
			free(cdef);
			free(code.p);
			continue;
		}
		/* One lane per offset: lane i loads from OFF[i] and stores four entries
		 * along, so the store address is exercised independently of the load
		 * address and the two verdicts have to AND together into one flag. */
		for (i = 0; i < noff; i++) {
			uint32_t *w = ref + (long)i * BYTES_NWORD;
			int lok = 0, sok = 0;
			int64_t v;
			for (j = 0; j < BYTES_NWORD; j++)
				w[j] = 0xA5000000u + (uint32_t)j * 0x01010101u + (uint32_t)i;
			w[0] = (uint32_t)OFF[i];
			w[1] = (uint32_t)OFF[(i + 4) % noff];
			for (j = 0; j < BYTES_NWORD; j++)
				buf[(long)i * BYTES_NWORD + j] = (int32_t)w[j];
			v = ast_eval_slice_bytes_load(w, BYTES_NBYTE, OFF[i], t, &lok);
			ast_eval_slice_bytes_store(w, BYTES_NBYTE, OFF[(i + 4) % noff], t, v,
																 &sok);
			cdef[i] = lok && sok;
		}
		if (mcc_gpu_dispatch_rw2(code.p, code.n, buf, noff, BYTES_NSLOT, ob)) {
			for (i = 0; i < noff; i++) {
				for (j = 0; j < BYTES_NWORD; j++) {
					uint32_t want = ref[(long)i * BYTES_NWORD + j];
					uint32_t got = (uint32_t)buf[(long)i * BYTES_NWORD + j];
					if (want == got)
						continue;
					if (!bad)
						fprintf(stderr,
										"  BYTES t=%#x load=%d store=%d word %d want=%08x "
										"got=%08x\n",
										t, OFF[i], OFF[(i + 4) % noff], j, want, got);
					bad++;
				}
				if ((ob[(long)i * MCC_GPU_OUT_SLOTS + 2] != 0) != cdef[i])
					defbad++;
			}
			CHECK(bad == 0, "every word of every region matches the reference");
			CHECK(defbad == 0,
						"and the device's verdict on range and alignment matches too");
		} else {
			CHECK(0, "the region kernel dispatches");
		}
		free(ref);
		free(buf);
		free(ob);
		free(cdef);
		free(code.p);
	}
}


#define DEREF_NSLOT 1
#define DEREF_LANE_WORD 16
#define DEREF_LANE_BYTE (DEREF_LANE_WORD * 4)
#define DEREF_ARENA_WORD 1024
#define DEREF_LANES MCC_GPU_LOCAL_SIZE

static int deref_kernel(int t, MccGpuCode *out) {
	SpvMod m;
	SpvRegion r;
	SpvV v;
	uint32_t base, mbase, olo, ohi;
	uint32_t *code;
	int nw = 0;
	spv_module_begin(&m, DEREF_NSLOT);
	base = spv_main_begin(&m, DEREF_NSLOT);
	mbase = spv_emit3(&m, SpvOpIAdd, m.id_int, spv_const(&m, DEREF_ARENA_WORD),
										spv_emit3(&m, SpvOpIMul, m.id_int, m.lane,
															spv_const(&m, DEREF_LANE_WORD)));
	r = spv_region(m.id_mem, mbase, spv_uintc(&m, DEREF_LANE_BYTE));
	olo = spv_load_at(&m, base);
	ohi = spv_load_at(&m,
										spv_emit3(&m, SpvOpIAdd, m.id_int, base, spv_const(&m, 1)));
	v = spv_load_region(&m, &r, olo, t);
	if (mcc_slice_mutate) {
		uint32_t p = spv_pair(&m, v);
		v = spv_mk(
				spv_u2(&m, spv_uop(&m, SpvOpBitwiseXor, spv_lo(&m, p), spv_uintc(&m, 1)),
							 spv_hi(&m, p)),
				1, 0);
	}
	spv_store_region(&m, &r, ohi, v, t);
	spv_main_end(&m, m.lane, v);
	code = spv_module_finish(&m, &nw);
	spv_module_free(&m);
	if (!code || nw <= 0) {
		free(code);
		return 0;
	}
	out->p = code;
	out->n = nw;
	return 1;
}

static uint32_t *deref_arena(void) {
	void *mem = NULL;
	unsigned long msz = 0;
	if (!mcc_gpu_mem(&mem, &msz) || !mem)
		return NULL;
	if (msz <
			(unsigned long)(DEREF_ARENA_WORD + DEREF_LANES * DEREF_LANE_WORD) * 4)
		return NULL;
	return (uint32_t *)mem + DEREF_ARENA_WORD;
}

static uint32_t deref_word(uint32_t seed0, int j) {
	return seed0 + (uint32_t)j * 0x04040404u;
}

static void deref_seed(uint32_t *arena, uint32_t seed0) {
	int i, j;
	for (i = 0; i < DEREF_LANES; i++)
		for (j = 0; j < DEREF_LANE_WORD; j++)
			arena[(long)i * DEREF_LANE_WORD + j] = deref_word(seed0, j);
}

typedef struct DerefWant {
	int32_t loff, soff;
	int def;
	int64_t val;
	int wword[2];
	uint32_t wval[2];
} DerefWant;

static void deref_directed(int t, uint32_t seed0, const DerefWant *w, int n,
													 const char *what) {
	MccGpuCode code;
	uint32_t *arena = deref_arena();
	int32_t *in, *ob;
	int i, j, k, badv = 0, badd = 0, badm = 0;
	if (!arena) {
		CHECK(0, "the shared address space is mappable for the directed cases");
		return;
	}
	if (!deref_kernel(t, &code)) {
		CHECK(0, "the binding-2 region kernel emits for the directed cases");
		return;
	}
	in = (int32_t *)calloc((size_t)n * DEREF_NSLOT * MCC_GPU_IN_SLOTS, sizeof *in);
	ob = (int32_t *)calloc((size_t)n * MCC_GPU_OUT_SLOTS, sizeof *ob);
	if (!in || !ob) {
		free(in);
		free(ob);
		free(code.p);
		return;
	}
	deref_seed(arena, seed0);
	for (i = 0; i < n; i++) {
		in[(long)i * MCC_GPU_IN_SLOTS] = w[i].loff;
		in[(long)i * MCC_GPU_IN_SLOTS + 1] = w[i].soff;
	}
	if (!mcc_gpu_dispatch_rw2(code.p, code.n, in, n, DEREF_NSLOT, ob)) {
		CHECK(0, "the binding-2 region kernel dispatches");
		free(in);
		free(ob);
		free(code.p);
		return;
	}
	for (i = 0; i < n; i++) {
		uint64_t lo = (uint32_t)ob[(long)i * MCC_GPU_OUT_SLOTS];
		uint64_t hi = (uint32_t)ob[(long)i * MCC_GPU_OUT_SLOTS + 1];
		int gd = ob[(long)i * MCC_GPU_OUT_SLOTS + 2] != 0;
		int64_t gv = (int64_t)(lo | (hi << 32));
		if (gd != w[i].def) {
			fprintf(stderr, "  DEREF %s lane %d def want=%d got=%d\n", what, i,
							w[i].def, gd);
			badd++;
		}
		if (gv != w[i].val) {
			fprintf(stderr, "  DEREF %s lane %d value want=%lld got=%lld\n", what, i,
							(long long)w[i].val, (long long)gv);
			badv++;
		}
		for (j = 0; j < DEREF_LANE_WORD; j++) {
			uint32_t want = deref_word(seed0, j);
			uint32_t got = arena[(long)i * DEREF_LANE_WORD + j];
			for (k = 0; k < 2; k++)
				if (w[i].wword[k] == j)
					want = w[i].wval[k];
			if (want == got)
				continue;
			fprintf(stderr, "  DEREF %s lane %d word %d want=%08x got=%08x\n", what, i,
							j, want, got);
			badm++;
		}
	}
	CHECK(badv == 0, "the device returns the hand-computed value at every width");
	CHECK(badd == 0, "and reaches the hand-computed range verdict");
	CHECK(badm == 0, "and writes exactly the hand-computed bytes and no others");
	free(in);
	free(ob);
	free(code.p);
}

static void suite_deref(void) {
	static const int32_t OFF[] = {0,  1,  2,  4,  7,  8,   16, 32,
																56, 60, 61, 62, 64, 128, -1, -4};
	static const int TY[] = {VT_BYTE,  VT_BYTE | VT_UNSIGNED,
													 VT_SHORT, VT_SHORT | VT_UNSIGNED,
													 VT_INT,   VT_INT | VT_UNSIGNED,
													 VT_LLONG, VT_LLONG | VT_UNSIGNED};
	static const DerefWant WINT[] = {
			{4, 8, 1, 0x07060504, {2, -1}, {0x07060504u, 0}},
			{60, 0, 1, 0x3F3E3D3C, {0, -1}, {0x3F3E3D3Cu, 0}},
			{61, 8, 0, 0x03020100, {2, -1}, {0x03020100u, 0}},
			{8, 62, 0, 0x0B0A0908, {0, -1}, {0x0B0A0908u, 0}},
			{2, 64, 0, 0x03020100, {-1, -1}, {0, 0}}};
	static const DerefWant WI8[] = {
			{0, 4, 1, -128, {1, -1}, {0x87868580u, 0}},
			{3, 6, 1, -125, {1, -1}, {0x87838584u, 0}},
			{4, 64, 0, -124, {0, -1}, {0x83828184u, 0}}};
	static const DerefWant WU8[] = {
			{0, 4, 1, 128, {1, -1}, {0x87868580u, 0}},
			{3, 6, 1, 131, {1, -1}, {0x87838584u, 0}}};
	static const DerefWant WI16[] = {
			{0, 4, 1, -32384, {1, -1}, {0x87868180u, 0}},
			{2, 8, 1, -31870, {2, -1}, {0x8B8A8382u, 0}}};
	static const DerefWant WU16[] = {
			{0, 4, 1, 33152, {1, -1}, {0x87868180u, 0}},
			{2, 8, 1, 33666, {2, -1}, {0x8B8A8382u, 0}}};
	static const DerefWant W64[] = {
			{0, 8, 1, (int64_t)(uint64_t)0x8786858483828180ull,
			 {2, 3},
			 {0x83828180u, 0x87868584u}},
			{56, 0, 1, (int64_t)(uint64_t)0xBFBEBDBCBBBAB9B8ull,
			 {0, 1},
			 {0xBBBAB9B8u, 0xBFBEBDBCu}},
			{4, 8, 0, (int64_t)(uint64_t)0x8786858483828180ull,
			 {2, 3},
			 {0x83828180u, 0x87868584u}}};
	int noff = (int)(sizeof OFF / sizeof *OFF);
	int nty = (int)(sizeof TY / sizeof *TY);
	int ti;
	uint32_t *arena;

	if (!g_have_device) {
		if (g_device_required) {
			fprintf(stderr,
							"FAIL slicerun: no usable device but a device is required\n");
			g_failures++;
		}
		return;
	}
	arena = deref_arena();
	CHECK(arena != NULL, "the shared address space holds the per-lane arena");
	if (!arena)
		return;

	deref_directed(VT_INT, 0x03020100u, WINT, (int)(sizeof WINT / sizeof *WINT),
								 "int");
	deref_directed(VT_BYTE, 0x83828180u, WI8, (int)(sizeof WI8 / sizeof *WI8),
								 "schar");
	deref_directed(VT_BYTE | VT_UNSIGNED, 0x83828180u, WU8,
								 (int)(sizeof WU8 / sizeof *WU8), "uchar");
	deref_directed(VT_SHORT, 0x83828180u, WI16, (int)(sizeof WI16 / sizeof *WI16),
								 "short");
	deref_directed(VT_SHORT | VT_UNSIGNED, 0x83828180u, WU16,
								 (int)(sizeof WU16 / sizeof *WU16), "ushort");
	deref_directed(VT_LLONG, 0x83828180u, W64, (int)(sizeof W64 / sizeof *W64),
								 "llong");

	for (ti = 0; ti < nty; ti++) {
		MccGpuCode code;
		uint32_t *ref;
		int32_t *in, *ob;
		int *cdef;
		int64_t *cval;
		int t = TY[ti], i, j, bad = 0, defbad = 0, valbad = 0;
		if (!deref_kernel(t, &code)) {
			CHECK(0, "the binding-2 region kernel emits for every width");
			continue;
		}
		ref = (uint32_t *)malloc((size_t)noff * DEREF_LANE_WORD * sizeof *ref);
		in = (int32_t *)calloc((size_t)noff * MCC_GPU_IN_SLOTS, sizeof *in);
		ob = (int32_t *)calloc((size_t)noff * MCC_GPU_OUT_SLOTS, sizeof *ob);
		cdef = (int *)malloc((size_t)noff * sizeof *cdef);
		cval = (int64_t *)malloc((size_t)noff * sizeof *cval);
		if (!ref || !in || !ob || !cdef || !cval) {
			free(ref);
			free(in);
			free(ob);
			free(cdef);
			free(cval);
			free(code.p);
			continue;
		}
		deref_seed(arena, 0xA5000000u);
		for (i = 0; i < noff; i++) {
			uint32_t *w = ref + (long)i * DEREF_LANE_WORD;
			int lok = 0, sok = 0;
			for (j = 0; j < DEREF_LANE_WORD; j++)
				w[j] = deref_word(0xA5000000u, j);
			in[(long)i * MCC_GPU_IN_SLOTS] = OFF[i];
			in[(long)i * MCC_GPU_IN_SLOTS + 1] = OFF[(i + 5) % noff];
			cval[i] = ast_eval_slice_bytes_load(w, DEREF_LANE_BYTE, OFF[i], t, &lok);
			ast_eval_slice_bytes_store(w, DEREF_LANE_BYTE, OFF[(i + 5) % noff], t,
																 cval[i], &sok);
			cdef[i] = lok && sok;
		}
		if (mcc_gpu_dispatch_rw2(code.p, code.n, in, noff, DEREF_NSLOT, ob)) {
			for (i = 0; i < noff; i++) {
				uint64_t lo = (uint32_t)ob[(long)i * MCC_GPU_OUT_SLOTS];
				uint64_t hi = (uint32_t)ob[(long)i * MCC_GPU_OUT_SLOTS + 1];
				for (j = 0; j < DEREF_LANE_WORD; j++) {
					uint32_t want = ref[(long)i * DEREF_LANE_WORD + j];
					uint32_t got = arena[(long)i * DEREF_LANE_WORD + j];
					if (want == got)
						continue;
					if (!bad)
						fprintf(stderr,
										"  DEREF t=%#x load=%d store=%d word %d want=%08x "
										"got=%08x\n",
										t, OFF[i], OFF[(i + 5) % noff], j, want, got);
					bad++;
				}
				if ((ob[(long)i * MCC_GPU_OUT_SLOTS + 2] != 0) != cdef[i])
					defbad++;
				if ((int64_t)(lo | (hi << 32)) != cval[i])
					valbad++;
			}
			CHECK(bad == 0,
						"every word of every lane's sub-region matches the reference");
			CHECK(defbad == 0,
						"and the device's verdict on range and alignment matches too");
			CHECK(valbad == 0, "and the value loaded through binding 2 matches");
		} else {
			CHECK(0, "the binding-2 region kernel dispatches");
		}
		free(ref);
		free(in);
		free(ob);
		free(cdef);
		free(cval);
		free(code.p);
	}
}

#define FMT_NSLOT 4
#define FMT_ARENA_WORD 4096
#define FMT_LANE_WORD 32
#define FMT_LANE_BYTE (FMT_LANE_WORD * 4)
#define FMT_LANES MCC_GPU_LOCAL_SIZE
#define FMT_DST 0

static const uint32_t FMT_SIZE[] = {0, 1, 5, 64};
#define FMT_SIZE_N ((int)(sizeof FMT_SIZE / sizeof FMT_SIZE[0]))

static int fmt_kernel(const MccFmtProg *p, MccGpuCode *out) {
	SpvMod m;
	SpvRegion r;
	SpvV arg[MCC_FMT_MAXARG];
	uint32_t base, mbase, size, len;
	uint32_t *code;
	int nw = 0, i;
	if (p->narg > FMT_NSLOT - 1)
		return 0;
	spv_module_begin(&m, FMT_NSLOT);
	base = spv_main_begin(&m, FMT_NSLOT);
	mbase = spv_emit3(&m, SpvOpIAdd, m.id_int, spv_const(&m, FMT_ARENA_WORD),
										spv_emit3(&m, SpvOpIMul, m.id_int, m.lane,
															spv_const(&m, FMT_LANE_WORD)));
	r = spv_region(m.id_mem, mbase, spv_uintc(&m, FMT_LANE_BYTE));
	for (i = 0; i < p->narg; i++) {
		uint32_t lo = spv_load_at(
				&m, spv_emit3(&m, SpvOpIAdd, m.id_int, base, spv_const(&m, i * 2)));
		uint32_t hi = spv_load_at(
				&m, spv_emit3(&m, SpvOpIAdd, m.id_int, base, spv_const(&m, i * 2 + 1)));
		arg[i] = spv_mk(spv_u2(&m, spv_emit2(&m, SpvOpBitcast, m.id_uint, lo),
													 spv_emit2(&m, SpvOpBitcast, m.id_uint, hi)),
										1, 0);
	}
	size = spv_emit2(
			&m, SpvOpBitcast, m.id_uint,
			spv_load_at(&m, spv_emit3(&m, SpvOpIAdd, m.id_int, base,
																spv_const(&m, (FMT_NSLOT - 1) * 2))));
	len = spv_fmt_emit(&m, &r, p, spv_uintc(&m, FMT_DST), size, arg, p->narg);
	if (mcc_slice_mutate) {
		uint32_t wi = spv_region_word(&m, &r, spv_uintc(&m, FMT_DST), 0);
		spv_word_set(&m, r.var, wi,
								 spv_emit3(&m, SpvOpBitwiseXor, m.id_int,
													 spv_word_at(&m, r.var, wi), spv_const(&m, 1)));
	}
	if (m.failed) {
		spv_module_free(&m);
		return 0;
	}
	spv_main_end(&m, m.lane,
							 spv_mk(spv_emit2(&m, SpvOpBitcast, m.id_int, len), 0, 1));
	code = spv_module_finish(&m, &nw);
	spv_module_free(&m);
	if (!code || nw <= 0 || nw > MCC_GPU_CODE_MAX) {
		free(code);
		return 0;
	}
	out->p = code;
	out->n = nw;
	return 1;
}

static uint32_t *fmt_arena(void) {
	void *mem = NULL;
	unsigned long msz = 0;
	if (!mcc_gpu_mem(&mem, &msz) || !mem)
		return NULL;
	if (msz < (unsigned long)(FMT_ARENA_WORD + FMT_LANES * FMT_LANE_WORD) * 4)
		return NULL;
	return (uint32_t *)mem + FMT_ARENA_WORD;
}

static uint32_t fmt_word(int j) { return 0x5A5A0000u + (uint32_t)j * 0x01010101u; }

static int64_t fmt_fit(const MccFmtItem *it, int64_t v) {
	if (it->kind == MCC_FMT_CHR)
		return (int64_t)((uint64_t)v & 0xFFu);
	if (it->wide)
		return v;
	return it->sgn ? (int64_t)(int32_t)v : (int64_t)(uint32_t)v;
}

static const MccFmtItem *fmt_conv(const MccFmtProg *p, int k) {
	int i, n = 0;
	for (i = 0; i < p->n; i++) {
		if (p->it[i].kind == MCC_FMT_LIT)
			continue;
		if (n++ == k)
			return &p->it[i];
	}
	return NULL;
}

static long g_fmt_cmp;
static long g_fmt_bytes;

static void fmt_case(const char *f) {
	MccFmtProg p;
	MccGpuCode code;
	uint32_t *arena, *ref;
	int32_t *in, *ob;
	uint32_t *clen;
	int n = W64_N * FMT_SIZE_N, i, j, k, bad = 0, lenbad = 0;

	if (!mcc_fmt_compile(f, &p)) {
		fprintf(stderr, "FAIL fmt_case: \"%s\" refused: %s\n", f,
						mcc_fmt_why(p.refuse));
		g_checks++;
		g_failures++;
		return;
	}
	if (!g_have_device)
		return;
	arena = fmt_arena();
	if (!arena) {
		CHECK(0, "the shared address space holds the per-lane format arena");
		return;
	}
	if (!fmt_kernel(&p, &code)) {
		fprintf(stderr, "FAIL fmt_case: \"%s\" did not emit\n", f);
		g_checks++;
		g_failures++;
		return;
	}
	ref = (uint32_t *)malloc((size_t)n * FMT_LANE_WORD * sizeof *ref);
	in = (int32_t *)calloc((size_t)n * FMT_NSLOT * MCC_GPU_IN_SLOTS, sizeof *in);
	ob = (int32_t *)calloc((size_t)n * MCC_GPU_OUT_SLOTS, sizeof *ob);
	clen = (uint32_t *)malloc((size_t)n * sizeof *clen);
	if (!ref || !in || !ob || !clen) {
		free(ref);
		free(in);
		free(ob);
		free(clen);
		free(code.p);
		return;
	}
	for (i = 0; i < n; i++)
		for (j = 0; j < FMT_LANE_WORD; j++)
			arena[(long)i * FMT_LANE_WORD + j] = fmt_word(j);
	for (i = 0; i < n; i++) {
		uint32_t *w = ref + (long)i * FMT_LANE_WORD;
		int64_t a[MCC_FMT_MAXARG];
		uint32_t size = FMT_SIZE[i / W64_N];
		for (j = 0; j < FMT_LANE_WORD; j++)
			w[j] = fmt_word(j);
		for (k = 0; k < p.narg; k++) {
			const MccFmtItem *it = fmt_conv(&p, k);
			long sp = (long)i * FMT_NSLOT * MCC_GPU_IN_SLOTS + k * MCC_GPU_IN_SLOTS;
			a[k] = fmt_fit(it, W64[(i + k * 5) % W64_N]);
			in[sp] = (int32_t)(uint32_t)(uint64_t)a[k];
			in[sp + 1] = (int32_t)(uint32_t)((uint64_t)a[k] >> 32);
		}
		in[(long)i * FMT_NSLOT * MCC_GPU_IN_SLOTS +
			 (FMT_NSLOT - 1) * MCC_GPU_IN_SLOTS] = (int32_t)size;
		clen[i] = mcc_fmt_exec(&p, w, FMT_LANE_BYTE, FMT_DST, size, a, p.narg);
	}
	if (mcc_gpu_dispatch_rw2(code.p, code.n, in, n, FMT_NSLOT, ob)) {
		for (i = 0; i < n; i++) {
			uint32_t glen = (uint32_t)ob[(long)i * MCC_GPU_OUT_SLOTS];
			for (j = 0; j < FMT_LANE_WORD; j++) {
				uint32_t want = ref[(long)i * FMT_LANE_WORD + j];
				uint32_t got = arena[(long)i * FMT_LANE_WORD + j];
				g_fmt_bytes += 4;
				if (want == got)
					continue;
				if (bad < 2)
					fprintf(stderr,
									"  FMT \"%s\" lane %d size=%u word %d want=%08x got=%08x\n",
									f, i, FMT_SIZE[i / W64_N], j, want, got);
				bad++;
			}
			if (glen != clen[i]) {
				if (lenbad < 2)
					fprintf(stderr, "  FMT \"%s\" lane %d size=%u len want=%u got=%u\n", f,
									i, FMT_SIZE[i / W64_N], clen[i], glen);
				lenbad++;
			}
			g_fmt_cmp++;
		}
		CHECK(bad == 0, "every byte the device wrote matches the reference");
		CHECK(lenbad == 0, "and the returned length matches at every size");
	} else {
		CHECK(0, "the compiled-format kernel dispatches");
	}
	free(ref);
	free(in);
	free(ob);
	free(clen);
	free(code.p);
}

static void fmt_refusals(void) {
	static const struct {
		const char *f;
		int why;
	} R[] = {{"%s", MCC_FMT_R_PTR},        {"%-3s", MCC_FMT_R_PTR},
					 {"%p", MCC_FMT_R_PTR},        {"a %s b", MCC_FMT_R_PTR},
					 {"%f", MCC_FMT_R_FLOAT},      {"%.17g", MCC_FMT_R_FLOAT},
					 {"%e", MCC_FMT_R_FLOAT},      {"%Lf", MCC_FMT_R_FLOAT},
					 {"%.*s", MCC_FMT_R_PTR},      {"%*d", MCC_FMT_R_SPEC},
					 {"%-10d", MCC_FMT_R_SPEC},    {"%02d", MCC_FMT_R_SPEC},
					 {"%o", MCC_FMT_R_SPEC},       {"%n", MCC_FMT_R_SPEC},
					 {"%+d", MCC_FMT_R_SPEC},      {"%#x", MCC_FMT_R_SPEC}};
	MccFmtProg p;
	int i;
	for (i = 0; i < (int)(sizeof R / sizeof *R); i++) {
		CHECK(mcc_fmt_compile(R[i].f, &p) == 0,
					"a conversion outside tranche 1 is refused, not approximated");
		CHECK(p.refuse == R[i].why,
					"and the refusal says which of the three reasons it is");
	}
	CHECK(mcc_fmt_compile("%d", &p) == 1 && p.narg == 1, "%d is in scope");
	CHECK(mcc_fmt_compile("%016llx", &p) == 1 && p.it[0].width == 16 &&
					p.it[0].pad == '0',
				"and so is the zero-padded wide hex the corpus actually uses");
	CHECK(mcc_fmt_compile("%%", &p) == 1 && p.narg == 0 && p.n == 1 &&
					p.it[0].llen == 1,
				"a doubled percent is one literal byte and consumes no argument");
}

static void fmt_directed(void) {
	static const struct {
		const char *f;
		int64_t a;
		uint32_t size;
		const char *want;
		uint32_t len;
	} D[] = {
			{"%d", -42, 64, "-42", 3},
			{"%d", 0, 64, "0", 1},
			{"%d", -2147483647 - 1, 64, "-2147483648", 11},
			{"%u", 4294967295u, 64, "4294967295", 10},
			{"%llu", (int64_t)0x8000000000000000ull, 64, "9223372036854775808", 19},
			{"%lld", (int64_t)0x8000000000000000ull, 64, "-9223372036854775808", 20},
			{"%llx", -1, 64, "ffffffffffffffff", 16},
			{"%016llx", 255, 64, "00000000000000ff", 16},
			{"%02x", 5, 64, "05", 2},
			{"%08x", 0xdeadbeefu, 64, "deadbeef", 8},
			{"%X", 0xabcu, 64, "ABC", 3},
			{"%c", 'q', 64, "q", 1},
			{"[%d]", 7, 64, "[7]", 3},
			{"%d", 12345, 4, "123", 5},
			{"%d", 12345, 1, "", 5},
			{"%d", 12345, 0, NULL, 5}};
	uint32_t w[FMT_LANE_WORD];
	MccFmtProg p;
	int i, j;
	for (i = 0; i < (int)(sizeof D / sizeof *D); i++) {
		const MccFmtItem *it;
		int64_t a;
		uint32_t got;
		int bad = 0;
		if (!mcc_fmt_compile(D[i].f, &p)) {
			CHECK(0, "the directed formats all compile");
			continue;
		}
		it = fmt_conv(&p, 0);
		a = fmt_fit(it, D[i].a);
		for (j = 0; j < FMT_LANE_WORD; j++)
			w[j] = 0xCCCCCCCCu;
		got = mcc_fmt_exec(&p, w, FMT_LANE_BYTE, FMT_DST, D[i].size, &a, 1);
		CHECK(got == D[i].len, "the reference returns the untruncated length");
		if (!D[i].want) {
			for (j = 0; j < FMT_LANE_WORD; j++)
				if (w[j] != 0xCCCCCCCCu)
					bad++;
			CHECK(bad == 0, "a zero size writes no byte at all, not even the NUL");
			continue;
		}
		for (j = 0; j <= (int)strlen(D[i].want); j++) {
			unsigned b = (w[j >> 2] >> ((j & 3) * 8)) & 0xFFu;
			unsigned e = (unsigned char)D[i].want[j];
			if (b != e) {
				fprintf(stderr, "  FMT ref \"%s\" byte %d want=%02x got=%02x\n", D[i].f,
								j, e, b);
				bad++;
			}
		}
		CHECK(bad == 0, "and writes exactly the hand-written bytes plus a NUL");
	}
}

static void suite_fmt(void) {
	static const char *F[] = {"%d",   "%u",     "%x",      "%X",   "%lld",
														"%llu", "%llx",   "%zu",     "%c",   "%02x",
														"%08x", "%016llx", "[%d]",   "%%",   "n=%d.",
														"%u/%x"};
	int i;
	fmt_refusals();
	fmt_directed();
	if (!g_have_device) {
		if (g_device_required) {
			fprintf(stderr,
							"FAIL slicerun: no usable device but a device is required\n");
			g_failures++;
		}
		return;
	}
	for (i = 0; i < (int)(sizeof F / sizeof *F); i++)
		fmt_case(F[i]);
	g_checks++;
	if (!g_fmt_cmp || !g_fmt_bytes) {
		fprintf(stderr,
						"FAIL suite_fmt: compared %ld lanes and %ld destination bytes\n",
						g_fmt_cmp, g_fmt_bytes);
		g_failures++;
	} else {
		fprintf(stderr, "slicerun: fmt compared %ld lanes, %ld destination bytes\n",
						g_fmt_cmp, g_fmt_bytes);
	}
}

/* ------------------------------------------ the pending-command-buffer UAF -- */

/* Runs last and in its own process: a stranded dispatch disables the device for
 * the rest of the process by design, so this suite cannot share one with any
 * other. The property under test is the one that matters after a timeout --
 * never return data. Before the fix, `done:` destroyed the fence, command pool,
 * pipeline, shader module, descriptor pool, both mappings, both allocations and
 * both buffers while the command buffer was still pending, so the driver
 * recycled that memory into the next dispatch's bin/bout underneath a zombie
 * kernel. The observable consequence was dispatch N+1 returning corrupt values,
 * which is strictly worse than returning nothing. */
/* The shared address space exists and the host can see it. This is the
 * foundation for pointers, malloc and the printf ring: one region, mapped on
 * both sides, where a pointer is a byte offset and offset 0 is NULL. */
static void suite_mem(void) {
	void *base = NULL;
	unsigned long size = 0;
	unsigned char *p;

	if (!g_have_device) {
		if (g_device_required) {
			fprintf(stderr, "FAIL slicerun: no usable device but a device is required\n");
			g_failures++;
		}
		return;
	}
	CHECK(mcc_gpu_mem(&base, &size) == 1, "the shared address space is mappable");
	CHECK(base != NULL, "and the host has a pointer to it");
	CHECK(size >= (1u << 20), "and it is at least the default extent");

	p = (unsigned char *)base;
	CHECK(p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 0,
				"offset 0 is zeroed, so it can be reserved as NULL");

	/* Host writes must survive to the next call, since seeding happens between
	 * dispatches and the region is persistent for the process. */
	p[64] = 0xA5;
	p[65] = 0x5A;
	base = NULL;
	CHECK(mcc_gpu_mem(&base, &size) == 1, "the region is re-mappable");
	CHECK(base == (void *)p, "and it is the same region, not a fresh one");
	CHECK(((unsigned char *)base)[64] == 0xA5 &&
					((unsigned char *)base)[65] == 0x5A,
				"host writes persist across the call, so it can be seeded");
	p[64] = p[65] = 0;
}

static void suite_fault(void) {
	AstArena *a;
	AstLocal root;
	MccSliceWork w;
	MccSliceKernel k;
	int64_t out[AFFINE_N];
	unsigned char def[AFFINE_N];
	int i, st;

	if (!g_have_device) {
		if (g_device_required) {
			fprintf(stderr, "FAIL slicerun: no usable device but a device is required\n");
			g_failures++;
		}
		return;
	}

	a = ast_arena_new();
	root = build_affine(a);
	CHECK(mcc_slice_work_from_ast(a, root, &w) == 1, "affine slice yields work");
	CHECK(mcc_slice_kernel_build(&w, &k) == 1, "the slice lowers");

	mcc_slice_work_bind(&w, AFFINE_IN, AFFINE_N, out, def);
	CHECK(mcc_slice_run_gpu(&w, &k, 0) == MCC_TASK_DONE, "a healthy dispatch runs");
	for (i = 0; i < AFFINE_N; i++)
		CHECK(out[i] == AFFINE_EXPECT[i], "and returns the expected values");

	/* One nanosecond is shorter than any real kernel, so the fence times out
	 * with the command buffer genuinely pending -- a real device, a real
	 * pending submission, no fault injection and no hang. */
	setenv("MCC_GPU_FENCE_NS", "1", 1);
	memset(out, 0xA5, sizeof out);
	memset(def, 0xA5, sizeof def);
	mcc_slice_work_bind(&w, AFFINE_IN, AFFINE_N, out, def);
	st = mcc_slice_run_gpu(&w, &k, 0);
	CHECK(st == MCC_TASK_FAILED, "a timed-out dispatch reports failure");
	CHECK(w.done == 0, "a timed-out dispatch consumes no tuple");

	/* The device must now be closed for business. If it is not, the next
	 * dispatch reuses memory a pending kernel still owns. */
	CHECK(mcc_gpu_stranded() == 1, "the timeout stranded exactly one dispatch");
	CHECK(mcc_gpu_alive() == 0, "a stranded device is marked unusable");

	unsetenv("MCC_GPU_FENCE_NS");
	memset(out, 0xA5, sizeof out);
	mcc_slice_work_bind(&w, AFFINE_IN, AFFINE_N, out, def);
	st = mcc_slice_run_gpu(&w, &k, 0);
	CHECK(st == MCC_TASK_FAILED,
				"a dispatch after a strand fails rather than reusing pending memory");
	CHECK(mcc_gpu_stranded() == 1, "and it does not strand a second time");

	mcc_slice_kernel_free(&k);
	ast_arena_free(a);
}

/* --------------------------------------------------------- H6: the cost -- */

/* Phase 0a. Both executors, timed the way a caller pays for them, so a
 * promotion decision has something to rest on. The device column deliberately
 * includes the pack, the dispatch, the readback and the unpack.
 *
 * Two batch sizes give the line: per-lane is the slope, fixed cost is the
 * intercept. Break-even is where the device's fixed cost is amortised by its
 * per-lane advantage, and it is infinite whenever a lane is not actually
 * cheaper than a CPU tuple -- which is the case this table exists to detect. */
#define COST_SMALL 64
#define COST_LARGE 65536
#define COST_REPS 5

static int64_t seed_value(long t, int k);

typedef struct CostRow {
	int nodes;
	int nlive;
	double cpu_per_tuple;
	double gpu_fixed;
	double gpu_per_lane;
	double breakeven;
	int ok;
} CostRow;

static double cost_min_gpu(MccSliceWork *w, MccSliceKernel *k, const int64_t *in,
													 int ntuple, int64_t *out, unsigned char *def) {
	double best = 0;
	int r;
	for (r = 0; r < COST_REPS; r++) {
		double t;
		mcc_slice_time_reset();
		mcc_slice_work_bind(w, in, ntuple, out, def);
		if (mcc_slice_run_gpu(w, k, 0) != MCC_TASK_DONE)
			return -1;
		t = mcc_slice_gpu_time();
		if (!r || t < best)
			best = t;
	}
	return best;
}

static double cost_min_cpu(MccSliceWork *w, const int64_t *in, int ntuple,
													 int64_t *out, unsigned char *def) {
	double best = 0;
	int r;
	for (r = 0; r < COST_REPS; r++) {
		double t;
		mcc_slice_time_reset();
		mcc_slice_work_bind(w, in, ntuple, out, def);
		if (mcc_slice_run_cpu(w, 0) != MCC_TASK_DONE)
			return -1;
		t = mcc_slice_cpu_time();
		if (!r || t < best)
			best = t;
	}
	return best;
}

static int cost_measure(AstArena *a, AstLocal root, CostRow *row) {
	MccSliceWork w;
	MccSliceKernel k;
	int64_t *in, *out;
	unsigned char *def;
	double c, g1, g2;
	int i, j;

	memset(row, 0, sizeof *row);
	if (!mcc_slice_work_from_ast(a, root, &w))
		return 0;

	in = (int64_t *)malloc((size_t)COST_LARGE * MCC_SLICE_MAXLIVE * sizeof *in);
	out = (int64_t *)malloc((size_t)COST_LARGE * sizeof *out);
	def = (unsigned char *)malloc((size_t)COST_LARGE);
	if (!in || !out || !def) {
		free(in); free(out); free(def);
		return 0;
	}
	for (i = 0; i < COST_LARGE; i++)
		for (j = 0; j < w.nlive; j++)
			in[(long)i * w.nlive + j] = seed_value(i, j);

	c = cost_min_cpu(&w, in, COST_LARGE, out, def);
	if (c < 0 || !mcc_slice_kernel_build(&w, &k)) {
		free(in); free(out); free(def);
		return 0;
	}
	g1 = cost_min_gpu(&w, &k, in, COST_SMALL, out, def);
	g2 = cost_min_gpu(&w, &k, in, COST_LARGE, out, def);
	mcc_slice_kernel_free(&k);
	free(in); free(out); free(def);
	if (g1 < 0 || g2 < 0)
		return 0;

	row->nodes = w.nodes;
	row->nlive = w.nlive;
	row->cpu_per_tuple = c / COST_LARGE;
	row->gpu_per_lane = (g2 - g1) / (COST_LARGE - COST_SMALL);
	row->gpu_fixed = g1 - row->gpu_per_lane * COST_SMALL;
	if (row->gpu_fixed < 0)
		row->gpu_fixed = 0;
	if (row->cpu_per_tuple > row->gpu_per_lane)
		row->breakeven = row->gpu_fixed / (row->cpu_per_tuple - row->gpu_per_lane);
	else
		row->breakeven = -1; /* no batch size wins */
	row->ok = 1;
	return 1;
}

static long g_cost_rows, g_cost_wins;
static double g_cost_best_be = -1;

static void cost_report(AstArena *a, AstLocal root, const char *label) {
	CostRow r;
	if (!cost_measure(a, root, &r))
		return;
	g_cost_rows++;
	if (r.breakeven > 0 &&
			(g_cost_best_be < 0 || r.breakeven < g_cost_best_be))
		g_cost_best_be = r.breakeven;
	if (r.breakeven > 0 && r.breakeven <= COST_LARGE)
		g_cost_wins++;
	printf("%s\t%d\t%d\t%.2f\t%.0f\t%.3f\t", label, r.nodes, r.nlive,
				 r.cpu_per_tuple, r.gpu_fixed, r.gpu_per_lane);
	if (r.breakeven < 0)
		printf("never\n");
	else
		printf("%.0f\n", r.breakeven);
}

/* The corpus answers "do today's slices win?" -- it cannot answer "could any
 * slice win?", because the greedy top-down scan plus the strict-width rule caps
 * real slices at 3-4 nodes. This sweeps synthetic slices of growing size to
 * find the node count at which the device's per-lane cost is finally below the
 * CPU's per-tuple cost, which is the number the whole plan turns on. */
static AstLocal build_chain(AstArena *a, int nodes, int nlive) {
	AstLocal e = mk_ref(a, -8, VT_INT);
	int i;
	for (i = 1; i < nodes; i++) {
		int op = (i % 3 == 0) ? '*' : (i % 3 == 1) ? '+' : '^';
		AstLocal r = (i % 2 && nlive > 1)
										 ? mk_ref(a, (int32_t)(-8 - 8 * (i % nlive)), VT_INT)
										 : mk_lit(a, (i % 7) + 1, VT_INT);
		e = mk_bin(a, op, e, r, VT_INT);
	}
	return e;
}

static int cost_synth(void) {
	static const int SIZES[] = {2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
	int i;
	if (!g_have_device) {
		printf("slicerun: no usable device; the H6 sweep needs one\n");
		return 77;
	}
	printf("# H6 synthetic sweep -- device %s\n", g_devname);
	printf("# slice\tnodes\tnlive\tcpu_ns_per_tuple\tgpu_fixed_ns\t"
				 "gpu_ns_per_lane\tbreakeven_ntuple\n");
	for (i = 0; i < (int)(sizeof SIZES / sizeof SIZES[0]); i++) {
		AstArena *a = ast_arena_new();
		AstLocal root = build_chain(a, SIZES[i], 2);
		char lab[32];
		snprintf(lab, sizeof lab, "chain%d", SIZES[i]);
		cost_report(a, root, lab);
		ast_arena_free(a);
	}
	printf("# rows=%ld  win-within-%d-lanes=%ld  lowest-breakeven=%.0f\n",
				 g_cost_rows, COST_LARGE, g_cost_wins, g_cost_best_be);
	if (!g_cost_rows) {
		printf("slicerun: FAIL (the sweep produced no rows)\n");
		return 1;
	}
	return 0;
}

/* -------------------------------------------------- real arenas from RIR -- */

#define RAW_MAX (1 << 22)

typedef struct RawNode {
	int kind, op, type_t;
	long long ival;
	unsigned first_child, next_sib;
	unsigned long long type_ref, sym, fbits;
	unsigned bp, bs;
	int size;
	int etype;
} RawNode;

static AstArena *rebuild_arena(const RawNode *raw, int n, AstLocal *root_out,
															 long root_in) {
	AstArena *a = ast_arena_new();
	int i;
	for (i = 0; i < n; i++) {
		AstLocal id = ast_node(a, (uint16_t)raw[i].kind);
		if ((long)id != i) {
			ast_arena_free(a);
			return NULL;
		}
		ast_set_op(a, id, raw[i].op);
		/* N12: consume all 12 dumped fields, not 7. sym and type_ref are interned
		 * ids after N7, not addresses, so they are stable identities -- but they
		 * are still installed into pointer-shaped slots, which is only safe
		 * because nothing on this path dereferences them (verified: zero uses of
		 * ast_sym/ast_type_ref/ast_fbits/ast_type_bp/ast_type_bs in
		 * ast_eval_slice.h and mccgpu.h). A consumer that needs the real Sym
		 * would need a side table, not this. */
		ast_set_type(a, id, raw[i].type_t, (uint64_t)raw[i].type_ref);
		if (raw[i].bp || raw[i].bs)
			ast_set_type_bf(a, id, raw[i].type_t, (uint64_t)raw[i].type_ref,
											(int)raw[i].bp, (int)raw[i].bs);
		ast_set_ival(a, id, (uint64_t)raw[i].ival);
		if (raw[i].sym)
			ast_set_sym(a, id, (uint64_t)raw[i].sym);
		if (raw[i].fbits)
			ast_set_fbits(a, id, (uint64_t)raw[i].fbits);
	}
	for (i = 0; i < n; i++) {
		unsigned c = raw[i].first_child;
		while (c != 0xFFFFFFFFu && (int)c < n) {
			ast_add_child(a, (AstLocal)i, (AstLocal)c);
			c = raw[c].next_sib;
		}
	}
	*root_out = (AstLocal)root_in;
	return a;
}

static long g_arena_bodies, g_arena_slices, g_arena_tuples, g_arena_mismatch;
static long g_arena_gpu_slices;

/* Deterministic, cheap, and spread across sign and magnitude: the point is a
 * value-for-value comparison of two runners on real shapes, not a search. */
static int64_t seed_value(long t, int k) {
	static const int64_t seeds[8] = {0, 1, -1, 2, 7, -3, 1000, -12345};
	return seeds[(t * 3 + k * 5) & 7];
}

static void dump_tree(AstArena *a, AstLocal n, int d) {
	AstLocal c;
	int i;
	for (i = 0; i < d; i++)
		fprintf(stderr, "  ");
	fprintf(stderr, "kind=%d op=%#x type=%#x wtype=%#x ival=%lld\n",
					(int)ast_kind(a, n), ast_op(a, n), ast_type_t(a, n),
					ast_eval_slice_wtype(a, n), (long long)ast_ival(a, n));
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		dump_tree(a, c, d + 1);
}

static int g_dumped;

static void run_real_slice(AstArena *a, AstLocal root, int quiet) {
	MccSliceWork w;
	MccSliceKernel k;
	int64_t in[8 * MCC_SLICE_MAXLIVE];
	int64_t cout[8], gout[8];
	unsigned char cdef[8], gdef[8];
	int t, j;

	if (!mcc_slice_work_from_ast(a, root, &w))
		return;
	if (w.nodes < 3)
		return;
	g_arena_slices++;

	for (t = 0; t < 8; t++)
		for (j = 0; j < w.nlive; j++)
			in[t * w.nlive + j] = seed_value(t, j);

	mcc_slice_work_bind(&w, in, 8, cout, cdef);
	if (mcc_slice_run_cpu(&w, 0) != MCC_TASK_DONE)
		return;
	g_arena_tuples += 8;

	if (!g_have_device || !mcc_slice_kernel_build(&w, &k))
		return;
	g_arena_gpu_slices++;
	mcc_slice_work_bind(&w, in, 8, gout, gdef);
	if (mcc_slice_run_gpu(&w, &k, 0) != MCC_TASK_DONE) {
		mcc_slice_kernel_free(&k);
		g_arena_mismatch++;
		return;
	}
	for (t = 0; t < 8; t++) {
		if (gdef[t] != cdef[t] || (cdef[t] && gout[t] != cout[t])) {
			if (!quiet && !g_dumped) {
				g_dumped = 1;
				fprintf(stderr, "--- first divergent slice ---\n");
				dump_tree(a, root, 1);
				fprintf(stderr, "--- inputs: ");
				for (j = 0; j < w.nlive; j++)
					fprintf(stderr, "off%d=%d val=%lld  ", j, w.off[j],
									(long long)in[t * w.nlive + j]);
				fprintf(stderr, "\n");
			}
			if (!quiet && g_arena_mismatch < 8)
				fprintf(stderr,
								"  MISMATCH nodes=%d nlive=%d kind=%d op=%#x wtype=%#x roott=%#x "
								"c0k=%d c0t=%#x c0w=%#x tuple=%d cpu=%lld gpu=%lld\n",
								w.nodes, w.nlive, (int)ast_kind(a, root), ast_op(a, root),
								w.wtype, ast_type_t(a, root),
								(int)ast_kind(a, ast_first_child(a, root)),
								ast_type_t(a, ast_first_child(a, root)),
								ast_eval_slice_wtype(a, ast_first_child(a, root)), t,
								(long long)cout[t], (long long)gout[t]);
			g_arena_mismatch++;
		}
	}
	mcc_slice_kernel_free(&k);
}

static int g_cost_mode;
static int g_cost_synth;
static int g_lax;

/* Three counters, not one. `accepted` is what the predicate admits; `built` is
 * what actually lowered to a kernel; `compared` is what a device dispatch
 * actually checked against the CPU. Only the last is evidence. Reporting the
 * first as coverage overstated it 2.26x, because a `return expr;`-only block is
 * accepted with nstmt == 0 and mcc_slice_frame_kernel_build refuses it. */
static long g_frame_slices, g_frame_stmts, g_frame_mismatch;
static long g_frame_built, g_frame_compared, g_frame_mem;

#define FRAME_NT MCC_GPU_LOCAL_SIZE
#define FRAME_PTR_BASE (128 * 1024)
#define FRAME_PTR_STRIDE (8 * 1024)
#define FRAME_PTR_SLOT 72
#define FRAME_PTR_SPAN (FRAME_PTR_BASE + FRAME_NT * FRAME_PTR_STRIDE)

static unsigned char *g_rw;
static unsigned long g_rwsz;
static unsigned char *g_rw_pre, *g_rw_cpu;

static int g_no_ptr;

static void frame_ptr_arm(void) {
	void *base = NULL;
	unsigned long sz = 0;
	if (g_no_ptr)
		return;
	if (!mcc_gpu_mem(&base, &sz) || !base || sz < FRAME_PTR_SPAN)
		return;
	if (sz > 0x7ffffffcul)
		sz = 0x7ffffffcul;
	g_rw_pre = (unsigned char *)malloc(sz);
	g_rw_cpu = (unsigned char *)malloc(sz);
	if (!g_rw_pre || !g_rw_cpu) {
		free(g_rw_pre);
		free(g_rw_cpu);
		g_rw_pre = g_rw_cpu = NULL;
		return;
	}
	g_rw = (unsigned char *)base;
	g_rwsz = sz;
	ast_eval_slice_rw = (uint32_t *)base;
	ast_eval_slice_rw_base = (int64_t)(intptr_t)base;
	ast_eval_slice_rw_nbyte = (int32_t)sz;
}

static unsigned char frame_ptr_byte(long b) {
	long k = b % FRAME_PTR_SLOT;
	unsigned char v = k ? (unsigned char)(1 + (k * 7) % 31) : 0;
	return (b % 37) ? v : (unsigned char)(v ^ 0x40);
}

static void frame_ptr_seed(void) {
	long i;
	for (i = 0; i < FRAME_NT * (long)FRAME_PTR_STRIDE; i++)
		g_rw[FRAME_PTR_BASE + i] = frame_ptr_byte(i);
}

static int64_t frame_ptr_addr(int t, int j) {
	return (int64_t)(intptr_t)(g_rw + FRAME_PTR_BASE +
														 (long)t * FRAME_PTR_STRIDE + 2048 +
														 (long)j * FRAME_PTR_SLOT);
}

/* Frame runs from real arenas, differentialled the same way expression slices
 * are: seed N independent frames, run both executors, compare every slot and
 * the returned value. */
static void run_real_frame(AstArena *a, AstLocal bb, int quiet) {
	MccSliceFrame fr;
	MccSliceKernel k;
	int64_t cf[FRAME_NT * MCC_SLICE_MAXSLOT], gf[FRAME_NT * MCC_SLICE_MAXSLOT];
	int64_t crv[FRAME_NT], grv[FRAME_NT];
	int cdf[FRAME_NT];
	unsigned char gdf[FRAME_NT];
	int t, j, bad = 0, membad = 0, usemem;

	if (!mcc_slice_frame_from_ast(a, bb, &fr))
		return;
	g_frame_slices++;
	g_frame_stmts += fr.nstmt;
	usemem = fr.nptr > 0 && g_rw != NULL;
	for (t = 0; t < FRAME_NT; t++)
		for (j = 0; j < fr.nslot; j++)
			cf[t * fr.nslot + j] = gf[t * fr.nslot + j] =
					fr.sptr[j] && usemem ? frame_ptr_addr(t, j) : seed_value(t, j);
	if (usemem) {
		frame_ptr_seed();
		memcpy(g_rw_pre, g_rw, g_rwsz);
	}
	for (t = 0; t < FRAME_NT; t++)
		if (!mcc_slice_frame_exec_cpu2(&fr, cf + (long)t * fr.nslot, &crv[t],
																	 &cdf[t])) {
			if (usemem)
				memcpy(g_rw, g_rw_pre, g_rwsz);
			return;
		}
	if (usemem) {
		memcpy(g_rw_cpu, g_rw, g_rwsz);
		memcpy(g_rw, g_rw_pre, g_rwsz);
	}
	if (!g_have_device || !mcc_slice_frame_kernel_build(&fr, &k))
		return;
	g_frame_built++;
	if (mcc_slice_run_frame_gpu(&fr, &k, gf, FRAME_NT, grv, gdf) != MCC_TASK_DONE) {
		mcc_slice_kernel_free(&k);
		g_frame_mismatch++;
		return;
	}
	g_frame_compared++;
	if (usemem) {
		g_frame_mem++;
		if (memcmp(g_rw_cpu, g_rw, g_rwsz)) {
			membad = 1;
			bad++;
		}
	}
	for (t = 0; t < FRAME_NT; t++) {
		/* Only a run that ends in Return has a value to compare. Without one the
		 * kernel still writes the out slots (spv_main_end always does), so the
		 * flag there is a dummy, not a verdict -- unless the run resolves an
		 * address at run time, in which case the flag carries the index bound and
		 * both executors have committed to the same verdict for it. */
		if (fr.ret != AST_NONE &&
				((int)gdf[t] != cdf[t] || (cdf[t] && grv[t] != crv[t])))
			bad++;
		if (fr.ret == AST_NONE && fr.nidx && (int)gdf[t] != cdf[t])
			bad++;
		for (j = 0; j < fr.nslot; j++)
			if (cf[t * fr.nslot + j] != gf[t * fr.nslot + j])
				bad++;
	}
	if (bad) {
		if (!quiet && g_frame_mismatch < 4) {
			fprintf(stderr, "  FRAME MISMATCH nslot=%d nstmt=%d ret=%d mem=%d\n",
							fr.nslot, fr.nstmt, fr.ret != AST_NONE, membad);
			if (membad) {
				unsigned long b;
				int shown = 0;
				for (b = 0; b < g_rwsz && shown < 4; b++)
					if (g_rw_cpu[b] != g_rw[b]) {
						fprintf(stderr, "    mem byte %lu cpu=%02x gpu=%02x\n", b,
										g_rw_cpu[b], g_rw[b]);
						shown++;
					}
			}
			for (t = 0; t < 2; t++)
				for (j = 0; j < fr.nslot; j++)
					if (cf[t * fr.nslot + j] != gf[t * fr.nslot + j])
						fprintf(stderr,
										"    frame%d slot%d off=%d seed=%lld cpu=%lld gpu=%lld\n", t,
										j, fr.slot[j], (long long)seed_value(t, j),
										(long long)cf[t * fr.nslot + j],
										(long long)gf[t * fr.nslot + j]);
			if (fr.ret != AST_NONE)
				fprintf(stderr, "    ret cpu=%lld/%d gpu=%lld/%d\n", (long long)crv[0],
								cdf[0], (long long)grv[0], gdf[0]);
			fprintf(stderr, "    def cpu=");
			for (t = 0; t < FRAME_NT; t++)
				fprintf(stderr, "%d", cdf[t]);
			fprintf(stderr, " gpu=");
			for (t = 0; t < FRAME_NT; t++)
				fprintf(stderr, "%d", gdf[t]);
			fprintf(stderr, " nidx=%d nptr=%d\n", fr.nidx, fr.nptr);
			for (j = 0; j < fr.nslot; j++)
				fprintf(stderr, "    slot%d off=%d sptr=%d seed=%lld\n", j, fr.slot[j],
								fr.sptr[j], (long long)(fr.sptr[j] && usemem
																						? frame_ptr_addr(0, j)
																						: seed_value(0, j)));
			fprintf(stderr, "    run: nslot=%d nstmt=%d nctrl=%d nloop=%d ret=%d\n",
							fr.nslot, fr.nstmt, fr.nctrl, fr.nloop, fr.ret != AST_NONE);
		}
		g_frame_mismatch++;
	}
	mcc_slice_kernel_free(&k);
}

static void scan_subtree(AstArena *a, AstLocal n, int quiet, long limit) {
	AstLocal c;
	MccSliceWork probe;
	if (n == AST_NONE)
		return;
	if (!g_cost_mode && ast_kind(a, n) == AST_BasicBlock)
		run_real_frame(a, n, quiet);
	if (limit && g_arena_slices >= limit)
		return;
	if (mcc_slice_work_from_ast(a, n, &probe) && probe.nodes >= 3) {
		if (g_cost_mode) {
			char lab[64];
			g_arena_slices++;
			snprintf(lab, sizeof lab, "%016llx",
							 (unsigned long long)ast_slice_ident_hash(a, n));
			cost_report(a, n, lab);
			return;
		}
		run_real_slice(a, n, quiet);
		return;
	}
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		scan_subtree(a, c, quiet, limit);
}

static int g_census;
static int g_inline = 1;
static long g_cn_blocks, g_cn_elig, g_cn_op8n, g_cn_op9n, g_cn_op6n;
static long g_cn_op8b, g_cn_op9b, g_cn_op6b;
static long g_cn_op8t, g_cn_op9t, g_cn_op6t;
static long g_cn_stfield, g_cn_staddr, g_cn_stother, g_cn_stlocal;
static long g_cn_deref, g_cn_derefok, g_cn_pstore, g_cn_pstoreok;
static long g_cn_pinc, g_cn_pincok, g_cn_dblocks, g_cn_pblocks;

/* D4b step 0. The board ranks "internal calls on the device" on 12,901 blocks
 * / 78.01%, a figure no committed tool in this tree produces: it was an ad-hoc
 * pass over MCC_ARENA_DUMP text, and `slicerun --census` -- the only per-block
 * counter here -- carries no callee classification at all. The two halves have
 * simply never been joined, so the number has never been ratcheted and cannot
 * be reproduced on demand.
 *
 * Joined here. The block unit is the existing one (a non-empty AST_BasicBlock,
 * 947 over the tests/exec 60), the callee class comes from the `[inv]` records
 * the dump already emits, and the classes follow tools/node-census.py: a
 * callee is INTERNAL when its name is the `fn=` of some body in the same dump,
 * INDIRECT when the dump wrote `?`, EXTERNAL otherwise.
 *
 * Three block-level figures come out, and only the third predicts payoff:
 *
 *   inv-blocks + class split  reproduces the board's methodology, i.e. blocks
 *                             whose invokes are all internal. It is an
 *                             eligibility statement about the Invoke node and
 *                             claims nothing about the rest of the block.
 *   sole                      blocks that mcc_slice_frame_stmt_ok refuses
 *                             today and would accept if every Invoke in them
 *                             were free. This is the block-granularity twin of
 *                             what rir_low_take already banks per body, where
 *                             `call` is the sole blocker of ~0.01% of body
 *                             bytes. A block is only counted when every
 *                             argument of every Invoke in it is itself
 *                             lowerable -- a call whose arguments cannot reach
 *                             the device is not unblocked by putting the call
 *                             there.
 *   inline-unblocked          blocks the leaf inliner in src/slice_inline.h
 *                             actually unblocks, which is a floor under the
 *                             first increment rather than a ceiling over the
 *                             phase. */
static long g_cn_invblk, g_cn_inv_int, g_cn_inv_ext, g_cn_inv_mix, g_cn_inv_ind;
static long g_cn_sole, g_cn_soleonly, g_cn_inlblk, g_cn_invn;
static signed char *g_cn_wasel;
static long g_cn_wascap;

#define INV_NAMEMAX 128

typedef struct LeafEnt {
	char name[INV_NAMEMAX];
	AstArena *a;
	AstLocal root;
	unsigned long long hash;
} LeafEnt;

static LeafEnt *g_leaf;
static int g_leaf_n, g_leaf_cap;
static char (*g_defn)[INV_NAMEMAX];
static long g_defn_n, g_defn_cap;
static int g_defn_sorted;
static signed char *g_invcls;
static int *g_invleaf;
static long g_invcap;

static int defn_cmp(const void *x, const void *y) {
	return strcmp((const char *)x, (const char *)y);
}

static void defn_add(const char *s) {
	if (g_defn_n == g_defn_cap) {
		long nc = g_defn_cap ? g_defn_cap * 2 : 1024;
		char (*p)[INV_NAMEMAX] = realloc(g_defn, (size_t)nc * INV_NAMEMAX);
		if (!p)
			return;
		g_defn = p;
		g_defn_cap = nc;
	}
	snprintf(g_defn[g_defn_n], INV_NAMEMAX, "%s", s);
	g_defn_n++;
	g_defn_sorted = 0;
}

static int defn_has(const char *s) {
	if (!g_defn_n)
		return 0;
	if (!g_defn_sorted) {
		qsort(g_defn, (size_t)g_defn_n, INV_NAMEMAX, defn_cmp);
		g_defn_sorted = 1;
	}
	return bsearch(s, g_defn, (size_t)g_defn_n, INV_NAMEMAX, defn_cmp) != NULL;
}

static int leaf_find(const char *s) {
	int i;
	for (i = 0; i < g_leaf_n; i++)
		if (!strcmp(g_leaf[i].name, s))
			return i;
	return -1;
}

/* A name that is defined twice with structurally different bodies is two
 * different static functions in two translation units, and the dump cannot
 * tell them apart. Grafting either one into the other's caller would be a
 * miscompile both executors would agree on, so such a name is dropped rather
 * than resolved. */
static void leaf_offer(const char *name, AstArena *a, AstLocal root,
											 unsigned long long h) {
	int i = leaf_find(name);
	if (i >= 0) {
		if (g_leaf[i].hash != h && g_leaf[i].a) {
			ast_arena_free(g_leaf[i].a);
			g_leaf[i].a = NULL;
		}
		ast_arena_free(a);
		return;
	}
	if (g_leaf_n == g_leaf_cap) {
		int nc = g_leaf_cap ? g_leaf_cap * 2 : 64;
		LeafEnt *p = realloc(g_leaf, (size_t)nc * sizeof *p);
		if (!p) {
			ast_arena_free(a);
			return;
		}
		g_leaf = p;
		g_leaf_cap = nc;
	}
	snprintf(g_leaf[g_leaf_n].name, INV_NAMEMAX, "%s", name);
	g_leaf[g_leaf_n].a = a;
	g_leaf[g_leaf_n].root = root;
	g_leaf[g_leaf_n].hash = h;
	g_leaf_n++;
}

static AstArena *slicerun_leaf_hook(AstArena *a, AstLocal inv, AstLocal *root) {
	int i;
	(void)a;
	if (!g_invleaf || (long)inv >= g_invcap)
		return NULL;
	i = g_invleaf[inv];
	if (i < 0 || !g_leaf[i].a)
		return NULL;
	*root = g_leaf[i].root;
	return g_leaf[i].a;
}

static void inv_reset(long n) {
	long i;
	if (n > g_invcap) {
		free(g_invcls);
		free(g_invleaf);
		g_invcls = malloc((size_t)n);
		g_invleaf = malloc((size_t)n * sizeof *g_invleaf);
		g_invcap = (g_invcls && g_invleaf) ? n : 0;
	}
	for (i = 0; i < g_invcap; i++) {
		g_invcls[i] = -1;
		g_invleaf[i] = -1;
	}
}

static void inv_set(long node, const char *callee) {
	if (!g_invcls || node < 0 || node >= g_invcap)
		return;
	g_invcls[node] = !strcmp(callee, "?") ? 0 : defn_has(callee) ? 2 : 1;
	g_invleaf[node] = leaf_find(callee);
}

static void leaf_pass0(const char *fn, const RawNode *raw, int n, long root) {
	unsigned long long h = 1469598103934665603ULL;
	MccSliceLeaf L;
	AstArena *a;
	AstLocal rt;
	int i;
	if (n > MCC_SLICE_INL_MAXNODE + 8)
		return;
	for (i = 0; i < n; i++) {
		int k = raw[i].kind;
		if (k != AST_BasicBlock && k != AST_Return && k != AST_Ref &&
				k != AST_Literal && k != AST_Unary && k != AST_Binary &&
				k != AST_Convert && k != AST_If)
			return;
		h = (h ^ (unsigned long long)k) * 1099511628211ULL;
		h = (h ^ (unsigned long long)raw[i].op) * 1099511628211ULL;
		h = (h ^ (unsigned long long)raw[i].type_t) * 1099511628211ULL;
		h = (h ^ (unsigned long long)raw[i].ival) * 1099511628211ULL;
		h = (h ^ raw[i].first_child) * 1099511628211ULL;
		h = (h ^ raw[i].next_sib) * 1099511628211ULL;
	}
	a = rebuild_arena(raw, n, &rt, root);
	if (!a)
		return;
	if (!mcc_slice_leaf_scan(a, rt, &L)) {
		ast_arena_free(a);
		return;
	}
	leaf_offer(fn, a, rt, h);
}

static int has_op(AstArena *a, AstLocal n, int op) {
	AstLocal c;
	if (n == AST_NONE)
		return 0;
	if (ast_kind(a, n) == AST_If && ast_op(a, n) == op)
		return 1;
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (has_op(a, c, op))
			return 1;
	return 0;
}

static int top_op(AstArena *a, AstLocal bb, int op) {
	AstLocal c;
	for (c = ast_first_child(a, bb); c != AST_NONE; c = ast_next_sib(a, c))
		if (ast_kind(a, c) == AST_If && ast_op(a, c) == op)
			return 1;
	return 0;
}

static void census_stores(AstArena *a, int n) {
	AstLocal d;
	int32_t off;
	int i;
	for (i = 0; i < n; i++) {
		if (ast_kind(a, (AstLocal)i) != AST_Store || ast_nchild(a, (AstLocal)i) != 2)
			continue;
		d = ast_child(a, (AstLocal)i, 0);
		if (mcc_slice_is_local_ref(a, d, &off))
			g_cn_stlocal++;
		else if (ast_kind(a, d) == AST_Unary && ast_op(a, d) == AST_EVAL_OP_MEMBER)
			g_cn_stfield++;
		else if (ast_kind(a, d) == AST_Unary && ast_op(a, d) == AST_EVAL_OP_ADDR)
			g_cn_staddr++;
		else
			g_cn_stother++;
	}
}

static void block_inv(AstArena *a, AstLocal n, int *cnt, int *cls) {
	AstLocal c;
	if (ast_kind(a, n) == AST_Invoke) {
		int k = ((long)n < g_invcap && g_invcls[n] >= 0) ? g_invcls[n] : 0;
		(*cnt)++;
		*cls |= 1 << k;
	}
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		block_inv(a, c, cnt, cls);
}

/* The "if the call were free" transform. A value-position Invoke becomes a
 * constant of its own type; a statement-position one is dropped from its
 * block. Both are refused when any argument is not itself lowerable, which
 * leaves the block blocked -- a device call whose arguments live on the host
 * is not a call the device can make. */
static void census_free_invokes(AstArena *a, signed char *mark) {
	AstLocal nn = ast_count(a), n, c;
	for (n = 0; n < nn; n++) {
		uint32_t k, nc;
		int t;
		mark[n] = 0;
		if (ast_kind(a, n) != AST_Invoke)
			continue;
		nc = ast_nchild(a, n);
		for (k = 1; k < nc; k++)
			if (!ast_eval_slice_kind_ok(a, ast_child(a, n, k), 1))
				break;
		if (k != nc)
			continue;
		mark[n] = ast_kind(a, ast_parent(a, n)) == AST_BasicBlock ? 2 : 1;
		t = ast_type_t(a, n);
		if (ast_bad_type(t) || is_float(t) || !ast_eval_slice_intt(t))
			t = VT_INT;
		ast_clear_children(a, n);
		ast_set_kind(a, n, AST_Literal);
		ast_set_op(a, n, VT_CONST);
		ast_set_type(a, n, t, 0);
		ast_set_ival(a, n, 0);
		ast_set_sym(a, n, 0);
	}
	for (n = 0; n < nn; n++) {
		AstLocal keep[MCC_SLICE_MAXSTMT * 4];
		int nk = 0, drop = 0, j;
		if (ast_kind(a, n) != AST_BasicBlock)
			continue;
		for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c)) {
			if (mark[c] == 2) {
				drop = 1;
				continue;
			}
			if (nk == (int)(sizeof keep / sizeof *keep)) {
				drop = 0;
				break;
			}
			keep[nk++] = c;
		}
		if (!drop)
			continue;
		ast_clear_children(a, n);
		for (j = 0; j < nk; j++)
			ast_add_child(a, n, keep[j]);
	}
}

static void census_probe(AstArena *a, int n, int free_invokes, long *unblk,
												 long *emptied) {
	AstArena *p = ast_arena_clone(a);
	MccSliceFrame fr;
	signed char *mark;
	int i;
	if (!p)
		return;
	mark = calloc((size_t)ast_count(p) + 1, 1);
	if (!mark) {
		ast_arena_free(p);
		return;
	}
	if (free_invokes)
		census_free_invokes(p, mark);
	else
		mcc_slice_inline_arena(p);
	for (i = 0; i < n; i++) {
		if (ast_kind(p, (AstLocal)i) != AST_BasicBlock || !g_cn_wasel ||
				g_cn_wasel[i] != 1)
			continue;
		if (ast_first_child(p, (AstLocal)i) == AST_NONE) {
			if (emptied)
				(*emptied)++;
			continue;
		}
		if (mcc_slice_frame_from_ast(p, (AstLocal)i, &fr))
			(*unblk)++;
	}
	free(mark);
	ast_arena_free(p);
}

static int census_ptr_ref(AstArena *a, AstLocal n) {
	int r, t;
	if (n == AST_NONE || ast_kind(a, n) != AST_Ref)
		return 0;
	r = ast_op(a, n);
	if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
		return 0;
	t = ast_type_t(a, n);
	return t && !ast_bad_type(t) && !(t & VT_ARRAY) && (t & VT_BTYPE) == VT_PTR;
}

static void census_ptrs(AstArena *a, int n) {
	int32_t pf;
	int i, et;
	for (i = 0; i < n; i++) {
		AstLocal x = (AstLocal)i;
		if (ast_kind(a, x) == AST_Load && census_ptr_ref(a, ast_first_child(a, x))) {
			g_cn_deref++;
			if (ast_eval_slice_deref(a, x, &pf, &et))
				g_cn_derefok++;
		}
		if (ast_kind(a, x) == AST_Store && ast_nchild(a, x) == 2) {
			AstLocal d = ast_child(a, x, 0);
			if (ast_kind(a, d) == AST_Load && census_ptr_ref(a, ast_first_child(a, d))) {
				g_cn_pstore++;
				if (ast_eval_slice_deref(a, d, &pf, &et) &&
						ast_eval_slice_tsize(et) >= 4)
					g_cn_pstoreok++;
			}
		}
		if (ast_kind(a, x) == AST_Unary &&
				(ast_op(a, x) == TOK_INC || ast_op(a, x) == TOK_DEC) &&
				census_ptr_ref(a, ast_first_child(a, x))) {
			g_cn_pinc++;
			if (ast_eval_slice_ptr_et(a, ast_first_child(a, x)))
				g_cn_pincok++;
		}
	}
}

static int census_has_deref(AstArena *a, AstLocal n) {
	AstLocal c;
	if (n == AST_NONE)
		return 0;
	if (ast_kind(a, n) == AST_Load && census_ptr_ref(a, ast_first_child(a, n)))
		return 1;
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (census_has_deref(a, c))
			return 1;
	return 0;
}

static void census_arena(AstArena *a, int n) {
	int i;
	long anyinv = 0;
	MccSliceFrame fr;
	if (n > g_cn_wascap) {
		free(g_cn_wasel);
		g_cn_wasel = malloc((size_t)n);
		g_cn_wascap = g_cn_wasel ? n : 0;
	}
	for (i = 0; i < n && i < g_cn_wascap; i++)
		g_cn_wasel[i] = 0;
	census_ptrs(a, n);
	for (i = 0; i < n; i++) {
		int ninv = 0, cls = 0;
		if (ast_kind(a, (AstLocal)i) == AST_If) {
			int op = ast_op(a, (AstLocal)i);
			if (op == 8)
				g_cn_op8n++;
			else if (op == 9)
				g_cn_op9n++;
			else if (op == 6)
				g_cn_op6n++;
		}
		if (ast_kind(a, (AstLocal)i) != AST_BasicBlock ||
				ast_first_child(a, (AstLocal)i) == AST_NONE)
			continue;
		g_cn_blocks++;
		block_inv(a, (AstLocal)i, &ninv, &cls);
		if (ninv) {
			g_cn_invblk++;
			g_cn_invn += ninv;
			if (cls & 1)
				g_cn_inv_ind++;
			else if (cls == 4)
				g_cn_inv_int++;
			else if (cls == 2)
				g_cn_inv_ext++;
			else
				g_cn_inv_mix++;
		}
		if (census_has_deref(a, (AstLocal)i))
			g_cn_dblocks++;
		if (mcc_slice_frame_from_ast(a, (AstLocal)i, &fr)) {
			g_cn_elig++;
			if (fr.nptr)
				g_cn_pblocks++;
			continue;
		}
		if (ninv && i < g_cn_wascap) {
			g_cn_wasel[i] = 1;
			anyinv = 1;
		}
		if (has_op(a, (AstLocal)i, 8))
			g_cn_op8b++;
		if (has_op(a, (AstLocal)i, 9))
			g_cn_op9b++;
		if (has_op(a, (AstLocal)i, 6))
			g_cn_op6b++;
		if (top_op(a, (AstLocal)i, 8))
			g_cn_op8t++;
		if (top_op(a, (AstLocal)i, 9))
			g_cn_op9t++;
		if (top_op(a, (AstLocal)i, 6))
			g_cn_op6t++;
	}
	if (anyinv) {
		census_probe(a, n, 1, &g_cn_sole, &g_cn_soleonly);
		census_probe(a, n, 0, &g_cn_inlblk, NULL);
	}
	census_stores(a, n);
}

/* Two passes over the same dump. A callee is INTERNAL when some body in the
 * dump defines it and a leaf body is only graftable once it is known not to be
 * one of two same-named statics, and neither fact is available until the whole
 * file has been read -- so pass 0 builds the name set and the leaf table, and
 * pass 1 does the work. */
static int arena_pass(FILE *f, int pass, long limit, int quiet) {
	char line[512];
	RawNode *raw = NULL;
	int cap = 0, pend = 0;

	while (pend ? (pend = 0, 1) : fgets(line, sizeof line, f) != NULL) {
		char fn[INV_NAMEMAX];
		long n, root;
		int i;
		if (sscanf(line, "[arena] fn=%127s n=%ld root=%ld", fn, &n, &root) != 3)
			continue;
		if (n <= 0 || n > RAW_MAX)
			continue;
		if (n > cap) {
			cap = (int)n;
			raw = (RawNode *)realloc(raw, (size_t)cap * sizeof *raw);
			if (!raw)
				return 1;
		}
		for (i = 0; i < n; i++) {
			long id, fc, ns;
			if (!fgets(line, sizeof line, f))
				break;
			int nf = sscanf(line,
											"%ld %d %d %d %lld %ld %ld %llu %u %u %llu %llu %d %d",
											&id, &raw[i].kind, &raw[i].op, &raw[i].type_t,
											&raw[i].ival, &fc, &ns, &raw[i].type_ref, &raw[i].bp,
											&raw[i].bs, &raw[i].sym, &raw[i].fbits, &raw[i].size,
											&raw[i].etype);
			if (nf < 7)
				break;
			if (nf < 12) {
				raw[i].type_ref = 0;
				raw[i].bp = raw[i].bs = 0;
				raw[i].sym = raw[i].fbits = 0;
			}
			/* The 13th column is the object's byte extent and the 14th its element
			 * type, neither of which older dumps carry. Both missing read as 0,
			 * i.e. "unknown", and anything that needs to bound or narrow a runtime
			 * index must refuse rather than guess. The extent alone is not enough:
			 * `arr[i]` scales i by the element size at replay, and the Load and
			 * Binary above it are untyped in every real arena, so without the
			 * element type there is neither an element count to bound i against nor
			 * a width to narrow the stored value to. */
			if (nf < 13)
				raw[i].size = 0;
			if (nf < 14)
				raw[i].etype = 0;
			raw[i].first_child = (unsigned)fc;
			raw[i].next_sib = (unsigned)ns;
		}
		if (i != n)
			continue;
		if (pass == 0) {
			defn_add(fn);
			leaf_pass0(fn, raw, (int)n, root);
			while (fgets(line, sizeof line, f)) {
				if (strncmp(line, "[inv] ", 6)) {
					pend = 1;
					break;
				}
			}
			continue;
		}
		inv_reset(n);
		while (fgets(line, sizeof line, f)) {
			long id;
			char cal[INV_NAMEMAX];
			if (sscanf(line, "[inv] %ld %127s", &id, cal) != 2) {
				pend = 1;
				break;
			}
			inv_set(id, cal);
		}
		{
			AstLocal rt;
			AstArena *a = rebuild_arena(raw, (int)n, &rt, root);
			if (!a)
				continue;
			g_arena_bodies++;
			slicerun_obj_reset(n);
			for (i = 0; i < n && i < g_obj_n; i++) {
				g_obj_ext[i] = (int32_t)raw[i].size;
				g_obj_ety[i] = raw[i].etype;
			}
			if (g_census) {
				census_arena(a, (int)n);
			} else {
				if (g_inline)
					mcc_slice_inline_arena(a);
				scan_subtree(a, rt, quiet, limit);
			}
			slicerun_obj_reset(0);
			ast_arena_free(a);
		}
		if (limit && g_arena_slices >= limit)
			break;
	}
	free(raw);
	return 0;
}

static int arena_mode(const char *path, long limit, int quiet) {
	FILE *f = fopen(path, "r");
	int rc;

	if (!f) {
		fprintf(stderr, "slicerun: cannot open %s\n", path);
		return 1;
	}
	mcc_slice_leaf_hook = slicerun_leaf_hook;
	mcc_slice_inl_dump = getenv("MCC_SLICE_INL_DUMP") != NULL;
	if (arena_pass(f, 0, limit, quiet)) {
		fclose(f);
		return 1;
	}
	rewind(f);
	rc = arena_pass(f, 1, limit, quiet);
	fclose(f);
	if (rc)
		return rc;

	if (g_census) {
		printf("census: blocks=%ld eligible=%ld\n", g_cn_blocks, g_cn_elig);
		printf("census: op8 nodes=%ld inelig-blocks-any=%ld inelig-blocks-top=%ld\n",
					 g_cn_op8n, g_cn_op8b, g_cn_op8t);
		printf("census: op9 nodes=%ld inelig-blocks-any=%ld inelig-blocks-top=%ld\n",
					 g_cn_op9n, g_cn_op9b, g_cn_op9t);
		printf("census: op6 nodes=%ld inelig-blocks-any=%ld inelig-blocks-top=%ld\n",
					 g_cn_op6n, g_cn_op6b, g_cn_op6t);
		printf("census: stores local=%ld field=%ld addr=%ld other=%ld\n",
					 g_cn_stlocal, g_cn_stfield, g_cn_staddr, g_cn_stother);
		printf("census: inv-blocks=%ld invokes=%ld all-internal=%ld all-external=%ld "
					 "mixed=%ld any-indirect=%ld\n",
					 g_cn_invblk, g_cn_invn, g_cn_inv_int, g_cn_inv_ext, g_cn_inv_mix,
					 g_cn_inv_ind);
		printf("census: inv-sole-blocker=%ld invoke-only-blocks=%ld "
					 "inline-unblocked=%ld inline-grafts=%ld leaf-callees=%ld\n",
					 g_cn_sole, g_cn_soleonly, g_cn_inlblk, mcc_slice_inl_n,
					 (long)g_leaf_n);
		printf("census: deref loads=%ld lowered=%ld stores=%ld lowered=%ld "
					 "ptrinc=%ld lowered=%ld\n",
					 g_cn_deref, g_cn_derefok, g_cn_pstore, g_cn_pstoreok, g_cn_pinc,
					 g_cn_pincok);
		printf("census: blocks-with-deref=%ld eligible-with-ptr=%ld\n", g_cn_dblocks,
					 g_cn_pblocks);
		return 0;
	}
	if (g_cost_mode) {
		printf("# rows=%ld  win-within-%d-lanes=%ld  lowest-breakeven=%.0f\n",
					 g_cost_rows, COST_LARGE, g_cost_wins, g_cost_best_be);
		if (!g_cost_rows) {
			printf("slicerun: FAIL (the cost table has no rows)\n");
			return 1;
		}
		return 0;
	}
	printf("slicerun: bodies=%ld slices=%ld tuples=%ld gpu-slices=%ld "
				 "dispatches=%ld mismatches=%ld\n",
				 g_arena_bodies, g_arena_slices, g_arena_tuples, g_arena_gpu_slices,
				 mcc_slice_dispatches(), g_arena_mismatch);
	printf("slicerun: frame-accepted=%ld frame-built=%ld frame-compared=%ld "
				 "frame-stmts=%ld frame-mismatches=%ld frame-mem=%ld\n",
				 g_frame_slices, g_frame_built, g_frame_compared, g_frame_stmts,
				 g_frame_mismatch, g_frame_mem);
	printf("slicerun: invoke-seen=%ld invoke-inlined=%ld leaf-callees=%d\n",
				 mcc_slice_inl_seen, mcc_slice_inl_n, g_leaf_n);
	g_arena_mismatch += g_frame_mismatch;
	if (!g_arena_slices) {
		printf("slicerun: FAIL (no real slice became schedulable work)\n");
		return 1;
	}
	if (g_have_device && !mcc_slice_dispatches()) {
		printf("slicerun: FAIL (a device was found and never dispatched)\n");
		return 1;
	}
	if (!g_have_device && g_device_required) {
		printf("slicerun: FAIL (no usable device but a device is required)\n");
		return 1;
	}
	printf("slicerun: %s\n", g_arena_mismatch ? "FAIL" : "OK");
	return g_arena_mismatch ? 1 : 0;
}

/* ------------------------------------------------------------------ main -- */

static void probe_device(void) {
	AstArena *a = ast_arena_new();
	AstLocal root = mk_bin(a, '+', mk_ref(a, -8, VT_INT), mk_lit(a, 1, VT_INT),
												 VT_INT);
	MccSliceWork w;
	MccSliceKernel k;
	int64_t in = 41, out = 0;
	unsigned char def = 0;
	MccGpuStats gs;

	if (mcc_slice_work_from_ast(a, root, &w) && mcc_slice_kernel_build(&w, &k)) {
		mcc_slice_work_bind(&w, &in, 1, &out, &def);
		if (mcc_slice_run_gpu(&w, &k, 0) == MCC_TASK_DONE && def && out == 42)
			g_have_device = 1;
		mcc_slice_kernel_free(&k);
	}
	mcc_gpu_stats(&gs);
	snprintf(g_devname, sizeof g_devname, "%s", gs.name ? gs.name : "?");
	ast_arena_free(a);
}

int main(int argc, char **argv) {
	const char *only = NULL;
	const char *arenas = NULL;
	long limit = 0;
	int quiet = 0, i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--arenas") && i + 1 < argc)
			arenas = argv[++i];
		else if (!strcmp(argv[i], "--limit") && i + 1 < argc)
			limit = atol(argv[++i]);
		else if (!strcmp(argv[i], "--quiet"))
			quiet = 1;
		else if (!strcmp(argv[i], "--require-device"))
			g_device_required = 1;
		else if (!strcmp(argv[i], "--device-or-skip"))
			g_device_or_skip = 1;
		else if (!strcmp(argv[i], "--mutate"))
			g_mutate = 1;
		else if (!strcmp(argv[i], "--census"))
			g_census = 1;
		else if (!strcmp(argv[i], "--no-inline"))
			g_inline = 0;
		else if (!strcmp(argv[i], "--cost"))
			g_cost_mode = 1;
		else if (!strcmp(argv[i], "--cost-synth"))
			g_cost_synth = 1;
		else if (!strcmp(argv[i], "--no-ptr"))
			g_no_ptr = 1;
		else if (!strcmp(argv[i], "--lax"))
			g_lax = 1;
		else
			only = argv[i];
	}

	probe_device();
	if (g_device_or_skip && !g_have_device) {
		fprintf(stderr, "slicerun: no usable device, and this run needs one\n");
		return 77;
	}
	mcc_slice_set_mutate(g_mutate);
	ast_eval_slice_obj_fn = slicerun_obj;
	frame_ptr_arm();
	(void)g_lax;

	if (g_cost_synth)
		return cost_synth();

	if (arenas) {
		if (g_cost_mode) {
			if (!g_have_device) {
				printf("slicerun: no usable device; the H6 table needs one\n");
				return 77;
			}
			printf("# H6 cost table -- device %s\n", g_devname);
			printf("# slice\tnodes\tnlive\tcpu_ns_per_tuple\tgpu_fixed_ns\t"
						 "gpu_ns_per_lane\tbreakeven_ntuple\n");
		} else {
			printf("slicerun: device %s (available=%d)\n", g_devname, g_have_device);
		}
		return arena_mode(arenas, limit, quiet);
	}

	if (!only || !strcmp(only, "task"))
		suite_task();
	if (!only || !strcmp(only, "work"))
		suite_work();
	if (!only || !strcmp(only, "cpu"))
		suite_cpu();
	if (!only || !strcmp(only, "gpu"))
		suite_gpu();
	if (!only || !strcmp(only, "bytes"))
		suite_bytes();
	if (!only || !strcmp(only, "wide64"))
		suite_wide64();
	if (!only || !strcmp(only, "ops"))
		suite_ops();
	if (!only || !strcmp(only, "frame"))
		suite_frame();
	if (!only || !strcmp(only, "mem"))
		suite_mem();
	if (!only || !strcmp(only, "deref"))
		suite_deref();
	if (!only || !strcmp(only, "fmt"))
		suite_fmt();
	if (only && !strcmp(only, "fault"))
		suite_fault();
	if (!only || !strcmp(only, "sched"))
		suite_sched();

	fprintf(stderr, "slicerun: %d checks, %d failures, device=%s available=%d "
									"dispatches=%ld\n",
					g_checks, g_failures, g_devname, g_have_device,
					mcc_slice_dispatches());

	if (!g_have_device && g_device_required) {
		fprintf(stderr, "FAIL slicerun: a device is required and none was usable\n");
		return 1;
	}
	if (only && (!strcmp(only, "gpu") || !strcmp(only, "wide64") ||
							 !strcmp(only, "ops") || !strcmp(only, "fault") ||
			 !strcmp(only, "mem") || !strcmp(only, "deref") ||
			 !strcmp(only, "fmt")) &&
			!g_have_device)
		return 77;
	return g_failures ? 1 : 0;
}
