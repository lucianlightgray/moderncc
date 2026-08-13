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

#undef malloc
#undef realloc
#undef free
#undef strdup

#include "ast_eval_slice.h"

#ifndef AST_EVAL_SLICE_PROVIDED
#error "slicerun needs the real ast_eval_slice; the mccast.c fallback stub returns 1 without writing *out, so the CPU runner would be comparing uninitialised memory against the device and every cell would pass."
#endif

#define MCC_GPU_EMITTER 1
#include "mccgpu.h"

#define MCC_SLICE_GPU 1
#include "mcctask.h"
#include "mccslice.h"
#include "slice_inline.h"

#define MCC_THREAD_ENGINE 1
#include "mccthread.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if MCC_HOST_WIN32
#include <windows.h>
/* MSVC has no POSIX setenv/unsetenv; route through _putenv_s as mccjit_win32.h
   does. overwrite is ignored (mcc only ever sets keys it owns). */
static int slicerun_setenv(const char *name, const char *val, int overwrite) {
	(void)overwrite;
	return _putenv_s(name, val);
}
static int slicerun_unsetenv(const char *name) { return _putenv_s(name, ""); }
#define setenv(n, v, o) slicerun_setenv((n), (v), (o))
#define unsetenv(n) slicerun_unsetenv((n))
#else
#include <dlfcn.h>
#endif

/* slicerun does not link mcchost.c, so it supplies its own dispatch-layer
   dl* shims. Mirror mcchost.c's host_dl* Windows/POSIX split verbatim. */
ST_FUNC void *host_dlopen(const char *name) {
#if MCC_HOST_WIN32
	return (void *)LoadLibraryA(name);
#else
	return dlopen(name, RTLD_GLOBAL | RTLD_LAZY);
#endif
}

ST_FUNC const char *host_dlerror(void) {
#if MCC_HOST_WIN32
	return "error";
#else
	return dlerror();
#endif
}

ST_FUNC void *host_dlsym(void *h, const char *symbol) {
#if MCC_HOST_WIN32
	return (void *)GetProcAddress((HMODULE)h, symbol);
#else
	return dlsym(h, symbol);
#endif
}

ST_FUNC void *host_dlsym_process(const char *symbol) {
#if MCC_HOST_WIN32
	(void)symbol;
	return NULL;
#else
	return dlsym(RTLD_DEFAULT, symbol);
#endif
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
/* Set when suite_f64 finds the device has no shaderFloat64, so the fp64 value
 * differential never ran and --mutate has nothing to perturb. */
static int g_f64_no_diff;
/* Set when a suite is asked for something this backend does not emit at all.
 * The suite then compares nothing, and reporting either pass or fail would be
 * a statement about the backend rather than about the comparison. */
static int g_unsupported;

/* The Metal arm emits per-value kernels (mslgate proves that against the CPU
 * reference) but not frame kernels, and has no region layer or format engine:
 * TODO.md §5 stages M2, M4 and M5. Under MCC_GPU_LANG_MSL the builders above
 * return 0 by construction, so the suites that drive them must say
 * "unsupported" rather than assert that lowering succeeded. */
static int backend_has_frame_kernels(void) {
#if MCC_GPU_LANG_MSL
	return 0;
#else
	return 1;
#endif
}

/* Only the Vulkan arm waits on a fence with a caller-supplied timeout, so only
 * it can be made to strand a dispatch on demand. Metal's dispatch is
 * synchronous. */
static int backend_has_fence_timeout(void) {
#if MCC_GPU_LANG_MSL
	return 0;
#else
	return 1;
#endif
}

static int backend_has_regions(void) {
#if MCC_GPU_LANG_MSL
	return 0;
#else
	return 1;
#endif
}

static void unsupported(const char *what, const char *stage) {
	fprintf(stderr, "SKIP: slicerun: this backend does not emit %s (%s); the "
									"comparison would compare nothing\n",
					what, stage);
	g_unsupported = 1;
}
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

#define SR_GLOB_BASE 0x40000000
#define SR_GLOB_STRIDE 0x1000
#define SR_GLOB_MAX 4096

static unsigned g_glob_id[SR_GLOB_MAX];
static int g_glob_n;

static int slicerun_reloc(AstArena *a, AstLocal n, int32_t *base) {
	uint64_t s = ast_sym(a, n);
	int i;
	if (!s || s > 0xFFFFFFFFu)
		return 0;
	if (g_obj_ext && (long)n < g_obj_n && g_obj_ext[n] > SR_GLOB_STRIDE)
		return 0;
	for (i = 0; i < g_glob_n; i++)
		if (g_glob_id[i] == (unsigned)s)
			break;
	if (i == g_glob_n) {
		if (g_glob_n >= SR_GLOB_MAX)
			return 0;
		g_glob_id[g_glob_n++] = (unsigned)s;
	}
	*base = (int32_t)((uint32_t)SR_GLOB_BASE + (uint32_t)i * SR_GLOB_STRIDE);
	return 1;
}

static int slicerun_glob_of(int32_t off, unsigned *id, int32_t *delta) {
	uint32_t d;
	int i;
	if (off < SR_GLOB_BASE)
		return 0;
	d = (uint32_t)off - (uint32_t)SR_GLOB_BASE;
	i = (int)(d / SR_GLOB_STRIDE);
	if (i >= g_glob_n)
		return 0;
	*id = g_glob_id[i];
	*delta = (int32_t)(d % SR_GLOB_STRIDE);
	return 1;
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
		CHECK(mcc_slice_work_from_ast(a, f, &w) == 1,
					"M6: a double slice IS schedulable work, and carries VT_DOUBLE");
		CHECK(w.wtype == VT_DOUBLE, "the work item keeps the double width type");
	}
	{
		AstLocal f = mk_bin(a, '/', mk_ref(a, -32, VT_DOUBLE),
												mk_ref(a, -40, VT_DOUBLE), VT_DOUBLE);
		CHECK(mcc_slice_work_from_ast(a, f, &w) == 0,
					"double division is not work: OpFDiv is 2.5 ULP by spec");
	}
	{
		AstLocal f = mk_bin(a, '+', mk_ref(a, -32, VT_FLOAT),
												mk_ref(a, -40, VT_FLOAT), VT_FLOAT);
		CHECK(mcc_slice_work_from_ast(a, f, &w) == 0,
					"a float slice is not work: fp32 denormals flush and cannot be pinned");
	}
	{
		AstLocal f = mk_bin(a, '+', mk_ref(a, -32, VT_LDOUBLE),
												mk_ref(a, -40, VT_LDOUBLE), VT_LDOUBLE);
		CHECK(mcc_slice_work_from_ast(a, f, &w) == 0,
					"a long double slice is not work: no device has an 80-bit float");
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

#define VT_DBL VT_DOUBLE
#define VT_FLT VT_FLOAT
#define VT_LDBL VT_LDOUBLE

static const uint64_t F64V[] = {
		0x0000000000000000ull, /* +0.0 */
		0x8000000000000000ull, /* -0.0 */
		0x3FF0000000000000ull, /* 1.0 */
		0xBFF0000000000000ull, /* -1.0 */
		0x4000000000000000ull, /* 2.0 */
		0x3FE0000000000000ull, /* 0.5 */
		0x400921FB54442D18ull, /* pi */
		0xC02E000000000000ull, /* -15.0 */
		0x3FF0000000000001ull, /* 1.0 + 1ulp, the value a 1-bit mutation lands on */
		0x0010000000000000ull, /* DBL_MIN, the smallest normal */
		0x000FFFFFFFFFFFFFull, /* the largest subnormal */
		0x0000000000000001ull, /* the smallest subnormal */
		0x8000000000000001ull, /* and its negative */
		0x7FEFFFFFFFFFFFFFull, /* DBL_MAX -- pairs with itself to overflow to inf */
		0xFFEFFFFFFFFFFFFFull,
		0x7FF0000000000000ull, /* +inf */
		0xFFF0000000000000ull, /* -inf */
		0x7FF8000000000000ull, /* the default quiet NaN */
		0x7FF8000000ABCDEFull, /* a quiet NaN with a distinctive payload */
		0xFFF8000000FEDCBAull, /* a negative quiet NaN, different payload again */
		0x4330000000000000ull, /* 2^52: x+1 stops being representable above here */
		0x3CB0000000000000ull  /* 2^-52 */
};
#define F64V_N ((int)(sizeof F64V / sizeof F64V[0]))

static double f64_of(int64_t b) {
	double d;
	uint64_t u = (uint64_t)b;
	memcpy(&d, &u, sizeof d);
	return d;
}

static int f64_is_subnormal(int64_t b) {
	uint64_t u = (uint64_t)b & 0x7FFFFFFFFFFFFFFFull;
	return u != 0 && u < 0x0010000000000000ull;
}

static int f64_is_zero(int64_t b) {
	return ((uint64_t)b & 0x7FFFFFFFFFFFFFFFull) == 0;
}

static int f64_denorm_flushed(int64_t cpu, int64_t dev) {
	if (!f64_is_zero(dev))
		return 0;
	return ((uint64_t)cpu >> 63) == ((uint64_t)dev >> 63);
}

static int f64_is_nan(int64_t b) {
	uint64_t u = (uint64_t)b;
	return (u & 0x7FF0000000000000ull) == 0x7FF0000000000000ull &&
				 (u & 0x000FFFFFFFFFFFFFull) != 0;
}

static int f64_nan_select(int64_t a, int64_t b) {
	return f64_is_nan(a) && f64_is_nan(b) && a != b;
}

static long g_f64_denorm_exact, g_f64_denorm_flush, g_f64_certified;
static long g_f64_nansel_first, g_f64_nansel_second;
static int g_f64_denorm_seen;

static void f64_binop(const char *what, int op, int cmpresult) {
	AstArena *a = ast_arena_new();
	AstLocal root =
			mk_bin(a, op, mk_ref(a, -8, VT_DBL), mk_ref(a, -16, VT_DBL), VT_DBL);
	MccSliceWork w;
	MccSliceKernel k;
	int64_t *in, *cout, *gout;
	unsigned char *cdef, *gdef;
	int n = F64V_N * F64V_N, i, j, bad = 0, cmp = 0, dn = 0, odd = 0;

	if (cmpresult)
		ast_set_type(a, root, VT_INT, 0);
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
	for (i = 0; i < F64V_N; i++)
		for (j = 0; j < F64V_N; j++) {
			in[((long)i * F64V_N + j) * 2] = (int64_t)F64V[i];
			in[((long)i * F64V_N + j) * 2 + 1] = (int64_t)F64V[j];
		}

	g_checks++;
	if (!mcc_slice_work_from_ast(a, root, &w)) {
		fprintf(stderr, "FAIL %s: %s is not schedulable work\n", __func__, what);
		g_failures++;
		goto out;
	}
	mcc_slice_work_bind(&w, in, n, cout, cdef);
	g_checks++;
	if (mcc_slice_run_cpu(&w, 0) != MCC_TASK_DONE) {
		fprintf(stderr, "FAIL %s: %s CPU run did not complete\n", __func__, what);
		g_failures++;
		goto out;
	}
	g_checks++;
	if (!mcc_slice_kernel_build(&w, &k)) {
		fprintf(stderr, "FAIL %s: %s did not lower to a device kernel\n", __func__,
						what);
		g_failures++;
		goto out;
	}
	mcc_slice_work_bind(&w, in, n, gout, gdef);
	g_checks++;
	if (mcc_slice_run_gpu(&w, &k, 0) != MCC_TASK_DONE) {
		fprintf(stderr, "FAIL %s: %s device run did not complete\n", __func__,
						what);
		g_failures++;
		mcc_slice_kernel_free(&k);
		goto out;
	}
	mcc_slice_kernel_free(&k);

	for (i = 0; i < n; i++) {
		int64_t la = in[(long)i * 2], rb = in[(long)i * 2 + 1];
		int touches;
		if (gdef[i] != cdef[i]) {
			if (bad < 4)
				fprintf(stderr, "FAIL %s: %s definedness a=%016llx b=%016llx c=%d g=%d\n",
								__func__, what, (unsigned long long)la, (unsigned long long)rb,
								cdef[i], gdef[i]);
			bad++;
			continue;
		}
		if (!cdef[i])
			continue;
		touches = !cmpresult && (f64_is_subnormal(la) || f64_is_subnormal(rb) ||
														 f64_is_subnormal(cout[i]));
		if (!cmpresult && f64_nan_select(la, rb)) {
			g_checks++;
			if (gout[i] == la)
				g_f64_nansel_first++;
			else if (gout[i] == rb)
				g_f64_nansel_second++;
			else {
				if (bad < 4)
					fprintf(stderr,
									"FAIL %s: %s a=%016llx b=%016llx gpu=%016llx is neither "
									"operand's NaN payload\n",
									__func__, what, (unsigned long long)la,
									(unsigned long long)rb, (unsigned long long)gout[i]);
				bad++;
			}
			continue;
		}
		if (gout[i] == cout[i]) {
			if (touches) {
				dn++;
				g_f64_denorm_exact++;
			} else {
				cmp++;
				g_f64_certified++;
			}
			continue;
		}
		if (touches && f64_denorm_flushed(cout[i], gout[i])) {
			dn++;
			g_f64_denorm_flush++;
			continue;
		}
		if (touches)
			odd++;
		if (bad < 4)
			fprintf(stderr,
							"FAIL %s: %s a=%016llx b=%016llx cpu=%016llx gpu=%016llx%s\n",
							__func__, what, (unsigned long long)la, (unsigned long long)rb,
							(unsigned long long)cout[i], (unsigned long long)gout[i],
							touches ? " (denormal-touching, and neither model fits)" : "");
		bad++;
	}
	g_checks++;
	if (bad)
		g_failures++;
	g_checks++;
	if (!cmp) {
		fprintf(stderr, "FAIL %s: %s compared no certified tuple\n", __func__,
						what);
		g_failures++;
	}
	if (dn)
		g_f64_denorm_seen = 1;
	(void)odd;
out:
	free(in); free(cout); free(gout); free(cdef); free(gdef);
	ast_arena_free(a);
}

static void f64_identity(void) {
	AstArena *a = ast_arena_new();
	AstLocal id = mk_ref(a, -8, VT_DBL);
	MccSliceWork w;
	MccSliceKernel k;
	int64_t in[F64V_N], out[F64V_N];
	unsigned char def[F64V_N];
	int i, bad = 0;
	for (i = 0; i < F64V_N; i++)
		in[i] = (int64_t)F64V[i];
	CHECK(mcc_slice_work_from_ast(a, id, &w) == 1, "a bare double ref is work");
	CHECK(w.wtype == VT_DBL, "the work item carries VT_DOUBLE as its width type");
	CHECK(mcc_slice_kernel_build(&w, &k) == 1, "a double ref lowers to a kernel");
	mcc_slice_work_bind(&w, in, F64V_N, out, def);
	CHECK(mcc_slice_run_gpu(&w, &k, 0) == MCC_TASK_DONE, "the double batch runs");
	for (i = 0; i < F64V_N; i++) {
		if (def[i] != 1 || out[i] != in[i]) {
			if (bad < 4)
				fprintf(stderr, "FAIL %s: payload %016llx returned %016llx def=%d\n",
								__func__, (unsigned long long)in[i],
								(unsigned long long)out[i], def[i]);
			bad++;
		}
	}
	g_checks++;
	if (bad)
		g_failures++;
	mcc_slice_kernel_free(&k);
	ast_arena_free(a);
}

static void f64_exclusions(void) {
	static const struct {
		const char *why;
		int op;
		int type;
	} NO[] = {
			{"double division: OpFDiv is 2.5 ULP by spec", '/', VT_DBL},
			{"double remainder is not an arithmetic op on floats", '%', VT_DBL},
			{"float add: fp32 denormals measurably flush on the device", '+', VT_FLT},
			{"float multiply", '*', VT_FLT},
			{"float compare", TOK_LT, VT_FLT},
			{"long double: no device has an 80-bit float", '+', VT_LDBL},
			{"long double multiply", '*', VT_LDBL}};
	int i;
	for (i = 0; i < (int)(sizeof NO / sizeof NO[0]); i++) {
		AstArena *a = ast_arena_new();
		AstLocal root = mk_bin(a, NO[i].op, mk_ref(a, -8, NO[i].type),
													 mk_ref(a, -16, NO[i].type), NO[i].type);
		MccSliceWork w;
		g_checks++;
		if (mcc_slice_work_from_ast(a, root, &w)) {
			fprintf(stderr, "FAIL %s: excluded shape became work -- %s\n", __func__,
							NO[i].why);
			g_failures++;
		}
		ast_arena_free(a);
	}
	{
		static const int MIXOP[] = {'+', '-', '*', TOK_LT, TOK_EQ};
		int i;
		for (i = 0; i < (int)(sizeof MIXOP / sizeof MIXOP[0]); i++) {
			int order;
			for (order = 0; order < 2; order++) {
				AstArena *a = ast_arena_new();
				AstLocal l = order ? mk_ref(a, -8, VT_INT) : mk_ref(a, -8, VT_DBL);
				AstLocal r = order ? mk_ref(a, -16, VT_DBL) : mk_ref(a, -16, VT_INT);
				AstLocal root = mk_bin(a, MIXOP[i], l, r, 0);
				MccSliceWork w;
				g_checks++;
				if (mcc_slice_work_from_ast(a, root, &w)) {
					fprintf(stderr,
									"FAIL %s: mixed int/double operands became work (op=%#x, "
									"double %s)\n",
									__func__, MIXOP[i], order ? "second" : "first");
					g_failures++;
				}
				ast_arena_free(a);
			}
		}
	}
	{
		int order;
		for (order = 0; order < 2; order++) {
			AstArena *a = ast_arena_new();
			AstLocal iff = ast_node(a, AST_If);
			AstLocal dbl = mk_bin(a, '-', mk_ref(a, -8, VT_DBL),
														mk_ref(a, -16, VT_DBL), 0);
			MccSliceWork w;
			ast_set_op(a, iff, 5);
			ast_add_child(a, iff, mk_bin(a, TOK_GT, mk_ref(a, -8, VT_DBL),
																	 mk_ref(a, -16, VT_DBL), 0));
			ast_add_child(a, iff, order ? mk_lit(a, 0, VT_INT) : dbl);
			ast_add_child(a, iff, order ? dbl : mk_lit(a, 0, VT_INT));
			g_checks++;
			if (mcc_slice_work_from_ast(a, iff, &w)) {
				fprintf(stderr,
								"FAIL %s: a ternary with one double arm and one integer arm "
								"became work (double %s). Its wtype is uac(0, VT_INT) = "
								"VT_INT, so the device result is narrowed to 32 bits and "
								"every double whose low word is zero reads back as 0\n",
								__func__, order ? "second" : "first");
				g_failures++;
			}
			ast_arena_free(a);
		}
	}
	{
		AstArena *a = ast_arena_new();
		AstLocal iff = ast_node(a, AST_If);
		MccSliceWork w;
		ast_set_op(a, iff, 5);
		ast_add_child(a, iff, mk_bin(a, TOK_GT, mk_ref(a, -8, VT_DBL),
																 mk_ref(a, -16, VT_DBL), 0));
		ast_add_child(a, iff, mk_ref(a, -8, VT_DBL));
		ast_add_child(a, iff, mk_ref(a, -16, VT_DBL));
		g_checks++;
		if (!mcc_slice_work_from_ast(a, iff, &w)) {
			fprintf(stderr, "FAIL %s: a ternary whose two arms are both double no "
											"longer becomes work; the mixed-arm refusal above has "
											"been widened past the shape it exists for\n",
							__func__);
			g_failures++;
		}
		ast_arena_free(a);
	}
	{
		AstArena *a = ast_arena_new();
		AstLocal cvt = ast_node(a, AST_Convert);
		MccSliceWork w;
		ast_set_type(a, cvt, VT_DBL, 0);
		ast_add_child(a, cvt, mk_ref(a, -8, VT_INT));
		g_checks++;
		if (mcc_slice_work_from_ast(a, cvt, &w))
			g_failures++, fprintf(stderr, "FAIL %s: int->double convert became work\n",
														__func__);
		ast_arena_free(a);
	}
	{
		AstArena *a = ast_arena_new();
		AstLocal cvt = ast_node(a, AST_Convert);
		MccSliceWork w;
		ast_set_type(a, cvt, VT_INT, 0);
		ast_add_child(a, cvt, mk_ref(a, -8, VT_DBL));
		g_checks++;
		if (mcc_slice_work_from_ast(a, cvt, &w))
			g_failures++, fprintf(stderr, "FAIL %s: double->int convert became work\n",
														__func__);
		ast_arena_free(a);
	}
}

static void suite_f64(void) {
	if (!g_have_device) {
		if (g_device_required) {
			fprintf(stderr,
							"FAIL slicerun: no usable device but a device is required\n");
			g_failures++;
		}
		return;
	}
#if defined(__FLT_EVAL_METHOD__)
	g_checks++;
	if (__FLT_EVAL_METHOD__ != 0) {
		fprintf(stderr,
						"FAIL suite_f64: FLT_EVAL_METHOD=%d, so the host reference is not "
						"plain double and no bit-exact claim is possible\n",
						(int)__FLT_EVAL_METHOD__);
		g_failures++;
		return;
	}
#endif
	g_checks++;
	if (!mcc_gpu_f64()) {
		fprintf(stderr,
						"slicerun: device %s lacks shaderFloat64; fp64 arithmetic excluded\n",
						g_devname);
		/* The exclusion checks below are real and still run. The fp64 *value*
		 * differential is not reached, though, so there is no fp64 kernel for
		 * --mutate to perturb -- and "the mutant survived" would then be a fact
		 * about the device, not about the comparison. MoltenVK on Apple silicon
		 * reports shaderFloat64 = 0, which is how this first showed up. */
		g_f64_no_diff = 1;
		f64_exclusions();
		{
			AstArena *a = ast_arena_new();
			AstLocal root = mk_bin(a, '+', mk_ref(a, -8, VT_DBL),
														 mk_ref(a, -16, VT_DBL), VT_DBL);
			MccSliceWork w;
			MccSliceKernel k;
			g_checks++;
			if (mcc_slice_work_from_ast(a, root, &w) &&
					mcc_slice_kernel_build(&w, &k)) {
				fprintf(stderr, "FAIL suite_f64: built an fp64 kernel for a device "
												"without shaderFloat64\n");
				g_failures++;
				mcc_slice_kernel_free(&k);
			}
			ast_arena_free(a);
		}
		return;
	}

	f64_identity();
	f64_exclusions();
	f64_binop("f64-add", '+', 0);
	f64_binop("f64-sub", '-', 0);
	f64_binop("f64-mul", '*', 0);
	f64_binop("f64-eq", TOK_EQ, 1);
	f64_binop("f64-ne", TOK_NE, 1);
	f64_binop("f64-lt", TOK_LT, 1);
	f64_binop("f64-le", TOK_LE, 1);
	f64_binop("f64-gt", TOK_GT, 1);
	f64_binop("f64-ge", TOK_GE, 1);

	{
		AstArena *a = ast_arena_new();
		AstLocal neg = ast_node(a, AST_Unary);
		MccSliceWork w;
		MccSliceKernel k;
		int64_t in[F64V_N], cout[F64V_N], gout[F64V_N];
		unsigned char cdef[F64V_N], gdef[F64V_N];
		int i, bad = 0;
		ast_set_op(a, neg, '-');
		ast_set_type(a, neg, VT_DBL, 0);
		ast_add_child(a, neg, mk_ref(a, -8, VT_DBL));
		for (i = 0; i < F64V_N; i++)
			in[i] = (int64_t)F64V[i];
		CHECK(mcc_slice_work_from_ast(a, neg, &w) == 1, "double negate is work");
		mcc_slice_work_bind(&w, in, F64V_N, cout, cdef);
		CHECK(mcc_slice_run_cpu(&w, 0) == MCC_TASK_DONE, "negate runs on the CPU");
		CHECK(mcc_slice_kernel_build(&w, &k) == 1, "double negate lowers");
		mcc_slice_work_bind(&w, in, F64V_N, gout, gdef);
		CHECK(mcc_slice_run_gpu(&w, &k, 0) == MCC_TASK_DONE, "negate runs on device");
		for (i = 0; i < F64V_N; i++)
			if (gdef[i] != cdef[i] || (cdef[i] && gout[i] != cout[i]))
				bad++;
		g_checks++;
		if (bad) {
			fprintf(stderr, "FAIL suite_f64: %d of %d negations diverged\n", bad,
							F64V_N);
			g_failures++;
		}
		CHECK(cout[0] == (int64_t)0x8000000000000000ull,
					"negating +0.0 gives -0.0, not +0.0");
		CHECK(cout[1] == 0, "negating -0.0 gives +0.0");
		mcc_slice_kernel_free(&k);
		ast_arena_free(a);
	}

	g_checks++;
	if (!g_f64_denorm_seen) {
		fprintf(stderr, "FAIL suite_f64: no denormal-touching tuple was compared, "
										"so the denormal question was never asked\n");
		g_failures++;
	}
	g_checks++;
	if (g_f64_denorm_exact && g_f64_denorm_flush) {
		fprintf(stderr,
						"FAIL suite_f64: the device preserved %ld denormal results and "
						"flushed %ld -- it matches neither model consistently\n",
						g_f64_denorm_exact, g_f64_denorm_flush);
		g_failures++;
	}
	g_checks++;
	if (!g_f64_nansel_first && !g_f64_nansel_second) {
		fprintf(stderr, "FAIL suite_f64: no two-NaN tuple was compared, so the "
										"payload tie-break was never asked\n");
		g_failures++;
	}
	g_checks++;
	if (g_f64_nansel_first && g_f64_nansel_second) {
		fprintf(stderr,
						"FAIL suite_f64: the device took the first operand's NaN payload "
						"%ld times and the second %ld times -- it follows neither rule\n",
						g_f64_nansel_first, g_f64_nansel_second);
		g_failures++;
	}
	fprintf(stderr,
					"slicerun: fp64 device=%s certified=%ld tuples bit-exact, "
					"denormal-touching=%ld (%s), two-NaN=%ld (device takes the %s "
					"operand's payload; host takes the first)\n",
					g_devname, g_f64_certified,
					g_f64_denorm_exact + g_f64_denorm_flush,
					g_f64_denorm_flush ? "device FLUSHES fp64 denormals, excluded from "
															 "the certified set"
														 : "device PRESERVES fp64 denormals, and they are "
															 "compared bit-exactly",
					g_f64_nansel_first + g_f64_nansel_second,
					g_f64_nansel_second ? "second" : "first");
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

static AstLocal mk_member(AstArena *a, int32_t base, int32_t madd, int type) {
	AstLocal m = ast_node(a, AST_Unary);
	ast_set_op(a, m, AST_EVAL_OP_MEMBER);
	ast_set_type(a, m, type, 0);
	ast_set_ival(a, m, (uint64_t)(int64_t)madd);
	ast_add_child(a, m, mk_ref(a, base, VT_INT));
	return m;
}

static AstLocal mk_member_idx(AstArena *a, int32_t base, int32_t madd,
															int32_t nbytes, int etype, int32_t ioff) {
	AstLocal m = mk_member(a, base, madd, VT_PTR | VT_ARRAY);
	AstLocal ld = ast_node(a, AST_Load);
	g_obj_ext[m] = nbytes;
	g_obj_ety[m] = etype;
	ast_add_child(a, ld, mk_bin(a, '+', m, mk_ref(a, ioff, VT_INT), 0));
	ast_set_type(a, ld, 0, 0);
	return ld;
}

static AstLocal mk_gref(AstArena *a, uint64_t sym, int type) {
	AstLocal n = ast_node(a, AST_Ref);
	ast_set_op(a, n, VT_CONST | VT_LVAL | VT_SYM);
	ast_set_type(a, n, type, 0);
	ast_set_ival(a, n, 0);
	ast_set_sym(a, n, sym);
	return n;
}

static AstLocal mk_gmember(AstArena *a, uint64_t sym, int32_t madd, int type) {
	AstLocal m = ast_node(a, AST_Unary);
	ast_set_op(a, m, AST_EVAL_OP_MEMBER);
	ast_set_type(a, m, type, 0);
	ast_set_ival(a, m, (uint64_t)(int64_t)madd);
	ast_add_child(a, m, mk_gref(a, sym, VT_STRUCT));
	return m;
}

static AstLocal mk_gmember_idx(AstArena *a, uint64_t sym, int32_t madd,
															 int32_t nbytes, int etype, int32_t ioff) {
	AstLocal m = mk_gmember(a, sym, madd, VT_PTR | VT_ARRAY);
	AstLocal ld = ast_node(a, AST_Load);
	g_obj_ext[m] = nbytes;
	g_obj_ety[m] = etype;
	ast_add_child(a, ld, mk_bin(a, '+', m, mk_ref(a, ioff, VT_INT), 0));
	ast_set_type(a, ld, 0, 0);
	return ld;
}

static void suite_frame(void) {
	AstArena *a;
	MccSliceFrame fr;
	int64_t f[MCC_SLICE_MAXSLOT];
	int i;

	if (!backend_has_frame_kernels()) {
		unsupported("frame kernels", "TODO.md §5 stage M2");
		return;
	}

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

	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		ast_add_child(a, bb,
									mk_store(a, -8, mk_ref(a, -32, VT_INT | VT_ARRAY), VT_INT));
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 0,
					"an array-typed local Ref in a value position is the array's "
					"ADDRESS, not a frame slot, and the frame is refused rather than "
					"reading slot -32 as if it held the value");
	}
	ast_arena_free(a);

	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		ast_add_child(a, bb,
									mk_store(a, -8,
													 mk_bin(a, '+', mk_ref(a, -16, VT_INT),
																	mk_lit(a, 1, VT_INT), VT_INT),
													 VT_INT));
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1,
					"and the refusal is on VT_ARRAY alone -- a scalar local Ref with no "
					"VT_LVAL is still a value, which is the convention the harnesses and "
					"the compiler both write");
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

	/* An array *field* indexed at run time. `s.arr[i]` is
	 * Load(Binary('+')(Unary(MEMBER), i)) and its base resolves to the struct's
	 * frame offset plus the field's, so it is the same slot run a plain local
	 * array takes and the device resolves it with the same arithmetic. The point
	 * of comparing it here rather than trusting the predicate is that a member
	 * base is the one shape where the base offset is *computed* rather than
	 * read, and an off-by-one there would still agree with itself. */
	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal dst, ld, st;
		int64_t cf[8 * MCC_SLICE_MAXSLOT], gf[8 * MCC_SLICE_MAXSLOT];
		int t, bad = 0;
		slicerun_obj_reset(64);
		dst = mk_member_idx(a, -64, 8, 16, VT_INT, -8);
		ld = mk_member_idx(a, -64, 8, 16, VT_INT, -8);
		st = ast_node(a, AST_Store);
		ast_add_child(a, st, dst);
		ast_add_child(a, st, mk_bin(a, '+', ld, mk_lit(a, 1, VT_INT), VT_INT));
		ast_add_child(a, bb, st);
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1,
					"an indexed array field is frame work");
		CHECK(fr.nslot == 5, "four element slots and the index");
		if (fr.nslot == 5) {
			CHECK(fr.slot[0] == -56, "the object starts at the struct offset plus the "
															 "field offset, not at the struct");
			CHECK(fr.slot[3] == -44 && fr.slot[4] == -8,
						"the element run is consecutive and the index follows it");
		}
		for (t = 0; t < 8 * MCC_SLICE_MAXSLOT; t++)
			cf[t] = gf[t] = 0;
		for (t = 0; t < 8; t++)
			for (i = 0; i < fr.nslot; i++) {
				int64_t v = fr.slot[i] == -8 ? (t & 3) : (t * 7 + i);
				cf[t * fr.nslot + i] = gf[t * fr.nslot + i] = v;
			}
		for (t = 0; t < 8; t++)
			CHECK(mcc_slice_frame_exec_cpu(&fr, cf + (long)t * fr.nslot) == 1,
						"the CPU reference runs each indexed-field frame");
		if (fr.nslot == 5) {
			CHECK(cf[0 * 5 + 0] == 1, "frame 0 indexes element 0 and increments it");
			CHECK(cf[1 * 5 + 1] == 9, "frame 1 indexes element 1 and increments it");
		}
		if (g_have_device && fr.nslot == 5) {
			MccSliceKernel k;
			CHECK(mcc_slice_frame_kernel_build(&fr, &k) == 1,
						"the indexed-field run lowers to a device kernel");
			CHECK(mcc_slice_run_frame_gpu(&fr, &k, gf, 8, NULL, NULL) == MCC_TASK_DONE,
						"the device runs eight indexed-field frames");
			for (t = 0; t < 8 * fr.nslot; t++)
				if (cf[t] != gf[t])
					bad++;
			CHECK(bad == 0, "every slot of every indexed-field frame matches");
			mcc_slice_kernel_free(&k);
		}
		slicerun_obj_reset(0);
	}
	ast_arena_free(a);

	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal dst, ld, st;
		int64_t cf[8 * MCC_SLICE_MAXSLOT], gf[8 * MCC_SLICE_MAXSLOT];
		unsigned gid = 0;
		int32_t gdel = -1;
		int t, bad = 0;
		slicerun_obj_reset(64);
		dst = mk_gmember_idx(a, 0x51ce01u, 8, 16, VT_INT, -8);
		ld = mk_gmember_idx(a, 0x51ce01u, 8, 16, VT_INT, -8);
		st = ast_node(a, AST_Store);
		ast_add_child(a, st, dst);
		ast_add_child(a, st, mk_bin(a, '+', ld, mk_lit(a, 1, VT_INT), VT_INT));
		ast_add_child(a, bb, st);
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1,
					"an indexed array field of a global struct is frame work");
		CHECK(fr.nslot == 5, "four element slots and the index");
		if (fr.nslot == 5) {
			CHECK(slicerun_glob_of(fr.slot[0], &gid, &gdel) == 1,
						"the object's first slot is a relocated global key");
			CHECK(gid == 0x51ce01u && gdel == 8,
						"the key names the base symbol at the field's byte offset");
			CHECK(fr.slot[3] == fr.slot[0] + 12 && fr.slot[4] == -8,
						"the element run is consecutive and the index follows it");
		}
		for (t = 0; t < 8 * MCC_SLICE_MAXSLOT; t++)
			cf[t] = gf[t] = 0;
		for (t = 0; t < 8; t++)
			for (i = 0; i < fr.nslot; i++) {
				int64_t v = fr.slot[i] == -8 ? (t & 3) : (t * 7 + i);
				cf[t * fr.nslot + i] = gf[t * fr.nslot + i] = v;
			}
		for (t = 0; t < 8; t++)
			CHECK(mcc_slice_frame_exec_cpu(&fr, cf + (long)t * fr.nslot) == 1,
						"the CPU reference runs each global indexed-field frame");
		if (fr.nslot == 5) {
			CHECK(cf[0 * 5 + 0] == 1, "frame 0 indexes element 0 and increments it");
			CHECK(cf[1 * 5 + 1] == 9, "frame 1 indexes element 1 and increments it");
		}
		if (g_have_device && fr.nslot == 5) {
			MccSliceKernel k;
			CHECK(mcc_slice_frame_kernel_build(&fr, &k) == 1,
						"the global indexed-field run lowers to a device kernel");
			CHECK(mcc_slice_run_frame_gpu(&fr, &k, gf, 8, NULL, NULL) == MCC_TASK_DONE,
						"the device runs eight global indexed-field frames");
			for (t = 0; t < 8 * fr.nslot; t++)
				if (cf[t] != gf[t])
					bad++;
			CHECK(bad == 0,
						"every slot of every global indexed-field frame matches");
			mcc_slice_kernel_free(&k);
		}
		slicerun_obj_reset(0);
	}
	ast_arena_free(a);

	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		unsigned gid = 0;
		int32_t gdel = -1;
		int64_t f2[MCC_SLICE_MAXSLOT];
		ast_add_child(a, bb,
									mk_store(a, -8,
													 mk_bin(a, '+',
																	mk_gmember(a, 0x51ce02u, 4, VT_INT),
																	mk_lit(a, 100, VT_INT), VT_INT),
													 VT_INT));
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1,
					"a scalar field of a global struct is frame work");
		CHECK(fr.nslot == 2, "the destination local and the global field");
		if (fr.nslot == 2) {
			CHECK(slicerun_glob_of(fr.slot[1], &gid, &gdel) == 1 &&
								gid == 0x51ce02u && gdel == 4,
						"the field key is the base symbol at the member offset");
			f2[0] = 0;
			f2[1] = 11;
			CHECK(mcc_slice_frame_exec_cpu(&fr, f2) == 1,
						"the CPU reference runs the global-field frame");
			CHECK(f2[0] == 111, "11 + 100 lands in the destination slot");
		}
	}
	ast_arena_free(a);

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

	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal iff = ast_node(a, AST_If);
		AstLocal thn = ast_node(a, AST_BasicBlock);
		AstLocal er = ast_node(a, AST_Return);
		AstLocal tr = ast_node(a, AST_Return);
		MccSliceKernel k;
		int64_t cf[6 * MCC_SLICE_MAXSLOT], gf[6 * MCC_SLICE_MAXSLOT];
		int64_t crv[6], grv[6];
		int cdf[6];
		unsigned char gdf[6];
		int t, sy = -1;
		ast_set_op(a, iff, 0);
		ast_add_child(a, iff,
									mk_bin(a, TOK_LT, mk_ref(a, -8, VT_INT),
												 mk_lit(a, 0, VT_INT), VT_INT));
		ast_add_child(a, thn, mk_store(a, -16, mk_lit(a, 111, VT_INT), VT_INT));
		ast_add_child(a, er,
									mk_bin(a, '+', mk_ref(a, -8, VT_INT),
												 mk_lit(a, 1, VT_INT), VT_INT));
		ast_add_child(a, thn, er);
		ast_add_child(a, iff, thn);
		ast_add_child(a, bb, iff);
		ast_add_child(a, bb, mk_store(a, -16, mk_lit(a, 222, VT_INT), VT_INT));
		ast_add_child(a, tr,
									mk_bin(a, '*', mk_ref(a, -8, VT_INT),
												 mk_lit(a, 2, VT_INT), VT_INT));
		ast_add_child(a, bb, tr);
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1,
					"an early return inside an if arm is frame work");
		CHECK(fr.neret == 1, "the early exit is recorded");
		CHECK(fr.ret != AST_NONE, "and the trailing Return still is too");
		for (i = 0; i < fr.nslot; i++)
			if (fr.slot[i] == -16)
				sy = i;
		CHECK(sy >= 0, "the stored-to slot is mapped");
		for (t = 0; t < 6; t++)
			for (i = 0; i < fr.nslot; i++)
				cf[t * fr.nslot + i] = gf[t * fr.nslot + i] =
						fr.slot[i] == -8 ? (int64_t)(t - 3) : 0;
		for (t = 0; t < 6; t++)
			CHECK(mcc_slice_frame_exec_cpu2(&fr, cf + (long)t * fr.nslot, &crv[t],
																			&cdf[t]) == 1,
						"the CPU reference runs the early-exit frame");
		for (t = 0; t < 6; t++) {
			int64_t x = (int64_t)(t - 3);
			CHECK(crv[t] == (x < 0 ? x + 1 : x * 2),
						"the value returned is the one the taken exit computed");
			CHECK(cf[t * fr.nslot + sy] == (x < 0 ? 111 : 222),
						"and the statements after the exit did not run on the taken path");
		}
		if (g_have_device && mcc_slice_frame_kernel_build(&fr, &k)) {
			int bad = 0;
			CHECK(mcc_slice_run_frame_gpu(&fr, &k, gf, 6, grv, gdf) == MCC_TASK_DONE,
						"the device runs the early-exit frame");
			for (t = 0; t < 6; t++) {
				CHECK(grv[t] == crv[t], "device and CPU agree on the returned value");
				CHECK((int)gdf[t] == cdf[t], "and on its definedness");
			}
			for (t = 0; t < 6 * fr.nslot; t++)
				if (cf[t] != gf[t])
					bad++;
			CHECK(bad == 0, "and on every frame slot across the early exit");
			mcc_slice_kernel_free(&k);
		}
	}
	ast_arena_free(a);

	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal iff = ast_node(a, AST_If);
		AstLocal thn = ast_node(a, AST_BasicBlock);
		AstLocal er = ast_node(a, AST_Return);
		MccSliceKernel k;
		int64_t cf[6 * MCC_SLICE_MAXSLOT], gf[6 * MCC_SLICE_MAXSLOT];
		int64_t crv[6], grv[6];
		int cdf[6];
		unsigned char gdf[6];
		int t, sy = -1;
		ast_set_op(a, iff, 0);
		ast_add_child(a, iff,
									mk_bin(a, TOK_LT, mk_ref(a, -8, VT_INT),
												 mk_lit(a, 0, VT_INT), VT_INT));
		ast_add_child(a, er, mk_lit(a, 7, VT_INT));
		ast_add_child(a, thn, er);
		ast_add_child(a, iff, thn);
		ast_add_child(a, bb, iff);
		ast_add_child(a, bb, mk_store(a, -16, mk_lit(a, 5, VT_INT), VT_INT));
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1,
					"an early return with nothing behind it is frame work");
		CHECK(fr.ret == AST_NONE && fr.neret == 1,
					"the run has an exit value but no terminating Return");
		for (i = 0; i < fr.nslot; i++)
			if (fr.slot[i] == -16)
				sy = i;
		CHECK(sy >= 0, "the stored-to slot is mapped");
		for (t = 0; t < 6; t++)
			for (i = 0; i < fr.nslot; i++)
				cf[t * fr.nslot + i] = gf[t * fr.nslot + i] =
						fr.slot[i] == -8 ? (int64_t)(t - 3) : 0;
		for (t = 0; t < 6; t++)
			CHECK(mcc_slice_frame_exec_cpu2(&fr, cf + (long)t * fr.nslot, &crv[t],
																			&cdf[t]) == 1,
						"the CPU reference runs the exit-only frame");
		for (t = 0; t < 6; t++) {
			int64_t x = (int64_t)(t - 3);
			CHECK(crv[t] == (x < 0 ? 7 : 0), "7 on the exit, 0 on the fallthrough");
			CHECK(cf[t * fr.nslot + sy] == (x < 0 ? 0 : 5),
						"and the store behind the exit runs only on the fallthrough");
		}
		if (g_have_device && mcc_slice_frame_kernel_build(&fr, &k)) {
			int bad = 0;
			CHECK(mcc_slice_run_frame_gpu(&fr, &k, gf, 6, grv, gdf) == MCC_TASK_DONE,
						"the device runs the exit-only frame");
			for (t = 0; t < 6; t++) {
				CHECK(grv[t] == crv[t], "device and CPU agree on the returned value");
				CHECK((int)gdf[t] == cdf[t], "and on its definedness");
			}
			for (t = 0; t < 6 * fr.nslot; t++)
				if (cf[t] != gf[t])
					bad++;
			CHECK(bad == 0, "and on every frame slot");
			mcc_slice_kernel_free(&k);
		}
	}
	ast_arena_free(a);

	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal iff = ast_node(a, AST_If);
		AstLocal thn = ast_node(a, AST_BasicBlock);
		AstLocal er = ast_node(a, AST_Return);
		ast_set_op(a, iff, 0);
		ast_add_child(a, iff,
									mk_bin(a, TOK_LT, mk_ref(a, -8, VT_INT),
												 mk_lit(a, 0, VT_INT), VT_INT));
		ast_add_child(a, er, mk_lit(a, 7, VT_INT));
		ast_add_child(a, thn, er);
		ast_add_child(a, thn, mk_store(a, -16, mk_lit(a, 1, VT_INT), VT_INT));
		ast_add_child(a, iff, thn);
		ast_add_child(a, bb, iff);
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 0,
					"a return with a statement behind it in its own arm is refused");
	}
	ast_arena_free(a);

	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal lp = ast_node(a, AST_If);
		AstLocal body = ast_node(a, AST_BasicBlock);
		AstLocal er = ast_node(a, AST_Return);
		ast_set_op(a, lp, 2);
		ast_add_child(a, lp,
									mk_bin(a, TOK_LT, mk_ref(a, -8, VT_INT),
												 mk_ref(a, -24, VT_INT), VT_INT));
		ast_add_child(a, er, mk_lit(a, 7, VT_INT));
		ast_add_child(a, body, er);
		ast_add_child(a, lp, body);
		ast_add_child(a, bb, lp);
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 0,
					"a return inside a loop body is refused");
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
#if MCC_GPU_LANG_MSL
	/* Metal arm: no region layer, so no byte kernel. TODO.md §5 stage M4.
	 * Returning 0 is the builder's existing "could not build" contract and the
	 * caller already reports the suite unsupported on it. */
	(void)t; (void)shared; (void)out;
	return 0;
