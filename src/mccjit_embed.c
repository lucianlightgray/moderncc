#ifdef MCC_EMBED_JIT

#include "mcc.h"
#include "mccast.h"
#include "mccgate.h"
#include "algorithms/jit.h"
#include "mccjit_internal.h"
#include "mccstats.h"

#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef MCC_JIT_DEFAULT
#define MCC_JIT_DEFAULT 1
#endif

MCCJIT_LOCAL unsigned mccjit_role_for_base(int t) { MCC_TRACE("enter\n");
	switch (t & VT_BTYPE) { MCC_TRACE("br\n");
	case VT_PTR:
		return MCCJIT_ROLE_PTR;
	case VT_FUNC:
		return MCCJIT_ROLE_FUNC;
	case VT_STRUCT:
		return MCCJIT_ROLE_STRUCT;
	default:
		return MCCJIT_ROLE_PLAIN;
	}
}

void ast_reemit_extern(Sym *sym, AstArena *ast);
int mccjit_ast_spec_fold(AstArena *ast, int off, int64_t val);
void mcc_jit_publish(void **slot, void *variant);
void mcc_jit_export_local(MCCState *s1, const char *name);

MCCJIT_LOCAL unsigned char *mccjit_last_blob;
MCCJIT_LOCAL size_t mccjit_last_len;
MCCJIT_LOCAL MCCState *mccjit_last_state;
MCCJIT_LOCAL int mccjit_last_purity;

MCCJIT_LOCAL uint32_t mccjit_last_nparam;
MCCJIT_LOCAL uint32_t mccjit_last_param_t[MCCJIT_KGC_MAXARG];
MCCJIT_LOCAL int mccjit_last_ret_wide;
MCCJIT_LOCAL int mccjit_last_kgc_ok;
MCCJIT_LOCAL int mccjit_last_allfp;
MCCJIT_LOCAL int mccjit_last_mixed;
MCCJIT_LOCAL uint32_t mccjit_last_ngp;
MCCJIT_LOCAL uint32_t mccjit_last_nsse;
MCCJIT_LOCAL int mccjit_last_ret_fp;

static int mccjit_type_wide(int t) { MCC_TRACE("enter\n");
	switch (t & VT_BTYPE) { MCC_TRACE("br\n");
	case VT_LLONG:
	case VT_PTR:
	case VT_FUNC:
		return 1;
	case VT_INT:
		return (t & VT_LONG) ? 1 : 0;
	default:
		return 0;
	}
}

static int mccjit_type_gp(int t) { MCC_TRACE("enter\n");
	switch (t & VT_BTYPE) { MCC_TRACE("br\n");
	case VT_BOOL:
	case VT_BYTE:
	case VT_SHORT:
	case VT_INT:
	case VT_LLONG:
	case VT_PTR:
	case VT_FUNC:
		return 1;
	default:
		return 0;
	}
}

static int mccjit_type_fp(int t) { MCC_TRACE("enter\n"); return (t & VT_BTYPE) == VT_DOUBLE; }

MCCJIT_LOCAL uint64_t mccjit_salt_witness(void) { MCC_TRACE("enter\n");
	uint64_t h = 0xcbf29ce484222325ull;
	const char *s;
	(void)s;
#ifdef MCC_VERSION_STR
	for (s = MCC_VERSION_STR; *s; s++)
		{ MCC_TRACE("br\n"); h = (h ^ (unsigned char)*s) * 0x100000001b3ull; }
#endif
#ifdef MCC_CONFIG_TRIPLET
	for (s = MCC_CONFIG_TRIPLET; *s; s++)
		{ MCC_TRACE("br\n"); h = (h ^ (unsigned char)*s) * 0x100000001b3ull; }
#endif
	return h;
}


static void mccjit_perf_map_emit(MCCState *js, const char *name, void *addr) { MCC_TRACE("enter\n");
	char path[64];
	FILE *f;
	size_t size = 0;
	int si;
	if (!getenv("MCC_JIT_PERF_MAP") || !addr || !name || !name[0] || !js ||
			!js->symtab)
		{ MCC_TRACE("br\n"); return; }
	si = find_elf_sym(js->symtab, name);
	if (si > 0)
		{ MCC_TRACE("br\n"); size = (size_t)((ElfSym *)js->symtab->data)[si].st_size; }
	if (!size)
		{ MCC_TRACE("br\n"); size = 16; }
	snprintf(path, sizeof path, "/tmp/perf-%d.map", (int)getpid());
	f = fopen(path, "a");
	if (!f)
		{ MCC_TRACE("br\n"); return; }
	fprintf(f, "%lx %lx %s\n", (unsigned long)(uintptr_t)addr,
					(unsigned long)size, name);
	fclose(f);
}

static int mccjit_internal_compile;

static void *mccjit_recompile_common(const void *buf, size_t len, int do_spec,
																		 int param_index, int64_t const_val) { MCC_TRACE("enter\n");
	MccjitIntent it;
	MCCState *js;
	Sym *sym;
	void *entry = NULL;

	if (mcc_stats_mask)
		{ MCC_TRACE("br\n"); mcc_stats_jit_recompile(); }
	js = mcc_new();
	if (!js)
		{ MCC_TRACE("br\n"); return NULL; }
	js->optimize = 0;
	js->nostdlib = 1;
	mcc_set_output_type(js, MCC_OUTPUT_MEMORY);

	mcc_enter_state(js);
	mccpp_new(js);
	mccgen_init(js);
	anon_sym = SYM_FIRST_ANOM;
	funcname = "";
	func_ind = -1;

	if (mccjit_intent_deserialize(buf, len, &it) != 0) { MCC_TRACE("br\n");
		mcc_exit_state(js);
		mcc_delete(js);
		return NULL;
	}

	if (it.has_external)
		{ MCC_TRACE("br\n"); js->nostdlib = 0; }

	mccjit_last_purity = ast_fn_purity(it.arena);

	{
		uint32_t qi;
		int allfp, scalar_ok, allgp, ret_gp, ret_fp;
		uint32_t ngp = 0, nsse = 0;
		mccjit_last_nparam = it.nparam;
		mccjit_last_ret_wide = mccjit_type_wide((int)it.ret_type_t);
		/* K4A all-double (SSE) class: 1-6 double params + a double return. */
		allfp = (it.nparam >= 1 && it.nparam <= MCCJIT_KGC_MAXARG &&
						 ((int)it.ret_type_t & VT_BTYPE) == VT_DOUBLE);
		for (qi = 0; allfp && qi < it.nparam && qi < MCCJIT_KGC_MAXARG; qi++)
			{ MCC_TRACE("br\n"); if (((int)it.param_type_t[qi] & VT_BTYPE) != VT_DOUBLE)
				{ MCC_TRACE("br\n"); allfp = 0; } }
		mccjit_last_allfp = allfp;
		for (qi = 0; qi < MCCJIT_KGC_MAXARG; qi++)
			{ MCC_TRACE("br\n"); mccjit_last_param_t[qi] = 0; }
		/* K4A/L12A scalar mixed GP+FP: each param and the return is either a GP
			 (int/ptr, non-bitfield) or a scalar double. SysV assigns GP args to
			 rdi.. and SSE args to xmm0.. with independent counters, so the two
			 per-class occupancy counts fully determine the marshalling — no
			 per-arg class vector is needed. all-GP routes to the n-stub and
			 all-double+double-ret to the fp-stub (both unchanged); every other
			 scalar case routes to the new mixed stub. */
		ret_gp = mccjit_type_gp((int)it.ret_type_t) && !(it.ret_type_t & VT_BITFIELD);
		ret_fp = mccjit_type_fp((int)it.ret_type_t);
		scalar_ok = (it.func_type != FUNC_ELLIPSIS && it.nparam >= 1 &&
								 it.nparam <= MCCJIT_KGC_MAXARG && (ret_gp || ret_fp));
		for (qi = 0; qi < it.nparam && qi < MCCJIT_KGC_MAXARG; qi++) { MCC_TRACE("br\n");
			int pt = (int)it.param_type_t[qi];
			mccjit_last_param_t[qi] = it.param_type_t[qi];
			if (mccjit_type_gp(pt) && !(pt & VT_BITFIELD))
				{ MCC_TRACE("br\n"); ngp++; }
			else if (mccjit_type_fp(pt))
				{ MCC_TRACE("br\n"); nsse++; }
			else
				{ MCC_TRACE("br\n"); scalar_ok = 0; }
		}
		mccjit_last_ret_fp = ret_fp;
		mccjit_last_ngp = ngp;
		mccjit_last_nsse = nsse;
		allgp = scalar_ok && nsse == 0 && !ret_fp;
		mccjit_last_mixed = scalar_ok && !allfp && !allgp;
		mccjit_last_kgc_ok = scalar_ok;
	}

	if (do_spec && param_index >= 0 && (uint32_t)param_index < it.nparam)
		{ MCC_TRACE("br\n"); mccjit_ast_spec_fold(it.arena, (int)it.param_off[param_index], const_val); }

	sym = mccjit_rebuild_sym(&it);
	mccjit_internal_compile = 1;
	if (sym) { MCC_TRACE("br\n");
		ast_fconst_reuse_disable(1);
		ast_reemit_extern(sym, it.arena);
		ast_fconst_reuse_disable(0);
	}
	mcc_exit_state(js);

	if (sym && mcc_relocate(js) == 0)
		{ MCC_TRACE("br\n"); entry = mcc_get_symbol(js, it.fn_name); }
	mccjit_internal_compile = 0;

	if (entry)
		{ MCC_TRACE("br\n"); mccjit_perf_map_emit(js, it.fn_name, entry); }

	mccjit_intent_release(&it);

	if (entry) { MCC_TRACE("br\n");
		mccjit_last_state = js;
	} else { MCC_TRACE("br\n");
		mcc_delete(js);
	}
	return entry;
}

MCCJIT_LOCAL void *mcc_jit_recompile_blob(const void *buf, size_t len) { MCC_TRACE("enter\n");
	return mccjit_recompile_common(buf, len, 0, -1, 0);
}

MCCJIT_LOCAL void *mcc_jit_recompile_blob_spec(const void *buf, size_t len,
																							 int param_index, int64_t const_val) { MCC_TRACE("enter\n");
	return mccjit_recompile_common(buf, len, 1, param_index, const_val);
}

MCCJIT_LOCAL void *mcc_jit_recompile(Sym *sym, const void *ctxkey) { MCC_TRACE("enter\n");
	(void)sym;
	(void)ctxkey;
	if (!mccjit_last_blob)
		{ MCC_TRACE("br\n"); return NULL; }
	return mcc_jit_recompile_blob(mccjit_last_blob, mccjit_last_len);
}

void mccjit_embed_stash_leaf(AstArena *ast, Sym *sym) { MCC_TRACE("enter\n");
	MccjitBuf b;
	if (!ast || !sym)
		{ MCC_TRACE("br\n"); return; }
	mccjit_buf_init(&b);
	if (mccjit_intent_serialize(ast, sym, &b) != 0) { MCC_TRACE("br\n");
		mccjit_buf_free(&b);
		return;
	}
	mcc_free(mccjit_last_blob);
	mccjit_last_blob = b.data;
	mccjit_last_len = b.len;
}

typedef struct MccjitEmbedFn {
	char *name;
	unsigned char *blob;
	size_t len;
	struct MccjitEmbedFn *next;
} MccjitEmbedFn;

MCCJIT_LOCAL MccjitEmbedFn *mccjit_embed_fns;

static char **mccjit_export_names;
static int mccjit_export_n, mccjit_export_cap;

MCCJIT_LOCAL void mccjit_note_export_name(const char *name) { MCC_TRACE("enter\n");
	int i;
	if (!name || !name[0])
		{ MCC_TRACE("br\n"); return; }
	for (i = 0; i < mccjit_export_n; i++)
		if (!strcmp(mccjit_export_names[i], name))
			{ MCC_TRACE("br\n"); return; }
	if (mccjit_export_n == mccjit_export_cap) { MCC_TRACE("br\n");
		int nc = mccjit_export_cap ? mccjit_export_cap * 2 : 32;
		char **np = mcc_realloc(mccjit_export_names, (size_t)nc * sizeof(char *));
		if (!np) { MCC_TRACE("br\n"); return; }
		mccjit_export_names = np;
		mccjit_export_cap = nc;
	}
	mccjit_export_names[mccjit_export_n++] = mcc_strdup(name);
}

void mccjit_embed_note(const char *name, AstArena *ast, Sym *sym) { MCC_TRACE("enter\n");
	MccjitBuf b;
	MccjitEmbedFn *e;
	if (!name || !name[0] || !ast || !sym || mccjit_internal_compile)
		{ MCC_TRACE("br\n"); return; }
	for (e = mccjit_embed_fns; e; e = e->next)
		{ MCC_TRACE("br\n"); if (!strcmp(e->name, name))
			{ MCC_TRACE("br\n"); return; } }
	mccjit_buf_init(&b);
	if (mccjit_intent_serialize(ast, sym, &b) != 0) { MCC_TRACE("br\n");
		mccjit_buf_free(&b);
		return;
	}
	e = mcc_mallocz(sizeof *e);
	if (!e) { MCC_TRACE("br\n");
		mccjit_buf_free(&b);
		return;
	}
	e->name = mcc_strdup(name);
	e->blob = b.data;
	e->len = b.len;
	e->next = mccjit_embed_fns;
	mccjit_embed_fns = e;
}

#if defined(__x86_64__)
#include <sys/mman.h>
static void *mccjit_make_trampoline(void *variant) { MCC_TRACE("enter\n");
	unsigned char *p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
													MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return variant; }
	p[0] = 0xc9;
	p[1] = 0x48;
	p[2] = 0xb8;
	memcpy(p + 3, &variant, 8);
	p[11] = 0xff;
	p[12] = 0xe0;
	return p;
}
#else
static void *mccjit_make_trampoline(void *variant) { MCC_TRACE("enter\n"); return variant; }
#endif

static double mccjit_elapsed(const struct timespec *t0) { MCC_TRACE("enter\n");
	struct timespec t1;
	if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0)
		{ MCC_TRACE("br\n"); return -1.0; }
	return (double)(t1.tv_sec - t0->tv_sec) +
				 (double)(t1.tv_nsec - t0->tv_nsec) / 1000000000.0;
}

static void *mccjit_make_kgc_stub_n(void *variant, void *baseline, int memoize_ok,
																		const uint32_t *param_t, uint32_t nargs,
																		int ret_wide);
static void *mccjit_make_kgc_stub_fp(void *variant, void *baseline,
																		 int memoize_ok, uint32_t nargs);
static void *mccjit_make_kgc_stub_mixed(void *variant, void *baseline,
																				int memoize_ok, uint32_t ngp,
																				uint32_t nsse, int ret_fp, int ret_wide);

static void mccjit_boot_swap_run(void **slot, const void *blob, unsigned long len,
																 unsigned long max_duration, const char *mode,
																 const struct timespec *t0, int timed) { MCC_TRACE("enter\n");
	void *variant = NULL;
	void *baseline = NULL;
	void *aot_init = slot ? *slot : NULL;
	void *entry = NULL;
	int over = 0;
	int skipped = 0;
	int routed = 0;
	int no_kgc = getenv("MCC_JIT_NO_KGC") != NULL;
	int spec_wrong = getenv("MCC_JIT_SPEC_WRONG") != NULL;
	if (timed && max_duration && mccjit_elapsed(t0) > (double)max_duration) { MCC_TRACE("br\n");
		skipped = 1;
	} else { MCC_TRACE("br\n");
		variant = spec_wrong
									? mcc_jit_recompile_blob_spec(blob, (size_t)len, 0, 7)
									: mcc_jit_recompile_blob(blob, (size_t)len);
		if (variant && !no_kgc && mccjit_last_kgc_ok) { MCC_TRACE("br\n");
			int memoize_ok;
			uint32_t nargs = mccjit_last_nparam;
			int ret_wide = mccjit_last_ret_wide;
			int all_fp = mccjit_last_allfp;
			int mixed = mccjit_last_mixed;
			uint32_t ngp = mccjit_last_ngp;
			uint32_t nsse = mccjit_last_nsse;
			int ret_fp = mccjit_last_ret_fp;
			uint32_t ptypes[MCCJIT_KGC_MAXARG];
			uint32_t qi;
			for (qi = 0; qi < MCCJIT_KGC_MAXARG; qi++)
				{ MCC_TRACE("br\n"); ptypes[qi] = mccjit_last_param_t[qi]; }
			baseline = mcc_jit_recompile_blob(blob, (size_t)len);
			memoize_ok = (mccjit_last_purity == AST_PURITY_TIER0);
			if (baseline) { MCC_TRACE("br\n");
				entry = mixed ? mccjit_make_kgc_stub_mixed(variant, baseline, memoize_ok,
																									 ngp, nsse, ret_fp, ret_wide)
							: all_fp ? mccjit_make_kgc_stub_fp(variant, baseline, memoize_ok,
																								 nargs)
											 : mccjit_make_kgc_stub_n(variant, baseline, memoize_ok,
																								ptypes, nargs, ret_wide);
				if (entry)
					{ MCC_TRACE("br\n"); routed = 1; }
			}
		}
		if (!entry && no_kgc)
			{ MCC_TRACE("br\n"); entry = variant ? mccjit_make_trampoline(variant) : NULL; }
		if (timed && max_duration && entry &&
				mccjit_elapsed(t0) > (double)max_duration) { MCC_TRACE("br\n");
			over = 1;
			entry = NULL;
		}
	}
	if (getenv("MCC_JIT_VERBOSE")) { MCC_TRACE("br\n");
		int probeable = variant && mccjit_last_nparam == 1 &&
										!mccjit_type_wide((int)mccjit_last_param_t[0]);
		int probe = probeable ? ((int (*)(int))variant)(7) : -1;
		fprintf(stderr,
						"mccjit-boot[%s]: slot=%p aot=%p blob=%p len=%lu variant=%p baseline=%p entry=%p route=%s np=%u probe(7)=%d %s\n",
						mode, (void *)slot, aot_init, blob, len, variant, baseline, entry,
						routed ? "kgc" : "direct", mccjit_last_nparam, probe,
						skipped				 ? "budget-skip"
						: over					 ? "over-budget-kept-aot"
						: entry					 ? "swapped"
						: (variant && !no_kgc && !mccjit_last_kgc_ok)
								? "refused-unverified"
								: "kept-aot");
	}
	if (entry)
		{ MCC_TRACE("br\n"); mcc_jit_publish(slot, entry); }
}

static int mccjit_bench_enabled(void) { MCC_TRACE("enter\n");
	const char *e = getenv("MCC_JIT_BENCH");
	return e && e[0] && e[0] != '0';
}

static void *mccjit_lazy_build(const void *blob, unsigned long len, int *routed) { MCC_TRACE("enter\n");
	int no_kgc = getenv("MCC_JIT_NO_KGC") != NULL;
	int spec_wrong = getenv("MCC_JIT_SPEC_WRONG") != NULL;
	void *variant = spec_wrong
											? mcc_jit_recompile_blob_spec(blob, (size_t)len, 0, 7)
											: mcc_jit_recompile_blob(blob, (size_t)len);
	void *entry = NULL;
	if (routed)
		{ MCC_TRACE("br\n"); *routed = 0; }
	if (variant && !no_kgc && mccjit_last_kgc_ok) { MCC_TRACE("br\n");
		uint32_t nargs = mccjit_last_nparam;
		int ret_wide = mccjit_last_ret_wide;
		int all_fp = mccjit_last_allfp;
		int mixed = mccjit_last_mixed;
		uint32_t ngp = mccjit_last_ngp;
		uint32_t nsse = mccjit_last_nsse;
		int ret_fp = mccjit_last_ret_fp;
		uint32_t ptypes[MCCJIT_KGC_MAXARG];
		uint32_t qi;
		void *baseline;
		int memoize_ok;
		for (qi = 0; qi < MCCJIT_KGC_MAXARG; qi++)
			{ MCC_TRACE("br\n"); ptypes[qi] = mccjit_last_param_t[qi]; }
		baseline = mcc_jit_recompile_blob(blob, (size_t)len);
		memoize_ok = (mccjit_last_purity == AST_PURITY_TIER0);
		if (baseline) { MCC_TRACE("br\n");
			entry = mixed ? mccjit_make_kgc_stub_mixed(variant, baseline, memoize_ok,
																								 ngp, nsse, ret_fp, ret_wide)
						: all_fp
									? mccjit_make_kgc_stub_fp(variant, baseline, memoize_ok, nargs)
									: mccjit_make_kgc_stub_n(variant, baseline, memoize_ok, ptypes,
																					 nargs, ret_wide);
			if (entry && routed)
				{ MCC_TRACE("br\n"); *routed = 1; }
		}
	}
	if (!entry && no_kgc)
		{ MCC_TRACE("br\n"); entry = variant ? mccjit_make_trampoline(variant) : NULL; }
	return entry;
}

#define MCCJIT_PROFILE_SAMPLES 8

typedef struct MccjitCounterState {
	void **slot;
	const void *blob;
	unsigned long len;
	void *baseline;
	long threshold;
	long count;
	void *promoted;
	int building;
	long argseen;
	int nsample;
	int64_t argmin[MCCJIT_KGC_MAXARG];
	int64_t argmax[MCCJIT_KGC_MAXARG];
	int64_t sample[MCCJIT_PROFILE_SAMPLES][MCCJIT_KGC_MAXARG];
	pthread_mutex_t lock;
} MccjitCounterState;

MCCJIT_LOCAL int mccjit_promote_by_profile(void *cand, void *incumbent,
																					 const MccjitCounterState *st,
																					 uint32_t nargs, int wide);

static int mccjit_bench_admit(void *cand, void *incumbent,
															const MccjitCounterState *st, uint32_t nargs,
															int wide, int allfp, int routed) { MCC_TRACE("enter\n");
	if (!mccjit_bench_enabled())
		{ MCC_TRACE("br\n"); return 1; }
	if (!routed || allfp || !cand || !incumbent || nargs == 0)
		{ MCC_TRACE("br\n"); return 1; }
	return mccjit_promote_by_profile(cand, incumbent, st, nargs, wide);
}

/* J6A jit-profile: runtime live-in capture riding the D5 hot counter. The
   counter stub spills the 6 GP arg registers and hands their address to the
   tick as `regs`; regs[MCCJIT_KGC_MAXARG-1-i] is param i (rdi..r9 pushed in
   order, so the pointer walks r9..rdi). We accumulate a per-param min/max
   range (feeds dispatch mode 5's range guard + J7A) and a small ring of real
   observed tuples (the safe live-in set for the K5 promotion benchmark). */
static void mccjit_counter_capture(MccjitCounterState *st, const int64_t *regs) { MCC_TRACE("enter\n");
	int i;
	for (i = 0; i < MCCJIT_KGC_MAXARG; i++) { MCC_TRACE("br\n");
		int64_t v = regs[MCCJIT_KGC_MAXARG - 1 - i];
		if (st->argseen == 0) { MCC_TRACE("br\n");
			st->argmin[i] = v;
			st->argmax[i] = v;
		} else { MCC_TRACE("br\n");
			if (v < st->argmin[i])
				{ MCC_TRACE("br\n"); st->argmin[i] = v; }
			if (v > st->argmax[i])
				{ MCC_TRACE("br\n"); st->argmax[i] = v; }
		}
	}
	if (st->nsample < MCCJIT_PROFILE_SAMPLES) { MCC_TRACE("br\n");
		for (i = 0; i < MCCJIT_KGC_MAXARG; i++)
			{ MCC_TRACE("br\n"); st->sample[st->nsample][i] = regs[MCCJIT_KGC_MAXARG - 1 - i]; }
		st->nsample++;
	}
	st->argseen++;
}

/* J7A value-range speculation: the actionable fact from J6A's captured range
   is the collapsed case — a param observed to hold a single value over the
   whole profile (argmin==argmax) is a speculative constant. We only consider
   the first `nargs` params (the counter also captures unused arg registers,
   whose "range" is meaningless). Returns 1 + the param index/value to fold. */
static int mccjit_profile_pick_const(const MccjitCounterState *st, uint32_t nargs,
																		 long min_samples, int *pidx, int64_t *pval) { MCC_TRACE("enter\n");
	uint32_t i;
	if (!st || nargs == 0 || st->argseen < min_samples)
		{ MCC_TRACE("br\n"); return 0; }
	for (i = 0; i < nargs && i < MCCJIT_KGC_MAXARG; i++) { MCC_TRACE("br\n");
		if (st->argmin[i] == st->argmax[i]) { MCC_TRACE("br\n");
			if (pidx)
				{ MCC_TRACE("br\n"); *pidx = (int)i; }
			if (pval)
				{ MCC_TRACE("br\n"); *pval = st->argmin[i]; }
			return 1;
		}
	}
	return 0;
}

/* Build the hot variant, speculating on the J6A profile: if a param is
   observed constant, const-specialize on it (mode-4 fold). The speculation is
   guarded downstream by the KGC differential verify (a wrong fold returns the
   baseline result) and the K1C poison policy (a frequently-wrong fold is
   discarded), so it needs no static soundness proof here. Falls back to the
   plain recompile when the profile shows no constant param. */
MCCJIT_LOCAL void *mccjit_recompile_profiled(const void *blob, size_t len,
																						 const MccjitCounterState *st,
																						 uint32_t nargs, long min_samples) { MCC_TRACE("enter\n");
	int pidx = -1;
	int64_t pval = 0;
	if (mccjit_profile_pick_const(st, nargs, min_samples, &pidx, &pval))
		{ MCC_TRACE("br\n"); return mcc_jit_recompile_blob_spec(blob, len, pidx, pval); }
	return mcc_jit_recompile_blob(blob, (size_t)len);
}

typedef struct MccjitSwapJob {
	void (*run)(struct MccjitSwapJob *);
	void **slot;
	const void *blob;
	unsigned long len;
	unsigned long max_duration;
	struct timespec start;
	int timed;
	MccjitCounterState *cst;
	struct MccjitSwapJob *next;
} MccjitSwapJob;

static pthread_mutex_t mccjit_swap_lock = PTHREAD_MUTEX_INITIALIZER;

static struct {
	MccjitSwapJob *head, *tail;
	pthread_mutex_t qlock;
	pthread_cond_t qcond;
	int started;
	int nworkers;
} mccjit_pool = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER,
								 0, 0};

static pthread_once_t mccjit_fork_once = PTHREAD_ONCE_INIT;

static void mccjit_atfork_prepare(void) { MCC_TRACE("enter\n");
	pthread_mutex_lock(&mccjit_pool.qlock);
	pthread_mutex_lock(&mccjit_swap_lock);
}

static void mccjit_atfork_parent(void) { MCC_TRACE("enter\n");
	pthread_mutex_unlock(&mccjit_swap_lock);
	pthread_mutex_unlock(&mccjit_pool.qlock);
}

static void mccjit_atfork_child(void) { MCC_TRACE("enter\n");
	mccjit_pool.head = mccjit_pool.tail = NULL;
	mccjit_pool.started = 0;
	mccjit_pool.nworkers = 0;
	pthread_cond_init(&mccjit_pool.qcond, NULL);
	pthread_mutex_unlock(&mccjit_swap_lock);
	pthread_mutex_unlock(&mccjit_pool.qlock);
}

static void mccjit_fork_setup(void) { MCC_TRACE("enter\n");
	pthread_atfork(mccjit_atfork_prepare, mccjit_atfork_parent,
								 mccjit_atfork_child);
}

static void *mccjit_pool_worker(void *arg) { MCC_TRACE("enter\n");
	(void)arg;
	for (;;) { MCC_TRACE("br\n");
		MccjitSwapJob *job;
		pthread_mutex_lock(&mccjit_pool.qlock);
		while (!mccjit_pool.head)
			{ MCC_TRACE("br\n"); pthread_cond_wait(&mccjit_pool.qcond, &mccjit_pool.qlock); }
		job = mccjit_pool.head;
		mccjit_pool.head = job->next;
		if (!mccjit_pool.head)
			{ MCC_TRACE("br\n"); mccjit_pool.tail = NULL; }
		pthread_mutex_unlock(&mccjit_pool.qlock);
		pthread_mutex_lock(&mccjit_swap_lock);
		job->run(job);
		pthread_mutex_unlock(&mccjit_swap_lock);
		mcc_free(job);
	}
	return NULL;
}

static int mccjit_pool_start(unsigned long workers) { MCC_TRACE("enter\n");
	int n;
	pthread_once(&mccjit_fork_once, mccjit_fork_setup);
	pthread_mutex_lock(&mccjit_pool.qlock);
	if (!mccjit_pool.started) { MCC_TRACE("br\n");
		int want = (int)workers;
		int i;
		if (want < 1)
			{ MCC_TRACE("br\n"); want = 1; }
		mccjit_pool.started = 1;
		for (i = 0; i < want; i++) { MCC_TRACE("br\n");
			pthread_t th;
			if (pthread_create(&th, NULL, mccjit_pool_worker, NULL) != 0)
				{ MCC_TRACE("br\n"); break; }
			pthread_detach(th);
			mccjit_pool.nworkers++;
		}
		if (getenv("MCC_JIT_VERBOSE"))
			{ MCC_TRACE("br\n"); fprintf(stderr, "mccjit-pool[start]: requested=%d live=%d\n", want,
							mccjit_pool.nworkers); }
	}
	n = mccjit_pool.nworkers;
	pthread_mutex_unlock(&mccjit_pool.qlock);
	return n;
}

static int mccjit_pool_ready(void) { MCC_TRACE("enter\n");
	return mccjit_pool.nworkers > 0;
}

static void mccjit_pool_enqueue(MccjitSwapJob *job) { MCC_TRACE("enter\n");
	pthread_mutex_lock(&mccjit_pool.qlock);
	job->next = NULL;
	if (mccjit_pool.tail)
		{ MCC_TRACE("br\n"); mccjit_pool.tail->next = job; }
	else
		{ MCC_TRACE("br\n"); mccjit_pool.head = job; }
	mccjit_pool.tail = job;
	pthread_cond_signal(&mccjit_pool.qcond);
	pthread_mutex_unlock(&mccjit_pool.qlock);
}

static void mccjit_job_run_eager(MccjitSwapJob *job) { MCC_TRACE("enter\n");
	mccjit_boot_swap_run(job->slot, job->blob, job->len, job->max_duration,
											 "async", &job->start, job->timed);
}

static void mccjit_job_run_lazy(MccjitSwapJob *job) { MCC_TRACE("enter\n");
	MccjitCounterState *st = job->cst;
	int routed = 0;
	void *entry = mccjit_lazy_build(st->blob, st->len, &routed);
	uint32_t nargs = mccjit_last_nparam;
	int wide = mccjit_last_ret_wide;
	int allfp = mccjit_last_allfp;
	pthread_mutex_lock(&st->lock);
	if (entry) { MCC_TRACE("br\n");
		void *incumbent = st->promoted ? st->promoted : st->baseline;
		if (!mccjit_bench_admit(entry, incumbent, st, nargs, wide, allfp, routed)) { MCC_TRACE("br\n");
			if (incumbent) { MCC_TRACE("br\n");
				st->promoted = incumbent;
				mcc_jit_publish(st->slot, incumbent);
			}
			entry = incumbent;
		} else { MCC_TRACE("br\n");
			st->promoted = entry;
			mcc_jit_publish(st->slot, entry);
			if (mcc_stats_mask)
				{ MCC_TRACE("br\n"); mcc_stats_jit_promote(1); }
		}
	}
	st->building = 0;
	pthread_mutex_unlock(&st->lock);
	if (getenv("MCC_JIT_VERBOSE"))
		{ MCC_TRACE("br\n"); fprintf(stderr,
						"mccjit-lazy[promote-async]: slot=%p entry=%p route=%s %s\n",
						(void *)st->slot, entry, routed ? "kgc" : "direct",
						entry ? "promoted" : "build-failed"); }
}

static void *mccjit_counter_tick(MccjitCounterState *st, const int64_t *regs) { MCC_TRACE("enter\n");
	long n;
	void *target;
	int verbose = getenv("MCC_JIT_VERBOSE") != NULL;
	pthread_mutex_lock(&st->lock);
	n = ++st->count;
	if (regs && !st->promoted)
		{ MCC_TRACE("br\n"); mccjit_counter_capture(st, regs); }
	if (st->promoted) { MCC_TRACE("br\n");
		target = st->promoted;
	} else if (n < st->threshold) { MCC_TRACE("br\n");
		if (verbose && n == 1)
			{ MCC_TRACE("br\n"); fprintf(stderr,
							"mccjit-lazy[cold]: slot=%p call=%ld<threshold=%ld running baseline=%p\n",
							(void *)st->slot, n, st->threshold, st->baseline); }
		target = st->baseline;
	} else if (mccjit_pool_ready()) { MCC_TRACE("br\n");
		if (!st->building) { MCC_TRACE("br\n");
			MccjitSwapJob *job = mcc_malloc(sizeof *job);
			if (job) { MCC_TRACE("br\n");
				job->run = mccjit_job_run_lazy;
				job->cst = st;
				st->building = 1;
				mccjit_pool_enqueue(job);
				if (verbose)
					{ MCC_TRACE("br\n"); fprintf(stderr,
									"mccjit-lazy[promote-async]: slot=%p hot after %ld calls -> queued\n",
									(void *)st->slot, n); }
			}
		}
		target = st->baseline;
	} else { MCC_TRACE("br\n");
		int routed = 0;
		void *entry = mccjit_lazy_build(st->blob, st->len, &routed);
		if (entry) { MCC_TRACE("br\n");
			void *incumbent = st->promoted ? st->promoted : st->baseline;
			if (!mccjit_bench_admit(entry, incumbent, st, mccjit_last_nparam,
															mccjit_last_ret_wide, mccjit_last_allfp,
															routed)) { MCC_TRACE("br\n");
				if (incumbent) { MCC_TRACE("br\n");
					st->promoted = incumbent;
					mcc_jit_publish(st->slot, incumbent);
				}
				target = incumbent ? incumbent : st->baseline;
				if (verbose)
					{ MCC_TRACE("br\n"); fprintf(stderr,
									"mccjit-lazy[promote]: slot=%p candidate=%p lost bench -> keep incumbent=%p\n",
									(void *)st->slot, entry, incumbent); }
			} else { MCC_TRACE("br\n");
				st->promoted = entry;
				mcc_jit_publish(st->slot, entry);
				target = entry;
				if (mcc_stats_mask)
					{ MCC_TRACE("br\n"); mcc_stats_jit_promote(0); }
				if (verbose)
					{ MCC_TRACE("br\n"); fprintf(stderr,
									"mccjit-lazy[promote]: slot=%p hot after %ld calls -> entry=%p route=%s\n",
									(void *)st->slot, n, entry, routed ? "kgc" : "direct"); }
			}
		} else { MCC_TRACE("br\n");
			target = st->baseline;
			if (verbose)
				{ MCC_TRACE("br\n"); fprintf(stderr,
								"mccjit-lazy[promote]: slot=%p build failed, staying cold\n",
								(void *)st->slot); }
		}
	}
	pthread_mutex_unlock(&st->lock);
	return target;
}