#else
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
#endif /* MCC_GPU_LANG_MSL */
}

static int bytes_kernel(int t, MccGpuCode *out) {
	return bytes_kernel_in(t, 0, out);
}

#define SWS_NSLOT 1
#define SWS_ARENA_WORD 2048
#define SWS_SPAN_BYTE 256
#define SWS_LANES MCC_GPU_LOCAL_SIZE

static int subword_shared_kernel(int t, MccGpuCode *out) {
#if MCC_GPU_LANG_MSL
	/* Metal arm: no region layer, so no shared sub-word store. TODO.md §5 M4.
	 * Same contract as the other builders here -- 0 means "could not build". */
	(void)t; (void)out;
	return 0;
#else
	SpvMod m;
	SpvRegion r;
	uint32_t base, off, val;
	uint32_t *code;
	int nw = 0;
	spv_module_begin(&m, SWS_NSLOT);
	base = spv_main_begin(&m, SWS_NSLOT);
	r = spv_region_shared(m.id_mem, spv_const(&m, SWS_ARENA_WORD),
												spv_uintc(&m, SWS_SPAN_BYTE));
	off = spv_load_at(&m, base);
	val = spv_load_at(&m,
										spv_emit3(&m, SpvOpIAdd, m.id_int, base, spv_const(&m, 1)));
	if (mcc_slice_mutate)
		val = spv_emit3(&m, SpvOpBitwiseXor, m.id_int, val, spv_const(&m, 1));
	spv_store_region(&m, &r, off, spv_fit_v(&m, spv_mk(val, 0, 0), t), t);
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
#endif /* MCC_GPU_LANG_MSL */
}

static unsigned char *subword_shared_span(void) {
	void *mem = NULL;
	unsigned long msz = 0;
	if (!mcc_gpu_mem(&mem, &msz) || !mem)
		return NULL;
	if (msz < (unsigned long)SWS_ARENA_WORD * 4 + SWS_SPAN_BYTE)
		return NULL;
	return (unsigned char *)mem + (long)SWS_ARENA_WORD * 4;
}

static void subword_shared_one(int t, const char *what) {
	MccGpuCode code;
	unsigned char *span = subword_shared_span();
	unsigned char ref[SWS_SPAN_BYTE];
	int32_t *in, *ob;
	int width = ast_eval_slice_tsize(t);
	int i, j, bad = 0;
	if (!span) {
		CHECK(0, "the shared address space is mappable for the sub-word race case");
		return;
	}
	if (!subword_shared_kernel(t, &code)) {
		CHECK(0, "a sub-word store into a shared region emits");
		return;
	}
	CHECK(1, "a sub-word store into a shared region emits");
	in = (int32_t *)calloc((size_t)SWS_LANES * MCC_GPU_IN_SLOTS, sizeof *in);
	ob = (int32_t *)calloc((size_t)SWS_LANES * MCC_GPU_OUT_SLOTS, sizeof *ob);
	if (!in || !ob) {
		free(in);
		free(ob);
		free(code.p);
		return;
	}
	for (j = 0; j < SWS_SPAN_BYTE; j++) {
		ref[j] = (unsigned char)(0xA5u ^ (unsigned)j);
		span[j] = ref[j];
	}
	for (i = 0; i < SWS_LANES; i++) {
		int32_t off = (int32_t)i * width;
		int32_t v = (int32_t)(0x1234 + i * 7);
		int ok = 0;
		if (off + width > SWS_SPAN_BYTE)
			off = 0;
		in[(long)i * MCC_GPU_IN_SLOTS] = off;
		in[(long)i * MCC_GPU_IN_SLOTS + 1] = v;
		ast_eval_slice_bytes_store((uint32_t *)ref, SWS_SPAN_BYTE, off, t,
															 ast_eval_slice_fit(v, t), &ok);
		CHECK(ok == 1, "every lane's sub-word address is in range for the reference");
	}
	if (!mcc_gpu_dispatch_rw2(code.p, code.n, in, SWS_LANES, SWS_NSLOT, ob)) {
		CHECK(0, "the shared sub-word kernel dispatches");
		free(in);
		free(ob);
		free(code.p);
		return;
	}
	for (j = 0; j < SWS_SPAN_BYTE; j++) {
		if (ref[j] == span[j])
			continue;
		if (!bad)
			fprintf(stderr, "  SUBWORD %s byte %d want=%02x got=%02x\n", what, j,
							ref[j], span[j]);
		bad++;
	}
	CHECK(bad == 0, "and every lane's sub-word write survives the ones sharing its "
									"word, byte for byte against the reference");
	free(in);
	free(ob);
	free(code.p);
}

static void bytes_subword_shared(void) {
	static const int NARROW[] = {VT_BYTE, VT_BYTE | VT_UNSIGNED, VT_SHORT,
															 VT_SHORT | VT_UNSIGNED, VT_BOOL};
	static const int WIDE[] = {VT_INT, VT_INT | VT_UNSIGNED, VT_LLONG,
														 VT_LLONG | VT_UNSIGNED};
	MccGpuCode c;
	int i;
	for (i = 0; i < (int)(sizeof NARROW / sizeof *NARROW); i++) {
		if (bytes_kernel_in(NARROW[i], 1, &c)) {
			CHECK(1, "a shared region admits a sub-word store");
			free(c.p);
		} else {
			CHECK(0, "a shared region admits a sub-word store");
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
	if (!g_have_device)
		return;
	subword_shared_one(VT_BYTE, "byte");
	subword_shared_one(VT_BYTE | VT_UNSIGNED, "ubyte");
	subword_shared_one(VT_SHORT, "short");
	subword_shared_one(VT_BOOL, "bool");
}

static void suite_bytes(void) {
	if (!backend_has_regions()) {
		unsupported("a region layer", "TODO.md §5 stage M4");
		return;
	}
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
#if MCC_GPU_LANG_MSL
	/* Metal arm: no region layer, so no host-pointer deref. TODO.md §5 M4. */
	(void)t; (void)out;
	return 0;
#else
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
#endif /* MCC_GPU_LANG_MSL */
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
	if (!backend_has_regions()) {
		unsupported("a region layer", "TODO.md §5 stage M4");
		return;
	}
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

#define FMT_NSLOT (MCC_FMT_MAXARG + 1)
#define FMT_ARENA_WORD 4096
#define FMT_LANE_WORD 32
#define FMT_LANE_BYTE (FMT_LANE_WORD * 4)
#define FMT_LANES MCC_GPU_LOCAL_SIZE
#define FMT_DST 0
#define FMT_POOL_BYTE 8192

static const uint32_t FMT_SIZE[] = {0, 1, 5, 64};
#define FMT_SIZE_N ((int)(sizeof FMT_SIZE / sizeof FMT_SIZE[0]))

static const char *const FMT_STR[] = {"",
																			"a",
																			"ab",
																			"abc",
																			"hello, world",
																			"0123456789abcdef0123456789abcde",
																			"0123456789abcdef0123456789abcdef01"};
#define FMT_STR_N ((int)(sizeof FMT_STR / sizeof FMT_STR[0]))
#define FMT_PTR_N (FMT_STR_N + 3)
#define FMT_TAIL 8

static int64_t g_fmt_ptr[FMT_PTR_N];
static MccFmtSrc g_fmt_src;

static int fmt_pool(void) {
	void *mem = NULL;
	unsigned long msz = 0;
	unsigned char *b;
	uint32_t off = FMT_POOL_BYTE + 1;
	int i, k = 0;
	if (!mcc_gpu_mem(&mem, &msz) || !mem || msz < (1u << 20))
		return 0;
	b = (unsigned char *)mem;
	for (i = 0; i < FMT_STR_N; i++) {
		size_t n = strlen(FMT_STR[i]) + 1;
		memcpy(b + off, FMT_STR[i], n);
		g_fmt_ptr[k++] = (int64_t)(intptr_t)(b + off);
		off += (uint32_t)n + 1;
	}
	memset(b + msz - FMT_TAIL, 'Z', FMT_TAIL);
	g_fmt_ptr[k++] = (int64_t)(intptr_t)(b + msz - FMT_TAIL);
	g_fmt_ptr[k++] = 0;
	g_fmt_ptr[k++] = (int64_t)(intptr_t)FMT_STR[4];
	g_fmt_src.w = (const uint32_t *)mem;
	g_fmt_src.nbyte = (uint32_t)msz;
	g_fmt_src.base = (int64_t)(intptr_t)mem;
	return 1;
}

static int fmt_kernel(const MccFmtProg *p, MccGpuCode *out) {
#if MCC_GPU_LANG_MSL
	/* Metal arm: src/mccfmt.h has no MSL half, and every fmt primitive
	 * addresses a region. TODO.md §5 stage M5, which depends on M4. */
	(void)p; (void)out;
	return 0;
#else
	SpvMod m;
	SpvRegion r, s;
	SpvV arg[MCC_FMT_MAXARG];
	uint32_t base, mbase, size, len;
	uint32_t *code;
	int nw = 0, i;
	if (p->narg > FMT_NSLOT - 1)
		return 0;
	spv_module_begin(&m, FMT_NSLOT);
	m.mem_base = g_fmt_src.base;
	m.mem_nbyte = g_fmt_src.nbyte;
	base = spv_main_begin(&m, FMT_NSLOT);
	mbase = spv_emit3(&m, SpvOpIAdd, m.id_int, spv_const(&m, FMT_ARENA_WORD),
										spv_emit3(&m, SpvOpIMul, m.id_int, m.lane,
															spv_const(&m, FMT_LANE_WORD)));
	r = spv_region(m.id_mem, mbase, spv_uintc(&m, FMT_LANE_BYTE));
	s = spv_region_shared(m.id_mem, spv_const(&m, 0),
												spv_uintc(&m, g_fmt_src.nbyte));
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
	len = spv_fmt_emit(&m, &r, &s, p, spv_uintc(&m, FMT_DST), size, arg,
										 p->narg);
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
#endif /* MCC_GPU_LANG_MSL */
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

static const int32_t FMT_PREC[] = {-1, 0, 1, 3, 5, 12, 31, 33};
#define FMT_PREC_N ((int)(sizeof FMT_PREC / sizeof FMT_PREC[0]))

static int64_t fmt_fit(const MccFmtItem *it, int64_t v) {
	if (it->kind == MCC_FMT_CHR)
		return (int64_t)((uint64_t)v & 0xFFu);
	if (it->wide)
		return v;
	return it->sgn ? (int64_t)(int32_t)v : (int64_t)(uint32_t)v;
}

static int64_t fmt_arg(const MccFmtItem *it, int lane, int k) {
	if (it->kind == MCC_FMT_STR)
		return g_fmt_ptr[(lane + k * 3) % FMT_PTR_N];
	if (it->kind == MCC_FMT_PREC)
		return (int64_t)FMT_PREC[(lane + k * 5) % FMT_PREC_N];
	return fmt_fit(it, W64[(lane + k * 5) % W64_N]);
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
static int g_fmt_report;

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
	if (!arena || !fmt_pool()) {
		CHECK(0, "the shared address space holds the per-lane format arena");
		return;
	}
	if (!fmt_kernel(&p, &code)) {
		fprintf(stderr, "FAIL fmt_case: \"%s\" did not emit\n", f);
		g_checks++;
		g_failures++;
		return;
	}
	CHECK(code.n <= p.cost,
				"the compile-time cost model bounds what the emitter lays down");
	if (g_fmt_report)
		fprintf(stderr, "  FMT cost \"%s\" predicted=%d emitted=%d\n", f, p.cost,
						code.n);
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
			a[k] = fmt_arg(it, i, k);
			in[sp] = (int32_t)(uint32_t)(uint64_t)a[k];
			in[sp + 1] = (int32_t)(uint32_t)((uint64_t)a[k] >> 32);
		}
		in[(long)i * FMT_NSLOT * MCC_GPU_IN_SLOTS +
			 (FMT_NSLOT - 1) * MCC_GPU_IN_SLOTS] = (int32_t)size;
		clen[i] = mcc_fmt_exec(&p, w, FMT_LANE_BYTE, FMT_DST, size, a, p.narg,
													 &g_fmt_src);
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

static int fmt_hexv(int c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int fmt_verdict_mode(void) {
	static char line[1 + 2 * MCC_FMT_MAXLIT * 8];
	static char buf[sizeof line / 2];
	while (fgets(line, (int)sizeof line, stdin)) {
		MccFmtProg p;
		size_t n = strlen(line), i, k = 0;
		int ok;
		while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
			line[--n] = 0;
		if (n & 1) {
			fprintf(stderr, "slicerun --fmt-verdict: odd hex run\n");
			return 2;
		}
		for (i = 0; i < n; i += 2) {
			int hi = fmt_hexv((unsigned char)line[i]);
			int lo = fmt_hexv((unsigned char)line[i + 1]);
			if (hi < 0 || lo < 0) {
				fprintf(stderr, "slicerun --fmt-verdict: bad hex\n");
				return 2;
			}
			buf[k++] = (char)(hi * 16 + lo);
		}
		buf[k] = 0;
		ok = mcc_fmt_compile(buf, &p);
		printf("%d %d %d %d\n", ok ? MCC_FMT_OK : p.refuse, p.cost, p.n, p.narg);
	}
	return fflush(stdout) ? 2 : 0;
}

static void fmt_refusals(void) {
	static const struct {
		const char *f;
		int why;
	} R[] = {{"%p", MCC_FMT_R_PTR},         {"%f", MCC_FMT_R_FLOAT},
					 {"%.17g", MCC_FMT_R_FLOAT},   {"%e", MCC_FMT_R_FLOAT},
					 {"%Lf", MCC_FMT_R_FLOAT},     {"%*d", MCC_FMT_R_SPEC},
					 {"%-10d", MCC_FMT_R_SPEC},    {"%02d", MCC_FMT_R_SPEC},
					 {"%o", MCC_FMT_R_SPEC},       {"%n", MCC_FMT_R_SPEC},
					 {"%+d", MCC_FMT_R_SPEC},      {"%#x", MCC_FMT_R_SPEC},
					 {"%.3d", MCC_FMT_R_SPEC},     {"%ls", MCC_FMT_R_SPEC},
					 {"%08s", MCC_FMT_R_SPEC},     {"%+s", MCC_FMT_R_SPEC},
					 {"%*s", MCC_FMT_R_SPEC},      {"%40s", MCC_FMT_R_SPEC},
					 {"%s%s%s", MCC_FMT_R_ROOM},   {"%s %s %d", MCC_FMT_R_ROOM},
					 {"%lld %lld %lld", MCC_FMT_R_ROOM},
					 {"%lu.%lu.%lu", MCC_FMT_R_ROOM},
					 {"%s%s-%s", MCC_FMT_R_ROOM},
					 {"%s %2d %d", MCC_FMT_R_SPEC},
					 {"arity %s n=%u nc=%u op=%d", MCC_FMT_R_ROOM},
					 {"'%s' has internal linkage but is referenced in an inline "
						"function with external linkage",
						MCC_FMT_R_ROOM}};
	static const char *const A[] = {
			"%s",      "%-3s",   "a %s b",   "%.*s",
			"%6s",     "%-8s",   "%.5s",     "%s:%s",
			"%s=%d",   "%.*s/%s", "%s (%d)", "%.*s%s",
			"%d %d %d", "%d %d %d %d", "root width %u != src %u",
			"reflect size %zu != src %u", "%smcc-me-%u-%u.c",
			"%smcc-tmp-%u-%u.tmp",
			"\tfirst=%d\tend=%d\tblen=%d\tnlen=%d",
			"int v(int x){volatile int a=0;int k;for(k=0;k<600;k++)a++;(void)x;"
			"return %d;}"};
	MccFmtProg p;
	int i;
	for (i = 0; i < (int)(sizeof R / sizeof *R); i++) {
		CHECK(mcc_fmt_compile(R[i].f, &p) == 0,
					"a conversion outside the tranche is refused, not approximated");
		if (p.refuse != R[i].why)
			fprintf(stderr, "  FMT refuse \"%s\" want=%d got=%d\n", R[i].f, R[i].why,
							p.refuse);
		CHECK(p.refuse == R[i].why,
					"and the refusal says which of the four reasons it is");
	}
	for (i = 0; i < (int)(sizeof A / sizeof *A); i++) {
		int ok = mcc_fmt_compile(A[i], &p);
		if (!ok)
			fprintf(stderr, "  FMT accept \"%s\" refused: %s\n", A[i],
							mcc_fmt_why(p.refuse));
		CHECK(ok == 1, "every spelling the corpus now gets accepted still compiles");
		CHECK(p.cost <= MCC_FMT_MAXCOST,
					"and its straight-line cost is inside the module budget");
	}
	CHECK(mcc_fmt_compile("%d", &p) == 1 && p.narg == 1, "%d is in scope");
	CHECK(mcc_fmt_compile("%016llx", &p) == 1 && p.it[0].width == 16 &&
					p.it[0].pad == '0',
				"and so is the zero-padded wide hex the corpus actually uses");
	CHECK(mcc_fmt_compile("%%", &p) == 1 && p.narg == 0 && p.n == 1 &&
					p.it[0].llen == 1,
				"a doubled percent is one literal byte and consumes no argument");
	CHECK(mcc_fmt_compile("%s", &p) == 1 && p.narg == 1 &&
					p.it[0].kind == MCC_FMT_STR && p.it[0].prc == MCC_FMT_P_NONE,
				"a bare %s is one item and one argument");
	CHECK(mcc_fmt_compile("%.*s", &p) == 1 && p.narg == 2 && p.n == 2 &&
					p.it[0].kind == MCC_FMT_PREC && p.it[1].prc == MCC_FMT_P_DYN,
				"a star precision is its own item, so it consumes its own argument");
	CHECK(mcc_fmt_compile("%-8s", &p) == 1 && p.it[0].left == 1 &&
					p.it[0].width == 8,
				"and left-justified width is carried on the item, not approximated");
	{
		MccFmtProg q;
		int narrow, wide;
		CHECK(mcc_fmt_compile("%d", &p) == 1 && mcc_fmt_compile("%lld", &q) == 1,
					"the narrow and the wide decimal both compile");
		narrow = p.cost;
		wide = q.cost;
		CHECK(narrow * 2 < wide,
					"a 32-bit decimal costs less than half a 64-bit one, because it "
					"needs ten native divisions instead of twenty software ones");
		CHECK(mcc_fmt_compile("%x", &p) == 1 && mcc_fmt_compile("%llx", &q) == 1 &&
						p.cost * 2 < q.cost,
					"and the same asymmetry holds for hex");
		CHECK(mcc_fmt_compile("%u", &p) == 1 && p.it[0].wide == 0 &&
						mcc_fmt_compile("%zu", &q) == 1 && q.it[0].wide == 1,
					"the length modifier, not the conversion, is what picks the path");
	}
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
			{"%d", 12345, 0, NULL, 5},
			{"%u", 1000000000, 64, "1000000000", 10},
			{"%d", 999999999, 64, "999999999", 9},
			{"%x", (int64_t)0xffffffffu, 64, "ffffffff", 8},
			{"%u", 0, 64, "0", 1},
			{"%x", 0, 64, "0", 1}};
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
		got = mcc_fmt_exec(&p, w, FMT_LANE_BYTE, FMT_DST, D[i].size, &a, 1, NULL);
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
	{
		static const struct {
			const char *f;
			int64_t a;
			const char *want;
		} N[] = {{"%u", (int64_t)0x1234567800000005ll, "5"},
						 {"%d", (int64_t)0x00000001ffffffffll, "-1"},
						 {"%x", (int64_t)0xabcdef00deadbeefll, "deadbeef"},
						 {"%llu", (int64_t)0x0000000100000000ll, "4294967296"}};
		for (i = 0; i < (int)(sizeof N / sizeof *N); i++) {
			int64_t a = N[i].a;
			int bad = 0;
			if (!mcc_fmt_compile(N[i].f, &p)) {
				CHECK(0, "the narrowing formats compile");
				continue;
			}
			for (j = 0; j < FMT_LANE_WORD; j++)
				w[j] = 0xCCCCCCCCu;
			mcc_fmt_exec(&p, w, FMT_LANE_BYTE, FMT_DST, 64, &a, 1, NULL);
			for (j = 0; j <= (int)strlen(N[i].want); j++)
				if (((w[j >> 2] >> ((j & 3) * 8)) & 0xFFu) !=
						(unsigned char)N[i].want[j])
					bad++;
			if (bad)
				fprintf(stderr, "  FMT narrow \"%s\" want=\"%s\"\n", N[i].f, N[i].want);
			CHECK(bad == 0,
						"a conversion with no length modifier reads the low 32 bits and "
						"nothing else, which is what the emitter's narrow path does");
		}
	}
}

#define FMT_SBW 32
#define FMT_SBB (FMT_SBW * 4)
#define FMT_SN 5

static void fmt_directed_str(void) {
	static const char *const S[FMT_SN] = {
			"", "abc", "hi", "abcdef",
			"0123456789012345678901234567890123456789"};
	static const struct {
		const char *f;
		int si;
		int64_t prec;
		uint32_t size;
		const char *want;
		uint32_t len;
	} D[] = {{"%s", 1, 0, 64, "abc", 3},
					 {"%s", 0, 0, 64, "", 0},
					 {"[%s]", 2, 0, 64, "[hi]", 4},
					 {"%6s", 1, 0, 64, "   abc", 6},
					 {"%-6s", 1, 0, 64, "abc   ", 6},
					 {"%3s", 3, 0, 64, "abcdef", 6},
					 {"%-3s", 3, 0, 64, "abcdef", 6},
					 {"%.2s", 1, 0, 64, "ab", 2},
					 {"%.0s", 1, 0, 64, "", 0},
					 {"%.*s", 3, 4, 64, "abcd", 4},
					 {"%.*s", 3, -1, 64, "abcdef", 6},
					 {"%.*s", 3, 0, 64, "", 0},
					 {"%6.2s", 1, 0, 64, "    ab", 6},
					 {"%-6.2s", 1, 0, 64, "ab    ", 6},
					 {"%s", 3, 0, 4, "abc", 6},
					 {"%s", 3, 0, 1, "", 6},
					 {"%s:%s", 2, 0, 64, "hi:hi", 5},
					 {"%s", -1, 0, 64, "", 0},
					 {"%s", -2, 0, 64, "", 0},
					 {"%s", -3, 0, 64, "QQQQ", 4}};
	uint32_t sbw[FMT_SBW], w[FMT_LANE_WORD];
	unsigned char *sb = (unsigned char *)sbw;
	uint32_t soff[FMT_SN];
	MccFmtSrc src;
	MccFmtProg p;
	uint32_t off = 1;
	int i, j;
	memset(sbw, 0, sizeof sbw);
	memset(soff, 0, sizeof soff);
	for (i = 0; i < FMT_SN; i++) {
		size_t n = strlen(S[i]) + 1;
		if (off + n + 1 > FMT_SBB - 4)
			break;
		memcpy(sb + off, S[i], n);
		soff[i] = off;
		off += (uint32_t)n + 1;
	}
	memset(sb + FMT_SBB - 4, 'Q', 4);
	src.w = sbw;
	src.nbyte = FMT_SBB;
	src.base = (int64_t)(intptr_t)sb;
	for (i = 0; i < (int)(sizeof D / sizeof *D); i++) {
		int64_t a[MCC_FMT_MAXARG];
		uint32_t got;
		int bad = 0, k, ai = 0;
		if (!mcc_fmt_compile(D[i].f, &p)) {
			fprintf(stderr, "  FMT str \"%s\" refused: %s\n", D[i].f,
							mcc_fmt_why(p.refuse));
			CHECK(0, "the directed string formats all compile");
			continue;
		}
		for (k = 0; k < p.narg; k++) {
			const MccFmtItem *it = fmt_conv(&p, k);
			if (it->kind == MCC_FMT_PREC)
				a[ai++] = D[i].prec;
			else if (D[i].si >= 0)
				a[ai++] = src.base + soff[D[i].si];
			else if (D[i].si == -1)
				a[ai++] = 0;
			else if (D[i].si == -2)
				a[ai++] = src.base + FMT_SBB + 4096;
			else
				a[ai++] = src.base + FMT_SBB - 4;
		}
		for (j = 0; j < FMT_LANE_WORD; j++)
			w[j] = 0xCCCCCCCCu;
		got = mcc_fmt_exec(&p, w, FMT_LANE_BYTE, FMT_DST, D[i].size, a, p.narg,
											 &src);
		if (got != D[i].len)
			fprintf(stderr, "  FMT str \"%s\" #%d len want=%u got=%u\n", D[i].f, i,
							D[i].len, got);
		CHECK(got == D[i].len, "the reference returns the untruncated length");
		for (j = 0; j <= (int)strlen(D[i].want); j++) {
			unsigned b = (w[j >> 2] >> ((j & 3) * 8)) & 0xFFu;
			unsigned e = (unsigned char)D[i].want[j];
			if (b != e) {
				fprintf(stderr, "  FMT str \"%s\" #%d byte %d want=%02x got=%02x\n",
								D[i].f, i, j, e, b);
				bad++;
			}
		}
		CHECK(bad == 0, "and copies exactly the hand-written bytes plus a NUL");
	}
}

static void suite_fmt(void) {
	if (!backend_has_regions()) {
		unsupported("the on-device format engine", "TODO.md §5 stage M5");
		return;
	}
	static const char *F[] = {
			"%d",     "%u",     "%x",      "%X",     "%lld", "%llu",
			"%llx",   "%zu",    "%c",      "%02x",   "%08x", "%016llx",
			"[%d]",   "%%",     "n=%d.",   "%u/%x",  "%s",   "[%s]",
			"%6s",    "%-8s",   "%.*s",    "%.5s",   "%s=%d", "%s:%s",
			"%.*s/%s",
			"root width %u != src %u",
			"reflect size %zu != src %u",
			"%smcc-me-%u-%u.c",
			"%smcc-tmp-%u-%u.tmp",
			"\tfirst=%d\tend=%d\tblen=%d\tnlen=%d",
			"int v(int x){volatile int a=0;int k;for(k=0;k<600;k++)a++;(void)x;"
			"return %d;}"};
	int i;
	fmt_refusals();
	fmt_directed();
	fmt_directed_str();
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

/* ------------------------------------------------ M1: store round-trip -- */

/* Loads slot 0 (signed) and slot 1 (unsigned), adds 1 to the first, and stores
 * both back through the live-slot path. Slot 1 is rewritten unchanged and is
 * there for its high word: it must zero-extend, not sign-extend. */
static int rwstore_kernel(MccGpuCode *out) {
#if MCC_GPU_LANG_MSL
	MslMod m;
	char *code;
	uint32_t base;
	int nb = 0;
	MslV a, b;
	msl_module_begin(&m, 2);
	base = msl_main_begin(&m, 2);
	a = msl_load_live_v(&m, base, 0, 0, 0);
	b = msl_load_live_v(&m, base, 1, 0, 1);
	a = msl_mk(msl_iv(&m, "mcc_add(v%u, v%u)", a.id, msl_const(&m, 1)), 0, 0);
	msl_store_live_v(&m, base, 0, a);
	msl_store_live_v(&m, base, 1, b);
	msl_main_end(&m, m.lane, msl_mk(msl_const(&m, 0), 0, 0));
	if (m.failed) {
		msl_module_free(&m);
		return 0;
	}
	code = msl_module_finish(&m, &nb);
	msl_module_free(&m);
	if (!code || nb <= 0) {
		free(code);
		return 0;
	}
	out->p = code;
	out->n = nb;
	return 1;
#else
	SpvMod m;
	uint32_t *code;
	uint32_t base;
	int nw = 0;
	SpvV a, b;
	spv_module_begin(&m, 2);
	base = spv_main_begin(&m, 2);
	a = spv_load_live_v(&m, base, 0, 0, 0);
	b = spv_load_live_v(&m, base, 1, 0, 1);
	a = spv_mk(spv_emit3(&m, SpvOpIAdd, m.id_int, a.id, spv_const(&m, 1)), 0, 0);
	spv_store_live_v(&m, base, 0, a);
	spv_store_live_v(&m, base, 1, b);
	spv_main_end(&m, m.lane, spv_mk(spv_const(&m, 0), 0, 0));
	if (m.failed) {
		spv_module_free(&m);
		return 0;
	}
	code = spv_module_finish(&m, &nw);
	spv_module_free(&m);
	if (!code || nw <= 0) {
		free(code);
		return 0;
	}
	out->p = code;
	out->n = nw;
	return 1;
#endif
}


/* The live-slot store path has no other differential. Every existing device
 * suite either never stores (gpu, ops, wide64 are expression suites reaching
 * the device through mcc_gpu_dispatch, which cannot write buffer 0) or is
 * behind a stage that is not built yet (frame needs M2, bytes/deref/fmt need
 * M4). So mcc_gpu_dispatch_rw2 had zero reachable call sites on the Metal arm
 * and M1 could have been entirely wrong while every cell stayed green.
 *
 * This builds one module by hand -- load slot k, transform, store it back --
 * dispatches it read-write, and checks the host frame afterwards. It also
 * passes out = NULL, which is the shape mcc_gpu_dispatch_rw uses and which was
 * an unguarded memcpy on the Metal arm until rw support was enabled. */
static void suite_rwstore(void) {
#define RW_NSLOT 2
#define RW_NT 8
	int32_t frame[RW_NT * RW_NSLOT * MCC_GPU_IN_SLOTS];
	int32_t want[RW_NT * RW_NSLOT * MCC_GPU_IN_SLOTS];
	MccGpuCode code;
	int t, ok;

	if (!g_have_device) {
		if (g_device_required) {
			fprintf(stderr, "FAIL slicerun: no usable device but a device is required\n");
			g_failures++;
		}
		return;
	}
	memset(&code, 0, sizeof code);
	if (!rwstore_kernel(&code)) {
		fprintf(stderr, "FAIL suite_rwstore: the store kernel did not build\n");
		g_failures++;
		return;
	}

	/* slot 0 = a signed value that goes negative, slot 1 = an unsigned one with
	 * the high bit set. The second is the case spv_store_live_v's comment is
	 * about: sign-extending it would put -2 in the slot where the host must see
	 * 4294967294. */
	for (t = 0; t < RW_NT; t++) {
		int b = t * RW_NSLOT * MCC_GPU_IN_SLOTS;
		frame[b + 0] = t - 4;
		frame[b + 1] = (t - 4) < 0 ? -1 : 0;
		frame[b + 2] = (int32_t)0xFFFFFFFEu;
		frame[b + 3] = 0;
	}
	memcpy(want, frame, sizeof frame);
	for (t = 0; t < RW_NT; t++) {
		int b = t * RW_NSLOT * MCC_GPU_IN_SLOTS;
		int32_t v = want[b + 0] + 1;
		want[b + 0] = v;
		want[b + 1] = v < 0 ? -1 : 0;
		/* slot 1 is rewritten unchanged, through the unsigned path */
		want[b + 2] = (int32_t)0xFFFFFFFEu;
		want[b + 3] = 0;
	}

	ok = mcc_gpu_dispatch_rw2(code.p, code.n, frame, RW_NT, RW_NSLOT, NULL);
	if (!ok) {
		/* mcc_gpu_dispatch_rw2 returns 0 both when the backend has no read-write
		 * path and when the dispatch itself failed. Either way nothing was
		 * compared, so this must not report success. */
		free(code.p);
		unsupported("a read-write dispatch", "TODO.md §5 stage M1");
		return;
	}
	g_checks++;
	{
		int bad = 0;
		for (t = 0; t < RW_NT * RW_NSLOT * MCC_GPU_IN_SLOTS; t++)
			if (frame[t] != want[t]) {
				if (!bad)
					fprintf(stderr,
									"  RWSTORE word %d cpu=%d gpu=%d\n", t, want[t], frame[t]);
				bad++;
			}
		g_checks++;
		if (bad) {
			fprintf(stderr, "FAIL suite_rwstore: %d of %d words wrong after the "
											"store-back\n",
							bad, RW_NT * RW_NSLOT * MCC_GPU_IN_SLOTS);
			g_failures++;
		}
	}
	free(code.p);
#undef RW_NSLOT
#undef RW_NT
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
	/* Deliberately NOT gated on backend_has_regions(): this suite tests the host
	 * mapping only -- that a shared region exists, is process-resident and can be
	 * seeded. No kernel addresses it, so it is live on a backend whose emitter
	 * has no region layer yet. */
	CHECK(mcc_gpu_mem(&base, &size) == 1, "the shared address space is mappable");
	CHECK(base != NULL, "and the host has a pointer to it");
	CHECK(size >= (1u << 20), "and it is at least the default extent");
	/* CHECK records and continues, so without this the NULL base below is
	 * dereferenced and the suite dies with SIGSEGV rather than reporting three
	 * failures. That is exactly what it did on the Metal arm. */
	if (!base || size < (1u << 20))
		return;

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

	/* The rest of this suite forces a fence timeout to strand a dispatch. That
	 * needs MCC_GPU_FENCE_NS, which only the Vulkan arm reads (src/mccgpu.c,
	 * inside the !MCC_GPU_LANG_MSL block) -- the Metal dispatch is synchronous
	 * through waitUntilCompleted and has no pending-submission state to strand.
	 * The checks above are real on both arms; asserting the stranding semantics
	 * on a backend that cannot produce them would grade the backend, not the
	 * fault path. */
	if (!backend_has_fence_timeout()) {
		fprintf(stderr, "slicerun: this backend cannot strand a dispatch (no "
										"fence-timeout injection); the stranding half of suite_fault "
										"is not applicable\n");
		mcc_slice_kernel_free(&k);
		ast_arena_free(a);
		return;
	}
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

	/* L2'(iii). The teardown now runs on a device with a command buffer that is
	 * pending forever, which is exactly the state the old one could not survive:
	 * it called vkDeviceWaitIdle, which takes no timeout, from a path that is
	 * reached out of an atexit handler. It must return, and it must not hand the
	 * stranded objects back to the driver -- so the release is skipped and a
	 * second call is still a no-op. */
	{
		double t0 = mcc_slice_now();
		mcc_gpu_quiesce();
		CHECK(mcc_slice_now() - t0 < 5e9,
					"quiesce returns on a device with a permanently pending submission");
		mcc_gpu_quiesce();
		CHECK(mcc_gpu_stranded() == 1,
					"and it strands nothing further, having destroyed nothing");
	}

	mcc_slice_kernel_free(&k);
	ast_arena_free(a);
}

/* ------------------------------------------------------- L2'(ii): lost -- */

/* VK_ERROR_DEVICE_LOST was a declared constant that nothing compared against.
 * Every handle derived from a lost device is unusable, so the only correct
 * response is to stop -- and before this the return code became a bare
 * `return 0`, mcc_gpu.ok stayed set, and the next dispatch called back into a
 * driver whose device no longer exists.
 *
 * This runs in its own process for the same reason suite_fault does: the
 * device is deliberately destroyed for the rest of it. The loss is injected at
 * vkQueueSubmit, after a healthy dispatch has proved the device works, so what
 * is under test is the transition and not a device that never came up. */
static void suite_lost(void) {
	AstArena *a;
	AstLocal root;
	MccSliceWork w;
	MccSliceKernel k;
	int64_t out[AFFINE_N];
	unsigned char def[AFFINE_N];
	int st;

	if (!g_have_device) {
		if (g_device_required) {
			fprintf(stderr, "FAIL slicerun: no usable device but a device is required\n");
			g_failures++;
		}
		return;
	}
	if (!backend_has_fence_timeout()) {
		fprintf(stderr, "slicerun: device-loss injection is Vulkan-only "
										"(MCC_GPU_FORCE_DEVICE_LOST); suite_lost is not "
										"applicable to this backend\n");
		return;
	}

	a = ast_arena_new();
	root = build_affine(a);
	CHECK(mcc_slice_work_from_ast(a, root, &w) == 1, "affine slice yields work");
	CHECK(mcc_slice_kernel_build(&w, &k) == 1, "the slice lowers");

	mcc_slice_work_bind(&w, AFFINE_IN, AFFINE_N, out, def);
	CHECK(mcc_slice_run_gpu(&w, &k, 0) == MCC_TASK_DONE, "a healthy dispatch runs");
	CHECK(mcc_gpu_alive() == 1, "and the device is alive before the loss");

	setenv("MCC_GPU_FORCE_DEVICE_LOST", "vkQueueSubmit", 1);
	memset(out, 0xA5, sizeof out);
	memset(def, 0xA5, sizeof def);
	mcc_slice_work_bind(&w, AFFINE_IN, AFFINE_N, out, def);
	st = mcc_slice_run_gpu(&w, &k, 0);
	CHECK(st == MCC_TASK_FAILED, "a dispatch onto a lost device reports failure");
	CHECK(w.done == 0, "and consumes no tuple");
	CHECK(mcc_gpu_alive() == 0, "VK_ERROR_DEVICE_LOST clears mcc_gpu.ok");

	/* Un-arming must not resurrect it. The flag is a property of the device,
	 * not of the last call. */
	unsetenv("MCC_GPU_FORCE_DEVICE_LOST");
	memset(out, 0xA5, sizeof out);
	mcc_slice_work_bind(&w, AFFINE_IN, AFFINE_N, out, def);
	st = mcc_slice_run_gpu(&w, &k, 0);
	CHECK(st == MCC_TASK_FAILED, "a dispatch after the loss still fails");
	CHECK(mcc_gpu_alive() == 0, "and the device stays dead");

	/* L2'(iii) on the loss path: no wait at all is legitimate here, and an
	 * unbounded one is not. This is the call that cluster L wants to reach from
	 * mccjit_shutdown on every run. */
	{
		double t0 = mcc_slice_now();
		mcc_gpu_quiesce();
		CHECK(mcc_slice_now() - t0 < 5e9, "quiesce returns on a lost device");
		mcc_gpu_quiesce();
		CHECK(mcc_gpu_alive() == 0, "and a second quiesce is a no-op");
	}

	mcc_slice_kernel_free(&k);
	ast_arena_free(a);
}

/* ---------------------------------------------------- L2'(iii): teardown -- */

/* The healthy half of the teardown, which the two suites above cannot reach:
 * both of them quiesce a device that is lost or stranded, where destroying
 * nothing is the correct answer. Here everything the device owns is genuinely
 * released, so this is the cell to run under VK_LAYER_KHRONOS_validation --
 * an object left behind is reported at vkDestroyInstance, and before this
 * there was no vkDestroyInstance to report it.
 *
 * Its own process: the device is gone afterwards. */
static void suite_quiesce(void) {
	AstArena *a;
	AstLocal root;
	MccSliceWork w;
	MccSliceKernel k;
	int64_t out[AFFINE_N];
	unsigned char def[AFFINE_N];
	double t0;
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
	CHECK(mcc_slice_kernel_build(&w, &k) == 1, "the slice lowers");
	mcc_slice_work_bind(&w, AFFINE_IN, AFFINE_N, out, def);
	CHECK(mcc_slice_run_gpu(&w, &k, 0) == MCC_TASK_DONE, "a healthy dispatch runs");
	for (i = 0; i < AFFINE_N; i++)
		CHECK(out[i] == AFFINE_EXPECT[i], "and returns the expected values");

	t0 = mcc_slice_now();
	mcc_gpu_quiesce();
	CHECK(mcc_slice_now() - t0 < 5e9, "quiesce returns on a healthy device");

	/* The invariant the old quiesce got for free by doing nothing, and that a
	 * teardown must not break: the door is shut against dispatch, but the
	 * question "did a device come up" still answers yes. ast_ladder_gpu_report
	 * quiesces and then prints mcc_gpu_stats, cmake/ladder_gpu_parity.cmake
	 * skips the whole cell on available=0, and tools/smokerun.c fails the run
	 * on it -- so clearing mcc_gpu.ok here would disarm the device suite rather
	 * than break it. */
	CHECK(mcc_gpu_alive() == 1, "quiesce does not retract the device's existence");

	memset(out, 0xA5, sizeof out);
	mcc_slice_work_bind(&w, AFFINE_IN, AFFINE_N, out, def);
	CHECK(mcc_slice_run_gpu(&w, &k, 0) == MCC_TASK_FAILED,
				"but no dispatch can start once the device has been torn down");
	CHECK(w.done == 0, "and it consumes no tuple");

	mcc_gpu_quiesce();
	CHECK(mcc_gpu_stranded() == 0,
				"a second quiesce strands nothing and destroys nothing twice");

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

static long g_bail_nodes;

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
		if (raw[i].kind == AST_Bailout)
			g_bail_nodes++;
		ast_set_op(a, id, raw[i].op);
		/* N12: consume all 12 dumped fields, not 7. sym and type_ref are interned
		 * ids after N7, not addresses, so they are stable identities -- but they
		 * are still installed into pointer-shaped slots, which is only safe
		 * because nothing on this path dereferences them (verified: zero uses of
		 * ast_sym/ast_type_ref/ast_fbits in ast_eval_slice.h and mccgpu.h; bp/bs
		 * are read there, and are plain integers rather than pointers). A
		 * consumer that needs the real Sym
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

static int64_t seed_f64_value(long t, int k) {
	static const double seeds[8] = {0.0, 1.0, -1.0, 2.0, 0.5, -3.0, 1000.0,
																	-12345.0};
	double d = seeds[(t * 3 + k * 5) & 7];
	uint64_t u;
	memcpy(&u, &d, sizeof u);
	return (int64_t)u;
}

static int slicerun_nan(int64_t b) {
	uint64_t u = (uint64_t)b;
	return (u & 0x7FF0000000000000ull) == 0x7FF0000000000000ull &&
				 (u & 0x000FFFFFFFFFFFFFull) != 0;
}

static long g_arena_nantie;
static long g_arena_f64_slices, g_arena_f64_frames;

static int arena_nan_tie(int flt, int64_t c, int64_t g) {
	return flt && c != g && slicerun_nan(c) && slicerun_nan(g);
}

static int subtree_has_f64(AstArena *a, AstLocal n, int depth) {
	AstLocal c;
	if (n == AST_NONE || depth > 24)
		return 0;
	if (ast_eval_slice_f64t(ast_type_t(a, n)))
		return 1;
	if (ast_kind(a, n) == AST_Load) {
		AstEvalSliceIdx ix;
		if (ast_eval_slice_dynidx(a, ast_first_child(a, n), &ix) &&
				ast_eval_slice_f64t(ix.etype))
			return 1;
	}
	if (ast_kind(a, n) == AST_Store) {
		AstEvalSliceIdx ix;
		if (mcc_slice_store_dyn(a, ast_child(a, n, 0), &ix) &&
				ast_eval_slice_f64t(ix.etype))
			return 1;
	}
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (subtree_has_f64(a, c, depth + 1))
			return 1;
	return 0;
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

static const char *g_cref_dir;
static const char *g_cref_pfx = "s";
static char g_cref_tag[64];
static FILE *g_cref_exp;
static long g_cref_emitted, g_cref_toobig, g_cref_unspellable, g_cref_seen;
static long g_cref_alldead, g_cref_tuples, g_cref_mixed;
static int g_cref_mutate;

#define CREF_MAXNODES 512

static const char *cref_ctype(int t) {
	int uns = (t & VT_UNSIGNED) != 0;
	switch (t & VT_BTYPE) {
	case VT_BOOL:
		return "_Bool";
	case VT_BYTE:
		return uns ? "unsigned char" : "signed char";
	case VT_SHORT:
		return uns ? "unsigned short" : "short";
	case VT_INT:
		return uns ? "unsigned int" : "int";
	case VT_LLONG:
		return uns ? "unsigned long long" : "long long";
	case VT_PTR:
		return uns ? "unsigned long long" : "long long";
	default:
		return NULL;
	}
}

static const char *cref_wtype(int t) {
	int uns = (t & VT_UNSIGNED) != 0;
	if (ast_eval_slice_is64(t))
		return uns ? "unsigned long long" : "long long";
	return uns ? "unsigned int" : "int";
}

static void cref_lit(FILE *f, int64_t v) {
	if (v == INT64_MIN)
		fprintf(f, "(-9223372036854775807LL - 1)");
	else
		fprintf(f, "%lldLL", (long long)v);
}

static void cref_glob_name(FILE *f, unsigned id, int32_t d) {
	if (d)
		fprintf(f, "g_%s_%u_%ld", g_cref_tag, id, (long)d);
	else
		fprintf(f, "g_%s_%u", g_cref_tag, id);
}

static int cref_slot(FILE *f, const char *ct, const int32_t *off, int nlive,
										 int32_t o) {
	unsigned id = 0;
	int32_t d = 0;
	int j;
	for (j = 0; j < nlive; j++)
		if (off[j] == o)
			break;
	if (j == nlive)
		return 0;
	fprintf(f, "((%s)", ct);
	if (slicerun_glob_of(o, &id, &d))
		cref_glob_name(f, id, d);
	else
		fprintf(f, "e%d", j);
	fprintf(f, ")");
	return 1;
}

static int cref_expr(FILE *f, AstArena *a, AstLocal n, const int32_t *off,
										 int nlive) {
	const char *ct;
	int t;
	if (n == AST_NONE)
		return 0;
	switch (ast_kind(a, n)) {
	case AST_Literal:
		ct = cref_ctype(ast_type_t(a, n));
		if (!ct)
			return 0;
		fprintf(f, "((%s)", ct);
		cref_lit(f, (int64_t)ast_ival(a, n));
		fprintf(f, ")");
		return 1;
	case AST_Ref: {
		int r = ast_op(a, n);
		int32_t o;
		ct = cref_ctype(ast_type_t(a, n));
		if (!ct)
			return 0;
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM))
			return cref_slot(f, ct, off, nlive, (int32_t)(int64_t)ast_ival(a, n));
		if (r & VT_SYM)
			return ast_eval_slice_globl(a, n, &o) &&
						 cref_slot(f, ct, off, nlive, o);
		fprintf(f, "((%s)", ct);
		cref_lit(f, (int64_t)ast_ival(a, n));
		fprintf(f, ")");
		return 1;
	}
	case AST_Convert:
		ct = cref_ctype(ast_type_t(a, n));
		if (!ct)
			return 0;
		fprintf(f, "((%s)(", ct);
		if (!cref_expr(f, a, ast_first_child(a, n), off, nlive))
			return 0;
		fprintf(f, "))");
		return 1;
	case AST_Unary: {
		int uop = ast_op(a, n);
		const char *s = uop == '~' ? "~" : uop == '!' ? "!" : "-";
		int32_t mo;
		if (ast_eval_slice_member_off(a, n, &mo)) {
			ct = cref_ctype(ast_type_t(a, n));
			if (!ct)
				return 0;
			return cref_slot(f, ct, off, nlive, mo);
		}
		if (uop != '-' && uop != TOK_NEG && uop != '~' && uop != '!')
			return 0;
		fprintf(f, "(%s(", s);
		if (!cref_expr(f, a, ast_first_child(a, n), off, nlive))
			return 0;
		fprintf(f, "))");
		return 1;
	}
	case AST_Binary: {
		int bop = ast_op(a, n);
		AstLocal x, y;
		const char *wt, *uwt, *swt, *s;
		int xt;
		if (bop == TOK_LAND || bop == TOK_LOR) {
			uint32_t nc = ast_nchild(a, n), k;
			fprintf(f, "(");
			for (k = 0; k < nc; k++) {
				if (k)
					fprintf(f, " %s ", bop == TOK_LAND ? "&&" : "||");
				fprintf(f, "(");
				if (!cref_expr(f, a, ast_child(a, n, k), off, nlive))
					return 0;
				fprintf(f, ")");
			}
			fprintf(f, ")");
			return 1;
		}
		if (ast_nchild(a, n) != 2)
			return 0;
		x = ast_child(a, n, 0);
		y = ast_child(a, n, 1);
		xt = ast_eval_slice_wtype(a, x);
		wt = cref_wtype(xt);
		uwt = ast_eval_slice_is64(xt) ? "unsigned long long" : "unsigned int";
		swt = ast_eval_slice_is64(xt) ? "long long" : "int";
		switch (bop) {
		case TOK_SHR:
		case TOK_UDIV:
		case TOK_UMOD:
			s = bop == TOK_SHR ? ">>" : bop == TOK_UDIV ? "/" : "%";
			fprintf(f, "((%s)((%s)(", wt, uwt);
			if (!cref_expr(f, a, x, off, nlive))
				return 0;
			fprintf(f, ") %s (%s)(", s, uwt);
			if (!cref_expr(f, a, y, off, nlive))
				return 0;
			fprintf(f, ")))");
			return 1;
		case TOK_SAR:
			fprintf(f, "((%s)((%s)(", wt, swt);
			if (!cref_expr(f, a, x, off, nlive))
				return 0;
			fprintf(f, ") >> (");
			if (!cref_expr(f, a, y, off, nlive))
				return 0;
			fprintf(f, ")))");
			return 1;
		case TOK_ULT:
		case TOK_UGE:
		case TOK_ULE:
		case TOK_UGT:
			s = bop == TOK_ULT ? "<" : bop == TOK_UGE ? ">=" : bop == TOK_ULE ? "<=" : ">";
			fprintf(f, "((unsigned long long)(long long)(%s)(", wt);
			if (!cref_expr(f, a, x, off, nlive))
				return 0;
			fprintf(f, ") %s (unsigned long long)(long long)(%s)(", s, wt);
			if (!cref_expr(f, a, y, off, nlive))
				return 0;
			fprintf(f, "))");
			return 1;
		default:
			break;
		}
		switch (bop) {
		case '+': s = "+"; break;
		case '-': s = "-"; break;
		case '*': s = "*"; break;
		case '/': case TOK_PDIV: s = "/"; break;
		case '%': s = "%"; break;
		case '&': s = "&"; break;
		case '|': s = "|"; break;
		case '^': s = "^"; break;
		case TOK_SHL: s = "<<"; break;
		case TOK_EQ: s = "=="; break;
		case TOK_NE: s = "!="; break;
		case TOK_LT: s = "<"; break;
		case TOK_GE: s = ">="; break;
		case TOK_LE: s = "<="; break;
		case TOK_GT: s = ">"; break;
		default: return 0;
		}
		fprintf(f, "((");
		if (!cref_expr(f, a, x, off, nlive))
			return 0;
		fprintf(f, ") %s (", s);
		if (!cref_expr(f, a, y, off, nlive))
			return 0;
		fprintf(f, "))");
		return 1;
	}
	case AST_If:
		if (ast_nchild(a, n) != 3)
			return 0;
		fprintf(f, "((");
		if (!cref_expr(f, a, ast_child(a, n, 0), off, nlive))
			return 0;
		fprintf(f, ") ? (");
		if (!cref_expr(f, a, ast_child(a, n, 1), off, nlive))
			return 0;
		fprintf(f, ") : (");
		if (!cref_expr(f, a, ast_child(a, n, 2), off, nlive))
			return 0;
		fprintf(f, "))");
		return 1;
	default:
		t = 0;
		(void)t;
		return 0;
	}
}

enum {
	REF_OK = 0,
	REF_CHILD,
	REF_KIND_INVOKE,
	REF_KIND_INVOKE_THREAD,
	REF_KIND_STORE,
	REF_KIND_STOREVAL,
	REF_KIND_BLOCK,
	REF_KIND_JUMP,
	REF_KIND_RETURN,
	REF_KIND_POISON,
	REF_KIND_OTHER,
	REF_LOAD,
	REF_TYPE_FLOAT,
	REF_TYPE_BAD,
	REF_TYPE_NONINT,
	REF_REF_GLOBAL,
	REF_LIT_NONCONST,
	REF_OP_UNARY,
	REF_OP_BINARY,
	REF_OP_TERNARY,
	REF_ARITY,
	REF_NOWTYPE,
	REF_N
};

static const char *refuse_name(int r) {
	switch (r) {
	case REF_OK: return "ok";
	case REF_CHILD: return "child-refused";
	case REF_KIND_INVOKE: return "kind-invoke";
	case REF_KIND_INVOKE_THREAD: return "kind-invoke-thread";
	case REF_KIND_STORE: return "kind-store";
	case REF_KIND_STOREVAL: return "kind-storeval";
	case REF_KIND_BLOCK: return "kind-basicblock";
	case REF_KIND_JUMP: return "kind-jump";
	case REF_KIND_RETURN: return "kind-return";
	case REF_KIND_POISON: return "kind-poison";
	case REF_KIND_OTHER: return "kind-other";
	case REF_LOAD: return "load-not-allowed";
	case REF_TYPE_FLOAT: return "type-float";
	case REF_TYPE_BAD: return "type-bad";
	case REF_TYPE_NONINT: return "type-nonint";
	case REF_REF_GLOBAL: return "ref-not-local";
	case REF_LIT_NONCONST: return "literal-not-const";
	case REF_OP_UNARY: return "op-unary";
	case REF_OP_BINARY: return "op-binary";
	case REF_OP_TERNARY: return "op-ternary";
	case REF_ARITY: return "arity";
	case REF_NOWTYPE: return "no-working-type";
	default: return "?";
	}
}

static int refuse_binop_known(int op) {
	switch (op) {
	case '+': case '-': case '*': case '/': case '%':
	case '&': case '|': case '^':
	case TOK_SHL: case TOK_SHR: case TOK_SAR:
	case TOK_PDIV: case TOK_UDIV: case TOK_UMOD:
	case TOK_EQ: case TOK_NE:
	case TOK_LT: case TOK_GE: case TOK_LE: case TOK_GT:
	case TOK_ULT: case TOK_UGE: case TOK_ULE: case TOK_UGT:
	case TOK_LAND: case TOK_LOR:
		return 1;
	default:
		return 0;
	}
}

static int refuse_type(int t) {
	if (is_float(t))
		return REF_TYPE_FLOAT;
	if (ast_bad_type(t))
		return REF_TYPE_BAD;
	if (!ast_eval_slice_intt(t))
		return REF_TYPE_NONINT;
	return REF_OK;
}

enum {
	POS_ROOT = 0,
	POS_LOAD_ADDR,
	POS_ADDR_CHAIN,
	POS_STMT,
	POS_STORE_DEST,
	POS_STORE_VAL,
	POS_VALUE,
	POS_CALL_ARG,
	POS_RETURN,
	POS_CTRL,
	POS_OTHER,
	POS_N
};

static const char *refuse_posname(int p) {
	switch (p) {
	case POS_ROOT: return "root";
	case POS_LOAD_ADDR: return "address-of-a-load";
	case POS_ADDR_CHAIN: return "base-of-an-address";
	case POS_STMT: return "statement-in-a-block";
	case POS_STORE_DEST: return "store-destination";
	case POS_STORE_VAL: return "store-value";
	case POS_VALUE: return "value-operand";
	case POS_CALL_ARG: return "call-operand";
	case POS_RETURN: return "return-value";
	case POS_CTRL: return "control-flow-operand";
	default: return "other";
	}
}

static long g_ref_pos[REF_N][POS_N];
static long g_ref_pos_acc[POS_N];
static long g_ref_pos_all[POS_N];

static int refuse_pos(AstArena *a, AstLocal parent, int idx) {
	if (parent == AST_NONE)
		return POS_ROOT;
	switch (ast_kind(a, parent)) {
	case AST_Load:
		return idx == 0 ? POS_LOAD_ADDR : POS_OTHER;
	case AST_BasicBlock:
		return POS_STMT;
	case AST_Store:
	case AST_StoreVal:
		return idx == 0 ? POS_STORE_DEST : POS_STORE_VAL;
	case AST_Unary: {
		int op = ast_op(a, parent);
		if (op == AST_EVAL_OP_ADDR || op == AST_EVAL_OP_MEMBER || op == 0x40002)
			return POS_ADDR_CHAIN;
		return POS_VALUE;
	}
	case AST_Binary:
	case AST_Convert:
		return POS_VALUE;
	case AST_If:
		return (ast_op(a, parent) == 5 || ast_op(a, parent) == 7) ? POS_VALUE
																															: POS_CTRL;
	case AST_Invoke:
		return POS_CALL_ARG;
	case AST_Return:
		return POS_RETURN;
	default:
		return POS_OTHER;
	}
}

static const char *refuse_kindname(int k) {
	switch (k) {
	case AST_Literal: return "Literal";
	case AST_Ref: return "Ref";
	case AST_Load: return "Load";
	case AST_Convert: return "Convert";
	case AST_Unary: return "Unary";
	case AST_Binary: return "Binary";
	case AST_If: return "If";
	case AST_Invoke: return "Invoke";
	case AST_Store: return "Store";
	case AST_StoreVal: return "StoreVal";
	case AST_BasicBlock: return "BasicBlock";
	case AST_Jump: return "Jump";
	case AST_Return: return "Return";
	case AST_Poison: return "Poison";
	case AST_Bailout: return "Bailout";
	default: return "?";
	}
}

static const char *refuse_opname(int op) {
	switch (op) {
	case '-': return "neg";
	case TOK_NEG: return "neg-tok";
	case '~': return "bnot";
	case '!': return "lnot";
	case TOK_INC: return "inc";
	case TOK_DEC: return "dec";
	case AST_EVAL_OP_ADDR: return "addr";
	case AST_EVAL_OP_MEMBER: return "member";
	case 0x40002: return "member-arrow";
	case 0x40003: return "imag";
	case 0x40004: return "vla";
	case 0x40005: return "vla-restore";
	case 0x40006: return "mulhu";
	case 0x40007: return "mulhs";
	case 0x40008: return "fabs";
	case 0x40009: return "sqrt";
	case 0x4000A: return "opassign";
	case 0x4000B: return "floor";
	case 0x4000C: return "ceil";
	case 0x4000D: return "trunc";
	case 0x4000E: return "copysign";
	case 0x4000F: return "round";
	case 0x40010: return "fmin";
	case 0x40011: return "fmax";
	case 0x40012: return "rint";
	case 0x40013: return "nearbyint";
	case 0x40014: return "fma";
	case 0x40015: return "fneg";
	case 0x40016: return "bswap";
	case 0x40017: return "signbit";
	case 0x40018: return "ffs";
	case 0x40019: return "bitscan";
	case 0x4001a: return "asmgen";
	case 0x4001b: return "asm";
	case 0x4001c: return "axadd";
	case 0x4001d: return "axchg";
	case 0x4001e: return "acmpxchg";
	case 0x4001f: return "bitb";
	case 0x40020: return "acasrmw";
	case 0x40021: return "ggoto";
	case 0x40022: return "cplxbuild";
	case 0x40023: return "vaarg";
	case 0x40024: return "vastart";
	case 0x40025: return "asmops";
	case 0x50001: return "member:bitfield";
	case 0x50002: return "member:untyped";
	case 0x50003: return "member:array-type";
	case 0x50004: return "member:bad-type";
	case 0x50005: return "member:float-type";
	case 0x50006: return "member:nonint-type";
	case 0x50007: return "member:base-not-a-frame-slot";
	case TOK_LAND: return "land";
	case TOK_LOR: return "lor";
	case TOK_ULT: return "ult";
	case TOK_UGE: return "uge";
	case TOK_EQ: return "eq";
	case TOK_NE: return "ne";
	case TOK_ULE: return "ule";
	case TOK_UGT: return "ugt";
	case TOK_LT: return "lt";
	case TOK_GE: return "ge";
	case TOK_LE: return "le";
	case TOK_GT: return "gt";
	default: return NULL;
	}
}

#define REF_DET_CAP 4096

static long g_det_cause[REF_DET_CAP];
static long g_det_key[REF_DET_CAP];
static long g_det_n[REF_DET_CAP];
static int g_det_used;

static void refuse_det(int cause, long key) {
	int i;
	for (i = 0; i < g_det_used; i++)
		if (g_det_cause[i] == cause && g_det_key[i] == key) {
			g_det_n[i]++;
			return;
		}
	if (g_det_used >= REF_DET_CAP)
		return;
	g_det_cause[g_det_used] = cause;
	g_det_key[g_det_used] = key;
	g_det_n[g_det_used] = 1;
	g_det_used++;
}

static int g_ref_pos_cur;

#define REF_DET_ARITY(k, nc, op) \
	(((long)(k) << 40) | ((long)(nc) << 32) | (long)(op))
#define REF_DET_WT(k, sub, op) \
	(((long)(k) << 40) | ((long)(sub) << 32) | (long)(op))

static int refuse_wt_sub(AstArena *a, AstLocal n) {
	int t;
	if (n == AST_NONE)
		return 5;
	t = ast_type_t(a, n);
	if (!t)
		return 0;
	if (ast_bad_type(t))
		return 2;
	if (is_float(t))
		return 1;
	if (!ast_eval_slice_intt(t))
		return 3;
	return 4;
}

static const char *refuse_wt_subname(int sub) {
	switch (sub) {
	case 0: return "untyped";
	case 1: return "float";
	case 2: return "bad-type";
	case 3: return "nonint";
	case 4: return "int-but-refused";
	default: return "no-child";
	}
}

static void refuse_det_wt(int cause, AstArena *a, AstLocal c) {
	int k = c == AST_NONE ? 0 : ast_kind(a, c);
	int sub = refuse_wt_sub(a, c);
	int op = (c != AST_NONE && (k == AST_Unary || k == AST_Binary))
							 ? ast_op(a, c)
							 : 0;
	refuse_det(cause, REF_DET_WT(k, sub, op));
}

static void refuse_det_label(int cause, long key, char *buf, size_t cap) {
	if (cause == REF_OP_UNARY || cause == REF_OP_TERNARY) {
		int op = (int)(key & 0xFFFFFFFF);
		int pos = (int)(key >> 40);
		const char *s = refuse_opname(op);
		char nm[64];
		if (s)
			snprintf(nm, sizeof nm, "%s (0x%x)", s, op);
		else if (op >= 32 && op < 127)
			snprintf(nm, sizeof nm, "'%c' (0x%x)", (char)op, op);
		else
			snprintf(nm, sizeof nm, "op 0x%x", op);
		snprintf(buf, cap, "%-22s in %s", nm, refuse_posname(pos));
		return;
	}
	if (cause == REF_ARITY) {
		int k = (int)(key >> 40);
		int nc = (int)((key >> 32) & 0xFF);
		int op = (int)(key & 0xFFFFFFFF);
		const char *s = op ? refuse_opname(op) : NULL;
		if (op)
			snprintf(buf, cap, "%s nchild=%d op=%s(0x%x)", refuse_kindname(k), nc,
							 s ? s : "?", op);
		else
			snprintf(buf, cap, "%s nchild=%d", refuse_kindname(k), nc);
		return;
	}
	if (cause == REF_NOWTYPE) {
		int k = (int)(key >> 40);
		int sub = (int)((key >> 32) & 0xFF);
		int op = (int)(key & 0xFFFFFFFF);
		const char *s = op ? refuse_opname(op) : NULL;
		if (op)
			snprintf(buf, cap, "child=%s/%s op=%s(0x%x)", refuse_kindname(k),
							 refuse_wt_subname(sub), s ? s : "?", op);
		else
			snprintf(buf, cap, "child=%s/%s", refuse_kindname(k),
							 refuse_wt_subname(sub));
		return;
	}
	snprintf(buf, cap, "key=%ld", key);
}

static signed char *g_invthr;
static long g_invthrcap;
static long g_thr_nodes[MCC_THREAD_N];
static long g_thr_blocks[MCC_THREAD_N];
static long g_ref_invoke_all;
static long g_bc_invoke_all;
static AstLocal g_bc_at;

static int inv_thread_class(long node) {
	if (!g_invthr || node < 0 || node >= g_invthrcap)
		return MCC_THREAD_NONE;
	return g_invthr[node];
}

static int refuse_local(AstArena *a, AstLocal n) {
	int t = ast_type_t(a, n), r;
	switch (ast_kind(a, n)) {
	case AST_Literal:
		r = refuse_type(t);
		if (r)
			return r;
		if ((ast_op(a, n) & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
			return REF_LIT_NONCONST;
		return REF_OK;
	case AST_Ref: {
		int rop = ast_op(a, n);
		if ((rop & VT_VALMASK) == VT_LOCAL && !(rop & VT_SYM)) {
			if (is_float(t))
				return REF_TYPE_FLOAT;
			if (!ast_eval_slice_intt(t))
				return REF_TYPE_NONINT;
			return REF_OK;
		}
		if ((rop & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST)
			return refuse_type(t);
		return REF_REF_GLOBAL;
	}
	case AST_Load:
		return REF_LOAD;
	case AST_Convert: {
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE) {
			refuse_det(REF_ARITY, REF_DET_ARITY(AST_Convert, 0, 0));
			return REF_ARITY;
		}
		if (is_float(t) || is_float(ast_type_t(a, c)))
			return REF_TYPE_FLOAT;
		if (ast_bad_type(t))
			return REF_TYPE_BAD;
		if (!ast_eval_slice_intt(t))
			return REF_TYPE_NONINT;
		return REF_OK;
	}
	case AST_Unary: {
		int uop = ast_op(a, n);
		if (ast_first_child(a, n) == AST_NONE) {
			refuse_det(REF_ARITY, REF_DET_ARITY(AST_Unary, 0, ast_op(a, n)));
			return REF_ARITY;
		}
		if (uop != '-' && uop != TOK_NEG && uop != '~' && uop != '!') {
			int key = uop;
			if (uop == AST_EVAL_OP_MEMBER) {
				int32_t mo;
				int mt = ast_type_t(a, n);
				if (ast_type_bp(a, n) || ast_type_bs(a, n))
					key = 0x50001;
				else if (!mt)
					key = 0x50002;
				else if (mt & VT_ARRAY)
					key = 0x50003;
				else if (ast_bad_type(mt))
					key = 0x50004;
				else if (is_float(mt))
					key = 0x50005;
				else if (!ast_eval_slice_intt(mt))
					key = 0x50006;
				else if (!ast_eval_slice_frame_off(a, n, &mo, 0))
					key = 0x50007;
			}
			refuse_det(REF_OP_UNARY, ((long)g_ref_pos_cur << 40) | (long)key);
			return REF_OP_UNARY;
		}
		if (!ast_eval_slice_wtype(a, n)) {
			refuse_det_wt(REF_NOWTYPE, a, ast_first_child(a, n));
			return REF_NOWTYPE;
		}
		return REF_OK;
	}
	case AST_Binary: {
		int bop = ast_op(a, n);
		AstLocal x, y;
		if (bop == TOK_LAND || bop == TOK_LOR)
			return REF_OK;
		if (!refuse_binop_known(bop))
			return REF_OP_BINARY;
		if (ast_nchild(a, n) != 2) {
			refuse_det(REF_ARITY,
								 REF_DET_ARITY(AST_Binary, ast_nchild(a, n), ast_op(a, n)));
			return REF_ARITY;
		}
		x = ast_child(a, n, 0);
		y = ast_child(a, n, 1);
		if (is_float(ast_type_t(a, x)) || is_float(ast_type_t(a, y)))
			return REF_TYPE_FLOAT;
		if (!ast_eval_slice_wtype(a, x)) {
			refuse_det_wt(REF_NOWTYPE, a, x);
			return REF_NOWTYPE;
		}
		return REF_OK;
	}
	case AST_If:
		if (ast_nchild(a, n) != 3) {
			refuse_det(REF_ARITY,
								 REF_DET_ARITY(AST_If, ast_nchild(a, n), ast_op(a, n)));
			return REF_ARITY;
		}
		if (ast_op(a, n) != 5 && ast_op(a, n) != 7) {
			refuse_det(REF_OP_TERNARY,
								 ((long)g_ref_pos_cur << 40) | (long)ast_op(a, n));
			return REF_OP_TERNARY;
		}
		return REF_OK;
	case AST_Invoke:
		return inv_thread_class((long)n) ? REF_KIND_INVOKE_THREAD : REF_KIND_INVOKE;
	case AST_Store:
		return REF_KIND_STORE;
	case AST_StoreVal:
		return REF_KIND_STOREVAL;
	case AST_BasicBlock:
		return REF_KIND_BLOCK;
	case AST_Jump:
		return REF_KIND_JUMP;
	case AST_Return:
		return REF_KIND_RETURN;
	case AST_Poison:
		return REF_KIND_POISON;
	default:
		return REF_KIND_OTHER;
	}
}

enum {
	RG_FUNC = 0,
	RG_AGG,
	RG_ARRAY,
	RG_SCALAR_INT,
	RG_SCALAR_FLOAT,
	RG_SCALAR_OTHER,
	RG_ADDR,
	RG_LLOCAL,
	RG_REG,
	RG_OTHER,
	RG_N
};

static const char *refuse_global_name(int g) {
	switch (g) {
	case RG_FUNC: return "func-symbol";
	case RG_AGG: return "global-aggregate";
	case RG_ARRAY: return "global-array";
	case RG_SCALAR_INT: return "global-scalar-int";
	case RG_SCALAR_FLOAT: return "global-scalar-float";
	case RG_SCALAR_OTHER: return "global-scalar-other";
	case RG_ADDR: return "global-address";
	case RG_LLOCAL: return "indirect-local";
	case RG_REG: return "register-temp";
	default: return "other";
	}
}

static int refuse_global_class(AstArena *a, AstLocal n) {
	int rop = ast_op(a, n), t = ast_type_t(a, n), vm = rop & VT_VALMASK, bt;
	if (vm == VT_LLOCAL)
		return RG_LLOCAL;
	if (vm != VT_CONST && vm != VT_LOCAL)
		return RG_REG;
	if (!(rop & VT_SYM))
		return RG_OTHER;
	bt = t & VT_BTYPE;
	if (bt == VT_FUNC)
		return RG_FUNC;
	if (t & VT_ARRAY)
		return RG_ARRAY;
	if (bt == VT_STRUCT)
		return RG_AGG;
	if (!(rop & VT_LVAL))
		return RG_ADDR;
	if (!t || bt == VT_VOID)
		return RG_OTHER;
	if (is_float(t))
		return RG_SCALAR_FLOAT;
	if (ast_eval_slice_intt(t))
		return RG_SCALAR_INT;
	return RG_SCALAR_OTHER;
}

static int refuse_parent_blocked(AstArena *a, AstLocal n) {
	AstLocal p = ast_parent(a, n);
	if (p == AST_NONE)
		return 1;
	if (ast_eval_slice_kind_ok(a, p, 0))
		return 0;
	return refuse_local(a, p) != REF_OK;
}

enum {
	RA_LOCAL_LVAL = 0,
	RA_LOCAL_SCALAR,
	RA_LOCAL_ARRAY,
	RA_GLOBAL_SCALAR,
	RA_CONST,
	RA_N
};

static const char *refuse_accept_name(int k) {
	switch (k) {
	case RA_LOCAL_LVAL: return "local-lvalue";
	case RA_LOCAL_SCALAR: return "local-address-scalar";
	case RA_LOCAL_ARRAY: return "local-address-array";
	case RA_GLOBAL_SCALAR: return "global-scalar-int";
	default: return "constant";
	}
}

static long g_ref_a_nodes[RA_N];
static long g_ref_a_value[RA_N];

static int refuse_base_consumed(AstArena *a, AstLocal n) {
	AstLocal p = ast_parent(a, n);
	AstEvalSliceIdx ix;
	int32_t fo;
	if (p == AST_NONE)
		return 0;
	if (ast_eval_slice_dynidx(a, p, &ix) && ix.idx != n)
		return 1;
	if (ast_kind(a, p) == AST_Unary &&
			(ast_op(a, p) == AST_EVAL_OP_MEMBER || ast_op(a, p) == AST_EVAL_OP_ADDR) &&
			ast_eval_slice_frame_off(a, p, &fo, 0))
		return 1;
	return 0;
}

static void refuse_accepted_ref(AstArena *a, AstLocal n) {
	int rop, t, k;
	int32_t go;
	if (ast_kind(a, n) != AST_Ref)
		return;
	rop = ast_op(a, n);
	t = ast_type_t(a, n);
	if ((rop & VT_VALMASK) == VT_LOCAL && !(rop & VT_SYM))
		k = (rop & VT_LVAL) ? RA_LOCAL_LVAL
											: ((t & VT_ARRAY) ? RA_LOCAL_ARRAY : RA_LOCAL_SCALAR);
	else if ((rop & VT_SYM) && ast_eval_slice_globl(a, n, &go))
		k = RA_GLOBAL_SCALAR;
	else
		k = RA_CONST;
	g_ref_a_nodes[k]++;
	if (k != RA_LOCAL_LVAL && k != RA_CONST && !refuse_base_consumed(a, n))
		g_ref_a_value[k]++;
}

static long g_ref_g_nodes[RG_N];
static long g_ref_g_free[RG_N];
static long g_ref_g_callee[RG_N];
static long g_ref_nodes[REF_N];
static long g_ref_bodies[REF_N];
static long g_ref_bodynodes[REF_N];
static long g_ref_total_nodes, g_ref_total_bodies, g_ref_total_bodynodes;
static long g_ref_accepted_nodes, g_ref_blocks, g_ref_blocks_acc;
static unsigned char g_ref_hit[REF_N];

enum {
	MSL_FRAME_F64,
	MSL_FRAME_INT,
	MSL_DEREF,
	MSL_DYNIDX,
	MSL_DYNIDX_IDX,
	MSL_NOCHILD,
	MSL_TYPE_FLOAT,
	MSL_TYPE_BAD,
	MSL_TYPE_NONINT,
	MSL_PTR_NOTREF,
	MSL_PTR_NOTLOCAL,
	MSL_PTR_NOTPTR,
	MSL_PTR_NOOBJ,
	MSL_PTR_ETYPE,
	MSL_IDX_NOTARRAY,
	MSL_IDX_NOOBJ,
	MSL_IDX_ETYPE,
	MSL_IDX_EXTENT,
	MSL_IDX_WIDE,
	MSL_ADDR_GLOBAL,
	MSL_ADDR_OTHER,
	MSL_N
};

static const char *memshape_load_name(int k) {
	switch (k) {
	case MSL_FRAME_F64: return "load/frame-f64";
	case MSL_FRAME_INT: return "load/frame-int";
	case MSL_DEREF: return "load/deref";
	case MSL_DYNIDX: return "load/dynidx";
	case MSL_DYNIDX_IDX: return "load/dynidx-index-refused";
	case MSL_NOCHILD: return "load/no-child";
	case MSL_TYPE_FLOAT: return "load/type-float";
	case MSL_TYPE_BAD: return "load/type-bad";
	case MSL_TYPE_NONINT: return "load/type-nonint";
	case MSL_PTR_NOTREF: return "load/ptr-base-not-ref";
	case MSL_PTR_NOTLOCAL: return "load/ptr-base-not-local";
	case MSL_PTR_NOTPTR: return "load/ptr-base-not-ptr";
	case MSL_PTR_NOOBJ: return "load/ptr-pointee-unknown";
	case MSL_PTR_ETYPE: return "load/ptr-pointee-type";
	case MSL_IDX_NOTARRAY: return "load/idx-base-not-array";
	case MSL_IDX_NOOBJ: return "load/idx-object-unknown";
	case MSL_IDX_ETYPE: return "load/idx-elem-type";
	case MSL_IDX_EXTENT: return "load/idx-extent";
	case MSL_IDX_WIDE: return "load/idx-64bit";
	case MSL_ADDR_GLOBAL: return "load/addr-global";
	case MSL_ADDR_OTHER: return "load/addr-other";
	default: return "?";
	}
}

enum {
	MSS_LOCAL,
	MSS_DEREF,
	MSS_DYNIDX,
	MSS_DYNIDX_IDX,
	MSS_ARITY,
	MSS_DEST_GLOBAL,
	MSS_DEST_LOAD,
	MSS_DEST_OTHER,
	MSS_TYPE,
	MSS_N
};

static const char *memshape_store_name(int k) {
	switch (k) {
	case MSS_LOCAL: return "store/local-slot";
	case MSS_DEREF: return "store/deref";
	case MSS_DYNIDX: return "store/dynidx";
	case MSS_DYNIDX_IDX: return "store/dynidx-index-refused";
	case MSS_ARITY: return "store/arity";
	case MSS_DEST_GLOBAL: return "store/dest-global";
	case MSS_DEST_LOAD: return "store/dest-load-other";
	case MSS_DEST_OTHER: return "store/dest-other";
	case MSS_TYPE: return "store/dest-type";
	default: return "?";
	}
}

enum {
	MSD_L_PTR_LOCAL,
	MSD_L_PTR_GLOBAL,
	MSD_L_ARR_GLOBAL,
	MSD_L_BASE_LOAD,
	MSD_L_BASE_CONV,
	MSD_L_BASE_OTHER,
	MSD_S_FRAMEOFF,
	MSD_S_MEMBER_NOFRAME,
	MSD_S_MEMBER_GLOBAL,
	MSD_S_CONV,
	MSD_S_OTHER,
	MSD_SL_DYN_PTR,
	MSD_SL_DYN_GLOBAL,
	MSD_SL_FRAMEOFF,
	MSD_SL_OTHER,
	MSD_N
};

static const char *memshape_detail_name(int k) {
	switch (k) {
	case MSD_L_PTR_LOCAL: return "load/idx-base=local-pointer";
	case MSD_L_PTR_GLOBAL: return "load/idx-base=global-pointer";
	case MSD_L_ARR_GLOBAL: return "load/idx-base=global-array";
	case MSD_L_BASE_LOAD: return "load/idx-base=load";
	case MSD_L_BASE_CONV: return "load/idx-base=convert";
	case MSD_L_BASE_OTHER: return "load/idx-base=other";
	case MSD_S_FRAMEOFF: return "store/dest-other=frame-off";
	case MSD_S_MEMBER_NOFRAME: return "store/dest-other=local-base-nonframe";
	case MSD_S_MEMBER_GLOBAL: return "store/dest-other=global-base";
	case MSD_S_CONV: return "store/dest-other=convert";
	case MSD_S_OTHER: return "store/dest-other=other";
	case MSD_SL_DYN_PTR: return "store/dest-load=ptr-index";
	case MSD_SL_DYN_GLOBAL: return "store/dest-load=global-index";
	case MSD_SL_FRAMEOFF: return "store/dest-load=frame-off";
	case MSD_SL_OTHER: return "store/dest-load=other";
	default: return "?";
	}
}

static long g_msd[MSD_N];
static long g_msd_w[MSD_N][9];
static long g_msd_kx[32], g_msd_ky[32];

static long g_msl[MSL_N];
static long g_msl_w[MSL_N][9];
static long g_mss[MSS_N];
static long g_mss_w[MSS_N][9];
static long g_mss_valbad[MSS_N];
static long g_msl_total, g_mss_total;

static int memshape_width_bucket(int t) {
	int w = ast_eval_slice_tsize(t);
	if (w < 0 || w > 8)
		return 0;
	return w;
}

static int memshape_ptr_why(AstArena *a, AstLocal c) {
	int32_t extent = 0;
	int et = 0, r, t;
	if (c == AST_NONE || ast_kind(a, c) != AST_Ref)
		return MSL_PTR_NOTREF;
	r = ast_op(a, c);
	if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
		return MSL_PTR_NOTLOCAL;
	t = ast_type_t(a, c);
	if (!t || ast_bad_type(t) || (t & VT_ARRAY) || (t & VT_BTYPE) != VT_PTR)
		return MSL_PTR_NOTPTR;
	if (!ast_eval_slice_obj_fn(a, c, &extent, &et))
		return MSL_PTR_NOOBJ;
	if (!et || (et & VT_ARRAY) || is_float(et) || !ast_eval_slice_intt(et) ||
			!ast_eval_slice_tsize(et))
		return MSL_PTR_ETYPE;
	return MSL_PTR_ETYPE;
}

static int memshape_idx_why(AstArena *a, AstLocal n, int *pw) {
	AstLocal x, y, base, idx;
	int32_t extent = 0, bo = 0;
	int etype = 0, it, esize;
	*pw = 0;
	if (n == AST_NONE || ast_kind(a, n) != AST_Binary || ast_op(a, n) != '+' ||
			ast_nchild(a, n) != 2)
		return MSL_ADDR_OTHER;
	x = ast_child(a, n, 0);
	y = ast_child(a, n, 1);
	if (ast_eval_slice_idx_base(a, x, &bo)) {
		base = x;
		idx = y;
	} else if (ast_eval_slice_idx_base(a, y, &bo)) {
		base = y;
		idx = x;
	} else {
		return MSL_IDX_NOTARRAY;
	}
	if (!ast_eval_slice_obj_fn(a, base, &extent, &etype))
		return MSL_IDX_NOOBJ;
	esize = ast_eval_slice_tsize(etype);
	*pw = esize < 0 || esize > 8 ? 0 : esize;
	if (!esize || (etype & VT_ARRAY) ||
			(!ast_eval_slice_f64t(etype) &&
			 (is_float(etype) || !ast_eval_slice_intt(etype))))
		return MSL_IDX_ETYPE;
	if (extent < esize || extent % esize)
		return MSL_IDX_EXTENT;
	it = ast_eval_slice_wtype(a, idx);
	if (!it || is_float(it) || !ast_eval_slice_intt(it))
		return MSL_IDX_ETYPE;
	if (ast_eval_slice_is64(it))
		return MSL_IDX_WIDE;
	return MSL_IDX_EXTENT;
}

static void memshape_bump(long *tab, long (*wtab)[9], int k, int w) {
	tab[k]++;
	if (w >= 0 && w <= 8)
		wtab[k][w]++;
}

static void memshape_load_detail(AstArena *a, AstLocal c) {
	AstLocal x, y, base;
	int bt;
	x = ast_child(a, c, 0);
	y = ast_child(a, c, 1);
	base = x;
	if (ast_kind(a, x) == AST_Literal ||
			(ast_kind(a, y) != AST_Literal &&
			 (ast_type_t(a, y) & VT_BTYPE) == VT_PTR &&
			 (ast_type_t(a, x) & VT_BTYPE) != VT_PTR))
		base = y;
	bt = ast_type_t(a, base);
	if (ast_kind(a, base) == AST_Ref) {
		int r = ast_op(a, base);
		if (bt & VT_ARRAY)
			g_msd[MSD_L_ARR_GLOBAL]++;
		else if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM))
			g_msd[MSD_L_PTR_LOCAL]++;
		else
			g_msd[MSD_L_PTR_GLOBAL]++;
		if ((bt & VT_BTYPE) == VT_PTR || (bt & VT_ARRAY)) {
			int w = 0;
			int32_t ext = 0;
			int et = 0;
			if (ast_eval_slice_obj_fn && ast_eval_slice_obj_fn(a, base, &ext, &et))
				w = memshape_width_bucket(et);
			g_msd_w[ast_kind(a, base) == AST_Ref && (bt & VT_ARRAY)
									? MSD_L_ARR_GLOBAL
									: ((ast_op(a, base) & VT_VALMASK) == VT_LOCAL &&
														 !(ast_op(a, base) & VT_SYM)
												 ? MSD_L_PTR_LOCAL
												 : MSD_L_PTR_GLOBAL)][w]++;
		}
		return;
	}
	if (ast_kind(a, base) == AST_Load)
		g_msd[MSD_L_BASE_LOAD]++;
	else if (ast_kind(a, base) == AST_Convert)
		g_msd[MSD_L_BASE_CONV]++;
	else
		g_msd[MSD_L_BASE_OTHER]++;
	if (ast_kind(a, x) == AST_Unary) {
		int uop = ast_op(a, x);
		int slot = uop == 0x40000 ? 0 : uop == 0x40001 ? 1 : uop == 0x40002 ? 2 : 3;
		AstLocal ub = ast_first_child(a, x);
		int32_t fo;
		g_msd_kx[slot]++;
		if (ast_eval_slice_frame_off(a, x, &fo, 0))
			g_msd_ky[slot]++;
		if (ub != AST_NONE && ast_kind(a, ub) == AST_Ref &&
				(ast_type_t(a, ub) & VT_ARRAY))
			g_msd_ky[8 + slot]++;
		if (ub != AST_NONE && ast_kind(a, ub) == AST_Ref &&
				(ast_op(a, ub) & VT_SYM))
			g_msd_ky[16 + slot]++;
	}
}

static void memshape_store_detail(AstArena *a, AstLocal d) {
	int32_t fo;
	int ty;
	if (mcc_slice_store_frame(a, d, &fo, &ty)) {
		memshape_bump(g_msd, g_msd_w, MSD_S_FRAMEOFF, memshape_width_bucket(ty));
		return;
	}
	{
		AstLocal b = d;
		int guard = 0;
		while (b != AST_NONE && guard++ < 8 &&
					 (ast_kind(a, b) == AST_Unary || ast_kind(a, b) == AST_Load ||
						ast_kind(a, b) == AST_Convert))
			b = ast_first_child(a, b);
		if (b != AST_NONE && ast_kind(a, b) == AST_Ref) {
			if (ast_op(a, b) & VT_SYM)
				g_msd[MSD_S_MEMBER_GLOBAL]++;
			else
				g_msd[MSD_S_MEMBER_NOFRAME]++;
			return;
		}
	}
	if (ast_kind(a, d) == AST_Convert)
		g_msd[MSD_S_CONV]++;
	else
		g_msd[MSD_S_OTHER]++;
}

static void memshape_storeload_detail(AstArena *a, AstLocal d) {
	AstLocal c = ast_first_child(a, d);
	int32_t fo;
	int et;
	if (mcc_slice_store_frame(a, d, &fo, &et)) {
		memshape_bump(g_msd, g_msd_w, MSD_SL_FRAMEOFF, memshape_width_bucket(et));
		return;
	}
	if (c != AST_NONE && ast_kind(a, c) == AST_Binary && ast_op(a, c) == '+' &&
			ast_nchild(a, c) == 2) {
		AstLocal x = ast_child(a, c, 0), y = ast_child(a, c, 1), b = x;
		int r;
		if ((ast_type_t(a, y) & (VT_ARRAY | VT_BTYPE)) &&
				ast_kind(a, y) == AST_Ref && ast_kind(a, x) != AST_Ref)
			b = y;
		r = ast_kind(a, b) == AST_Ref ? ast_op(a, b) : 0;
		if (ast_kind(a, b) == AST_Ref && (r & VT_SYM))
			g_msd[MSD_SL_DYN_GLOBAL]++;
		else
			g_msd[MSD_SL_DYN_PTR]++;
		return;
	}
	g_msd[MSD_SL_OTHER]++;
}

static void memshape_load(AstArena *a, AstLocal n) {
	AstLocal c = ast_first_child(a, n);
	int t = ast_type_t(a, n);
	AstEvalSliceIdx ix;
	int32_t fo;
	int et, w;

	g_msl_total++;
	if (c == AST_NONE) {
		memshape_bump(g_msl, g_msl_w, MSL_NOCHILD, 0);
		return;
	}
	if (!ast_bad_type(t) && ast_eval_slice_f64t(t) &&
			ast_eval_slice_frame_off(a, c, &fo, 0)) {
		memshape_bump(g_msl, g_msl_w, MSL_FRAME_F64, 8);
		return;
	}
	if (!ast_bad_type(t) && !is_float(t) && ast_eval_slice_intt(t) &&
			ast_eval_slice_frame_off(a, c, &fo, 0)) {
		memshape_bump(g_msl, g_msl_w, MSL_FRAME_INT, memshape_width_bucket(t));
		return;
	}
	if (ast_eval_slice_deref(a, n, &fo, &et)) {
		memshape_bump(g_msl, g_msl_w, MSL_DEREF, memshape_width_bucket(et));
		return;
	}
	if (ast_eval_slice_dynidx(a, c, &ix)) {
		w = ix.esize < 0 || ix.esize > 8 ? 0 : ix.esize;
		if (ast_eval_slice_kind_ok(a, ix.idx, 1))
			memshape_bump(g_msl, g_msl_w, MSL_DYNIDX, w);
		else
			memshape_bump(g_msl, g_msl_w, MSL_DYNIDX_IDX, w);
		return;
	}
	if (t && !ast_bad_type(t) && is_float(t)) {
		memshape_bump(g_msl, g_msl_w, MSL_TYPE_FLOAT, memshape_width_bucket(t));
		return;
	}
	if (t && ast_bad_type(t)) {
		memshape_bump(g_msl, g_msl_w, MSL_TYPE_BAD, 0);
		return;
	}
	if (t && !ast_eval_slice_intt(t)) {
		memshape_bump(g_msl, g_msl_w, MSL_TYPE_NONINT, memshape_width_bucket(t));
		return;
	}
	if (!t) {
		int why;
		w = 0;
		why = memshape_idx_why(a, c, &w);
		memshape_bump(g_msl, g_msl_w, why, w);
		if (why == MSL_IDX_NOTARRAY)
			memshape_load_detail(a, c);
		return;
	}
	if (ast_kind(a, c) == AST_Ref && (ast_op(a, c) & VT_SYM)) {
		memshape_bump(g_msl, g_msl_w, MSL_ADDR_GLOBAL, memshape_width_bucket(t));
		return;
	}
	if ((ast_type_t(a, c) & VT_BTYPE) == VT_PTR || ast_kind(a, c) == AST_Ref) {
		memshape_bump(g_msl, g_msl_w, memshape_ptr_why(a, c),
									memshape_width_bucket(t));
		return;
	}
	memshape_bump(g_msl, g_msl_w, MSL_ADDR_OTHER, memshape_width_bucket(t));
}

static void memshape_store(AstArena *a, AstLocal n) {
	AstLocal d, v;
	AstEvalSliceIdx ix;
	int32_t off, pf;
	int dt, et, k, w;

	g_mss_total++;
	if (ast_nchild(a, n) != 2) {
		memshape_bump(g_mss, g_mss_w, MSS_ARITY, 0);
		return;
	}
	d = ast_child(a, n, 0);
	v = ast_child(a, n, 1);
	if (mcc_slice_store_dyn(a, d, &ix)) {
		w = ix.esize < 0 || ix.esize > 8 ? 0 : ix.esize;
		k = ast_eval_slice_kind_ok(a, ix.idx, 1) ? MSS_DYNIDX : MSS_DYNIDX_IDX;
		memshape_bump(g_mss, g_mss_w, k, w);
		if (!ast_eval_slice_kind_ok(a, v, 1))
			g_mss_valbad[k]++;
		return;
	}
	if (ast_eval_slice_deref(a, d, &pf, &et)) {
		memshape_bump(g_mss, g_mss_w, MSS_DEREF, memshape_width_bucket(et));
		if (!ast_eval_slice_kind_ok(a, v, 1))
			g_mss_valbad[MSS_DEREF]++;
		return;
	}
	if (mcc_slice_is_local_ref(a, d, &off)) {
		dt = ast_type_t(a, d);
		if (!dt || ast_bad_type(dt) ||
				(!ast_eval_slice_f64t(dt) &&
				 (is_float(dt) || !ast_eval_slice_intt(dt)))) {
			memshape_bump(g_mss, g_mss_w, MSS_TYPE, memshape_width_bucket(dt));
			return;
		}
		memshape_bump(g_mss, g_mss_w, MSS_LOCAL, memshape_width_bucket(dt));
		if (!ast_eval_slice_kind_ok(a, v, 1))
			g_mss_valbad[MSS_LOCAL]++;
		return;
	}
	if (ast_kind(a, d) == AST_Ref && (ast_op(a, d) & VT_SYM)) {
		memshape_bump(g_mss, g_mss_w, MSS_DEST_GLOBAL,
									memshape_width_bucket(ast_type_t(a, d)));
		return;
	}
	if (ast_kind(a, d) == AST_Load) {
		memshape_bump(g_mss, g_mss_w, MSS_DEST_LOAD,
									memshape_width_bucket(ast_type_t(a, d)));
		memshape_storeload_detail(a, d);
		return;
	}
	memshape_bump(g_mss, g_mss_w, MSS_DEST_OTHER,
								memshape_width_bucket(ast_type_t(a, d)));
	memshape_store_detail(a, d);
}

static void memshape_report(void) {
	int i, w;
	printf("memshape: loads=%ld stores=%ld\n", g_msl_total, g_mss_total);
	for (i = 0; i < MSL_N; i++) {
		if (!g_msl[i])
			continue;
		printf("memshape: %-28s n=%ld share=%.2f%% widths=", memshape_load_name(i),
					 g_msl[i], g_msl_total ? 100.0 * g_msl[i] / g_msl_total : 0.0);
		for (w = 0; w <= 8; w++)
			if (g_msl_w[i][w])
				printf("%d:%ld ", w, g_msl_w[i][w]);
		printf("\n");
	}
	for (i = 0; i < MSS_N; i++) {
		if (!g_mss[i])
			continue;
		printf("memshape: %-28s n=%ld share=%.2f%% val-refused=%ld widths=",
					 memshape_store_name(i), g_mss[i],
					 g_mss_total ? 100.0 * g_mss[i] / g_mss_total : 0.0,
					 g_mss_valbad[i]);
		for (w = 0; w <= 8; w++)
			if (g_mss_w[i][w])
				printf("%d:%ld ", w, g_mss_w[i][w]);
		printf("\n");
	}
	for (i = 0; i < MSD_N; i++) {
		if (!g_msd[i])
			continue;
		printf("memshape: %-28s n=%ld widths=", memshape_detail_name(i), g_msd[i]);
		for (w = 0; w <= 8; w++)
			if (g_msd_w[i][w])
				printf("%d:%ld ", w, g_msd_w[i][w]);
		printf("\n");
	}
	for (i = 0; i < 32; i++)
		if (g_msd_kx[i] || g_msd_ky[i])
			printf("memshape: idx-operand-kind %d lhs=%ld rhs=%ld\n", i, g_msd_kx[i],
						 g_msd_ky[i]);
}

enum {
	BC_OK,
	BC_EMPTY,
	BC_CAPACITY,
	BC_INVOKE,
	BC_INVOKE_THREAD,
	BC_STOREVAL,
	BC_JUMP,
	BC_POISON,
	BC_RET_MID,
	BC_RET_NOVAL,
	BC_RET_TYPE,
	BC_RET_EXPR,
	BC_RET_EARLY_MID,
	BC_RET_EARLY_LOOP,
	BC_RET_EARLY_NOVAL,
	BC_RET_EARLY_EXPR,
	BC_SWITCH,
	BC_IF_OTHER,
	BC_CTRL_COND,
	BC_CTRL_BODY,
	BC_INCDEC,
	BC_ST_ARITY,
	BC_ST_DEST_GLOBAL,
	BC_ST_DEST_OTHER,
	BC_ST_DEREF_SUBWORD,
	BC_ST_DEREF_TYPE,
	BC_ST_IDX,
	BC_ST_TYPE,
	BC_ST_VALUE,
	BC_EXPR,
	BC_OTHER,
	BC_NOCAUSE,
	BC_N
};

static const char *block_cause_name(int k) {
	switch (k) {
	case BC_EMPTY: return "block/empty";
	case BC_CAPACITY: return "block/depth-limit";
	case BC_INVOKE: return "block/stmt-invoke";
	case BC_INVOKE_THREAD: return "block/stmt-thread";
	case BC_STOREVAL: return "block/stmt-storeval";
	case BC_JUMP: return "block/stmt-jump";
	case BC_POISON: return "block/stmt-poison";
	case BC_RET_MID: return "block/return-not-last";
	case BC_RET_NOVAL: return "block/return-no-value";
	case BC_RET_TYPE: return "block/return-type";
	case BC_RET_EXPR: return "block/return-expr";
	case BC_RET_EARLY_MID: return "block/early-return-not-last";
	case BC_RET_EARLY_LOOP: return "block/early-return-in-loop";
	case BC_RET_EARLY_NOVAL: return "block/early-return-no-value";
	case BC_RET_EARLY_EXPR: return "block/early-return-expr";
	case BC_SWITCH: return "block/switch";
	case BC_IF_OTHER: return "block/if-other-op";
	case BC_CTRL_COND: return "block/ctrl-cond";
	case BC_CTRL_BODY: return "block/ctrl-body";
	case BC_INCDEC: return "block/incdec";
	case BC_ST_ARITY: return "block/store-arity";
	case BC_ST_DEST_GLOBAL: return "block/store-dest-global";
	case BC_ST_DEST_OTHER: return "block/store-dest-other";
	case BC_ST_DEREF_SUBWORD: return "block/store-deref-subword";
	case BC_ST_DEREF_TYPE: return "block/store-deref-type";
	case BC_ST_IDX: return "block/store-index-refused";
	case BC_ST_TYPE: return "block/store-dest-type";
	case BC_ST_VALUE: return "block/store-value-refused";
	case BC_EXPR: return "block/expr-refused";
	case BC_OTHER: return "block/other";
	case BC_NOCAUSE: return "block/no-cause";
	default: return "?";
	}
}

static long g_bc[BC_N];
static long g_bc_blocks;

#define COND_CAUSE_CAP 512

static long g_cc_key[COND_CAUSE_CAP];
static long g_cc_n[COND_CAUSE_CAP];
static int g_cc_used;
static long g_cc_blocked;

static long g_cc_dropped;

static void cause_tab_add(long *key, long *n, int *used, int cap, long k) {
	int i;
	for (i = 0; i < *used; i++)
		if (key[i] == k) {
			n[i]++;
			return;
		}
	if (*used >= cap) {
		g_cc_dropped++;
		return;
	}
	key[*used] = k;
	n[*used] = 1;
	(*used)++;
}

static void cause_tab_report(const char *tag, long *key, long *n, int used,
														 long tot,
														 void (*label)(long, char *, size_t)) {
	for (;;) {
		long best = 0;
		int bi = -1, i;
		char lbl[96];
		for (i = 0; i < used; i++)
			if (n[i] > best) {
				best = n[i];
				bi = i;
			}
		if (bi < 0)
			break;
		label(key[bi], lbl, sizeof lbl);
		printf("%s: %-52s n=%ld share=%.2f%%\n", tag, lbl, n[bi],
					 tot ? 100.0 * (double)n[bi] / (double)tot : 0.0);
		n[bi] = 0;
	}
}

static void cond_cause_add(long key) {
	cause_tab_add(g_cc_key, g_cc_n, &g_cc_used, COND_CAUSE_CAP, key);
}

static int cond_load_sub(AstArena *a, AstLocal n) {
	AstLocal c = ast_first_child(a, n);
	AstEvalSliceIdx ix;
	int32_t fo;
	int et, t = ast_type_t(a, n);
	if (c == AST_NONE)
		return 0;
	if (ast_eval_slice_deref(a, n, &fo, &et) ||
			(ast_eval_slice_dynidx(a, c, &ix) &&
			 ast_eval_slice_kind_ok(a, ix.idx, 1)))
		return 1;
	if (!t)
		return 2;
	if (ast_bad_type(t))
		return 3;
	if (is_float(t) && !ast_eval_slice_f64t(t))
		return 4;
	if (!ast_eval_slice_intt(t) && !ast_eval_slice_f64t(t))
		return 5;
	return 6;
}

static const char *cond_load_subname(int sub) {
	switch (sub) {
	case 0: return "no-child";
	case 1: return "index-refused";
	case 2: return "untyped";
	case 3: return "bad-type";
	case 4: return "float";
	case 5: return "nonint";
	default: return "typed-addr-unresolved";
	}
}

#define COND_CC_KEY(k, sub, op) \
	(((long)(k) << 40) | ((long)(sub) << 32) | (long)(op))

static void cond_cause_label(long key, char *buf, size_t cap) {
	int k = (int)(key >> 40);
	int sub = (int)((key >> 32) & 0xFF);
	int op = (int)(key & 0xFFFFFFFF);
	const char *s = op ? refuse_opname(op) : NULL;
	if (k == AST_Load) {
		snprintf(buf, cap, "%s/%s addr=%s", refuse_kindname(k),
						 cond_load_subname(sub), refuse_kindname(op));
		return;
	}
	if (k == AST_Ref) {
		snprintf(buf, cap, "Ref/%s%s", refuse_global_name(sub),
						 op == 1		 ? " local/array-type"
						 : op == 2 ? " local/struct"
						 : op == 3 ? " local/untyped"
						 : op == 4 ? " local/float"
						 : op == 5 ? " local/nonint"
						 : op == 6 ? " local/address-taken"
											 : "");
		return;
	}
	if (op)
		snprintf(buf, cap, "%s op=%s(0x%x)", refuse_kindname(k), s ? s : "?", op);
	else
		snprintf(buf, cap, "%s", refuse_kindname(k));
}

/* The deepest node in a refused condition whose own children all pass, i.e.
 * the node that has to change for the condition to lower. Reading the cause
 * off the root of the condition names the comparison, not the operand that
 * actually blocked it. */
static AstLocal cond_first_blocker(AstArena *a, AstLocal n) {
	AstLocal c;
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (!ast_eval_slice_kind_ok(a, c, 1))
			return cond_first_blocker(a, c);
	return n;
}

static long blocker_key(AstArena *a, AstLocal b) {
	int k = ast_kind(a, b);
	if (k == AST_Ref) {
		int cls = refuse_global_class(a, b), t = ast_type_t(a, b), sub = 0;
		if (cls == RG_OTHER) {
			int rop = ast_op(a, b);
			sub = !t											 ? 3
						: (t & VT_ARRAY)				 ? 1
						: (t & VT_BTYPE) == VT_STRUCT ? 2
						: is_float(t)					 ? 4
						: !ast_eval_slice_intt(t)		 ? 5
						: !(rop & VT_LVAL)			 ? 6
														 : 0;
		}
		return COND_CC_KEY(k, cls, sub);
	}
	if (k == AST_Load) {
		AstLocal c = ast_first_child(a, b);
		return COND_CC_KEY(k, cond_load_sub(a, b),
											 c == AST_NONE ? 0 : ast_kind(a, c));
	}
	return COND_CC_KEY(
			k, 0,
			(k == AST_Unary || k == AST_Binary || k == AST_If) ? ast_op(a, b) : 0);
}

static void cond_cause_note(AstArena *a, AstLocal cond) {
	g_cc_blocked++;
	if (cond == AST_NONE) {
		cond_cause_add(COND_CC_KEY(0, 0, 0));
		return;
	}
	cond_cause_add(blocker_key(a, cond_first_blocker(a, cond)));
}

#define OTHER_CAUSE_CAP 256

static long g_oc_key[OTHER_CAUSE_CAP];
static long g_oc_n[OTHER_CAUSE_CAP];
static int g_oc_used;
static long g_oc_blocks;

static long g_ob_key[OTHER_CAUSE_CAP];
static long g_ob_n[OTHER_CAUSE_CAP];
static int g_ob_used;
static long g_ob_blocks;

#define OTHER_KEY(k, ev, op) \
	(((long)(k) << 40) | ((long)(ev) << 32) | (long)(op))

static void other_cause_label(long key, char *buf, size_t cap) {
	int k = (int)(key >> 40);
	int ev = (int)((key >> 32) & 0xFF);
	int op = (int)(key & 0xFFFFFFFF);
	const char *s = op ? refuse_opname(op) : NULL;
	const char *ok = ev ? "value-ok" : "value-refused";
	if (op)
		snprintf(buf, cap, "%s op=%s(0x%x) %s", refuse_kindname(k), s ? s : "?", op,
						 ok);
	else
		snprintf(buf, cap, "%s %s", refuse_kindname(k), ok);
}

static void other_cause_note(AstArena *a, AstLocal s) {
	int k = ast_kind(a, s);
	int ev = ast_eval_slice_kind_ok(a, s, 1);
	int op = (k == AST_Unary || k == AST_Binary || k == AST_If) ? ast_op(a, s) : 0;
	g_oc_blocks++;
	cause_tab_add(g_oc_key, g_oc_n, &g_oc_used, OTHER_CAUSE_CAP,
								OTHER_KEY(k, ev, op));
	if (ev)
		return;
	g_ob_blocks++;
	cause_tab_add(g_ob_key, g_ob_n, &g_ob_used, OTHER_CAUSE_CAP,
								blocker_key(a, cond_first_blocker(a, s)));
}

static int block_cause_stmt(AstArena *a, AstLocal s, int depth);

static int block_cause_seq(AstArena *a, AstLocal n, int depth) {
	AstLocal c;
	int r;
	if (n == AST_NONE)
		return BC_OK;
	if (ast_kind(a, n) != AST_BasicBlock)
		return block_cause_stmt(a, n, depth);
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c)) {
		r = block_cause_stmt(a, c, depth);
		if (r != BC_OK)
			return r;
	}
	return BC_OK;
}

static int block_cause_stmt(AstArena *a, AstLocal s, int depth) {
	AstLocal d, v;
	AstEvalSliceIdx ix;
	int32_t off, pf;
	int dt, et, r;
	if (depth > 8)
		return BC_CAPACITY;
	switch (ast_kind(a, s)) {
	case AST_Invoke:
		g_bc_at = s;
		return inv_thread_class((long)s) ? BC_INVOKE_THREAD : BC_INVOKE;
	case AST_StoreVal:
		return BC_STOREVAL;
	case AST_Jump:
		return BC_JUMP;
	case AST_Poison:
		return BC_POISON;
	default:
		break;
	}
	if (ast_kind(a, s) == AST_Return) {
		AstLocal rv = ast_first_child(a, s);
		if (ast_next_sib(a, s) != AST_NONE)
			return BC_RET_EARLY_MID;
		if (rv == AST_NONE)
			return BC_RET_EARLY_NOVAL;
		return ast_eval_slice_kind_ok(a, rv, 1) ? BC_OK : BC_RET_EARLY_EXPR;
	}
	if (ast_kind(a, s) == AST_If) {
		int op = ast_op(a, s);
		if (op == 6)
			return BC_SWITCH;
		if (op == 7)
			return ast_eval_slice_kind_ok(a, s, 1) ? BC_OK : BC_EXPR;
		if (op == 0 || op == 2 || op == 3 || op == 4) {
			AstLocal cond = op == 4 ? ast_child(a, s, 1) : ast_child(a, s, 0);
			AstLocal body = op == 0	 ? ast_child(a, s, 1)
											: op == 4	 ? ast_child(a, s, 0)
											: op == 3	 ? ast_child(a, s, 2)
																 : ast_child(a, s, 1);
			if (op != 0 && mcc_slice_may_ret(a, s))
				return BC_RET_EARLY_LOOP;
			if (!ast_eval_slice_kind_ok(a, cond, 1)) {
				cond_cause_note(a, cond);
				return BC_CTRL_COND;
			}
			r = block_cause_seq(a, body, depth + 1);
			if (r != BC_OK)
				return r;
			if (op == 0 && ast_nchild(a, s) == 3)
				return block_cause_seq(a, ast_child(a, s, 2), depth + 1);
			if (op == 3)
				return block_cause_seq(a, ast_child(a, s, 1), depth + 1);
			return BC_OK;
		}
		return BC_IF_OTHER;
	}
	if (ast_kind(a, s) == AST_Unary &&
			(ast_op(a, s) == TOK_INC || ast_op(a, s) == TOK_DEC)) {
		AstLocal t = ast_first_child(a, s);
		int tt;
		if (t == AST_NONE || !mcc_slice_is_local_ref(a, t, &off))
			return BC_INCDEC;
		tt = ast_type_t(a, t);
		if (!tt || ast_bad_type(tt) || is_float(tt) || !ast_eval_slice_intt(tt))
			return BC_INCDEC;
		if ((tt & VT_BTYPE) == VT_PTR && !ast_eval_slice_ptr_et(a, t))
			return BC_INCDEC;
		return BC_OK;
	}
	if (ast_kind(a, s) != AST_Store) {
		other_cause_note(a, s);
		return BC_OTHER;
	}
	if (ast_nchild(a, s) != 2)
		return BC_ST_ARITY;
	d = ast_child(a, s, 0);
	v = ast_child(a, s, 1);
	if (mcc_slice_store_dyn(a, d, &ix)) {
		if (!ast_eval_slice_kind_ok(a, ix.idx, 1))
			return BC_ST_IDX;
		return ast_eval_slice_kind_ok(a, v, 1) ? BC_OK : BC_ST_VALUE;
	}
	if (ast_eval_slice_deref(a, d, &pf, &et)) {
		if (ast_eval_slice_tsize(et) <= 0)
			return BC_ST_DEREF_SUBWORD;
		return ast_eval_slice_kind_ok(a, v, 1) ? BC_OK : BC_ST_VALUE;
	}
	{
		int32_t madd;
		if (ast_eval_slice_arrow(a, d, &pf, &madd, &et))
			return ast_eval_slice_kind_ok(a, v, 1) ? BC_OK : BC_ST_VALUE;
	}
	if (ast_kind(a, d) == AST_Load && ast_first_child(a, d) != AST_NONE &&
			ast_kind(a, ast_first_child(a, d)) == AST_Ref &&
			!(ast_op(a, ast_first_child(a, d)) & VT_SYM) &&
			(ast_op(a, ast_first_child(a, d)) & VT_VALMASK) == VT_LOCAL)
		return BC_ST_DEREF_TYPE;
	if (!mcc_slice_is_local_ref(a, d, &off)) {
		if (mcc_slice_store_frame(a, d, &off, &dt))
			return ast_eval_slice_kind_ok(a, v, 1) ? BC_OK : BC_ST_VALUE;
		if (ast_kind(a, d) == AST_Ref && (ast_op(a, d) & VT_SYM))
			return BC_ST_DEST_GLOBAL;
		return BC_ST_DEST_OTHER;
	}
	dt = ast_type_t(a, d);
	if (!dt || ast_bad_type(dt) ||
			(!ast_eval_slice_f64t(dt) && (is_float(dt) || !ast_eval_slice_intt(dt))))
		return BC_ST_TYPE;
	return ast_eval_slice_kind_ok(a, v, 1) ? BC_OK : BC_ST_VALUE;
}

enum {
	FF_NONE,
	FF_RET_MID,
	FF_RET_NOVAL,
	FF_RET_SCAN,
	FF_RET_TYPE,
	FF_MAXSTMT,
	FF_SLOT,
	FF_STMT_OTHER,
	FF_EMPTY,
	FF_EXT,
	FF_N
};

static long g_ff[FF_N];

static const char *ff_name(int r) {
	switch (r) {
	case FF_RET_MID: return "return-not-last";
	case FF_RET_NOVAL: return "return-no-value";
	case FF_RET_SCAN: return "return-value-refused";
	case FF_RET_TYPE: return "return-type";
	case FF_MAXSTMT: return "statement-budget";
	case FF_SLOT: return "slot-table-full";
	case FF_STMT_OTHER: return "statement-unmodelled";
	case FF_EMPTY: return "no-statement-and-no-return";
	case FF_EXT: return "extent-conflict";
	default: return "unexplained";
	}
}

static int frame_fail_reason(AstArena *a, AstLocal root) {
	MccSliceFrame f;
	AstLocal s;
	int i;
	memset(&f, 0, sizeof f);
	f.a = a;
	f.root = root;
	f.ret = AST_NONE;
	for (s = ast_first_child(a, root); s != AST_NONE; s = ast_next_sib(a, s)) {
		if (f.ret != AST_NONE)
			return FF_RET_MID;
		if (ast_kind(a, s) == AST_Return) {
			AstLocal rv = ast_first_child(a, s);
			int rt;
			if (rv == AST_NONE)
				return FF_RET_NOVAL;
			if (!mcc_slice_frame_scan(&f, rv))
				return f.nslot >= MCC_SLICE_MAXSLOT ? FF_SLOT : FF_RET_SCAN;
			rt = ast_type_t(a, rv);
			if (!rt)
				rt = ast_eval_slice_wtype(a, rv);
			if (!rt)
				rt = ast_eval_slice_ftype(a, rv);
			if (!rt || ast_bad_type(rt))
				return FF_RET_TYPE;
			if (!ast_eval_slice_f64t(rt) &&
					(is_float(rt) || !ast_eval_slice_intt(rt)))
				return FF_RET_TYPE;
			if ((ast_eval_slice_ftype(a, rv) != 0) != (ast_eval_slice_f64t(rt) != 0))
				return FF_RET_TYPE;
			if (f.rettype && f.rettype != rt)
				return FF_RET_TYPE;
			f.rettype = rt;
			f.ret = rv;
			continue;
		}
		if (f.nstmt + f.nctrl >= MCC_SLICE_MAXSTMT)
			return FF_MAXSTMT;
		if (!mcc_slice_frame_stmt_ok(&f, s, 0))
			return f.nslot >= MCC_SLICE_MAXSLOT ? FF_SLOT : FF_STMT_OTHER;
		if (f.ntop < MCC_SLICE_MAXSTMT)
			f.top[f.ntop++] = s;
	}
	if (f.ntop == 0 && f.ret == AST_NONE)
		return FF_EMPTY;
	for (i = 0; i < f.ntop; i++)
		mcc_slice_frame_mark_ptr(&f, f.top[i]);
	mcc_slice_frame_mark_ptr(&f, f.ret);
	for (i = 0; i < f.ntop; i++)
		if (!mcc_slice_frame_mark_ext(&f, f.top[i]))
			return FF_EXT;
	if (!mcc_slice_frame_mark_ext(&f, f.ret))
		return FF_EXT;
	return FF_NONE;
}

static void block_cause_walk(AstArena *a, AstLocal n) {
	AstLocal s;
	int r = BC_OK, seen = 0;
	g_bc_blocks++;
	g_bc_at = AST_NONE;
	for (s = ast_first_child(a, n); s != AST_NONE; s = ast_next_sib(a, s)) {
		seen++;
		if (ast_kind(a, s) == AST_Return) {
			AstLocal rv = ast_first_child(a, s);
			if (ast_next_sib(a, s) != AST_NONE)
				r = BC_RET_MID;
			else if (rv == AST_NONE)
				r = BC_RET_NOVAL;
			else if (!ast_eval_slice_kind_ok(a, rv, 1))
				r = BC_RET_EXPR;
			break;
		}
		r = block_cause_stmt(a, s, 0);
		if (r != BC_OK)
			break;
	}
	if (!seen)
		r = BC_EMPTY;
	if (r == BC_OK) {
		r = BC_NOCAUSE;
		g_ff[frame_fail_reason(a, n)]++;
	}
	if (g_bc_at != AST_NONE && ast_kind(a, g_bc_at) == AST_Invoke) {
		g_bc_invoke_all++;
		g_thr_blocks[inv_thread_class((long)g_bc_at)]++;
	}
	g_bc[r]++;
}

static void cond_cause_report(void) {
	if (!g_cc_blocked)
		return;
	printf("condcause: blocked-conditions=%ld\n", g_cc_blocked);
	cause_tab_report("condcause", g_cc_key, g_cc_n, g_cc_used, g_cc_blocked,
									 cond_cause_label);
}

static void other_cause_report(void) {
	int i;
	long sum = 0;
	if (!g_oc_blocks)
		return;
	printf("othercause: other-blocks=%ld blocker-walked=%ld\n", g_oc_blocks,
				 g_ob_blocks);
	for (i = 0; i < g_oc_used; i++)
		sum += g_oc_n[i];
	printf("othercause: forms-sum=%ld\n", sum);
	cause_tab_report("othercause", g_oc_key, g_oc_n, g_oc_used, g_oc_blocks,
									 other_cause_label);
	if (!g_ob_blocks)
		return;
	sum = 0;
	for (i = 0; i < g_ob_used; i++)
		sum += g_ob_n[i];
	printf("otherblocker: blockers-sum=%ld\n", sum);
	cause_tab_report("otherblocker", g_ob_key, g_ob_n, g_ob_used, g_ob_blocks,
									 cond_cause_label);
}

static void frame_fail_report(void) {
	int i;
	long sum = 0;
	for (i = 0; i < FF_N; i++)
		sum += g_ff[i];
	if (!sum)
		return;
	printf("nocause: blocks=%ld\n", sum);
	for (i = 0; i < FF_N; i++)
		if (g_ff[i])
			printf("nocause: %-30s n=%ld share=%.2f%%\n", ff_name(i), g_ff[i],
						 100.0 * (double)g_ff[i] / (double)sum);
}

static void block_cause_report(void) {
	int i;
	long sum = 0;
	printf("blockcause: refused-blocks=%ld unwalked=%ld table-overflow=%ld\n",
				 g_bc_blocks, g_ref_blocks - g_ref_blocks_acc - g_bc_blocks,
				 g_cc_dropped);
	for (i = 1; i < BC_N; i++)
		if (g_bc[i]) {
			sum += g_bc[i];
			printf("blockcause: %-28s n=%ld share=%.2f%%\n", block_cause_name(i),
						 g_bc[i], g_bc_blocks ? 100.0 * g_bc[i] / g_bc_blocks : 0.0);
		}
	printf("blockcause: causes-sum=%ld\n", sum);
	for (i = 1; i < MCC_THREAD_N; i++)
		printf("blockcause: stmt-thread/%-16s n=%ld\n", mcc_thread_class_name(i),
					 g_thr_blocks[i]);
	if (g_bc_invoke_all != g_bc[BC_INVOKE] + g_bc[BC_INVOKE_THREAD]) {
		fprintf(stderr,
						"slicerun: block/stmt-invoke %ld + block/stmt-thread %ld against "
						"%ld blocks attributed to an Invoke statement; the thread split is "
						"not a partition of the bucket it was carved out of\n",
						g_bc[BC_INVOKE], g_bc[BC_INVOKE_THREAD], g_bc_invoke_all);
		return;
	}
	printf("blockcause: stmt-invoke-partition-sum=%ld\n", g_bc_invoke_all);
}

static void refuse_walk(AstArena *a, AstLocal n, AstLocal parent, int idx) {
	AstLocal c;
	int r, k = 0;
	if (n == AST_NONE)
		return;
	g_ref_total_nodes++;
	g_ref_pos_cur = refuse_pos(a, parent, idx);
	g_ref_pos_all[g_ref_pos_cur]++;
	if (ast_eval_slice_kind_ok(a, n, 0)) {
		g_ref_accepted_nodes++;
		g_ref_pos_acc[g_ref_pos_cur]++;
		refuse_accepted_ref(a, n);
	} else {
		r = refuse_local(a, n);
		if (r == REF_OK)
			r = REF_CHILD;
		g_ref_nodes[r]++;
		g_ref_hit[r] = 1;
		g_ref_pos[r][g_ref_pos_cur]++;
		if (ast_kind(a, n) == AST_Invoke) {
			g_ref_invoke_all++;
			g_thr_nodes[inv_thread_class((long)n)]++;
		}
		if (r == REF_REF_GLOBAL) {
			int g = refuse_global_class(a, n);
			AstLocal p = ast_parent(a, n);
			g_ref_g_nodes[g]++;
			if (!refuse_parent_blocked(a, n))
				g_ref_g_free[g]++;
			if (p != AST_NONE && ast_kind(a, p) == AST_Invoke &&
					ast_first_child(a, p) == n)
				g_ref_g_callee[g]++;
		}
	}
	if (ast_kind(a, n) == AST_BasicBlock) {
		MccSliceFrame f;
		g_ref_blocks++;
		if (mcc_slice_frame_from_ast(a, n, &f))
			g_ref_blocks_acc++;
		else
			block_cause_walk(a, n);
	}
	if (ast_kind(a, n) == AST_Load)
		memshape_load(a, n);
	if (ast_kind(a, n) == AST_Store)
		memshape_store(a, n);
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		refuse_walk(a, c, n, k++);
}

static void refuse_body(AstArena *a, AstLocal root) {
	int i;
	long nn = mcc_slice_nodes(a, root);
	memset(g_ref_hit, 0, sizeof g_ref_hit);
	refuse_walk(a, root, AST_NONE, 0);
	g_ref_total_bodies++;
	g_ref_total_bodynodes += nn;
	for (i = 0; i < REF_N; i++)
		if (g_ref_hit[i]) {
			g_ref_bodies[i]++;
			g_ref_bodynodes[i] += nn;
		}
}

static int refuse_report(void) {
	int i;
	long gsum = 0;
	printf("refusal: nodes=%ld accepted=%ld refused=%ld bodies=%ld "
				 "body-nodes=%ld blocks=%ld frame-accepted-blocks=%ld\n",
				 g_ref_total_nodes, g_ref_accepted_nodes,
				 g_ref_total_nodes - g_ref_accepted_nodes, g_ref_total_bodies,
				 g_ref_total_bodynodes, g_ref_blocks, g_ref_blocks_acc);
	for (i = 1; i < REF_N; i++) {
		if (!g_ref_nodes[i] && !g_ref_bodies[i])
			continue;
		printf("refusal: %-18s nodes=%ld node-share=%.2f%% bodies=%ld "
					 "body-share=%.2f%% body-node-share=%.2f%%\n",
					 refuse_name(i), g_ref_nodes[i],
					 g_ref_total_nodes ? 100.0 * g_ref_nodes[i] / g_ref_total_nodes : 0.0,
					 g_ref_bodies[i],
					 g_ref_total_bodies ? 100.0 * g_ref_bodies[i] / g_ref_total_bodies
															: 0.0,
					 g_ref_total_bodynodes
							 ? 100.0 * g_ref_bodynodes[i] / g_ref_total_bodynodes
							 : 0.0);
	}
	for (i = 0; i < POS_N; i++)
		if (g_ref_pos_all[i])
			printf("refusal-slot: %-24s nodes=%ld accepted=%ld accept-rate=%.2f%%\n",
						 refuse_posname(i), g_ref_pos_all[i], g_ref_pos_acc[i],
						 100.0 * g_ref_pos_acc[i] / g_ref_pos_all[i]);
	for (i = 1; i < REF_N; i++) {
		int j;
		for (j = 0; j < POS_N; j++)
			if (g_ref_pos[i][j])
				printf("refusal-position: %-16s %-24s nodes=%ld node-share=%.2f%%\n",
							 refuse_name(i), refuse_posname(j), g_ref_pos[i][j],
							 g_ref_total_nodes
									 ? 100.0 * g_ref_pos[i][j] / g_ref_total_nodes
									 : 0.0);
		for (;;) {
			long best = -1;
			int bi = -1;
			for (j = 0; j < g_det_used; j++)
				if (g_det_cause[j] == i && g_det_n[j] > best) {
					best = g_det_n[j];
					bi = j;
				}
			if (bi < 0)
				break;
			{
				char lab[160];
				refuse_det_label(i, g_det_key[bi], lab, sizeof lab);
				printf("refusal-detail: %-16s %-44s nodes=%ld node-share=%.2f%%\n",
							 refuse_name(i), lab, g_det_n[bi],
							 g_ref_total_nodes ? 100.0 * g_det_n[bi] / g_ref_total_nodes
																 : 0.0);
			}
			g_det_cause[bi] = -1;
		}
	}
	for (i = 0; i < RG_N; i++) {
		gsum += g_ref_g_nodes[i];
		if (!g_ref_g_nodes[i])
			continue;
		printf("refusal: ref-not-local/%-20s nodes=%ld node-share=%.2f%% "
					 "class-share=%.2f%% parent-open=%ld callee=%ld\n",
					 refuse_global_name(i), g_ref_g_nodes[i],
					 g_ref_total_nodes
							 ? 100.0 * g_ref_g_nodes[i] / g_ref_total_nodes
							 : 0.0,
					 g_ref_nodes[REF_REF_GLOBAL]
							 ? 100.0 * g_ref_g_nodes[i] / g_ref_nodes[REF_REF_GLOBAL]
							 : 0.0,
					 g_ref_g_free[i], g_ref_g_callee[i]);
	}
	for (i = 0; i < RA_N; i++) {
		if (!g_ref_a_nodes[i])
			continue;
		printf("refusal: ref-accepted/%-21s nodes=%ld value-position=%ld\n",
					 refuse_accept_name(i), g_ref_a_nodes[i], g_ref_a_value[i]);
	}
	if (gsum != g_ref_nodes[REF_REF_GLOBAL]) {
		fprintf(stderr,
						"slicerun: ref-not-local classes sum to %ld, bucket holds %ld; "
						"the sub-attribution is not a partition\n",
						gsum, g_ref_nodes[REF_REF_GLOBAL]);
		return 1;
	}
	printf("refusal: ref-not-local-classes-sum=%ld\n", gsum);
	for (i = 1; i < MCC_THREAD_N; i++)
		printf("refusal: kind-invoke-thread/%-16s nodes=%ld\n",
					 mcc_thread_class_name(i), g_thr_nodes[i]);
	if (g_ref_invoke_all !=
			g_ref_nodes[REF_KIND_INVOKE] + g_ref_nodes[REF_KIND_INVOKE_THREAD]) {
		fprintf(stderr,
						"slicerun: kind-invoke %ld + kind-invoke-thread %ld against %ld "
						"refused Invoke nodes; the thread split is not a partition of the "
						"bucket it was carved out of\n",
						g_ref_nodes[REF_KIND_INVOKE], g_ref_nodes[REF_KIND_INVOKE_THREAD],
						g_ref_invoke_all);
		return 1;
	}
	printf("refusal: kind-invoke-partition-sum=%ld\n", g_ref_invoke_all);
	return 0;
}

static int cref_mixed_operands(AstArena *a, AstLocal n) {
	AstLocal c;
	if (n == AST_NONE)
		return 0;
	if (ast_kind(a, n) == AST_Binary && ast_op(a, n) != TOK_LAND &&
			ast_op(a, n) != TOK_LOR && ast_nchild(a, n) == 2) {
		int xt = ast_eval_slice_wtype(a, ast_child(a, n, 0));
		int yt = ast_eval_slice_wtype(a, ast_child(a, n, 1));
		int op = ast_op(a, n);
		if (op != TOK_SHL && op != TOK_SHR && op != TOK_SAR) {
			if (!xt || !yt)
				return 1;
			if (ast_eval_slice_is64(xt) != ast_eval_slice_is64(yt) ||
					((xt & VT_UNSIGNED) != 0) != ((yt & VT_UNSIGNED) != 0))
				return 1;
		}
	}
	if ((ast_kind(a, n) == AST_Binary || ast_kind(a, n) == AST_Unary)) {
		int t = ast_eval_slice_wtype(a, ast_first_child(a, n));
		int bt = t & VT_BTYPE;
		if ((t & VT_UNSIGNED) &&
				(bt == VT_BYTE || bt == VT_SHORT || bt == VT_BOOL))
			return 1;
	}
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (cref_mixed_operands(a, c))
			return 1;
	return 0;
}

static void cref_emit(AstArena *a, AstLocal root, MccSliceWork *w,
											const int64_t *in, const int64_t *cout,
											const unsigned char *cdef) {
	char path[1024];
	FILE *f;
	long id = g_cref_emitted;
	unsigned gid[MCC_SLICE_MAXLIVE];
	int32_t gdel[MCC_SLICE_MAXLIVE];
	int t, j, nd, any = 0, npar = 0;

	g_cref_seen++;
	if (w->nodes > CREF_MAXNODES) {
		g_cref_toobig++;
		return;
	}
	for (t = 0; t < 8; t++)
		if (cdef[t])
			any = 1;
	if (!any) {
		g_cref_alldead++;
		return;
	}
	if (cref_mixed_operands(a, root))
		g_cref_mixed++;
	snprintf(g_cref_tag, sizeof g_cref_tag, "%s%06ld", g_cref_pfx, id);

	for (j = 0; j < w->nlive; j++) {
		gid[j] = 0;
		gdel[j] = 0;
		if (!slicerun_glob_of(w->off[j], &gid[j], &gdel[j])) {
			gid[j] = 0;
			npar++;
		}
	}

	snprintf(path, sizeof path, "%s/s%06ld.c", g_cref_dir, id);
	f = fopen(path, "w");
	if (!f)
		return;
	for (j = 0; j < w->nlive; j++)
		if (gid[j]) {
			fprintf(f, "static volatile long long ");
			cref_glob_name(f, gid[j], gdel[j]);
			fprintf(f, ";\n");
		}
	fprintf(f, "static long long fn_%s(", g_cref_tag);
	if (!npar) {
		fprintf(f, "void");
	} else {
		int first = 1;
		for (j = 0; j < w->nlive; j++) {
			if (gid[j])
				continue;
			fprintf(f, "%slong long e%d", first ? "" : ", ", j);
			first = 0;
		}
	}
	fprintf(f, ") {\n\treturn (long long)(");
	if (!cref_expr(f, a, root, w->off, w->nlive)) {
		fclose(f);
		remove(path);
		g_cref_unspellable++;
		return;
	}
	fprintf(f, ")%s;\n}\n", g_cref_mutate ? " ^ 1" : "");
	fprintf(f, "static int chk_%s(void) {\n\tint bad = 0;\n\tlong long v;\n",
					g_cref_tag);
	{
		int first = 1;
		for (j = 0; j < w->nlive; j++) {
			if (gid[j])
				continue;
			fprintf(f, "%sa%d", first ? "\tvolatile long long " : ", ", j);
			first = 0;
		}
		if (!first)
			fprintf(f, ";\n");
	}
	nd = 0;
	for (t = 0; t < 8; t++) {
		int first;
		if (!cdef[t])
			continue;
		nd++;
		for (j = 0; j < w->nlive; j++) {
			fprintf(f, "\t");
			if (gid[j])
				cref_glob_name(f, gid[j], gdel[j]);
			else
				fprintf(f, "a%d", j);
			fprintf(f, " = ");
			cref_lit(f, in[t * w->nlive + j]);
			fprintf(f, ";\n");
		}
		fprintf(f, "\tv = fn_%s(", g_cref_tag);
		first = 1;
		for (j = 0; j < w->nlive; j++) {
			if (gid[j])
				continue;
			fprintf(f, "%sa%d", first ? "" : ", ", j);
			first = 0;
		}
		fprintf(f, ");\n\tif (v != ");
		cref_lit(f, cout[t]);
		fprintf(f, ") { printf(\"MISMATCH %s t%d got %%lld want %lld\\n\", v);"
							 " bad = 1; }\n",
						g_cref_tag, t, (long long)cout[t]);
	}
	fprintf(f, "\treturn bad;\n}\n");
	fclose(f);

	fprintf(g_cref_exp, "%s %d %d %d\n", g_cref_tag, w->nlive, w->nodes, nd);
	g_cref_tuples += nd;
	g_cref_emitted++;
}

static void run_real_slice(AstArena *a, AstLocal root, int quiet) {
	MccSliceWork w;
	MccSliceKernel k;
	int64_t in[8 * MCC_SLICE_MAXLIVE];
	int64_t cout[8], gout[8];
	unsigned char cdef[8], gdef[8];
	int t, j, flt;

	if (!mcc_slice_work_from_ast(a, root, &w))
		return;
	if (w.nodes < 3)
		return;
	g_arena_slices++;

	flt = subtree_has_f64(a, root, 0);
	for (t = 0; t < 8; t++)
		for (j = 0; j < w.nlive; j++)
			in[t * w.nlive + j] = flt ? seed_f64_value(t, j) : seed_value(t, j);

	mcc_slice_work_bind(&w, in, 8, cout, cdef);
	if (mcc_slice_run_cpu(&w, 0) != MCC_TASK_DONE)
		return;
	g_arena_tuples += 8;

	if (g_cref_dir)
		cref_emit(a, root, &w, in, cout, cdef);

	if (!g_have_device || !mcc_slice_kernel_build(&w, &k))
		return;
	g_arena_gpu_slices++;
	if (subtree_has_f64(a, root, 0))
		g_arena_f64_slices++;
	mcc_slice_work_bind(&w, in, 8, gout, gdef);
	if (mcc_slice_run_gpu(&w, &k, 0) != MCC_TASK_DONE) {
		mcc_slice_kernel_free(&k);
		g_arena_mismatch++;
		return;
	}
	for (t = 0; t < 8; t++) {
		if (cdef[t] && gdef[t] == cdef[t] && arena_nan_tie(flt, cout[t], gout[t])) {
			g_arena_nantie++;
			continue;
		}
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
static long g_frame_er, g_frame_er_built, g_frame_er_cmp;

enum {
	FD_SEEN,
	FD_REFUSED,
	FD_EXT_WIDE,
	FD_NO_REGION,
	FD_CPU_BAIL,
	FD_NO_DEVICE,
	FD_LOWER_SLOT,
	FD_LOWER_EMPTY,
	FD_LOWER_F64,
	FD_LOWER_EMIT,
	FD_DISPATCH,
	FD_DISAGREE,
	FD_AGREE,
	FD_N
};

static long g_fd[FD_N];

static const char *frame_drop_name(int i) {
	switch (i) {
	case FD_SEEN: return "funnel-seen";
	case FD_REFUSED: return "funnel-refused";
	case FD_EXT_WIDE: return "funnel-extent-over-slot";
	case FD_NO_REGION: return "funnel-no-device-region";
	case FD_CPU_BAIL: return "funnel-cpu-reference-bailed";
	case FD_NO_DEVICE: return "funnel-no-device";
	case FD_LOWER_SLOT: return "funnel-lower-no-live-in";
	case FD_LOWER_EMPTY: return "funnel-lower-nothing-to-run";
	case FD_LOWER_F64: return "funnel-lower-f64-unsupported";
	case FD_LOWER_EMIT: return "funnel-lower-emit-failed";
	case FD_DISPATCH: return "funnel-dispatch-failed";
	case FD_DISAGREE: return "funnel-disagreed";
	default: return "funnel-agreed";
	}
}

/* Why an accepted block reaches mcc_slice_frame_kernel_build with nslot == 0,
 * which is the largest named drop in the funnel above. Every slot in a frame
 * comes from one of two places: a statement writing a
 * destination (mcc_slice_slot_of) or an expression naming storage
 * (ast_eval_slice_livein). So a zero-slot block wrote nothing and read nothing
 * the collector recognised, and the question is whether there was anything to
 * recognise. The walk therefore looks for nodes that NAME storage and reports
 * the first one it meets in document order; a block where it meets none
 * computes from constants and the refusal is correct. */
enum {
	NS_RET_LITERAL,
	NS_RET_CONST_EXPR,
	NS_DISCARDED,
	NS_LOCAL,
	NS_GLOBAL,
	NS_MEMBER,
	NS_ARROW,
	NS_LOAD,
	NS_BAILOUT,
	NS_N
};

static long g_ns[NS_N];
static int g_noslot;
static char g_cur_fn[128];

static const char *noslot_name(int i) {
	switch (i) {
	case NS_RET_LITERAL: return "noslot/return-literal";
	case NS_RET_CONST_EXPR: return "noslot/return-const-expr";
	case NS_DISCARDED: return "noslot/discarded-operand";
	case NS_LOCAL: return "noslot/local-ref-unseen";
	case NS_GLOBAL: return "noslot/global-ref-unseen";
	case NS_MEMBER: return "noslot/member-unseen";
	case NS_ARROW: return "noslot/arrow-unseen";
	case NS_LOAD: return "noslot/load-unseen";
	default: return "noslot/bailout";
	}
}

static int noslot_node(AstArena *a, AstLocal n) {
	int r;
	switch (ast_kind(a, n)) {
	case AST_Ref:
		r = ast_op(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM))
			return NS_LOCAL;
		if (r & VT_SYM)
			return NS_GLOBAL;
		return -1;
	case AST_Unary:
		if (ast_op(a, n) == AST_EVAL_OP_MEMBER || ast_op(a, n) == AST_EVAL_OP_ADDR)
			return NS_MEMBER;
		if (ast_op(a, n) == AST_EVAL_OP_ARROW)
			return NS_ARROW;
		return -1;
	case AST_Load: return NS_LOAD;
	case AST_Bailout: return NS_BAILOUT;
	default: return -1;
	}
}

/* `disc` is a statement AST_If op 7, a ternary whose value is computed and
 * thrown away. Both executors return 1 for it without evaluating anything
 * (mcc_slice_frame_exec_stmt and mcc_slice_spv_stmt each have the same two
 * lines), so an operand under it is not live-in to anything and the collector
 * is right not to give it a slot. Naming it separately is what keeps the other
 * classes an invariant rather than a corpus fact: storage named in a
 * position that IS evaluated, with no slot, would be a live-in the kernel
 * cannot read. */
static void noslot_walk(AstArena *a, AstLocal n, int disc, int *ev, int *dc) {
	AstLocal c;
	int k;
	if (n == AST_NONE || *ev >= 0)
		return;
	if (ast_kind(a, n) == AST_If && ast_op(a, n) == 7)
		disc = 1;
	k = noslot_node(a, n);
	if (k >= 0) {
		if (disc) {
			if (*dc < 0)
				*dc = k;
		} else {
			*ev = k;
		}
		return;
	}
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		noslot_walk(a, c, disc, ev, dc);
}

static void noslot_classify(MccSliceFrame *f) {
	int i, k, ev = -1, dc = -1;
	for (i = 0; i < f->ntop; i++)
		noslot_walk(f->a, f->top[i], 0, &ev, &dc);
	noslot_walk(f->a, f->ret, 0, &ev, &dc);
	k = ev >= 0 ? ev
			: dc >= 0 ? NS_DISCARDED
			: f->ntop == 0 && f->ret != AST_NONE &&
							ast_kind(f->a, f->ret) == AST_Literal
					? NS_RET_LITERAL
					: NS_RET_CONST_EXPR;
	g_ns[k]++;
	if (g_noslot)
		printf("noslot %s fn=%s root=%lu ntop=%d nstmt=%d nctrl=%d nloop=%d "
					 "ret=%d neret=%d nodes=%d retkind=%d retop=%d retival=%lld\n",
					 noslot_name(k), g_cur_fn, (unsigned long)f->root, f->ntop, f->nstmt,
					 f->nctrl, f->nloop, f->ret != AST_NONE, f->neret, f->nodes,
					 f->ret == AST_NONE ? -1 : (int)ast_kind(f->a, f->ret),
					 f->ret == AST_NONE ? -1 : ast_op(f->a, f->ret),
					 f->ret == AST_NONE ? 0LL : (long long)ast_ival(f->a, f->ret));
}

#define FRAME_NT MCC_GPU_LOCAL_SIZE
#define FRAME_PTR_BASE (128 * 1024)
#define FRAME_PTR_STRIDE (12 * 1024)
#define FRAME_PTR_SLOT 72
#define FRAME_EXT_BASE 4096
#define FRAME_EXT_SLOT 512
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

static int64_t frame_ext_addr(int t, int j) {
	return (int64_t)(intptr_t)(g_rw + FRAME_PTR_BASE +
														 (long)t * FRAME_PTR_STRIDE + FRAME_EXT_BASE +
														 (long)j * FRAME_EXT_SLOT);
}

static AstLocal ext_lit_f64(AstArena *a, double d) {
	uint64_t u;
	memcpy(&u, &d, sizeof u);
	return mk_lit(a, (int64_t)u, VT_DOUBLE);
}

static void ext_fill(int64_t addr, int nspan, int etype) {
	unsigned char *p = (unsigned char *)(intptr_t)addr;
	int i;
	for (i = 0; i < nspan; i++) {
		if (ast_eval_slice_f64t(etype)) {
			double d = (double)(i * 3 - 5) / 4.0;
			memcpy(p + (long)i * 8, &d, sizeof d);
		} else if (ast_eval_slice_tsize(etype) == 8) {
			int64_t v = (int64_t)i * 7 - 3;
			memcpy(p + (long)i * 8, &v, sizeof v);
		} else {
			int32_t v = (int32_t)(i * 11 - 4);
			memcpy(p + (long)i * 4, &v, sizeof v);
		}
	}
}

static void ext_case(int nelem, int etype, const int64_t *idx, int nidx,
										 const char *what) {
	AstArena *a = ast_arena_new();
	AstLocal bb = ast_node(a, AST_BasicBlock);
	AstLocal arr = mk_ref(a, -256, VT_PTR | VT_ARRAY);
	AstLocal arr2 = mk_ref(a, -256, VT_PTR | VT_ARRAY);
	AstLocal ld = ast_node(a, AST_Load);
	AstLocal dst = ast_node(a, AST_Load);
	MccSliceFrame fr;
	MccSliceKernel k;
	int esize = ast_eval_slice_tsize(etype);
	int64_t *cf, *gf;
	int t, i, si = -1, sa = -1, bad = 0;

	slicerun_obj_reset(64);
	g_obj_ext[arr] = (int32_t)(nelem * esize);
	g_obj_ety[arr] = etype;
	g_obj_ext[arr2] = g_obj_ext[arr];
	g_obj_ety[arr2] = etype;
	ast_add_child(a, ld, mk_bin(a, '+', arr2, mk_ref(a, -8, VT_INT), 0));
	ast_set_type(a, ld, 0, 0);
	ast_add_child(a, dst, mk_bin(a, '+', arr, mk_ref(a, -8, VT_INT), 0));
	ast_set_type(a, dst, 0, 0);
	{
		AstLocal st = ast_node(a, AST_Store);
		ast_add_child(a, st, dst);
		ast_add_child(a, st,
									ast_eval_slice_f64t(etype)
											? mk_bin(a, '+', ld, ext_lit_f64(a, 1.5), etype)
											: mk_bin(a, '+', ld, mk_lit(a, 1, etype), etype));
		ast_add_child(a, bb, st);
	}
	CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1, what);
	if (fr.nslot < 1) {
		slicerun_obj_reset(0);
		ast_arena_free(a);
		return;
	}
	CHECK(fr.nslot == 2, "the array costs one descriptor slot, not one per element");
	CHECK(fr.next == 1, "and exactly one slot is marked as an extent");
	for (i = 0; i < fr.nslot; i++) {
		if (fr.slot[i] == -256)
			sa = i;
		if (fr.slot[i] == -8)
			si = i;
	}
	CHECK(sa >= 0 && si >= 0, "the descriptor and the index are both live-ins");
	if (sa < 0 || si < 0) {
		slicerun_obj_reset(0);
		ast_arena_free(a);
		return;
	}
	{
		int32_t nspan = 1;
		while (nspan < nelem)
			nspan <<= 1;
		CHECK(fr.sextb[sa] == nspan * esize,
					"the reserved span is the padded span the mask can reach");
	}
	cf = (int64_t *)malloc((size_t)FRAME_NT * fr.nslot * sizeof *cf);
	gf = (int64_t *)malloc((size_t)FRAME_NT * fr.nslot * sizeof *gf);
	if (!cf || !gf) {
		free(cf);
		free(gf);
		slicerun_obj_reset(0);
		ast_arena_free(a);
		return;
	}
	for (t = 0; t < FRAME_NT; t++)
		for (i = 0; i < fr.nslot; i++)
			cf[t * fr.nslot + i] = gf[t * fr.nslot + i] =
					i == sa ? frame_ext_addr(t, i) : idx[t % nidx];
	frame_ptr_seed();
	{
		int32_t nspan = 1;
		while (nspan < nelem)
			nspan <<= 1;
		for (t = 0; t < FRAME_NT; t++)
			ext_fill(frame_ext_addr(t, sa), nspan, etype);
	}
	memcpy(g_rw_pre, g_rw, g_rwsz);
	for (t = 0; t < FRAME_NT; t++)
		CHECK(mcc_slice_frame_exec_cpu(&fr, cf + (long)t * fr.nslot) == 1,
					"the CPU reference runs every extent frame");
	memcpy(g_rw_cpu, g_rw, g_rwsz);
	memcpy(g_rw, g_rw_pre, g_rwsz);
	CHECK(mcc_slice_frame_kernel_build(&fr, &k) == 1,
				"the extent run lowers to a device kernel");
	if (k.code.p) {
		CHECK(k.usemem == 1, "and the kernel declares that it touches binding 2");
		CHECK(mcc_slice_run_frame_gpu(&fr, &k, gf, FRAME_NT, NULL, NULL) ==
							MCC_TASK_DONE,
					"the device runs a whole workgroup of extent frames");
		for (t = 0; t < FRAME_NT * fr.nslot; t++)
			if (cf[t] != gf[t])
				bad++;
		CHECK(bad == 0, "every descriptor slot survives the dispatch unchanged");
		CHECK(memcmp(g_rw_cpu, g_rw, g_rwsz) == 0,
					"and every byte the two executors wrote to the shared region agrees");
		if (memcmp(g_rw_cpu, g_rw, g_rwsz)) {
			unsigned long b;
			int shown = 0;
			for (b = 0; b < g_rwsz && shown < 4; b++)
				if (g_rw_cpu[b] != g_rw[b]) {
					fprintf(stderr, "  EXT mem byte %lu cpu=%02x gpu=%02x\n", b,
									g_rw_cpu[b], g_rw[b]);
					shown++;
				}
		}
		mcc_slice_kernel_free(&k);
	}
	free(cf);
	free(gf);
	slicerun_obj_reset(0);
	ast_arena_free(a);
}

static AstLocal mk_arrow(AstArena *a, int32_t poff, int32_t madd, int mtype) {
	AstLocal n = ast_node(a, AST_Unary);
	ast_set_op(a, n, AST_EVAL_OP_ARROW);
	ast_set_type(a, n, mtype, 0);
	ast_set_ival(a, n, (uint64_t)(int64_t)madd);
	ast_add_child(a, n, mk_ref(a, poff, VT_PTR));
	return n;
}

/* `p->f` read and written at a runtime pointer. The differential is the point:
 * the CPU reference and the device both address the same shared region, so a
 * disagreement about where `ptr + member offset` lands, or about the width the
 * field is read at, shows up as a byte difference rather than as a refusal. */
static void arrow_case(int32_t madd, int mtype, int64_t v, const char *what) {
	AstArena *a = ast_arena_new();
	AstLocal bb = ast_node(a, AST_BasicBlock);
	MccSliceFrame fr;
	MccSliceKernel k;
	int64_t cf[FRAME_NT * MCC_SLICE_MAXSLOT], gf[FRAME_NT * MCC_SLICE_MAXSLOT];
	int t, j, bad = 0;

	{
		AstLocal st = ast_node(a, AST_Store);
		ast_set_type(a, st, mtype, 0);
		ast_add_child(a, st, mk_arrow(a, -8, madd, mtype));
		ast_add_child(a, st, mk_lit(a, v, mtype));
		ast_add_child(a, bb, st);
	}
	ast_add_child(a, bb,
								mk_store(a, -16,
												 mk_bin(a, '+', mk_arrow(a, -8, madd, mtype),
																mk_ref(a, -16, VT_INT), VT_INT),
												 VT_INT));
	CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1, what);
	if (fr.nslot < 2 || fr.nptr != 1) {
		CHECK(0, "the pointer slot is marked so the seeder puts a real address in it");
		ast_arena_free(a);
		return;
	}
	for (t = 0; t < FRAME_NT; t++)
		for (j = 0; j < fr.nslot; j++)
			cf[t * fr.nslot + j] = gf[t * fr.nslot + j] =
					fr.sptr[j] ? frame_ptr_addr(t, j) : seed_value(t, j);
	frame_ptr_seed();
	memcpy(g_rw_pre, g_rw, g_rwsz);
	for (t = 0; t < FRAME_NT; t++)
		CHECK(mcc_slice_frame_exec_cpu(&fr, cf + (long)t * fr.nslot) == 1,
					"the CPU reference runs every arrow frame");
	memcpy(g_rw_cpu, g_rw, g_rwsz);
	memcpy(g_rw, g_rw_pre, g_rwsz);
	if (mcc_slice_frame_kernel_build(&fr, &k) != 1) {
		CHECK(0, "the arrow run lowers to a device kernel");
		ast_arena_free(a);
		return;
	}
	CHECK(k.usemem == 1, "and the kernel declares that it touches binding 2");
	CHECK(mcc_slice_run_frame_gpu(&fr, &k, gf, FRAME_NT, NULL, NULL) ==
						MCC_TASK_DONE,
				"the device runs a whole workgroup of arrow frames");
	for (t = 0; t < FRAME_NT * fr.nslot; t++)
		if (cf[t] != gf[t])
			bad++;
	CHECK(bad == 0, "every frame slot agrees after the dispatch");
	CHECK(memcmp(g_rw_cpu, g_rw, g_rwsz) == 0,
				"and every byte the two executors wrote through `p->f` agrees");
	mcc_slice_kernel_free(&k);
	ast_arena_free(a);
}

static void suite_arrow(void) {
	AstArena *a;
	MccSliceFrame fr;

	if (!backend_has_frame_kernels()) {
		unsupported("frame kernels", "TODO.md §5 stage M2");
		return;
	}
	if (!g_have_device) {
		if (g_device_required) {
			fprintf(stderr,
							"FAIL slicerun: no usable device but a device is required\n");
			g_failures++;
		}
		return;
	}
	CHECK(g_rw != NULL && ast_eval_slice_rw != NULL,
				"the shared region is armed, without which `p->f` has no address");
	if (!g_rw)
		return;

	arrow_case(0, VT_INT, 0x11223344, "`p->f` at offset 0 is frame work");
	arrow_case(4, VT_INT, -7, "and at a non-zero member offset");
	arrow_case(8, VT_INT | VT_UNSIGNED, 0x89abcdef, "and an unsigned int field");
	arrow_case(5, VT_BYTE, -3, "and a signed byte field at an odd offset");
	arrow_case(6, VT_SHORT | VT_UNSIGNED, 0xbeef,
						 "and an unsigned short field");
	arrow_case(16, VT_LLONG, -1234567890123LL, "and a 64-bit field");

	/* Refused rather than approximated: the shapes whose width or address the
	 * tree does not write down. */
	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal ar = mk_arrow(a, -8, 4, VT_INT);
		ast_set_type_bf(a, ar, VT_INT | VT_BITFIELD, 0, 3, 5);
		ast_add_child(a, bb, mk_store(a, -16, ar, VT_INT));
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 0,
					"a bitfield member is refused, not read at the containing width");
	}
	ast_arena_free(a);

	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		ast_add_child(a, bb,
									mk_store(a, -16, mk_arrow(a, -8, 4, VT_INT | VT_ARRAY),
													 VT_INT));
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 0,
					"an array-typed member is refused: it is an address, not a value");
	}
	ast_arena_free(a);

	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal ar = ast_node(a, AST_Unary);
		ast_set_op(a, ar, AST_EVAL_OP_ARROW);
		ast_set_type(a, ar, VT_INT, 0);
		ast_set_ival(a, ar, 4);
		ast_add_child(a, ar, mk_ref(a, -8, VT_INT));
		ast_add_child(a, bb, mk_store(a, -16, ar, VT_INT));
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 0,
					"a non-pointer base is refused: an int slot is not an address");
	}
	ast_arena_free(a);
}