#if defined(__x86_64__)
static void *mccjit_make_counter_stub(MccjitCounterState *st) { MCC_TRACE("enter\n");
	void *tick = (void *)mccjit_counter_tick;
	unsigned char *p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
													MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	size_t o = 0;
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return NULL; }
	p[o++] = 0x57;
	p[o++] = 0x56;
	p[o++] = 0x52;
	p[o++] = 0x51;
	p[o++] = 0x41;
	p[o++] = 0x50;
	p[o++] = 0x41;
	p[o++] = 0x51;
	p[o++] = 0x48;
	p[o++] = 0x89;
	p[o++] = 0xe6;
	p[o++] = 0x48;
	p[o++] = 0xbf;
	memcpy(p + o, &st, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0xb8;
	memcpy(p + o, &tick, 8);
	o += 8;
	p[o++] = 0xff;
	p[o++] = 0xd0;
	p[o++] = 0x41;
	p[o++] = 0x59;
	p[o++] = 0x41;
	p[o++] = 0x58;
	p[o++] = 0x59;
	p[o++] = 0x5a;
	p[o++] = 0x5e;
	p[o++] = 0x5f;
	p[o++] = 0xff;
	p[o++] = 0xe0;
	return p;
}
#elif defined(__aarch64__)
static void *mccjit_make_counter_stub(MccjitCounterState *st) { MCC_TRACE("enter\n");
	void *tick = (void *)mccjit_counter_tick;
	size_t page = host_pagesize();
	unsigned char *p;
	static const uint32_t code[22] = {
			0xd10103ffu, /* sub  sp,sp,#64 */
			0xf90017e0u, /* str  x0,[sp,#40] */
			0xf90013e1u, /* str  x1,[sp,#32] */
			0xf9000fe2u, /* str  x2,[sp,#24] */
			0xf9000be3u, /* str  x3,[sp,#16] */
			0xf90007e4u, /* str  x4,[sp,#8] */
			0xf90003e5u, /* str  x5,[sp,#0] */
			0xf9001bfeu, /* str  x30,[sp,#48] */
			0x580001c0u, /* ldr  x0,#56   (state @ +88) */
			0x910003e1u, /* mov  x1,sp */
			0x580001d0u, /* ldr  x16,#56  (tick @ +96) */
			0xd63f0200u, /* blr  x16 */
			0xaa0003f0u, /* mov  x16,x0 */
			0xf9401bfeu, /* ldr  x30,[sp,#48] */
			0xf94017e0u, /* ldr  x0,[sp,#40] */
			0xf94013e1u, /* ldr  x1,[sp,#32] */
			0xf9400fe2u, /* ldr  x2,[sp,#24] */
			0xf9400be3u, /* ldr  x3,[sp,#16] */
			0xf94007e4u, /* ldr  x4,[sp,#8] */
			0xf94003e5u, /* ldr  x5,[sp,#0] */
			0x910103ffu, /* add  sp,sp,#64 */
			0xd61f0200u, /* br   x16 */
	};
	p = mmap(0, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return NULL; }
	memcpy(p, code, sizeof code);
	memcpy(p + 88, &st, 8);
	memcpy(p + 96, &tick, 8);
	if (host_runmem_protect(p, page, HOST_PROT_RX) != 0) { MCC_TRACE("br\n");
		munmap(p, page);
		return NULL;
	}
	return p;
}
#else
static void *mccjit_make_counter_stub(MccjitCounterState *st) { MCC_TRACE("enter\n");
	(void)st;
	return NULL;
}
#endif

static int mccjit_lazy_install(void **slot, const void *blob, unsigned long len) { MCC_TRACE("enter\n");
	void *baseline = slot ? *slot : NULL;
	long threshold = 1000;
	const char *e = getenv("MCC_JIT_HOT_THRESHOLD");
	MccjitCounterState *st;
	void *stub;
	if (e && e[0]) { MCC_TRACE("br\n");
		long v = strtol(e, NULL, 10);
		if (v > 0)
			{ MCC_TRACE("br\n"); threshold = v; }
	}
	st = mcc_mallocz(sizeof *st);
	if (!st)
		{ MCC_TRACE("br\n"); return -1; }
	st->slot = slot;
	st->blob = blob;
	st->len = len;
	st->baseline = baseline;
	st->threshold = threshold;
	st->count = 0;
	st->promoted = NULL;
	pthread_mutex_init(&st->lock, NULL);
	stub = mccjit_make_counter_stub(st);
	if (getenv("MCC_JIT_VERBOSE"))
		{ MCC_TRACE("br\n"); fprintf(stderr,
						"mccjit-lazy[install]: slot=%p baseline=%p blob=%p len=%lu threshold=%ld stub=%p\n",
						(void *)slot, baseline, blob, len, threshold, stub); }
	if (!stub) { MCC_TRACE("br\n");
		mcc_free(st);
		return -1;
	}
	mcc_jit_publish(slot, stub);
	return 0;
}

static int mccjit_lazy_enabled(void) { MCC_TRACE("enter\n");
	const char *e = getenv("MCC_JIT_LAZY");
	return e && e[0] && e[0] != '0';
}

static int mccjit_probe_exec_mem(void) { MCC_TRACE("enter\n");
#if defined(__aarch64__)
	size_t page = host_pagesize();
	void *p = mmap(0, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	uint32_t code[2];
	int got;
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return 0; }
	code[0] = 0x52800b40u; /* mov w0, #0x5a */
	code[1] = 0xd65f03c0u; /* ret */
	memcpy(p, code, sizeof code);
	if (host_runmem_protect(p, page, HOST_PROT_RX) != 0) { MCC_TRACE("br\n");
		munmap(p, page);
		return 0;
	}
	got = ((int (*)(void))p)();
	munmap(p, page);
	return got == 0x5a;
#else
	void *p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
								 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return 0; }
#if defined(__x86_64__)
	{
		static const unsigned char code[] = {0xb8, 0x5a, 0x5a, 0x00, 0x00, 0xc3};
		int got;
		memcpy(p, code, sizeof code);
		got = ((int (*)(void))p)();
		munmap(p, 4096);
		return got == 0x5a5a;
	}
#else
	munmap(p, 4096);
	return 1;
#endif
#endif
}

static int mccjit_feasible_flag;
static pthread_once_t mccjit_feasible_once = PTHREAD_ONCE_INIT;

static void mccjit_feasible_probe(void) { MCC_TRACE("enter\n");
	mccjit_feasible_flag = mccjit_probe_exec_mem();
	if (!mccjit_feasible_flag && getenv("MCC_JIT_VERBOSE"))
		{ MCC_TRACE("br\n"); fprintf(stderr,
						"mccjit: executable-memory probe failed — JIT disabled, running "
						"AOT baseline\n"); }
}

static int mccjit_feasible(void) { MCC_TRACE("enter\n");
	if (getenv("MCC_JIT_FORCE_INFEASIBLE"))
		{ MCC_TRACE("br\n"); return 0; }
	pthread_once(&mccjit_feasible_once, mccjit_feasible_probe);
	return mccjit_feasible_flag;
}

/* K9/L2A/L3 — QSBR reclamation. Pointer-swap is the always-correct default: it
   never mutates bytes a running thread may execute, so an old variant can only
   be *freed* once no thread can still be inside it. This is the epoch core:
   each participating thread holds a slot with a local epoch; retiring an old
   variant tags it with the bumped global epoch; a retiree is reclaimed once
   every registered thread has announced a quiescent state at or after that
   epoch (min local >= tag), i.e. every thread has left any JIT call since the
   swap. Quiescent points (L2A: function-entry + loop back-edges) and the
   swap-path retire wiring are the deferred integration; the default stays
   leak-and-cap so a QSBR bug can never free a live variant. */
#define MCCJIT_QSBR_SLOTS 64
#define MCCJIT_QSBR_LIMBO 256

static struct {
	uint64_t global;
	uint64_t local[MCCJIT_QSBR_SLOTS];
	int used[MCCJIT_QSBR_SLOTS];
	struct {
		void *ptr;
		size_t size;
		uint64_t epoch;
	} limbo[MCCJIT_QSBR_LIMBO];
	int nlimbo;
	uint64_t reclaimed;
	uint64_t leaked;
	pthread_mutex_t lock;
} mccjit_qsbr = {1, {0}, {0}, {{0}}, 0, 0, 0, PTHREAD_MUTEX_INITIALIZER};

static int mccjit_qsbr_register(void) { MCC_TRACE("enter\n");
	int i, slot = -1;
	pthread_mutex_lock(&mccjit_qsbr.lock);
	for (i = 0; i < MCCJIT_QSBR_SLOTS; i++)
		{ MCC_TRACE("br\n"); if (!mccjit_qsbr.used[i]) { MCC_TRACE("br\n");
			slot = i;
			mccjit_qsbr.used[i] = 1;
			mccjit_qsbr.local[i] = mccjit_qsbr.global;
			break;
		} }
	pthread_mutex_unlock(&mccjit_qsbr.lock);
	return slot;
}

static void mccjit_qsbr_unregister(int slot) { MCC_TRACE("enter\n");
	if (slot < 0 || slot >= MCCJIT_QSBR_SLOTS)
		{ MCC_TRACE("br\n"); return; }
	pthread_mutex_lock(&mccjit_qsbr.lock);
	mccjit_qsbr.used[slot] = 0;
	mccjit_qsbr.local[slot] = 0;
	pthread_mutex_unlock(&mccjit_qsbr.lock);
}

/* Lock-free hot path: announce this thread has observed the current epoch. */
static void mccjit_qsbr_quiescent(int slot) { MCC_TRACE("enter\n");
	if (slot < 0 || slot >= MCCJIT_QSBR_SLOTS)
		{ MCC_TRACE("br\n"); return; }
	__atomic_store_n(&mccjit_qsbr.local[slot],
									 __atomic_load_n(&mccjit_qsbr.global, __ATOMIC_ACQUIRE),
									 __ATOMIC_RELEASE);
}

static uint64_t mccjit_qsbr_min_local(void) { MCC_TRACE("enter\n");
	int i;
	uint64_t m = __atomic_load_n(&mccjit_qsbr.global, __ATOMIC_ACQUIRE);
	for (i = 0; i < MCCJIT_QSBR_SLOTS; i++)
		{ MCC_TRACE("br\n"); if (mccjit_qsbr.used[i]) { MCC_TRACE("br\n");
			uint64_t l = __atomic_load_n(&mccjit_qsbr.local[i], __ATOMIC_ACQUIRE);
			if (l < m)
				{ MCC_TRACE("br\n"); m = l; }
		} }
	return m;
}

static void mccjit_qsbr_reclaim_locked(void) { MCC_TRACE("enter\n");
	uint64_t minl = mccjit_qsbr_min_local();
	int i = 0;
	while (i < mccjit_qsbr.nlimbo) { MCC_TRACE("br\n");
		if (mccjit_qsbr.limbo[i].epoch <= minl) { MCC_TRACE("br\n");
			if (mccjit_qsbr.limbo[i].ptr && mccjit_qsbr.limbo[i].size)
				{ MCC_TRACE("br\n"); munmap(mccjit_qsbr.limbo[i].ptr, mccjit_qsbr.limbo[i].size); }
			mccjit_qsbr.reclaimed++;
			mccjit_qsbr.limbo[i] = mccjit_qsbr.limbo[--mccjit_qsbr.nlimbo];
		} else { MCC_TRACE("br\n");
			i++;
		}
	}
}

static void mccjit_qsbr_reclaim(void) { MCC_TRACE("enter\n");
	pthread_mutex_lock(&mccjit_qsbr.lock);
	mccjit_qsbr_reclaim_locked();
	pthread_mutex_unlock(&mccjit_qsbr.lock);
}

/* Defer-free an old variant after its slot has been pointer-swapped away.
   MUST be called only after the publish, so no thread can newly enter it. */
static void mccjit_qsbr_retire(void *ptr, size_t size) { MCC_TRACE("enter\n");
	pthread_mutex_lock(&mccjit_qsbr.lock);
	{
		uint64_t e = __atomic_add_fetch(&mccjit_qsbr.global, 1, __ATOMIC_ACQ_REL);
		if (mccjit_qsbr.nlimbo < MCCJIT_QSBR_LIMBO) { MCC_TRACE("br\n");
			mccjit_qsbr.limbo[mccjit_qsbr.nlimbo].ptr = ptr;
			mccjit_qsbr.limbo[mccjit_qsbr.nlimbo].size = size;
			mccjit_qsbr.limbo[mccjit_qsbr.nlimbo].epoch = e;
			mccjit_qsbr.nlimbo++;
		} else { MCC_TRACE("br\n");
			mccjit_qsbr.leaked++; /* leak-and-cap fallback: never free a live variant */
		}
		mccjit_qsbr_reclaim_locked();
	}
	pthread_mutex_unlock(&mccjit_qsbr.lock);
}

static void mccjit_qsbr_reset(void) { MCC_TRACE("enter\n");
	int i;
	pthread_mutex_lock(&mccjit_qsbr.lock);
	for (i = 0; i < mccjit_qsbr.nlimbo; i++)
		{ MCC_TRACE("br\n"); if (mccjit_qsbr.limbo[i].ptr && mccjit_qsbr.limbo[i].size)
			{ MCC_TRACE("br\n"); munmap(mccjit_qsbr.limbo[i].ptr, mccjit_qsbr.limbo[i].size); } }
	mccjit_qsbr.nlimbo = 0;
	mccjit_qsbr.global = 1;
	mccjit_qsbr.reclaimed = 0;
	mccjit_qsbr.leaked = 0;
	for (i = 0; i < MCCJIT_QSBR_SLOTS; i++) { MCC_TRACE("br\n");
		mccjit_qsbr.used[i] = 0;
		mccjit_qsbr.local[i] = 0;
	}
	pthread_mutex_unlock(&mccjit_qsbr.lock);
}

void mccjit_boot_swap(void **slot, const void *blob, unsigned long len) { MCC_TRACE("enter\n");
	mcc_stats_env_init();
	if (!mccjit_feasible())
		{ MCC_TRACE("br\n"); return; }
	if (mccjit_lazy_enabled() && mccjit_lazy_install(slot, blob, len) == 0)
		{ MCC_TRACE("br\n"); return; }
	mccjit_boot_swap_run(slot, blob, len, 0, "sync", NULL, 0);
}

void mccjit_boot_swap_async(void **slot, const void *blob, unsigned long len,
														unsigned long max_duration, unsigned long workers) { MCC_TRACE("enter\n");
	MccjitSwapJob *job;
	int nw;
	mcc_stats_env_init();
	if (!mccjit_feasible())
		{ MCC_TRACE("br\n"); return; }
	nw = mccjit_pool_start(workers);
	if (mccjit_lazy_enabled() && mccjit_lazy_install(slot, blob, len) == 0)
		{ MCC_TRACE("br\n"); return; }
	job = (nw > 0) ? mcc_malloc(sizeof *job) : NULL;
	if (job) { MCC_TRACE("br\n");
		job->run = mccjit_job_run_eager;
		job->slot = slot;
		job->blob = blob;
		job->len = len;
		job->max_duration = max_duration;
		job->timed =
				(max_duration != 0) && (clock_gettime(CLOCK_MONOTONIC, &job->start) == 0);
		mccjit_pool_enqueue(job);
		return;
	}
	{
		struct timespec t0;
		int timed =
				(max_duration != 0) && (clock_gettime(CLOCK_MONOTONIC, &t0) == 0);
		mccjit_boot_swap_run(slot, blob, len, max_duration, "sync-fallback", &t0,
												 timed);
	}
}

int mccjit_embed_have_fns(void) { MCC_TRACE("enter\n");
	return mccjit_embed_fns != NULL;
}

void mccjit_embed_finalize(MCCState *s1) { MCC_TRACE("enter\n");
	MccjitEmbedFn *e, *nx;
	CString cs;
	int n = 0;
	int async = 0;
	if (!s1 || !(s1->embed_jit || s1->output_type == MCC_OUTPUT_MEMORY) ||
			!mccjit_embed_fns || mccjit_internal_compile)
		{ MCC_TRACE("br\n"); return; }
	async = s1->jit_threads > 0;
	if (s1->output_type == MCC_OUTPUT_MEMORY) { MCC_TRACE("br\n");
		mcc_add_symbol(s1, "mccjit_boot_swap", (void *)mccjit_boot_swap);
		mcc_add_symbol(s1, "mccjit_boot_swap_async",
									 (void *)mccjit_boot_swap_async);
	} else if (getenv("MCC_JIT_EXPORT_INTERNALS")) { MCC_TRACE("br\n");
		int i;
		s1->rdynamic = 1;
		for (i = 0; i < mccjit_export_n; i++)
			{ MCC_TRACE("br\n"); mcc_jit_export_local(s1, mccjit_export_names[i]); }
	}
	cstr_new(&cs);
	if (async)
		{ MCC_TRACE("br\n"); cstr_printf(&cs,
								"extern void mccjit_boot_swap_async(void**, const void*, unsigned long, unsigned long, unsigned long);\n"); }
	else
		{ MCC_TRACE("br\n"); cstr_printf(&cs,
								"extern void mccjit_boot_swap(void**, const void*, unsigned long);\n"); }
	for (e = mccjit_embed_fns; e; e = e->next) { MCC_TRACE("br\n");
		int off = data_section->data_offset;
		unsigned char *p = section_ptr_add(data_section, e->len ? e->len : 1);
		char blobname[256];
		if (e->len)
			{ MCC_TRACE("br\n"); memcpy(p, e->blob, e->len); }
		snprintf(blobname, sizeof blobname, "%s__mccjit_blob_%s",
						 s1->leading_underscore ? "_" : "", e->name);
		set_global_sym(s1, blobname, data_section, off);
		cstr_printf(&cs, "extern unsigned char __mccjit_blob_%s[];\n", e->name);
		cstr_printf(&cs, "extern void *__mccjit_slot_%s;\n", e->name);
		n++;
	}
	cstr_printf(&cs,
							"static struct __mccjit_reg { void **slot; const unsigned char *blob; "
							"unsigned long len; } __mccjit_registry[] = {\n");
	for (e = mccjit_embed_fns; e; e = e->next)
		{ MCC_TRACE("br\n"); cstr_printf(&cs, "{&__mccjit_slot_%s, __mccjit_blob_%s, %luUL},\n", e->name,
								e->name, (unsigned long)e->len); }
	cstr_printf(&cs, "};\n");
	{
		int def_on = (s1->output_type == MCC_OUTPUT_MEMORY && s1->jit >= 0)
										 ? s1->jit
										 : (MCC_JIT_DEFAULT ? 1 : 0);
		cstr_printf(&cs, "extern char *getenv(const char*);\n");
		cstr_printf(
				&cs,
				"__attribute__((constructor)) static void __mccjit_boot_all(void){\n"
				"const char *__e = getenv(\"MCC_JIT\");\n"
				"int __on = __e ? (__e[0] != '0') : %d;\n"
				"int __i;\n"
				"if(!__on) return;\n"
				"for(__i=0;__i<%d;__i++)\n",
				def_on, n);
	}
	if (async)
		{ MCC_TRACE("br\n"); cstr_printf(&cs,
								"mccjit_boot_swap_async(__mccjit_registry[__i].slot, __mccjit_registry[__i].blob, __mccjit_registry[__i].len, %luUL, %luUL);\n}\n",
								(unsigned long)s1->jit_max_duration,
								(unsigned long)s1->jit_threads); }
	else
		{ MCC_TRACE("br\n"); cstr_printf(&cs,
								"mccjit_boot_swap(__mccjit_registry[__i].slot, __mccjit_registry[__i].blob, __mccjit_registry[__i].len);\n}\n"); }
	mcc_compile_string(s1, cs.data);
	cstr_free(&cs);
	for (e = mccjit_embed_fns; e; e = nx) { MCC_TRACE("br\n");
		nx = e->next;
		mcc_free(e->name);
		mcc_free(e->blob);
		mcc_free(e);
	}
	mccjit_embed_fns = NULL;
	{
		int i;
		for (i = 0; i < mccjit_export_n; i++)
			{ MCC_TRACE("br\n"); mcc_free(mccjit_export_names[i]); }
		mcc_free(mccjit_export_names);
		mccjit_export_names = NULL;
		mccjit_export_n = mccjit_export_cap = 0;
	}
}