static void suite_ext(void) {
	static const int64_t INR[8] = {0, 1, 5, 17, 31, 12, 3, 30};
	static const int64_t WILD[8] = {0, 40, -1, 31, 99, 1000000, 7, -12345};
	AstArena *a;
	MccSliceFrame fr;

	if (!g_have_device) {
		if (g_device_required) {
			fprintf(stderr,
							"FAIL slicerun: no usable device but a device is required\n");
			g_failures++;
		}
		return;
	}
	/* An extent frame is a frame kernel (M2) that addresses binding 2 (M4).
	 * Neither exists on the Metal arm yet, so this compares nothing there and
	 * must say so rather than assert that lowering succeeded. */
	if (!backend_has_frame_kernels() || !backend_has_regions()) {
		unsupported("extent frames", "TODO.md §5 stages M2 and M4");
		return;
	}
	CHECK(g_rw != NULL && ast_eval_slice_rw != NULL,
				"the shared region is armed, without which no object is an extent");
	if (!g_rw)
		return;

	ext_case(32, VT_INT, INR, 8, "a 32-element array is frame work at all");
	ext_case(32, VT_INT, WILD, 8, "and stays frame work with a wild index");
	ext_case(64, VT_INT, INR, 8, "so is a 64-element one, four times the slot cap");
	ext_case(64, VT_INT, WILD, 8, "and it too survives every wild index");
	ext_case(48, VT_LLONG, INR, 8, "and one of 48 eight-byte elements");
	ext_case(32, VT_DOUBLE, INR, 8, "and one of 32 doubles");

	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal arr = mk_ref(a, -32, VT_PTR | VT_ARRAY);
		AstLocal dst = ast_node(a, AST_Load);
		slicerun_obj_reset(64);
		g_obj_ext[arr] = 16;
		g_obj_ety[arr] = VT_INT;
		ast_add_child(a, dst, mk_bin(a, '+', arr, mk_ref(a, -8, VT_INT), 0));
		ast_set_type(a, dst, 0, 0);
		{
			AstLocal st = ast_node(a, AST_Store);
			ast_add_child(a, st, dst);
			ast_add_child(a, st, mk_lit(a, 7, VT_INT));
			ast_add_child(a, bb, st);
		}
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1,
					"a four-element array is still frame work");
		CHECK(fr.next == 0 && fr.nslot == 5,
					"and still takes one slot per element, not a descriptor");
		slicerun_obj_reset(0);
	}
	ast_arena_free(a);

	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal arr = mk_ref(a, -128, VT_PTR | VT_ARRAY);
		AstLocal dst = ast_node(a, AST_Load);
		slicerun_obj_reset(64);
		g_obj_ext[arr] = 64;
		g_obj_ety[arr] = VT_SHORT;
		ast_add_child(a, dst, mk_bin(a, '+', arr, mk_ref(a, -8, VT_INT), 0));
		ast_set_type(a, dst, 0, 0);
		{
			AstLocal st = ast_node(a, AST_Store);
			ast_add_child(a, st, dst);
			ast_add_child(a, st, mk_lit(a, 7, VT_SHORT));
			ast_add_child(a, bb, st);
		}
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 0,
					"a 32-element array of shorts is refused, not raced");
		slicerun_obj_reset(0);
	}
	ast_arena_free(a);

	/* An array *field* over the dense threshold. The descriptor is keyed by the
	 * field's frame offset, so ast_eval_slice_livein_ext reserves the padded span
	 * at the struct offset plus the field offset and the seeder has one slot to
	 * put an address in -- exactly as for a plain local array, which is the whole
	 * claim being checked. */
	a = ast_arena_new();
	{
		AstLocal bb = ast_node(a, AST_BasicBlock);
		AstLocal dst;
		int sa = -1, i;
		slicerun_obj_reset(64);
		dst = mk_member_idx(a, -512, 16, 128, VT_INT, -8);
		{
			AstLocal st = ast_node(a, AST_Store);
			ast_add_child(a, st, dst);
			ast_add_child(a, st, mk_lit(a, 7, VT_INT));
			ast_add_child(a, bb, st);
		}
		CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1,
					"a 32-element array field is frame work");
		CHECK(fr.nslot == 2 && fr.next == 1,
					"and costs one descriptor slot plus the index");
		for (i = 0; i < fr.nslot; i++)
			if (fr.slot[i] == -496)
				sa = i;
		CHECK(sa >= 0, "the descriptor is keyed by the field offset, not the struct");
		if (sa >= 0)
			CHECK(fr.sextb[sa] == 128, "and reserves the padded span of the field");
		slicerun_obj_reset(0);
	}
	ast_arena_free(a);
}

#define HI_N MCC_GPU_LOCAL_SIZE
#define HI_NSLOT 2

static double g_hi_a[HI_N];
static double g_hi_b[HI_N];
static long long g_hi_c[HI_N];

static int hi_kernel(MccGpuCode *out) {
#if MCC_GPU_LANG_MSL
	(void)out;
	return 0;
#else
	SpvMod m;
	SpvRegion r;
	SpvV v, pa, pc;
	uint32_t base;
	uint32_t *code;
	int nw = 0;
	spv_module_begin(&m, HI_NSLOT);
	m.mem_base = ast_eval_slice_rw_base;
	m.mem_nbyte = (uint32_t)ast_eval_slice_rw_nbyte;
	base = spv_main_begin(&m, HI_NSLOT);
	if (!spv_mem_region(&m, &r)) {
		spv_module_free(&m);
		return 0;
	}
	pa = spv_load_live_v(&m, base, 0, 1, 1);
	pc = spv_load_live_v(&m, base, 1, 1, 1);
	v = spv_load_region(&m, &r, spv_mem_off(&m, pa), VT_LLONG);
	if (mcc_slice_mutate) {
		uint32_t p = spv_pair(&m, v);
		v = spv_mk(spv_u2(&m,
											spv_uop(&m, SpvOpBitwiseXor, spv_lo(&m, p),
															spv_uintc(&m, 1)),
											spv_hi(&m, p)),
							 1, 0);
	}
	spv_store_region(&m, &r, spv_mem_off(&m, pc), v, VT_LLONG);
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
#endif
}