int mccjit_selftest(void) { MCC_TRACE("enter\n");
	static const char src[] = "int f(int x){return x*2+1;}";
	MCCState *s1;
	int (*aotf)(int) = NULL;
	int (*jitf)(int) = NULL;
	int inputs[4] = {5, 0, -3, 100};
	int i, fails = 0;
	void *slot = NULL, *v1 = NULL, *v2 = NULL, *vspec = NULL;
	MCCState *v1state = NULL, *v2state = NULL, *vspecstate = NULL;

	mcc_free(mccjit_last_blob);
	mccjit_last_blob = NULL;
	mccjit_last_len = 0;
	mccjit_last_state = NULL;

	s1 = mcc_new();
	if (!s1) { MCC_TRACE("br\n");
		printf("mccjit-selftest: mcc_new failed\n");
		return 1;
	}
	s1->optimize = 1;
	s1->nostdlib = 1;
	mcc_free(s1->jit_functions);
	s1->jit_functions = mcc_strdup("f");
	mcc_set_output_type(s1, MCC_OUTPUT_MEMORY);

	if (mcc_compile_string(s1, src) != 0) { MCC_TRACE("br\n");
		printf("mccjit-selftest: state1 compile failed\n");
		mcc_delete(s1);
		return 1;
	}
	if (!mccjit_last_blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest: no intent blob stashed for 'f' (not faithful?)\n");
		mcc_delete(s1);
		return 1;
	}
	printf("mccjit-selftest: stashed leaf-int intent = %lu bytes\n",
				 (unsigned long)mccjit_last_len);

	if (mcc_relocate(s1) == 0)
		{ MCC_TRACE("br\n"); aotf = (int (*)(int))mcc_get_symbol(s1, "f"); }

	jitf = (int (*)(int))mcc_jit_recompile_blob(mccjit_last_blob, mccjit_last_len);
	if (!jitf) { MCC_TRACE("br\n");
		printf("mccjit-selftest: cross-session recompile returned NULL\n");
		mcc_delete(s1);
		return 1;
	}
	v1state = mccjit_last_state;

	for (i = 0; i < 4; i++) { MCC_TRACE("br\n");
		int x = inputs[i];
		int got = jitf(x);
		int want = x * 2 + 1;
		int aot = aotf ? aotf(x) : want;
		int ok = (got == want) && (got == aot);
		printf("mccjit-selftest: f(%d) jit=%d expect=%d aot=%d %s\n", x, got, want,
					 aot, ok ? "OK" : "FAIL");
		if (!ok)
			{ MCC_TRACE("br\n"); fails++; }
	}

	v1 = (void *)jitf;
	slot = v1;
	printf("mccjit-selftest: hotswap slot init -> v1=%p\n", slot);
	for (i = 0; i < 4; i++) { MCC_TRACE("br\n");
		int x = inputs[i];
		int got = ((int (*)(int))slot)(x);
		int want = x * 2 + 1;
		int ok = (got == want);
		printf("mccjit-selftest: slot(v1) f(%d)=%d expect=%d %s\n", x, got, want,
					 ok ? "OK" : "FAIL");
		if (!ok)
			{ MCC_TRACE("br\n"); fails++; }
	}

	v2 = mcc_jit_recompile_blob(mccjit_last_blob, mccjit_last_len);
	if (!v2) { MCC_TRACE("br\n");
		printf("mccjit-selftest: v2 recompile returned NULL\n");
		if (v1state)
			{ MCC_TRACE("br\n"); mcc_delete(v1state); }
		mcc_delete(s1);
		return 1;
	}
	v2state = mccjit_last_state;
	mcc_jit_publish(&slot, v2);
	printf("mccjit-selftest: published v2=%p into slot (was v1=%p)\n", v2, v1);
	if (slot != v2) { MCC_TRACE("br\n");
		printf("mccjit-selftest: slot did not observe v2 after publish\n");
		fails++;
	}
	for (i = 0; i < 4; i++) { MCC_TRACE("br\n");
		int x = inputs[i];
		int gv1 = ((int (*)(int))v1)(x);
		int gv2 = ((int (*)(int))slot)(x);
		int want = x * 2 + 1;
		int ok = (gv2 == want) && (gv1 == gv2);
		printf("mccjit-selftest: swap f(%d) v1=%d v2=%d expect=%d %s\n", x, gv1, gv2,
					 want, ok ? "OK" : "FAIL");
		if (!ok)
			{ MCC_TRACE("br\n"); fails++; }
	}

	vspec = mcc_jit_recompile_blob_spec(mccjit_last_blob, mccjit_last_len, 0, 7);
	if (!vspec) { MCC_TRACE("br\n");
		printf("mccjit-selftest: specialized recompile returned NULL\n");
		fails++;
	} else { MCC_TRACE("br\n");
		int sval = ((int (*)(int))vspec)(7);
		int sfold = ((int (*)(int))vspec)(5);
		int ok = (sval == 15) && (sfold == 15);
		vspecstate = mccjit_last_state;
		printf("mccjit-selftest: spec[x==7] f(7)=%d expect=15; f(5)=%d (folded=%s) %s\n",
					 sval, sfold, sfold == 15 ? "const" : "live", ok ? "OK" : "FAIL");
		if (!ok)
			{ MCC_TRACE("br\n"); fails++; }
	}

	if (vspecstate)
		{ MCC_TRACE("br\n"); mcc_delete(vspecstate); }
	if (v2state)
		{ MCC_TRACE("br\n"); mcc_delete(v2state); }
	if (v1state)
		{ MCC_TRACE("br\n"); mcc_delete(v1state); }
	mccjit_last_state = NULL;
	mcc_delete(s1);
	printf("mccjit-selftest: %s (%d failure%s)\n", fails ? "FAIL" : "PASS", fails,
				 fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static unsigned char *mccjit_stash_one(const char *src, const char *fn,
																			 int nostdlib, size_t *out_len,
																			 MCCState **out_state) { MCC_TRACE("enter\n");
	MCCState *s1;
	unsigned char *blob = NULL;
	*out_len = 0;
	*out_state = NULL;
	mcc_free(mccjit_last_blob);
	mccjit_last_blob = NULL;
	mccjit_last_len = 0;
	s1 = mcc_new();
	if (!s1)
		{ MCC_TRACE("br\n"); return NULL; }
	s1->optimize = 1;
	s1->nostdlib = nostdlib;
	mcc_free(s1->jit_functions);
	s1->jit_functions = mcc_strdup(fn);
	mcc_set_output_type(s1, MCC_OUTPUT_MEMORY);
	mccjit_internal_compile = 1;
	if (mcc_compile_string(s1, src) != 0) { MCC_TRACE("br\n");
		mccjit_internal_compile = 0;
		mcc_delete(s1);
		return NULL;
	}
	mccjit_internal_compile = 0;
	if (mccjit_last_blob) { MCC_TRACE("br\n");
		blob = mcc_malloc(mccjit_last_len ? mccjit_last_len : 1);
		if (blob) { MCC_TRACE("br\n");
			memcpy(blob, mccjit_last_blob, mccjit_last_len);
			*out_len = mccjit_last_len;
		}
	}
	*out_state = s1;
	return blob;
}

int mccjit_selftest_stage2(void) { MCC_TRACE("enter\n");
	static const char src_g[] =
			"int g(int *p, int x){ return p ? *p + x : -1; }";
	static const char src_h[] =
			"int abs(int); int h(int x){ return abs(x) + 1; }";
	int fails = 0;
	unsigned char *blob;
	size_t blen;
	MCCState *s1;

	printf("mccjit-selftest-stage2: begin\n");

	blob = mccjit_stash_one(src_g, "g", 1, &blen, &s1);
	if (!s1) { MCC_TRACE("br\n");
		printf("mccjit-selftest-stage2: g compile setup failed\n");
		return 1;
	}
	if (!blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-stage2: no intent blob stashed for 'g'\n");
		mcc_delete(s1);
		fails++;
	} else { MCC_TRACE("br\n");
		int (*aotg)(int *, int) = NULL;
		int (*jitg)(int *, int);
		MCCState *js;
		int cells[3] = {41, -8, 0};
		int i;
		printf("mccjit-selftest-stage2: stashed pointer-param intent = %lu bytes\n",
					 (unsigned long)blen);
		if (mcc_relocate(s1) == 0)
			{ MCC_TRACE("br\n"); aotg = (int (*)(int *, int))mcc_get_symbol(s1, "g"); }
		jitg = (int (*)(int *, int))mcc_jit_recompile_blob(blob, blen);
		if (!jitg) { MCC_TRACE("br\n");
			printf("mccjit-selftest-stage2: g recompile returned NULL\n");
			fails++;
		} else { MCC_TRACE("br\n");
			js = mccjit_last_state;
			for (i = 0; i < 3; i++) { MCC_TRACE("br\n");
				int x = cells[i] + i * 7;
				int got = jitg(&cells[i], x);
				int want = cells[i] + x;
				int aot = aotg ? aotg(&cells[i], x) : want;
				int ok = (got == want) && (got == aot);
				printf("mccjit-selftest-stage2: g(&%d,%d) jit=%d expect=%d aot=%d %s\n",
							 cells[i], x, got, want, aot, ok ? "OK" : "FAIL");
				if (!ok)
					{ MCC_TRACE("br\n"); fails++; }
			}
			{
				int got = jitg((int *)0, 99);
				int aot = aotg ? aotg((int *)0, 99) : -1;
				int ok = (got == -1) && (got == aot);
				printf("mccjit-selftest-stage2: g(NULL,99) jit=%d expect=-1 aot=%d %s\n",
							 got, aot, ok ? "OK" : "FAIL");
				if (!ok)
					{ MCC_TRACE("br\n"); fails++; }
			}
			if (js)
				{ MCC_TRACE("br\n"); mcc_delete(js); }
			mccjit_last_state = NULL;
		}
		mcc_free(blob);
		mcc_delete(s1);
	}

	blob = mccjit_stash_one(src_h, "h", 0, &blen, &s1);
	if (!s1) { MCC_TRACE("br\n");
		printf("mccjit-selftest-stage2: h compile setup failed\n");
		return fails + 1;
	}
	if (!blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-stage2: no intent blob stashed for 'h'\n");
		mcc_delete(s1);
		fails++;
	} else { MCC_TRACE("br\n");
		int (*aoth)(int) = NULL;
		int (*jith)(int);
		MCCState *js;
		int inputs[4] = {5, -12, 0, 40};
		int i;
		printf("mccjit-selftest-stage2: stashed call-bearing intent = %lu bytes\n",
					 (unsigned long)blen);
		if (mcc_relocate(s1) == 0)
			{ MCC_TRACE("br\n"); aoth = (int (*)(int))mcc_get_symbol(s1, "h"); }
		jith = (int (*)(int))mcc_jit_recompile_blob(blob, blen);
		if (!jith) { MCC_TRACE("br\n");
			printf("mccjit-selftest-stage2: h recompile returned NULL (callee unbound?)\n");
			fails++;
		} else { MCC_TRACE("br\n");
			js = mccjit_last_state;
			for (i = 0; i < 4; i++) { MCC_TRACE("br\n");
				int x = inputs[i];
				int got = jith(x);
				int want = (x < 0 ? -x : x) + 1;
				int aot = aoth ? aoth(x) : want;
				int ok = (got == want) && (got == aot);
				printf("mccjit-selftest-stage2: h(%d) jit=%d expect=%d aot=%d %s\n", x,
							 got, want, aot, ok ? "OK" : "FAIL");
				if (!ok)
					{ MCC_TRACE("br\n"); fails++; }
			}
			if (js)
				{ MCC_TRACE("br\n"); mcc_delete(js); }
			mccjit_last_state = NULL;
		}
		mcc_free(blob);
		mcc_delete(s1);
	}

	printf("mccjit-selftest-stage2: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

struct MccjitTestS {
	int a;
	int b;
};

struct MccjitTestT {
	int *p;
	int k;
};

int mccjit_selftest_struct(void) { MCC_TRACE("enter\n");
	static const char src_f[] =
			"struct S{int a; int b;}; int f(struct S*s){return s->a*3 + s->b;}";
	static const char src_i[] =
			"struct S{int a; int b;}; int fi(struct S*s){return s[1].a*3 + s[0].b;}";
	static const char src_g[] =
			"struct T{int *p; int k;}; int g(struct T*t){return *t->p + t->k;}";
	int fails = 0;
	unsigned char *blob;
	size_t blen;
	MCCState *s1;

	printf("mccjit-selftest-struct: begin\n");

	blob = mccjit_stash_one(src_f, "f", 1, &blen, &s1);
	if (!s1) { MCC_TRACE("br\n");
		printf("mccjit-selftest-struct: f compile setup failed\n");
		return 1;
	}
	if (!blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-struct: no intent blob stashed for 'f'\n");
		mcc_delete(s1);
		fails++;
	} else { MCC_TRACE("br\n");
		int (*aotf)(struct MccjitTestS *) = NULL;
		int (*jitf)(struct MccjitTestS *);
		MCCState *js;
		struct MccjitTestS cells[3] = {{7, 5}, {-2, 9}, {0, -11}};
		int i;
		printf("mccjit-selftest-struct: stashed struct-param intent = %lu bytes\n",
					 (unsigned long)blen);
		if (mcc_relocate(s1) == 0)
			{ MCC_TRACE("br\n"); aotf = (int (*)(struct MccjitTestS *))mcc_get_symbol(s1, "f"); }
		jitf = (int (*)(struct MccjitTestS *))mcc_jit_recompile_blob(blob, blen);
		if (!jitf) { MCC_TRACE("br\n");
			printf("mccjit-selftest-struct: f recompile returned NULL\n");
			fails++;
		} else { MCC_TRACE("br\n");
			js = mccjit_last_state;
			for (i = 0; i < 3; i++) { MCC_TRACE("br\n");
				int got = jitf(&cells[i]);
				int want = cells[i].a * 3 + cells[i].b;
				int aot = aotf ? aotf(&cells[i]) : want;
				int ok = (got == want) && (got == aot);
				printf("mccjit-selftest-struct: f({%d,%d}) jit=%d expect=%d aot=%d %s\n",
							 cells[i].a, cells[i].b, got, want, aot, ok ? "OK" : "FAIL");
				if (!ok)
					{ MCC_TRACE("br\n"); fails++; }
			}
			if (js)
				{ MCC_TRACE("br\n"); mcc_delete(js); }
			mccjit_last_state = NULL;
		}
		mcc_free(blob);
		mcc_delete(s1);
	}

	blob = mccjit_stash_one(src_i, "fi", 1, &blen, &s1);
	if (!s1) { MCC_TRACE("br\n");
		printf("mccjit-selftest-struct: fi compile setup failed\n");
		return fails + 1;
	}
	if (!blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-struct: no intent blob stashed for 'fi'\n");
		mcc_delete(s1);
		fails++;
	} else { MCC_TRACE("br\n");
		int (*aotfi)(struct MccjitTestS *) = NULL;
		int (*jitfi)(struct MccjitTestS *);
		MCCState *js;
		struct MccjitTestS cells[6] = {{7, 5}, {-2, 9}, {0, -11},
																	 {3, 4},  {8, -1}, {6, 2}};
		int i;
		printf("mccjit-selftest-struct: stashed struct-index intent = %lu bytes\n",
					 (unsigned long)blen);
		if (mcc_relocate(s1) == 0)
			{ MCC_TRACE("br\n"); aotfi = (int (*)(struct MccjitTestS *))mcc_get_symbol(s1, "fi"); }
		jitfi = (int (*)(struct MccjitTestS *))mcc_jit_recompile_blob(blob, blen);
		if (!jitfi) { MCC_TRACE("br\n");
			printf("mccjit-selftest-struct: fi recompile returned NULL\n");
			fails++;
		} else { MCC_TRACE("br\n");
			js = mccjit_last_state;
			for (i = 0; i + 1 < 6; i++) { MCC_TRACE("br\n");
				int got = jitfi(&cells[i]);
				int want = cells[i + 1].a * 3 + cells[i].b;
				int aot = aotfi ? aotfi(&cells[i]) : want;
				int ok = (got == want) && (got == aot);
				printf("mccjit-selftest-struct: fi(&cells[%d]) jit=%d expect=%d aot=%d %s\n",
							 i, got, want, aot, ok ? "OK" : "FAIL");
				if (!ok)
					{ MCC_TRACE("br\n"); fails++; }
			}
			if (js)
				{ MCC_TRACE("br\n"); mcc_delete(js); }
			mccjit_last_state = NULL;
		}
		mcc_free(blob);
		mcc_delete(s1);
	}

	blob = mccjit_stash_one(src_g, "g", 1, &blen, &s1);
	if (!s1) { MCC_TRACE("br\n");
		printf("mccjit-selftest-struct: g compile setup failed\n");
		return fails + 1;
	}
	if (!blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-struct: no intent blob stashed for 'g'\n");
		mcc_delete(s1);
		fails++;
	} else { MCC_TRACE("br\n");
		int (*aotg)(struct MccjitTestT *) = NULL;
		int (*jitg)(struct MccjitTestT *);
		MCCState *js;
		int slots[3] = {10, -4, 100};
		struct MccjitTestT recs[3];
		int i;
		for (i = 0; i < 3; i++) { MCC_TRACE("br\n");
			recs[i].p = &slots[i];
			recs[i].k = i * 3 - 1;
		}
		printf("mccjit-selftest-struct: stashed pointer-field intent = %lu bytes\n",
					 (unsigned long)blen);
		if (mcc_relocate(s1) == 0)
			{ MCC_TRACE("br\n"); aotg = (int (*)(struct MccjitTestT *))mcc_get_symbol(s1, "g"); }
		jitg = (int (*)(struct MccjitTestT *))mcc_jit_recompile_blob(blob, blen);
		if (!jitg) { MCC_TRACE("br\n");
			printf("mccjit-selftest-struct: g recompile returned NULL\n");
			fails++;
		} else { MCC_TRACE("br\n");
			js = mccjit_last_state;
			for (i = 0; i < 3; i++) { MCC_TRACE("br\n");
				int got = jitg(&recs[i]);
				int want = slots[i] + recs[i].k;
				int aot = aotg ? aotg(&recs[i]) : want;
				int ok = (got == want) && (got == aot);
				printf("mccjit-selftest-struct: g({*%d,%d}) jit=%d expect=%d aot=%d %s\n",
							 slots[i], recs[i].k, got, want, aot, ok ? "OK" : "FAIL");
				if (!ok)
					{ MCC_TRACE("br\n"); fails++; }
			}
			if (js)
				{ MCC_TRACE("br\n"); mcc_delete(js); }
			mccjit_last_state = NULL;
		}
		mcc_free(blob);
		mcc_delete(s1);
	}

	printf("mccjit-selftest-struct: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

#define MCCJIT_KGC_ARITY 6
#define MCCJIT_KGC_MAGIC 0x43474b4dul

typedef struct MccjitKgcHdr {
	uint32_t magic;
	uint32_t arity;
	uint64_t salt;
	uint64_t count;
	uint64_t cap;
} MccjitKgcHdr;

typedef struct MccjitKgc {
	int fd;
	int anon;
	char *path;
	void *map;
	size_t map_len;
	MccjitKgcHdr *hdr;
	int64_t *tuples;
	uint32_t arity;
	int memoize_ok;
	int ret_wide;
	uint64_t hits;
	uint64_t misses;
	int poisoned;
	void *mx_variant;
	void *mx_baseline;
	uint32_t mx_ngp;
	uint32_t mx_nsse;
	int mx_ret_fp;
	int *mx_flag;
	pthread_mutex_t lock;
} MccjitKgc;

static int mccjit_poison_min(void) { MCC_TRACE("enter\n");
	const char *e = getenv("MCC_JIT_POISON_MIN");
	if (e && e[0]) { MCC_TRACE("br\n");
		long v = strtol(e, NULL, 10);
		if (v > 0)
			{ MCC_TRACE("br\n"); return (int)v; }
	}
	return 8;
}

static int mccjit_poison_pct(void) { MCC_TRACE("enter\n");
	const char *e = getenv("MCC_JIT_POISON_PCT");
	if (e && e[0]) { MCC_TRACE("br\n");
		long v = strtol(e, NULL, 10);
		if (v > 0 && v <= 100)
			{ MCC_TRACE("br\n"); return (int)v; }
	}
	return 50;
}

#define MCCJIT_KGC_MAX ((uint64_t)1 << 16)

static size_t mccjit_kgc_bytes(uint64_t cap, uint32_t arity) { MCC_TRACE("enter\n");
	return sizeof(MccjitKgcHdr) + (size_t)cap * arity * sizeof(int64_t);
}

static void mccjit_kgc_bind(MccjitKgc *k) { MCC_TRACE("enter\n");
	k->hdr = (MccjitKgcHdr *)k->map;
	k->tuples = (int64_t *)((char *)k->map + sizeof(MccjitKgcHdr));
}

static int mccjit_kgc_map_shared(MccjitKgc *k, size_t bytes) { MCC_TRACE("enter\n");
	void *m = mmap(0, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, k->fd, 0);
	if (m == MAP_FAILED)
		{ MCC_TRACE("br\n"); return -1; }
	k->map = m;
	k->map_len = bytes;
	mccjit_kgc_bind(k);
	return 0;
}

static void mccjit_kgc_init_hdr(MccjitKgc *k, uint64_t salt, uint64_t cap) { MCC_TRACE("enter\n");
	k->hdr->magic = MCCJIT_KGC_MAGIC;
	k->hdr->arity = k->arity;
	k->hdr->salt = salt;
	k->hdr->count = 0;
	k->hdr->cap = cap;
}

static int mccjit_kgc_open(MccjitKgc *k, const char *path, uint64_t salt,
													 uint32_t arity) { MCC_TRACE("enter\n");
	uint64_t initcap = 64;
	size_t initbytes;
	memset(k, 0, sizeof *k);
	k->fd = -1;
	k->memoize_ok = 1;
	pthread_mutex_init(&k->lock, NULL);
	if (arity == 0 || arity > MCCJIT_KGC_ARITY)
		{ MCC_TRACE("br\n"); return -1; }
	k->arity = arity;
	initbytes = mccjit_kgc_bytes(initcap, arity);
	if (!path) { MCC_TRACE("br\n");
		void *m = mmap(0, initbytes, PROT_READ | PROT_WRITE,
									 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (m == MAP_FAILED)
			{ MCC_TRACE("br\n"); return -1; }
		k->anon = 1;
		k->map = m;
		k->map_len = initbytes;
		mccjit_kgc_bind(k);
		mccjit_kgc_init_hdr(k, salt, initcap);
		return 0;
	}
	k->path = mcc_strdup(path);
	k->fd = open(path, O_RDWR | O_CREAT, 0644);
	if (k->fd < 0)
		{ MCC_TRACE("br\n"); return -1; }
	{
		struct stat st;
		MccjitKgcHdr peek;
		int valid = 0;
		if (fstat(k->fd, &st) == 0 && (size_t)st.st_size >= sizeof(MccjitKgcHdr) &&
				pread(k->fd, &peek, sizeof peek, 0) == (ssize_t)sizeof peek &&
				peek.magic == MCCJIT_KGC_MAGIC && peek.salt == salt &&
				peek.arity == arity && peek.cap >= 1 && peek.count <= peek.cap &&
				(size_t)st.st_size >= mccjit_kgc_bytes(peek.cap, arity)) { MCC_TRACE("br\n");
			if (mccjit_kgc_map_shared(k, mccjit_kgc_bytes(peek.cap, arity)) == 0)
				{ MCC_TRACE("br\n"); valid = 1; }
		}
		if (!valid) { MCC_TRACE("br\n");
			if (ftruncate(k->fd, (off_t)initbytes) != 0 ||
					mccjit_kgc_map_shared(k, initbytes) != 0) { MCC_TRACE("br\n");
				close(k->fd);
				k->fd = -1;
				return -1;
			}
			mccjit_kgc_init_hdr(k, salt, initcap);
			msync(k->map, k->map_len, MS_SYNC);
		}
	}
	return 0;
}

static void mccjit_kgc_close(MccjitKgc *k) { MCC_TRACE("enter\n");
	if (!k)
		{ MCC_TRACE("br\n"); return; }
	if (k->map && k->map != MAP_FAILED) { MCC_TRACE("br\n");
		if (!k->anon)
			{ MCC_TRACE("br\n"); msync(k->map, k->map_len, MS_SYNC); }
		munmap(k->map, k->map_len);
	}
	if (k->fd >= 0)
		{ MCC_TRACE("br\n"); close(k->fd); }
	mcc_free(k->path);
	memset(k, 0, sizeof *k);
	k->fd = -1;
}

static int mccjit_kgc_cmp(const int64_t *a, const int64_t *b, uint32_t arity) { MCC_TRACE("enter\n");
	uint32_t i;
	for (i = 0; i < arity; i++) { MCC_TRACE("br\n");
		if (a[i] < b[i])
			{ MCC_TRACE("br\n"); return -1; }
		if (a[i] > b[i])
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

static uint64_t mccjit_kgc_lower(const MccjitKgc *k, const int64_t *tuple,
																 int *found) { MCC_TRACE("enter\n");
	uint64_t lo = 0, hi = k->hdr->count;
	*found = 0;
	while (lo < hi) { MCC_TRACE("br\n");
		uint64_t mid = lo + (hi - lo) / 2;
		int c = mccjit_kgc_cmp(k->tuples + mid * k->arity, tuple, k->arity);
		if (c < 0) { MCC_TRACE("br\n");
			lo = mid + 1;
		} else if (c > 0) { MCC_TRACE("br\n");
			hi = mid;
		} else { MCC_TRACE("br\n");
			*found = 1;
			return mid;
		}
	}
	return lo;
}

static int mccjit_kgc_contains(const MccjitKgc *k, const int64_t *tuple) { MCC_TRACE("enter\n");
	int found;
	mccjit_kgc_lower(k, tuple, &found);
	return found;
}

static int mccjit_kgc_grow(MccjitKgc *k, uint64_t need) { MCC_TRACE("enter\n");
	uint64_t ncap = k->hdr->cap ? k->hdr->cap : 1;
	size_t nbytes;
	while (ncap < need)
		{ MCC_TRACE("br\n"); ncap *= 2; }
	nbytes = mccjit_kgc_bytes(ncap, k->arity);
	if (k->anon) { MCC_TRACE("br\n");
		void *nm = mmap(0, nbytes, PROT_READ | PROT_WRITE,
										MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (nm == MAP_FAILED)
			{ MCC_TRACE("br\n"); return -1; }
		memcpy(nm, k->map, k->map_len);
		munmap(k->map, k->map_len);
		k->map = nm;
		k->map_len = nbytes;
		mccjit_kgc_bind(k);
		k->hdr->cap = ncap;
		return 0;
	}
	munmap(k->map, k->map_len);
	k->map = NULL;
	if (ftruncate(k->fd, (off_t)nbytes) != 0)
		{ MCC_TRACE("br\n"); return -1; }
	if (mccjit_kgc_map_shared(k, nbytes) != 0)
		{ MCC_TRACE("br\n"); return -1; }
	k->hdr->cap = ncap;
	return 0;
}

static int mccjit_kgc_insert(MccjitKgc *k, const int64_t *tuple) { MCC_TRACE("enter\n");
	int found;
	uint64_t at = mccjit_kgc_lower(k, tuple, &found);
	int64_t *dst;
	if (found)
		{ MCC_TRACE("br\n"); return 0; }
	if (k->hdr->count >= MCCJIT_KGC_MAX)
		{ MCC_TRACE("br\n"); return 0; }
	if (k->hdr->count + 1 > k->hdr->cap &&
			mccjit_kgc_grow(k, k->hdr->count + 1) != 0)
		{ MCC_TRACE("br\n"); return -1; }
	dst = k->tuples + at * k->arity;
	if (at < k->hdr->count)
		{ MCC_TRACE("br\n"); memmove(dst + k->arity, dst,
						(size_t)(k->hdr->count - at) * k->arity * sizeof(int64_t)); }
	memcpy(dst, tuple, (size_t)k->arity * sizeof(int64_t));
	k->hdr->count++;
	if (!k->anon)
		{ MCC_TRACE("br\n"); msync(k->map, k->map_len, MS_SYNC); }
	return 1;
}

static int64_t mccjit_kgc_call1(MccjitKgc *k, void *variant, void *baseline,
																int64_t x, int *flagged) { MCC_TRACE("enter\n");
	int (*vf)(int) = (int (*)(int))variant;
	int (*bf)(int) = (int (*)(int))baseline;
	int64_t tuple[MCCJIT_KGC_ARITY];
	int64_t bval, vval;
	uint32_t i;
	for (i = 0; i < MCCJIT_KGC_ARITY; i++)
		{ MCC_TRACE("br\n"); tuple[i] = 0; }
	tuple[0] = x;
	pthread_mutex_lock(&k->lock);
	if (k->memoize_ok && mccjit_kgc_contains(k, tuple)) { MCC_TRACE("br\n");
		pthread_mutex_unlock(&k->lock);
		return (int64_t)vf((int)x);
	}
	bval = (int64_t)bf((int)x);
	vval = (int64_t)vf((int)x);
	if (vval == bval) { MCC_TRACE("br\n");
		if (k->memoize_ok)
			{ MCC_TRACE("br\n"); mccjit_kgc_insert(k, tuple); }
		pthread_mutex_unlock(&k->lock);
		return bval;
	}
	pthread_mutex_unlock(&k->lock);
	if (flagged)
		{ MCC_TRACE("br\n"); *flagged = 1; }
	return bval;
}

static int64_t mccjit_invoke(void *fn, const int64_t *a, uint32_t n, int wide) { MCC_TRACE("enter\n");
	switch (n) { MCC_TRACE("br\n");
	case 1:
		return wide ? (int64_t)((long (*)(long))fn)(a[0])
								: (int64_t)((int (*)(long))fn)(a[0]);
	case 2:
		return wide ? (int64_t)((long (*)(long, long))fn)(a[0], a[1])
								: (int64_t)((int (*)(long, long))fn)(a[0], a[1]);
	case 3:
		return wide ? (int64_t)((long (*)(long, long, long))fn)(a[0], a[1], a[2])
								: (int64_t)((int (*)(long, long, long))fn)(a[0], a[1], a[2]);
	case 4:
		return wide
							 ? (int64_t)((long (*)(long, long, long, long))fn)(a[0], a[1], a[2],
																																 a[3])
							 : (int64_t)((int (*)(long, long, long, long))fn)(a[0], a[1], a[2],
																															 a[3]);
	case 5:
		return wide ? (int64_t)((long (*)(long, long, long, long, long))fn)(
											a[0], a[1], a[2], a[3], a[4])
								: (int64_t)((int (*)(long, long, long, long, long))fn)(
											a[0], a[1], a[2], a[3], a[4]);
	case 6:
		return wide ? (int64_t)((long (*)(long, long, long, long, long, long))fn)(
											a[0], a[1], a[2], a[3], a[4], a[5])
								: (int64_t)((int (*)(long, long, long, long, long, long))fn)(
											a[0], a[1], a[2], a[3], a[4], a[5]);
	default:
		return 0;
	}
}

/* K4A: scalar all-double marshalling (SSE class). Args come in via a spilled
   xmm0-7 double vector; the return is a double (xmm0). Fixed C signatures let
   the compiler place args in xmm and read the xmm0 return, so no hand-emitted
   argument classifier is needed for the all-FP case. */
static double mccjit_invoke_fp(void *fn, const double *a, uint32_t n) { MCC_TRACE("enter\n");
	switch (n) { MCC_TRACE("br\n");
	case 1:
		return ((double (*)(double))fn)(a[0]);
	case 2:
		return ((double (*)(double, double))fn)(a[0], a[1]);
	case 3:
		return ((double (*)(double, double, double))fn)(a[0], a[1], a[2]);
	case 4:
		return ((double (*)(double, double, double, double))fn)(a[0], a[1], a[2],
																														a[3]);
	case 5:
		return ((double (*)(double, double, double, double, double))fn)(
				a[0], a[1], a[2], a[3], a[4]);
	case 6:
		return ((double (*)(double, double, double, double, double, double))fn)(
				a[0], a[1], a[2], a[3], a[4], a[5]);
	default:
		return 0.0;
	}
}

static volatile int64_t mccjit_bench_sink;

static long mccjit_bench_iters(void) { MCC_TRACE("enter\n");
	const char *e = getenv("MCC_JIT_BENCH_ITERS");
	if (e && e[0]) { MCC_TRACE("br\n");
		long v = strtol(e, NULL, 10);
		if (v > 0)
			{ MCC_TRACE("br\n"); return v; }
	}
	return 100000;
}

static int mccjit_bench_margin_pct(void) { MCC_TRACE("enter\n");
	const char *e = getenv("MCC_JIT_BENCH_MARGIN_PCT");
	if (e && e[0]) { MCC_TRACE("br\n");
		long v = strtol(e, NULL, 10);
		if (v >= 0 && v <= 100)
			{ MCC_TRACE("br\n"); return (int)v; }
	}
	return 6;
}

static double mccjit_bench_run(void *fn, const int64_t *tuples, uint32_t ntuples,
															uint32_t nargs, int wide, uint32_t reps) { MCC_TRACE("enter\n");
	struct timespec t0;
	int64_t sink = 0;
	uint32_t r, i;
	if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0)
		{ MCC_TRACE("br\n"); return 1e300; }
	for (r = 0; r < reps; r++)
		{ MCC_TRACE("br\n"); for (i = 0; i < ntuples; i++)
			{ MCC_TRACE("br\n"); sink += mccjit_invoke(fn, tuples + (size_t)i * MCCJIT_KGC_ARITY, nargs, wide); } }
	mccjit_bench_sink ^= sink;
	return mccjit_elapsed(&t0);
}

/* K5/L4A/L5A promotion scorer: best-of-3 wall-clock benchmark of a candidate
   vs the incumbent over a caller-supplied live-in set (the real observed
   distribution, provided by J6A once it lands). Returns 1 only if the
   candidate is faster by more than a hysteresis margin — the incumbent wins
   ties (L5A). Work is bounded by a deterministic inner-iteration cap, not
   wall-clock, so the decision is reproducible. */
static int mccjit_bench_pair(void *cand, void *incumbent, const int64_t *tuples,
														 uint32_t ntuples, uint32_t nargs, int wide) { MCC_TRACE("enter\n");
	double cb = 1e300, ib = 1e300;
	uint32_t k, reps;
	long iters;
	int margin;
	if (!cand || !incumbent || !tuples || ntuples == 0 || nargs == 0)
		{ MCC_TRACE("br\n"); return 1; }
	iters = mccjit_bench_iters();
	margin = mccjit_bench_margin_pct();
	reps = (uint32_t)(iters / (long)ntuples);
	if (reps < 1)
		{ MCC_TRACE("br\n"); reps = 1; }
	for (k = 0; k < 3; k++) { MCC_TRACE("br\n");
		double c = mccjit_bench_run(cand, tuples, ntuples, nargs, wide, reps);
		double i2 = mccjit_bench_run(incumbent, tuples, ntuples, nargs, wide, reps);
		if (c < cb)
			{ MCC_TRACE("br\n"); cb = c; }
		if (i2 < ib)
			{ MCC_TRACE("br\n"); ib = i2; }
	}
	return cb * (100.0 + (double)margin) < ib * 100.0;
}

/* L4A — the K5 promotion scorer over the REAL observed distribution: benchmark
   a candidate vs the incumbent on the live-in tuples J6A captured on the hot
   counter (st->sample), not a synthetic sweep. Real observed values are the
   only safe input (a synthesized pointer/divisor would crash the callee). With
   no observed samples yet there is no basis to reject, so promotion is allowed
   (the incumbent-wins-on-tie hysteresis still lives in mccjit_bench_pair). */
MCCJIT_LOCAL int mccjit_promote_by_profile(void *cand, void *incumbent,
																					 const MccjitCounterState *st,
																					 uint32_t nargs, int wide) { MCC_TRACE("enter\n");
	int64_t tuples[MCCJIT_PROFILE_SAMPLES * MCCJIT_KGC_ARITY];
	uint32_t nt, i, j;
	if (!cand || !incumbent || !st || st->nsample <= 0 || nargs == 0)
		{ MCC_TRACE("br\n"); return 1; }
	nt = (uint32_t)st->nsample;
	if (nt > MCCJIT_PROFILE_SAMPLES)
		{ MCC_TRACE("br\n"); nt = MCCJIT_PROFILE_SAMPLES; }
	for (i = 0; i < nt; i++)
		{ MCC_TRACE("br\n"); for (j = 0; j < MCCJIT_KGC_ARITY; j++)
			{ MCC_TRACE("br\n"); tuples[i * MCCJIT_KGC_ARITY + j] = st->sample[i][j]; } }
	return mccjit_bench_pair(cand, incumbent, tuples, nt, nargs, wide);
}

static int64_t mccjit_kgc_calln(MccjitKgc *k, void *variant, void *baseline,
																const int64_t *argv, uint32_t nargs,
																int *flagged) { MCC_TRACE("enter\n");
	int wide = k->ret_wide;
	int64_t tuple[MCCJIT_KGC_ARITY];
	int64_t bval, vval;
	uint32_t i;
	for (i = 0; i < MCCJIT_KGC_ARITY; i++)
		{ MCC_TRACE("br\n"); tuple[i] = 0; }
	for (i = 0; i < nargs && i < MCCJIT_KGC_ARITY; i++)
		{ MCC_TRACE("br\n"); tuple[i] = argv[i]; }
	pthread_mutex_lock(&k->lock);
	if (k->poisoned) { MCC_TRACE("br\n");
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke(baseline, argv, nargs, wide);
	}
	if (k->memoize_ok && mccjit_kgc_contains(k, tuple)) { MCC_TRACE("br\n");
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke(variant, argv, nargs, wide);
	}
	bval = mccjit_invoke(baseline, argv, nargs, wide);
	vval = mccjit_invoke(variant, argv, nargs, wide);
	if (vval == bval) { MCC_TRACE("br\n");
		k->hits++;
		if (mcc_stats_mask)
			{ MCC_TRACE("br\n"); mcc_stats_jit_kgc_hit(); }
		if (k->memoize_ok)
			{ MCC_TRACE("br\n"); mccjit_kgc_insert(k, tuple); }
		pthread_mutex_unlock(&k->lock);
		return bval;
	}
	k->misses++;
	if (mcc_stats_mask)
		{ MCC_TRACE("br\n"); mcc_stats_jit_kgc_miss(); }
	{
		uint64_t total = k->hits + k->misses;
		if (total >= (uint64_t)mccjit_poison_min() &&
				k->misses * 100 >= total * (uint64_t)mccjit_poison_pct()) { MCC_TRACE("br\n");
			if (mcc_stats_mask && !k->poisoned)
				{ MCC_TRACE("br\n"); mcc_stats_jit_poison(); }
			k->poisoned = 1;
		}
	}
	pthread_mutex_unlock(&k->lock);
	if (flagged)
		{ MCC_TRACE("br\n"); *flagged = 1; }
	return bval;
}

/* All-double differential verify. Args/return are doubles; the memo key and
   the mismatch check use the raw bit pattern (a faithful recompile is
   bit-identical, and treating +0/-0 or a NaN-bit difference as a mismatch is
   the conservative-correct choice — it just returns the baseline). */
static double mccjit_kgc_calln_fp(MccjitKgc *k, void *variant, void *baseline,
																	const double *argv, uint32_t nargs,
																	int *flagged) { MCC_TRACE("enter\n");
	int64_t tuple[MCCJIT_KGC_ARITY];
	double bval, vval;
	uint64_t bbits = 0, vbits = 0;
	uint32_t i;
	for (i = 0; i < MCCJIT_KGC_ARITY; i++)
		{ MCC_TRACE("br\n"); tuple[i] = 0; }
	for (i = 0; i < nargs && i < MCCJIT_KGC_ARITY; i++)
		{ MCC_TRACE("br\n"); memcpy(&tuple[i], &argv[i], sizeof tuple[i]); }
	pthread_mutex_lock(&k->lock);
	if (k->poisoned) { MCC_TRACE("br\n");
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke_fp(baseline, argv, nargs);
	}
	if (k->memoize_ok && mccjit_kgc_contains(k, tuple)) { MCC_TRACE("br\n");
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke_fp(variant, argv, nargs);
	}
	bval = mccjit_invoke_fp(baseline, argv, nargs);
	vval = mccjit_invoke_fp(variant, argv, nargs);
	memcpy(&bbits, &bval, sizeof bbits);
	memcpy(&vbits, &vval, sizeof vbits);
	if (vbits == bbits) { MCC_TRACE("br\n");
		k->hits++;
		if (k->memoize_ok)
			{ MCC_TRACE("br\n"); mccjit_kgc_insert(k, tuple); }
		pthread_mutex_unlock(&k->lock);
		return bval;
	}
	k->misses++;
	{
		uint64_t total = k->hits + k->misses;
		if (total >= (uint64_t)mccjit_poison_min() &&
				k->misses * 100 >= total * (uint64_t)mccjit_poison_pct())
			{ MCC_TRACE("br\n"); k->poisoned = 1; }
	}
	pthread_mutex_unlock(&k->lock);
	if (flagged)
		{ MCC_TRACE("br\n"); *flagged = 1; }
	return bval;
}

#if defined(__x86_64__)
/* K4A/L12A scalar mixed GP+FP marshalling. One signature-agnostic forwarding
   thunk reconstructs any scalar SysV call: it loads gpv[0..5] into rdi..r9 and
   fpv[0..7] into xmm0..7 and tail-calls the target. Because SysV assigns
   INTEGER-class args to the GP regs and SSE-class args to the XMM regs with
   independent counters, per-class arrays (gpv,fpv) placed in class order fully
   reconstruct the call — no per-arg class vector is needed. Unused registers are
   loaded with harmless garbage the callee ignores. Same bytes, cast to a GP-int
   or a double return per the callee's return class. */
static const unsigned char mccjit_mixed_thunk_code[] = {
		0x55, 0x48, 0x89, 0xe5, 0x49, 0x89, 0xfb, 0x49, 0x89, 0xd2, 0xf2, 0x41,
		0x0f, 0x10, 0x02, 0xf2, 0x41, 0x0f, 0x10, 0x4a, 0x08, 0xf2, 0x41, 0x0f,
		0x10, 0x52, 0x10, 0xf2, 0x41, 0x0f, 0x10, 0x5a, 0x18, 0xf2, 0x41, 0x0f,
		0x10, 0x62, 0x20, 0xf2, 0x41, 0x0f, 0x10, 0x6a, 0x28, 0xf2, 0x41, 0x0f,
		0x10, 0x72, 0x30, 0xf2, 0x41, 0x0f, 0x10, 0x7a, 0x38, 0x48, 0x8b, 0x3e,
		0x48, 0x8b, 0x56, 0x10, 0x48, 0x8b, 0x4e, 0x18, 0x4c, 0x8b, 0x46, 0x20,
		0x4c, 0x8b, 0x4e, 0x28, 0x48, 0x8b, 0x76, 0x08, 0xb0, 0x08, 0x41, 0xff,
		0xd3, 0xc9, 0xc3};

typedef int64_t (*MccjitThunkI)(void *fn, const int64_t *gpv, const double *fpv);
typedef double (*MccjitThunkD)(void *fn, const int64_t *gpv, const double *fpv);

static void *mccjit_mixed_thunk;
static pthread_once_t mccjit_mixed_thunk_once = PTHREAD_ONCE_INIT;

static void mccjit_mixed_thunk_build(void) { MCC_TRACE("enter\n");
	unsigned char *p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
													MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) { MCC_TRACE("br\n");
		mccjit_mixed_thunk = NULL;
		return;
	}
	memcpy(p, mccjit_mixed_thunk_code, sizeof mccjit_mixed_thunk_code);
	mccjit_mixed_thunk = p;
}

static void *mccjit_mixed_thunk_get(void) { MCC_TRACE("enter\n");
	pthread_once(&mccjit_mixed_thunk_once, mccjit_mixed_thunk_build);
	return mccjit_mixed_thunk;
}

static int64_t mccjit_invoke_mixed_i(void *fn, const int64_t *gpv,
																		 const double *fpv) { MCC_TRACE("enter\n");
	return ((MccjitThunkI)mccjit_mixed_thunk_get())(fn, gpv, fpv);
}

static double mccjit_invoke_mixed_d(void *fn, const int64_t *gpv,
																		const double *fpv) { MCC_TRACE("enter\n");
	return ((MccjitThunkD)mccjit_mixed_thunk_get())(fn, gpv, fpv);
}

static void mccjit_mixed_key(const MccjitKgc *k, const int64_t *gpv,
														 const double *fpv, int64_t *tuple) { MCC_TRACE("enter\n");
	uint32_t i, a = 0;
	for (i = 0; i < MCCJIT_KGC_ARITY; i++)
		{ MCC_TRACE("br\n"); tuple[i] = 0; }
	for (i = 0; i < k->mx_ngp && a < MCCJIT_KGC_ARITY; i++, a++)
		{ MCC_TRACE("br\n"); tuple[a] = gpv[i]; }
	for (i = 0; i < k->mx_nsse && a < MCCJIT_KGC_ARITY; i++, a++)
		{ MCC_TRACE("br\n"); memcpy(&tuple[a], &fpv[i], sizeof tuple[a]); }
}

static void mccjit_mixed_poison_update(MccjitKgc *k) { MCC_TRACE("enter\n");
	uint64_t total = k->hits + k->misses;
	if (total >= (uint64_t)mccjit_poison_min() &&
			k->misses * 100 >= total * (uint64_t)mccjit_poison_pct())
		{ MCC_TRACE("br\n"); k->poisoned = 1; }
}

static int64_t mccjit_kgc_calln_mixed_i(MccjitKgc *k, const int64_t *gpv,
																				const double *fpv) { MCC_TRACE("enter\n");
	void *variant = k->mx_variant, *baseline = k->mx_baseline;
	int64_t tuple[MCCJIT_KGC_ARITY];
	int64_t bval, vval, bc, vc;
	mccjit_mixed_key(k, gpv, fpv, tuple);
	pthread_mutex_lock(&k->lock);
	if (k->poisoned) { MCC_TRACE("br\n");
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke_mixed_i(baseline, gpv, fpv);
	}
	if (k->memoize_ok && mccjit_kgc_contains(k, tuple)) { MCC_TRACE("br\n");
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke_mixed_i(variant, gpv, fpv);
	}
	bval = mccjit_invoke_mixed_i(baseline, gpv, fpv);
	vval = mccjit_invoke_mixed_i(variant, gpv, fpv);
	bc = k->ret_wide ? bval : (int64_t)(int32_t)bval;
	vc = k->ret_wide ? vval : (int64_t)(int32_t)vval;
	if (vc == bc) { MCC_TRACE("br\n");
		k->hits++;
		if (k->memoize_ok)
			{ MCC_TRACE("br\n"); mccjit_kgc_insert(k, tuple); }
		pthread_mutex_unlock(&k->lock);
		return bval;
	}
	k->misses++;
	mccjit_mixed_poison_update(k);
	pthread_mutex_unlock(&k->lock);
	if (k->mx_flag)
		{ MCC_TRACE("br\n"); *k->mx_flag = 1; }
	return bval;
}

static double mccjit_kgc_calln_mixed_d(MccjitKgc *k, const int64_t *gpv,
																			 const double *fpv) { MCC_TRACE("enter\n");
	void *variant = k->mx_variant, *baseline = k->mx_baseline;
	int64_t tuple[MCCJIT_KGC_ARITY];
	double bval, vval;
	uint64_t bbits = 0, vbits = 0;
	mccjit_mixed_key(k, gpv, fpv, tuple);
	pthread_mutex_lock(&k->lock);
	if (k->poisoned) { MCC_TRACE("br\n");
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke_mixed_d(baseline, gpv, fpv);
	}
	if (k->memoize_ok && mccjit_kgc_contains(k, tuple)) { MCC_TRACE("br\n");
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke_mixed_d(variant, gpv, fpv);
	}
	bval = mccjit_invoke_mixed_d(baseline, gpv, fpv);
	vval = mccjit_invoke_mixed_d(variant, gpv, fpv);
	memcpy(&bbits, &bval, sizeof bbits);
	memcpy(&vbits, &vval, sizeof vbits);
	if (vbits == bbits) { MCC_TRACE("br\n");
		k->hits++;
		if (k->memoize_ok)
			{ MCC_TRACE("br\n"); mccjit_kgc_insert(k, tuple); }
		pthread_mutex_unlock(&k->lock);
		return bval;
	}
	k->misses++;
	mccjit_mixed_poison_update(k);
	pthread_mutex_unlock(&k->lock);
	if (k->mx_flag)
		{ MCC_TRACE("br\n"); *k->mx_flag = 1; }
	return bval;
}

static void *mccjit_make_kgc_stub_fp(void *variant, void *baseline,
																		 int memoize_ok, uint32_t nargs) { MCC_TRACE("enter\n");
	unsigned char *p;
	MccjitKgc *kgc;
	int *flag;
	void *calln = (void *)mccjit_kgc_calln_fp;
	void *fp;
	size_t o = 0;
	uint32_t i;
	if (nargs < 1 || nargs > MCCJIT_KGC_MAXARG)
		{ MCC_TRACE("br\n"); return NULL; }
	kgc = mcc_mallocz(sizeof *kgc);
	if (!kgc)
		{ MCC_TRACE("br\n"); return NULL; }
	if (mccjit_kgc_open(kgc, NULL, mccjit_salt_witness(), nargs) != 0) { MCC_TRACE("br\n");
		mcc_free(kgc);
		return NULL;
	}
	kgc->memoize_ok = memoize_ok;
	kgc->ret_wide = 1;
	p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
					 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) { MCC_TRACE("br\n");
		mccjit_kgc_close(kgc);
		mcc_free(kgc);
		return NULL;
	}
	flag = (int *)(p + 256);
	*flag = 0;
	fp = flag;
	p[o++] = 0xc9;
	p[o++] = 0x55;
	p[o++] = 0x48;
	p[o++] = 0x83;
	p[o++] = 0xec;
	p[o++] = 0x40;
	for (i = 0; i < nargs; i++) { MCC_TRACE("br\n");
		unsigned char disp = (unsigned char)(i * 8);
		p[o++] = 0xf2;
		p[o++] = 0x0f;
		p[o++] = 0x11;
		p[o++] = (unsigned char)(0x44 + i * 8);
		p[o++] = 0x24;
		p[o++] = disp;
	}
	p[o++] = 0x48;
	p[o++] = 0xbf;
	memcpy(p + o, &kgc, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0xbe;
	memcpy(p + o, &variant, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0xba;
	memcpy(p + o, &baseline, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0x89;
	p[o++] = 0xe1;
	p[o++] = 0x41;
	p[o++] = 0xb8;
	memcpy(p + o, &nargs, 4);
	o += 4;
	p[o++] = 0x49;
	p[o++] = 0xb9;
	memcpy(p + o, &fp, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0xb8;
	memcpy(p + o, &calln, 8);
	o += 8;
	p[o++] = 0xff;
	p[o++] = 0xd0;
	p[o++] = 0x48;
	p[o++] = 0x83;
	p[o++] = 0xc4;
	p[o++] = 0x40;
	p[o++] = 0x5d;
	p[o++] = 0xc3;
	return p;
}

static void *mccjit_make_kgc_stub_n(void *variant, void *baseline, int memoize_ok,
																		const uint32_t *param_t, uint32_t nargs,
																		int ret_wide) { MCC_TRACE("enter\n");
	static const unsigned char mov64_pre[MCCJIT_KGC_MAXARG][4] = {
			{0x48, 0x89, 0x7c, 0x24}, {0x48, 0x89, 0x74, 0x24},
			{0x48, 0x89, 0x54, 0x24}, {0x48, 0x89, 0x4c, 0x24},
			{0x4c, 0x89, 0x44, 0x24}, {0x4c, 0x89, 0x4c, 0x24}};
	static const unsigned char movsxd[MCCJIT_KGC_MAXARG][3] = {
			{0x48, 0x63, 0xc7}, {0x48, 0x63, 0xc6}, {0x48, 0x63, 0xc2},
			{0x48, 0x63, 0xc1}, {0x49, 0x63, 0xc0}, {0x49, 0x63, 0xc1}};
	unsigned char *p;
	MccjitKgc *kgc;
	int *flag;
	void *calln = (void *)mccjit_kgc_calln;
	void *fp;
	size_t o = 0;
	uint32_t i;
	if (nargs < 1 || nargs > MCCJIT_KGC_MAXARG)
		{ MCC_TRACE("br\n"); return NULL; }
	kgc = mcc_mallocz(sizeof *kgc);
	if (!kgc)
		{ MCC_TRACE("br\n"); return NULL; }
	if (mccjit_kgc_open(kgc, NULL, mccjit_salt_witness(), nargs) != 0) { MCC_TRACE("br\n");
		mcc_free(kgc);
		return NULL;
	}
	kgc->memoize_ok = memoize_ok;
	kgc->ret_wide = ret_wide;
	p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
					 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) { MCC_TRACE("br\n");
		mccjit_kgc_close(kgc);
		mcc_free(kgc);
		return NULL;
	}
	flag = (int *)(p + 256);
	*flag = 0;
	fp = flag;
	p[o++] = 0xc9;
	p[o++] = 0x55;
	p[o++] = 0x48;
	p[o++] = 0x83;
	p[o++] = 0xec;
	p[o++] = 0x40;
	for (i = 0; i < nargs; i++) { MCC_TRACE("br\n");
		unsigned char disp = (unsigned char)(i * 8);
		if (mccjit_type_wide((int)param_t[i])) { MCC_TRACE("br\n");
			memcpy(p + o, mov64_pre[i], 4);
			o += 4;
			p[o++] = disp;
		} else { MCC_TRACE("br\n");
			memcpy(p + o, movsxd[i], 3);
			o += 3;
			p[o++] = 0x48;
			p[o++] = 0x89;
			p[o++] = 0x44;
			p[o++] = 0x24;
			p[o++] = disp;
		}
	}
	p[o++] = 0x48;
	p[o++] = 0xbf;
	memcpy(p + o, &kgc, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0xbe;
	memcpy(p + o, &variant, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0xba;
	memcpy(p + o, &baseline, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0x89;
	p[o++] = 0xe1;
	p[o++] = 0x41;
	p[o++] = 0xb8;
	memcpy(p + o, &nargs, 4);
	o += 4;
	p[o++] = 0x49;
	p[o++] = 0xb9;
	memcpy(p + o, &fp, 8);
	o += 8;
	p[o++] = 0x48;
	p[o++] = 0xb8;
	memcpy(p + o, &calln, 8);
	o += 8;
	p[o++] = 0xff;
	p[o++] = 0xd0;
	p[o++] = 0x48;
	p[o++] = 0x83;
	p[o++] = 0xc4;
	p[o++] = 0x40;
	p[o++] = 0x5d;
	p[o++] = 0xc3;
	return p;
}

/* K4A/L12A mixed stub: spill ALL 6 GP arg regs to gpv[0..5] and ALL 8 XMM to
   fpv[0..7] (uniform, no per-arg logic — SysV's independent GP/SSE counters mean
   gpv[k]/fpv[k] are exactly the k-th arg of that class), then call the mixed
   verify calln (i or d by the return class). Leading `leave` is the dispatch-only
   entry quirk shared with the GP/FP stubs. The kgc immediate is patched at
   offset 88, the calln address at offset 106 (both movabs-loaded). */
static void *mccjit_make_kgc_stub_mixed(void *variant, void *baseline,
																				int memoize_ok, uint32_t ngp,
																				uint32_t nsse, int ret_fp, int ret_wide) { MCC_TRACE("enter\n");
	static const unsigned char tmpl[] = {
			0xc9, 0x55, 0x48, 0x81, 0xec, 0x80, 0x00, 0x00, 0x00, 0x48, 0x89, 0x3c,
			0x24, 0x48, 0x89, 0x74, 0x24, 0x08, 0x48, 0x89, 0x54, 0x24, 0x10, 0x48,
			0x89, 0x4c, 0x24, 0x18, 0x4c, 0x89, 0x44, 0x24, 0x20, 0x4c, 0x89, 0x4c,
			0x24, 0x28, 0xf2, 0x0f, 0x11, 0x44, 0x24, 0x30, 0xf2, 0x0f, 0x11, 0x4c,
			0x24, 0x38, 0xf2, 0x0f, 0x11, 0x54, 0x24, 0x40, 0xf2, 0x0f, 0x11, 0x5c,
			0x24, 0x48, 0xf2, 0x0f, 0x11, 0x64, 0x24, 0x50, 0xf2, 0x0f, 0x11, 0x6c,
			0x24, 0x58, 0xf2, 0x0f, 0x11, 0x74, 0x24, 0x60, 0xf2, 0x0f, 0x11, 0x7c,
			0x24, 0x68, 0x48, 0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x48, 0x89, 0xe6, 0x48, 0x8d, 0x54, 0x24, 0x30, 0x48, 0xb8, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xd0, 0x48, 0x81, 0xc4, 0x80,
			0x00, 0x00, 0x00, 0x5d, 0xc3};
	unsigned char *p;
	MccjitKgc *kgc;
	int *flag;
	void *calln = ret_fp ? (void *)mccjit_kgc_calln_mixed_d
											 : (void *)mccjit_kgc_calln_mixed_i;
	uint32_t arity = ngp + nsse;
	if (arity < 1 || arity > MCCJIT_KGC_ARITY)
		{ MCC_TRACE("br\n"); return NULL; }
	if (!mccjit_mixed_thunk_get())
		{ MCC_TRACE("br\n"); return NULL; }
	kgc = mcc_mallocz(sizeof *kgc);
	if (!kgc)
		{ MCC_TRACE("br\n"); return NULL; }
	if (mccjit_kgc_open(kgc, NULL, mccjit_salt_witness(), arity) != 0) { MCC_TRACE("br\n");
		mcc_free(kgc);
		return NULL;
	}
	kgc->memoize_ok = memoize_ok;
	kgc->ret_wide = ret_wide;
	kgc->mx_variant = variant;
	kgc->mx_baseline = baseline;
	kgc->mx_ngp = ngp;
	kgc->mx_nsse = nsse;
	kgc->mx_ret_fp = ret_fp;
	p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
					 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) { MCC_TRACE("br\n");
		mccjit_kgc_close(kgc);
		mcc_free(kgc);
		return NULL;
	}
	flag = (int *)(p + 256);
	*flag = 0;
	kgc->mx_flag = flag;
	memcpy(p, tmpl, sizeof tmpl);
	memcpy(p + 88, &kgc, 8);
	memcpy(p + 106, &calln, 8);
	return p;
}
#elif defined(__aarch64__)
#define MCCJIT_A64_W(word) do { uint32_t w_ = (uint32_t)(word); memcpy(p + o, &w_, 4); o += 4; } while (0)
#define MCCJIT_A64_LDR(T, slotoff) \
	MCCJIT_A64_W(0x58000000u | ((((uint32_t)((slotoff) - o) / 4) & 0x7ffffu) << 5) | (T))
static void *mccjit_make_kgc_stub_n(void *variant, void *baseline, int memoize_ok,
																		const uint32_t *param_t, uint32_t nargs,
																		int ret_wide) { MCC_TRACE("enter\n");
	void *calln = (void *)mccjit_kgc_calln;
	size_t page = host_pagesize();
	const uint32_t D = 256;
	unsigned char *p;
	MccjitKgc *kgc;
	int *flag;
	uint64_t flagaddr, kp, vp, bp, cp;
	uint32_t i, o = 0;
	if (nargs < 1 || nargs > MCCJIT_KGC_MAXARG)
		{ MCC_TRACE("br\n"); return NULL; }
	kgc = mcc_mallocz(sizeof *kgc);
	flag = mcc_mallocz(sizeof *flag);
	if (!kgc || !flag) { MCC_TRACE("br\n");
		mcc_free(kgc);
		mcc_free(flag);
		return NULL;
	}
	if (mccjit_kgc_open(kgc, NULL, mccjit_salt_witness(), nargs) != 0) { MCC_TRACE("br\n");
		mcc_free(kgc);
		mcc_free(flag);
		return NULL;
	}
	kgc->memoize_ok = memoize_ok;
	kgc->ret_wide = ret_wide;
	p = mmap(0, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) { MCC_TRACE("br\n");
		mccjit_kgc_close(kgc);
		mcc_free(kgc);
		mcc_free(flag);
		return NULL;
	}
	MCCJIT_A64_W(0xa9bf7bfdu); /* stp x29,x30,[sp,#-16]! */
	MCCJIT_A64_W(0xd100c3ffu); /* sub sp,sp,#48 */
	for (i = 0; i < nargs; i++) { MCC_TRACE("br\n");
		if (mccjit_type_wide((int)param_t[i]))
			{ MCC_TRACE("br\n"); MCCJIT_A64_W(0xf90003e0u | (i << 10) | i); } /* str x_i,[sp,#i*8] */
		else { MCC_TRACE("br\n");
			MCCJIT_A64_W(0x93407c00u | (i << 5) | 8); /* sxtw x8,w_i */
			MCCJIT_A64_W(0xf90003e8u | (i << 10));    /* str x8,[sp,#i*8] */
		}
	}
	MCCJIT_A64_LDR(0, D + 0);  /* ldr x0,[kgc] */
	MCCJIT_A64_LDR(1, D + 8);  /* ldr x1,[variant] */
	MCCJIT_A64_LDR(2, D + 16); /* ldr x2,[baseline] */
	MCCJIT_A64_W(0x910003e3u); /* mov x3,sp */
	MCCJIT_A64_W(0x52800004u | (nargs << 5)); /* mov w4,#nargs */
	MCCJIT_A64_LDR(5, D + 24);  /* ldr x5,[&flag] */
	MCCJIT_A64_LDR(16, D + 32); /* ldr x16,[verify] */
	MCCJIT_A64_W(0xd63f0200u);  /* blr x16 */
	MCCJIT_A64_W(0x9100c3ffu);  /* add sp,sp,#48 */
	MCCJIT_A64_W(0xa8c17bfdu);  /* ldp x29,x30,[sp],#16 */
	MCCJIT_A64_W(0xd65f03c0u);  /* ret */
	*flag = 0;
	flagaddr = (uint64_t)(uintptr_t)flag;
	kp = (uint64_t)(uintptr_t)kgc;
	vp = (uint64_t)(uintptr_t)variant;
	bp = (uint64_t)(uintptr_t)baseline;
	cp = (uint64_t)(uintptr_t)calln;
	memcpy(p + D + 0, &kp, 8);
	memcpy(p + D + 8, &vp, 8);
	memcpy(p + D + 16, &bp, 8);
	memcpy(p + D + 24, &flagaddr, 8);
	memcpy(p + D + 32, &cp, 8);
	if (host_runmem_protect(p, page, HOST_PROT_RX) != 0) { MCC_TRACE("br\n");
		munmap(p, page);
		mccjit_kgc_close(kgc);
		mcc_free(kgc);
		mcc_free(flag);
		return NULL;
	}
	return p;
}
static void *mccjit_make_kgc_stub_fp(void *variant, void *baseline,
																		 int memoize_ok, uint32_t nargs) { MCC_TRACE("enter\n");
	void *calln = (void *)mccjit_kgc_calln_fp;
	size_t page = host_pagesize();
	const uint32_t D = 256;
	unsigned char *p;
	MccjitKgc *kgc;
	int *flag;
	uint64_t flagaddr, kp, vp, bp, cp;
	uint32_t i, o = 0;
	if (nargs < 1 || nargs > MCCJIT_KGC_MAXARG)
		{ MCC_TRACE("br\n"); return NULL; }
	kgc = mcc_mallocz(sizeof *kgc);
	flag = mcc_mallocz(sizeof *flag);
	if (!kgc || !flag) { MCC_TRACE("br\n");
		mcc_free(kgc);
		mcc_free(flag);
		return NULL;
	}
	if (mccjit_kgc_open(kgc, NULL, mccjit_salt_witness(), nargs) != 0) { MCC_TRACE("br\n");
		mcc_free(kgc);
		mcc_free(flag);
		return NULL;
	}
	kgc->memoize_ok = memoize_ok;
	kgc->ret_wide = 1;
	p = mmap(0, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) { MCC_TRACE("br\n");
		mccjit_kgc_close(kgc);
		mcc_free(kgc);
		mcc_free(flag);
		return NULL;
	}
	MCCJIT_A64_W(0xa9bf7bfdu); /* stp x29,x30,[sp,#-16]! */
	MCCJIT_A64_W(0xd100c3ffu); /* sub sp,sp,#48 */
	for (i = 0; i < nargs; i++) { MCC_TRACE("br\n");
		MCCJIT_A64_W(0xfd0003e0u | (i << 10) | i); /* str d_i,[sp,#i*8] */
	}
	MCCJIT_A64_LDR(0, D + 0);  /* ldr x0,[kgc] */
	MCCJIT_A64_LDR(1, D + 8);  /* ldr x1,[variant] */
	MCCJIT_A64_LDR(2, D + 16); /* ldr x2,[baseline] */
	MCCJIT_A64_W(0x910003e3u); /* mov x3,sp (argv of doubles by pointer) */
	MCCJIT_A64_W(0x52800004u | (nargs << 5)); /* mov w4,#nargs */
	MCCJIT_A64_LDR(5, D + 24);  /* ldr x5,[&flag] */
	MCCJIT_A64_LDR(16, D + 32); /* ldr x16,[verify] */
	MCCJIT_A64_W(0xd63f0200u);  /* blr x16 */
	MCCJIT_A64_W(0x9100c3ffu);  /* add sp,sp,#48 */
	MCCJIT_A64_W(0xa8c17bfdu);  /* ldp x29,x30,[sp],#16 */
	MCCJIT_A64_W(0xd65f03c0u);  /* ret */
	*flag = 0;
	flagaddr = (uint64_t)(uintptr_t)flag;
	kp = (uint64_t)(uintptr_t)kgc;
	vp = (uint64_t)(uintptr_t)variant;
	bp = (uint64_t)(uintptr_t)baseline;
	cp = (uint64_t)(uintptr_t)calln;
	memcpy(p + D + 0, &kp, 8);
	memcpy(p + D + 8, &vp, 8);
	memcpy(p + D + 16, &bp, 8);
	memcpy(p + D + 24, &flagaddr, 8);
	memcpy(p + D + 32, &cp, 8);
	if (host_runmem_protect(p, page, HOST_PROT_RX) != 0) { MCC_TRACE("br\n");
		munmap(p, page);
		mccjit_kgc_close(kgc);
		mcc_free(kgc);
		mcc_free(flag);
		return NULL;
	}
	return p;
}
static void *mccjit_make_kgc_stub_mixed(void *variant, void *baseline,
																				int memoize_ok, uint32_t ngp,
																				uint32_t nsse, int ret_fp, int ret_wide) { MCC_TRACE("enter\n");
	(void)variant;
	(void)baseline;
	(void)memoize_ok;
	(void)ngp;
	(void)nsse;
	(void)ret_fp;
	(void)ret_wide;
	return NULL;
}
#undef MCCJIT_A64_W
#undef MCCJIT_A64_LDR
#else
static void *mccjit_make_kgc_stub_n(void *variant, void *baseline, int memoize_ok,
																		const uint32_t *param_t, uint32_t nargs,
																		int ret_wide) { MCC_TRACE("enter\n");
	(void)variant;
	(void)baseline;
	(void)memoize_ok;
	(void)param_t;
	(void)nargs;
	(void)ret_wide;
	return NULL;
}
static void *mccjit_make_kgc_stub_fp(void *variant, void *baseline,
																		 int memoize_ok, uint32_t nargs) { MCC_TRACE("enter\n");
	(void)variant;
	(void)baseline;
	(void)memoize_ok;
	(void)nargs;
	return NULL;
}
static void *mccjit_make_kgc_stub_mixed(void *variant, void *baseline,
																				int memoize_ok, uint32_t ngp,
																				uint32_t nsse, int ret_fp, int ret_wide) { MCC_TRACE("enter\n");
	(void)variant;
	(void)baseline;
	(void)memoize_ok;
	(void)ngp;
	(void)nsse;
	(void)ret_fp;
	(void)ret_wide;
	return NULL;
}
#endif

int mccjit_selftest_kgc(void) { MCC_TRACE("enter\n");
	static const char src[] = "int f(int x){return x*2+1;}";
	MCCState *s1;
	int (*baseline)(int) = NULL;
	void *variant = NULL;
	MCCState *vstate = NULL;
	MccjitKgc kgc;
	int fails = 0;
	int variant_flagged = 0;
	int64_t inputs[6] = {7, 7, 5, 0, 3, 100};
	int i;

	mcc_free(mccjit_last_blob);
	mccjit_last_blob = NULL;
	mccjit_last_len = 0;
	mccjit_last_state = NULL;

	printf("mccjit-selftest-kgc: begin (arity=%d salt=%016llx)\n", MCCJIT_KGC_ARITY,
				 (unsigned long long)mccjit_salt_witness());

	s1 = mcc_new();
	if (!s1) { MCC_TRACE("br\n");
		printf("mccjit-selftest-kgc: mcc_new failed\n");
		return 1;
	}
	s1->optimize = 1;
	s1->nostdlib = 1;
	mcc_free(s1->jit_functions);
	s1->jit_functions = mcc_strdup("f");
	mcc_set_output_type(s1, MCC_OUTPUT_MEMORY);
	if (mcc_compile_string(s1, src) != 0 || !mccjit_last_blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-kgc: compile/stash failed\n");
		mcc_delete(s1);
		return 1;
	}
	if (mcc_relocate(s1) == 0)
		{ MCC_TRACE("br\n"); baseline = (int (*)(int))mcc_get_symbol(s1, "f"); }
	if (!baseline) { MCC_TRACE("br\n");
		printf("mccjit-selftest-kgc: no AOT baseline entry for f\n");
		mcc_delete(s1);
		return 1;
	}
	variant = mcc_jit_recompile_blob_spec(mccjit_last_blob, mccjit_last_len, 0, 7);
	if (!variant) { MCC_TRACE("br\n");
		printf("mccjit-selftest-kgc: wrongly-specialized variant recompile NULL\n");
		mcc_delete(s1);
		return 1;
	}
	vstate = mccjit_last_state;
	printf("mccjit-selftest-kgc: baseline f=%p variant spec[x==7]=%p v(0)=%d v(7)=%d\n",
				 (void *)baseline, variant, ((int (*)(int))variant)(0),
				 ((int (*)(int))variant)(7));

	if (mccjit_kgc_open(&kgc, NULL, mccjit_salt_witness(), 1) != 0) { MCC_TRACE("br\n");
		printf("mccjit-selftest-kgc: kgc open (anon) failed\n");
		if (vstate)
			{ MCC_TRACE("br\n"); mcc_delete(vstate); }
		mcc_delete(s1);
		return 1;
	}

	printf("mccjit-selftest-kgc:    x  path  variant  baseline  returned  flagged  ok\n");
	for (i = 0; i < 6; i++) { MCC_TRACE("br\n");
		int64_t x = inputs[i];
		int64_t tuple[MCCJIT_KGC_ARITY];
		int hit, flagged = 0, ok;
		int64_t returned, want = x * 2 + 1;
		int vv = ((int (*)(int))variant)((int)x);
		int bv = baseline((int)x);
		uint32_t j;
		for (j = 0; j < MCCJIT_KGC_ARITY; j++)
			{ MCC_TRACE("br\n"); tuple[j] = 0; }
		tuple[0] = x;
		hit = mccjit_kgc_contains(&kgc, tuple);
		returned = mccjit_kgc_call1(&kgc, variant, baseline, x, &flagged);
		if (flagged)
			{ MCC_TRACE("br\n"); variant_flagged = 1; }
		ok = (returned == want);
		if (i == 1 && !hit)
			{ MCC_TRACE("br\n"); ok = 0; }
		if (x != 7 && !flagged)
			{ MCC_TRACE("br\n"); ok = 0; }
		if (x == 7 && flagged)
			{ MCC_TRACE("br\n"); ok = 0; }
		printf("mccjit-selftest-kgc: %4lld  %-4s  %7d  %8d  %8lld  %7d  %s\n",
					 (long long)x, hit ? "HIT" : "MISS", vv, bv, (long long)returned,
					 flagged, ok ? "OK" : "FAIL");
		if (!ok)
			{ MCC_TRACE("br\n"); fails++; }
	}
	printf("mccjit-selftest-kgc: variant flagged-unsound=%d (expected 1)\n",
				 variant_flagged);
	if (!variant_flagged)
		{ MCC_TRACE("br\n"); fails++; }
	{
		int64_t t7[MCCJIT_KGC_ARITY];
		uint32_t j;
		for (j = 0; j < MCCJIT_KGC_ARITY; j++)
			{ MCC_TRACE("br\n"); t7[j] = 0; }
		t7[0] = 7;
		if (!mccjit_kgc_contains(&kgc, t7)) { MCC_TRACE("br\n");
			printf("mccjit-selftest-kgc: x=7 not cached after verify\n");
			fails++;
		}
	}
	mccjit_kgc_close(&kgc);

	{
		char path[] = "/tmp/mccjit_kgc_XXXXXX";
		int fd = mkstemp(path);
		uint64_t salt = mccjit_salt_witness();
		int64_t vals[5] = {100, -7, 42, 7, 3};
		int j;
		MccjitKgc p;
		uint64_t saved = 0;
		if (fd >= 0)
			{ MCC_TRACE("br\n"); close(fd); }
		if (fd < 0 || mccjit_kgc_open(&p, path, salt, 1) != 0) { MCC_TRACE("br\n");
			printf("mccjit-selftest-kgc: persistence open failed\n");
			fails++;
		} else { MCC_TRACE("br\n");
			for (j = 0; j < 5; j++) { MCC_TRACE("br\n");
				int64_t t[MCCJIT_KGC_ARITY];
				uint32_t m;
				for (m = 0; m < MCCJIT_KGC_ARITY; m++)
					{ MCC_TRACE("br\n"); t[m] = 0; }
				t[0] = vals[j];
				mccjit_kgc_insert(&p, t);
			}
			saved = p.hdr->count;
			mccjit_kgc_close(&p);
			printf("mccjit-selftest-kgc: persistence wrote %llu tuples, closed\n",
						 (unsigned long long)saved);
			if (mccjit_kgc_open(&p, path, salt, 1) != 0) { MCC_TRACE("br\n");
				printf("mccjit-selftest-kgc: persistence reopen failed\n");
				fails++;
			} else { MCC_TRACE("br\n");
				int survived = (p.hdr->count == saved);
				for (j = 0; j < 5; j++) { MCC_TRACE("br\n");
					int64_t t[MCCJIT_KGC_ARITY];
					uint32_t m;
					for (m = 0; m < MCCJIT_KGC_ARITY; m++)
						{ MCC_TRACE("br\n"); t[m] = 0; }
					t[0] = vals[j];
					if (!mccjit_kgc_contains(&p, t))
						{ MCC_TRACE("br\n"); survived = 0; }
				}
				printf("mccjit-selftest-kgc: reopened count=%llu survived=%s\n",
							 (unsigned long long)p.hdr->count, survived ? "yes" : "no");
				if (!survived)
					{ MCC_TRACE("br\n"); fails++; }
				mccjit_kgc_close(&p);
			}
			if (mccjit_kgc_open(&p, path, salt ^ 0xdeadbeefull, 1) == 0) { MCC_TRACE("br\n");
				printf("mccjit-selftest-kgc: stale-salt reopen count=%llu (expect 0 reset)\n",
							 (unsigned long long)p.hdr->count);
				if (p.hdr->count != 0)
					{ MCC_TRACE("br\n"); fails++; }
				mccjit_kgc_close(&p);
			}
			unlink(path);
		}
	}

	if (vstate)
		{ MCC_TRACE("br\n"); mcc_delete(vstate); }
	mccjit_last_state = NULL;
	mcc_delete(s1);
	printf("mccjit-selftest-kgc: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static int mccjit_classify_blob(const void *buf, size_t len) { MCC_TRACE("enter\n");
	MccjitIntent it;
	MCCState *js;
	int purity;
	js = mcc_new();
	if (!js)
		{ MCC_TRACE("br\n"); return -1; }
	js->optimize = 0;
	js->nostdlib = 1;
	mcc_set_output_type(js, MCC_OUTPUT_MEMORY);
	mcc_enter_state(js);
	mccpp_new(js);
	mccgen_init(js);
	anon_sym = SYM_FIRST_ANOM;
	funcname = "";
	func_ind = -1;
	if (mccjit_intent_deserialize(buf, len, &it) != 0) { MCC_TRACE("br\n");
		mcc_exit_state(js);
		mcc_delete(js);
		return -1;
	}
	purity = ast_fn_purity(it.arena);
	mccjit_intent_release(&it);
	mcc_exit_state(js);
	mcc_delete(js);
	return purity;
}

static int mccjit_slice_profile_blob(const void *buf, size_t len,
																		 AstSliceProfile *out) { MCC_TRACE("enter\n");
	MccjitIntent it;
	MCCState *js;
	js = mcc_new();
	if (!js)
		{ MCC_TRACE("br\n"); return -1; }
	js->optimize = 0;
	js->nostdlib = 1;
	mcc_set_output_type(js, MCC_OUTPUT_MEMORY);
	mcc_enter_state(js);
	mccpp_new(js);
	mccgen_init(js);
	anon_sym = SYM_FIRST_ANOM;
	funcname = "";
	func_ind = -1;
	if (mccjit_intent_deserialize(buf, len, &it) != 0) { MCC_TRACE("br\n");
		mcc_exit_state(js);
		mcc_delete(js);
		return -1;
	}
	ast_fn_slice_profile(it.arena, out);
	mccjit_intent_release(&it);
	mcc_exit_state(js);
	mcc_delete(js);
	return 0;
}

static const char *mccjit_purity_name(int p) { MCC_TRACE("enter\n");
	switch (p) { MCC_TRACE("br\n");
	case AST_PURITY_TIER0:
		return "TIER0";
	case AST_PURITY_TIER1:
		return "TIER1";
	case AST_PURITY_IMPURE:
		return "IMPURE";
	default:
		return "ERR";
	}
}

int mccjit_selftest_strlit(void) { MCC_TRACE("enter\n");
	static const char src[] = "int f(int i){ return \"ABCDE\"[i]; }";
	unsigned char *blob;
	size_t blen;
	MCCState *s1 = NULL;
	int (*jitf)(int) = NULL;
	int want[5] = {'A', 'B', 'C', 'D', 'E'};
	int fails = 0, i;

	printf("mccjit-selftest-strlit: begin (rodata string literal recompile)\n");

	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-strlit: stash failed (string literal bailed?) FAIL\n");
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return 1;
	}
	printf("mccjit-selftest-strlit: serialized string-using intent = %lu bytes OK\n",
				 (unsigned long)blen);

	jitf = (int (*)(int))mcc_jit_recompile_blob(blob, blen);
	if (!jitf) { MCC_TRACE("br\n");
		printf("mccjit-selftest-strlit: recompile returned NULL FAIL\n");
		mcc_free(blob);
		mcc_delete(s1);
		return 1;
	}

	for (i = 0; i < 5; i++) { MCC_TRACE("br\n");
		int got = jitf(i);
		int ok = (got == want[i]);
		printf("mccjit-selftest-strlit: f(%d)=%d expect=%d %s\n", i, got, want[i],
					 ok ? "OK" : "FAIL");
		if (!ok)
			{ MCC_TRACE("br\n"); fails++; }
	}

	if (mccjit_last_state)
		{ MCC_TRACE("br\n"); mcc_delete(mccjit_last_state); }
	mccjit_last_state = NULL;
	mcc_free(blob);
	mcc_delete(s1);

	{
		static const char src2[] =
				"int g(int i){ return i ? \"XY\"[0] : \"PQRS\"[2]; }";
		MCCState *s2 = NULL;
		unsigned char *b2;
		size_t l2;
		int (*g)(int) = NULL;
		b2 = mccjit_stash_one(src2, "g", 1, &l2, &s2);
		if (s2 && b2)
			{ MCC_TRACE("br\n"); g = (int (*)(int))mcc_jit_recompile_blob(b2, l2); }
		if (!g) { MCC_TRACE("br\n");
			printf("mccjit-selftest-strlit: two-string recompile NULL FAIL\n");
			fails++;
		} else { MCC_TRACE("br\n");
			int ok = (g(1) == 'X') && (g(0) == 'R');
			printf("mccjit-selftest-strlit: g(1)=%d g(0)=%d expect=88,82 %s\n", g(1),
						 g(0), ok ? "OK" : "FAIL");
			if (!ok)
				{ MCC_TRACE("br\n"); fails++; }
		}
		if (mccjit_last_state)
			{ MCC_TRACE("br\n"); mcc_delete(mccjit_last_state); }
		mccjit_last_state = NULL;
		mcc_free(b2);
		if (s2)
			{ MCC_TRACE("br\n"); mcc_delete(s2); }
	}

	{
		static const char src3[] =
				"int h(int i){ return \"lo\"[i & 1] + \"hi\"[i & 1]; }";
		MCCState *s3 = NULL;
		unsigned char *b3;
		size_t l3;
		int (*h)(int) = NULL;
		b3 = mccjit_stash_one(src3, "h", 1, &l3, &s3);
		if (s3 && b3)
			{ MCC_TRACE("br\n"); h = (int (*)(int))mcc_jit_recompile_blob(b3, l3); }
		if (!h) { MCC_TRACE("br\n");
			printf("mccjit-selftest-strlit: two-string-mix recompile NULL FAIL\n");
			fails++;
		} else { MCC_TRACE("br\n");
			int ok = (h(0) == 'l' + 'h') && (h(1) == 'o' + 'i');
			printf("mccjit-selftest-strlit: h(0)=%d h(1)=%d expect=%d,%d %s\n", h(0),
						 h(1), 'l' + 'h', 'o' + 'i', ok ? "OK" : "FAIL");
			if (!ok)
				{ MCC_TRACE("br\n"); fails++; }
		}
		if (mccjit_last_state)
			{ MCC_TRACE("br\n"); mcc_delete(mccjit_last_state); }
		mccjit_last_state = NULL;
		mcc_free(b3);
		if (s3)
			{ MCC_TRACE("br\n"); mcc_delete(s3); }
	}

	printf("mccjit-selftest-strlit: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_ptrret(void) { MCC_TRACE("enter\n");
	int fails = 0;

	printf("mccjit-selftest-ptrret: begin (pointer-returning recompile)\n");

	{
		static const char src[] = "char *h(char *p, int i){ return p + i; }";
		unsigned char *blob;
		size_t blen;
		MCCState *s1 = NULL;
		char *(*h)(char *, int) = NULL;
		char buf[8] = "ABCDEFG";
		blob = mccjit_stash_one(src, "h", 1, &blen, &s1);
		if (!s1 || !blob) { MCC_TRACE("br\n");
			printf("mccjit-selftest-ptrret: char* stash failed FAIL\n");
			fails++;
		} else { MCC_TRACE("br\n");
			h = (char *(*)(char *, int))mcc_jit_recompile_blob(blob, blen);
			if (!h) { MCC_TRACE("br\n");
				printf("mccjit-selftest-ptrret: char* recompile NULL FAIL\n");
				fails++;
			} else { MCC_TRACE("br\n");
				int ok = (h(buf, 0) == buf) && (h(buf, 3) == buf + 3) &&
								 (*h(buf, 3) == 'D');
				printf("mccjit-selftest-ptrret: h(buf,3)=%p buf+3=%p *=%c %s\n",
							 (void *)h(buf, 3), (void *)(buf + 3), *h(buf, 3),
							 ok ? "OK" : "FAIL");
				if (!ok)
					{ MCC_TRACE("br\n"); fails++; }
			}
		}
		if (mccjit_last_state)
			{ MCC_TRACE("br\n"); mcc_delete(mccjit_last_state); }
		mccjit_last_state = NULL;
		mcc_free(blob);
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
	}

	{
		static const char src[] = "int *g(int *p, int i){ return p + i; }";
		unsigned char *blob;
		size_t blen;
		MCCState *s1 = NULL;
		int *(*g)(int *, int) = NULL;
		int arr[4] = {10, 20, 30, 40};
		blob = mccjit_stash_one(src, "g", 1, &blen, &s1);
		if (!s1 || !blob) { MCC_TRACE("br\n");
			printf("mccjit-selftest-ptrret: int* stash failed FAIL\n");
			fails++;
		} else { MCC_TRACE("br\n");
			g = (int *(*)(int *, int))mcc_jit_recompile_blob(blob, blen);
			if (!g) { MCC_TRACE("br\n");
				printf("mccjit-selftest-ptrret: int* recompile NULL FAIL\n");
				fails++;
			} else { MCC_TRACE("br\n");
				int ok = (g(arr, 2) == arr + 2) && (*g(arr, 2) == 30);
				printf("mccjit-selftest-ptrret: g(arr,2)=%p arr+2=%p *=%d %s\n",
							 (void *)g(arr, 2), (void *)(arr + 2), *g(arr, 2),
							 ok ? "OK" : "FAIL");
				if (!ok)
					{ MCC_TRACE("br\n"); fails++; }
			}
		}
		if (mccjit_last_state)
			{ MCC_TRACE("br\n"); mcc_delete(mccjit_last_state); }
		mccjit_last_state = NULL;
		mcc_free(blob);
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
	}

	{
		static const char src[] = "char *s(int i){ return \"world\" + i; }";
		unsigned char *blob;
		size_t blen;
		MCCState *s1 = NULL;
		char *(*sf)(int) = NULL;
		blob = mccjit_stash_one(src, "s", 1, &blen, &s1);
		if (!s1 || !blob) { MCC_TRACE("br\n");
			printf("mccjit-selftest-ptrret: strlit+ptr stash failed FAIL\n");
			fails++;
		} else { MCC_TRACE("br\n");
			sf = (char *(*)(int))mcc_jit_recompile_blob(blob, blen);
			if (!sf) { MCC_TRACE("br\n");
				printf("mccjit-selftest-ptrret: strlit+ptr recompile NULL FAIL\n");
				fails++;
			} else { MCC_TRACE("br\n");
				int ok = (sf(0)[0] == 'w') && (sf(1)[0] == 'o') &&
								 (strcmp(sf(0), "world") == 0);
				printf("mccjit-selftest-ptrret: s(0)=\"%s\" s(1)[0]=%c %s\n", sf(0),
							 sf(1)[0], ok ? "OK" : "FAIL");
				if (!ok)
					{ MCC_TRACE("br\n"); fails++; }
			}
		}
		if (mccjit_last_state)
			{ MCC_TRACE("br\n"); mcc_delete(mccjit_last_state); }
		mccjit_last_state = NULL;
		mcc_free(blob);
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
	}

	printf("mccjit-selftest-ptrret: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_purity(void) { MCC_TRACE("enter\n");
	static const struct {
		const char *src;
		const char *fn;
		int nostdlib;
		int want;
	} cases[4] = {
			{"int f(int x){return x*2+1;}", "f", 1, AST_PURITY_TIER0},
			{"int g(int *p, int x){return p ? *p + x : -1;}", "g", 1,
			 AST_PURITY_TIER1},
			{"int s(int *p, int x){*p = x; return x;}", "s", 1, AST_PURITY_IMPURE},
			{"int abs(int); int h(int x){return abs(x) + 1;}", "h", 1,
			 AST_PURITY_IMPURE},
	};
	int fails = 0;
	int i;

	printf("mccjit-selftest-purity: begin\n");
	printf("mccjit-selftest-purity: classify  fn  got       want      ok\n");
	for (i = 0; i < 4; i++) { MCC_TRACE("br\n");
		unsigned char *blob;
		size_t blen;
		MCCState *s1;
		int got, ok;
		blob = mccjit_stash_one(cases[i].src, cases[i].fn, cases[i].nostdlib, &blen,
														&s1);
		if (!s1 || !blob) { MCC_TRACE("br\n");
			printf("mccjit-selftest-purity: %s stash failed\n", cases[i].fn);
			if (s1)
				{ MCC_TRACE("br\n"); mcc_delete(s1); }
			mcc_free(blob);
			fails++;
			continue;
		}
		got = mccjit_classify_blob(blob, blen);
		ok = (got == cases[i].want);
		printf("mccjit-selftest-purity:           %-3s %-9s %-9s %s\n", cases[i].fn,
					 mccjit_purity_name(got), mccjit_purity_name(cases[i].want),
					 ok ? "OK" : "FAIL");
		if (!ok)
			{ MCC_TRACE("br\n"); fails++; }
		mcc_free(blob);
		mcc_delete(s1);
	}

	{
		static const struct {
			const char *src;
			const char *fn;
			int want;
		} wire[2] = {
				{"int f(int x){return x*2+1;}", "f", AST_PURITY_TIER0},
				{"int g(int *p, int x){return p ? *p + x : -1;}", "g",
				 AST_PURITY_TIER1},
		};
		int w;
		printf("mccjit-selftest-purity: recompile-path wiring (mccjit_last_purity "
					 "set by mcc_jit_recompile_blob):\n");
		for (w = 0; w < 2; w++) { MCC_TRACE("br\n");
			unsigned char *blob;
			size_t blen;
			MCCState *s1;
			void *rj;
			int tier, ok;
			blob = mccjit_stash_one(wire[w].src, wire[w].fn, 1, &blen, &s1);
			if (!s1 || !blob) { MCC_TRACE("br\n");
				if (s1)
					{ MCC_TRACE("br\n"); mcc_delete(s1); }
				mcc_free(blob);
				fails++;
				continue;
			}
			rj = mcc_jit_recompile_blob(blob, blen);
			tier = mccjit_last_purity;
			if (rj && mccjit_last_state)
				{ MCC_TRACE("br\n"); mcc_delete(mccjit_last_state); }
			mccjit_last_state = NULL;
			ok = (tier == wire[w].want);
			printf("mccjit-selftest-purity:           %-3s -> %-6s memoize_ok=%d %s\n",
						 wire[w].fn, mccjit_purity_name(tier), tier == AST_PURITY_TIER0,
						 ok ? "OK" : "FAIL");
			if (!ok)
				{ MCC_TRACE("br\n"); fails++; }
			mcc_free(blob);
			mcc_delete(s1);
		}
	}

	{
		static const char src_t[] = "int *gp; int t(int x){return *gp + x;}";
		static const char src_tv[] = "int tv(int x){return x + 10;}";
		unsigned char *tblob = NULL, *vblob = NULL;
		size_t tlen = 0, vlen = 0;
		MCCState *st = NULL, *sv = NULL;
		int (*baseline)(int) = NULL;
		int (*variant)(int) = NULL;
		int **gpp = NULL;
		int cell = 0;
		int tier = -1;

		tblob = mccjit_stash_one(src_t, "t", 1, &tlen, &st);
		if (st && tblob && mcc_relocate(st) == 0) { MCC_TRACE("br\n");
			baseline = (int (*)(int))mcc_get_symbol(st, "t");
			gpp = (int **)mcc_get_symbol(st, "gp");
		}
		vblob = mccjit_stash_one(src_tv, "tv", 1, &vlen, &sv);
		if (sv && vblob && mcc_relocate(sv) == 0)
			{ MCC_TRACE("br\n"); variant = (int (*)(int))mcc_get_symbol(sv, "tv"); }

		if (tblob)
			{ MCC_TRACE("br\n"); tier = mccjit_classify_blob(tblob, tlen); }

		printf("mccjit-selftest-purity: gating demo tier(t)=%s (memoize_ok=%d)\n",
					 mccjit_purity_name(tier), tier == AST_PURITY_TIER0);
		if (tier != AST_PURITY_TIER1)
			{ MCC_TRACE("br\n"); fails++; }

		if (gpp)
			{ MCC_TRACE("br\n"); *gpp = &cell; }

		if (!baseline || !variant || !gpp) { MCC_TRACE("br\n");
			printf("mccjit-selftest-purity: gating demo setup failed "
						 "(baseline=%p variant=%p gpp=%p)\n",
						 (void *)baseline, (void *)variant, (void *)gpp);
			fails++;
		} else { MCC_TRACE("br\n");
			MccjitKgc ka, kb;
			int oa = mccjit_kgc_open(&ka, NULL, mccjit_salt_witness(), 1);
			int ob = mccjit_kgc_open(&kb, NULL, mccjit_salt_witness(), 1);
			if (oa != 0 || ob != 0) { MCC_TRACE("br\n");
				printf("mccjit-selftest-purity: kgc open failed\n");
				fails++;
			} else { MCC_TRACE("br\n");
				int64_t r1, r2, r3, r4;
				int f1 = 0, f2 = 0, f3 = 0, f4 = 0;

				ka.memoize_ok = 1;
				kb.memoize_ok = (tier == AST_PURITY_TIER0);

				printf("mccjit-selftest-purity: --- gating demo: t is %s, correct "
							 "treatment memoize_ok=%d ---\n",
							 mccjit_purity_name(tier), kb.memoize_ok);
				printf("mccjit-selftest-purity: t(x)=*gp+x ; "
							 "variant=speculative t (assumes *gp==10)\n");

				cell = 10;
				r1 = mccjit_kgc_call1(&ka, (void *)variant, (void *)baseline, 5, &f1);
				cell = 100;
				r2 = mccjit_kgc_call1(&ka, (void *)variant, (void *)baseline, 5, &f2);
				printf("mccjit-selftest-purity: [memoize_ok=1] t(5) *gp=10 ->%lld ; "
							 "*gp=100 (repeat)->%lld flagged=%d  (unsound: stale, want 105)\n",
							 (long long)r1, (long long)r2, f2);

				cell = 10;
				r3 = mccjit_kgc_call1(&kb, (void *)variant, (void *)baseline, 5, &f3);
				cell = 100;
				r4 = mccjit_kgc_call1(&kb, (void *)variant, (void *)baseline, 5, &f4);
				printf("mccjit-selftest-purity: [memoize_ok=0] t(5) *gp=10 ->%lld ; "
							 "*gp=100 (repeat)->%lld flagged=%d  (sound: re-differentiated)\n",
							 (long long)r3, (long long)r4, f4);

				if (r1 != 15) { MCC_TRACE("br\n");
					printf("mccjit-selftest-purity: setup r1 expected 15\n");
					fails++;
				}
				if (r2 != 15) { MCC_TRACE("br\n");
					printf("mccjit-selftest-purity: memoize_ok=1 did not fast-path stale\n");
					fails++;
				}
				if (r4 != 105 || !f4) { MCC_TRACE("br\n");
					printf("mccjit-selftest-purity: memoize_ok=0 did not re-differentiate "
								 "(r4=%lld f4=%d, want 105/1)\n",
								 (long long)r4, f4);
					fails++;
				}
				mccjit_kgc_close(&ka);
				mccjit_kgc_close(&kb);
			}
		}

		mcc_free(tblob);
		mcc_free(vblob);
		if (st)
			{ MCC_TRACE("br\n"); mcc_delete(st); }
		if (sv)
			{ MCC_TRACE("br\n"); mcc_delete(sv); }
	}

	mcc_free(mccjit_last_blob);
	mccjit_last_blob = NULL;
	mccjit_last_len = 0;
	mccjit_last_state = NULL;
	printf("mccjit-selftest-purity: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_lazy(void) { MCC_TRACE("enter\n");
	static const char src[] = "int f(int x){return x*2+1;}";
	unsigned char *blob;
	size_t blen;
	MCCState *s1;
	int (*baseline)(int) = NULL;
	MCCState *bstate = NULL;
	void *slot = NULL;
	MccjitCounterState st;
	long threshold = 8;
	int inputs[6] = {5, 0, -3, 100, 7, -40};
	int fails = 0;
	long c;
	int i;

	printf("mccjit-selftest-lazy: begin (threshold=%ld)\n", threshold);

	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-lazy: stash failed\n");
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return 1;
	}
	baseline = (int (*)(int))mcc_jit_recompile_blob(blob, blen);
	bstate = mccjit_last_state;
	mccjit_last_state = NULL;
	if (!baseline) { MCC_TRACE("br\n");
		printf("mccjit-selftest-lazy: baseline recompile returned NULL\n");
		mcc_free(blob);
		mcc_delete(s1);
		return 1;
	}

	slot = (void *)baseline;
	memset(&st, 0, sizeof st);
	st.slot = &slot;
	st.blob = blob;
	st.len = blen;
	st.baseline = (void *)baseline;
	st.threshold = threshold;
	st.count = 0;
	st.promoted = NULL;
	pthread_mutex_init(&st.lock, NULL);

	printf("mccjit-selftest-lazy: cold phase (calls 1..%ld run baseline):\n",
				 threshold - 1);
	for (c = 1; c < threshold; c++) { MCC_TRACE("br\n");
		void *t = mccjit_counter_tick(&st, NULL);
		int x = inputs[(c - 1) % 6];
		int got = ((int (*)(int))t)(x);
		int want = x * 2 + 1;
		int ok = (t == (void *)baseline) && (got == want) && (st.promoted == NULL) &&
						 (slot == (void *)baseline);
		printf("mccjit-selftest-lazy: cold call %ld path=%s f(%d)=%d expect=%d %s\n", c,
					 t == (void *)baseline ? "baseline" : "other", x, got, want,
					 ok ? "OK" : "FAIL");
		if (!ok)
			{ MCC_TRACE("br\n"); fails++; }
	}

	{
		void *t = mccjit_counter_tick(&st, NULL);
		int ok = (st.promoted != NULL) && (t == st.promoted) &&
						 (slot == st.promoted) && (t != (void *)baseline);
		printf("mccjit-selftest-lazy: PROMOTE at call %ld promoted=%p slot=%p %s\n",
					 st.count, st.promoted, slot, ok ? "OK" : "FAIL");
		if (!ok)
			{ MCC_TRACE("br\n"); fails++; }
	}

	for (i = 0; i < 3; i++) { MCC_TRACE("br\n");
		void *t = mccjit_counter_tick(&st, NULL);
		int ok = (t == st.promoted) && (slot == st.promoted);
		printf("mccjit-selftest-lazy: hot call %ld path=promoted stable=%s %s\n",
					 st.count, t == st.promoted ? "yes" : "no", ok ? "OK" : "FAIL");
		if (!ok)
			{ MCC_TRACE("br\n"); fails++; }
	}

	{
		int (*v)(int) = (int (*)(int))mcc_jit_recompile_blob(blob, blen);
		MCCState *vstate = mccjit_last_state;
		mccjit_last_state = NULL;
		if (!v) { MCC_TRACE("br\n");
			printf("mccjit-selftest-lazy: post-promote variant recompile NULL\n");
			fails++;
		} else { MCC_TRACE("br\n");
			for (i = 0; i < 6; i++) { MCC_TRACE("br\n");
				int x = inputs[i];
				int got = v(x);
				int want = x * 2 + 1;
				int ok = (got == want);
				printf("mccjit-selftest-lazy: post-promote variant f(%d)=%d expect=%d %s\n",
							 x, got, want, ok ? "OK" : "FAIL");
				if (!ok)
					{ MCC_TRACE("br\n"); fails++; }
			}
		}
		if (vstate)
			{ MCC_TRACE("br\n"); mcc_delete(vstate); }
	}

	pthread_mutex_destroy(&st.lock);
	if (bstate)
		{ MCC_TRACE("br\n"); mcc_delete(bstate); }
	mccjit_last_state = NULL;
	mcc_free(blob);
	mcc_delete(s1);
	printf("mccjit-selftest-lazy: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static void mccjit_pool_nap(void) { MCC_TRACE("enter\n");
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 1000000;
	nanosleep(&ts, NULL);
}

/* KGC/trampoline dispatch stubs are dispatch-only: they open with `leave` and
 * must be entered via `jmp *slot` (the mode-6 dispatch convention), never a
 * direct call. This builds a minimal dispatcher — push rbp; mov rbp,rsp; jmp
 * *[slot] — so a test can invoke a published stub through its correct entry.
 * Non-x86_64 hosts fall back to the raw pointer (their stub shapes differ). */
static void *mccjit_dispatch_entry(void **slot, void *fallback) { MCC_TRACE("enter\n");
#if defined(__x86_64__)
	unsigned char *p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
													MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	int o = 0;
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return fallback; }
	p[o++] = 0x55;                                    /* push rbp */
	p[o++] = 0x48, p[o++] = 0x89, p[o++] = 0xe5;      /* mov rbp, rsp */
	p[o++] = 0x48, p[o++] = 0xb8;                     /* movabs rax, imm64 */
	memcpy(p + o, &slot, 8), o += 8;
	p[o++] = 0x48, p[o++] = 0x8b, p[o++] = 0x00;      /* mov rax, [rax] */
	p[o++] = 0xff, p[o++] = 0xe0;                     /* jmp rax */
	return p;
#else
	(void)slot;
	return fallback;
#endif
}

int mccjit_selftest_pool(void) { MCC_TRACE("enter\n");
	static const char src[] = "int f(int x){return x*2+1;}";
	static void *slot_a;
	static void *slot_b;
	static MccjitCounterState st;
	unsigned char *blob;
	size_t blen;
	MCCState *s1;
	int (*baseline)(int) = NULL;
	int inputs[6] = {5, 0, -3, 100, 7, -40};
	int fails = 0;
	int i;
	int nw;

	printf("mccjit-selftest-pool: begin\n");

	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-pool: stash failed\n");
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return 1;
	}
	baseline = (int (*)(int))mcc_jit_recompile_blob(blob, blen);
	mccjit_last_state = NULL;
	if (!baseline) { MCC_TRACE("br\n");
		printf("mccjit-selftest-pool: baseline recompile returned NULL\n");
		return 1;
	}

	nw = mccjit_pool_start(2);
	printf("mccjit-selftest-pool: pool workers=%d\n", nw);
	if (nw <= 0) { MCC_TRACE("br\n");
		printf("mccjit-selftest-pool: pool start failed FAIL\n");
		return 1;
	}

	slot_a = (void *)baseline;
	{
		MccjitSwapJob *job = mcc_malloc(sizeof *job);
		void *pub = (void *)baseline;
		int spins = 0;
		if (!job) { MCC_TRACE("br\n");
			printf("mccjit-selftest-pool: eager job alloc failed FAIL\n");
			fails++;
		} else { MCC_TRACE("br\n");
			job->run = mccjit_job_run_eager;
			job->slot = &slot_a;
			job->blob = blob;
			job->len = blen;
			job->max_duration = 0;
			job->timed = 0;
			mccjit_pool_enqueue(job);
			while (spins++ < 5000) { MCC_TRACE("br\n");
				pub = __atomic_load_n(&slot_a, __ATOMIC_ACQUIRE);
				if (pub != (void *)baseline)
					{ MCC_TRACE("br\n"); break; }
				mccjit_pool_nap();
			}
			if (pub == (void *)baseline) { MCC_TRACE("br\n");
				printf("mccjit-selftest-pool: eager async never published (timeout) FAIL\n");
				fails++;
			} else { MCC_TRACE("br\n");
				void *disp_a = mccjit_dispatch_entry(&slot_a, pub);
				for (i = 0; i < 6; i++) { MCC_TRACE("br\n");
					int x = inputs[i];
					int got = ((int (*)(int))disp_a)(x);
					int want = x * 2 + 1;
					int ok = got == want;
					printf("mccjit-selftest-pool: eager pub f(%d)=%d expect=%d %s\n", x, got,
								 want, ok ? "OK" : "FAIL");
					if (!ok)
						{ MCC_TRACE("br\n"); fails++; }
				}
			}
		}
	}

	{
		long threshold = 4;
		long c;
		void *promoted = NULL;
		int spins = 0;
		int building = 0;
		slot_b = (void *)baseline;
		memset(&st, 0, sizeof st);
		st.slot = &slot_b;
		st.blob = blob;
		st.len = blen;
		st.baseline = (void *)baseline;
		st.threshold = threshold;
		pthread_mutex_init(&st.lock, NULL);

		for (c = 1; c < threshold; c++) { MCC_TRACE("br\n");
			void *t = mccjit_counter_tick(&st, NULL);
			int ok = (t == (void *)baseline);
			if (!ok) { MCC_TRACE("br\n");
				printf("mccjit-selftest-pool: cold call %ld not baseline FAIL\n", c);
				fails++;
			}
		}
		{
			void *t = mccjit_counter_tick(&st, NULL);
			pthread_mutex_lock(&st.lock);
			building = st.building;
			promoted = st.promoted;
			pthread_mutex_unlock(&st.lock);
			printf("mccjit-selftest-pool: cross call %ld path=%s building=%d %s\n",
						 st.count, t == (void *)baseline ? "baseline" : "other", building,
						 (t == (void *)baseline) ? "OK" : "FAIL");
			if (t != (void *)baseline)
				{ MCC_TRACE("br\n"); fails++; }
		}
		while (spins++ < 5000) { MCC_TRACE("br\n");
			pthread_mutex_lock(&st.lock);
			promoted = st.promoted;
			pthread_mutex_unlock(&st.lock);
			if (promoted)
				{ MCC_TRACE("br\n"); break; }
			mccjit_pool_nap();
		}
		if (!promoted) { MCC_TRACE("br\n");
			printf("mccjit-selftest-pool: async promote never landed (timeout) FAIL\n");
			fails++;
		} else { MCC_TRACE("br\n");
			printf("mccjit-selftest-pool: PROMOTE-ASYNC promoted=%p slot=%p %s\n",
						 promoted, slot_b, (slot_b == promoted) ? "OK" : "FAIL");
			if (slot_b != promoted)
				{ MCC_TRACE("br\n"); fails++; }
			void *disp_b = mccjit_dispatch_entry(&slot_b, promoted);
			for (i = 0; i < 6; i++) { MCC_TRACE("br\n");
				void *t = mccjit_counter_tick(&st, NULL);
				int x = inputs[i];
				int got = ((int (*)(int))disp_b)(x);
				int want = x * 2 + 1;
				int ok = (t == promoted) && (got == want);
				printf("mccjit-selftest-pool: promoted f(%d)=%d expect=%d %s\n", x, got,
							 want, ok ? "OK" : "FAIL");
				if (!ok)
					{ MCC_TRACE("br\n"); fails++; }
			}
		}
	}

	printf("mccjit-selftest-pool: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_eligibility(void) { MCC_TRACE("enter\n");
	static const struct {
		const char *src;
		const char *fn;
		int eligible;
		const char *why;
	} cases[] = {
			{"int f(int x){return x*2+1;}", "f", 1, "GP-int arg + int return"},
			{"long f(long a, long b){return a+b;}", "f", 1, "two GP args"},
			{"int f(int *p){return *p;}", "f", 1, "pointer arg"},
			{"int f(int a,int b,int c,int d,int e,int g){return a+b+c+d+e+g;}", "f", 1,
			 "six GP args (ABI max)"},
			{"double f(int x){return (double)x;}", "f", 1, "FP return (mixed)"},
			{"int f(double x){return (int)x;}", "f", 1, "FP arg (mixed)"},
			{"struct S{int a;int b;}; struct S f(int x){struct S s; s.a=x; s.b=-x; return s;}",
			 "f", 0, "struct-by-value return"},
			{"struct S{int a;int b;}; int f(struct S s){return s.a+s.b;}", "f", 0,
			 "struct-by-value arg"},
			{"void f(int x){(void)x;}", "f", 0, "void return (not verifiable)"},
			{"int f(void){return 42;}", "f", 0, "zero args"},
			{"int f(int a,int b,int c,int d,int e,int g,int h){return a+h;}", "f", 0,
			 "seven args (over ABI max)"},
			{"int f(int x, ...){return x;}", "f", 0, "variadic"},
	};
	int n = (int)(sizeof cases / sizeof cases[0]);
	int fails = 0;
	int i;

	printf("mccjit-selftest-eligibility: begin (%d cases)\n", n);

	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		size_t blen = 0;
		MCCState *st = NULL;
		unsigned char *blob =
				mccjit_stash_one(cases[i].src, cases[i].fn, 1, &blen, &st);
		int selected = (blob != NULL);
		int compiled = (st != NULL);
		int ok;
		if (!compiled) { MCC_TRACE("br\n");
			printf("mccjit-selftest-eligibility: [%s] compile FAILED (setup)\n",
						 cases[i].why);
			fails++;
		} else { MCC_TRACE("br\n");
			ok = (selected == cases[i].eligible);
			printf(
					"mccjit-selftest-eligibility: %-28s want=%s got=%s %s\n", cases[i].why,
					cases[i].eligible ? "jit" : "refuse", selected ? "jit" : "refuse",
					ok ? "OK" : "FAIL");
			if (!ok)
				{ MCC_TRACE("br\n"); fails++; }
		}
		mcc_free(blob);
		if (st)
			{ MCC_TRACE("br\n"); mcc_delete(st); }
	}

	mcc_free(mccjit_last_blob);
	mccjit_last_blob = NULL;
	mccjit_last_len = 0;
	mccjit_last_state = NULL;

	printf("mccjit-selftest-eligibility: %s (%d failure%s)\n",
				 fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_fork(void) { MCC_TRACE("enter\n");
	static const char src[] = "int f(int x){return x*2+1;}";
	unsigned char *blob;
	size_t blen;
	MCCState *s1;
	void *baseline;
	int nw;
	int fails = 0;
	pid_t pid;

	printf("mccjit-selftest-fork: begin\n");

	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-fork: stash failed\n");
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return 1;
	}
	baseline = mcc_jit_recompile_blob(blob, blen);
	if (!baseline) { MCC_TRACE("br\n");
		printf("mccjit-selftest-fork: baseline recompile NULL\n");
		mcc_free(blob);
		mcc_delete(s1);
		return 1;
	}
	mccjit_last_state = NULL;

	nw = mccjit_pool_start(2);
	printf("mccjit-selftest-fork: parent pool workers=%d started=%d\n", nw,
				 mccjit_pool.started);
	if (nw <= 0 || !mccjit_pool.started) { MCC_TRACE("br\n");
		printf("mccjit-selftest-fork: pool failed to start FAIL\n");
		return 1;
	}

	fflush(stdout);
	pid = fork();
	if (pid == 0) { MCC_TRACE("br\n");
		int cf = 0;
		if (mccjit_pool.started != 0 || mccjit_pool.nworkers != 0) { MCC_TRACE("br\n");
			fprintf(stderr,
							"mccjit-selftest-fork[child]: phantom pool NOT reset "
							"(started=%d nworkers=%d) FAIL\n",
							mccjit_pool.started, mccjit_pool.nworkers);
			cf++;
		}
		if (((int (*)(int))baseline)(9) != 19) { MCC_TRACE("br\n");
			fprintf(stderr,
							"mccjit-selftest-fork[child]: installed variant f(9)!=19 FAIL\n");
			cf++;
		}
		if (mccjit_pool_start(2) <= 0) { MCC_TRACE("br\n");
			fprintf(stderr,
							"mccjit-selftest-fork[child]: pool locks unusable post-fork "
							"(deadlock/start failure) FAIL\n");
			cf++;
		}
		fflush(stderr);
		_exit(cf ? 1 : 0);
	}

	if (pid < 0) { MCC_TRACE("br\n");
		printf("mccjit-selftest-fork: fork() failed FAIL\n");
		fails++;
	} else { MCC_TRACE("br\n");
		int status = 0;
		while (waitpid(pid, &status, 0) < 0)
			;
		{
			int child_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
			printf(
					"mccjit-selftest-fork: child pool reset + variant runs after fork: %s\n",
					child_ok ? "OK" : "FAIL");
			if (!child_ok)
				{ MCC_TRACE("br\n"); fails++; }
		}
		if (mccjit_pool.started != 1 || mccjit_pool.nworkers != nw ||
				((int (*)(int))baseline)(9) != 19) { MCC_TRACE("br\n");
			printf("mccjit-selftest-fork: parent pool broken after fork "
						 "(started=%d nworkers=%d) FAIL\n",
						 mccjit_pool.started, mccjit_pool.nworkers);
			fails++;
		} else { MCC_TRACE("br\n");
			printf("mccjit-selftest-fork: parent pool intact after fork OK\n");
		}
	}

	mcc_free(blob);
	mcc_delete(s1);
	printf("mccjit-selftest-fork: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_observability(void) { MCC_TRACE("enter\n");
	static const char src[] = "int f(int x){return x*2+1;}";
	int fails = 0;
	char path[64];
	unsigned char *blob;
	size_t blen;
	MCCState *s1;

	printf("mccjit-selftest-observability: begin\n");

	if (!mccjit_feasible()) { MCC_TRACE("br\n");
		printf("mccjit-selftest-observability: exec-mem probe infeasible on this "
					 "host FAIL\n");
		fails++;
	} else { MCC_TRACE("br\n");
		printf("mccjit-selftest-observability: exec-mem probe feasible OK\n");
	}

	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-observability: stash failed\n");
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return fails + 1;
	}

	{
		void *baseline = mcc_jit_recompile_blob(blob, blen);
		MCCState *bstate = mccjit_last_state;
		void *slot = baseline;
		mccjit_last_state = NULL;
		if (!baseline) { MCC_TRACE("br\n");
			printf("mccjit-selftest-observability: baseline recompile NULL FAIL\n");
			fails++;
		} else { MCC_TRACE("br\n");
			setenv("MCC_JIT_FORCE_INFEASIBLE", "1", 1);
			mccjit_boot_swap(&slot, blob, blen);
			unsetenv("MCC_JIT_FORCE_INFEASIBLE");
			if (slot != baseline) { MCC_TRACE("br\n");
				printf("mccjit-selftest-observability: infeasible boot swapped anyway "
							 "(no silent fallback) FAIL\n");
				fails++;
			} else { MCC_TRACE("br\n");
				printf("mccjit-selftest-observability: infeasible -> kept AOT baseline "
							 "OK\n");
			}
		}
		if (bstate)
			{ MCC_TRACE("br\n"); mcc_delete(bstate); }
		mccjit_last_state = NULL;
	}

	{
		void *v;
		MCCState *vstate;
		FILE *f;
		int found = 0;
		snprintf(path, sizeof path, "/tmp/perf-%d.map", (int)getpid());
		remove(path);
		setenv("MCC_JIT_PERF_MAP", "1", 1);
		v = mcc_jit_recompile_blob(blob, blen);
		vstate = mccjit_last_state;
		mccjit_last_state = NULL;
		unsetenv("MCC_JIT_PERF_MAP");
		if (!v) { MCC_TRACE("br\n");
			printf("mccjit-selftest-observability: perf-map recompile NULL FAIL\n");
			fails++;
		}
		f = fopen(path, "r");
		if (f) { MCC_TRACE("br\n");
			char line[256];
			while (fgets(line, sizeof line, f)) { MCC_TRACE("br\n");
				unsigned long a = 0, sz = 0;
				char nm[128] = {0};
				if (sscanf(line, "%lx %lx %127s", &a, &sz, nm) == 3 &&
						!strcmp(nm, "f") && a == (unsigned long)(uintptr_t)v && sz > 0)
					{ MCC_TRACE("br\n"); found = 1; }
			}
			fclose(f);
		}
		printf("mccjit-selftest-observability: perf-map %s (%s addr=%p) %s\n",
					 found ? "line for 'f' present" : "line MISSING", path, v,
					 found ? "OK" : "FAIL");
		if (!found)
			{ MCC_TRACE("br\n"); fails++; }
		remove(path);
		if (vstate)
			{ MCC_TRACE("br\n"); mcc_delete(vstate); }
		mccjit_last_state = NULL;
	}

	mcc_free(blob);
	mcc_delete(s1);
	printf("mccjit-selftest-observability: %s (%d failure%s)\n",
				 fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_liverun(const char *libpath, const char *incpath) { MCC_TRACE("enter\n");
	static const char src[] =
			"int f(int x){return x*3+7;}\nint main(void){return f(11);}\n";
	MCCState *s;
	char *av[] = {"liverun", NULL};
	char path[64];
	int fails = 0;
	int rc;
	int perf_found = 0;

	printf("mccjit-selftest-liverun: begin\n");

	setenv("MCC_AST_JIT_DISPATCH", "6", 1);
	setenv("MCC_JIT_PERF_MAP", "1", 1);
	snprintf(path, sizeof path, "/tmp/perf-%d.map", (int)getpid());
	remove(path);

	s = mcc_new();
	if (!s) { MCC_TRACE("br\n");
		printf("mccjit-selftest-liverun: mcc_new failed\n");
		unsetenv("MCC_AST_JIT_DISPATCH");
		unsetenv("MCC_JIT_PERF_MAP");
		return 1;
	}
	if (libpath)
		{ MCC_TRACE("br\n"); mcc_set_lib_path(s, libpath); }
	if (incpath)
		{ MCC_TRACE("br\n"); mcc_add_include_path(s, incpath); }
	s->optimize = 1;
	s->embed_jit = 1;
	s->jit_threads = 0;
	mcc_free(s->jit_functions);
	s->jit_functions = mcc_strdup("f");
	mcc_set_output_type(s, MCC_OUTPUT_MEMORY);

	if (mcc_compile_string(s, src) != 0) { MCC_TRACE("br\n");
		printf("mccjit-selftest-liverun: compile failed FAIL\n");
		fails++;
		rc = -1;
	} else { MCC_TRACE("br\n");
		rc = mcc_run(s, 1, av);
	}
	unsetenv("MCC_AST_JIT_DISPATCH");
	unsetenv("MCC_JIT_PERF_MAP");

	printf("mccjit-selftest-liverun: main() returned %d (expect 40) %s\n", rc,
				 rc == 40 ? "OK" : "FAIL");
	if (rc != 40)
		{ MCC_TRACE("br\n"); fails++; }

	{
		FILE *pf = fopen(path, "r");
		if (pf) { MCC_TRACE("br\n");
			char line[256];
			while (fgets(line, sizeof line, pf)) { MCC_TRACE("br\n");
				char nm[128] = {0};
				unsigned long a = 0, sz = 0;
				if (sscanf(line, "%lx %lx %127s", &a, &sz, nm) == 3 &&
						!strcmp(nm, "f"))
					{ MCC_TRACE("br\n"); perf_found = 1; }
			}
			fclose(pf);
		}
	}
	printf("mccjit-selftest-liverun: live recompile %s during .init_array ctor %s\n",
				 perf_found ? "fired" : "did NOT fire", perf_found ? "OK" : "FAIL");
	if (!perf_found)
		{ MCC_TRACE("br\n"); fails++; }
	remove(path);

	mcc_delete(s);
	printf("mccjit-selftest-liverun: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_poison(void) { MCC_TRACE("enter\n");
	static const char src[] = "int f(int x){return x*2+1;}";
	unsigned char *blob;
	size_t blen;
	MCCState *s1;
	int (*baseline)(int) = NULL;
	void *variant = NULL;
	MCCState *vstate = NULL;
	MccjitKgc kgc;
	int fails = 0;
	int i;
	int flagged_calls = 0;
	int min;
	int poison_at = -1;

	printf("mccjit-selftest-poison: begin\n");

	setenv("MCC_JIT_POISON_MIN", "4", 1);
	setenv("MCC_JIT_POISON_PCT", "50", 1);
	min = mccjit_poison_min();

	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-poison: stash failed\n");
		unsetenv("MCC_JIT_POISON_MIN");
		unsetenv("MCC_JIT_POISON_PCT");
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return 1;
	}
	if (mcc_relocate(s1) == 0)
		{ MCC_TRACE("br\n"); baseline = (int (*)(int))mcc_get_symbol(s1, "f"); }
	variant = mcc_jit_recompile_blob_spec(blob, blen, 0, 7);
	vstate = mccjit_last_state;
	mccjit_last_state = NULL;
	if (!baseline || !variant) { MCC_TRACE("br\n");
		printf("mccjit-selftest-poison: baseline/variant build failed FAIL\n");
		fails++;
	} else if (mccjit_kgc_open(&kgc, NULL, mccjit_salt_witness(), 1) != 0) { MCC_TRACE("br\n");
		printf("mccjit-selftest-poison: kgc open failed FAIL\n");
		fails++;
	} else { MCC_TRACE("br\n");
		for (i = 0; i < 10; i++) { MCC_TRACE("br\n");
			int64_t args[1];
			int flagged = 0;
			int64_t r;
			args[0] = 3;
			r = mccjit_kgc_calln(&kgc, variant, baseline, args, 1, &flagged);
			if (kgc.poisoned && poison_at < 0)
				{ MCC_TRACE("br\n"); poison_at = i; }
			if (flagged)
				{ MCC_TRACE("br\n"); flagged_calls++; }
			if (r != 7) { MCC_TRACE("br\n");
				printf("mccjit-selftest-poison: call %d returned %lld (expect baseline "
							 "7) FAIL\n",
							 i, (long long)r);
				fails++;
			}
		}
		printf(
				"mccjit-selftest-poison: mismatches-flagged=%d (expect %d) poisoned=%d "
				"first-poison-call=%d %s\n",
				flagged_calls, min, kgc.poisoned, poison_at,
				(flagged_calls == min && kgc.poisoned) ? "OK" : "FAIL");
		if (flagged_calls != min || !kgc.poisoned)
			{ MCC_TRACE("br\n"); fails++; }
		if (kgc.hits != 0 || kgc.misses != (uint64_t)min) { MCC_TRACE("br\n");
			printf("mccjit-selftest-poison: counters hits=%llu misses=%llu "
						 "(expect 0/%d) FAIL\n",
						 (unsigned long long)kgc.hits, (unsigned long long)kgc.misses, min);
			fails++;
		}
		mccjit_kgc_close(&kgc);
	}

	unsetenv("MCC_JIT_POISON_MIN");
	unsetenv("MCC_JIT_POISON_PCT");
	if (vstate)
		{ MCC_TRACE("br\n"); mcc_delete(vstate); }
	mccjit_last_state = NULL;
	mcc_free(blob);
	mcc_delete(s1);
	printf("mccjit-selftest-poison: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static long mccjit_bench_fast_fn(long x) { MCC_TRACE("enter\n");
	long s = 0;
	int i;
	for (i = 0; i < 30; i++)
		{ MCC_TRACE("br\n"); s += (x ^ (long)i) * 2654435761L; }
	return s;
}

static long mccjit_bench_slow_fn(long x) { MCC_TRACE("enter\n");
	long s = 0;
	int i;
	for (i = 0; i < 900; i++)
		{ MCC_TRACE("br\n"); s += (x ^ (long)i) * 2654435761L; }
	return s;
}

int mccjit_selftest_bench(void) { MCC_TRACE("enter\n");
	int64_t tuples[4 * MCCJIT_KGC_ARITY];
	uint32_t nt = 4, i, j;
	int fails = 0;
	int r_win, r_lose, r_tie;

	printf("mccjit-selftest-bench: begin\n");
	setenv("MCC_JIT_BENCH_ITERS", "3000", 1);
	setenv("MCC_JIT_BENCH_MARGIN_PCT", "10", 1);
	for (i = 0; i < nt; i++)
		{ MCC_TRACE("br\n"); for (j = 0; j < MCCJIT_KGC_ARITY; j++)
			{ MCC_TRACE("br\n"); tuples[i * MCCJIT_KGC_ARITY + j] = (int64_t)(i * 7 + 1); } }

	r_win = mccjit_bench_pair((void *)mccjit_bench_fast_fn,
													 (void *)mccjit_bench_slow_fn, tuples, nt, 1, 1);
	r_lose = mccjit_bench_pair((void *)mccjit_bench_slow_fn,
														(void *)mccjit_bench_fast_fn, tuples, nt, 1, 1);
	r_tie = mccjit_bench_pair((void *)mccjit_bench_slow_fn,
													 (void *)mccjit_bench_slow_fn, tuples, nt, 1, 1);
	unsetenv("MCC_JIT_BENCH_ITERS");
	unsetenv("MCC_JIT_BENCH_MARGIN_PCT");

	printf("mccjit-selftest-bench: faster candidate promoted=%d (expect 1) %s\n",
				 r_win, r_win == 1 ? "OK" : "FAIL");
	if (r_win != 1)
		{ MCC_TRACE("br\n"); fails++; }
	printf("mccjit-selftest-bench: slower candidate promoted=%d (expect 0) %s\n",
				 r_lose, r_lose == 0 ? "OK" : "FAIL");
	if (r_lose != 0)
		{ MCC_TRACE("br\n"); fails++; }
	printf(
			"mccjit-selftest-bench: equal candidate promoted=%d (expect 0, incumbent-wins-tie) %s\n",
			r_tie, r_tie == 0 ? "OK" : "FAIL");
	if (r_tie != 0)
		{ MCC_TRACE("br\n"); fails++; }

	printf("mccjit-selftest-bench: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

/* J10 — hot-patch is a strategy family: the how-to-patch mechanism is itself a
   search dial. These are two direct-callable (tail-jump) dispatch shapes for
   the benchmark harness:
     - slot dispatch  (pointer-swap): jmp *[rip+slot]; swap = 1 store to the slot
     - trampoline     (in-place patch): movabs rax,imm; jmp rax; swap = rewrite imm
   both forward to *slot / imm and tail-return to the caller (no frame). */
#if defined(__x86_64__)
static unsigned char *mccjit_patch_make_slot(void *target, void ***slotout) { MCC_TRACE("enter\n");
	unsigned char *p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
													MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	int32_t disp;
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return NULL; }
	p[0] = 0xff;
	p[1] = 0x25;
	disp = 2; /* slot at p+8, next insn at p+6 -> disp = 2 */
	memcpy(p + 2, &disp, 4);
	memcpy(p + 8, &target, 8);
	if (slotout)
		{ MCC_TRACE("br\n"); *slotout = (void **)(p + 8); }
	return p;
}

static unsigned char *mccjit_patch_make_tramp(void *target,
																							void **immout) { MCC_TRACE("enter\n");
	unsigned char *p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
													MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return NULL; }
	p[0] = 0x48;
	p[1] = 0xb8;
	memcpy(p + 2, &target, 8);
	p[10] = 0xff;
	p[11] = 0xe0;
	if (immout)
		{ MCC_TRACE("br\n"); *immout = (void *)(p + 2); }
	return p;
}
#elif defined(__aarch64__)
static unsigned char *mccjit_patch_make_slot(void *target, void ***slotout) { MCC_TRACE("enter\n");
	size_t page = host_pagesize();
	void **slot = mcc_malloc(sizeof(void *));
	unsigned char *p;
	uint32_t insns[6];
	uint64_t a;
	if (!slot)
		{ MCC_TRACE("br\n"); return NULL; }
	*slot = target;
	p = mmap(0, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) { MCC_TRACE("br\n");
		mcc_free(slot);
		return NULL;
	}
	a = (uint64_t)(uintptr_t)slot;
	insns[0] = 0xd2800011u | (uint32_t)((a & 0xffff) << 5);         /* movz x17,#a0 */
	insns[1] = 0xf2a00011u | (uint32_t)(((a >> 16) & 0xffff) << 5); /* movk x17,#a1,lsl16 */
	insns[2] = 0xf2c00011u | (uint32_t)(((a >> 32) & 0xffff) << 5); /* movk x17,#a2,lsl32 */
	insns[3] = 0xf2e00011u | (uint32_t)(((a >> 48) & 0xffff) << 5); /* movk x17,#a3,lsl48 */
	insns[4] = 0xf9400230u; /* ldr x16,[x17] */
	insns[5] = 0xd61f0200u; /* br  x16 */
	memcpy(p, insns, sizeof insns);
	if (host_runmem_protect(p, page, HOST_PROT_RX) != 0) { MCC_TRACE("br\n");
		munmap(p, page);
		mcc_free(slot);
		return NULL;
	}
	if (slotout)
		{ MCC_TRACE("br\n"); *slotout = slot; }
	return p;
}
#endif

static int mccjit_patch_t1(int x) { MCC_TRACE("enter\n"); return x * 2 + 1; }
static int mccjit_patch_t2(int x) { MCC_TRACE("enter\n"); return x * 3 + 7; }

typedef struct MccjitPatchStrategy {
	const char *name;
	unsigned footprint;
	int (*available)(void);
	void *(*make)(void *target, void **handle);
	void (*swap)(void *handle, void *target);
	int (*call_i)(void *entry, int arg);
	void (*dispose)(void *entry);
} MccjitPatchStrategy;

static int mccjit_patch_avail_yes(void) { MCC_TRACE("enter\n"); return 1; }
static int mccjit_patch_avail_no(void) { MCC_TRACE("enter\n"); return 0; }

static void mccjit_patch_swap_store(void *handle, void *target) { MCC_TRACE("enter\n");
	__atomic_store_n((void **)handle, target, __ATOMIC_RELEASE);
}

static void *mccjit_patch_mk_cell(void *target, void **handle) { MCC_TRACE("enter\n");
	void **cell = mcc_malloc(sizeof(void *));
	if (!cell)
		{ MCC_TRACE("br\n"); return NULL; }
	*cell = target;
	if (handle)
		{ MCC_TRACE("br\n"); *handle = cell; }
	return cell;
}

static int mccjit_patch_call_cell(void *entry, int arg) { MCC_TRACE("enter\n");
	return (*(int (**)(int))entry)(arg);
}

static void mccjit_patch_free_cell(void *entry) { MCC_TRACE("enter\n");
	mcc_free(entry);
}

#if defined(__x86_64__) || defined(__aarch64__)
static int mccjit_patch_call_code(void *entry, int arg) { MCC_TRACE("enter\n");
	return ((int (*)(int))entry)(arg);
}

static void *mccjit_patch_mk_slot(void *target, void **handle) { MCC_TRACE("enter\n");
	void **slot = NULL;
	void *entry = mccjit_patch_make_slot(target, &slot);
	if (!entry)
		{ MCC_TRACE("br\n"); return NULL; }
	if (handle)
		{ MCC_TRACE("br\n"); *handle = slot; }
	return entry;
}
#endif

#if defined(__x86_64__)
static void mccjit_patch_swap_imm(void *handle, void *target) { MCC_TRACE("enter\n");
	memcpy(handle, &target, 8);
}

static void *mccjit_patch_mk_tramp(void *target, void **handle) { MCC_TRACE("enter\n");
	void *imm = NULL;
	void *entry = mccjit_patch_make_tramp(target, &imm);
	if (!entry)
		{ MCC_TRACE("br\n"); return NULL; }
	if (handle)
		{ MCC_TRACE("br\n"); *handle = imm; }
	return entry;
}

static void mccjit_patch_free_page(void *entry) { MCC_TRACE("enter\n");
	munmap(entry, 4096);
}
#endif

#if defined(__aarch64__)
static void mccjit_patch_free_runmem(void *entry) { MCC_TRACE("enter\n");
	munmap(entry, host_pagesize());
}
#endif

static const MccjitPatchStrategy mccjit_patch_reg[] = {
		{"c-indirect", (unsigned)sizeof(void *), mccjit_patch_avail_yes,
		 mccjit_patch_mk_cell, mccjit_patch_swap_store, mccjit_patch_call_cell,
		 mccjit_patch_free_cell},
#if defined(__x86_64__)
		{"ptr-swap-slot", 16, mccjit_patch_avail_yes, mccjit_patch_mk_slot,
		 mccjit_patch_swap_store, mccjit_patch_call_code, mccjit_patch_free_page},
		{"inplace-tramp", 12, mccjit_patch_avail_yes, mccjit_patch_mk_tramp,
		 mccjit_patch_swap_imm, mccjit_patch_call_code, mccjit_patch_free_page},
#elif defined(__aarch64__)
		{"ptr-swap-slot", 16, mccjit_patch_avail_yes, mccjit_patch_mk_slot,
		 mccjit_patch_swap_store, mccjit_patch_call_code, mccjit_patch_free_runmem},
#endif
		{"nop-pad-d3b", 8, mccjit_patch_avail_no, NULL, NULL, NULL, NULL},
};

#define MCCJIT_PATCH_NREG (int)(sizeof mccjit_patch_reg / sizeof mccjit_patch_reg[0])

static long mccjit_patch_iters(void) { MCC_TRACE("enter\n");
	const char *e = getenv("MCC_JIT_PATCH_ITERS");
	if (e && e[0]) { MCC_TRACE("br\n");
		long v = strtol(e, NULL, 10);
		if (v > 0)
			{ MCC_TRACE("br\n"); return v; }
	}
	return 500000;
}

static int mccjit_patch_benchmarkable(const MccjitPatchStrategy *s) { MCC_TRACE("enter\n");
	return s->available() && s->make && s->call_i;
}

static int mccjit_patch_bench_rank(void *target, int *order, double *nspc,
																	 int cap) { MCC_TRACE("enter\n");
	long iters = mccjit_patch_iters();
	int cnt = 0, i, j;
	for (i = 0; i < MCCJIT_PATCH_NREG && cnt < cap; i++) { MCC_TRACE("br\n");
		const MccjitPatchStrategy *s = &mccjit_patch_reg[i];
		void *handle = NULL, *entry;
		double best = 1e300;
		int64_t acc = 0;
		int rep;
		if (!mccjit_patch_benchmarkable(s))
			{ MCC_TRACE("br\n"); continue; }
		entry = s->make(target, &handle);
		if (!entry)
			{ MCC_TRACE("br\n"); continue; }
		for (rep = 0; rep < 3; rep++) { MCC_TRACE("br\n");
			struct timespec t0;
			double d;
			long k;
			clock_gettime(CLOCK_MONOTONIC, &t0);
			for (k = 0; k < iters; k++)
				{ MCC_TRACE("br\n"); acc += s->call_i(entry, (int)k); }
			d = mccjit_elapsed(&t0);
			if (d < best)
				{ MCC_TRACE("br\n"); best = d; }
		}
		mccjit_bench_sink ^= acc;
		if (s->dispose)
			{ MCC_TRACE("br\n"); s->dispose(entry); }
		order[cnt] = i;
		nspc[cnt] = best / iters * 1e9;
		cnt++;
	}
	for (i = 1; i < cnt; i++) { MCC_TRACE("br\n");
		int oi = order[i];
		double on = nspc[i];
		j = i - 1;
		while (j >= 0 && nspc[j] > on) { MCC_TRACE("br\n");
			order[j + 1] = order[j];
			nspc[j + 1] = nspc[j];
			j--;
		}
		order[j + 1] = oi;
		nspc[j + 1] = on;
	}
	return cnt;
}

int mccjit_selftest_patch(void) { MCC_TRACE("enter\n");
	int fails = 0, i, avail = 0, nrank;
	int order[MCCJIT_PATCH_NREG];
	double nspc[MCCJIT_PATCH_NREG];

	printf("mccjit-selftest-patch: begin (hot-patch strategy registry, %d rows)\n",
				 MCCJIT_PATCH_NREG);

	for (i = 0; i < MCCJIT_PATCH_NREG; i++) { MCC_TRACE("br\n");
		const MccjitPatchStrategy *s = &mccjit_patch_reg[i];
		if (!s->name || !s->available) { MCC_TRACE("br\n");
			printf("mccjit-selftest-patch: row %d malformed FAIL\n", i);
			fails++;
		}
	}

	for (i = 0; i < MCCJIT_PATCH_NREG; i++) { MCC_TRACE("br\n");
		const MccjitPatchStrategy *s = &mccjit_patch_reg[i];
		void *handle = NULL, *entry;
		int r1, r2;
		if (!s->available()) { MCC_TRACE("br\n");
			printf("mccjit-selftest-patch: %-14s deferred/unavailable SKIP\n", s->name);
			continue;
		}
		avail++;
		if (!s->make || !s->swap || !s->call_i) { MCC_TRACE("br\n");
			printf("mccjit-selftest-patch: %-14s incomplete FAIL\n", s->name);
			fails++;
			continue;
		}
		entry = s->make((void *)mccjit_patch_t1, &handle);
		if (!entry) { MCC_TRACE("br\n");
			printf("mccjit-selftest-patch: %-14s make failed FAIL\n", s->name);
			fails++;
			continue;
		}
		r1 = s->call_i(entry, 5);
		s->swap(handle, (void *)mccjit_patch_t2);
		r2 = s->call_i(entry, 5);
		if (r1 != mccjit_patch_t1(5) || r2 != mccjit_patch_t2(5)) { MCC_TRACE("br\n");
			printf("mccjit-selftest-patch: %-14s dispatch=%d redirect=%d FAIL\n",
						 s->name, r1, r2);
			fails++;
		} else { MCC_TRACE("br\n");
			printf("mccjit-selftest-patch: %-14s dispatch=%d redirect=%d OK\n", s->name,
						 r1, r2);
		}
		if (s->dispose)
			{ MCC_TRACE("br\n"); s->dispose(entry); }
	}

	nrank = mccjit_patch_bench_rank((void *)mccjit_patch_t1, order, nspc,
																	MCCJIT_PATCH_NREG);
	if (nrank < 1) { MCC_TRACE("br\n");
		printf("mccjit-selftest-patch: no benchmarkable strategy SKIP\n");
	} else { MCC_TRACE("br\n");
		int nb = 0;
		for (i = 0; i < MCCJIT_PATCH_NREG; i++)
			{ MCC_TRACE("br\n"); if (mccjit_patch_benchmarkable(&mccjit_patch_reg[i]))
				{ MCC_TRACE("br\n"); nb++; } }
		printf("mccjit-selftest-patch: benchmark ranking (best-first):\n");
		for (i = 0; i < nrank; i++)
			{ MCC_TRACE("br\n"); printf("mccjit-selftest-patch:   #%d %-14s %.2f ns/call "
																	"footprint=%uB\n",
																	i + 1, mccjit_patch_reg[order[i]].name, nspc[i],
																	mccjit_patch_reg[order[i]].footprint); }
		if (nrank != nb) { MCC_TRACE("br\n");
			printf("mccjit-selftest-patch: rank count %d != benchmarkable %d FAIL\n",
						 nrank, nb);
			fails++;
		}
		for (i = 1; i < nrank; i++)
			{ MCC_TRACE("br\n"); if (nspc[i] < nspc[i - 1]) { MCC_TRACE("br\n");
				printf("mccjit-selftest-patch: ranking not sorted FAIL\n");
				fails++;
				break;
			} }
		for (i = 0; i < nrank; i++)
			{ MCC_TRACE("br\n"); if (nspc[i] <= 0.0) { MCC_TRACE("br\n");
				printf("mccjit-selftest-patch: nonpositive ns/call FAIL\n");
				fails++;
				break;
			} }
	}

	printf("mccjit-selftest-patch: %d strategy row(s) available, %d row(s) total\n",
				 avail, MCCJIT_PATCH_NREG);
	printf("mccjit-selftest-patch: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static void *mccjit_qsbr_thread(void *arg) { MCC_TRACE("enter\n");
	int rounds = *(int *)arg;
	int slot = mccjit_qsbr_register();
	int i;
	if (slot < 0)
		{ MCC_TRACE("br\n"); return NULL; }
	for (i = 0; i < rounds; i++) { MCC_TRACE("br\n");
		mccjit_qsbr_quiescent(slot);
		mccjit_pool_nap();
	}
	mccjit_qsbr_unregister(slot);
	return NULL;
}

int mccjit_selftest_qsbr(void) { MCC_TRACE("enter\n");
	int fails = 0;
	int s0, s1;
	void *p1, *p2;

	printf("mccjit-selftest-qsbr: begin\n");
	mccjit_qsbr_reset();

	s0 = mccjit_qsbr_register();
	s1 = mccjit_qsbr_register();
	if (s0 < 0 || s1 < 0) { MCC_TRACE("br\n");
		printf("mccjit-selftest-qsbr: register failed FAIL\n");
		return 1;
	}

	p1 = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p1 == MAP_FAILED) { MCC_TRACE("br\n");
		printf("mccjit-selftest-qsbr: mmap failed\n");
		return 1;
	}
	mccjit_qsbr_retire(p1, 4096);
	printf("mccjit-selftest-qsbr: after retire nlimbo=%d reclaimed=%llu "
				 "(expect 1,0) %s\n",
				 mccjit_qsbr.nlimbo, (unsigned long long)mccjit_qsbr.reclaimed,
				 (mccjit_qsbr.nlimbo == 1 && mccjit_qsbr.reclaimed == 0) ? "OK" : "FAIL");
	if (mccjit_qsbr.nlimbo != 1 || mccjit_qsbr.reclaimed != 0)
		{ MCC_TRACE("br\n"); fails++; }

	mccjit_qsbr_quiescent(s0);
	mccjit_qsbr_reclaim();
	printf("mccjit-selftest-qsbr: one thread quiesced nlimbo=%d (expect 1, "
				 "not-yet-reclaimed) %s\n",
				 mccjit_qsbr.nlimbo, mccjit_qsbr.nlimbo == 1 ? "OK" : "FAIL");
	if (mccjit_qsbr.nlimbo != 1)
		{ MCC_TRACE("br\n"); fails++; }

	mccjit_qsbr_quiescent(s1);
	mccjit_qsbr_reclaim();
	printf("mccjit-selftest-qsbr: all threads quiesced nlimbo=%d reclaimed=%llu "
				 "(expect 0,1) %s\n",
				 mccjit_qsbr.nlimbo, (unsigned long long)mccjit_qsbr.reclaimed,
				 (mccjit_qsbr.nlimbo == 0 && mccjit_qsbr.reclaimed == 1) ? "OK" : "FAIL");
	if (mccjit_qsbr.nlimbo != 0 || mccjit_qsbr.reclaimed != 1)
		{ MCC_TRACE("br\n"); fails++; }

	mccjit_qsbr_unregister(s0);
	mccjit_qsbr_unregister(s1);

	mccjit_qsbr_reset();
	p2 = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p2 != MAP_FAILED) { MCC_TRACE("br\n");
		mccjit_qsbr_retire(p2, 4096);
		printf("mccjit-selftest-qsbr: no registered threads -> immediate reclaim "
					 "nlimbo=%d reclaimed=%llu (expect 0,1) %s\n",
					 mccjit_qsbr.nlimbo, (unsigned long long)mccjit_qsbr.reclaimed,
					 (mccjit_qsbr.nlimbo == 0 && mccjit_qsbr.reclaimed == 1) ? "OK" : "FAIL");
		if (mccjit_qsbr.nlimbo != 0 || mccjit_qsbr.reclaimed != 1)
			{ MCC_TRACE("br\n"); fails++; }
	}

	{
		mccjit_qsbr_reset();
		pthread_t th[3];
		int rounds = 200;
		int i, n = 0;
		for (i = 0; i < 3; i++)
			{ MCC_TRACE("br\n"); if (pthread_create(&th[i], NULL, mccjit_qsbr_thread, &rounds) == 0)
				{ MCC_TRACE("br\n"); n++; } }
		for (i = 0; i < 8; i++) { MCC_TRACE("br\n");
			void *pg =
					mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			if (pg != MAP_FAILED)
				{ MCC_TRACE("br\n"); mccjit_qsbr_retire(pg, 4096); }
			mccjit_pool_nap();
		}
		for (i = 0; i < n; i++)
			{ MCC_TRACE("br\n"); pthread_join(th[i], NULL); }
		mccjit_qsbr_reclaim();
		printf("mccjit-selftest-qsbr: MT smoke (%d threads) after join nlimbo=%d "
					 "reclaimed=%llu leaked=%llu %s\n",
					 n, mccjit_qsbr.nlimbo, (unsigned long long)mccjit_qsbr.reclaimed,
					 (unsigned long long)mccjit_qsbr.leaked,
					 mccjit_qsbr.nlimbo == 0 ? "OK" : "FAIL");
		if (mccjit_qsbr.nlimbo != 0)
			{ MCC_TRACE("br\n"); fails++; }
	}

	mccjit_qsbr_reset();
	printf("mccjit-selftest-qsbr: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_fparg(const char *libpath, const char *incpath) { MCC_TRACE("enter\n");
	static const char src[] = "double f(double a, double b){return a*b + a;}";
	static const char src_g[] = "double g(double a, double b){return a*b + b;}";
	unsigned char *blob;
	size_t blen;
	MCCState *s1;
	double (*baseline)(double, double) = NULL;
	double (*variant)(double, double) = NULL;
	MCCState *bstate = NULL, *vstate = NULL;
	int fails = 0;
	int allfp_seen;
	int i;

	printf("mccjit-selftest-fparg: begin\n");

	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-fparg: stash failed\n");
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return 1;
	}
	baseline = (double (*)(double, double))mcc_jit_recompile_blob(blob, blen);
	allfp_seen = mccjit_last_allfp;
	bstate = mccjit_last_state;
	mccjit_last_state = NULL;
	if (!baseline) { MCC_TRACE("br\n");
		printf("mccjit-selftest-fparg: baseline recompile NULL FAIL\n");
		mcc_free(blob);
		mcc_delete(s1);
		return 1;
	}
	printf("mccjit-selftest-fparg: all-double signature detected=%d (expect 1) %s\n",
				 allfp_seen, allfp_seen ? "OK" : "FAIL");
	if (!allfp_seen)
		{ MCC_TRACE("br\n"); fails++; }
	if (baseline(3.0, 4.0) != 15.0 || baseline(2.5, 2.0) != 7.5) { MCC_TRACE("br\n");
		printf("mccjit-selftest-fparg: baseline miscomputes FAIL\n");
		fails++;
	}

	{
		static const char qsrc[] = "double q(double a){ return a*2.0 + 1.0; }";
		unsigned char *qb;
		size_t ql;
		MCCState *qs;
		qb = mccjit_stash_one(qsrc, "q", 1, &ql, &qs);
		if (qs && qb) { MCC_TRACE("br\n");
			double (*qf)(double) = (double (*)(double))mcc_jit_recompile_blob(qb, ql);
			MCCState *qstate = mccjit_last_state;
			double got = qf ? qf(3.0) : -1.0;
			mccjit_last_state = NULL;
			printf("mccjit-selftest-fparg: FP-constant re-emit q(3)=%g (expect 7) %s\n",
						 got, got == 7.0 ? "OK" : "FAIL");
			if (got != 7.0)
				{ MCC_TRACE("br\n"); fails++; }
			if (qstate)
				{ MCC_TRACE("br\n"); mcc_delete(qstate); }
		}
		mcc_free(qb);
		if (qs)
			{ MCC_TRACE("br\n"); mcc_delete(qs); }
	}

	variant = (double (*)(double, double))mcc_jit_recompile_blob(blob, blen);
	vstate = mccjit_last_state;
	mccjit_last_state = NULL;

	/* FP differential-verify marshalling (mccjit_invoke_fp + bitwise compare),
	   driven directly — a faithful variant must agree with the baseline and not
	   flag; a divergent one (g) must return the baseline and flag. */
	if (variant) { MCC_TRACE("br\n");
		MccjitKgc kgc;
		if (mccjit_kgc_open(&kgc, NULL, mccjit_salt_witness(), 2) == 0) { MCC_TRACE("br\n");
			static const struct {
				double a, b, want;
			} cs[5] = {{3, 4, 15},	 {2.5, 2, 7.5}, {-1, 5, -6},
								 {0, 9, 0}, {1.5, 1.5, 3.75}};
			int okall = 1;
			for (i = 0; i < 5; i++) { MCC_TRACE("br\n");
				double a2[2];
				int flagged = 0;
				double r;
				a2[0] = cs[i].a;
				a2[1] = cs[i].b;
				r = mccjit_kgc_calln_fp(&kgc, (void *)variant, (void *)baseline, a2, 2,
																&flagged);
				if (r != cs[i].want || flagged)
					{ MCC_TRACE("br\n"); okall = 0; }
			}
			printf("mccjit-selftest-fparg: faithful FP verify (5 cases) %s\n",
						 okall ? "OK" : "FAIL");
			if (!okall)
				{ MCC_TRACE("br\n"); fails++; }
			mccjit_kgc_close(&kgc);
		}
	}

	{
		unsigned char *gblob;
		size_t glen;
		MCCState *sg;
		gblob = mccjit_stash_one(src_g, "g", 1, &glen, &sg);
		if (sg && gblob) { MCC_TRACE("br\n");
			double (*gv)(double, double) =
					(double (*)(double, double))mcc_jit_recompile_blob(gblob, glen);
			MCCState *gstate = mccjit_last_state;
			mccjit_last_state = NULL;
			if (gv) { MCC_TRACE("br\n");
				MccjitKgc kgc;
				if (mccjit_kgc_open(&kgc, NULL, mccjit_salt_witness(), 2) == 0) { MCC_TRACE("br\n");
					double args[2];
					int flagged = 0;
					double r;
					args[0] = 3.0;
					args[1] = 4.0;
					r = mccjit_kgc_calln_fp(&kgc, (void *)gv, (void *)baseline, args, 2,
																	&flagged);
					printf("mccjit-selftest-fparg: mismatch f-vs-g returned=%g flagged=%d "
								 "(expect 15,1) %s\n",
								 r, flagged, (r == 15.0 && flagged) ? "OK" : "FAIL");
					if (r != 15.0 || !flagged)
						{ MCC_TRACE("br\n"); fails++; }
					mccjit_kgc_close(&kgc);
				}
			}
			if (gstate)
				{ MCC_TRACE("br\n"); mcc_delete(gstate); }
		}
		mcc_free(gblob);
		if (sg)
			{ MCC_TRACE("br\n"); mcc_delete(sg); }
	}

	/* End-to-end: a -run program self-recompiles its all-double f and calls it
	   through the mode-6 dispatch slot -> the hand-emitted FP stub (movsd spill
	   + calln_fp). This exercises the stub via its correct entry (jmp *slot);
	   direct-calling a dispatch stub corrupts the frame. */
	{
		static const char prog[] =
				"double f(double a, double b){return a*b + a;}\n"
				"int main(void){ double r = f(3.0, 4.0); return (int)r; }\n";
		MCCState *rs;
		char *av[] = {"fparg", NULL};
		char path[64];
		int rc = -1;
		int swapped = 0;
		setenv("MCC_AST_JIT_DISPATCH", "6", 1);
		setenv("MCC_JIT_PERF_MAP", "1", 1);
		snprintf(path, sizeof path, "/tmp/perf-%d.map", (int)getpid());
		remove(path);
		rs = mcc_new();
		if (rs) { MCC_TRACE("br\n");
			FILE *pf;
			if (libpath)
				{ MCC_TRACE("br\n"); mcc_set_lib_path(rs, libpath); }
			if (incpath)
				{ MCC_TRACE("br\n"); mcc_add_include_path(rs, incpath); }
			rs->optimize = 1;
			rs->embed_jit = 1;
			rs->jit_threads = 0;
			mcc_free(rs->jit_functions);
			rs->jit_functions = mcc_strdup("f");
			mcc_set_output_type(rs, MCC_OUTPUT_MEMORY);
			if (mcc_compile_string(rs, prog) == 0)
				{ MCC_TRACE("br\n"); rc = mcc_run(rs, 1, av); }
			pf = fopen(path, "r");
			if (pf) { MCC_TRACE("br\n");
				char line[256];
				while (fgets(line, sizeof line, pf)) { MCC_TRACE("br\n");
					char nm[128] = {0};
					unsigned long a = 0, sz = 0;
					if (sscanf(line, "%lx %lx %127s", &a, &sz, nm) == 3 &&
							!strcmp(nm, "f"))
						{ MCC_TRACE("br\n"); swapped = 1; }
				}
				fclose(pf);
			}
			mcc_delete(rs);
		}
		remove(path);
		unsetenv("MCC_AST_JIT_DISPATCH");
		unsetenv("MCC_JIT_PERF_MAP");
		printf("mccjit-selftest-fparg: end-to-end dispatch main()=%d (expect 15) "
					 "fp-stub-swapped=%d %s\n",
					 rc, swapped, (rc == 15 && swapped) ? "OK" : "FAIL");
		if (rc != 15 || !swapped)
			{ MCC_TRACE("br\n"); fails++; }
	}

	if (vstate)
		{ MCC_TRACE("br\n"); mcc_delete(vstate); }
	if (bstate)
		{ MCC_TRACE("br\n"); mcc_delete(bstate); }
	mccjit_last_state = NULL;
	mcc_free(blob);
	mcc_delete(s1);
	printf("mccjit-selftest-fparg: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_mixed(const char *libpath, const char *incpath) { MCC_TRACE("enter\n");
#if !defined(__x86_64__)
	(void)libpath;
	(void)incpath;
	printf("mccjit-selftest-mixed: non-x86_64 SKIP\n");
	return 0;
#else
	static const char src_f[] =
			"long f(long a, double b, long c){ return a + (long)b + c; }";
	static const char src_g[] =
			"long g(long a, double b, long c){ return a + (long)b + c + 1; }";
	static const char src_h[] =
			"double h(long a, double b){ return (double)a + b*2.0; }";
	unsigned char *blob;
	size_t blen;
	MCCState *s1;
	MCCState *fbstate = NULL, *fvstate = NULL;
	long (*fbase)(long, double, long) = NULL;
	long (*fvar)(long, double, long) = NULL;
	int fails = 0;
	int mixed_seen, ngp_seen, nsse_seen, retfp_seen;

	printf("mccjit-selftest-mixed: begin (scalar mixed GP+FP marshalling)\n");

	blob = mccjit_stash_one(src_f, "f", 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-mixed: stash failed\n");
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return 1;
	}
	fbase = (long (*)(long, double, long))mcc_jit_recompile_blob(blob, blen);
	mixed_seen = mccjit_last_mixed;
	ngp_seen = (int)mccjit_last_ngp;
	nsse_seen = (int)mccjit_last_nsse;
	retfp_seen = mccjit_last_ret_fp;
	fbstate = mccjit_last_state;
	mccjit_last_state = NULL;
	if (!fbase) { MCC_TRACE("br\n");
		printf("mccjit-selftest-mixed: baseline recompile NULL FAIL\n");
		mcc_free(blob);
		mcc_delete(s1);
		return 1;
	}
	printf("mccjit-selftest-mixed: f(long,double,long)->long classified mixed=%d "
				 "ngp=%d nsse=%d ret_fp=%d (expect 1,2,1,0) %s\n",
				 mixed_seen, ngp_seen, nsse_seen, retfp_seen,
				 (mixed_seen && ngp_seen == 2 && nsse_seen == 1 && !retfp_seen) ? "OK"
																																				: "FAIL");
	if (!mixed_seen || ngp_seen != 2 || nsse_seen != 1 || retfp_seen)
		{ MCC_TRACE("br\n"); fails++; }

	fvar = (long (*)(long, double, long))mcc_jit_recompile_blob(blob, blen);
	fvstate = mccjit_last_state;
	mccjit_last_state = NULL;

	{
		int64_t gpv[MCCJIT_KGC_MAXARG] = {5, 3, 0, 0, 0, 0};
		double fpv[MCCJIT_KGC_MAXARG] = {2.5, 0, 0, 0, 0, 0};
		long direct = fbase(5, 2.5, 3);
		int64_t thunked = mccjit_invoke_mixed_i((void *)fbase, gpv, fpv);
		printf("mccjit-selftest-mixed: thunk marshalling direct=%ld thunk=%lld "
					 "(expect 10,10) %s\n",
					 direct, (long long)thunked,
					 (direct == 10 && thunked == 10) ? "OK" : "FAIL");
		if (direct != 10 || thunked != 10)
			{ MCC_TRACE("br\n"); fails++; }
	}

	if (fvar) { MCC_TRACE("br\n");
		MccjitKgc kgc;
		if (mccjit_kgc_open(&kgc, NULL, mccjit_salt_witness(), 3) == 0) { MCC_TRACE("br\n");
			int64_t gpv[MCCJIT_KGC_MAXARG] = {5, 3, 0, 0, 0, 0};
			double fpv[MCCJIT_KGC_MAXARG] = {2.5, 0, 0, 0, 0, 0};
			int flag = 0;
			int64_t r;
			kgc.memoize_ok = 0;
			kgc.ret_wide = 1;
			kgc.mx_variant = (void *)fvar;
			kgc.mx_baseline = (void *)fbase;
			kgc.mx_ngp = 2;
			kgc.mx_nsse = 1;
			kgc.mx_ret_fp = 0;
			kgc.mx_flag = &flag;
			r = mccjit_kgc_calln_mixed_i(&kgc, gpv, fpv);
			printf("mccjit-selftest-mixed: faithful GP-ret verify r=%lld flag=%d "
						 "(expect 10,0) %s\n",
						 (long long)r, flag, (r == 10 && !flag) ? "OK" : "FAIL");
			if (r != 10 || flag)
				{ MCC_TRACE("br\n"); fails++; }
			mccjit_kgc_close(&kgc);
		}
	}

	{
		unsigned char *gblob;
		size_t glen;
		MCCState *sg;
		gblob = mccjit_stash_one(src_g, "g", 1, &glen, &sg);
		if (sg && gblob) { MCC_TRACE("br\n");
			long (*gv)(long, double, long) =
					(long (*)(long, double, long))mcc_jit_recompile_blob(gblob, glen);
			MCCState *gstate = mccjit_last_state;
			mccjit_last_state = NULL;
			if (gv) { MCC_TRACE("br\n");
				MccjitKgc kgc;
				if (mccjit_kgc_open(&kgc, NULL, mccjit_salt_witness(), 3) == 0) { MCC_TRACE("br\n");
					int64_t gpv[MCCJIT_KGC_MAXARG] = {5, 3, 0, 0, 0, 0};
					double fpv[MCCJIT_KGC_MAXARG] = {2.5, 0, 0, 0, 0, 0};
					int flag = 0;
					int64_t r;
					kgc.memoize_ok = 0;
					kgc.ret_wide = 1;
					kgc.mx_variant = (void *)gv;
					kgc.mx_baseline = (void *)fbase;
					kgc.mx_ngp = 2;
					kgc.mx_nsse = 1;
					kgc.mx_ret_fp = 0;
					kgc.mx_flag = &flag;
					r = mccjit_kgc_calln_mixed_i(&kgc, gpv, fpv);
					printf("mccjit-selftest-mixed: divergent GP-ret r=%lld flag=%d "
								 "(expect 10,1) %s\n",
								 (long long)r, flag, (r == 10 && flag) ? "OK" : "FAIL");
					if (r != 10 || !flag)
						{ MCC_TRACE("br\n"); fails++; }
					mccjit_kgc_close(&kgc);
				}
			}
			if (gstate)
				{ MCC_TRACE("br\n"); mcc_delete(gstate); }
		}
		mcc_free(gblob);
		if (sg)
			{ MCC_TRACE("br\n"); mcc_delete(sg); }
	}

	{
		unsigned char *hblob;
		size_t hlen;
		MCCState *sh;
		hblob = mccjit_stash_one(src_h, "h", 1, &hlen, &sh);
		if (sh && hblob) { MCC_TRACE("br\n");
			double (*hb)(long, double) =
					(double (*)(long, double))mcc_jit_recompile_blob(hblob, hlen);
			MCCState *hstate = mccjit_last_state;
			int h_mixed = mccjit_last_mixed, h_retfp = mccjit_last_ret_fp;
			int h_ngp = (int)mccjit_last_ngp, h_nsse = (int)mccjit_last_nsse;
			mccjit_last_state = NULL;
			printf("mccjit-selftest-mixed: h(long,double)->double classified mixed=%d "
						 "ngp=%d nsse=%d ret_fp=%d (expect 1,1,1,1) %s\n",
						 h_mixed, h_ngp, h_nsse, h_retfp,
						 (h_mixed && h_ngp == 1 && h_nsse == 1 && h_retfp) ? "OK" : "FAIL");
			if (!h_mixed || h_ngp != 1 || h_nsse != 1 || !h_retfp)
				{ MCC_TRACE("br\n"); fails++; }
			if (hb) { MCC_TRACE("br\n");
				MccjitKgc kgc;
				if (mccjit_kgc_open(&kgc, NULL, mccjit_salt_witness(), 2) == 0) { MCC_TRACE("br\n");
					int64_t gpv[MCCJIT_KGC_MAXARG] = {3, 0, 0, 0, 0, 0};
					double fpv[MCCJIT_KGC_MAXARG] = {1.5, 0, 0, 0, 0, 0};
					int flag = 0;
					double r;
					kgc.memoize_ok = 0;
					kgc.ret_wide = 1;
					kgc.mx_variant = (void *)hb;
					kgc.mx_baseline = (void *)hb;
					kgc.mx_ngp = 1;
					kgc.mx_nsse = 1;
					kgc.mx_ret_fp = 1;
					kgc.mx_flag = &flag;
					r = mccjit_kgc_calln_mixed_d(&kgc, gpv, fpv);
					printf("mccjit-selftest-mixed: faithful FP-ret verify r=%g flag=%d "
								 "(expect 6,0) %s\n",
								 r, flag, (r == 6.0 && !flag) ? "OK" : "FAIL");
					if (r != 6.0 || flag)
						{ MCC_TRACE("br\n"); fails++; }
					mccjit_kgc_close(&kgc);
				}
			}
			if (hstate)
				{ MCC_TRACE("br\n"); mcc_delete(hstate); }
		}
		mcc_free(hblob);
		if (sh)
			{ MCC_TRACE("br\n"); mcc_delete(sh); }
	}

	{
		static const char prog[] =
				"long f(long a, double b, long c){ return a + (long)b + c; }\n"
				"int main(void){ return (int)f(5, 2.5, 3); }\n";
		MCCState *rs;
		char *av[] = {"mixed", NULL};
		char path[64];
		int rc = -1;
		int swapped = 0;
		setenv("MCC_AST_JIT_DISPATCH", "6", 1);
		setenv("MCC_JIT_PERF_MAP", "1", 1);
		snprintf(path, sizeof path, "/tmp/perf-%d.map", (int)getpid());
		remove(path);
		rs = mcc_new();
		if (rs) { MCC_TRACE("br\n");
			FILE *pf;
			if (libpath)
				{ MCC_TRACE("br\n"); mcc_set_lib_path(rs, libpath); }
			if (incpath)
				{ MCC_TRACE("br\n"); mcc_add_include_path(rs, incpath); }
			rs->optimize = 1;
			rs->embed_jit = 1;
			rs->jit_threads = 0;
			mcc_free(rs->jit_functions);
			rs->jit_functions = mcc_strdup("f");
			mcc_set_output_type(rs, MCC_OUTPUT_MEMORY);
			if (mcc_compile_string(rs, prog) == 0)
				{ MCC_TRACE("br\n"); rc = mcc_run(rs, 1, av); }
			pf = fopen(path, "r");
			if (pf) { MCC_TRACE("br\n");
				char line[256];
				while (fgets(line, sizeof line, pf)) { MCC_TRACE("br\n");
					char nm[128] = {0};
					unsigned long a = 0, sz = 0;
					if (sscanf(line, "%lx %lx %127s", &a, &sz, nm) == 3 && !strcmp(nm, "f"))
						{ MCC_TRACE("br\n"); swapped = 1; }
				}
				fclose(pf);
			}
			mcc_delete(rs);
		}
		remove(path);
		unsetenv("MCC_AST_JIT_DISPATCH");
		unsetenv("MCC_JIT_PERF_MAP");
		printf("mccjit-selftest-mixed: end-to-end dispatch main()=%d (expect 10) "
					 "mixed-stub-swapped=%d %s\n",
					 rc, swapped, (rc == 10 && swapped) ? "OK" : "FAIL");
		if (rc != 10 || !swapped)
			{ MCC_TRACE("br\n"); fails++; }
	}

	if (fvstate)
		{ MCC_TRACE("br\n"); mcc_delete(fvstate); }
	if (fbstate)
		{ MCC_TRACE("br\n"); mcc_delete(fbstate); }
	mcc_free(blob);
	mcc_delete(s1);
	printf("mccjit-selftest-mixed: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
#endif
}

static long mccjit_profile_id1(long x) { MCC_TRACE("enter\n"); return x; }
static long mccjit_profile_sum2(long a, long b) { MCC_TRACE("enter\n"); return a + b; }

int mccjit_selftest_vrange(void) { MCC_TRACE("enter\n");
	static const char src[] = "int f(int a, int b){return a*100 + b;}";
	unsigned char *blob;
	size_t blen;
	MCCState *s1;
	int (*baseline)(int, int) = NULL;
	MCCState *bstate = NULL;
	MccjitCounterState st;
	void *stub;
	int fails = 0;
	int i;
	int pidx = -1;
	int64_t pval = 0;

	printf("mccjit-selftest-vrange: begin\n");

	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-vrange: stash failed\n");
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return 1;
	}
	baseline = (int (*)(int, int))mcc_jit_recompile_blob(blob, blen);
	bstate = mccjit_last_state;
	mccjit_last_state = NULL;
	if (!baseline) { MCC_TRACE("br\n");
		printf("mccjit-selftest-vrange: baseline recompile NULL FAIL\n");
		mcc_free(blob);
		mcc_delete(s1);
		return 1;
	}

	memset(&st, 0, sizeof st);
	st.baseline = (void *)baseline;
	st.threshold = 1L << 30;
	pthread_mutex_init(&st.lock, NULL);
	stub = mccjit_make_counter_stub(&st);
	if (!stub) { MCC_TRACE("br\n");
		printf("mccjit-selftest-vrange: counter stub NULL (non-x86_64?) SKIP\n");
		pthread_mutex_destroy(&st.lock);
		if (bstate)
			{ MCC_TRACE("br\n"); mcc_delete(bstate); }
		mcc_free(blob);
		mcc_delete(s1);
		return 0;
	}
	for (i = 1; i <= 5; i++)
		{ MCC_TRACE("br\n"); ((int (*)(int, int))stub)(7, i); }

	if (!mccjit_profile_pick_const(&st, 2, 3, &pidx, &pval) || pidx != 0 ||
			pval != 7) { MCC_TRACE("br\n");
		printf("mccjit-selftest-vrange: const-param detect pidx=%d pval=%lld "
					 "(expect 0,7) FAIL\n",
					 pidx, (long long)pval);
		fails++;
	} else { MCC_TRACE("br\n");
		printf("mccjit-selftest-vrange: profile detected const param a==7 OK\n");
	}

	{
		int (*v)(int, int) =
				(int (*)(int, int))mccjit_recompile_profiled(blob, blen, &st, 2, 3);
		MCCState *vstate = mccjit_last_state;
		mccjit_last_state = NULL;
		if (!v) { MCC_TRACE("br\n");
			printf("mccjit-selftest-vrange: profiled recompile NULL FAIL\n");
			fails++;
		} else { MCC_TRACE("br\n");
			int in_domain = v(7, 5);
			int folded = v(999, 5);
			int base = baseline(7, 5);
			printf("mccjit-selftest-vrange: spec v(7,5)=%d v(999,5)=%d base(7,5)=%d\n",
						 in_domain, folded, base);
			if (in_domain != 705 || base != 705) { MCC_TRACE("br\n");
				printf("mccjit-selftest-vrange: in-domain value wrong FAIL\n");
				fails++;
			}
			if (folded != 705) { MCC_TRACE("br\n");
				printf("mccjit-selftest-vrange: a NOT folded (v(999,5)=%d, expect 705) "
							 "FAIL\n",
							 folded);
				fails++;
			} else { MCC_TRACE("br\n");
				printf("mccjit-selftest-vrange: speculation folded a=7 (variant ignores "
							 "passed a) OK\n");
			}
		}
		if (vstate)
			{ MCC_TRACE("br\n"); mcc_delete(vstate); }
		mccjit_last_state = NULL;
	}
	pthread_mutex_destroy(&st.lock);

	{
		MccjitCounterState st2;
		void *stub2;
		memset(&st2, 0, sizeof st2);
		st2.baseline = (void *)baseline;
		st2.threshold = 1L << 30;
		pthread_mutex_init(&st2.lock, NULL);
		stub2 = mccjit_make_counter_stub(&st2);
		if (stub2) { MCC_TRACE("br\n");
			for (i = 0; i < 5; i++)
				{ MCC_TRACE("br\n"); ((int (*)(int, int))stub2)(i + 1, i * 2); }
			if (mccjit_profile_pick_const(&st2, 2, 3, NULL, NULL)) { MCC_TRACE("br\n");
				printf("mccjit-selftest-vrange: false-positive const on varying params "
							 "FAIL\n");
				fails++;
			} else { MCC_TRACE("br\n");
				int (*v2)(int, int) =
						(int (*)(int, int))mccjit_recompile_profiled(blob, blen, &st2, 2, 3);
				MCCState *v2state = mccjit_last_state;
				mccjit_last_state = NULL;
				if (v2 && v2(999, 5) == baseline(999, 5)) { MCC_TRACE("br\n");
					printf("mccjit-selftest-vrange: no const param -> unspecialized "
								 "(v(999,5)=base) OK\n");
				} else { MCC_TRACE("br\n");
					printf("mccjit-selftest-vrange: no-const case wrong FAIL\n");
					fails++;
				}
				if (v2state)
					{ MCC_TRACE("br\n"); mcc_delete(v2state); }
				mccjit_last_state = NULL;
			}
		}
		pthread_mutex_destroy(&st2.lock);
	}

	if (bstate)
		{ MCC_TRACE("br\n"); mcc_delete(bstate); }
	mccjit_last_state = NULL;
	mcc_free(blob);
	mcc_delete(s1);
	printf("mccjit-selftest-vrange: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_profile(void) { MCC_TRACE("enter\n");
	MccjitCounterState st;
	void *stub;
	int fails = 0;
	int64_t inputs[5] = {5, 0, -3, 100, 7};
	int i;

	printf("mccjit-selftest-profile: begin\n");

	memset(&st, 0, sizeof st);
	st.baseline = (void *)mccjit_profile_id1;
	st.threshold = 1L << 30;
	pthread_mutex_init(&st.lock, NULL);
	stub = mccjit_make_counter_stub(&st);
	if (!stub) { MCC_TRACE("br\n");
		printf("mccjit-selftest-profile: counter stub NULL (non-x86_64?) SKIP\n");
		pthread_mutex_destroy(&st.lock);
		return 0;
	}
	for (i = 0; i < 5; i++) { MCC_TRACE("br\n");
		long r = ((long (*)(long))stub)((long)inputs[i]);
		if (r != inputs[i]) { MCC_TRACE("br\n");
			printf("mccjit-selftest-profile: stub(%lld) returned %ld (expect %lld) FAIL\n",
						 (long long)inputs[i], r, (long long)inputs[i]);
			fails++;
		}
	}
	printf("mccjit-selftest-profile: 1-arg range=[%lld,%lld] seen=%ld nsample=%d\n",
				 (long long)st.argmin[0], (long long)st.argmax[0], st.argseen,
				 st.nsample);
	if (st.argmin[0] != -3 || st.argmax[0] != 100 || st.argseen != 5) { MCC_TRACE("br\n");
		printf("mccjit-selftest-profile: 1-arg range/seen mismatch FAIL\n");
		fails++;
	}
	if (st.nsample != 5) { MCC_TRACE("br\n");
		printf("mccjit-selftest-profile: nsample=%d (expect 5) FAIL\n", st.nsample);
		fails++;
	} else { MCC_TRACE("br\n");
		for (i = 0; i < 5; i++)
			{ MCC_TRACE("br\n"); if (st.sample[i][0] != inputs[i]) { MCC_TRACE("br\n");
				printf("mccjit-selftest-profile: sample[%d][0]=%lld (expect %lld) FAIL\n",
							 i, (long long)st.sample[i][0], (long long)inputs[i]);
				fails++;
			} }
	}
	pthread_mutex_destroy(&st.lock);

	memset(&st, 0, sizeof st);
	st.baseline = (void *)mccjit_profile_sum2;
	st.threshold = 1L << 30;
	pthread_mutex_init(&st.lock, NULL);
	stub = mccjit_make_counter_stub(&st);
	if (stub) { MCC_TRACE("br\n");
		((long (*)(long, long))stub)(3, 7);
		((long (*)(long, long))stub)(-2, 20);
		printf("mccjit-selftest-profile: 2-arg p0=[%lld,%lld] p1=[%lld,%lld]\n",
					 (long long)st.argmin[0], (long long)st.argmax[0],
					 (long long)st.argmin[1], (long long)st.argmax[1]);
		if (st.argmin[0] != -2 || st.argmax[0] != 3 || st.argmin[1] != 7 ||
				st.argmax[1] != 20) { MCC_TRACE("br\n");
			printf("mccjit-selftest-profile: 2-arg per-param range mismatch FAIL\n");
			fails++;
		}
	}
	pthread_mutex_destroy(&st.lock);

	printf("mccjit-selftest-profile: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static void mccjit_evalgate_compile(const char *src) { MCC_TRACE("enter\n");
	MCCState *s = mcc_new();
	if (!s)
		{ MCC_TRACE("br\n"); return; }
	s->optimize = 1;
	s->nostdlib = 1;
	mcc_free(s->jit_functions);
	s->jit_functions = mcc_strdup("f");
	mcc_set_output_type(s, MCC_OUTPUT_MEMORY);
	mcc_compile_string(s, src);
	mcc_delete(s);
}

int mccjit_selftest_evalgate(void) { MCC_TRACE("enter\n");
#if !defined(__x86_64__)
	printf("mccjit-selftest-evalgate: non-x86_64 SKIP "
				 "(spec-slice eval-gate path is x86_64-only; arm64 dispatches mode-6)\n");
	return 0;
#else
	static const char src[] = "int f(int *p){return *p + 1;}";
	int fails = 0;
	int r0, r1, r2;

	printf("mccjit-selftest-evalgate: begin (7A eval-slice hard gate)\n");
	setenv("MCC_AST_JIT_DISPATCH", "3", 1);

	setenv("MCC_AST_JIT_EVAL_GATE", "1", 1);
	setenv("MCC_AST_EVAL_FORCE_UNSOUND", "1", 1);
	r0 = ast_jit_eval_refused_count();
	mccjit_evalgate_compile(src);
	r1 = ast_jit_eval_refused_count();
	unsetenv("MCC_AST_EVAL_FORCE_UNSOUND");
	printf("mccjit-selftest-evalgate: forced-unsound + gate: refused delta=%d "
				 "(expect >=1) %s\n",
				 r1 - r0, (r1 > r0) ? "OK" : "FAIL");
	if (r1 <= r0)
		{ MCC_TRACE("br\n"); fails++; }

	mccjit_evalgate_compile(src);
	r2 = ast_jit_eval_refused_count();
	printf("mccjit-selftest-evalgate: sound spec + gate: refused delta=%d "
				 "(expect 0) %s\n",
				 r2 - r1, (r2 == r1) ? "OK" : "FAIL");
	if (r2 != r1)
		{ MCC_TRACE("br\n"); fails++; }

	setenv("MCC_AST_EVAL_FORCE_UNSOUND", "1", 1);
	unsetenv("MCC_AST_JIT_EVAL_GATE");
	mccjit_evalgate_compile(src);
	if (ast_jit_eval_refused_count() != r2) { MCC_TRACE("br\n");
		printf("mccjit-selftest-evalgate: gate OFF still refused (should not) FAIL\n");
		fails++;
	} else { MCC_TRACE("br\n");
		printf("mccjit-selftest-evalgate: gate OFF -> no refusal (rollout opt-in) OK\n");
	}
	unsetenv("MCC_AST_EVAL_FORCE_UNSOUND");
	unsetenv("MCC_AST_JIT_DISPATCH");

	printf("mccjit-selftest-evalgate: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
#endif
}

int mccjit_selftest_slice(void) { MCC_TRACE("enter\n");
	static const struct {
		const char *src;
		const char *fn;
		int want_impure;
		int want_loads_pos;
	} cases[5] = {
			{"int f(int x){return x*2+1;}", "f", 0, 0},
			{"int r(int *p, int x){return *p + x;}", "r", 0, 1},
			{"int s(int *p, int x){*p = x; return x;}", "s", 1, 0},
			{"int s2(int *p, int *q, int x){*p = x; *q = x + 1; return x;}", "s2", 2, 0},
			{"int mabs(int); int h(int x){return mabs(x) + 1;}", "h", 1, 0},
	};
	int fails = 0;
	int i;

	printf("mccjit-selftest-slice: begin (M5c pure/impure partition analysis)\n");
	for (i = 0; i < 5; i++) { MCC_TRACE("br\n");
		unsigned char *blob;
		size_t blen;
		MCCState *s1;
		AstSliceProfile prof;
		int ok;
		blob = mccjit_stash_one(cases[i].src, cases[i].fn, 1, &blen, &s1);
		if (!s1 || !blob) { MCC_TRACE("br\n");
			printf("mccjit-selftest-slice: %s stash failed\n", cases[i].fn);
			if (s1)
				{ MCC_TRACE("br\n"); mcc_delete(s1); }
			mcc_free(blob);
			fails++;
			continue;
		}
		if (mccjit_slice_profile_blob(blob, blen, &prof) != 0) { MCC_TRACE("br\n");
			printf("mccjit-selftest-slice: %s profile failed\n", cases[i].fn);
			fails++;
		} else { MCC_TRACE("br\n");
			ok = (prof.impure_ops == cases[i].want_impure) && (prof.pure_compute > 0) &&
					 (!cases[i].want_loads_pos || prof.loads > 0);
			printf("mccjit-selftest-slice: %-3s impure=%d(want %d) loads=%d compute=%d "
						 "nodes=%d %s\n",
						 cases[i].fn, prof.impure_ops, cases[i].want_impure, prof.loads,
						 prof.pure_compute, prof.nodes, ok ? "OK" : "FAIL");
			if (!ok)
				{ MCC_TRACE("br\n"); fails++; }
		}
		mcc_free(blob);
		mcc_delete(s1);
	}
	printf("mccjit-selftest-slice: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static AstLocal mccjit_subtree_count(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal c, k = 1;
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); k += mccjit_subtree_count(a, c); }
	return k;
}

static int mccjit_slice_walk_eq(const AstArena *A, AstLocal na, const AstArena *B,
																AstLocal nb) { MCC_TRACE("enter\n");
	AstLocal ca, cb;
	if (ast_kind(A, na) != ast_kind(B, nb) || ast_op(A, na) != ast_op(B, nb) ||
			ast_type_t(A, na) != ast_type_t(B, nb) ||
			ast_type_ref(A, na) != ast_type_ref(B, nb) ||
			ast_ival(A, na) != ast_ival(B, nb) || ast_fbits(A, na) != ast_fbits(B, nb) ||
			ast_sym(A, na) != ast_sym(B, nb) || ast_cst(A, na) != ast_cst(B, nb) ||
			ast_nchild(A, na) != ast_nchild(B, nb))
		{ MCC_TRACE("br\n"); return 0; }
	ca = ast_first_child(A, na);
	cb = ast_first_child(B, nb);
	while (ca != AST_NONE && cb != AST_NONE) { MCC_TRACE("br\n");
		if (!mccjit_slice_walk_eq(A, ca, B, cb))
			{ MCC_TRACE("br\n"); return 0; }
		ca = ast_next_sib(A, ca);
		cb = ast_next_sib(B, cb);
	}
	return ca == AST_NONE && cb == AST_NONE;
}

static int mccjit_slice_extract_blob(const void *buf, size_t len,
																		 int *checked) { MCC_TRACE("enter\n");
	MccjitIntent it;
	MCCState *js;
	int fails = 0, chk = 0;
	AstLocal r, nn;
	js = mcc_new();
	if (!js)
		{ MCC_TRACE("br\n"); return -1; }
	js->optimize = 0;
	js->nostdlib = 1;
	mcc_set_output_type(js, MCC_OUTPUT_MEMORY);
	mcc_enter_state(js);
	mccpp_new(js);
	mccgen_init(js);
	anon_sym = SYM_FIRST_ANOM;
	funcname = "";
	func_ind = -1;
	if (mccjit_intent_deserialize(buf, len, &it) != 0) { MCC_TRACE("br\n");
		mcc_exit_state(js);
		mcc_delete(js);
		return -1;
	}
	nn = ast_count(it.arena);
	for (r = 0; r < nn; r++) { MCC_TRACE("br\n");
		AstArena *sl = ast_slice_extract(it.arena, r);
		AstLocal want = mccjit_subtree_count(it.arena, r);
		int ok;
		chk++;
		if (!sl) { MCC_TRACE("br\n"); fails++; continue; }
		ok = ast_count(sl) == want && ast_root(sl) == 0 &&
				 ast_parent(sl, 0) == AST_NONE && ast_next_sib(sl, 0) == AST_NONE &&
				 mccjit_slice_walk_eq(it.arena, r, sl, 0) &&
				 ast_intention_hash(sl, 0) == ast_intention_hash(it.arena, r);
		if (!ok)
			{ MCC_TRACE("br\n"); fails++; }
		ast_arena_free(sl);
	}
	mccjit_intent_release(&it);
	mcc_exit_state(js);
	mcc_delete(js);
	*checked = chk;
	return fails;
}

int mccjit_selftest_sliceextract(void) { MCC_TRACE("enter\n");
	static const struct {
		const char *src;
		const char *fn;
	} cases[5] = {
			{"int f(int x){return x*2+1;}", "f"},
			{"int r(int *p, int x){return *p + x;}", "r"},
			{"int s(int *p, int x){*p = x; return x;}", "s"},
			{"int s2(int *p, int *q, int x){*p = x; *q = x + 1; return x;}", "s2"},
			{"int mabs(int); int h(int x){return mabs(x) + 1;}", "h"},
	};
	int fails = 0, i, total = 0;

	printf("mccjit-selftest-sliceextract: begin (K7 slice-extraction primitive)\n");
	for (i = 0; i < 5; i++) { MCC_TRACE("br\n");
		unsigned char *blob;
		size_t blen;
		MCCState *s1;
		int chk = 0, f;
		blob = mccjit_stash_one(cases[i].src, cases[i].fn, 1, &blen, &s1);
		if (!s1 || !blob) { MCC_TRACE("br\n");
			printf("mccjit-selftest-sliceextract: %s stash failed\n", cases[i].fn);
			if (s1)
				{ MCC_TRACE("br\n"); mcc_delete(s1); }
			mcc_free(blob);
			fails++;
			continue;
		}
		f = mccjit_slice_extract_blob(blob, blen, &chk);
		total += chk;
		printf("mccjit-selftest-sliceextract: %-3s slices=%d %s\n", cases[i].fn, chk,
					 f == 0 ? "OK" : "FAIL");
		if (f != 0)
			{ MCC_TRACE("br\n"); fails++; }
		mcc_free(blob);
		mcc_delete(s1);
	}
	printf("mccjit-selftest-sliceextract: %s (%d failure%s, %d slices checked)\n",
				 fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s", total);
	return fails ? 1 : 0;
}

static AstLocal mccjit_ret_expr(const AstArena *a) { MCC_TRACE("enter\n");
	AstLocal r, nn = ast_count(a);
	for (r = 0; r < nn; r++) { MCC_TRACE("br\n");
		if (ast_kind(a, r) == AST_Return && ast_nchild(a, r) == 1)
			{ MCC_TRACE("br\n"); return ast_first_child(a, r); }
	}
	return AST_NONE;
}

static AstLocal mccjit_find_kind(const AstArena *a, AstLocal n,
																 uint16_t kind) { MCC_TRACE("enter\n");
	AstLocal c, r;
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if (ast_kind(a, n) == kind)
		{ MCC_TRACE("br\n"); return n; }
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c)) { MCC_TRACE("br\n");
		r = mccjit_find_kind(a, c, kind);
		if (r != AST_NONE)
			{ MCC_TRACE("br\n"); return r; }
	}
	return AST_NONE;
}

static AstArena *mccjit_extract_ret_slice(const char *src,
																					const char *fn) { MCC_TRACE("enter\n");
	unsigned char *blob;
	size_t blen;
	MCCState *s1, *js;
	MccjitIntent it;
	AstArena *slice = NULL;
	blob = mccjit_stash_one(src, fn, 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return NULL;
	}
	js = mcc_new();
	if (!js) { MCC_TRACE("br\n");
		mcc_free(blob);
		mcc_delete(s1);
		return NULL;
	}
	js->optimize = 0;
	js->nostdlib = 1;
	mcc_set_output_type(js, MCC_OUTPUT_MEMORY);
	mcc_enter_state(js);
	mccpp_new(js);
	mccgen_init(js);
	anon_sym = SYM_FIRST_ANOM;
	funcname = "";
	func_ind = -1;
	if (mccjit_intent_deserialize(blob, blen, &it) == 0) { MCC_TRACE("br\n");
		AstLocal ret = mccjit_ret_expr(it.arena);
		if (ret != AST_NONE)
			{ MCC_TRACE("br\n"); slice = ast_slice_extract(it.arena, ret); }
		mccjit_intent_release(&it);
	}
	mcc_exit_state(js);
	mcc_delete(js);
	mcc_free(blob);
	mcc_delete(s1);
	return slice;
}

static int mccjit_certify_one(const char *src, const char *fn, int want,
															int do_equiv) { MCC_TRACE("enter\n");
	unsigned char *blob;
	size_t blen;
	MCCState *s1, *js;
	MccjitIntent it;
	int fails = 0;
	blob = mccjit_stash_one(src, fn, 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-sliceoracle: %s stash failed\n", fn);
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return 1;
	}
	js = mcc_new();
	if (js) { MCC_TRACE("br\n");
		js->optimize = 0;
		js->nostdlib = 1;
		mcc_set_output_type(js, MCC_OUTPUT_MEMORY);
		mcc_enter_state(js);
		mccpp_new(js);
		mccgen_init(js);
		anon_sym = SYM_FIRST_ANOM;
		funcname = "";
		func_ind = -1;
		if (mccjit_intent_deserialize(blob, blen, &it) == 0) { MCC_TRACE("br\n");
			AstLocal ret = mccjit_ret_expr(it.arena);
			int cert = (ret != AST_NONE) ? ast_slice_certifiable(it.arena, ret) : -1;
			int ok = (cert == want);
			printf("mccjit-selftest-sliceoracle: %-3s certifiable=%d(want %d) %s\n", fn,
						 cert, want, ok ? "OK" : "FAIL");
			if (!ok)
				{ MCC_TRACE("br\n"); fails++; }
			if (do_equiv && ret != AST_NONE) { MCC_TRACE("br\n");
				AstArena *sl = ast_slice_extract(it.arena, ret);
				AstLocal lit = mccjit_find_kind(it.arena, ret, AST_Literal);
				int self_eq = ast_slice_equiv(it.arena, ret, it.arena, ret);
				int slice_eq = sl ? ast_slice_equiv(it.arena, ret, sl, ast_root(sl)) : 0;
				int neg_eq = (lit != AST_NONE)
												 ? ast_slice_equiv(it.arena, ret, it.arena, lit)
												 : 1;
				printf("mccjit-selftest-sliceoracle: %-3s equiv self=%d slice=%d neg=%d "
							 "(want 1,1,0) %s\n",
							 fn, self_eq, slice_eq, neg_eq,
							 (self_eq == 1 && slice_eq == 1 && neg_eq == 0) ? "OK" : "FAIL");
				if (!(self_eq == 1 && slice_eq == 1 && neg_eq == 0))
					{ MCC_TRACE("br\n"); fails++; }
				ast_arena_free(sl);
			}
			mccjit_intent_release(&it);
		} else { MCC_TRACE("br\n");
			printf("mccjit-selftest-sliceoracle: %s deserialize failed\n", fn);
			fails++;
		}
		mcc_exit_state(js);
		mcc_delete(js);
	}
	mcc_free(blob);
	mcc_delete(s1);
	return fails;
}

int mccjit_selftest_sliceoracle(void) { MCC_TRACE("enter\n");
	int fails = 0;
	AstArena *sc1, *sc2, *sc3;

	printf("mccjit-selftest-sliceoracle: begin (C1b slice equiv + C4b certifiable)\n");

	fails += mccjit_certify_one("int f(int x){return x*2+1;}", "f", 1, 1);
	fails += mccjit_certify_one("int r(int *p, int x){return *p + x;}", "r", 0, 0);
	fails += mccjit_certify_one("int mabs(int); int h(int x){return mabs(x)+1;}", "h",
															0, 0);

	sc1 = mccjit_extract_ret_slice("int c1(int x){return 41;}", "c1");
	sc3 = mccjit_extract_ret_slice("int c3(int x){return 40+1;}", "c3");
	sc2 = mccjit_extract_ret_slice("int c2(int x){return 42;}", "c2");
	if (!sc1 || !sc2 || !sc3) { MCC_TRACE("br\n");
		printf("mccjit-selftest-sliceoracle: constant-slice extract failed FAIL\n");
		fails++;
	} else { MCC_TRACE("br\n");
		AstLocal r1 = ast_root(sc1), r2 = ast_root(sc2), r3 = ast_root(sc3);
		int self_eq = ast_slice_equiv(sc1, r1, sc1, r1);
		int sem_eq = ast_slice_equiv(sc1, r1, sc3, r3);
		int diff_eq = ast_slice_equiv(sc1, r1, sc2, r2);
		int ok = (self_eq == 1 && sem_eq == 1 && diff_eq == 0);
		printf("mccjit-selftest-sliceoracle: const equiv self=%d semantic(41==40+1)=%d "
					 "diff(41!=42)=%d (want 1,1,0) %s\n",
					 self_eq, sem_eq, diff_eq, ok ? "OK" : "FAIL");
		if (!ok)
			{ MCC_TRACE("br\n"); fails++; }
	}
	ast_arena_free(sc1);
	ast_arena_free(sc2);
	ast_arena_free(sc3);

	printf("mccjit-selftest-sliceoracle: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static int mccjit_livein_one(const char *src, const char *fn, int want_n,
														 int want_cert) { MCC_TRACE("enter\n");
	unsigned char *blob;
	size_t blen;
	MCCState *s1, *js;
	MccjitIntent it;
	int fails = 0;
	blob = mccjit_stash_one(src, fn, 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-slicekernel: %s stash failed\n", fn);
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return 1;
	}
	js = mcc_new();
	if (js) { MCC_TRACE("br\n");
		js->optimize = 0;
		js->nostdlib = 1;
		mcc_set_output_type(js, MCC_OUTPUT_MEMORY);
		mcc_enter_state(js);
		mccpp_new(js);
		mccgen_init(js);
		anon_sym = SYM_FIRST_ANOM;
		funcname = "";
		func_ind = -1;
		if (mccjit_intent_deserialize(blob, blen, &it) == 0) { MCC_TRACE("br\n");
			AstLocal ret = mccjit_ret_expr(it.arena);
			int32_t offs[16];
			int n = (ret != AST_NONE) ? ast_slice_live_ins(it.arena, ret, offs, 16) : -2;
			int cert = (ret != AST_NONE) ? ast_slice_certifiable(it.arena, ret) : -1;
			int ok = (n == want_n) && (cert == want_cert);
			printf("mccjit-selftest-slicekernel: %-3s live-ins=%d(want %d) "
						 "certifiable=%d(want %d) %s\n",
						 fn, n, want_n, cert, want_cert, ok ? "OK" : "FAIL");
			if (!ok)
				{ MCC_TRACE("br\n"); fails++; }
			mccjit_intent_release(&it);
		} else { MCC_TRACE("br\n");
			printf("mccjit-selftest-slicekernel: %s deserialize failed\n", fn);
			fails++;
		}
		mcc_exit_state(js);
		mcc_delete(js);
	}
	mcc_free(blob);
	mcc_delete(s1);
	return fails;
}

static int mccjit_wrap_one(const char *src, const char *fn) { MCC_TRACE("enter\n");
	unsigned char *blob;
	size_t blen;
	MCCState *s1, *js;
	MccjitIntent it;
	int fails = 0;
	blob = mccjit_stash_one(src, fn, 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-slicekernel: %s wrap stash failed\n", fn);
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return 1;
	}
	js = mcc_new();
	if (js) { MCC_TRACE("br\n");
		js->optimize = 0;
		js->nostdlib = 1;
		mcc_set_output_type(js, MCC_OUTPUT_MEMORY);
		mcc_enter_state(js);
		mccpp_new(js);
		mccgen_init(js);
		anon_sym = SYM_FIRST_ANOM;
		funcname = "";
		func_ind = -1;
		if (mccjit_intent_deserialize(blob, blen, &it) == 0) { MCC_TRACE("br\n");
			AstLocal ret = mccjit_ret_expr(it.arena);
			AstArena *k = (ret != AST_NONE) ? ast_slice_wrap_kernel(it.arena, ret) : NULL;
			if (!k) { MCC_TRACE("br\n");
				printf("mccjit-selftest-slicekernel: %s wrap failed\n", fn);
				fails++;
			} else { MCC_TRACE("br\n");
				char vmsg[128];
				AstLocal kbb = ast_root(k);
				AstLocal kret = (kbb != AST_NONE) ? ast_first_child(k, kbb) : AST_NONE;
				AstLocal kexpr = (kret != AST_NONE) ? ast_first_child(k, kret) : AST_NONE;
				int32_t o1[16], o2[16];
				int shape = (kbb != AST_NONE && ast_kind(k, kbb) == AST_BasicBlock &&
										 kret != AST_NONE && ast_kind(k, kret) == AST_Return &&
										 kexpr != AST_NONE);
				int valid = ast_validate(k, vmsg, sizeof vmsg) == 0;
				int cert = shape ? ast_slice_certifiable(k, kexpr) : -1;
				int eq = shape ? ast_slice_equiv(it.arena, ret, k, kexpr) : 0;
				int nk = shape ? ast_slice_live_ins(k, kexpr, o1, 16) : -2;
				int no = ast_slice_live_ins(it.arena, ret, o2, 16);
				int ok = shape && valid && cert == 1 && eq == 1 && nk == no;
				printf("mccjit-selftest-slicekernel: %-3s wrap shape=%d valid=%d cert=%d "
							 "equiv=%d live-ins=%d(orig %d) %s\n",
							 fn, shape, valid, cert, eq, nk, no, ok ? "OK" : "FAIL");
				if (!ok)
					{ MCC_TRACE("br\n"); fails++; }
				ast_arena_free(k);
			}
			mccjit_intent_release(&it);
		} else { MCC_TRACE("br\n");
			printf("mccjit-selftest-slicekernel: %s wrap deserialize failed\n", fn);
			fails++;
		}
		mcc_exit_state(js);
		mcc_delete(js);
	}
	mcc_free(blob);
	mcc_delete(s1);
	return fails;
}

int mccjit_selftest_slicekernel(void) { MCC_TRACE("enter\n");
	int fails = 0;

	printf("mccjit-selftest-slicekernel: begin (B2b live-in signature + D3a kernel wrap)\n");
	fails += mccjit_livein_one("int f(int x){return x*2+1;}", "f", 1, 1);
	fails += mccjit_livein_one("int c(int x){return 41;}", "c", 0, 1);
	fails += mccjit_livein_one("int g(int a, int b){return a*b+a;}", "g", 2, 1);
	fails += mccjit_livein_one("int t(int a, int b, int c){return a*b+c;}", "t", 3, 1);
	fails += mccjit_livein_one("int r(int *p, int x){return *p + x;}", "r", 2, 0);

	fails += mccjit_wrap_one("int f(int x){return x*2+1;}", "f");
	fails += mccjit_wrap_one("int g(int a, int b){return a*b+a;}", "g");
	fails += mccjit_wrap_one("int t(int a, int b, int c){return (a+b)*c-1;}", "t");

	printf("mccjit-selftest-slicekernel: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static void *mccjit_reemit_arena_blob(const void *buf, size_t len, AstArena *arena,
																			MCCState **keep) { MCC_TRACE("enter\n");
	MccjitIntent it;
	MCCState *js;
	Sym *sym;
	void *entry = NULL;
	js = mcc_new();
	if (!js)
		{ MCC_TRACE("br\n"); return NULL; }
	js->optimize = 0;
	js->nostdlib = 1;
	mcc_set_output_type(js, MCC_OUTPUT_MEMORY);
	mcc_enter_state(js);
	mccpp_new(js);
	mccgen_init(js);
	anon_sym = SYM_FIRST_ANOM;
	funcname = "";
	func_ind = -1;
	if (mccjit_intent_deserialize(buf, len, &it) != 0) { MCC_TRACE("br\n");
		mcc_exit_state(js);
		mcc_delete(js);
		return NULL;
	}
	if (it.has_external)
		{ MCC_TRACE("br\n"); js->nostdlib = 0; }
	sym = mccjit_rebuild_sym(&it);
	mccjit_internal_compile = 1;
	if (sym) { MCC_TRACE("br\n");
		ast_fconst_reuse_disable(1);
		ast_reemit_extern(sym, arena ? arena : it.arena);
		ast_fconst_reuse_disable(0);
	}
	mcc_exit_state(js);
	if (sym && mcc_relocate(js) == 0)
		{ MCC_TRACE("br\n"); entry = mcc_get_symbol(js, it.fn_name); }
	mccjit_internal_compile = 0;
	mccjit_intent_release(&it);
	if (entry)
		{ MCC_TRACE("br\n"); *keep = js; }
	else
		{ MCC_TRACE("br\n"); mcc_delete(js); }
	return entry;
}

static AstArena *mccjit_kernel_from_blob(const void *buf, size_t len) { MCC_TRACE("enter\n");
	MccjitIntent it;
	MCCState *js;
	AstArena *k = NULL;
	js = mcc_new();
	if (!js)
		{ MCC_TRACE("br\n"); return NULL; }
	js->optimize = 0;
	js->nostdlib = 1;
	mcc_set_output_type(js, MCC_OUTPUT_MEMORY);
	mcc_enter_state(js);
	mccpp_new(js);
	mccgen_init(js);
	anon_sym = SYM_FIRST_ANOM;
	funcname = "";
	func_ind = -1;
	if (mccjit_intent_deserialize(buf, len, &it) == 0) { MCC_TRACE("br\n");
		AstLocal ret = mccjit_ret_expr(it.arena);
		if (ret != AST_NONE && ast_slice_certifiable(it.arena, ret))
			{ MCC_TRACE("br\n"); k = ast_slice_wrap_kernel(it.arena, ret); }
		mccjit_intent_release(&it);
	}
	mcc_exit_state(js);
	mcc_delete(js);
	return k;
}

static int mccjit_reemit_one(const char *src, const char *fn, int arity,
														 const int *a0, const int *a1, const int *a2,
														 int nsamp) { MCC_TRACE("enter\n");
	unsigned char *blob;
	size_t blen;
	MCCState *s1, *ko = NULL, *kk = NULL;
	void *orig, *kern;
	int fails = 0, i, mism = 0;
	AstArena *k;
	blob = mccjit_stash_one(src, fn, 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-slicereemit: %s stash failed\n", fn);
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return 1;
	}
	k = mccjit_kernel_from_blob(blob, blen);
	orig = mccjit_reemit_arena_blob(blob, blen, NULL, &ko);
	kern = k ? mccjit_reemit_arena_blob(blob, blen, k, &kk) : NULL;
	if (!k || !orig || !kern) { MCC_TRACE("br\n");
		printf("mccjit-selftest-slicereemit: %-3s reemit failed (k=%p orig=%p kern=%p) FAIL\n",
					 fn, (void *)k, orig, kern);
		fails++;
	} else { MCC_TRACE("br\n");
		for (i = 0; i < nsamp; i++) { MCC_TRACE("br\n");
			int ro = 0, rk = 0;
			if (arity == 1) { MCC_TRACE("br\n");
				ro = ((int (*)(int))orig)(a0[i]);
				rk = ((int (*)(int))kern)(a0[i]);
			} else if (arity == 2) { MCC_TRACE("br\n");
				ro = ((int (*)(int, int))orig)(a0[i], a1[i]);
				rk = ((int (*)(int, int))kern)(a0[i], a1[i]);
			} else { MCC_TRACE("br\n");
				ro = ((int (*)(int, int, int))orig)(a0[i], a1[i], a2[i]);
				rk = ((int (*)(int, int, int))kern)(a0[i], a1[i], a2[i]);
			}
			if (ro != rk)
				{ MCC_TRACE("br\n"); mism++; }
		}
		printf("mccjit-selftest-slicereemit: %-3s reemitted+executed kernel, %d/%d "
					 "samples match origin %s\n",
					 fn, nsamp - mism, nsamp, mism ? "FAIL" : "OK");
		if (mism)
			{ MCC_TRACE("br\n"); fails++; }
	}
	ast_arena_free(k);
	if (ko)
		{ MCC_TRACE("br\n"); mcc_delete(ko); }
	if (kk)
		{ MCC_TRACE("br\n"); mcc_delete(kk); }
	mcc_free(blob);
	mcc_delete(s1);
	return fails;
}

int mccjit_selftest_slicereemit(void) { MCC_TRACE("enter\n");
	static const int s0[6] = {0, 1, -1, 5, 12, -7};
	static const int s1v[6] = {3, 2, 9, -4, 6, 1};
	static const int s2v[6] = {1, -2, 4, 7, -1, 3};
	int fails = 0;

	printf("mccjit-selftest-slicereemit: begin (D3a reemit slice kernel + execute)\n");
	fails += mccjit_reemit_one("int f(int x){return x*2+1;}", "f", 1, s0, NULL, NULL, 6);
	fails += mccjit_reemit_one("int g(int a, int b){return a*b+a;}", "g", 2, s0, s1v,
														 NULL, 6);
	fails += mccjit_reemit_one("int t(int a, int b, int c){return (a+b)*c-1;}", "t", 3,
														 s0, s1v, s2v, 6);

	printf("mccjit-selftest-slicereemit: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static void *mccjit_reemit_kernel_of(const char *src, const char *fn,
																		 MCCState **stash_state, MCCState **keep,
																		 AstArena **kernel) { MCC_TRACE("enter\n");
	unsigned char *blob;
	size_t blen;
	void *ptr;
	blob = mccjit_stash_one(src, fn, 1, &blen, stash_state);
	if (!*stash_state || !blob) { MCC_TRACE("br\n");
		mcc_free(blob);
		return NULL;
	}
	*kernel = mccjit_kernel_from_blob(blob, blen);
	ptr = *kernel ? mccjit_reemit_arena_blob(blob, blen, *kernel, keep) : NULL;
	mcc_free(blob);
	return ptr;
}

int mccjit_selftest_sliceinstall(void) { MCC_TRACE("enter\n");
	MCCState *sf = NULL, *sf2 = NULL, *kef = NULL, *kef2 = NULL;
	AstArena *akf = NULL, *akf2 = NULL;
	void *kf, *kf2;
	int fails = 0, i, installed = 0;

	printf("mccjit-selftest-sliceinstall: begin (F2b slice hot-patch install + F3c "
				 "ranking)\n");
	kf = mccjit_reemit_kernel_of("int f(int x){return x*2+1;}", "f", &sf, &kef, &akf);
	kf2 = mccjit_reemit_kernel_of("int f2(int x){return x*3+7;}", "f2", &sf2, &kef2,
																&akf2);
	if (!kf || !kf2) { MCC_TRACE("br\n");
		printf("mccjit-selftest-sliceinstall: kernel reemit failed (kf=%p kf2=%p) FAIL\n",
					 kf, kf2);
		fails++;
	} else { MCC_TRACE("br\n");
		for (i = 0; i < MCCJIT_PATCH_NREG; i++) { MCC_TRACE("br\n");
			const MccjitPatchStrategy *s = &mccjit_patch_reg[i];
			void *h = NULL, *entry;
			int r1, r2, ok;
			if (!mccjit_patch_benchmarkable(s)) { MCC_TRACE("br\n");
				printf("mccjit-selftest-sliceinstall: %-14s unavailable SKIP\n", s->name);
				continue;
			}
			entry = s->make(kf, &h);
			if (!entry) { MCC_TRACE("br\n");
				printf("mccjit-selftest-sliceinstall: %-14s make failed FAIL\n", s->name);
				fails++;
				continue;
			}
			installed++;
			r1 = s->call_i(entry, 5);
			s->swap(h, kf2);
			r2 = s->call_i(entry, 5);
			ok = (r1 == 11 && r2 == 22);
			printf("mccjit-selftest-sliceinstall: %-14s install(f)=%d swap(f2)=%d "
						 "(want 11,22) %s\n",
						 s->name, r1, r2, ok ? "OK" : "FAIL");
			if (!ok)
				{ MCC_TRACE("br\n"); fails++; }
			if (s->dispose)
				{ MCC_TRACE("br\n"); s->dispose(entry); }
		}
		{
			int order[MCCJIT_PATCH_NREG];
			double ns[MCCJIT_PATCH_NREG];
			int nr = mccjit_patch_bench_rank(kf, order, ns, MCCJIT_PATCH_NREG);
			if (nr > 0)
				{ MCC_TRACE("br\n"); printf("mccjit-selftest-sliceinstall: F3c ranked "
																		"install mechanism = %s (%.2f ns/call)\n",
																		mccjit_patch_reg[order[0]].name, ns[0]); }
		}
		printf("mccjit-selftest-sliceinstall: %d mechanism(s) installed a slice kernel\n",
					 installed);
	}
	ast_arena_free(akf);
	ast_arena_free(akf2);
	if (kef)
		{ MCC_TRACE("br\n"); mcc_delete(kef); }
	if (kef2)
		{ MCC_TRACE("br\n"); mcc_delete(kef2); }
	if (sf)
		{ MCC_TRACE("br\n"); mcc_delete(sf); }
	if (sf2)
		{ MCC_TRACE("br\n"); mcc_delete(sf2); }
	printf("mccjit-selftest-sliceinstall: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static int mccjit_search_one(const char *src, const char *fn, int budget,
														 int want_n) { MCC_TRACE("enter\n");
	unsigned char *blob;
	size_t blen;
	MCCState *s1, *js;
	MccjitIntent it;
	int fails = 0;
	blob = mccjit_stash_one(src, fn, 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-slicesearch: %s stash failed\n", fn);
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return 1;
	}
	js = mcc_new();
	if (js) { MCC_TRACE("br\n");
		js->optimize = 0;
		js->nostdlib = 1;
		mcc_set_output_type(js, MCC_OUTPUT_MEMORY);
		mcc_enter_state(js);
		mccpp_new(js);
		mccgen_init(js);
		anon_sym = SYM_FIRST_ANOM;
		funcname = "";
		func_ind = -1;
		if (mccjit_intent_deserialize(blob, blen, &it) == 0) { MCC_TRACE("br\n");
			AstLocal ret = mccjit_ret_expr(it.arena);
			AstLocal out[16];
			int n = (ret != AST_NONE) ? ast_slice_search(it.arena, ret, budget, out, 16)
															 : -1;
			int ok = (n == want_n);
			printf("mccjit-selftest-slicesearch: %-3s budget=%d -> %d kernel(s)"
						 "(want %d) %s\n",
						 fn, budget, n, want_n, ok ? "OK" : "FAIL");
			if (!ok)
				{ MCC_TRACE("br\n"); fails++; }
			mccjit_intent_release(&it);
		} else { MCC_TRACE("br\n");
			printf("mccjit-selftest-slicesearch: %s deserialize failed\n", fn);
			fails++;
		}
		mcc_exit_state(js);
		mcc_delete(js);
	}
	mcc_free(blob);
	mcc_delete(s1);
	return fails;
}

int mccjit_selftest_slicesearch(void) { MCC_TRACE("enter\n");
	int fails = 0;

	printf("mccjit-selftest-slicesearch: begin (H1b perm x comb slice-region search)\n");
	fails += mccjit_search_one("int q(int *p, int a, int b){return *p + a*2 + b*3;}",
														 "q", 2, 2);
	fails += mccjit_search_one("int q(int *p, int a, int b){return *p + a*2 + b*3;}",
														 "q", 1, 1);
	fails += mccjit_search_one("int f(int x){return x*2+1;}", "f", 2, 1);
	fails += mccjit_search_one("int r(int *p, int x){return *p + x;}", "r", 2, 0);

	printf("mccjit-selftest-slicesearch: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_l4a(void) { MCC_TRACE("enter\n");
	MccjitCounterState st;
	void *stub;
	int fails = 0;
	int64_t inputs[6] = {5, 12, -3, 100, 7, 40};
	int i;
	int r_no_samples, r_promote_fast, r_keep_slow;

	printf("mccjit-selftest-l4a: begin (K5 scorer over J6A-captured live-ins)\n");
	setenv("MCC_JIT_BENCH_ITERS", "3000", 1);
	setenv("MCC_JIT_BENCH_MARGIN_PCT", "10", 1);

	memset(&st, 0, sizeof st);
	pthread_mutex_init(&st.lock, NULL);
	r_no_samples = mccjit_promote_by_profile(
			(void *)mccjit_bench_fast_fn, (void *)mccjit_bench_slow_fn, &st, 1, 1);
	printf("mccjit-selftest-l4a: no captured samples -> promote=%d (expect 1, "
				 "allow) %s\n",
				 r_no_samples, r_no_samples == 1 ? "OK" : "FAIL");
	if (r_no_samples != 1)
		{ MCC_TRACE("br\n"); fails++; }
	pthread_mutex_destroy(&st.lock);

	memset(&st, 0, sizeof st);
	st.baseline = (void *)mccjit_profile_id1;
	st.threshold = 1L << 30;
	pthread_mutex_init(&st.lock, NULL);
	stub = mccjit_make_counter_stub(&st);
	if (!stub) { MCC_TRACE("br\n");
		printf("mccjit-selftest-l4a: counter stub NULL (non-x86_64?) SKIP\n");
		pthread_mutex_destroy(&st.lock);
		unsetenv("MCC_JIT_BENCH_ITERS");
		unsetenv("MCC_JIT_BENCH_MARGIN_PCT");
		return 0;
	}
	for (i = 0; i < 6; i++)
		{ MCC_TRACE("br\n"); ((long (*)(long))stub)((long)inputs[i]); }
	printf("mccjit-selftest-l4a: captured nsample=%d from the hot counter\n",
				 st.nsample);
	if (st.nsample <= 0)
		{ MCC_TRACE("br\n"); fails++; }

	r_promote_fast = mccjit_promote_by_profile(
			(void *)mccjit_bench_fast_fn, (void *)mccjit_bench_slow_fn, &st, 1, 1);
	r_keep_slow = mccjit_promote_by_profile(
			(void *)mccjit_bench_slow_fn, (void *)mccjit_bench_fast_fn, &st, 1, 1);
	unsetenv("MCC_JIT_BENCH_ITERS");
	unsetenv("MCC_JIT_BENCH_MARGIN_PCT");

	printf("mccjit-selftest-l4a: faster candidate over captured live-ins "
				 "promote=%d (expect 1) %s\n",
				 r_promote_fast, r_promote_fast == 1 ? "OK" : "FAIL");
	if (r_promote_fast != 1)
		{ MCC_TRACE("br\n"); fails++; }
	printf("mccjit-selftest-l4a: slower candidate over captured live-ins "
				 "promote=%d (expect 0) %s\n",
				 r_keep_slow, r_keep_slow == 0 ? "OK" : "FAIL");
	if (r_keep_slow != 0)
		{ MCC_TRACE("br\n"); fails++; }
	pthread_mutex_destroy(&st.lock);

	printf("mccjit-selftest-l4a: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

int mccjit_selftest_benchwire(void) { MCC_TRACE("enter\n");
	MccjitCounterState st, empty;
	void *fast = (void *)mccjit_bench_fast_fn;
	void *slow = (void *)mccjit_bench_slow_fn;
	int fails = 0, i;
	int a_off, a_faster, a_slower, a_unrouted, a_allfp, a_nosamp;

	printf("mccjit-selftest-benchwire: begin (MCC_JIT_BENCH admit gate)\n");
	setenv("MCC_JIT_BENCH_ITERS", "3000", 1);
	setenv("MCC_JIT_BENCH_MARGIN_PCT", "10", 1);

	memset(&st, 0, sizeof st);
	pthread_mutex_init(&st.lock, NULL);
	st.nsample = 4;
	for (i = 0; i < st.nsample; i++)
		{ MCC_TRACE("br\n"); st.sample[i][0] = 11 + i; }
	memset(&empty, 0, sizeof empty);
	pthread_mutex_init(&empty.lock, NULL);

	unsetenv("MCC_JIT_BENCH");
	a_off = mccjit_bench_admit(slow, fast, &st, 1, 1, 0, 1);
	printf("mccjit-selftest-benchwire: gate off, slower candidate -> admit=%d "
				 "(expect 1) %s\n",
				 a_off, a_off == 1 ? "OK" : "FAIL");
	if (a_off != 1)
		{ MCC_TRACE("br\n"); fails++; }

	setenv("MCC_JIT_BENCH", "1", 1);
	a_faster = mccjit_bench_admit(fast, slow, &st, 1, 1, 0, 1);
	a_slower = mccjit_bench_admit(slow, fast, &st, 1, 1, 0, 1);
	a_unrouted = mccjit_bench_admit(slow, fast, &st, 1, 1, 0, 0);
	a_allfp = mccjit_bench_admit(slow, fast, &st, 1, 1, 1, 1);
	a_nosamp = mccjit_bench_admit(slow, fast, &empty, 1, 1, 0, 1);
	unsetenv("MCC_JIT_BENCH");
	unsetenv("MCC_JIT_BENCH_ITERS");
	unsetenv("MCC_JIT_BENCH_MARGIN_PCT");

	printf("mccjit-selftest-benchwire: gate on, faster candidate -> admit=%d "
				 "(expect 1) %s\n",
				 a_faster, a_faster == 1 ? "OK" : "FAIL");
	if (a_faster != 1)
		{ MCC_TRACE("br\n"); fails++; }
	printf("mccjit-selftest-benchwire: gate on, slower candidate -> admit=%d "
				 "(expect 0) %s\n",
				 a_slower, a_slower == 0 ? "OK" : "FAIL");
	if (a_slower != 0)
		{ MCC_TRACE("br\n"); fails++; }
	printf("mccjit-selftest-benchwire: gate on, unrouted -> admit=%d (expect 1) "
				 "%s\n",
				 a_unrouted, a_unrouted == 1 ? "OK" : "FAIL");
	if (a_unrouted != 1)
		{ MCC_TRACE("br\n"); fails++; }
	printf("mccjit-selftest-benchwire: gate on, all-fp -> admit=%d (expect 1) "
				 "%s\n",
				 a_allfp, a_allfp == 1 ? "OK" : "FAIL");
	if (a_allfp != 1)
		{ MCC_TRACE("br\n"); fails++; }
	printf("mccjit-selftest-benchwire: gate on, no samples -> admit=%d (expect "
				 "1) %s\n",
				 a_nosamp, a_nosamp == 1 ? "OK" : "FAIL");
	if (a_nosamp != 1)
		{ MCC_TRACE("br\n"); fails++; }

	pthread_mutex_destroy(&st.lock);
	pthread_mutex_destroy(&empty.lock);
	printf("mccjit-selftest-benchwire: %s (%d failure%s)\n",
				 fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

void mcc_jit_publish(void **slot, void *variant) { MCC_TRACE("enter\n");
	if (!slot)
		{ MCC_TRACE("br\n"); return; }
	__atomic_store_n(slot, variant, __ATOMIC_RELEASE);
}

MCCJIT_LOCAL int mccjit_embed_active(void) { MCC_TRACE("enter\n");
	return 1;
}

int mccjit_embed_manifest(MCCState *s) { MCC_TRACE("enter\n");
	if (!s || !s->verbose)
		{ MCC_TRACE("br\n"); return mccjit_embed_active(); }
	printf("embed-jit manifest: functions=%s max-duration=%us%s\n",
				 s->jit_functions ? s->jit_functions : "main", s->jit_max_duration,
				 s->jit_max_duration == 0 ? " (unlimited)" : "");
	printf("embed-jit: Tier-B engine slice linked; graduated-records=%d salt=%016llx\n",
				 JIT_GRADUATED_COUNT, (unsigned long long)mccjit_salt_witness());
	return mccjit_embed_active();
}

#else
typedef int mccjit_embed_translation_unit_not_empty;
#endif /* MCC_EMBED_JIT */