static void hi_read_back(const void *src, int64_t *out) {
	uint64_t u;
	memcpy(&u, src, sizeof u);
	*out = (int64_t)u;
}

static void hi_round(const MccGpuCode *code, const void *src, size_t esize,
										 const char *what) {
	int32_t *in, *ob;
	int i, badv = 0, baddef = 0, badstore = 0, badcpu = 0;

	in = (int32_t *)calloc((size_t)HI_N * HI_NSLOT * MCC_GPU_IN_SLOTS, sizeof *in);
	ob = (int32_t *)calloc((size_t)HI_N * MCC_GPU_OUT_SLOTS, sizeof *ob);
	if (!in || !ob) {
		free(in);
		free(ob);
		return;
	}
	memset(g_hi_c, 0, sizeof g_hi_c);
	for (i = 0; i < HI_N; i++) {
		int64_t pa = (int64_t)(intptr_t)((const char *)src + (size_t)i * esize);
		int64_t pc = (int64_t)(intptr_t)&g_hi_c[i];
		int32_t *slot = in + (long)i * HI_NSLOT * MCC_GPU_IN_SLOTS;
		slot[0] = (int32_t)(uint32_t)(uint64_t)pa;
		slot[1] = (int32_t)(uint32_t)((uint64_t)pa >> 32);
		slot[2] = (int32_t)(uint32_t)(uint64_t)pc;
		slot[3] = (int32_t)(uint32_t)((uint64_t)pc >> 32);
	}
	if (!mcc_gpu_dispatch_rw2(code->p, code->n, in, HI_N, HI_NSLOT, ob)) {
		CHECK(0, "the kernel reading a static through the imported window "
							"dispatches");
		free(in);
		free(ob);
		return;
	}
	for (i = 0; i < HI_N; i++) {
		int64_t want, got, stored;
		int32_t off = 0;
		int cpuok = 0;
		int64_t cpu;
		hi_read_back((const char *)src + (size_t)i * esize, &want);
		got = (int64_t)((uint64_t)(uint32_t)ob[(long)i * MCC_GPU_OUT_SLOTS] |
										((uint64_t)(uint32_t)ob[(long)i * MCC_GPU_OUT_SLOTS + 1]
										 << 32));
		if (!ob[(long)i * MCC_GPU_OUT_SLOTS + 2])
			baddef++;
		if (got != want)
			badv++;
		stored = g_hi_c[i];
		if (stored != want)
			badstore++;
		if (!ast_eval_slice_rw_addr(
						(int64_t)(intptr_t)((const char *)src + (size_t)i * esize), &off)) {
			badcpu++;
			continue;
		}
		cpu = ast_eval_slice_bytes_load(ast_eval_slice_rw, ast_eval_slice_rw_nbyte,
																		off, VT_LLONG, &cpuok);
		if (!cpuok || cpu != want)
			badcpu++;
	}
	if (baddef)
		fprintf(stderr,
						"slicerun: %d of %d lanes reported the address of %s outside the "
						"window; the import did not take\n",
						baddef, HI_N, what);
	CHECK(baddef == 0, "every lane's address of a real static lands inside the "
										 "imported window");
	CHECK(badv == 0, "and the device reads back the value that static holds");
	CHECK(badstore == 0, "and a device store lands in a real static the host "
											 "then reads with ordinary C");
	CHECK(badcpu == 0, "and the CPU reference resolves the same address to the "
										 "same bytes");
	free(in);
	free(ob);
}

static void suite_hostimport(void) {
	const char *why = "";
	unsigned long align, span, i;
	uintptr_t lo, hi, p;
	void *base;
	unsigned long msz = 0;
	MccGpuCode code;

	if (!backend_has_regions()) {
		unsupported("a region layer", "TODO.md §5 stage M4");
		return;
	}
	if (!g_have_device) {
		if (g_device_required) {
			fprintf(stderr,
							"FAIL slicerun: no usable device but a device is required\n");
			g_failures++;
		}
		return;
	}
	align = mcc_gpu_host_import_align(&why);
	if (!align) {
		fprintf(stderr,
						"SKIP: slicerun hostimport needs VK_EXT_external_memory_host to put "
						"a static's own pages behind binding 2, and this device cannot: "
						"%s (device=%s)\n",
						why && why[0] ? why : "no reason reported", g_devname);
		g_unsupported = 1;
		return;
	}

	for (i = 0; i < HI_N; i++) {
		g_hi_a[i] = (double)((long)i * 3 - 7) / 8.0;
		g_hi_b[i] = (double)((long)i * -5 + 11) / 3.0;
	}

	lo = (uintptr_t)(void *)g_hi_a;
	hi = lo + sizeof g_hi_a;
	p = (uintptr_t)(void *)g_hi_b;
	if (p < lo)
		lo = p;
	if (p + sizeof g_hi_b > hi)
		hi = p + sizeof g_hi_b;
	p = (uintptr_t)(void *)g_hi_c;
	if (p < lo)
		lo = p;
	if (p + sizeof g_hi_c > hi)
		hi = p + sizeof g_hi_c;
	lo &= ~(uintptr_t)(align - 1);
	hi = (hi + align - 1) & ~(uintptr_t)(align - 1);
	span = (unsigned long)(hi - lo);
	if (span > 0x7ffffffcul) {
		fprintf(stderr,
						"SKIP: slicerun hostimport: the three statics span %lu bytes, more "
						"than a 32-bit window offset can address\n",
						span);
		g_unsupported = 1;
		return;
	}

	if (!mcc_gpu_mem_import((void *)lo, span)) {
		CHECK(0, "the host pages holding three statics import as device memory");
		return;
	}
	if (!mcc_gpu_mem(&base, &msz) || base != (void *)lo || msz != span) {
		CHECK(0, "and the shared window then reports exactly that host range");
		return;
	}
	ast_eval_slice_rw = (uint32_t *)base;
	ast_eval_slice_rw_base = (int64_t)(intptr_t)base;
	ast_eval_slice_rw_nbyte = (int32_t)msz;
	fprintf(stderr,
					"slicerun hostimport: imported %lu bytes at %p (align %lu) covering "
					"g_hi_a=%p g_hi_b=%p g_hi_c=%p\n",
					span, (void *)lo, align, (void *)g_hi_a, (void *)g_hi_b,
					(void *)g_hi_c);

	if (!hi_kernel(&code)) {
		CHECK(0, "the imported-window kernel emits");
		return;
	}
	hi_round(&code, g_hi_a, sizeof g_hi_a[0], "g_hi_a");
	hi_round(&code, g_hi_b, sizeof g_hi_b[0], "g_hi_b");
	free(code.p);
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
	int t, j, bad = 0, membad = 0, usemem, flt, rethas;

	if (!backend_has_frame_kernels()) {
		unsupported("frame kernels", "TODO.md §5 stage M2");
		return;
	}
	g_fd[FD_SEEN]++;
	if (!mcc_slice_frame_from_ast(a, bb, &fr)) {
		g_fd[FD_REFUSED]++;
		return;
	}
	for (j = 0; j < fr.nslot; j++)
		if (fr.sextb[j] > FRAME_EXT_SLOT) {
			g_fd[FD_EXT_WIDE]++;
			return;
		}
	g_frame_slices++;
	if (fr.neret)
		g_frame_er++;
	g_frame_stmts += fr.nstmt;
	usemem = (fr.nptr > 0 || fr.next > 0) && g_rw != NULL;
	if (fr.next > 0 && !usemem) {
		g_fd[FD_NO_REGION]++;
		return;
	}
	rethas = fr.ret != AST_NONE || fr.neret;
	flt = subtree_has_f64(a, bb, 0);
	for (t = 0; t < FRAME_NT; t++)
		for (j = 0; j < fr.nslot; j++)
			cf[t * fr.nslot + j] = gf[t * fr.nslot + j] =
					fr.sextb[j] && usemem
							? frame_ext_addr(t, j)
							: fr.sptr[j] && usemem ? frame_ptr_addr(t, j)
																		 : flt ? seed_f64_value(t, j)
																					 : seed_value(t, j);
	if (usemem) {
		frame_ptr_seed();
		memcpy(g_rw_pre, g_rw, g_rwsz);
	}
	for (t = 0; t < FRAME_NT; t++)
		if (!mcc_slice_frame_exec_cpu2(&fr, cf + (long)t * fr.nslot, &crv[t],
																	 &cdf[t])) {
			if (usemem)
				memcpy(g_rw, g_rw_pre, g_rwsz);
			g_fd[FD_CPU_BAIL]++;
			return;
		}
	if (usemem) {
		memcpy(g_rw_cpu, g_rw, g_rwsz);
		memcpy(g_rw, g_rw_pre, g_rwsz);
	}
	if (!g_have_device) {
		g_fd[FD_NO_DEVICE]++;
		return;
	}
	if (!mcc_slice_frame_kernel_build(&fr, &k)) {
		/* "nothing to run" is tested first even though kernel_build tests
		 * nslot first, because the two conditions are independent and when both
		 * hold only one of them is informative. A run with no store, no return
		 * and no early return has nothing for either executor to do -- both skip
		 * a discarded ternary outright -- so its emptiness is the cause and its
		 * lack of a live-in is a consequence. Measured over gcc c-torture the
		 * kernel_build order put 4 such blocks under "no live-in", which is how
		 * that bucket came to contain a block that does name a local. */
		int d = fr.nstmt < 1 && fr.ret == AST_NONE && !fr.neret ? FD_LOWER_EMPTY
						: fr.nslot < 1																	? FD_LOWER_SLOT
						: flt && !mcc_gpu_f64()													? FD_LOWER_F64
																														: FD_LOWER_EMIT;
		if (d == FD_LOWER_SLOT)
			noslot_classify(&fr);
		g_fd[d]++;
		return;
	}
	g_frame_built++;
	if (fr.neret)
		g_frame_er_built++;
	if (flt)
		g_arena_f64_frames++;
	if (getenv("MCC_SLICE_SPV_DUMP")) {
		char p[256];
		FILE *fp;
		snprintf(p, sizeof p, "%s/frame%04ld." MCC_GPU_CODE_SUFFIX,
						 getenv("MCC_SLICE_SPV_DUMP"), g_frame_built);
		fp = fopen(p, "wb");
		if (fp) {
			fwrite(k.code.p, MCC_GPU_CODE_UNIT, (size_t)k.code.n, fp);
			fclose(fp);
		}
	}
	if (mcc_slice_run_frame_gpu(&fr, &k, gf, FRAME_NT, grv, gdf) != MCC_TASK_DONE) {
		mcc_slice_kernel_free(&k);
		g_frame_mismatch++;
		g_fd[FD_DISPATCH]++;
		return;
	}
	g_frame_compared++;
	if (fr.neret)
		g_frame_er_cmp++;
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
		if (rethas && (int)gdf[t] == cdf[t] && cdf[t] &&
				arena_nan_tie(flt, crv[t], grv[t]))
			g_arena_nantie++;
		else if (rethas &&
						 ((int)gdf[t] != cdf[t] || (cdf[t] && grv[t] != crv[t])))
			bad++;
		if (!rethas && fr.nidx && (int)gdf[t] != cdf[t])
			bad++;
		for (j = 0; j < fr.nslot; j++)
			if (cf[t * fr.nslot + j] != gf[t * fr.nslot + j]) {
				if (arena_nan_tie(flt, cf[t * fr.nslot + j], gf[t * fr.nslot + j]))
					g_arena_nantie++;
				else
					bad++;
			}
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
	g_fd[bad ? FD_DISAGREE : FD_AGREE]++;
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
static int g_refusals;
static int g_noglob;
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

static AstArena *slicerun_leaf_hook(AstArena *a, AstLocal inv, AstLocal *root,
                                    int32_t *poff, int *pnparam) {
	int i;
	(void)a;
	/* Rebuilt from a dump: no parameter list to hand over. NULL asks the scan
	 * to order the slots by |offset|, which is layout-neutral. */
	(void)poff;
	*pnparam = 0;
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
		free(g_invthr);
		g_invcls = malloc((size_t)n);
		g_invleaf = malloc((size_t)n * sizeof *g_invleaf);
		g_invthr = malloc((size_t)n);
		g_invcap = (g_invcls && g_invleaf && g_invthr) ? n : 0;
		g_invthrcap = g_invcap;
	}
	for (i = 0; i < g_invcap; i++) {
		g_invcls[i] = -1;
		g_invleaf[i] = -1;
		g_invthr[i] = MCC_THREAD_NONE;
	}
}

static void inv_set(long node, const char *callee) {
	if (!g_invcls || node < 0 || node >= g_invcap)
		return;
	g_invcls[node] = !strcmp(callee, "?") ? 0 : defn_has(callee) ? 2 : 1;
	g_invleaf[node] = leaf_find(callee);
	g_invthr[node] = (signed char)mcc_thread_sym_class(callee);
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
	if (!mcc_slice_leaf_scan(a, rt, &L, NULL, 0)) {
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
static int arena_intern_overflow(const char *line) {
	if (strncmp(line, "[intern-overflow]", 17))
		return 0;
	fprintf(stderr,
					"slicerun: the arena dump reports %s"
					"slicerun: column 11 is the identity a global is relocated and named "
					"by, so past that point the dump cannot distinguish two objects. "
					"Refusing the run\n",
					line + 18);
	return 1;
}

static int arena_pass(FILE *f, int pass, long limit, int quiet) {
	char line[512];
	RawNode *raw = NULL;
	int cap = 0, pend = 0;

	while (pend ? (pend = 0, 1) : fgets(line, sizeof line, f) != NULL) {
		char fn[INV_NAMEMAX];
		long n, root;
		int i;
		if (arena_intern_overflow(line)) {
			free(raw);
			return 1;
		}
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
			if (arena_intern_overflow(line)) {
				free(raw);
				return 1;
			}
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
			snprintf(g_cur_fn, sizeof g_cur_fn, "%s", fn);
			slicerun_obj_reset(n);
			for (i = 0; i < n && i < g_obj_n; i++) {
				g_obj_ext[i] = (int32_t)raw[i].size;
				g_obj_ety[i] = raw[i].etype;
			}
			if (g_refusals) {
				if (g_inline)
					mcc_slice_inline_arena(a);
				refuse_body(a, rt);
			} else if (g_census) {
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

	if (g_refusals) {
		if (!g_ref_total_nodes) {
			fprintf(stderr, "slicerun: --refusals walked zero nodes; a refusal "
											"breakdown over an empty tree is not a measurement\n");
			return 1;
		}
		refuse_report();
		memshape_report();
		block_cause_report();
		cond_cause_report();
		other_cause_report();
		frame_fail_report();
		return 0;
	}

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
		if (!g_cn_blocks) {
			printf("slicerun: FAIL (the census walked 0 blocks -- every number "
						 "above is a zero with no subject behind it, and the two "
						 "sibling modes in this same function already refuse this)\n");
			return 1;
		}
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
	printf("slicerun: frame-earlyret-accepted=%ld frame-earlyret-built=%ld "
				 "frame-earlyret-compared=%ld\n",
				 g_frame_er, g_frame_er_built, g_frame_er_cmp);
	{
		int i;
		long sum = 0;
		printf("slicerun: frame-funnel");
		for (i = 0; i < FD_N; i++) {
			printf(" %s=%ld", frame_drop_name(i), g_fd[i]);
			if (i)
				sum += g_fd[i];
		}
		printf(" funnel-drops-sum=%ld\n", sum);
	}
	{
		int i;
		long sum = 0;
		printf("slicerun: noslot");
		for (i = 0; i < NS_N; i++) {
			printf(" %s=%ld", noslot_name(i), g_ns[i]);
			sum += g_ns[i];
		}
		printf(" noslot-sum=%ld\n", sum);
		if (sum != g_fd[FD_LOWER_SLOT]) {
			printf("slicerun: FAIL (the noslot classes sum to %ld against %ld "
						 "funnel-lower-no-live-in, so they are not a partition of it)\n",
						 sum, g_fd[FD_LOWER_SLOT]);
			g_arena_mismatch++;
		}
	}
	printf("slicerun: invoke-seen=%ld invoke-inlined=%ld leaf-callees=%d\n",
				 mcc_slice_inl_seen, mcc_slice_inl_n, g_leaf_n);
	printf("slicerun: depth-guards-emitted=%ld depth-guards-hit=%ld "
				 "depth-guards-gpu=%ld\n",
				 g_bail_nodes, ast_eval_slice_bail_n, mcc_gpu_bail_emit);
	if (g_cref_exp) {
		fclose(g_cref_exp);
		g_cref_exp = NULL;
		printf("slicerun: cref-seen=%ld cref-emitted=%ld cref-tuples=%ld "
					 "cref-toobig=%ld cref-unspellable=%ld cref-alldead=%ld "
					 "cref-mixed-operand-types=%ld\n",
					 g_cref_seen, g_cref_emitted, g_cref_tuples, g_cref_toobig,
					 g_cref_unspellable, g_cref_alldead, g_cref_mixed);
	}
	printf("slicerun: f64-slices=%ld f64-frames=%ld nan-tiebreak=%ld\n",
				 g_arena_f64_slices, g_arena_f64_frames, g_arena_nantie);
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

#define EFFECT_LANES 8
#define EFFECT_REGION_BYTE (64 * 1024)
#define EFFECT_LANE_BYTE 256
#define EFFECT_P_OFF 64
#define EFFECT_Q_OFF 128
#define EFFECT_PER_LANE 3

static unsigned char *g_eff_mem;
static unsigned char *g_eff_pre;
static unsigned char *g_eff_post;
static long g_eff_undone;
static long g_eff_records;
static long g_eff_replays;
static int g_eff_detected;
static int g_eff_tried;
static MccEffectLog g_eff_log;

static void effect_seed_mem(void) {
	long i;
	for (i = 0; i < EFFECT_REGION_BYTE; i++)
		g_eff_mem[i] = (i % 8) ? 0 : (unsigned char)(((i / 8) * 7 + 3) & 0x1f);
}

static int64_t effect_addr(int lane, int32_t off) {
	return (int64_t)(intptr_t)(g_eff_mem + (long)lane * EFFECT_LANE_BYTE + off);
}

static AstLocal effect_deref(AstArena *a, int32_t poff, int etype) {
	AstLocal p = mk_ref(a, poff, VT_PTR);
	AstLocal ld = ast_node(a, AST_Load);
	if (g_obj_ext && (long)p < g_obj_n) {
		g_obj_ext[p] = (int32_t)ast_eval_slice_tsize(etype);
		g_obj_ety[p] = etype;
	}
	ast_add_child(a, ld, p);
	ast_set_type(a, ld, 0, 0);
	return ld;
}

static int effect_exec(MccSliceFrame *fr, const int64_t *seed, int64_t *frame,
											 int64_t *rv, int *rd, int reverse) {
	int i, t, bad = 0;
	for (i = 0; i < EFFECT_LANES; i++) {
		t = reverse ? EFFECT_LANES - 1 - i : i;
		memcpy(frame + (long)t * fr->nslot, seed + (long)t * fr->nslot,
					 (size_t)fr->nslot * sizeof *frame);
		ast_eval_slice_effect_lane((uint32_t)t);
		if (!mcc_slice_frame_exec_cpu2(fr, frame + (long)t * fr->nslot, &rv[t],
																	 &rd[t]))
			bad++;
	}
	return bad == 0;
}

static void effect_record(MccSliceFrame *fr, const int64_t *seed,
													int64_t *frame, int64_t *rv, int *rd) {
	effect_seed_mem();
	memcpy(g_eff_pre, g_eff_mem, EFFECT_REGION_BYTE);
	mcc_effect_log_clear(&g_eff_log);
	g_eff_log.mode = MCC_EFFECT_RECORD;
	ast_eval_slice_effect_bind(&g_eff_log);
	effect_exec(fr, seed, frame, rv, rd, 0);
	g_eff_log.mode = MCC_EFFECT_OFF;
	ast_eval_slice_effect_bind(NULL);
	memcpy(g_eff_post, g_eff_mem, EFFECT_REGION_BYTE);
	g_eff_records += g_eff_log.recorded;
}

static int effect_rewind(void) {
	memcpy(g_eff_mem, g_eff_post, EFFECT_REGION_BYTE);
	g_eff_undone += mcc_effect_undo(&g_eff_log, ast_eval_slice_eff_undo, NULL);
	return memcmp(g_eff_mem, g_eff_pre, EFFECT_REGION_BYTE) == 0;
}

static void effect_key_case(void) {
	MccEffectKey k, k2;
	MccEffectLog L;
	MccEffect ev;
	char whatp[128];

	mcc_effect_key_init(&k, 0x9e3779b97f4a7c15ull);
	CHECK(mcc_effect_traceable(&k) == 0,
				"a slice with no live-ins recorded yet is not yet traceable");
	CHECK(mcc_effect_key_live(&k, -8, MCC_PROV_PARAM) == 1,
				"a live-in and its provenance class go into the key");
	CHECK(mcc_effect_key_live(&k, -16, MCC_PROV_LITERAL) == 1,
				"and so does a second");
	CHECK(mcc_effect_key_live(&k, -24, MCC_PROV_LOAD) == 1,
				"and a load from a known object is traceable provenance");
	CHECK(mcc_effect_traceable(&k) == 1,
				"the eligibility question is one integer compare, not a walk");
	CHECK(mcc_effect_key_live(&k, -32, MCC_PROV_ANON) == 1,
				"a pointer from an anonymously-linked call is recorded like any other");
	CHECK(mcc_effect_traceable(&k) == 0,
				"and it is the one class that makes the whole slice ineligible");
	CHECK(k.nopaque == 1, "the opaque count says how many, not merely whether");

	mcc_effect_key_init(&k2, 0x9e3779b97f4a7c15ull);
	mcc_effect_key_live(&k2, -8, MCC_PROV_PARAM);
	mcc_effect_key_live(&k2, -16, MCC_PROV_LITERAL);
	mcc_effect_key_live(&k2, -24, MCC_PROV_LOAD);
	CHECK(mcc_effect_key_eq(&k, &k2) == 0,
				"two keys over the same slice with different live-in vectors differ");
	CHECK(mcc_effect_key_hash(&k) != mcc_effect_key_hash(&k2),
				"and so do their hashes, so a cache cannot conflate them");
	CHECK(mcc_effect_traceable(&k2) == 1,
				"the traceable one stays traceable");

	mcc_effect_log_init(&L);
	CHECK(mcc_effect_key_seal(&k2, &L) == 1, "an empty log seals as a certificate");
	CHECK(mcc_effect_effectful(&k2) == 0, "a slice that performed no effect is pure");
	CHECK(mcc_effect_memoisable(&k2) == 1,
				"and a traceable, sealed, effect-free slice is the only memoisable "
				"shape");
	CHECK(mcc_effect_key_match(&k2, &L) == 1, "and its certificate matches its log");

	L.mode = MCC_EFFECT_RECORD;
	memset(&ev, 0, sizeof ev);
	ev.kind = MCC_EFFECT_STORE;
	ev.site = 7;
	ev.addr = 16;
	ev.width = 4;
	ev.value = 99;
	mcc_effect_record(&L, &ev, NULL, 0);
	L.mode = MCC_EFFECT_OFF;
	CHECK(mcc_effect_key_match(&k2, &L) == 0,
				"a log that grew an effect no longer matches the sealed certificate");
	mcc_effect_key_seal(&k2, &L);
	CHECK(mcc_effect_effectful(&k2) == 1,
				"and re-sealing records that the slice is effectful");
	CHECK(mcc_effect_memoisable(&k2) == 0,
				"which is exactly the admission test a value-memoising cache needs: a "
				"non-empty effect log is the refusal");
	CHECK(mcc_effect_perturb(&L, 0, whatp, sizeof whatp) == 1,
				"the sealed log can be perturbed");
	CHECK(mcc_effect_key_match(&k2, &L) == 0,
				"and a certificate detects a log perturbed after sealing");
	mcc_effect_log_free(&L);
}

static int effect_replay(MccSliceFrame *fr, const int64_t *seed,
												 int64_t *frame, int64_t *rv, int *rd, int reverse) {
	int closed;
	memcpy(g_eff_mem, g_eff_pre, EFFECT_REGION_BYTE);
	mcc_effect_replay_begin(&g_eff_log);
	ast_eval_slice_effect_bind(&g_eff_log);
	effect_exec(fr, seed, frame, rv, rd, reverse);
	closed = mcc_effect_replay_end(&g_eff_log);
	ast_eval_slice_effect_bind(NULL);
	g_eff_replays += g_eff_log.consumed;
	return closed && !mcc_effect_diverged(&g_eff_log);
}

static int effect_same(const MccSliceFrame *fr, const int64_t *cf,
											 const int64_t *rf, const int64_t *crv,
											 const int64_t *rrv, const int *cdf, const int *rdf) {
	int i;
	for (i = 0; i < EFFECT_LANES * fr->nslot; i++)
		if (cf[i] != rf[i])
			return 0;
	for (i = 0; i < EFFECT_LANES; i++)
		if (crv[i] != rrv[i] || cdf[i] != rdf[i])
			return 0;
	return 1;
}

static const char *effect_verdict(int v) {
	switch (v) {
	case 1:
		return "divergence";
	case 2:
		return "result mismatch";
	case 3:
		return "effect performed twice";
	case 4:
		return "rewind did not restore the pre-image";
	default:
		return "UNDETECTED";
	}
}

static void effect_case(int etype, const char *what) {
	AstArena *a = ast_arena_new();
	AstLocal bb = ast_node(a, AST_BasicBlock);
	MccSliceFrame fr;
	int64_t *seed = NULL, *cf = NULL, *rf = NULL;
	int64_t crv[EFFECT_LANES], rrv[EFFECT_LANES];
	int cdf[EFFECT_LANES], rdf[EFFECT_LANES];
	int i, t, sp = -1, sq = -1, w;
	char msg[256];

	slicerun_obj_reset(64);
	ast_add_child(a, bb,
								mk_store(a, -24,
												 mk_bin(a, '+', effect_deref(a, -8, etype),
																mk_lit(a, 1, etype), etype),
												 etype));
	{
		AstLocal st = ast_node(a, AST_Store);
		ast_add_child(a, st, effect_deref(a, -16, etype));
		ast_add_child(a, st,
									mk_bin(a, '*', mk_ref(a, -24, etype),
												 mk_lit(a, 3, etype), etype));
		ast_add_child(a, bb, st);
	}
	ast_add_child(a, bb,
								mk_store(a, -32, effect_deref(a, -16, etype), etype));

	CHECK(mcc_slice_frame_from_ast(a, bb, &fr) == 1, what);
	if (fr.nslot < 1) {
		slicerun_obj_reset(0);
		ast_arena_free(a);
		return;
	}
	for (i = 0; i < fr.nslot; i++) {
		if (fr.slot[i] == -8)
			sp = i;
		if (fr.slot[i] == -16)
			sq = i;
	}
	CHECK(sp >= 0 && sq >= 0, "both pointer live-ins survive into the slot map");
	if (sp < 0 || sq < 0) {
		slicerun_obj_reset(0);
		ast_arena_free(a);
		return;
	}
	seed = (int64_t *)malloc((size_t)EFFECT_LANES * fr.nslot * sizeof *seed);
	cf = (int64_t *)malloc((size_t)EFFECT_LANES * fr.nslot * sizeof *cf);
	rf = (int64_t *)malloc((size_t)EFFECT_LANES * fr.nslot * sizeof *rf);
	if (!seed || !cf || !rf) {
		free(seed);
		free(cf);
		free(rf);
		slicerun_obj_reset(0);
		ast_arena_free(a);
		return;
	}
	for (t = 0; t < EFFECT_LANES; t++)
		for (i = 0; i < fr.nslot; i++)
			seed[(long)t * fr.nslot + i] =
					i == sp ? effect_addr(t, EFFECT_P_OFF)
									: i == sq ? effect_addr(t, EFFECT_Q_OFF) : (int64_t)t;

	effect_record(&fr, seed, cf, crv, cdf);
	CHECK(g_eff_log.recorded == (long)EFFECT_LANES * EFFECT_PER_LANE,
				"the recorder saw every effect of every lane and no others");
	CHECK(!mcc_effect_diverged(&g_eff_log), "recording itself does not diverge");
	CHECK(memcmp(g_eff_mem, g_eff_pre, EFFECT_REGION_BYTE) != 0,
				"the executor's run really did change the shared region");

	CHECK(effect_replay(&fr, seed, rf, rrv, rdf, 0) == 1,
				"the replayed side asks for exactly the effects the executor performed");
	if (mcc_effect_diverged(&g_eff_log)) {
		mcc_effect_div_str(&g_eff_log, msg, sizeof msg);
		fprintf(stderr, "  %s\n", msg);
	}
	CHECK(memcmp(g_eff_mem, g_eff_pre, EFFECT_REGION_BYTE) == 0,
				"and performs none of them: every observable effect ran exactly once");
	CHECK(effect_same(&fr, cf, rf, crv, rrv, cdf, rdf),
				"and computes the same frame, return value and verdict as the executor");
	CHECK(g_eff_log.consumed == g_eff_log.recorded,
				"and consumes the whole log, so no effect went unclaimed");

	{
		uint64_t h0 = mcc_effect_hash(&g_eff_log);
		CHECK(effect_replay(&fr, seed, rf, rrv, rdf, 1) == 1,
					"replaying the lanes in the opposite order is still faithful");
		CHECK(effect_same(&fr, cf, rf, crv, rrv, cdf, rdf),
					"and lane order does not change any lane's result");
		CHECK(mcc_effect_hash(&g_eff_log) == h0,
					"and a replay never mutates the log it replays against");
		CHECK(mcc_effect_shuffle_lanes(&g_eff_log) == 1,
					"the log can be regrouped across lanes without touching per-lane "
					"order");
		CHECK(mcc_effect_hash(&g_eff_log) != h0,
					"and that regrouping really is a different serialisation");
	}
	CHECK(effect_replay(&fr, seed, rf, rrv, rdf, 0) == 1,
				"a cross-lane reinterleaving of the log is not a divergence, by design");
	CHECK(effect_same(&fr, cf, rf, crv, rrv, cdf, rdf),
				"and yields the same per-lane results");

	effect_record(&fr, seed, cf, crv, cdf);
	CHECK(mcc_effect_rewindable(&g_eff_log, 0) == 0,
				"prior values are off by default, so the log says outright that it "
				"cannot be rewound rather than offering a plausible zero");
	g_eff_log.want_prev = 1;
	effect_record(&fr, seed, cf, crv, cdf);
	CHECK(mcc_effect_rewindable(&g_eff_log, 0) == 1,
				"and with the mode switch on it says it can");
	CHECK(mcc_effect_mark(&g_eff_log) == (int)g_eff_log.recorded,
				"a checkpoint is an index into the log, taken after the dispatch's "
				"effects");
	CHECK(effect_rewind() == 1,
				"walking the log backwards and restoring each store's prior value "
				"returns the shared region to its pre-image exactly");
	CHECK(mcc_effect_undo_to(&g_eff_log, mcc_effect_mark(&g_eff_log),
													 ast_eval_slice_eff_undo, NULL) == 0,
				"and rewinding to the checkpoint at the end of the log undoes nothing");

	for (w = 0; w < MCC_EFFECT_PERTURB_COUNT; w++) {
		char whatp[128];
		int verdict = 0;
		effect_record(&fr, seed, cf, crv, cdf);
		if (!mcc_effect_perturb(&g_eff_log, w, whatp, sizeof whatp))
			continue;
		g_eff_tried++;
		if (!effect_replay(&fr, seed, rf, rrv, rdf, 0))
			verdict = 1;
		else if (!effect_same(&fr, cf, rf, crv, rrv, cdf, rdf))
			verdict = 2;
		else if (memcmp(g_eff_mem, g_eff_pre, EFFECT_REGION_BYTE) != 0)
			verdict = 3;
		else if (!effect_rewind())
			verdict = 4;
		if (verdict)
			g_eff_detected++;
		snprintf(msg, sizeof msg, "a perturbed log is detected: %s -> %s", whatp,
						 effect_verdict(verdict));
		CHECK(verdict != 0, msg);
	}
	g_eff_log.want_prev = 0;

	if (g_mutate) {
		char whatp[128];
		effect_record(&fr, seed, cf, crv, cdf);
		if (mcc_effect_perturb(&g_eff_log, 0, whatp, sizeof whatp)) {
			fprintf(stderr, "  MUTATE: %s\n", whatp);
			CHECK(effect_replay(&fr, seed, rf, rrv, rdf, 0) == 1,
						"the mutated log replays faithfully");
			mcc_effect_div_str(&g_eff_log, msg, sizeof msg);
			fprintf(stderr, "  MUTATE: %s\n", msg);
			CHECK(effect_same(&fr, cf, rf, crv, rrv, cdf, rdf),
						"the mutated log reproduces the executor's results");
		}
	}

	free(seed);
	free(cf);
	free(rf);
	slicerun_obj_reset(0);
	ast_arena_free(a);
}

static int effect_write_lane(MccEffectLog *L, int mode, uint32_t lane, int fd,
														 int32_t off, const char *bytes, int nb) {
	MccEffect ev;
	memset(&ev, 0, sizeof ev);
	ev.kind = MCC_EFFECT_WRITE;
	ev.space = MCC_EFFECT_SPACE_FD;
	ev.chan = fd;
	ev.site = 1000u + lane;
	ev.lane = lane;
	ev.addr = off;
	ev.width = nb;
	ev.flags = MCC_EFFECT_F_NOUNDO;
	if (mode == MCC_EFFECT_REPLAY)
		return mcc_effect_replay(L, &ev, bytes, nb);
	return mcc_effect_record(L, &ev, bytes, nb);
}

static void effect_kinds_case(void) {
	static const char *TXT[4] = {"alpha", "bravo", "charlie", "delta"};
	MccEffectLog L;
	char msg[256], whatp[128];
	int lane, w, detected = 0, tried = 0;

	mcc_effect_log_init(&L);
	L.mode = MCC_EFFECT_RECORD;
	for (lane = 0; lane < 4; lane++) {
		effect_write_lane(&L, MCC_EFFECT_RECORD, (uint32_t)lane, 1, lane * 16,
											TXT[lane], (int)strlen(TXT[lane]));
		effect_write_lane(&L, MCC_EFFECT_RECORD, (uint32_t)lane, 2, lane * 8,
											TXT[3 - lane], (int)strlen(TXT[3 - lane]));
	}
	L.mode = MCC_EFFECT_OFF;
	CHECK(L.recorded == 8,
				"a kind the memory path never produces records through the same log");
	CHECK(mcc_effect_rewindable(&L, 0) == 0,
				"and a file write declares itself not rewindable, so a rollback that "
				"crossed it would be reported as partial rather than believed");
	CHECK(mcc_effect_undo(&L, ast_eval_slice_eff_undo, NULL) == 0,
				"and the rewind walk skips it rather than inventing a prior value");

	mcc_effect_replay_begin(&L);
	for (lane = 3; lane >= 0; lane--) {
		effect_write_lane(&L, MCC_EFFECT_REPLAY, (uint32_t)lane, 1, lane * 16,
											TXT[lane], (int)strlen(TXT[lane]));
		effect_write_lane(&L, MCC_EFFECT_REPLAY, (uint32_t)lane, 2, lane * 8,
											TXT[3 - lane], (int)strlen(TXT[3 - lane]));
	}
	CHECK(mcc_effect_replay_end(&L) == 1,
				"a payload-carrying output effect replays by byte comparison");
	if (mcc_effect_diverged(&L)) {
		mcc_effect_div_str(&L, msg, sizeof msg);
		fprintf(stderr, "  %s\n", msg);
	}

	for (w = 11; w <= 13; w++) {
		int ok;
		mcc_effect_log_clear(&L);
		L.mode = MCC_EFFECT_RECORD;
		for (lane = 0; lane < 4; lane++)
			effect_write_lane(&L, MCC_EFFECT_RECORD, (uint32_t)lane, 1, lane * 16,
												TXT[lane], (int)strlen(TXT[lane]));
		L.mode = MCC_EFFECT_OFF;
		if (!mcc_effect_perturb(&L, w, whatp, sizeof whatp))
			continue;
		tried++;
		mcc_effect_replay_begin(&L);
		for (lane = 0; lane < 4; lane++)
			effect_write_lane(&L, MCC_EFFECT_REPLAY, (uint32_t)lane, 1, lane * 16,
												TXT[lane], (int)strlen(TXT[lane]));
		ok = mcc_effect_replay_end(&L);
		if (!ok)
			detected++;
		snprintf(msg, sizeof msg, "a perturbed write log is detected: %s -> %s",
						 whatp, ok ? "UNDETECTED" : mcc_effect_div_name(L.div.code));
		CHECK(!ok, msg);
	}
	CHECK(tried == 3 && detected == 3,
				"channel, payload and address-space perturbations are all caught");
	g_eff_tried += tried;
	g_eff_detected += detected;
	mcc_effect_log_free(&L);
}

static void suite_effect(void) {
	uint32_t *save_rw = ast_eval_slice_rw;
	int32_t save_n = ast_eval_slice_rw_nbyte;
	int64_t save_b = ast_eval_slice_rw_base;

	g_eff_mem = (unsigned char *)malloc(EFFECT_REGION_BYTE);
	g_eff_pre = (unsigned char *)malloc(EFFECT_REGION_BYTE);
	g_eff_post = (unsigned char *)malloc(EFFECT_REGION_BYTE);
	if (!g_eff_mem || !g_eff_pre || !g_eff_post) {
		free(g_eff_mem);
		free(g_eff_pre);
		free(g_eff_post);
		g_eff_mem = g_eff_pre = g_eff_post = NULL;
		CHECK(0, "the effect suite can allocate its own shared region");
		return;
	}
	ast_eval_slice_rw = (uint32_t *)(void *)g_eff_mem;
	ast_eval_slice_rw_nbyte = EFFECT_REGION_BYTE;
	ast_eval_slice_rw_base = (int64_t)(intptr_t)g_eff_mem;
	mcc_effect_log_init(&g_eff_log);

	effect_case(VT_BYTE, "a run of pointer loads and stores is frame work (i8)");
	effect_case(VT_INT, "a run of pointer loads and stores is frame work (i32)");
	effect_case(VT_LLONG, "a run of pointer loads and stores is frame work (i64)");
	effect_kinds_case();
	effect_key_case();

	CHECK(g_eff_records >= 3 * EFFECT_LANES * EFFECT_PER_LANE,
				"the record/replay self-check recorded effects rather than nothing");
	CHECK(g_eff_replays >= 3 * 3 * EFFECT_LANES * EFFECT_PER_LANE,
				"and every faithful replay consumed a whole log rather than stopping "
				"early");
	CHECK(g_eff_tried >= 3 * 11 + 3,
				"and every perturbation shape was applied somewhere it applies");
	CHECK(g_eff_detected == g_eff_tried,
				"and not one of them survived");
	CHECK(g_eff_undone >= 3 * EFFECT_LANES,
				"and the rewind path really put stores back rather than finding none");
	fprintf(stderr,
					"slicerun: effect records=%ld replays=%ld undone=%ld "
					"perturbations=%d/%d detected\n",
					g_eff_records, g_eff_replays, g_eff_undone, g_eff_detected,
					g_eff_tried);

	mcc_effect_log_free(&g_eff_log);
	ast_eval_slice_effect_bind(NULL);
	ast_eval_slice_rw = save_rw;
	ast_eval_slice_rw_nbyte = save_n;
	ast_eval_slice_rw_base = save_b;
	free(g_eff_mem);
	free(g_eff_pre);
	free(g_eff_post);
	g_eff_mem = g_eff_pre = g_eff_post = NULL;
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

#define THR_MAXORDER 128

static int g_thr_order[THR_MAXORDER];
static int g_thr_norder;

typedef struct ThrJob {
	int id;
	int steps;
	int done;
	MccDepGraph *g;
	uint32_t lane;
	int cell;
	int64_t delta;
	int fd;
} ThrJob;

static void thr_stamp(int id) {
	if (g_thr_norder < THR_MAXORDER)
		g_thr_order[g_thr_norder++] = id;
}

static int thr_first(int id) {
	int i;
	for (i = 0; i < g_thr_norder; i++)
		if (g_thr_order[i] == id)
			return i;
	return -1;
}

static int thr_last(int id) {
	int i, r = -1;
	for (i = 0; i < g_thr_norder; i++)
		if (g_thr_order[i] == id)
			r = i;
	return r;
}

static void thr_job(ThrJob *j, MccDepGraph *g, int id, int steps) {
	memset(j, 0, sizeof *j);
	j->id = id;
	j->steps = steps;
	j->g = g;
	j->lane = (uint32_t)id;
}

static int thr_tick(MccTask *t) {
	ThrJob *j = (ThrJob *)t->ctx;
	thr_stamp(j->id);
	j->done++;
	return j->done < j->steps ? MCC_TASK_YIELDED : MCC_TASK_DONE;
}

static int thr_tick_cell(MccTask *t) {
	ThrJob *j = (ThrJob *)t->ctx;
	int ok = 0;
	thr_stamp(j->id);
	mcc_thread_cell_rmw(j->g, j->lane, j->cell, j->delta, (uint32_t)j->id, &ok);
	j->done++;
	if (!ok)
		return MCC_TASK_FAILED;
	return j->done < j->steps ? MCC_TASK_YIELDED : MCC_TASK_DONE;
}

static int thr_tick_store(MccTask *t) {
	ThrJob *j = (ThrJob *)t->ctx;
	thr_stamp(j->id);
	if (!mcc_thread_cell_store(j->g, j->lane, j->cell, j->delta, (uint32_t)j->id))
		return MCC_TASK_FAILED;
	return MCC_TASK_DONE;
}

static int thr_tick_write(MccTask *t) {
	ThrJob *j = (ThrJob *)t->ctx;
	MccEffect e;
	unsigned char b = (unsigned char)('a' + j->id);
	thr_stamp(j->id);
	memset(&e, 0, sizeof e);
	e.kind = MCC_EFFECT_WRITE;
	e.space = MCC_EFFECT_SPACE_FD;
	e.chan = j->fd;
	e.site = (uint32_t)j->id;
	e.lane = j->lane;
	e.width = 1;
	e.flags = MCC_EFFECT_F_NOUNDO;
	if (j->g->log && j->g->log->mode == MCC_EFFECT_RECORD)
		mcc_effect_record(j->g->log, &e, &b, 1);
	return MCC_TASK_DONE;
}

static int thr_cell_nonzero(void *user) {
	ThrJob *j = (ThrJob *)user;
	return j->g->cell[j->cell] != 0;
}

static int g_thr_world_have;
static int g_thr_world_calls;

static int thr_world_ready(void *user) {
	(void)user;
	return g_thr_world_have > 0;
}

static int thr_world_refill(void *user) {
	(void)user;
	if (++g_thr_world_calls >= 2)
		g_thr_world_have = 1;
	return 1;
}

static int thr_world_dry(void *user) {
	(void)user;
	g_thr_world_calls++;
	return 0;
}

static int thr_supported_n(void) {
	int i, n = 0;
	for (i = 0; i < MCC_THREAD_TAB_N; i++)
		if (mcc_thread_op_supported(mcc_thread_tab[i].op))
			n++;
	return n;
}

static int thr_unclassed_n(void) {
	int i, n = 0;
	for (i = 0; i < MCC_THREAD_TAB_N; i++)
		if (mcc_thread_sym_class(mcc_thread_tab[i].name) == MCC_THREAD_NONE)
			n++;
	return n;
}

static void thr_classify_cells(void) {
	CHECK(mcc_thread_classify("pthread_create") == MCC_THR_CREATE,
				"pthread_create is recognised as publish");
	CHECK(mcc_thread_classify("thrd_create") == MCC_THR_CREATE,
				"the C11 spelling maps to the same construct");
	CHECK(mcc_thread_classify("pthread_join") == MCC_THR_JOIN,
				"pthread_join is recognised as a dependency edge");
	CHECK(mcc_thread_classify("pthread_mutex_lock") == MCC_THR_LOCK,
				"pthread_mutex_lock is recognised");
	CHECK(mcc_thread_classify("pthread_cond_wait") == MCC_THR_WAIT,
				"pthread_cond_wait is recognised as a readiness predicate");
	CHECK(mcc_thread_classify("pthread_mutex_init") == MCC_THR_NOP,
				"a mutex constructor has no engine meaning and is erased");
	CHECK(mcc_thread_classify("pthread_barrier_wait") == MCC_THR_BARRIER &&
						!mcc_thread_op_supported(MCC_THR_BARRIER),
				"a recognised primitive with no translation says so rather than passing through");
	CHECK(mcc_thread_classify("pthread_create_helper") == MCC_THR_NONE,
				"a name that merely contains a primitive is not one");
	CHECK(mcc_thread_classify("memcpy") == MCC_THR_NONE,
				"an unrelated callee is not classified");
	CHECK(mcc_thread_classify(NULL) == MCC_THR_NONE,
				"an unresolved callee is not classified");
	CHECK(thr_supported_n() > 0 && thr_supported_n() < MCC_THREAD_TAB_N,
				"the table distinguishes translated primitives from recognised-only ones");
	CHECK(thr_unclassed_n() == 0,
				"every name the op table recognises is also named by the refusal "
				"classifier, so the two views cannot drift apart");
	CHECK(mcc_thread_sym_class("pthread_self") == MCC_THREAD_PTHREAD &&
						mcc_thread_sym_class("sched_yield") == MCC_THREAD_PTHREAD &&
						mcc_thread_sym_class("thrd_create") == MCC_THREAD_C11 &&
						mcc_thread_sym_class("__atomic_load_4") == MCC_THREAD_ATOMIC,
				"the refusal classifier reads its families off the one table plus the "
				"open-ended prefixes");
	CHECK(mcc_thread_sym_class("memcpy") == MCC_THREAD_NONE &&
						mcc_thread_sym_class(NULL) == MCC_THREAD_NONE,
				"and classifies nothing else");
}

static void suite_thread(void) {
	MccDepGraph g;
	MccDepNode n[8];
	ThrJob j[8];
	MccEffectLog log;
	int i, a, b, k;

	thr_classify_cells();

	g_thr_norder = 0;
	mcc_dep_init(&g);
	thr_job(&j[0], &g, 0, 2);
	thr_job(&j[1], &g, 1, 3);
	thr_job(&j[2], &g, 2, 1);
	a = mcc_dep_publish(&g, &n[0], thr_tick, &j[0]);
	b = mcc_dep_publish(&g, &n[1], thr_tick, &j[1]);
	k = mcc_dep_publish(&g, &n[2], thr_tick, &j[2]);
	CHECK(a == 0 && b == 1 && k == 2, "create publishes one node per thread");
	CHECK(mcc_dep_concurrent(&g, a, b) == 1,
				"two created threads carry no derived edge between them");
	if (!g_mutate) {
		CHECK(mcc_dep_edge(&g, a, k) == 1, "join adds an edge from the joined work");
		CHECK(mcc_dep_edge(&g, b, k) == 1, "the second join adds its edge too");
	}
	CHECK(mcc_dep_run(&g, 1000) == MCC_DEP_OK, "the joined graph drains");
	CHECK(g.admitted == 3, "every published node was admitted exactly once");
	CHECK(thr_last(0) < thr_first(2) && thr_last(1) < thr_first(2),
				"the continuation runs only after both joined threads finished");
	CHECK(mcc_dep_concurrent(&g, a, b) == 1,
				"joining to a common continuation orders neither worker before the other");

	g_thr_norder = 0;
	mcc_dep_init(&g);
	thr_job(&j[0], &g, 0, 2);
	thr_job(&j[1], &g, 1, 2);
	a = mcc_dep_publish(&g, &n[0], thr_tick, &j[0]);
	b = mcc_dep_publish(&g, &n[1], thr_tick, &j[1]);
	CHECK(mcc_dep_mutex(&g, a, b, 1) == 0,
				"a lock whose protected regions are derivably independent adds no edge");
	CHECK(g.elided == 1 && g.serialized == 0 && g.edges == 0,
				"the elided lock is counted and the graph stays edge-free");
	CHECK(mcc_dep_concurrent(&g, a, b) == 1,
				"both protected regions may still run concurrently");
	CHECK(mcc_dep_run(&g, 1000) == MCC_DEP_OK, "the unserialized pair drains");
	CHECK(thr_last(0) > thr_first(1),
				"the independent regions really did overlap rather than run in sequence");

	g_thr_norder = 0;
	mcc_dep_init(&g);
	thr_job(&j[0], &g, 0, 2);
	thr_job(&j[1], &g, 1, 2);
	a = mcc_dep_publish(&g, &n[0], thr_tick, &j[0]);
	b = mcc_dep_publish(&g, &n[1], thr_tick, &j[1]);
	if (!g_mutate)
		CHECK(mcc_dep_mutex(&g, a, b, 0) == 1,
					"a lock over a conflicting region derives a serializing edge");
	CHECK(mcc_dep_run(&g, 1000) == MCC_DEP_OK, "the serialized pair drains");
	CHECK(thr_last(0) < thr_first(1),
				"the serialized region runs entirely before the region it conflicts with");

	mcc_dep_init(&g);
	thr_job(&j[0], &g, 0, 1);
	thr_job(&j[1], &g, 1, 1);
	a = mcc_dep_publish(&g, &n[0], thr_tick, &j[0]);
	b = mcc_dep_publish(&g, &n[1], thr_tick, &j[1]);
	CHECK(mcc_dep_mutex(&g, a, b, -1) == 1,
				"a lock whose regions cannot be decided serializes rather than eliding");
	CHECK(g.elided == 0 && g.serialized == 1,
				"an undecided pair is never counted as independent");

	memset(&log, 0, sizeof log);
	mcc_effect_log_init(&log);

	g_thr_norder = 0;
	mcc_dep_init(&g);
	g.log = &log;
	thr_job(&j[0], &g, 0, 1);
	thr_job(&j[1], &g, 1, 1);
	j[1].delta = 1;
	a = mcc_dep_publish(&g, &n[0], thr_tick, &j[0]);
	b = mcc_dep_publish(&g, &n[1], thr_tick_store, &j[1]);
	mcc_dep_pred(&g, a, thr_cell_nonzero, &j[0], 0);
	CHECK(mcc_dep_concurrent(&g, a, b) == 1,
				"a condvar wait adds a readiness predicate, not an edge");
	CHECK(mcc_dep_run(&g, 1000) == MCC_DEP_OK,
				"the waiter is admitted once the signaller made its predicate hold");
	CHECK(g.stalls > 0, "the waiter was actually held back at least once");
	CHECK(thr_first(1) < thr_first(0), "the signal precedes the wakeup");

	mcc_dep_init(&g);
	g.log = &log;
	thr_job(&j[0], &g, 0, 1);
	a = mcc_dep_publish(&g, &n[0], thr_tick, &j[0]);
	mcc_dep_pred(&g, a, thr_cell_nonzero, &j[0], 0);
	CHECK(mcc_dep_run(&g, 1000) == MCC_DEP_STUCK,
				"a wait no work can satisfy is reported stuck, never silently skipped");

	g_thr_world_have = 0;
	g_thr_world_calls = 0;
	mcc_dep_init(&g);
	thr_job(&j[0], &g, 0, 1);
	a = mcc_dep_publish(&g, &n[0], thr_tick, &j[0]);
	mcc_dep_pred(&g, a, thr_world_ready, &j[0], 1);
	mcc_dep_world_source(&g, thr_world_refill, NULL);
	CHECK(mcc_dep_run(&g, 1000) == MCC_DEP_OK,
				"a thread blocked on the world runs once the host refill supplies it");
	CHECK(g.refills >= 2, "the readiness source outside the graph was polled");

	g_thr_world_have = 0;
	g_thr_world_calls = 0;
	mcc_dep_init(&g);
	thr_job(&j[0], &g, 0, 1);
	a = mcc_dep_publish(&g, &n[0], thr_tick, &j[0]);
	mcc_dep_pred(&g, a, thr_world_ready, &j[0], 1);
	mcc_dep_world_source(&g, thr_world_dry, NULL);
	CHECK(mcc_dep_run(&g, 1000) == MCC_DEP_STUCK,
				"a world source that never supplies leaves the graph stuck rather than done");
	CHECK(g_thr_world_calls > 0, "the dry world source was actually consulted");

	g_thr_norder = 0;
	mcc_dep_init(&g);
	thr_job(&j[0], &g, 0, 1);
	a = mcc_dep_publish(&g, &n[0], thr_tick, &j[0]);
	CHECK(mcc_dep_detach(&g, a, 1) == 1,
				"a detached slice with a derivable finite bound is accepted");
	CHECK(mcc_dep_run(&g, 1000) == MCC_DEP_OK,
				"an accepted detached slice is joined implicitly at exit");

	mcc_dep_init(&g);
	thr_job(&j[0], &g, 0, 1);
	a = mcc_dep_publish(&g, &n[0], thr_tick, &j[0]);
	CHECK(mcc_dep_detach(&g, a, 0) == 0,
				"a detached slice with no derivable bound is refused");
	CHECK(g.verdict == MCC_DEP_REFUSED &&
						g.refusal == MCC_DEP_R_UNBOUNDED_DETACH,
				"the refusal names the unbounded detached slice");
	CHECK(mcc_dep_run(&g, 1000) == MCC_DEP_REFUSED,
				"a refused graph is never run as if it had been translated");

	g_thr_norder = 0;
	mcc_effect_log_clear(&log);
	log.mode = MCC_EFFECT_RECORD;
	mcc_dep_init(&g);
	g.log = &log;
	thr_job(&j[0], &g, 0, 1);
	thr_job(&j[1], &g, 1, 1);
	j[0].fd = 1;
	j[1].fd = 1;
	a = mcc_dep_publish(&g, &n[0], thr_tick_write, &j[0]);
	b = mcc_dep_publish(&g, &n[1], thr_tick_write, &j[1]);
	CHECK(mcc_dep_run(&g, 1000) == MCC_DEP_OK, "both writers ran");
	{
		int ca = -1, cb = -1;
		CHECK(mcc_thread_output_conflict(&g, &log, &ca, &cb) > 0,
					"two concurrent threads writing one channel is an observable interleaving");
		CHECK(ca >= 0 && cb >= 0 && ca != cb,
					"the interleaving names the two nodes it found");
		CHECK(g.verdict == MCC_DEP_REFUSED &&
							g.refusal == MCC_DEP_R_INTERLEAVED_OUTPUT,
					"that interleaving is refused by name rather than silently ordered");
	}

	g_thr_norder = 0;
	mcc_effect_log_clear(&log);
	log.mode = MCC_EFFECT_RECORD;
	mcc_dep_init(&g);
	g.log = &log;
	thr_job(&j[0], &g, 0, 1);
	thr_job(&j[1], &g, 1, 1);
	j[0].fd = 1;
	j[1].fd = 1;
	a = mcc_dep_publish(&g, &n[0], thr_tick_write, &j[0]);
	b = mcc_dep_publish(&g, &n[1], thr_tick_write, &j[1]);
	mcc_dep_edge(&g, a, b);
	CHECK(mcc_dep_run(&g, 1000) == MCC_DEP_OK, "the ordered writers ran");
	CHECK(mcc_thread_output_conflict(&g, &log, NULL, NULL) == 0,
				"writers the graph already orders are not an interleaving");

	g_thr_norder = 0;
	mcc_effect_log_clear(&log);
	log.mode = MCC_EFFECT_RECORD;
	mcc_dep_init(&g);
	g.log = &log;
	for (i = 0; i < 3; i++) {
		thr_job(&j[i], &g, i, 2);
		j[i].delta = 1;
		mcc_dep_publish(&g, &n[i], thr_tick_cell, &j[i]);
	}
	CHECK(mcc_dep_run(&g, 1000) == MCC_DEP_OK,
				"the lane-visible counter graph drains");
	CHECK(g.cell[0] == 6, "every lane's increment landed on the shared cell");
	CHECK(log.n == 12, "each increment recorded one cell load and one cell store");

	{
		int64_t after = g.cell[0];
		log.mode = MCC_EFFECT_OFF;
		mcc_dep_init(&g);
		g.log = &log;
		g.cell[0] = after;
		for (i = 0; i < 3; i++) {
			thr_job(&j[i], &g, i, 2);
			j[i].delta = 1;
			mcc_dep_publish(&g, &n[i], thr_tick_cell, &j[i]);
		}
		mcc_effect_replay_begin(&log);
		CHECK(mcc_dep_run(&g, 1000) == MCC_DEP_OK, "the replayed graph drains");
		CHECK(!mcc_effect_diverged(&log),
					"the replay asked for exactly the recorded cell traffic");
		CHECK(mcc_effect_replay_end(&log) == 1, "the replay consumed the whole log");
		CHECK(g.cell[0] == after,
					"a replayed lane-visible cell is not advanced a second time");
	}

	if (g_mutate && log.n > 2) {
		log.e[log.n / 2].value ^= 1;
		mcc_dep_init(&g);
		g.log = &log;
		for (i = 0; i < 3; i++) {
			thr_job(&j[i], &g, i, 2);
			j[i].delta = 1;
			mcc_dep_publish(&g, &n[i], thr_tick_cell, &j[i]);
		}
		mcc_effect_replay_begin(&log);
		mcc_dep_run(&g, 1000);
		CHECK(mcc_effect_diverged(&log),
					"a perturbed cell log is caught by the replay");
	}
	mcc_effect_log_free(&log);

	fprintf(stderr,
					"slicerun: thread map -- %d primitives classified, %d with an engine "
					"translation\n",
					MCC_THREAD_TAB_N, thr_supported_n());
}

int main(int argc, char **argv) {
	const char *only = NULL;
	const char *arenas = NULL;
	long limit = 0;
	int quiet = 0, i, fmt_verdict = 0;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--fmt-verdict"))
			fmt_verdict = 1;
	}
	if (fmt_verdict)
		return fmt_verdict_mode();

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
		else if (!strcmp(argv[i], "--cref") && i + 1 < argc)
			g_cref_dir = argv[++i];
		else if (!strcmp(argv[i], "--cref-prefix") && i + 1 < argc)
			g_cref_pfx = argv[++i];
		else if (!strcmp(argv[i], "--refusals"))
			g_refusals = 1;
		else if (!strcmp(argv[i], "--census"))
			g_census = 1;
		else if (!strcmp(argv[i], "--noslot"))
			g_noslot = 1;
		else if (!strcmp(argv[i], "--no-globals"))
			g_noglob = 1;
		else if (!strcmp(argv[i], "--fmt-cost-report"))
			g_fmt_report = 1;
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
		else if (argv[i][0])
			only = argv[i];
		/* An empty argument is not a suite name. CMake generator expressions that
		 * evaluate false expand to an empty *argument* rather than to nothing, so
		 * `slicerun mem "" ""` is what the slice/<suite> cells actually ran --
		 * and the last non-flag argument wins, so `only` became "", every suite
		 * test below failed to match, and the run reported "0 checks, 0 failures"
		 * with exit 0. Eight cells were green because they executed nothing. */
	}

	probe_device();
	if (g_device_or_skip && !g_have_device) {
		fprintf(stderr, "slicerun: no usable device, and this run needs one\n");
		return 77;
	}
	mcc_slice_set_mutate(g_mutate);
	ast_eval_slice_obj_fn = slicerun_obj;
	if (!g_noglob)
		ast_eval_slice_reloc_fn = slicerun_reloc;
	frame_ptr_arm();
	(void)g_lax;

	if (g_cref_dir) {
		char p[1024];
		if (!arenas) {
			fprintf(stderr, "slicerun: --cref needs --arenas\n");
			return 2;
		}
		snprintf(p, sizeof p, "%s/expect.txt", g_cref_dir);
		g_cref_exp = fopen(p, "w");
		if (!g_cref_exp) {
			fprintf(stderr, "slicerun: cannot write %s\n", p);
			return 2;
		}
		g_cref_mutate = g_mutate;
	}

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
		{
			int _rc = arena_mode(arenas, limit, quiet);
			/* Same rule as the suite path below: if the backend emitted nothing
			 * this mode compares, say so with 77 rather than returning a clean 0
			 * that the drivers would read as "compared and agreed". */
			if (_rc == 0 && g_unsupported && !g_failures) {
				fprintf(stderr, "SKIP: slicerun --arenas: nothing this backend emits "
												"was exercised (device=%s)\n",
								g_devname);
				return 77;
			}
			return _rc;
		}
	}

	/* A named suite that matches nothing must be an error. Without this the
	 * ladder below simply runs no suite, main returns g_failures ? 1 : 0 with
	 * g_failures still 0, and the cell reports a pass having compared nothing --
	 * which is how eight slice/<suite> cells stayed green while executing
	 * nothing at all. A typo in a driver script deserves the same treatment. */
	if (only) {
		static const char *const SUITES[] = {
				"task",  "work",   "cpu",	 "gpu",		"bytes",	"wide64",
				"f64",	 "ops",		"frame", "mem",		"deref",	"fmt",
				"fault", "sched",	"ext",	 "rwstore", "arrow",	"effect",
				"hostimport", "thread", "lost", "quiesce"};
		size_t si;
		int known = 0;
		for (si = 0; si < sizeof SUITES / sizeof SUITES[0]; si++)
			if (!strcmp(only, SUITES[si])) {
				known = 1;
				break;
			}
		if (!known) {
			fprintf(stderr, "slicerun: unknown suite '%s'; nothing would run and "
											"a run that compared nothing must not report success\n",
							only);
			return 2;
		}
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
	if (!only || !strcmp(only, "f64"))
		suite_f64();
	if (!only || !strcmp(only, "ops"))
		suite_ops();
	if (!only || !strcmp(only, "frame"))
		suite_frame();
	if (!only || !strcmp(only, "mem"))
		suite_mem();
	if (!only || !strcmp(only, "deref"))
		suite_deref();
	if (!only || !strcmp(only, "ext"))
		suite_ext();
	if (!only || !strcmp(only, "arrow"))
		suite_arrow();
	if (!only || !strcmp(only, "fmt"))
		suite_fmt();
	if (!only || !strcmp(only, "rwstore"))
		suite_rwstore();
	if (only && !strcmp(only, "fault"))
		suite_fault();
	if (only && !strcmp(only, "lost"))
		suite_lost();
	if (only && !strcmp(only, "quiesce"))
		suite_quiesce();
	if (only && !strcmp(only, "hostimport"))
		suite_hostimport();
	if (!only || !strcmp(only, "sched"))
		suite_sched();
	if (!only || !strcmp(only, "effect"))
		suite_effect();
	if (!only || !strcmp(only, "thread"))
		suite_thread();

	fprintf(stderr, "slicerun: %d checks, %d failures, device=%s available=%d "
									"dispatches=%ld\n",
					g_checks, g_failures, g_devname, g_have_device,
					mcc_slice_dispatches());

	if (!g_have_device && g_device_required) {
		fprintf(stderr, "FAIL slicerun: a device is required and none was usable\n");
		return 1;
	}
	if (only && (!strcmp(only, "rwstore") || !strcmp(only, "gpu") ||
							 !strcmp(only, "wide64") ||
							 !strcmp(only, "ops") || !strcmp(only, "fault") ||
			 !strcmp(only, "mem") || !strcmp(only, "deref") ||
			 !strcmp(only, "ext") || !strcmp(only, "arrow") ||
			 !strcmp(only, "f64") || !strcmp(only, "fmt") ||
			 !strcmp(only, "hostimport")) &&
			!g_have_device) {
		fprintf(stderr, "SKIP: slicerun %s is a device differential and no usable "
										"device was found on this host (device=%s)\n",
						only, g_devname);
		return 77;
	}
	if (g_unsupported && !g_failures) {
		fprintf(stderr, "SKIP: slicerun %s: nothing this backend emits was "
										"exercised (device=%s)\n",
						only ? only : "(all)", g_devname);
		return 77;
	}
	if (g_mutate && g_f64_no_diff && !g_failures) {
		fprintf(stderr, "SKIP: slicerun f64 --mutate has no fp64 kernel to perturb "
										"on a device without shaderFloat64 (device=%s); reporting the "
										"mutant as survived would grade the device, not the "
										"differential\n",
						g_devname);
		return 77;
	}
	return g_failures ? 1 : 0;
}
