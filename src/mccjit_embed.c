#ifdef MCC_EMBED_JIT

#include "mcc.h"
#include "mccast.h"
#include "mccgate.h"
#include "algorithms/jit.h"
#include "mccjit_internal.h"
#include "mccinv.h"
#include "mccstats.h"
#include "mcctask.h"

#if MCC_HOST_WIN32
#include "mccjit_win32.h"
#else
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define MCCJIT_X64 1
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
#define MCCJIT_ARM64 1
#endif
#if (defined(__i386__) || defined(_M_IX86)) && !defined(MCCJIT_X64)
#define MCCJIT_I386 1
#endif

#if defined(MCCJIT_X64) || defined(MCCJIT_ARM64) || defined(MCCJIT_I386)
#define MCCJIT_HAVE_STUB_TAIL 1
#else
#define MCCJIT_HAVE_STUB_TAIL 0
#endif

#ifndef MCC_JIT_DEFAULT
#define MCC_JIT_DEFAULT 1
#endif

#if MCC_HOST_WIN32
#ifndef EXCEPTION_ACCESS_VIOLATION
#define EXCEPTION_ACCESS_VIOLATION 0xC0000005L
#endif

#define MCCJIT_DIAG_RING 32u
typedef struct MccjitDiagPub {
	void *slot;
	void *entry;
} MccjitDiagPub;
typedef struct MccjitDiagBoot {
	void *variant;
	void *baseline;
	void *entry;
	const char *mode;
	unsigned nargs;
	int ret_wide;
	uint32_t param_t[MCCJIT_KGC_MAXARG];
} MccjitDiagBoot;
static MccjitDiagPub mccjit_diag_pubs[MCCJIT_DIAG_RING];
static MccjitDiagBoot mccjit_diag_boots[MCCJIT_DIAG_RING];
static unsigned long long mccjit_diag_pub_n;
static unsigned long long mccjit_diag_boot_n;
static pthread_once_t mccjit_diag_once = PTHREAD_ONCE_INIT;
static int mccjit_diag_fired;

static int mccjit_diag_enabled(void) { MCC_TRACE("enter\n");
	static int v = -1;
	if (v < 0) { MCC_TRACE("br\n");
		const char *e = MCC_DEV_ENV("MCC_JIT_CRASH_DIAG");
		v = (e && e[0] && e[0] != '0') ? 1 : 0;
	}
	return v;
}

static int mccjit_type_wide(int t);
static int mccjit_type_fp(int t);

static void mccjit_diag_hexdump(const char *label, void *addr, size_t want) { MCC_TRACE("enter\n");
	MEMORY_BASIC_INFORMATION mbi;
	if (addr && VirtualQuery(addr, &mbi, sizeof mbi) == sizeof mbi &&
			mbi.State == MEM_COMMIT &&
			!(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) { MCC_TRACE("br\n");
		unsigned char *b = (unsigned char *)addr;
		size_t avail = (size_t)((char *)mbi.BaseAddress + mbi.RegionSize - (char *)addr);
		size_t k, m = avail < want ? avail : want;
		fprintf(stderr, "  %s @%p:", label, addr);
		for (k = 0; k < m; k++)
			fprintf(stderr, " %02x", b[k]);
		fprintf(stderr, "\n");
	} else { MCC_TRACE("br\n");
		fprintf(stderr, "  %s @%p: <not readable>\n", label, addr);
	}
}

static void mccjit_diag_dump(void *pc) { MCC_TRACE("enter\n");
	unsigned long long total, n, i;
	void *best_entry = NULL, *best_slot = NULL;
	long long bestd = -1;
	total = __atomic_load_n(&mccjit_diag_pub_n, __ATOMIC_ACQUIRE);
	n = total < MCCJIT_DIAG_RING ? total : MCCJIT_DIAG_RING;
	fprintf(stderr, "  published entries (most recent first, %llu total):\n", total);
	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		MccjitDiagPub p = mccjit_diag_pubs[(total - 1 - i) & (MCCJIT_DIAG_RING - 1)];
		long long d = (long long)((char *)pc - (char *)p.entry);
		fprintf(stderr, "    pub[-%llu] slot=%p entry=%p  pc-entry=%lld\n", i,
						p.slot, p.entry, d);
		if (p.entry && d >= 0 && (bestd < 0 || d < bestd)) { MCC_TRACE("br\n");
			bestd = d;
			best_entry = p.entry;
			best_slot = p.slot;
		}
	}
	if (best_entry) { MCC_TRACE("br\n");
		fprintf(stderr,
						"  nearest published entry <= pc: %p  (pc = entry + %lld)  slot=%p\n",
						best_entry, bestd, best_slot);
		mccjit_diag_hexdump("bytes@nearest-entry", best_entry, 48);
	} else { MCC_TRACE("br\n");
		fprintf(stderr, "  no published entry lies at or below pc\n");
	}
	total = __atomic_load_n(&mccjit_diag_boot_n, __ATOMIC_ACQUIRE);
	n = total < MCCJIT_DIAG_RING ? total : MCCJIT_DIAG_RING;
	fprintf(stderr, "  boot swaps (most recent first, %llu total):\n", total);
	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		MccjitDiagBoot b =
				mccjit_diag_boots[(total - 1 - i) & (MCCJIT_DIAG_RING - 1)];
		unsigned k, na = b.nargs < MCCJIT_KGC_MAXARG ? b.nargs : MCCJIT_KGC_MAXARG;
		fprintf(stderr,
						"    boot[-%llu] mode=%s variant=%p baseline=%p entry=%p nargs=%u ret_wide=%d\n",
						i, b.mode ? b.mode : "?", b.variant, b.baseline, b.entry, b.nargs,
						b.ret_wide);
		fprintf(stderr, "      param_t:");
		for (k = 0; k < na; k++) { MCC_TRACE("br\n");
			int w = mccjit_type_wide((int)b.param_t[k]);
			fprintf(stderr, " [%u]=0x%x(%s%s)", k, b.param_t[k],
							w ? "wide" : "NARROW->movsxd-truncates",
							mccjit_type_fp((int)b.param_t[k]) ? ",fp" : "");
		}
		fprintf(stderr, "\n");
	}
}

static LONG CALLBACK mccjit_diag_veh(EXCEPTION_POINTERS *ep) { MCC_TRACE("enter\n");
	EXCEPTION_RECORD *er;
	CONTEXT *cx;
	void *pc;
	void *addr;
	const char *acc;
	MEMORY_BASIC_INFORMATION mbi;
	if (!ep || !ep->ExceptionRecord || !ep->ContextRecord)
		return EXCEPTION_CONTINUE_SEARCH;
	er = ep->ExceptionRecord;
	cx = ep->ContextRecord;
	if (er->ExceptionCode != (DWORD)EXCEPTION_ACCESS_VIOLATION)
		return EXCEPTION_CONTINUE_SEARCH;
	if (mccjit_diag_fired)
		return EXCEPTION_CONTINUE_SEARCH;
	mccjit_diag_fired = 1;
#if defined(MCCJIT_X64)
	pc = (void *)cx->Rip;
#elif defined(MCCJIT_ARM64)
	pc = (void *)cx->Pc;
#else
	pc = (void *)0;
#endif
	acc = er->NumberParameters >= 1
						? (er->ExceptionInformation[0] == 0		? "read"
							 : er->ExceptionInformation[0] == 1 ? "write"
							 : er->ExceptionInformation[0] == 8 ? "execute"
																									: "?")
						: "?";
	addr = er->NumberParameters >= 2 ? (void *)er->ExceptionInformation[1] : NULL;
	fprintf(stderr, "\n==== mccjit-diag: EXCEPTION_ACCESS_VIOLATION ====\n");
	fprintf(stderr, "  pc=%p  fault=%s addr=%p\n", pc, acc, addr);
	if (pc && VirtualQuery(pc, &mbi, sizeof mbi) == sizeof mbi) { MCC_TRACE("br\n");
		fprintf(stderr,
						"  pc region: base=%p size=%llu state=0x%lx protect=0x%lx type=0x%lx\n",
						mbi.BaseAddress, (unsigned long long)mbi.RegionSize,
						(unsigned long)mbi.State, (unsigned long)mbi.Protect,
						(unsigned long)mbi.Type);
	} else if (pc) { MCC_TRACE("br\n");
		fprintf(stderr, "  pc region: <VirtualQuery failed>\n");
	}
	mccjit_diag_hexdump("bytes@pc-16", (char *)pc - 16, 64);
#if defined(MCCJIT_X64)
	fprintf(stderr,
					"  regs: rax=%p rbx=%p rcx=%p rdx=%p rsi=%p rdi=%p rbp=%p rsp=%p\n"
					"        r8=%p r9=%p r10=%p r11=%p r12=%p r13=%p r14=%p r15=%p\n",
					(void *)cx->Rax, (void *)cx->Rbx, (void *)cx->Rcx, (void *)cx->Rdx,
					(void *)cx->Rsi, (void *)cx->Rdi, (void *)cx->Rbp, (void *)cx->Rsp,
					(void *)cx->R8, (void *)cx->R9, (void *)cx->R10, (void *)cx->R11,
					(void *)cx->R12, (void *)cx->R13, (void *)cx->R14, (void *)cx->R15);
	{
		void **sp = (void **)cx->Rsp;
		MEMORY_BASIC_INFORMATION smbi;
		if (sp && VirtualQuery(sp, &smbi, sizeof smbi) == sizeof smbi &&
				smbi.State == MEM_COMMIT &&
				!(smbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) { MCC_TRACE("br\n");
			size_t avail =
					(size_t)((char *)smbi.BaseAddress + smbi.RegionSize - (char *)sp);
			size_t j, m = avail / sizeof(void *);
			if (m > 8)
				m = 8;
			fprintf(stderr, "  stack@rsp:");
			for (j = 0; j < m; j++)
				fprintf(stderr, " [%llu]=%p", (unsigned long long)j, sp[j]);
			fprintf(stderr, "\n");
		}
	}
#elif defined(MCCJIT_ARM64)
	{
		int r;
		void **sp;
		MEMORY_BASIC_INFORMATION smbi;
		for (r = 0; r < 31; r++) { MCC_TRACE("br\n");
			if (r % 4 == 0)
				fprintf(stderr, "%s  x%d-%d:", r ? "\n" : "", r, r + 3 < 31 ? r + 3 : 30);
			fprintf(stderr, " %p", (void *)cx->X[r]);
		}
		fprintf(stderr, "\n  fp=%p lr=%p sp=%p pc=%p\n", (void *)cx->Fp,
						(void *)cx->Lr, (void *)cx->Sp, (void *)cx->Pc);
		sp = (void **)cx->Sp;
		if (sp && VirtualQuery(sp, &smbi, sizeof smbi) == sizeof smbi &&
				smbi.State == MEM_COMMIT &&
				!(smbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) { MCC_TRACE("br\n");
			size_t avail =
					(size_t)((char *)smbi.BaseAddress + smbi.RegionSize - (char *)sp);
			size_t j, m = avail / sizeof(void *);
			if (m > 8)
				m = 8;
			fprintf(stderr, "  stack@sp:");
			for (j = 0; j < m; j++)
				fprintf(stderr, " [%llu]=%p", (unsigned long long)j, sp[j]);
			fprintf(stderr, "\n");
		}
	}
#endif
	mccjit_diag_dump(pc);
	fprintf(stderr,
					"==== mccjit-diag: continuing to default handler (0xC0000005) ====\n");
	fflush(stderr);
	return EXCEPTION_CONTINUE_SEARCH;
}

static void mccjit_diag_install_(void) { MCC_TRACE("enter\n");
	AddVectoredExceptionHandler(1u, (PVECTORED_EXCEPTION_HANDLER)&mccjit_diag_veh);
	fprintf(stderr, "mccjit-diag: crash handler armed (MCC_JIT_CRASH_DIAG, MCC_DEV build)\n");
	fflush(stderr);
}

static void mccjit_diag_arm(void) { MCC_TRACE("enter\n");
	if (!mccjit_diag_enabled())
		return;
	pthread_once(&mccjit_diag_once, mccjit_diag_install_);
}

static void mccjit_diag_note_pub(void *slot, void *entry) { MCC_TRACE("enter\n");
	unsigned long long idx;
	if (!mccjit_diag_enabled())
		return;
	mccjit_diag_arm();
	idx = __atomic_add_fetch(&mccjit_diag_pub_n, 1, __ATOMIC_RELEASE) - 1;
	mccjit_diag_pubs[idx & (MCCJIT_DIAG_RING - 1)].slot = slot;
	mccjit_diag_pubs[idx & (MCCJIT_DIAG_RING - 1)].entry = entry;
}

static void mccjit_diag_note_boot(void *variant, void *baseline, void *entry,
																	unsigned nargs, const char *mode,
																	const uint32_t *param_t, int ret_wide) { MCC_TRACE("enter\n");
	unsigned long long idx;
	unsigned k;
	MccjitDiagBoot *slot;
	if (!mccjit_diag_enabled())
		return;
	mccjit_diag_arm();
	idx = __atomic_add_fetch(&mccjit_diag_boot_n, 1, __ATOMIC_RELEASE) - 1;
	slot = &mccjit_diag_boots[idx & (MCCJIT_DIAG_RING - 1)];
	slot->variant = variant;
	slot->baseline = baseline;
	slot->entry = entry;
	slot->nargs = nargs;
	slot->mode = mode;
	slot->ret_wide = ret_wide;
	for (k = 0; k < MCCJIT_KGC_MAXARG; k++)
		slot->param_t[k] = param_t ? param_t[k] : 0;
}
#define MCCJIT_DIAG_NOTE_PUB(s, e) mccjit_diag_note_pub((s), (e))
#define MCCJIT_DIAG_NOTE_BOOT(v, b, e, n, m, pt, rw)                            \
	mccjit_diag_note_boot((v), (b), (e), (n), (m), (pt), (rw))
#else
#define MCCJIT_DIAG_NOTE_PUB(s, e) ((void)0)
#define MCCJIT_DIAG_NOTE_BOOT(v, b, e, n, m, pt, rw) ((void)0)
#endif

#if defined(MCCJIT_I386)
static int mccjit_i386_stubs_enabled(void) { MCC_TRACE("enter\n");
	return mcc_env_flag("MCC_JIT_I386_STUBS", 1);
}

static int mccjit_stub_tail_active(void) { MCC_TRACE("enter\n");
	return mccjit_i386_stubs_enabled();
}
#endif

static void mccjit_perf_map_path(char *buf, size_t n) { MCC_TRACE("enter\n");
#if MCC_HOST_WIN32
	char dir[MAX_PATH];
	DWORD r = GetTempPathA((DWORD)sizeof dir, dir);
	if (r == 0 || r >= sizeof dir)
		{ MCC_TRACE("br\n"); dir[0] = '.'; dir[1] = '\\'; dir[2] = '\0'; }
	snprintf(buf, n, "%sperf-%lu.map", dir, (unsigned long)GetCurrentProcessId());
#else
	snprintf(buf, n, "/tmp/perf-%d.map", (int)getpid());
#endif
}

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
extern int mccjit_recompiling;
void ast_reemit_with_gates(Sym *sym, AstArena *ast, uint64_t gate_mask);
int ast_jit_fold_consts(AstArena *ast);
int ast_jit_search_vocab(uint64_t *out, int max);
int mccjit_ast_spec_fold(AstArena *ast, int off, int64_t val);
int mccjit_ast_blind_retype(AstArena *ast);
void mcc_jit_publish(void **slot, void *variant);
int mcc_jit_submit_ast(Sym *sym, AstArena *ast, uint64_t gate_mask, int flags);
void mcc_jit_export_local(MCCState *s1, const char *name);
void ast_slice_graduate_arena(const AstArena *ast, uint64_t gate_mask);
int ast_slice_enabled(void);

MCCJIT_LOCAL unsigned char *mccjit_last_blob;
MCCJIT_LOCAL size_t mccjit_last_len;
MCCJIT_LOCAL MCCState *mccjit_last_state;


MCCJIT_LOCAL int mccjit_last_purity;
MCCJIT_LOCAL int mccjit_last_purity_ne;

static int mccjit_purity_noescape_enabled(void) { MCC_TRACE("enter\n");
	static int v = -1;
	if (v < 0)
		{ MCC_TRACE("br\n"); v = mcc_env_on("MCC_JIT_PURITY_NOESCAPE"); }
	return v;
}
MCCJIT_LOCAL long mccjit_last_cost = -1;
MCCJIT_LOCAL long mccjit_variant_cost = -1;

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
	if (!mcc_env_on("MCC_JIT_PERF_MAP") || !addr || !name || !name[0] || !js ||
			!js->symtab)
		{ MCC_TRACE("br\n"); return; }
	si = find_elf_sym(js->symtab, name);
	if (si > 0)
		{ MCC_TRACE("br\n"); size = (size_t)((ElfSym *)js->symtab->data)[si].st_size; }
	if (!size)
		{ MCC_TRACE("br\n"); size = 16; }
	mccjit_perf_map_path(path, sizeof path);
	f = fopen(path, "a");
	if (!f)
		{ MCC_TRACE("br\n"); return; }
	fprintf(f, "%lx %lx %s\n", (unsigned long)(uintptr_t)addr,
					(unsigned long)size, name);
	fclose(f);
}

static int mccjit_internal_compile;
static uint64_t mccjit_recompile_gate_mask;
static int mccjit_recompile_use_gates;
static int mccjit_recompile_blind_retype;
static int mccjit_last_blind_proven;

int mccjit_boundary_hit;
static unsigned long mccjit_search_budget_baked_s;

typedef struct MccjitOverride {
	int64_t tokv;
	unsigned char *blob;
	size_t len;
	uint64_t gate_mask;
	int flags;
} MccjitOverride;
static MccjitOverride *mccjit_overrides;
static int mccjit_override_n, mccjit_override_cap;
static pthread_mutex_t mccjit_override_lock = PTHREAD_MUTEX_INITIALIZER;

static int mccjit_override_get(int64_t tokv, unsigned char **blob, size_t *len,
															uint64_t *mask) { MCC_TRACE("enter\n");
	int i, found = 0;
	pthread_mutex_lock(&mccjit_override_lock);
	for (i = 0; i < mccjit_override_n; i++)
		if (mccjit_overrides[i].tokv == tokv) { MCC_TRACE("br\n");
			*blob = mccjit_overrides[i].blob;
			*len = mccjit_overrides[i].len;
			*mask = mccjit_overrides[i].gate_mask;
			found = 1;
			break;
		}
	pthread_mutex_unlock(&mccjit_override_lock);
	return found;
}

static void mccjit_override_put(int64_t tokv, unsigned char *blob, size_t len,
																uint64_t gate_mask, int flags) { MCC_TRACE("enter\n");
	int i;
	pthread_mutex_lock(&mccjit_override_lock);
	for (i = 0; i < mccjit_override_n; i++)
		{ MCC_TRACE("br\n"); if (mccjit_overrides[i].tokv == tokv) { MCC_TRACE("br\n"); break; } }
	if (i == mccjit_override_n) { MCC_TRACE("br\n");
		if (mccjit_override_n == mccjit_override_cap) { MCC_TRACE("br\n");
			mccjit_override_cap = mccjit_override_cap ? mccjit_override_cap * 2 : 8;
			mccjit_overrides = mcc_realloc(mccjit_overrides,
																		 mccjit_override_cap * sizeof *mccjit_overrides);
		}
		mccjit_override_n++;
	} else { MCC_TRACE("br\n");
		mcc_free(mccjit_overrides[i].blob);
	}
	mccjit_overrides[i].tokv = tokv;
	mccjit_overrides[i].blob = blob;
	mccjit_overrides[i].len = len;
	mccjit_overrides[i].gate_mask = gate_mask;
	mccjit_overrides[i].flags = flags;
	pthread_mutex_unlock(&mccjit_override_lock);
}

int mcc_jit_submit_ast(Sym *sym, AstArena *ast, uint64_t gate_mask, int flags) { MCC_TRACE("enter\n");
	MccjitBuf b;
	if (!sym || !ast)
		{ MCC_TRACE("br\n"); return -1; }
	mccjit_buf_init(&b);
	if (mccjit_intent_serialize(ast, sym, &b, 0) != 0) { MCC_TRACE("br\n");
		mccjit_buf_free(&b);
		return -1;
	}
	mccjit_override_put(sym->v, b.data, b.len, gate_mask, flags);
	return 0;
}

static AstLocal mccjit_slice_ret_expr(const AstArena *a) { MCC_TRACE("enter\n");
	AstLocal n, nn = ast_count(a);
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) == AST_Return && ast_nchild(a, n) == 1) { MCC_TRACE("br\n");
			AstLocal e = ast_first_child(a, n);
			return ast_first_child(a, e) != AST_NONE ? e : AST_NONE;
		}
	}
	return AST_NONE;
}

static int mccjit_slice_hotpatch(AstArena *arena) { MCC_TRACE("enter\n");
	AstLocal site, opt_expr = AST_NONE, sites[64];
	AstArena *base_wrap = NULL, *opt_wrap = NULL, *opt_k = NULL;
	uint64_t ident;
	int nsites, i, keep, spliced = 0;
	int64_t base_cost, cand_cost;
	if (!ast_slice_enabled() || !mcc_env_on("MCC_AST_SLICE_SPLICE") || !arena)
		{ MCC_TRACE("br\n"); return 0; }
	site = mccjit_slice_ret_expr(arena);
	if (site == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	base_wrap = ast_slice_wrap_kernel(arena, site);
	opt_wrap = ast_slice_wrap_kernel(arena, site);
	if (!base_wrap || !opt_wrap)
		{ MCC_TRACE("br\n"); goto done; }
	ast_jit_fold_consts(opt_wrap);
	base_cost = ast_cost_score(base_wrap);
	cand_cost = ast_cost_score(opt_wrap);
	{
		const char *lv = getenv("MCC_JIT_SLICE_LANES");
		long lanes = lv && lv[0] ? strtol(lv, NULL, 10) : 0;
		cand_cost = ast_slice_width_cost(cand_cost, lanes);
	}
	keep = ast_slice_promote_static(base_cost, cand_cost);
	if (mcc_env_on("MCC_JIT_VERBOSE"))
		{ MCC_TRACE("br\n"); fprintf(stderr,
					"mccjit-slice-splice: site=%u base_cost=%lld cand_cost=%lld -> %s\n",
					(unsigned)site, (long long)base_cost, (long long)cand_cost,
					keep ? "KEEP" : "REJECT"); }
	if (!keep)
		{ MCC_TRACE("br\n"); goto done; }
	opt_expr = mccjit_slice_ret_expr(opt_wrap);
	if (opt_expr == AST_NONE)
		{ MCC_TRACE("br\n"); goto done; }
	opt_k = ast_slice_extract(opt_wrap, opt_expr);
	if (!opt_k)
		{ MCC_TRACE("br\n"); goto done; }
	ident = ast_slice_ident_hash(arena, site);
	nsites = ast_slice_locate(arena, ident, sites, 64);
	if (nsites <= 0)
		{ MCC_TRACE("br\n"); nsites = 1; sites[0] = site; }
	for (i = 0; i < nsites && i < 64; i++) { MCC_TRACE("br\n");
		int s = ast_slice_splice(arena, sites[i], opt_k, ast_root(opt_k));
		if (s > 0)
			{ MCC_TRACE("br\n"); spliced += s; }
	}
	if (mcc_env_on("MCC_JIT_VERBOSE"))
		{ MCC_TRACE("br\n"); fprintf(stderr,
					"mccjit-slice-splice: spliced %d node(s) across %d site(s)\n", spliced,
					nsites); }
done:
	ast_arena_free(base_wrap);
	ast_arena_free(opt_wrap);
	ast_arena_free(opt_k);
	return spliced;
}

static int mccjit_bind_name_eq(MCCState *js, const char *sym_name,
															 const char *bind_name) { MCC_TRACE("enter\n");
	if (js->leading_underscore && !strchr(bind_name, '@')) { MCC_TRACE("br\n");
		return sym_name[0] == '_' && !strcmp(sym_name + 1, bind_name);
	}
	return !strcmp(sym_name, bind_name);
}

static int mccjit_bind_apply(MCCState *js, const MccjitIntent *it) { MCC_TRACE("enter\n");
	ElfSym *es;
	Section *st = js->symtab;
	uint32_t bi;
	int bound = 0;
	if (!st || !st->link)
		{ MCC_TRACE("br\n"); return 0; }
	for_each_elem(st, 1, es, ElfSym) {
		const char *nm;
		if (es->st_shndx != SHN_UNDEF)
			{ MCC_TRACE("br\n"); continue; }
		nm = (const char *)st->link->data + es->st_name;
		for (bi = 0; bi < it->nbind; bi++) { MCC_TRACE("br\n");
			if (!mccjit_bind_name_eq(js, nm, it->bind_name[bi]))
				{ MCC_TRACE("br\n"); continue; }
			es->st_shndx = SHN_ABS;
			es->st_value = (addr_t)(uintptr_t)it->bind_addr[bi];
			bound++;
			if (mcc_env_on("MCC_JIT_VERBOSE"))
				{ MCC_TRACE("br\n"); fprintf(stderr, "mccjit-bind[%s]: %s -> %p\n",
										it->fn_name ? it->fn_name : "?", nm,
										(void *)(uintptr_t)it->bind_addr[bi]); }
			break;
		}
	}
	return bound;
}

static void *mccjit_recompile_common(const void *buf, size_t len, int do_spec,
																		 int param_index, int64_t const_val) { MCC_TRACE("enter\n");
	MccjitIntent it;
	MCCState *js;
	Sym *sym;
	Sym *sav_global, *sav_local;
	void *entry = NULL;
	uint64_t override_mask = 0;
	int have_override = 0;

	if (mcc_stats_mask)
		{ MCC_TRACE("br\n"); mcc_stats_jit_recompile(); }
	js = mcc_new();
	if (!js)
		{ MCC_TRACE("br\n"); return NULL; }
	mccjit_recompiling++;
	mcc_leakcheck_quiet = 1;
	js->optimize = 0;
	js->nostdlib = 1;
#if defined(MCCJIT_I386)
	js->nostdlib = 0;
#endif
	mcc_set_output_type(js, MCC_OUTPUT_MEMORY);

	mcc_enter_state(js);
	mccpp_new(js);
	mccgen_init(js);
	ast_configure(js);
	anon_sym = SYM_FIRST_ANOM;
	funcname = "";
	func_ind = -1;
	sav_global = global_stack;
	sav_local = local_stack;

	if (mccjit_intent_deserialize(buf, len, &it) != 0) { MCC_TRACE("br\n");
		mcc_exit_state(js);
		mcc_delete(js);
		mccjit_recompiling--;
		return NULL;
	}

	if (!mccjit_recompile_use_gates && mccjit_override_n) { MCC_TRACE("br\n");
		unsigned char *ov_blob = NULL;
		size_t ov_len = 0;
		uint64_t ov_mask = 0;
		if (mccjit_override_get(it.anchor_sym_v, &ov_blob, &ov_len, &ov_mask)) { MCC_TRACE("br\n");
			MccjitIntent ov_it;
			if (mccjit_intent_deserialize(ov_blob, ov_len, &ov_it) == 0) { MCC_TRACE("br\n");
				if (mcc_env_on("MCC_JIT_VERBOSE"))
					{ MCC_TRACE("br\n"); fprintf(stderr,
									"mccjit-override[%s]: using backend-submitted AST (%lu bytes)\n",
									ov_it.fn_name ? ov_it.fn_name : "?", (unsigned long)ov_len); }
				mccjit_intent_release(&it);
				it = ov_it;
				override_mask = ov_mask;
				have_override = 1;
			}
		}
	}

	if (it.has_external)
		{ MCC_TRACE("br\n"); js->nostdlib = 0; }

	mccjit_last_purity = ast_fn_purity(it.arena);
	mccjit_last_purity_ne = ast_fn_purity_noescape(it.arena);
	ast_hash_out_emit("jit:", it.fn_name ? it.fn_name : "?",
										ast_intention_hash(it.arena, AST_NONE));

	{
		uint32_t qi;
		int allfp, scalar_ok, allgp, ret_gp, ret_fp;
		uint32_t ngp = 0, nsse = 0;
		mccjit_last_nparam = it.nparam;
		mccjit_last_ret_wide = mccjit_type_wide((int)it.ret_type_t);
		allfp = (it.nparam >= 1 && it.nparam <= MCCJIT_KGC_MAXARG &&
						 ((int)it.ret_type_t & VT_BTYPE) == VT_DOUBLE);
		for (qi = 0; allfp && qi < it.nparam && qi < MCCJIT_KGC_MAXARG; qi++)
			{ MCC_TRACE("br\n"); if (((int)it.param_type_t[qi] & VT_BTYPE) != VT_DOUBLE)
				{ MCC_TRACE("br\n"); allfp = 0; } }
		mccjit_last_allfp = allfp;
		for (qi = 0; qi < MCCJIT_KGC_MAXARG; qi++)
			{ MCC_TRACE("br\n"); mccjit_last_param_t[qi] = 0; }
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
		mccjit_last_kgc_ok =
				scalar_ok && (mccjit_purity_noescape_enabled() ? mccjit_last_purity_ne
																											: mccjit_last_purity) != AST_PURITY_IMPURE;
	}

	if (do_spec && param_index >= 0 && (uint32_t)param_index < it.nparam) { MCC_TRACE("br\n");
		int spec_folds = mccjit_ast_spec_fold(it.arena, (int)it.param_off[param_index], const_val);
		if (mcc_stats_mask)
			{ MCC_TRACE("br\n"); mcc_stats_jit_specfold(spec_folds); }
	}

	sym = mccjit_rebuild_sym(&it);
	mccjit_internal_compile = 1;
	volatile int reemit_ok = 0;
	char sv_debug_modes = debug_modes;
	debug_modes = 0;
	int sv_stk_floor = stk_data_floor;
	stk_data_floor = nb_stk_data;
	js->error_set_jmp_enabled = 1;
	if (setjmp(js->error_jmp_buf) == 0) { MCC_TRACE("br\n");
		if (sym) { MCC_TRACE("br\n");
			ast_fconst_reuse_disable(1);
			if (MCC_DEV_ENV_ON("MCC_JIT_SELFTEST_FOLD_CONSTS"))
				{ MCC_TRACE("br\n"); ast_jit_fold_consts(it.arena); }
			mccjit_slice_hotpatch(it.arena);
			if (mccjit_recompile_blind_retype) { MCC_TRACE("br\n");
				int rt = mccjit_ast_blind_retype(it.arena);
				mccjit_last_blind_proven = (rt == 2);
				if (rt && mcc_stats_mask)
					{ MCC_TRACE("br\n"); mcc_stats_jit_blind(1, rt == 2); }
			} else { MCC_TRACE("br\n");
				mccjit_last_blind_proven = 0;
			}
			if (mccjit_recompile_use_gates)
				{ MCC_TRACE("br\n"); ast_reemit_with_gates(sym, it.arena, mccjit_recompile_gate_mask); }
			else if (have_override && override_mask)
				{ MCC_TRACE("br\n"); ast_reemit_with_gates(sym, it.arena, override_mask); }
			else if (MCC_DEV_ENV_ON("MCC_JIT_SELFTEST_REEMIT_GATES"))
				{ MCC_TRACE("br\n"); ast_reemit_with_gates(sym, it.arena, 0); }
			else
				{ MCC_TRACE("br\n"); ast_reemit_extern(sym, it.arena); }
			mccjit_last_cost = ast_cost_score(it.arena);
			reemit_ok = 1;
		}
	}
	js->error_set_jmp_enabled = 0;
	debug_modes = sv_debug_modes;
	stk_data_floor = sv_stk_floor;
	ast_fconst_reuse_disable(0);
	if (!reemit_ok)
		{ MCC_TRACE("br\n"); mccjit_last_cost = -1; }
	sym_pop(&local_stack, sav_local, 0);
	sym_pop(&global_stack, sav_global, 0);
	mcc_exit_state(js);

	if (reemit_ok && it.nbind)
		{ MCC_TRACE("br\n"); mccjit_bind_apply(js, &it); }
	mccjit_error_quiet = 1;
	if (reemit_ok && mcc_relocate(js) == 0)
		{ MCC_TRACE("br\n"); entry = mcc_get_symbol(js, it.fn_name); }
	mccjit_error_quiet = 0;
	mccjit_internal_compile = 0;

	if (entry)
		{ MCC_TRACE("br\n"); mccjit_perf_map_emit(js, it.fn_name, entry); }

	mccjit_intent_release(&it);

	if (entry) { MCC_TRACE("br\n");
		mccjit_last_state = js;
	} else { MCC_TRACE("br\n");
		mcc_delete(js);
	}
	mccjit_recompiling--;
	return entry;
}

MCCJIT_LOCAL void *mcc_jit_recompile_blob(const void *buf, size_t len) { MCC_TRACE("enter\n");
	return mccjit_recompile_common(buf, len, 0, -1, 0);
}

MCCJIT_LOCAL void *mcc_jit_recompile_blob_gated(const void *buf, size_t len,
																								uint64_t gate_mask) { MCC_TRACE("enter\n");
	void *r;
	mccjit_recompile_use_gates = 1;
	mccjit_recompile_gate_mask = gate_mask;
	r = mccjit_recompile_common(buf, len, 0, -1, 0);
	mccjit_recompile_use_gates = 0;
	return r;
}

static double mccjit_elapsed(const struct timespec *t0);

MCCJIT_LOCAL void *mccjit_search_masks(const void *blob, size_t len,
																			 const uint64_t *masks, int nmask,
																			 double budget_s, uint64_t *best_mask_out,
																			 int *tried_out) { MCC_TRACE("enter\n");
	struct timespec t0;
	void *best = NULL;
	int tried = 0, i;
	int timed = (clock_gettime(CLOCK_MONOTONIC, &t0) == 0);
	for (i = 0; i < nmask; i++) { MCC_TRACE("br\n");
		void *v;
		if (timed && budget_s > 0 && mccjit_elapsed(&t0) > budget_s)
			{ MCC_TRACE("br\n"); break; }
		v = mcc_jit_recompile_blob_gated(blob, len, masks[i]);
		if (!v)
			{ MCC_TRACE("br\n"); continue; }
		tried++;
		if (!best) { MCC_TRACE("br\n");
			best = v;
			if (best_mask_out)
				{ MCC_TRACE("br\n"); *best_mask_out = masks[i]; }
		}
	}
	if (tried_out)
		{ MCC_TRACE("br\n"); *tried_out = tried; }
	return best;
}

MCCJIT_LOCAL void *mcc_jit_recompile_blob_spec(const void *buf, size_t len,
																							 int param_index, int64_t const_val) { MCC_TRACE("enter\n");
	return mccjit_recompile_common(buf, len, 1, param_index, const_val);
}

MCCJIT_LOCAL void *mcc_jit_recompile_blob_retype(const void *buf, size_t len) { MCC_TRACE("enter\n");
	void *r;
	mccjit_recompile_blind_retype = 1;
	r = mccjit_recompile_common(buf, len, 0, -1, 0);
	mccjit_recompile_blind_retype = 0;
	return r;
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
	if (mccjit_intent_serialize(ast, sym, &b, 0) != 0) { MCC_TRACE("br\n");
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
	int bind_n;
	int bind_elfsym[MCCJIT_BIND_MAX];
	size_t bind_off[MCCJIT_BIND_MAX];
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

static int mccjit_engine_internal(const char *name) { MCC_TRACE("enter\n");
	if (!strncmp(name, "mccjit_", 7) || !strncmp(name, "mcc_jit_", 8))
		{ MCC_TRACE("br\n"); return 1; }
	if (!strcmp(name, "mcc_log_enabled") || !strcmp(name, "mcc_log_enabled_v") ||
			!strcmp(name, "mcc_trace_at") || !strcmp(name, "mcc_logf") ||
			!strcmp(name, "mcc_log_tag"))
		{ MCC_TRACE("br\n"); return 1; }
	return 0;
}

void mccjit_embed_note(const char *name, AstArena *ast, Sym *sym, uint64_t warm_gates) { MCC_TRACE("enter\n");
	MccjitBuf b;
	MccjitEmbedFn *e;
	if (!name || !name[0] || !ast || !sym || mccjit_internal_compile)
		{ MCC_TRACE("br\n"); return; }
	if (mccjit_engine_internal(name))
		{ MCC_TRACE("br\n"); return; }
	for (e = mccjit_embed_fns; e; e = e->next)
		{ MCC_TRACE("br\n"); if (!strcmp(e->name, name))
			{ MCC_TRACE("br\n"); return; } }
	mccjit_buf_init(&b);
	b.bind_allow = 1;
	if (mccjit_intent_serialize(ast, sym, &b, warm_gates) != 0) { MCC_TRACE("br\n");
		if (mcc_env_on("MCC_JIT_BAKE_WHY"))
			{ MCC_TRACE("br\n"); fprintf(stderr, "mccjit-site[%s]: refused\n", name); }
		mccjit_buf_free(&b);
		return;
	}
	if (mcc_env_on("MCC_JIT_BAKE_WHY"))
		{ MCC_TRACE("br\n"); fprintf(stderr, "mccjit-site[%s]: baked\n", name); }
	e = mcc_mallocz(sizeof *e);
	if (!e) { MCC_TRACE("br\n");
		mccjit_buf_free(&b);
		return;
	}
	e->name = mcc_strdup(name);
	e->blob = b.data;
	e->len = b.len;
	e->bind_n = b.bind_n;
	memcpy(e->bind_elfsym, b.bind_elfsym, sizeof e->bind_elfsym);
	memcpy(e->bind_off, b.bind_off, sizeof e->bind_off);
	e->next = mccjit_embed_fns;
	mccjit_embed_fns = e;
	mcc_inv_add("jit.embed", 1);
	mcc_inv_add("jit.embed_bytes", (long long)e->len);
	if (mcc_stats_mask)
		{ MCC_TRACE("br\n"); mcc_stats_jit_capture((unsigned long)e->len); }
}

#if defined(MCCJIT_X64)
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
#elif defined(MCCJIT_I386)
static void *mccjit_make_trampoline(void *variant) { MCC_TRACE("enter\n");
	unsigned char *p;
	if (!mccjit_i386_stubs_enabled())
		{ MCC_TRACE("br\n"); return variant; }
	p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
					 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return variant; }
	p[0] = 0xc9;
	p[1] = 0xb8;
	{ uint32_t v = (uint32_t)(uintptr_t)variant; memcpy(p + 2, &v, 4); }
	p[6] = 0xff; p[7] = 0xe0;
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

/* On the PE embed-JIT binary the engine (mingw/ucrt) getenv + stderr are not
 * visible to the launching process (msvcrt/ucrt CRT split), so the swap
 * diagnostic the detectors parse never reaches them.  Read the gate and emit
 * through kernel32 (CRT-agnostic) instead.  See DETAILS
 * #t-win-50021-2b-ROOTCAUSE-detection-artifact-2026-08-17. */
#if MCC_HOST_WIN32
static int mccjit_verbose_on(void) { MCC_TRACE("enter\n");
	char b[8];
	DWORD n = GetEnvironmentVariableA("MCC_JIT_VERBOSE", b, sizeof b);
	return n > 0 && n < sizeof b && b[0] != '0';
}
static void mccjit_diag_emit(const char *s, unsigned n) { MCC_TRACE("enter\n");
	HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
	if (h && h != INVALID_HANDLE_VALUE) { MCC_TRACE("br\n"); DWORD w; WriteFile(h, s, n, &w, NULL); }
}
#else
static int mccjit_verbose_on(void) { MCC_TRACE("enter\n"); return mcc_env_on("MCC_JIT_VERBOSE"); }
static void mccjit_diag_emit(const char *s, unsigned n) { MCC_TRACE("enter\n"); fwrite(s, 1, n, stderr); }
#endif

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
	int no_kgc = !mcc_env_flag("MCC_JIT_KGC", 1);
	int spec_wrong = MCC_DEV_ENV_ON("MCC_JIT_SPEC_WRONG");
	struct timespec cstart;
	int ctimed = 0;
	if (timed && max_duration && mccjit_elapsed(t0) > (double)max_duration) { MCC_TRACE("br\n");
		skipped = 1;
	} else { MCC_TRACE("br\n");
		ctimed = mcc_stats_mask && clock_gettime(CLOCK_MONOTONIC, &cstart) == 0;
		variant = spec_wrong
									? mcc_jit_recompile_blob_spec(blob, (size_t)len, 0, 7)
									: mcc_env_on("MCC_JIT_BLIND_RETYPE")
											? mcc_jit_recompile_blob_retype(blob, (size_t)len)
											: mcc_jit_recompile_blob(blob, (size_t)len);
		if (variant && mcc_env_on("MCC_JIT_BLIND_RETYPE") &&
				mccjit_last_blind_proven) { MCC_TRACE("br\n");
			entry = mccjit_make_trampoline(variant);
			if (entry) { MCC_TRACE("br\n");
				routed = 1;
				if (mcc_stats_mask)
					{ MCC_TRACE("br\n"); mcc_stats_jit_blind_promote(); }
			}
		}
		if (!entry && variant && !no_kgc && mccjit_last_kgc_ok) { MCC_TRACE("br\n");
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
				if (entry) { MCC_TRACE("br\n");
					routed = 1;
					if (mcc_stats_mask)
						{ MCC_TRACE("br\n"); mcc_stats_jit_kgc_stub(); }
				}
			}
		}
		if (!entry && no_kgc)
			{ MCC_TRACE("br\n"); entry = variant ? mccjit_make_trampoline(variant) : NULL; }
		if (ctimed) { MCC_TRACE("br\n");
			double dt = mccjit_elapsed(&cstart);
			if (dt >= 0)
				{ MCC_TRACE("br\n"); mcc_stats_jit_compile((unsigned)(dt * 1000.0)); }
		}
		if (timed && max_duration && entry &&
				mccjit_elapsed(t0) > (double)max_duration) { MCC_TRACE("br\n");
			over = 1;
			entry = NULL;
		}
	}
	if (mcc_stats_mask) { MCC_TRACE("br\n");
		int outcome = skipped						? MCC_JIT_OUT_BUDGET_SKIP
									: over							? MCC_JIT_OUT_OVER_BUDGET
									: entry							? MCC_JIT_OUT_SWAPPED
									: (variant && !no_kgc && !mccjit_last_kgc_ok)
											? MCC_JIT_OUT_REFUSED
											: MCC_JIT_OUT_KEPT_AOT;
		mcc_stats_jit_outcome(outcome);
	}
	if (mccjit_verbose_on()) { MCC_TRACE("br\n");
		int probeable = MCC_DEV_ENV_ON("MCC_JIT_PROBE") && variant &&
										mccjit_last_nparam == 1 &&
										!mccjit_type_wide((int)mccjit_last_param_t[0]);
		int probe = probeable ? ((int (*)(int))variant)(7) : -1;
		char _vb[512];
		int _vn = snprintf(_vb, sizeof _vb,
						"mccjit-boot[%s]: slot=%p aot=%p blob=%p len=%lu variant=%p baseline=%p entry=%p route=%s np=%u warm=%llx probe(7)=%d %s\n",
						mode, (void *)slot, aot_init, blob, len, variant, baseline, entry,
						routed ? "kgc" : "direct", mccjit_last_nparam,
						(unsigned long long)mccjit_intent_peek_warm_gates(blob, (size_t)len), probe,
						skipped				 ? "budget-skip"
						: over					 ? "over-budget-kept-aot"
						: entry					 ? "swapped"
						: (variant && !no_kgc && !mccjit_last_kgc_ok)
								? "refused-unverified"
								: "kept-aot");
		if (_vn > 0) { MCC_TRACE("br\n");
			if (_vn >= (int)sizeof _vb) _vn = (int)sizeof _vb - 1;
			mccjit_diag_emit(_vb, (unsigned)_vn);
		}
	}
	MCCJIT_DIAG_NOTE_BOOT(variant, baseline, entry, mccjit_last_nparam, mode,
												mccjit_last_param_t, mccjit_last_ret_wide);
	if (entry)
		{ MCC_TRACE("br\n"); mcc_jit_publish(slot, entry); }
}

static int mccjit_bench_enabled(void) { MCC_TRACE("enter\n");
	const char *e = MCC_DEV_ENV("MCC_JIT_BENCH");
	return e && e[0] && e[0] != '0';
}

static void mccjit_graduate_slices_blob(const void *blob, size_t len,
																				uint64_t gate_mask) { MCC_TRACE("enter\n");
	MccjitIntent it;
	MCCState *js;
	if (!ast_slice_enabled() || !blob)
		{ MCC_TRACE("br\n"); return; }
	js = mcc_new();
	if (!js)
		{ MCC_TRACE("br\n"); return; }
	js->optimize = 0;
	js->nostdlib = 1;
	mcc_set_output_type(js, MCC_OUTPUT_MEMORY);
	mcc_enter_state(js);
	mccpp_new(js);
	mccgen_init(js);
	ast_configure(js);
	anon_sym = SYM_FIRST_ANOM;
	funcname = "";
	func_ind = -1;
	if (mccjit_intent_deserialize(blob, len, &it) != 0) { MCC_TRACE("br\n");
		mcc_exit_state(js);
		mcc_delete(js);
		return;
	}
	ast_slice_graduate_arena(it.arena, gate_mask);
	if (mcc_env_on("MCC_JIT_VERBOSE"))
		{ MCC_TRACE("br\n"); fprintf(stderr,
					"mccjit-graduate[%s]: slice-cache proven gates=%llx\n",
					it.fn_name ? it.fn_name : "?", (unsigned long long)gate_mask); }
	mccjit_intent_release(&it);
	mcc_exit_state(js);
	mcc_delete(js);
}

static void *mccjit_lazy_build_masked(const void *blob, unsigned long len,
																			uint64_t gate_mask, int use_gates,
																			int *routed) { MCC_TRACE("enter\n");
	int no_kgc = !mcc_env_flag("MCC_JIT_KGC", 1);
	int spec_wrong = MCC_DEV_ENV_ON("MCC_JIT_SPEC_WRONG");
	void *variant = spec_wrong
											? mcc_jit_recompile_blob_spec(blob, (size_t)len, 0, 7)
											: mcc_env_on("MCC_JIT_BLIND_RETYPE")
													? mcc_jit_recompile_blob_retype(blob, (size_t)len)
													: use_gates
															? mcc_jit_recompile_blob_gated(blob, (size_t)len, gate_mask)
															: mcc_jit_recompile_blob(blob, (size_t)len);
	void *entry = NULL;
	int blind_proven = mcc_env_on("MCC_JIT_BLIND_RETYPE") && mccjit_last_blind_proven;
	mccjit_variant_cost = mccjit_last_cost;
	if (routed)
		{ MCC_TRACE("br\n"); *routed = 0; }
	if (variant && blind_proven) { MCC_TRACE("br\n");
		entry = mccjit_make_trampoline(variant);
		if (entry && mcc_stats_mask)
			{ MCC_TRACE("br\n"); mcc_stats_jit_blind_promote(); }
		if (entry)
			{ MCC_TRACE("br\n"); return entry; }
	}
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
			if (entry) { MCC_TRACE("br\n");
				if (routed)
					{ MCC_TRACE("br\n"); *routed = 1; }
				if (mcc_stats_mask)
					{ MCC_TRACE("br\n"); mcc_stats_jit_kgc_stub(); }
			}
		}
	}
	if (!entry && no_kgc)
		{ MCC_TRACE("br\n"); entry = variant ? mccjit_make_trampoline(variant) : NULL; }
	return entry;
}

static void *mccjit_lazy_build(const void *blob, unsigned long len, int *routed) { MCC_TRACE("enter\n");
	return mccjit_lazy_build_masked(blob, len, 0, 0, routed);
}

#define MCCJIT_PROFILE_SAMPLES 8
#define MCCJIT_LAZY_MAX_BUILD_FAIL 3

typedef struct MccjitCounterState {
	void **slot;
	const void *blob;
	unsigned long len;
	void *baseline;
	long threshold;
	long count;
	void *promoted;
	int building;
	int failed;
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

#define MCCJIT_KGC_ARITY 6
#define MCCJIT_WC_MAXCAND 8

/* Defined later (after the bench primitives); forward-declared here because
 * mccjit_lazy_search calls them (T-mac-30295 slice 2). */
static int mccjit_bench_rank_n(void *const *cands, int ncand,
															 const int64_t *tuples, uint32_t ntuples,
															 uint32_t nargs, int wide, int fp,
															 int *order, double *secs);
static int mccjit_bench_pick_significant(const int *order, const double *secs,
																				 int cnt, int ref);

/* T-mac-30295 slice 2: collect a benchmarkable candidate (bounded, dedup by
 * mask) for the JIT wall-clock final-selection pass in mccjit_lazy_search. */
static void mccjit_wc_collect(void **fn, uint64_t *mask, int *routed, long *cost,
															int *n, void *cand, uint64_t m, int r, long cc) { MCC_TRACE("enter\n");
	int i;
	if (*n >= MCCJIT_WC_MAXCAND)
		{ MCC_TRACE("br\n"); return; }
	for (i = 0; i < *n; i++) { MCC_TRACE("br\n");
		if (mask[i] == m)
			{ MCC_TRACE("br\n"); return; }
	}
	fn[*n] = cand;
	mask[*n] = m;
	routed[*n] = r;
	cost[*n] = cc;
	(*n)++;
}

static void *mccjit_lazy_search(MccjitCounterState *st, int *routed, int async) { MCC_TRACE("enter\n");
	uint64_t vocab[256];
	int nv = ast_jit_search_vocab(vocab, (int)(sizeof vocab / sizeof vocab[0]));
	double budget_s;
	const char *e = getenv("MCC_JIT_SEARCH_MS");
	struct timespec t0;
	void *best = NULL;
	int best_routed = 0, i, timed;
	long gs_cands = 0, gs_admits = 0;
	int gs_budget_hit = 0;
	uint64_t gs_best_mask = 0;
	int gs_bench_won = 0;
	long best_cost = -1;
	int stale = 0;
	int stale_max = (int)mcc_env_num("MCC_JIT_SEARCH_PLATEAU", 3);
	int wallclock = mcc_env_on("MCC_JIT_SEARCH_WALLCLOCK");
	void *wc_fn[MCCJIT_WC_MAXCAND];
	uint64_t wc_mask[MCCJIT_WC_MAXCAND];
	int wc_routed[MCCJIT_WC_MAXCAND];
	long wc_cost[MCCJIT_WC_MAXCAND];
	int wc_n = 0;
	uint32_t wc_nargs = 0;
	int wc_wide = 0;
	if (e && e[0])
		{ MCC_TRACE("br\n"); budget_s = strtod(e, NULL) / 1000.0; }
	else if (mccjit_search_budget_baked_s)
		{ MCC_TRACE("br\n"); budget_s = (double)mccjit_search_budget_baked_s; }
	else
		{ MCC_TRACE("br\n"); budget_s = 0.05; }
	if (!async && budget_s > 0.1)
		{ MCC_TRACE("br\n"); budget_s = 0.1; }
	if (stale_max < 1)
		{ MCC_TRACE("br\n"); stale_max = 1; }
	timed = (clock_gettime(CLOCK_MONOTONIC, &t0) == 0);
	{
		uint64_t warm = mccjit_intent_peek_warm_gates(st->blob, st->len);
		if (warm) { MCC_TRACE("br\n");
			int r = 0;
			void *cand = mccjit_lazy_build_masked(st->blob, st->len, warm, 1, &r);
			gs_cands++;
			if (cand) { MCC_TRACE("br\n");
				best = cand;
				best_routed = r;
				gs_admits++;
				gs_best_mask = warm;
				best_cost = mccjit_variant_cost;
				if (async)
					{ MCC_TRACE("br\n"); mcc_jit_publish(st->slot, best); }
				if (wallclock && r && !mccjit_last_allfp && mccjit_last_nparam > 0) { MCC_TRACE("br\n");
					wc_nargs = mccjit_last_nparam;
					wc_wide = mccjit_last_ret_wide;
					mccjit_wc_collect(wc_fn, wc_mask, wc_routed, wc_cost, &wc_n, cand,
														warm, r, mccjit_variant_cost);
				}
			}
		}
	}
	for (i = 0; i < nv; i++) { MCC_TRACE("br\n");
		int r = 0;
		void *cand;
		long cc;
		int admit, improved;
		if (best && timed && budget_s > 0 && mccjit_elapsed(&t0) > budget_s)
			{ MCC_TRACE("br\n"); gs_budget_hit = 1; break; }
		cand = mccjit_lazy_build_masked(st->blob, st->len, vocab[i], 1, &r);
		cc = mccjit_variant_cost;
		gs_cands++;
		if (!cand)
			{ MCC_TRACE("br\n"); continue; }
		improved = (cc >= 0 && (best_cost < 0 || cc < best_cost));
		if (!best)
			{ MCC_TRACE("br\n"); admit = 1; }
		else if (mccjit_bench_enabled())
			{ MCC_TRACE("br\n"); admit = mccjit_bench_admit(cand, best, st, mccjit_last_nparam,
																	mccjit_last_ret_wide, mccjit_last_allfp, r); }
		else
			{ MCC_TRACE("br\n"); admit = improved; }
		if (admit) { MCC_TRACE("br\n");
			if (mccjit_bench_enabled() && best && r && !mccjit_last_allfp &&
					mccjit_last_nparam > 0) { MCC_TRACE("br\n"); gs_bench_won = 1; }
			best = cand;
			best_routed = r;
			gs_admits++;
			gs_best_mask = vocab[i];
			if (async)
				{ MCC_TRACE("br\n"); mcc_jit_publish(st->slot, best); }
		}
		if (wallclock && r && !mccjit_last_allfp && mccjit_last_nparam > 0) { MCC_TRACE("br\n");
			wc_nargs = mccjit_last_nparam;
			wc_wide = mccjit_last_ret_wide;
			mccjit_wc_collect(wc_fn, wc_mask, wc_routed, wc_cost, &wc_n, cand, vocab[i], r, cc);
		}
		if (improved)
			{ MCC_TRACE("br\n"); best_cost = cc; stale = 0; }
		else
			{ MCC_TRACE("br\n"); stale++; }
		if (wallclock && wc_n >= MCCJIT_WC_MAXCAND)
			{ MCC_TRACE("br\n"); break; }
		if (!wallclock && best && best_cost >= 0 && stale >= stale_max)
			{ MCC_TRACE("br\n"); break; }
	}
	if (wallclock && mcc_env_on("MCC_JIT_OUT_WALLCLOCK"))
		{ MCC_TRACE("br\n"); fprintf(stderr, "mccjit-wallclock: cands=%d nargs=%u "
					"nsample=%d\n", wc_n, wc_nargs, st->nsample); }
	if (wallclock && wc_n >= 2 && wc_nargs > 0 && st->nsample > 0) { MCC_TRACE("br\n");
		int64_t wc_tuples[MCCJIT_PROFILE_SAMPLES * MCCJIT_KGC_ARITY];
		int order[MCCJIT_WC_MAXCAND];
		double secs[MCCJIT_WC_MAXCAND];
		uint32_t nt = (uint32_t)st->nsample, ti, tj;
		int nn, ref = 0, pick, k;
		if (nt > MCCJIT_PROFILE_SAMPLES)
			{ MCC_TRACE("br\n"); nt = MCCJIT_PROFILE_SAMPLES; }
		for (ti = 0; ti < nt; ti++)
			{ MCC_TRACE("br\n"); for (tj = 0; tj < MCCJIT_KGC_ARITY; tj++)
				{ MCC_TRACE("br\n"); wc_tuples[ti * MCCJIT_KGC_ARITY + tj] = st->sample[ti][tj]; } }
		for (k = 0; k < wc_n; k++) { MCC_TRACE("br\n");
			if (wc_mask[k] == gs_best_mask)
				{ MCC_TRACE("br\n"); ref = k; break; }
		}
		nn = mccjit_bench_rank_n((void *const *)wc_fn, wc_n, wc_tuples, nt, wc_nargs,
														 wc_wide, 0, order, secs);
		if (nn >= 2) { MCC_TRACE("br\n");
			pick = mccjit_bench_pick_significant(order, secs, nn, ref);
			if (pick >= 0 && pick < wc_n && wc_fn[pick]) { MCC_TRACE("br\n");
				best = wc_fn[pick];
				best_routed = wc_routed[pick];
				gs_best_mask = wc_mask[pick];
				gs_bench_won = 1;
				if (async)
					{ MCC_TRACE("br\n"); mcc_jit_publish(st->slot, best); }
			}
			if (mcc_env_on("MCC_JIT_OUT_WALLCLOCK"))
				{ MCC_TRACE("br\n"); fprintf(stderr, "mccjit-wallclock: ranked=%d ref=%d "
							"pick=%d picked_mask=0x%llx\n", nn, ref, pick,
							(unsigned long long)gs_best_mask); }
		}
	}
	if (routed)
		{ MCC_TRACE("br\n"); *routed = best_routed; }
	if (mcc_stats_mask)
		{ MCC_TRACE("br\n"); mcc_stats_jit_gsearch(gs_cands, gs_admits, gs_budget_hit, gs_best_mask); }
	if (gs_bench_won && best)
		{ MCC_TRACE("br\n"); mccjit_graduate_slices_blob(st->blob, st->len, gs_best_mask); }
	return best;
}

static void *mccjit_lazy_entry(MccjitCounterState *st, int *routed, int async);

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

MCCJIT_LOCAL void *mccjit_recompile_profiled(const void *blob, size_t len,
																						 const MccjitCounterState *st,
																						 uint32_t nargs, long min_samples) { MCC_TRACE("enter\n");
	int pidx = -1;
	int64_t pval = 0;
	if (mccjit_profile_pick_const(st, nargs, min_samples, &pidx, &pval)) { MCC_TRACE("br\n");
		if (mcc_stats_mask)
			{ MCC_TRACE("br\n"); mcc_stats_jit_prof_spec(); }
		return mcc_jit_recompile_blob_spec(blob, len, pidx, pval);
	}
	return mcc_jit_recompile_blob(blob, (size_t)len);
}

typedef struct MccjitSwapJob {
	int resume;
	void **slot;
	const void *blob;
	unsigned long len;
	unsigned long max_duration;
	struct timespec start;
	int timed;
	MccjitCounterState *cst;
} MccjitSwapJob;

static pthread_mutex_t mccjit_swap_lock = PTHREAD_MUTEX_INITIALIZER;

static int mccjit_swap_wide;

static void mccjit_codegen_lock(void) { MCC_TRACE("enter\n");
	if (!mccjit_swap_wide)
		{ MCC_TRACE("br\n"); pthread_mutex_lock(&mccjit_swap_lock); }
}

static void mccjit_codegen_unlock(void) { MCC_TRACE("enter\n");
	if (!mccjit_swap_wide)
		{ MCC_TRACE("br\n"); pthread_mutex_unlock(&mccjit_swap_lock); }
}

static struct {
	pthread_mutex_t lock;
	int cur;
	int peak;
} mccjit_conc = {PTHREAD_MUTEX_INITIALIZER, 0, 0};

static void mccjit_conc_enter(void) { MCC_TRACE("enter\n");
	pthread_mutex_lock(&mccjit_conc.lock);
	mccjit_conc.cur++;
	if (mccjit_conc.cur > mccjit_conc.peak)
		{ MCC_TRACE("br\n"); mccjit_conc.peak = mccjit_conc.cur; }
	pthread_mutex_unlock(&mccjit_conc.lock);
}

static void mccjit_conc_leave(void) { MCC_TRACE("enter\n");
	pthread_mutex_lock(&mccjit_conc.lock);
	mccjit_conc.cur--;
	pthread_mutex_unlock(&mccjit_conc.lock);
}

static void mccjit_conc_reset(void) { MCC_TRACE("enter\n");
	pthread_mutex_lock(&mccjit_conc.lock);
	mccjit_conc.cur = 0;
	mccjit_conc.peak = 0;
	pthread_mutex_unlock(&mccjit_conc.lock);
}

#include "mccpool.h"

static int mccjit_qsbr_register(void);
static void mccjit_qsbr_unregister(int slot);
static void mccjit_qsbr_quiescent(int slot);
PUB_FUNC void mccjit_shutdown(void);

static MccPool mccjit_pool = MCC_POOL_INIT("mccjit");

static pthread_once_t mccjit_fork_once = PTHREAD_ONCE_INIT;

static void *mccjit_pool_qsbr_begin(void) { MCC_TRACE("enter\n");
	return (void *)(intptr_t)mccjit_qsbr_register();
}

static void mccjit_pool_qsbr_end(void *token) { MCC_TRACE("enter\n");
	int slot = (int)(intptr_t)token;
	mccjit_qsbr_quiescent(slot);
	mccjit_qsbr_unregister(slot);
}

static void mccjit_pool_swap_lock(void) { MCC_TRACE("enter\n");
	if (mccjit_swap_wide)
		{ MCC_TRACE("br\n"); pthread_mutex_lock(&mccjit_swap_lock); }
}

static void mccjit_pool_swap_unlock(void) { MCC_TRACE("enter\n");
	if (mccjit_swap_wide)
		{ MCC_TRACE("br\n"); pthread_mutex_unlock(&mccjit_swap_lock); }
}

#if !MCC_HOST_WIN32
static void mccjit_atfork_prepare(void) { MCC_TRACE("enter\n");
	mcc_pool_atfork_prepare(&mccjit_pool);
	pthread_mutex_lock(&mccjit_swap_lock);
}

static void mccjit_atfork_parent(void) { MCC_TRACE("enter\n");
	pthread_mutex_unlock(&mccjit_swap_lock);
	mcc_pool_atfork_parent(&mccjit_pool);
}

static void mccjit_atfork_child(void) { MCC_TRACE("enter\n");
	mcc_pool_atfork_child(&mccjit_pool);
	pthread_mutex_unlock(&mccjit_swap_lock);
}
#endif

static void mccjit_fork_setup(void) { MCC_TRACE("enter\n");
#if !MCC_HOST_WIN32
	pthread_atfork(mccjit_atfork_prepare, mccjit_atfork_parent,
								 mccjit_atfork_child);
#endif
}

static MccjitSwapJob *mccjit_swap_job_new(void) { MCC_TRACE("enter\n");
	MccjitSwapJob *job = mcc_malloc(sizeof *job);
	if (!job)
		{ MCC_TRACE("br\n"); return NULL; }
	*job = (MccjitSwapJob){0};
	return job;
}

static int mccjit_pool_start(unsigned long workers) { MCC_TRACE("enter\n");
	pthread_once(&mccjit_fork_once, mccjit_fork_setup);
	mccjit_swap_wide = mcc_env_on("MCC_JIT_SWAP_WIDE");
	mccjit_pool.job_begin = mccjit_pool_qsbr_begin;
	mccjit_pool.job_end = mccjit_pool_qsbr_end;
	mccjit_pool.tick_lock = mccjit_pool_swap_lock;
	mccjit_pool.tick_unlock = mccjit_pool_swap_unlock;
	mccjit_pool.cap_label = "JIT";
	mccjit_pool.cap_macro = "MCCJIT_POOL_MAX";
	mccjit_pool.verbose = mcc_env_on("MCC_JIT_VERBOSE");
	return mcc_pool_start(&mccjit_pool, workers);
}

static int mccjit_pool_ready(void) { MCC_TRACE("enter\n");
	return mcc_pool_ready(&mccjit_pool);
}

static int mccjit_pool_submit(int (*tick)(void *), MccjitSwapJob *payload) { MCC_TRACE("enter\n");
	return mcc_pool_submit(&mccjit_pool, tick, payload, mcc_free);
}

static void mccjit_pool_shutdown(void) { MCC_TRACE("enter\n");
	mcc_pool_shutdown(&mccjit_pool);
}

static int mccjit_job_run_eager(void *ctx) { MCC_TRACE("enter\n");
	MccjitSwapJob *job = ctx;
	mccjit_codegen_lock();
	mccjit_boot_swap_run(job->slot, job->blob, job->len, job->max_duration,
											 "async", &job->start, job->timed);
	mccjit_codegen_unlock();
	return MCC_TASK_DONE;
}

static int mccjit_job_run_lazy(void *ctx) { MCC_TRACE("enter\n");
	MccjitSwapJob *job = ctx;
	MccjitCounterState *st = job->cst;
	int routed = 0;
	void *entry;
	uint32_t nargs;
	int wide;
	int allfp;
	mccjit_codegen_lock();
	entry = mccjit_lazy_entry(st, &routed, 1);
	nargs = mccjit_last_nparam;
	wide = mccjit_last_ret_wide;
	allfp = mccjit_last_allfp;
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
	} else { MCC_TRACE("br\n");
		st->failed++;
	}
	st->building = 0;
	pthread_mutex_unlock(&st->lock);
	mccjit_codegen_unlock();
	if (mcc_env_on("MCC_JIT_VERBOSE"))
		{ MCC_TRACE("br\n"); fprintf(stderr,
						"mccjit-lazy[promote-async]: slot=%p entry=%p route=%s %s\n",
						(void *)st->slot, entry, routed ? "kgc" : "direct",
						entry ? "promoted" : "build-failed"); }
	return MCC_TASK_DONE;
}

static void *mccjit_counter_tick(MccjitCounterState *st, const int64_t *regs) { MCC_TRACE("enter\n");
	long n;
	void *target;
	int verbose = mcc_env_on("MCC_JIT_VERBOSE");
	pthread_mutex_lock(&st->lock);
	n = ++st->count;
	if (n == st->threshold && mcc_stats_mask)
		{ MCC_TRACE("br\n"); mcc_stats_jit_hot(); }
	if (regs && !st->promoted)
		{ MCC_TRACE("br\n"); mccjit_counter_capture(st, regs); }
	if (st->promoted) { MCC_TRACE("br\n");
		target = st->promoted;
	} else if (st->failed >= MCCJIT_LAZY_MAX_BUILD_FAIL) { MCC_TRACE("br\n");
		target = st->baseline;
	} else if (n < st->threshold) { MCC_TRACE("br\n");
		if (verbose && n == 1)
			{ MCC_TRACE("br\n"); fprintf(stderr,
							"mccjit-lazy[cold]: slot=%p call=%ld<threshold=%ld running baseline=%p\n",
							(void *)st->slot, n, st->threshold, st->baseline); }
		target = st->baseline;
	} else if (mccjit_pool_ready()) { MCC_TRACE("br\n");
		if (!st->building) { MCC_TRACE("br\n");
			MccjitSwapJob *job = mccjit_swap_job_new();
			if (job) { MCC_TRACE("br\n");
				job->cst = st;
				st->building = 1;
				if (!mccjit_pool_submit(mccjit_job_run_lazy, job)) { MCC_TRACE("br\n");
					st->building = 0;
				} else if (verbose) { MCC_TRACE("br\n");
					fprintf(stderr,
									"mccjit-lazy[promote-async]: slot=%p hot after %ld calls -> queued\n",
									(void *)st->slot, n);
				}
			}
		}
		target = st->baseline;
	} else { MCC_TRACE("br\n");
		int routed = 0;
		void *entry = mccjit_lazy_entry(st, &routed, 0);
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
			st->failed++;
			if (verbose)
				{ MCC_TRACE("br\n"); fprintf(stderr,
								"mccjit-lazy[promote]: slot=%p build failed (%d/%d), %s\n",
								(void *)st->slot, st->failed, MCCJIT_LAZY_MAX_BUILD_FAIL,
								st->failed >= MCCJIT_LAZY_MAX_BUILD_FAIL ? "giving up, baseline is final"
																												 : "staying cold"); }
		}
	}
	pthread_mutex_unlock(&st->lock);
	return target;
}

#if defined(MCCJIT_X64)
static void *mccjit_make_counter_stub(MccjitCounterState *st) { MCC_TRACE("enter\n");
	void *tick = (void *)mccjit_counter_tick;
	unsigned char *p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
													MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	size_t o = 0;
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return NULL; }
#if MCC_HOST_WIN32
	p[o++] = 0x51;
	p[o++] = 0x52;
	p[o++] = 0x41; p[o++] = 0x50;
	p[o++] = 0x41; p[o++] = 0x51;
	p[o++] = 0x6a; p[o++] = 0x00;
	p[o++] = 0x6a; p[o++] = 0x00;
	p[o++] = 0x48; p[o++] = 0x89; p[o++] = 0xe2;
	p[o++] = 0x48; p[o++] = 0x83; p[o++] = 0xec; p[o++] = 0x28;
	p[o++] = 0x48; p[o++] = 0xb9;
	memcpy(p + o, &st, 8);
	o += 8;
	p[o++] = 0x48; p[o++] = 0xb8;
	memcpy(p + o, &tick, 8);
	o += 8;
	p[o++] = 0xff; p[o++] = 0xd0;
	p[o++] = 0x48; p[o++] = 0x83; p[o++] = 0xc4; p[o++] = 0x28;
	p[o++] = 0x41; p[o++] = 0x5a;
	p[o++] = 0x41; p[o++] = 0x5a;
	p[o++] = 0x41; p[o++] = 0x59;
	p[o++] = 0x41; p[o++] = 0x58;
	p[o++] = 0x5a;
	p[o++] = 0x59;
	p[o++] = 0xff; p[o++] = 0xe0;
#else
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
#endif
	return p;
}
#elif defined(MCCJIT_ARM64)
static void *mccjit_make_counter_stub(MccjitCounterState *st) { MCC_TRACE("enter\n");
	void *tick = (void *)mccjit_counter_tick;
	size_t page = host_pagesize();
	unsigned char *p;
	static const uint32_t code[30] = {
			0xd10303ffu,
			0xf90003e5u,
			0xf90007e4u,
			0xf9000be3u,
			0xf9000fe2u,
			0xf90013e1u,
			0xf90017e0u,
			0xf9001bfeu,
			0xad0207e0u,
			0xad030fe2u,
			0xad0417e4u,
			0xad051fe6u,
			0x58000240u,
			0x910003e1u,
			0x58000250u,
			0xd63f0200u,
			0xaa0003f0u,
			0xf9401bfeu,
			0xf94017e0u,
			0xf94013e1u,
			0xf9400fe2u,
			0xf9400be3u,
			0xf94007e4u,
			0xf94003e5u,
			0xad4207e0u,
			0xad430fe2u,
			0xad4417e4u,
			0xad451fe6u,
			0x910303ffu,
			0xd61f0200u,
	};
	p = mmap(0, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return NULL; }
	memcpy(p, code, sizeof code);
	memcpy(p + 120, &st, 8);
	memcpy(p + 128, &tick, 8);
	if (host_runmem_protect(p, page, HOST_PROT_RX) != 0) { MCC_TRACE("br\n");
		munmap(p, page);
		return NULL;
	}
	return p;
}
#elif defined(MCCJIT_I386)
static void *mccjit_make_counter_stub(MccjitCounterState *st) { MCC_TRACE("enter\n");
	void *tick = (void *)mccjit_counter_tick;
	unsigned char *p;
	size_t o = 0;
	int i;
	const int REGS = 8;
	if (!mccjit_i386_stubs_enabled())
		{ MCC_TRACE("br\n"); return NULL; }
	p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
					 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return NULL; }
	p[o++] = 0x55;
	p[o++] = 0x89; p[o++] = 0xe5;
	p[o++] = 0x81; p[o++] = 0xec;
	{ uint32_t fr = (uint32_t)(REGS + 8 * MCCJIT_KGC_MAXARG + 16);
		memcpy(p + o, &fr, 4); o += 4; }
	for (i = 0; i < MCCJIT_KGC_MAXARG; i++) { MCC_TRACE("br\n");
		int d = REGS + (MCCJIT_KGC_MAXARG - 1 - i) * 8;
		p[o++] = 0x8b; p[o++] = 0x45; p[o++] = (unsigned char)(8 + i * 4);
		p[o++] = 0x99;
		p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)d;
		p[o++] = 0x89; p[o++] = 0x54; p[o++] = 0x24; p[o++] = (unsigned char)(d + 4);
	}
	p[o++] = 0xc7; p[o++] = 0x04; p[o++] = 0x24;
	{ uint32_t s = (uint32_t)(uintptr_t)st; memcpy(p + o, &s, 4); o += 4; }
	p[o++] = 0x8d; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)REGS;
	p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = 4;
	p[o++] = 0xb8; { uint32_t t = (uint32_t)(uintptr_t)tick; memcpy(p + o, &t, 4); o += 4; }
	p[o++] = 0xff; p[o++] = 0xd0;
	p[o++] = 0x89; p[o++] = 0xec;
	p[o++] = 0x5d;
	p[o++] = 0xff; p[o++] = 0xe0;
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
	long threshold = mcc_env_num("MCC_JIT_HOT_CALLS", 1000);
	MccjitCounterState *st;
	void *stub;
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
	if (mcc_env_on("MCC_JIT_VERBOSE"))
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
	return mcc_env_on("MCC_JIT_LAZY");
}

static int mccjit_probe_exec_mem(void) { MCC_TRACE("enter\n");
#if defined(MCCJIT_ARM64)
	size_t page = host_pagesize();
	void *p = mmap(0, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	uint32_t code[2];
	int got;
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return 0; }
	code[0] = 0x52800b40u;
	code[1] = 0xd65f03c0u;
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
#if defined(MCCJIT_X64)
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
	if (!mccjit_feasible_flag && mcc_env_on("MCC_JIT_VERBOSE"))
		{ MCC_TRACE("br\n"); fprintf(stderr,
						"mccjit: executable-memory probe failed — JIT disabled, running "
						"AOT baseline\n"); }
}

static int mccjit_feasible(void) { MCC_TRACE("enter\n");
	if (MCC_DEV_ENV_ON("MCC_JIT_FORCE_INFEASIBLE"))
		{ MCC_TRACE("br\n"); return 0; }
	pthread_once(&mccjit_feasible_once, mccjit_feasible_probe);
	return mccjit_feasible_flag;
}

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
	uint64_t retired;
	pthread_mutex_t lock;
} mccjit_qsbr = {1, {0}, {0}, {{0}}, 0, 0, 0, 0, PTHREAD_MUTEX_INITIALIZER};

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

static void mccjit_qsbr_retire(void *ptr, size_t size) { MCC_TRACE("enter\n");
	pthread_mutex_lock(&mccjit_qsbr.lock);
	{
		uint64_t e = __atomic_add_fetch(&mccjit_qsbr.global, 1, __ATOMIC_ACQ_REL);
		mccjit_qsbr.retired++;
		if (mccjit_qsbr.nlimbo < MCCJIT_QSBR_LIMBO) { MCC_TRACE("br\n");
			mccjit_qsbr.limbo[mccjit_qsbr.nlimbo].ptr = ptr;
			mccjit_qsbr.limbo[mccjit_qsbr.nlimbo].size = size;
			mccjit_qsbr.limbo[mccjit_qsbr.nlimbo].epoch = e;
			mccjit_qsbr.nlimbo++;
		} else { MCC_TRACE("br\n");
			mccjit_qsbr.leaked++;
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
	mccjit_qsbr.retired = 0;
	for (i = 0; i < MCCJIT_QSBR_SLOTS; i++) { MCC_TRACE("br\n");
		mccjit_qsbr.used[i] = 0;
		mccjit_qsbr.local[i] = 0;
	}
	pthread_mutex_unlock(&mccjit_qsbr.lock);
}

/* Declared rather than reached through "mccgpu.h": that header is the shader
 * emitters as well as the device layer, and its own comment says it needs the
 * AST accessors and the ast_eval_slice width helpers already in the includer's
 * scope -- which is why mccast.c includes it mid-file rather than at the top.
 * This fragment only needs the device layer's one entry point, and a fragment
 * that does not compile standalone is what build/fragments-are-not-tus exists
 * to catch. */
void mcc_gpu_quiesce(void);

PUB_FUNC void mccjit_shutdown(void) { MCC_TRACE("enter\n");
	if (!mcc_env_flag("MCC_JIT_SHUTDOWN", 1))
		{ MCC_TRACE("br\n"); return; }
	mccjit_pool_shutdown();
	mccjit_qsbr_reclaim();
	/* After the pool join, never before: either order is safe under the lock
	 * plus mcc_gpu_closing, but joining first means no worker can be inside a
	 * dispatch when the device goes away.  Until this call existed the ordering
	 * was incidental -- ast_ladder_gpu_setup() registers its atexit teardown and
	 * mccjit_pool_start() registers this one, so the pair came out right only
	 * because boot_swap_async calls them in that order; the mccjit_kgc_reg path
	 * registers this handler independently and can invert it.  Quiescing here
	 * makes the order a property of the code rather than of the registration
	 * sequence.  The second quiesce is a no-op on both backends by construction:
	 * Metal releases nil and reads back zeroed fields, Vulkan guards on
	 * mcc_gpu.dev, which the first call cleared. */
	mcc_gpu_quiesce();
}

void mccjit_set_search_budget(unsigned long secs) { MCC_TRACE("enter\n");
	mccjit_search_budget_baked_s = secs;
}

#if MCC_HOST_WIN32
/* No-op ucrt invalid-parameter handler + installer, linked into PE embed-JIT
 * programs (and mcc's own -run). ucrt printf/scanf fastfail (0xC0000409) on an
 * invalid parameter; this no-op handler makes them tolerate it, as mingw's CRT
 * startup does and as the msvcrt AOT path already behaves. Compatible-signature
 * self-decl avoids a <stdlib.h> dependency here (in C on win wchar_t==unsigned
 * short and uintptr_t==unsigned long long). (T-win-50021) */
typedef void (*mccjit_iph_t)(const unsigned short *, const unsigned short *,
		                     const unsigned short *, unsigned int,
		                     unsigned long long);
extern mccjit_iph_t _set_invalid_parameter_handler(mccjit_iph_t);
static void mccjit_noop_iph(const unsigned short *a, const unsigned short *b,
			        const unsigned short *c, unsigned int d, unsigned long long e) { MCC_TRACE("enter\n");
	(void)a; (void)b; (void)c; (void)d; (void)e;
}
void mccjit_win_crt_compat(void) { MCC_TRACE("enter\n");
	_set_invalid_parameter_handler(mccjit_noop_iph);
}
#endif

void mccjit_set_always_gpu(int on) { MCC_TRACE("enter\n");
	const char *e = getenv("MCC_JIT_ALWAYS_GPU");
	if (e && *e)
		{ MCC_TRACE("br\n"); on = (*e != '0'); }
	if (on)
		{ MCC_TRACE("br\n"); ast_ladder_gpu_force(); }
}

void mccjit_boot_swap(void **slot, const void *blob, unsigned long len) { MCC_TRACE("enter\n");
	mcc_stats_env_init();
	if (!mccjit_feasible())
		{ MCC_TRACE("br\n"); return; }
	ast_ladder_gpu_setup();
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
	ast_ladder_gpu_setup();
	nw = mccjit_pool_start(workers);
	if (mccjit_lazy_enabled() && mccjit_lazy_install(slot, blob, len) == 0)
		{ MCC_TRACE("br\n"); return; }
	job = (nw > 0) ? mccjit_swap_job_new() : NULL;
	if (job) { MCC_TRACE("br\n");
		job->slot = slot;
		job->blob = blob;
		job->len = len;
		job->max_duration = max_duration;
		job->timed =
				(max_duration != 0) && (clock_gettime(CLOCK_MONOTONIC, &job->start) == 0);
		if (mccjit_pool_submit(mccjit_job_run_eager, job))
			{ MCC_TRACE("br\n"); return; }
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

void mccjit_embed_reset(void) { MCC_TRACE("enter\n");
	MccjitEmbedFn *e, *nx;
	int i;
	for (e = mccjit_embed_fns; e; e = nx) { MCC_TRACE("br\n");
		nx = e->next;
		mcc_free(e->name);
		mcc_free(e->blob);
		mcc_free(e);
	}
	mccjit_embed_fns = NULL;
	for (i = 0; i < mccjit_export_n; i++)
		{ MCC_TRACE("br\n"); mcc_free(mccjit_export_names[i]); }
	mcc_free(mccjit_export_names);
	mccjit_export_names = NULL;
	mccjit_export_n = mccjit_export_cap = 0;
}

void mccjit_embed_finalize(MCCState *s1) { MCC_TRACE("enter\n");
	MccjitEmbedFn *e, *nx;
	CString cs;
	int n = 0;
	int async = 0;
	if (!s1 || !(s1->embed_jit || s1->output_type == MCC_OUTPUT_MEMORY) ||
			!mccjit_embed_fns || mccjit_internal_compile)
		{ MCC_TRACE("br\n");
			if (s1 && s1->embed_jit && !mccjit_embed_fns && !mccjit_internal_compile)
				{ MCC_TRACE("br\n");
					static const char nobake[] =
							"mcc: warning: --embed-jit: no functions were JIT-baked, so the output "
							"carries no runtime JIT engine (baking needs -O1+ and is disabled by "
							"-g/-ftest-coverage)";
					if (s1->error_func) s1->error_func(s1->error_opaque, nobake);
					else { MCC_TRACE("br\n"); fflush(stdout); fprintf(stderr, "%s\n", nobake); fflush(stderr); } }
			return; }
	async = s1->jit_threads > 0;
	if (s1->output_type == MCC_OUTPUT_MEMORY) { MCC_TRACE("br\n");
		mcc_add_symbol(s1, "mccjit_boot_swap", (void *)mccjit_boot_swap);
		mcc_add_symbol(s1, "mccjit_set_search_budget", (void *)mccjit_set_search_budget);
		mcc_add_symbol(s1, "mccjit_set_always_gpu", (void *)mccjit_set_always_gpu);
		mcc_add_symbol(s1, "mccjit_boot_swap_async",
									 (void *)mccjit_boot_swap_async);
#if MCC_HOST_WIN32
		mcc_add_symbol(s1, "getenv", (void *)getenv);
		mcc_add_symbol(s1, "mccjit_win_crt_compat", (void *)mccjit_win_crt_compat);
#endif
	} else if (MCC_DEV_ENV_ON("MCC_JIT_EXPORT_INTERNALS")) { MCC_TRACE("br\n");
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
		int off = (int)section_add(data_section, e->len ? e->len : 1, MCC_PTR_SIZE);
		unsigned char *p = data_section->data + off;
		char blobname[256];
		if (e->len)
			{ MCC_TRACE("br\n"); memcpy(p, e->blob, e->len); }
		{
			int bi;
			for (bi = 0; bi < e->bind_n; bi++)
				{ MCC_TRACE("br\n"); put_elf_reloca(symtab_section, data_section,
															 (unsigned long)off + e->bind_off[bi], R_DATA_PTR,
															 e->bind_elfsym[bi], 0); }
		}
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
			cstr_printf(&cs, "extern void mccjit_set_search_budget(unsigned long);\n");
			cstr_printf(&cs, "extern void mccjit_set_always_gpu(int);\n");
#ifdef MCC_TARGET_PE
			/* PE embed-JIT programs link the ucrt api-sets (from the engine blob),
			 * whose CRT fastfails (0xC0000409) on an invalid parameter -- e.g. a
			 * printf format ucrt rejects -- instead of the lenient msvcrt behavior
			 * the AOT path relies on. Install a no-op invalid-parameter handler via
			 * the engine fn mccjit_win_crt_compat (so the ucrt import resolves with
			 * the engine's own imports, not as a late-added reference), exactly as
			 * mingw's own CRT startup does; before the MCC_JIT gate so it applies
			 * even at MCC_JIT=0. (T-win-50021) */
			cstr_printf(&cs, "extern void mccjit_win_crt_compat(void);\n");
#endif
			cstr_printf(
				&cs,
				"__attribute__((constructor)) static void __mccjit_boot_all(void){\n"
#ifdef MCC_TARGET_PE
				"mccjit_win_crt_compat();\n"
#endif
				"const char *__e = getenv(\"MCC_JIT\");\n"
				"int __on = __e ? (__e[0] != '0') : %d;\n"
				"int __i;\n"
				"if(!__on) return;\n"
				"mccjit_set_search_budget(%luUL);\n"
				"mccjit_set_always_gpu(%d);\n"
				"for(__i=0;__i<%d;__i++)\n",
				def_on, (unsigned long)s1->jit_max_duration,
				s1->jit_always_gpu ? 1 : 0, n);
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
	mccjit_embed_reset();
}

PUB_FUNC int mccjit_selftest(void) { MCC_TRACE("enter\n");
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

PUB_FUNC int mccjit_selftest_stage2(void) { MCC_TRACE("enter\n");
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

PUB_FUNC int mccjit_selftest_struct(void) { MCC_TRACE("enter\n");
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
	int nearmatch_on;
	int nearmatch;
	int nm_decided;
	int nm_benching;
	uint64_t nm_total;
	uint64_t nm_match;
	uint64_t nm_last_corr;
	int64_t *corr;
	uint64_t corr_n;
	uint64_t corr_cap;
	void *mx_variant;
	void *mx_baseline;
	uint32_t mx_ngp;
	uint32_t mx_nsse;
	uint32_t mx_nargs;
	uint32_t mx_argclass;
	int mx_ret_fp;
	int *mx_flag;
#if defined(MCCJIT_I386)
	void *mx_thunk;
	uint32_t mx_argsz;
#endif
	pthread_mutex_t lock;
} MccjitKgc;

static int mccjit_poison_min(void) { MCC_TRACE("enter\n");
	const char *e = MCC_DEV_ENV("MCC_JIT_POISON_MIN");
	if (e && e[0]) { MCC_TRACE("br\n");
		long v = strtol(e, NULL, 10);
		if (v > 0)
			{ MCC_TRACE("br\n"); return (int)v; }
	}
	return 8;
}

static int mccjit_poison_pct(void) { MCC_TRACE("enter\n");
	const char *e = MCC_DEV_ENV("MCC_JIT_POISON_PCT");
	if (e && e[0]) { MCC_TRACE("br\n");
		long v = strtol(e, NULL, 10);
		if (v > 0 && v <= 100)
			{ MCC_TRACE("br\n"); return (int)v; }
	}
	return 50;
}

static int mccjit_nearmatch_on(void) { MCC_TRACE("enter\n");
	return mcc_env_flag("MCC_JIT_NEARMATCH", 1);
}

#define MCCJIT_NEARMATCH_CORR_MAX ((uint64_t)64)
#define MCCJIT_NEARMATCH_WARMUP 64
#define MCCJIT_NEARMATCH_STABLE 64

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

static void mccjit_kgc_register(MccjitKgc *k);
static void mccjit_kgc_unregister(MccjitKgc *k);

static int mccjit_kgc_open(MccjitKgc *k, const char *path, uint64_t salt,
													 uint32_t arity) { MCC_TRACE("enter\n");
	uint64_t initcap = 64;
	size_t initbytes;
	memset(k, 0, sizeof *k);
	k->fd = -1;
	k->memoize_ok = 1;
	k->nearmatch_on = mccjit_nearmatch_on();
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
		mccjit_kgc_register(k);
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
			if (mccjit_kgc_map_shared(k, mccjit_kgc_bytes(peek.cap, arity)) == 0) { MCC_TRACE("br\n");
				valid = 1;
				if (mcc_stats_mask)
					{ MCC_TRACE("br\n"); mcc_stats_jit_kgc_warm((unsigned long)peek.count); }
			}
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
	mccjit_kgc_register(k);
	return 0;
}

static void mccjit_vput(unsigned char **p, uint64_t v) { MCC_TRACE("enter\n");
	while (v >= 0x80) { MCC_TRACE("br\n"); *(*p)++ = (unsigned char)(v | 0x80); v >>= 7; }
	*(*p)++ = (unsigned char)v;
}

static uint64_t mccjit_vget(const unsigned char **p, const unsigned char *end) { MCC_TRACE("enter\n");
	uint64_t v = 0;
	int shift = 0;
	while (*p < end) { MCC_TRACE("br\n");
		unsigned char c = *(*p)++;
		v |= (uint64_t)(c & 0x7f) << shift;
		if (!(c & 0x80))
			{ MCC_TRACE("br\n"); break; }
		shift += 7;
	}
	return v;
}

static uint64_t mccjit_zz(int64_t x) { MCC_TRACE("enter\n");
	return ((uint64_t)x << 1) ^ (uint64_t)(x >> 63);
}

static int64_t mccjit_unzz(uint64_t v) { MCC_TRACE("enter\n");
	return (int64_t)(v >> 1) ^ -(int64_t)(v & 1);
}

static int mccjit_kgc_encode_compressed(const MccjitKgc *k, unsigned char **out,
																				size_t *outlen) { MCC_TRACE("enter\n");
	uint64_t count, i;
	uint32_t arity, j;
	unsigned char *buf, *p;
	int64_t prev[MCCJIT_KGC_ARITY];
	*out = NULL;
	*outlen = 0;
	if (!k || !k->hdr || !k->tuples)
		{ MCC_TRACE("br\n"); return -1; }
	count = k->hdr->count;
	arity = k->arity;
	if (count == 0)
		{ MCC_TRACE("br\n"); return 0; }
	buf = mcc_malloc((size_t)32 + (size_t)count * arity * 10);
	if (!buf)
		{ MCC_TRACE("br\n"); return -1; }
	p = buf;
	mccjit_vput(&p, count);
	mccjit_vput(&p, arity);
	for (j = 0; j < arity; j++)
		{ MCC_TRACE("br\n"); prev[j] = 0; }
	for (i = 0; i < count; i++) { MCC_TRACE("br\n");
		const int64_t *t = k->tuples + i * arity;
		for (j = 0; j < arity; j++) { MCC_TRACE("br\n");
			mccjit_vput(&p, mccjit_zz(t[j] - prev[j]));
			prev[j] = t[j];
		}
	}
	*out = buf;
	*outlen = (size_t)(p - buf);
	return 0;
}

static int64_t mccjit_kgc_decode_compressed(const unsigned char *buf, size_t len,
																						int64_t *dst, uint64_t maxtuples,
																						uint32_t *parity) { MCC_TRACE("enter\n");
	const unsigned char *p = buf, *end = buf + len;
	uint64_t count, i;
	uint32_t arity, j;
	int64_t prev[MCCJIT_KGC_ARITY];
	if (!buf || len == 0)
		{ MCC_TRACE("br\n"); return -1; }
	count = mccjit_vget(&p, end);
	arity = (uint32_t)mccjit_vget(&p, end);
	if (arity == 0 || arity > MCCJIT_KGC_ARITY || count > maxtuples)
		{ MCC_TRACE("br\n"); return -1; }
	for (j = 0; j < arity; j++)
		{ MCC_TRACE("br\n"); prev[j] = 0; }
	for (i = 0; i < count; i++) { MCC_TRACE("br\n");
		for (j = 0; j < arity; j++) { MCC_TRACE("br\n");
			if (p >= end)
				{ MCC_TRACE("br\n"); return -1; }
			prev[j] += mccjit_unzz(mccjit_vget(&p, end));
			dst[i * arity + j] = prev[j];
		}
	}
	if (parity)
		{ MCC_TRACE("br\n"); *parity = arity; }
	return (int64_t)count;
}

static void mccjit_kgc_flush_compressed(MccjitKgc *k) { MCC_TRACE("enter\n");
	static unsigned long seq;
	const char *dir = MCC_DEV_ENV("MCC_JIT_KGC_SAVE");
	unsigned char *buf;
	size_t n;
	if (!k || !k->hdr || k->hdr->count == 0)
		{ MCC_TRACE("br\n"); return; }
	if (!mcc_stats_mask && !(dir && dir[0]))
		{ MCC_TRACE("br\n"); return; }
	if (mccjit_kgc_encode_compressed(k, &buf, &n) != 0 || !buf)
		{ MCC_TRACE("br\n"); return; }
	if (MCC_DEV_ENV_ON("MCC_JIT_KGC_SELFTEST")) { MCC_TRACE("br\n");
		int64_t *chk = mcc_malloc((size_t)k->hdr->count * k->arity * sizeof(int64_t));
		uint32_t da = 0;
		int64_t dc = chk ? mccjit_kgc_decode_compressed(buf, n, chk, k->hdr->count, &da)
										 : -1;
		if (!chk || dc != (int64_t)k->hdr->count || da != k->arity ||
				memcmp(chk, k->tuples,
							 (size_t)k->hdr->count * k->arity * sizeof(int64_t)) != 0)
			{ MCC_TRACE("br\n"); fprintf(stderr, "mccjit: KGC compressed round-trip MISMATCH\n"); }
		mcc_free(chk);
	}
	if (mcc_stats_mask)
		{ MCC_TRACE("br\n"); mcc_stats_jit_memo(
				(unsigned long)k->hdr->count,
				(unsigned long)((size_t)k->hdr->count * k->arity * sizeof(int64_t)),
				(unsigned long)n); }
	if (dir && dir[0]) { MCC_TRACE("br\n");
		char path[1024];
		int fd;
		snprintf(path, sizeof path, "%s/kgc-%016llx-%u-%lu.z", dir,
						 (unsigned long long)k->hdr->salt, (unsigned)k->arity, seq++);
		fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd >= 0) { MCC_TRACE("br\n");
			ssize_t w = write(fd, buf, n);
			(void)w;
			close(fd);
		}
	}
	mcc_free(buf);
}

typedef struct MccjitKgcReg {
	MccjitKgc *k;
	int flushed;
	struct MccjitKgcReg *next;
} MccjitKgcReg;

static MccjitKgcReg *mccjit_kgc_reg_head;
static pthread_mutex_t mccjit_kgc_reg_lock = PTHREAD_MUTEX_INITIALIZER;
static int mccjit_kgc_reg_hooked;

static void mccjit_kgc_flush_all(void) { MCC_TRACE("enter\n");
	MccjitKgcReg *r;
	pthread_mutex_lock(&mccjit_kgc_reg_lock);
	for (r = mccjit_kgc_reg_head; r; r = r->next) { MCC_TRACE("br\n");
		if (r->flushed)
			{ MCC_TRACE("br\n"); continue; }
		r->flushed = 1;
		mccjit_kgc_flush_compressed(r->k);
	}
	pthread_mutex_unlock(&mccjit_kgc_reg_lock);
}

static void mccjit_kgc_register(MccjitKgc *k) { MCC_TRACE("enter\n");
	MccjitKgcReg *r;
	if (!k)
		{ MCC_TRACE("br\n"); return; }
	pthread_mutex_lock(&mccjit_kgc_reg_lock);
	if (!mccjit_kgc_reg_hooked) { MCC_TRACE("br\n");
		/* The cache flush still runs from atexit: it only writes files and is
		 * order-independent. The SHUTDOWN that used to be registered here is
		 * gone -- it is main's job now (src/mccrt.h), and registering it from
		 * this path is precisely what could invert the pool-then-device order. */
		atexit(mccjit_kgc_flush_all);
		mcc_stats_set_flush_hook(mccjit_kgc_flush_all);
		mccjit_kgc_reg_hooked = 1;
	}
	r = mcc_malloc(sizeof *r);
	if (r) { MCC_TRACE("br\n");
		r->k = k;
		r->flushed = 0;
		r->next = mccjit_kgc_reg_head;
		mccjit_kgc_reg_head = r;
	}
	pthread_mutex_unlock(&mccjit_kgc_reg_lock);
}

static void mccjit_kgc_unregister(MccjitKgc *k) { MCC_TRACE("enter\n");
	MccjitKgcReg **pp;
	pthread_mutex_lock(&mccjit_kgc_reg_lock);
	for (pp = &mccjit_kgc_reg_head; *pp; pp = &(*pp)->next) { MCC_TRACE("br\n");
		if ((*pp)->k == k) { MCC_TRACE("br\n");
			MccjitKgcReg *d = *pp;
			*pp = d->next;
			mcc_free(d);
			break;
		}
	}
	pthread_mutex_unlock(&mccjit_kgc_reg_lock);
}

static void mccjit_kgc_close(MccjitKgc *k) { MCC_TRACE("enter\n");
	if (!k)
		{ MCC_TRACE("br\n"); return; }
	mccjit_kgc_flush_compressed(k);
	mccjit_kgc_unregister(k);
	if (k->map && k->map != MAP_FAILED) { MCC_TRACE("br\n");
		if (!k->anon)
			{ MCC_TRACE("br\n"); msync(k->map, k->map_len, MS_SYNC); }
		munmap(k->map, k->map_len);
	}
	if (k->fd >= 0)
		{ MCC_TRACE("br\n"); close(k->fd); }
	mcc_free(k->path);
	mcc_free(k->corr);
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

static int mccjit_nearmatch_active(const MccjitKgc *k) { MCC_TRACE("enter\n");
	return k->nearmatch_on && k->memoize_ok;
}

static uint64_t mccjit_corr_lower(const MccjitKgc *k, const int64_t *tuple,
																	int *found) { MCC_TRACE("enter\n");
	uint64_t lo = 0, hi = k->corr_n, stride = (uint64_t)k->arity + 1;
	*found = 0;
	while (lo < hi) { MCC_TRACE("br\n");
		uint64_t mid = lo + (hi - lo) / 2;
		int c = mccjit_kgc_cmp(k->corr + mid * stride, tuple, k->arity);
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

static int64_t *mccjit_corr_find(MccjitKgc *k, const int64_t *tuple) { MCC_TRACE("enter\n");
	int found;
	uint64_t at;
	if (!k->corr)
		{ MCC_TRACE("br\n"); return NULL; }
	at = mccjit_corr_lower(k, tuple, &found);
	if (!found)
		{ MCC_TRACE("br\n"); return NULL; }
	return k->corr + at * ((uint64_t)k->arity + 1) + k->arity;
}

static int mccjit_corr_insert(MccjitKgc *k, const int64_t *tuple, int64_t out) { MCC_TRACE("enter\n");
	uint64_t stride = (uint64_t)k->arity + 1;
	int found;
	uint64_t at;
	int64_t *dst;
	if (k->corr) { MCC_TRACE("br\n");
		at = mccjit_corr_lower(k, tuple, &found);
		if (found) { MCC_TRACE("br\n");
			k->corr[at * stride + k->arity] = out;
			return 0;
		}
	} else { MCC_TRACE("br\n");
		at = 0;
	}
	if (k->corr_n >= MCCJIT_NEARMATCH_CORR_MAX)
		{ MCC_TRACE("br\n"); return 0; }
	if (k->corr_n + 1 > k->corr_cap) { MCC_TRACE("br\n");
		uint64_t ncap = k->corr_cap ? k->corr_cap * 2 : 32;
		int64_t *nb = mcc_mallocz((size_t)ncap * stride * sizeof(int64_t));
		if (!nb)
			{ MCC_TRACE("br\n"); return 0; }
		if (k->corr) { MCC_TRACE("br\n");
			memcpy(nb, k->corr, (size_t)k->corr_n * stride * sizeof(int64_t));
			mcc_free(k->corr);
		}
		k->corr = nb;
		k->corr_cap = ncap;
	}
	dst = k->corr + at * stride;
	if (at < k->corr_n)
		{ MCC_TRACE("br\n"); memmove(dst + stride, dst,
						(size_t)(k->corr_n - at) * stride * sizeof(int64_t)); }
	memcpy(dst, tuple, (size_t)k->arity * sizeof(int64_t));
	dst[k->arity] = out;
	k->corr_n++;
	return 1;
}

static int mccjit_bench_pair(void *cand, void *incumbent, const int64_t *tuples,
														 uint32_t ntuples, uint32_t nargs, int wide, int fp);

static uint32_t mccjit_nearmatch_build_sample(MccjitKgc *k, int64_t *sample,
																							uint32_t cap) { MCC_TRACE("enter\n");
	uint32_t n = 0, i;
	for (i = 0; i < k->hdr->count && n < cap; i++) { MCC_TRACE("br\n");
		uint32_t j;
		for (j = 0; j < k->arity; j++)
			{ MCC_TRACE("br\n"); sample[n * MCCJIT_KGC_ARITY + j] = k->tuples[i * k->arity + j]; }
		n++;
	}
	return n;
}

static void mccjit_nearmatch_decide(MccjitKgc *k, void *variant, void *baseline,
																		int fp, int benchable) { MCC_TRACE("enter\n");
	int64_t *sample;
	uint32_t n, cap = 64;
	int win;
	if (k->nm_decided)
		{ MCC_TRACE("br\n"); return; }
	if (k->corr_n >= MCCJIT_NEARMATCH_CORR_MAX) { MCC_TRACE("br\n");
		k->nm_decided = 1;
		if (mcc_stats_mask && !k->poisoned)
			{ MCC_TRACE("br\n"); mcc_stats_jit_poison(); }
		k->poisoned = 1;
		return;
	}
	if (k->corr_n == 0)
		{ MCC_TRACE("br\n"); return; }
	if (k->nm_total < MCCJIT_NEARMATCH_WARMUP ||
			k->nm_total - k->nm_last_corr < MCCJIT_NEARMATCH_STABLE)
		{ MCC_TRACE("br\n"); return; }
	if (k->nm_benching)
		{ MCC_TRACE("br\n"); return; }
	if (!benchable) { MCC_TRACE("br\n");
		k->nm_decided = 1;
		if (mcc_stats_mask && !k->poisoned)
			{ MCC_TRACE("br\n"); mcc_stats_jit_poison(); }
		k->poisoned = 1;
		return;
	}
	if (!variant || !baseline || k->hdr->count == 0) { MCC_TRACE("br\n");
		k->nm_decided = 1;
		if (mcc_stats_mask && !k->poisoned)
			{ MCC_TRACE("br\n"); mcc_stats_jit_poison(); }
		k->poisoned = 1;
		return;
	}
	sample = mcc_mallocz((size_t)cap * MCCJIT_KGC_ARITY * sizeof(int64_t));
	if (!sample)
		{ MCC_TRACE("br\n"); return; }
	n = mccjit_nearmatch_build_sample(k, sample, cap);
	k->nm_benching = 1;
	pthread_mutex_unlock(&k->lock);
	win = mccjit_bench_pair(variant, baseline, sample, n, k->arity, k->ret_wide, fp);
	pthread_mutex_lock(&k->lock);
	mcc_free(sample);
	if (!k->nm_decided) { MCC_TRACE("br\n");
		k->nm_decided = 1;
		if (win) { MCC_TRACE("br\n");
			k->nearmatch = 1;
			if (mcc_stats_mask)
				{ MCC_TRACE("br\n"); mcc_stats_jit_nearmatch(); }
		} else { MCC_TRACE("br\n");
			if (mcc_stats_mask && !k->poisoned)
				{ MCC_TRACE("br\n"); mcc_stats_jit_poison(); }
			k->poisoned = 1;
		}
	}
	k->nm_benching = 0;
}

static int mccjit_nearmatch_miss(MccjitKgc *k, const int64_t *tuple, int64_t bval) { MCC_TRACE("enter\n");
	if (!mccjit_nearmatch_active(k))
		{ MCC_TRACE("br\n"); return 0; }
	if (k->corr_n < MCCJIT_NEARMATCH_CORR_MAX) { MCC_TRACE("br\n");
		if (mccjit_corr_insert(k, tuple, bval)) { MCC_TRACE("br\n");
			k->nm_last_corr = k->nm_total;
			if (mcc_stats_mask)
				{ MCC_TRACE("br\n"); mcc_stats_jit_kgc_correction(); }
		}
	}
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
	if (k->poisoned) { MCC_TRACE("br\n");
		pthread_mutex_unlock(&k->lock);
		return (int64_t)bf((int)x);
	}
	if (mccjit_nearmatch_active(k)) { MCC_TRACE("br\n");
		int64_t *co;
		k->nm_total++;
		mccjit_nearmatch_decide(k, variant, baseline, 0, 1);
		if (k->poisoned) { MCC_TRACE("br\n");
			pthread_mutex_unlock(&k->lock);
			return (int64_t)bf((int)x);
		}
		co = mccjit_corr_find(k, tuple);
		if (co) { MCC_TRACE("br\n");
			int64_t v = *co;
			pthread_mutex_unlock(&k->lock);
			return v;
		}
	}
	if (k->memoize_ok && mccjit_kgc_contains(k, tuple)) { MCC_TRACE("br\n");
		k->nm_match++;
		pthread_mutex_unlock(&k->lock);
		return (int64_t)vf((int)x);
	}
	bval = (int64_t)bf((int)x);
	mccjit_boundary_hit = 0;
	vval = (int64_t)vf((int)x);
	if (mcc_stats_mask && mcc_env_on("MCC_JIT_BLIND_RETYPE"))
		{ MCC_TRACE("br\n"); mcc_stats_jit_bguard((vval != bval) == (mccjit_boundary_hit != 0)); }
	if (vval == bval) { MCC_TRACE("br\n");
		k->hits++;
		k->nm_match++;
		if (k->memoize_ok)
			{ MCC_TRACE("br\n"); mccjit_kgc_insert(k, tuple); }
		pthread_mutex_unlock(&k->lock);
		return bval;
	}
	k->misses++;
	if (k->misses == 1 && mcc_stats_mask) { MCC_TRACE("br\n"); mcc_stats_jit_blind_dirty(); }
	if (mcc_stats_mask) { MCC_TRACE("br\n");
		int fits = 1;
		uint32_t j;
		for (j = 0; j < MCCJIT_KGC_ARITY; j++)
			if ((int64_t)(int32_t)tuple[j] != tuple[j])
				{ MCC_TRACE("br\n"); fits = 0; break; }
		mcc_stats_jit_blind_div_range(fits);
	}
	mccjit_nearmatch_miss(k, tuple, bval);
	pthread_mutex_unlock(&k->lock);
	if (flagged)
		{ MCC_TRACE("br\n"); *flagged = 1; }
	return bval;
}

typedef intptr_t mccjit_argw;
static int64_t mccjit_invoke_raw(void *fn, const int64_t *a, uint32_t n, int wide) { MCC_TRACE("enter\n");
	switch (n) { MCC_TRACE("br\n");
	case 1:
		return wide ? (int64_t)((long long (*)(mccjit_argw))fn)(a[0])
								: (int64_t)((int (*)(mccjit_argw))fn)(a[0]);
	case 2:
		return wide ? (int64_t)((long long (*)(mccjit_argw, mccjit_argw))fn)(a[0], a[1])
								: (int64_t)((int (*)(mccjit_argw, mccjit_argw))fn)(a[0], a[1]);
	case 3:
		return wide ? (int64_t)((long long (*)(mccjit_argw, mccjit_argw, mccjit_argw))fn)(a[0], a[1], a[2])
								: (int64_t)((int (*)(mccjit_argw, mccjit_argw, mccjit_argw))fn)(a[0], a[1], a[2]);
	case 4:
		return wide
							 ? (int64_t)((long long (*)(mccjit_argw, mccjit_argw, mccjit_argw, mccjit_argw))fn)(a[0], a[1], a[2],
																																 a[3])
							 : (int64_t)((int (*)(mccjit_argw, mccjit_argw, mccjit_argw, mccjit_argw))fn)(a[0], a[1], a[2],
																															 a[3]);
	case 5:
		return wide ? (int64_t)((long long (*)(mccjit_argw, mccjit_argw, mccjit_argw, mccjit_argw, mccjit_argw))fn)(
											a[0], a[1], a[2], a[3], a[4])
								: (int64_t)((int (*)(mccjit_argw, mccjit_argw, mccjit_argw, mccjit_argw, mccjit_argw))fn)(
											a[0], a[1], a[2], a[3], a[4]);
	case 6:
		return wide ? (int64_t)((long long (*)(mccjit_argw, mccjit_argw, mccjit_argw, mccjit_argw, mccjit_argw, mccjit_argw))fn)(
											a[0], a[1], a[2], a[3], a[4], a[5])
								: (int64_t)((int (*)(mccjit_argw, mccjit_argw, mccjit_argw, mccjit_argw, mccjit_argw, mccjit_argw))fn)(
											a[0], a[1], a[2], a[3], a[4], a[5]);
	default:
		return 0;
	}
}

static int64_t mccjit_invoke(void *fn, const int64_t *a, uint32_t n, int wide) { MCC_TRACE("enter\n");
	int64_t r = mccjit_invoke_raw(fn, a, n, wide);
	if (wide)
		{ MCC_TRACE("br\n"); return r; }
	return (int64_t)(uint32_t)r;
}

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
	const char *e = MCC_DEV_ENV("MCC_JIT_BENCH_ITERS");
	if (e && e[0]) { MCC_TRACE("br\n");
		long v = strtol(e, NULL, 10);
		if (v > 0)
			{ MCC_TRACE("br\n"); return v; }
	}
	return 100000;
}

static int mccjit_bench_margin_pct(void) { MCC_TRACE("enter\n");
	const char *e = MCC_DEV_ENV("MCC_JIT_BENCH_MARGIN_PCT");
	if (e && e[0]) { MCC_TRACE("br\n");
		long v = strtol(e, NULL, 10);
		if (v >= 0 && v <= 100)
			{ MCC_TRACE("br\n"); return (int)v; }
	}
	return 6;
}

static int mccjit_bench_rounds(void) { MCC_TRACE("enter\n");
	const char *e = MCC_DEV_ENV("MCC_JIT_BENCH_ROUNDS");
	if (e && e[0]) { MCC_TRACE("br\n");
		long v = strtol(e, NULL, 10);
		if (v >= 1 && v <= 1024)
			{ MCC_TRACE("br\n"); return (int)v; }
	}
	return 3;
}

static double mccjit_ts_delta(const struct timespec *a,
															const struct timespec *b) { MCC_TRACE("enter\n");
	return (double)(b->tv_sec - a->tv_sec) +
				 (double)(b->tv_nsec - a->tv_nsec) / 1000000000.0;
}

static void mccjit_bench_run_pair(void *cand, void *incumbent,
																	const int64_t *tuples, uint32_t ntuples,
																	uint32_t nargs, int wide, uint32_t reps,
																	double *cand_s, double *inc_s,
																	int64_t *sink_out, int fp) { MCC_TRACE("enter\n");
	int64_t sink = 0;
	double c = 0.0, ic = 0.0;
	uint32_t r, i;
	struct timespec t0, t1, t2;
	*cand_s = *inc_s = 1e300;
	for (r = 0; r < reps; r++) { MCC_TRACE("br\n");
		if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0)
			{ MCC_TRACE("br\n"); return; }
		for (i = 0; i < ntuples; i++) { MCC_TRACE("br\n");
			if (fp) { MCC_TRACE("br\n");
				double d = mccjit_invoke_fp(cand,
						(const double *)(tuples + (size_t)i * MCCJIT_KGC_ARITY), nargs);
				int64_t b; memcpy(&b, &d, sizeof b); sink += b;
			} else { MCC_TRACE("br\n");
				sink += mccjit_invoke_raw(cand, tuples + (size_t)i * MCCJIT_KGC_ARITY, nargs, wide);
			}
		}
		if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0)
			{ MCC_TRACE("br\n"); return; }
		for (i = 0; i < ntuples; i++) { MCC_TRACE("br\n");
			if (fp) { MCC_TRACE("br\n");
				double d = mccjit_invoke_fp(incumbent,
						(const double *)(tuples + (size_t)i * MCCJIT_KGC_ARITY), nargs);
				int64_t b; memcpy(&b, &d, sizeof b); sink += b;
			} else { MCC_TRACE("br\n");
				sink += mccjit_invoke_raw(incumbent, tuples + (size_t)i * MCCJIT_KGC_ARITY, nargs, wide);
			}
		}
		if (clock_gettime(CLOCK_MONOTONIC, &t2) != 0)
			{ MCC_TRACE("br\n"); return; }
		c += mccjit_ts_delta(&t0, &t1);
		ic += mccjit_ts_delta(&t1, &t2);
	}
	*sink_out ^= sink;
	*cand_s = c;
	*inc_s = ic;
}

#define MCCJIT_BENCH_MAXCORES 16

static int mccjit_bench_cores(void) { MCC_TRACE("enter\n");
	const char *e = MCC_DEV_ENV("MCC_JIT_BENCH_CORES");
	long n = 0;
	if (e && e[0]) { MCC_TRACE("br\n");
		n = strtol(e, NULL, 10);
	} else { MCC_TRACE("br\n");
#if defined _SC_NPROCESSORS_ONLN
		n = sysconf(_SC_NPROCESSORS_ONLN);
#endif
	}
	if (n < 1)
		{ MCC_TRACE("br\n"); n = 1; }
	if (n > MCCJIT_BENCH_MAXCORES)
		{ MCC_TRACE("br\n"); n = MCCJIT_BENCH_MAXCORES; }
	return (int)n;
}

typedef struct MccjitBenchSib {
	void *cand, *incumbent;
	const int64_t *tuples;
	uint32_t ntuples, nargs, reps;
	int wide, rounds, margin, fp;
	double cb, ib;
	int64_t sink;
	int verdict;
} MccjitBenchSib;

static void mccjit_bench_sibling_run(MccjitBenchSib *w) { MCC_TRACE("enter\n");
	double cb = 1e300, ib = 1e300;
	int k;
	w->sink = 0;
	for (k = 0; k < w->rounds; k++) { MCC_TRACE("br\n");
		double c, i2;
		mccjit_bench_run_pair(w->cand, w->incumbent, w->tuples, w->ntuples, w->nargs,
													w->wide, w->reps, &c, &i2, &w->sink, w->fp);
		if (c < cb)
			{ MCC_TRACE("br\n"); cb = c; }
		if (i2 < ib)
			{ MCC_TRACE("br\n"); ib = i2; }
	}
	w->cb = cb;
	w->ib = ib;
	w->verdict = cb * (100.0 + (double)w->margin) < ib * 100.0;
}

static void *mccjit_bench_sibling_thread(void *arg) { MCC_TRACE("enter\n");
	mccjit_bench_sibling_run((MccjitBenchSib *)arg);
	return NULL;
}

static int mccjit_bench_pair(void *cand, void *incumbent, const int64_t *tuples,
														 uint32_t ntuples, uint32_t nargs, int wide, int fp) { MCC_TRACE("enter\n");
	MccjitBenchSib sib[MCCJIT_BENCH_MAXCORES];
	pthread_t th[MCCJIT_BENCH_MAXCORES];
	char started[MCCJIT_BENCH_MAXCORES];
	uint32_t reps;
	long iters;
	int margin, rounds, cores, i, votes = 0;
	int64_t sinkacc = 0;
	if (!cand || !incumbent || !tuples || ntuples == 0 || nargs == 0)
		{ MCC_TRACE("br\n"); return 1; }
	iters = mccjit_bench_iters();
	margin = mccjit_bench_margin_pct();
	rounds = mccjit_bench_rounds();
	reps = (uint32_t)(iters / (long)ntuples);
	if (reps < 1)
		{ MCC_TRACE("br\n"); reps = 1; }
	cores = mccjit_bench_cores();
	for (i = 0; i < cores; i++) { MCC_TRACE("br\n");
		sib[i].cand = cand;
		sib[i].incumbent = incumbent;
		sib[i].tuples = tuples;
		sib[i].ntuples = ntuples;
		sib[i].nargs = nargs;
		sib[i].reps = reps;
		sib[i].wide = wide;
		sib[i].rounds = rounds;
		sib[i].margin = margin;
		sib[i].fp = fp;
		sib[i].verdict = 0;
		sib[i].sink = 0;
		started[i] = 0;
	}
	for (i = 1; i < cores; i++) { MCC_TRACE("br\n");
		if (pthread_create(&th[i], NULL, mccjit_bench_sibling_thread, &sib[i]) == 0)
			{ MCC_TRACE("br\n"); started[i] = 1; }
		else
			{ MCC_TRACE("br\n"); mccjit_bench_sibling_run(&sib[i]); }
	}
	mccjit_bench_sibling_run(&sib[0]);
	for (i = 1; i < cores; i++)
		{ MCC_TRACE("br\n"); if (started[i]) { MCC_TRACE("br\n"); pthread_join(th[i], NULL); } }
	for (i = 0; i < cores; i++) { MCC_TRACE("br\n");
		votes += sib[i].verdict;
		sinkacc ^= sib[i].sink;
	}
	mccjit_bench_sink ^= sinkacc;
	return votes * 2 > cores;
}

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
	return mccjit_bench_pair(cand, incumbent, tuples, nt, nargs, wide, 0);
}

/* -- T-mac-30295: N-way empirical wall-clock optimization selection (JIT) --
 * The JIT optimizes a live program with no byte-identity constraint, so wall
 * clock is the correct signal for choosing AMONG equivalent optimization
 * parameterizations of a slice (AOT keeps its deterministic tick budget --
 * tools/opt-search-determinism.py). These helpers time already-built candidate
 * variants over the profiled live-in tuples, rank them fastest-first, and pick
 * the fastest beyond a significance margin, self-suppressing when the host is
 * too busy to time reliably. Nuance: DETAILS.md#t-mac-30295-research-and-slice-plan. */

static int mccjit_host_too_busy(void) { MCC_TRACE("enter\n");
	long force = mcc_env_num("MCC_JIT_BENCH_FORCE_BUSY", -1);
	double la;
	int ncpu;
	if (force >= 0)
		{ MCC_TRACE("br\n"); return force != 0; }
	la = host_loadavg();
	if (la < 0.0)
		{ MCC_TRACE("br\n"); return 0; }
	ncpu = host_nproc();
	if (ncpu < 1)
		{ MCC_TRACE("br\n"); ncpu = 1; }
	return la > 0.90 * (double)ncpu;
}

static double mccjit_bench_time_one(void *fn, const int64_t *tuples,
																		uint32_t ntuples, uint32_t nargs, int wide,
																		uint32_t reps, int fp, int64_t *sink_out) { MCC_TRACE("enter\n");
	int64_t sink = 0;
	double best = 1e300;
	int rounds = mccjit_bench_rounds(), rd;
	uint32_t r, i;
	struct timespec t0;
	for (rd = 0; rd < rounds; rd++) { MCC_TRACE("br\n");
		double d;
		if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0)
			{ MCC_TRACE("br\n"); break; }
		for (r = 0; r < reps; r++) { MCC_TRACE("br\n");
			for (i = 0; i < ntuples; i++) { MCC_TRACE("br\n");
				if (fp) { MCC_TRACE("br\n");
					double dv = mccjit_invoke_fp(fn,
							(const double *)(tuples + (size_t)i * MCCJIT_KGC_ARITY), nargs);
					int64_t b;
					memcpy(&b, &dv, sizeof b);
					sink += b;
				} else { MCC_TRACE("br\n");
					sink += mccjit_invoke_raw(fn,
							tuples + (size_t)i * MCCJIT_KGC_ARITY, nargs, wide);
				}
			}
		}
		d = mccjit_elapsed(&t0);
		if (d < best)
			{ MCC_TRACE("br\n"); best = d; }
	}
	*sink_out ^= sink;
	return best;
}

/* Time each candidate over the tuples (best-of-rounds min), then insertion-sort
 * order[]/secs[] ascending by seconds. Returns the count ranked, or -1 when the
 * host is too busy to time (caller falls back to the deterministic static-cost
 * ranking). order[]/secs[] must have room for ncand entries. */
static int mccjit_bench_rank_n(void *const *cands, int ncand,
															 const int64_t *tuples, uint32_t ntuples,
															 uint32_t nargs, int wide, int fp,
															 int *order, double *secs) { MCC_TRACE("enter\n");
	long iters = mccjit_bench_iters();
	int64_t sink = 0;
	uint32_t reps;
	int cnt = 0, i, j;
	if (!cands || ncand <= 0 || !tuples || ntuples == 0 || nargs == 0 || !order || !secs)
		{ MCC_TRACE("br\n"); return -1; }
	if (mccjit_host_too_busy())
		{ MCC_TRACE("br\n"); return -1; }
	reps = (uint32_t)(iters / (long)ntuples);
	if (reps < 1)
		{ MCC_TRACE("br\n"); reps = 1; }
	for (i = 0; i < ncand; i++) { MCC_TRACE("br\n");
		if (!cands[i])
			{ MCC_TRACE("br\n"); continue; }
		order[cnt] = i;
		secs[cnt] = mccjit_bench_time_one(cands[i], tuples, ntuples, nargs, wide,
																			reps, fp, &sink);
		cnt++;
	}
	mccjit_bench_sink ^= sink;
	for (i = 1; i < cnt; i++) { MCC_TRACE("br\n");
		int oi = order[i];
		double os = secs[i];
		j = i - 1;
		while (j >= 0 && secs[j] > os) { MCC_TRACE("br\n");
			order[j + 1] = order[j];
			secs[j + 1] = secs[j];
			j--;
		}
		order[j + 1] = oi;
		secs[j + 1] = os;
	}
	return cnt;
}

/* Given ranked order[]/secs[] and the index of the reference (incumbent)
 * candidate, return the fastest candidate index iff it beats the reference by
 * more than MCC_JIT_BENCH_MARGIN_PCT (the adoption-side significance gate);
 * otherwise keep the reference. Stable: sub-margin noise never flips the choice. */
static int mccjit_bench_pick_significant(const int *order, const double *secs,
																				 int cnt, int ref) { MCC_TRACE("enter\n");
	int margin = mccjit_bench_margin_pct();
	double ref_secs = -1.0;
	int k;
	if (cnt <= 0 || !order || !secs)
		{ MCC_TRACE("br\n"); return ref; }
	for (k = 0; k < cnt; k++) { MCC_TRACE("br\n");
		if (order[k] == ref)
			{ MCC_TRACE("br\n"); ref_secs = secs[k]; break; }
	}
	if (ref_secs < 0.0)
		{ MCC_TRACE("br\n"); return ref; }
	if (order[0] == ref)
		{ MCC_TRACE("br\n"); return ref; }
	if (secs[0] * (100.0 + (double)margin) < ref_secs * 100.0)
		{ MCC_TRACE("br\n"); return order[0]; }
	return ref;
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
	if (mccjit_nearmatch_active(k)) { MCC_TRACE("br\n");
		int64_t *co;
		k->nm_total++;
		mccjit_nearmatch_decide(k, variant, baseline, 0, 1);
		if (k->poisoned) { MCC_TRACE("br\n");
			pthread_mutex_unlock(&k->lock);
			return mccjit_invoke(baseline, argv, nargs, wide);
		}
		co = mccjit_corr_find(k, tuple);
		if (co) { MCC_TRACE("br\n");
			int64_t v = *co;
			pthread_mutex_unlock(&k->lock);
			return v;
		}
	}
	if (k->memoize_ok && mccjit_kgc_contains(k, tuple)) { MCC_TRACE("br\n");
		k->nm_match++;
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke(variant, argv, nargs, wide);
	}
	bval = mccjit_invoke(baseline, argv, nargs, wide);
	mccjit_boundary_hit = 0;
	vval = mccjit_invoke(variant, argv, nargs, wide);
	if (mcc_stats_mask && mcc_env_on("MCC_JIT_BLIND_RETYPE"))
		{ MCC_TRACE("br\n"); mcc_stats_jit_bguard((vval != bval) == (mccjit_boundary_hit != 0)); }
	if (vval == bval) { MCC_TRACE("br\n");
		k->hits++;
		k->nm_match++;
		if (mcc_stats_mask)
			{ MCC_TRACE("br\n"); mcc_stats_jit_kgc_hit(); }
		if (k->memoize_ok)
			{ MCC_TRACE("br\n"); mccjit_kgc_insert(k, tuple); }
		pthread_mutex_unlock(&k->lock);
		return bval;
	}
	k->misses++;
	if (k->misses == 1 && mcc_stats_mask) { MCC_TRACE("br\n"); mcc_stats_jit_blind_dirty(); }
	if (mcc_stats_mask) { MCC_TRACE("br\n");
		int fits = 1;
		uint32_t jr;
		for (jr = 0; jr < MCCJIT_KGC_ARITY; jr++)
			if ((int64_t)(int32_t)tuple[jr] != tuple[jr])
				{ MCC_TRACE("br\n"); fits = 0; break; }
		mcc_stats_jit_blind_div_range(fits);
	}
	if (mcc_stats_mask)
		{ MCC_TRACE("br\n"); mcc_stats_jit_kgc_miss(); }
	if (!mccjit_nearmatch_miss(k, tuple, bval)) { MCC_TRACE("br\n");
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
	if (mccjit_nearmatch_active(k)) { MCC_TRACE("br\n");
		int64_t *co;
		k->nm_total++;
		mccjit_nearmatch_decide(k, variant, baseline, 1, 1);
		if (k->poisoned) { MCC_TRACE("br\n");
			pthread_mutex_unlock(&k->lock);
			return mccjit_invoke_fp(baseline, argv, nargs);
		}
		co = mccjit_corr_find(k, tuple);
		if (co) { MCC_TRACE("br\n");
			double v;
			memcpy(&v, co, sizeof v);
			pthread_mutex_unlock(&k->lock);
			return v;
		}
	}
	if (k->memoize_ok && mccjit_kgc_contains(k, tuple)) { MCC_TRACE("br\n");
		k->nm_match++;
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke_fp(variant, argv, nargs);
	}
	bval = mccjit_invoke_fp(baseline, argv, nargs);
	vval = mccjit_invoke_fp(variant, argv, nargs);
	memcpy(&bbits, &bval, sizeof bbits);
	memcpy(&vbits, &vval, sizeof vbits);
	if (vbits == bbits) { MCC_TRACE("br\n");
		k->hits++;
		k->nm_match++;
		if (k->memoize_ok)
			{ MCC_TRACE("br\n"); mccjit_kgc_insert(k, tuple); }
		pthread_mutex_unlock(&k->lock);
		return bval;
	}
	k->misses++;
	if (k->misses == 1 && mcc_stats_mask) { MCC_TRACE("br\n"); mcc_stats_jit_blind_dirty(); }
	if (!mccjit_nearmatch_miss(k, tuple, (int64_t)bbits)) { MCC_TRACE("br\n");
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

#if defined(MCCJIT_X64) || defined(MCCJIT_I386) || defined(MCCJIT_ARM64)
#if defined(MCCJIT_X64)
#if MCC_HOST_WIN32
static const unsigned char mccjit_mixed_thunk_code[] = {
		0x49, 0x89, 0xca,
		0xf2, 0x41, 0x0f, 0x10, 0x00,
		0xf2, 0x41, 0x0f, 0x10, 0x48, 0x08,
		0xf2, 0x41, 0x0f, 0x10, 0x50, 0x10,
		0xf2, 0x41, 0x0f, 0x10, 0x58, 0x18,
		0x48, 0x8b, 0x0a,
		0x4c, 0x8b, 0x4a, 0x18,
		0x4c, 0x8b, 0x42, 0x10,
		0x48, 0x8b, 0x52, 0x08,
		0x48, 0x83, 0xec, 0x28,
		0x41, 0xff, 0xd2,
		0x48, 0x83, 0xc4, 0x28,
		0xc3};
#else
static const unsigned char mccjit_mixed_thunk_code[] = {
		0x55, 0x48, 0x89, 0xe5, 0x49, 0x89, 0xfb, 0x49, 0x89, 0xd2, 0xf2, 0x41,
		0x0f, 0x10, 0x02, 0xf2, 0x41, 0x0f, 0x10, 0x4a, 0x08, 0xf2, 0x41, 0x0f,
		0x10, 0x52, 0x10, 0xf2, 0x41, 0x0f, 0x10, 0x5a, 0x18, 0xf2, 0x41, 0x0f,
		0x10, 0x62, 0x20, 0xf2, 0x41, 0x0f, 0x10, 0x6a, 0x28, 0xf2, 0x41, 0x0f,
		0x10, 0x72, 0x30, 0xf2, 0x41, 0x0f, 0x10, 0x7a, 0x38, 0x48, 0x8b, 0x3e,
		0x48, 0x8b, 0x56, 0x10, 0x48, 0x8b, 0x4e, 0x18, 0x4c, 0x8b, 0x46, 0x20,
		0x4c, 0x8b, 0x4e, 0x28, 0x48, 0x8b, 0x76, 0x08, 0xb0, 0x08, 0x41, 0xff,
		0xd3, 0xc9, 0xc3};
#endif

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
#elif defined(MCCJIT_ARM64)
static const unsigned char mccjit_mixed_thunk_code[] = {
		0xf0, 0x03, 0x00, 0xaa,
		0x40, 0x00, 0x40, 0xfd,
		0x41, 0x04, 0x40, 0xfd,
		0x42, 0x08, 0x40, 0xfd,
		0x43, 0x0c, 0x40, 0xfd,
		0x44, 0x10, 0x40, 0xfd,
		0x45, 0x14, 0x40, 0xfd,
		0x20, 0x00, 0x40, 0xf9,
		0x22, 0x08, 0x40, 0xf9,
		0x23, 0x0c, 0x40, 0xf9,
		0x24, 0x10, 0x40, 0xf9,
		0x25, 0x14, 0x40, 0xf9,
		0x21, 0x04, 0x40, 0xf9,
		0x00, 0x02, 0x1f, 0xd6
};

typedef int64_t (*MccjitThunkI)(void *fn, const int64_t *gpv, const double *fpv);
typedef double (*MccjitThunkD)(void *fn, const int64_t *gpv, const double *fpv);

static void *mccjit_mixed_thunk;
static pthread_once_t mccjit_mixed_thunk_once = PTHREAD_ONCE_INIT;

static void mccjit_mixed_thunk_build(void) { MCC_TRACE("enter\n");
	unsigned char *p = mmap(0, 4096, PROT_READ | PROT_WRITE,
													MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) { MCC_TRACE("br\n");
		mccjit_mixed_thunk = NULL;
		return;
	}
	memcpy(p, mccjit_mixed_thunk_code, sizeof mccjit_mixed_thunk_code);
	if (host_runmem_protect(p, 4096, HOST_PROT_RX) != 0) { MCC_TRACE("br\n");
		munmap(p, 4096);
		mccjit_mixed_thunk = NULL;
		return;
	}
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
#else
typedef int64_t (*MccjitI386ThunkI)(void *fn, const int64_t *gp,
																		const double *fp);
typedef double (*MccjitI386ThunkD)(void *fn, const int64_t *gp,
																	 const double *fp);

static void *mccjit_i386_active_thunk;

static void *mccjit_mixed_thunk_get(void) { MCC_TRACE("enter\n"); return (void *)1; }

static int64_t mccjit_invoke_mixed_i(void *fn, const int64_t *gpv,
																		 const double *fpv) { MCC_TRACE("enter\n");
	return ((MccjitI386ThunkI)mccjit_i386_active_thunk)(fn, gpv, fpv);
}

static double mccjit_invoke_mixed_d(void *fn, const int64_t *gpv,
																		const double *fpv) { MCC_TRACE("enter\n");
	return ((MccjitI386ThunkD)mccjit_i386_active_thunk)(fn, gpv, fpv);
}
#endif

static void mccjit_mixed_key(const MccjitKgc *k, const int64_t *gpv,
														 const double *fpv, int64_t *tuple) { MCC_TRACE("enter\n");
	uint32_t i, a = 0;
	for (i = 0; i < MCCJIT_KGC_ARITY; i++)
		{ MCC_TRACE("br\n"); tuple[i] = 0; }
#if MCC_HOST_WIN32
	for (i = 0; i < k->mx_nargs && a < MCCJIT_KGC_ARITY; i++, a++) { MCC_TRACE("br\n");
		if (k->mx_argclass & (1u << i))
			{ MCC_TRACE("br\n"); memcpy(&tuple[a], &fpv[i], sizeof tuple[a]); }
		else
			{ MCC_TRACE("br\n"); tuple[a] = gpv[i]; }
	}
#else
	for (i = 0; i < k->mx_ngp && a < MCCJIT_KGC_ARITY; i++, a++)
		{ MCC_TRACE("br\n"); tuple[a] = gpv[i]; }
	for (i = 0; i < k->mx_nsse && a < MCCJIT_KGC_ARITY; i++, a++)
		{ MCC_TRACE("br\n"); memcpy(&tuple[a], &fpv[i], sizeof tuple[a]); }
#endif
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
#if defined(MCCJIT_I386)
	mccjit_i386_active_thunk = k->mx_thunk;
#endif
	pthread_mutex_lock(&k->lock);
	if (k->poisoned) { MCC_TRACE("br\n");
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke_mixed_i(baseline, gpv, fpv);
	}
	if (mccjit_nearmatch_active(k)) { MCC_TRACE("br\n");
		int64_t *co;
		k->nm_total++;
		mccjit_nearmatch_decide(k, variant, baseline, 0, 0);
		if (k->poisoned) { MCC_TRACE("br\n");
			pthread_mutex_unlock(&k->lock);
			return mccjit_invoke_mixed_i(baseline, gpv, fpv);
		}
		co = mccjit_corr_find(k, tuple);
		if (co) { MCC_TRACE("br\n");
			int64_t v = *co;
			pthread_mutex_unlock(&k->lock);
			return v;
		}
	}
	if (k->memoize_ok && mccjit_kgc_contains(k, tuple)) { MCC_TRACE("br\n");
		k->nm_match++;
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke_mixed_i(variant, gpv, fpv);
	}
	bval = mccjit_invoke_mixed_i(baseline, gpv, fpv);
	mccjit_boundary_hit = 0;
	vval = mccjit_invoke_mixed_i(variant, gpv, fpv);
	bc = k->ret_wide ? bval : (int64_t)(int32_t)bval;
	vc = k->ret_wide ? vval : (int64_t)(int32_t)vval;
	if (mcc_stats_mask && mcc_env_on("MCC_JIT_BLIND_RETYPE"))
		{ MCC_TRACE("br\n"); mcc_stats_jit_bguard((vc != bc) == (mccjit_boundary_hit != 0)); }
	if (vc == bc) { MCC_TRACE("br\n");
		k->hits++;
		k->nm_match++;
		if (k->memoize_ok)
			{ MCC_TRACE("br\n"); mccjit_kgc_insert(k, tuple); }
		pthread_mutex_unlock(&k->lock);
		return bval;
	}
	k->misses++;
	if (k->misses == 1 && mcc_stats_mask) { MCC_TRACE("br\n"); mcc_stats_jit_blind_dirty(); }
	if (!mccjit_nearmatch_miss(k, tuple, bc))
		{ MCC_TRACE("br\n"); mccjit_mixed_poison_update(k); }
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
#if defined(MCCJIT_I386)
	mccjit_i386_active_thunk = k->mx_thunk;
#endif
	pthread_mutex_lock(&k->lock);
	if (k->poisoned) { MCC_TRACE("br\n");
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke_mixed_d(baseline, gpv, fpv);
	}
	if (mccjit_nearmatch_active(k)) { MCC_TRACE("br\n");
		int64_t *co;
		k->nm_total++;
		mccjit_nearmatch_decide(k, variant, baseline, 0, 0);
		if (k->poisoned) { MCC_TRACE("br\n");
			pthread_mutex_unlock(&k->lock);
			return mccjit_invoke_mixed_d(baseline, gpv, fpv);
		}
		co = mccjit_corr_find(k, tuple);
		if (co) { MCC_TRACE("br\n");
			double v;
			memcpy(&v, co, sizeof v);
			pthread_mutex_unlock(&k->lock);
			return v;
		}
	}
	if (k->memoize_ok && mccjit_kgc_contains(k, tuple)) { MCC_TRACE("br\n");
		k->nm_match++;
		pthread_mutex_unlock(&k->lock);
		return mccjit_invoke_mixed_d(variant, gpv, fpv);
	}
	bval = mccjit_invoke_mixed_d(baseline, gpv, fpv);
	vval = mccjit_invoke_mixed_d(variant, gpv, fpv);
	memcpy(&bbits, &bval, sizeof bbits);
	memcpy(&vbits, &vval, sizeof vbits);
	if (vbits == bbits) { MCC_TRACE("br\n");
		k->hits++;
		k->nm_match++;
		if (k->memoize_ok)
			{ MCC_TRACE("br\n"); mccjit_kgc_insert(k, tuple); }
		pthread_mutex_unlock(&k->lock);
		return bval;
	}
	k->misses++;
	if (k->misses == 1 && mcc_stats_mask) { MCC_TRACE("br\n"); mcc_stats_jit_blind_dirty(); }
	if (!mccjit_nearmatch_miss(k, tuple, (int64_t)bbits))
		{ MCC_TRACE("br\n"); mccjit_mixed_poison_update(k); }
	pthread_mutex_unlock(&k->lock);
	if (k->mx_flag)
		{ MCC_TRACE("br\n"); *k->mx_flag = 1; }
	return bval;
}
#endif

#if defined(MCCJIT_X64)
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
#if MCC_HOST_WIN32
	p[o++] = 0xc9;
	p[o++] = 0x55;
	p[o++] = 0x48; p[o++] = 0x83; p[o++] = 0xec; p[o++] = 0x60;
	for (i = 0; i < nargs; i++) { MCC_TRACE("br\n");
		unsigned char disp = (unsigned char)(0x30 + i * 8);
		if (i < 4) { MCC_TRACE("br\n");
			p[o++] = 0xf2; p[o++] = 0x0f; p[o++] = 0x11;
			p[o++] = (unsigned char)(0x44 + i * 8);
			p[o++] = 0x24;
			p[o++] = disp;
		} else { MCC_TRACE("br\n");
			uint32_t src = 0x90u + (uint32_t)(i - 4) * 8u;
			p[o++] = 0x48; p[o++] = 0x8b; p[o++] = 0x84; p[o++] = 0x24;
			memcpy(p + o, &src, 4);
			o += 4;
			p[o++] = 0x48; p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24;
			p[o++] = disp;
		}
	}
	p[o++] = 0x48; p[o++] = 0xb9; memcpy(p + o, &kgc, 8); o += 8;
	p[o++] = 0x48; p[o++] = 0xba; memcpy(p + o, &variant, 8); o += 8;
	p[o++] = 0x49; p[o++] = 0xb8; memcpy(p + o, &baseline, 8); o += 8;
	p[o++] = 0x4c; p[o++] = 0x8d; p[o++] = 0x4c; p[o++] = 0x24; p[o++] = 0x30;
	p[o++] = 0xc7; p[o++] = 0x44; p[o++] = 0x24; p[o++] = 0x20;
	memcpy(p + o, &nargs, 4); o += 4;
	p[o++] = 0x48; p[o++] = 0xb8; memcpy(p + o, &fp, 8); o += 8;
	p[o++] = 0x48; p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = 0x28;
	p[o++] = 0x48; p[o++] = 0xb8; memcpy(p + o, &calln, 8); o += 8;
	p[o++] = 0xff; p[o++] = 0xd0;
	p[o++] = 0x48; p[o++] = 0x83; p[o++] = 0xc4; p[o++] = 0x60;
	p[o++] = 0x5d;
	p[o++] = 0xc3;
#else
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
#endif
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
#if MCC_HOST_WIN32
	{
		static const unsigned char win_st_wide[4][4] = {
				{0x48, 0x89, 0x4c, 0x24},
				{0x48, 0x89, 0x54, 0x24},
				{0x4c, 0x89, 0x44, 0x24},
				{0x4c, 0x89, 0x4c, 0x24},
		};
		static const unsigned char win_movsxd[4][3] = {
				{0x48, 0x63, 0xc1},
				{0x48, 0x63, 0xc2},
				{0x49, 0x63, 0xc0},
				{0x49, 0x63, 0xc1},
		};
		p[o++] = 0xc9;
		p[o++] = 0x55;
		p[o++] = 0x48; p[o++] = 0x83; p[o++] = 0xec; p[o++] = 0x60;
		for (i = 0; i < nargs; i++) { MCC_TRACE("br\n");
			unsigned char disp = (unsigned char)(0x30 + i * 8);
			int wide = mccjit_type_wide((int)param_t[i]);
			if (i < 4) { MCC_TRACE("br\n");
				if (wide) { MCC_TRACE("br\n");
					memcpy(p + o, win_st_wide[i], 4);
					o += 4;
					p[o++] = disp;
				} else { MCC_TRACE("br\n");
					memcpy(p + o, win_movsxd[i], 3);
					o += 3;
					p[o++] = 0x48; p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24;
					p[o++] = disp;
				}
			} else { MCC_TRACE("br\n");
				uint32_t src = 0x90u + (uint32_t)(i - 4) * 8u;
				p[o++] = 0x48; p[o++] = wide ? 0x8b : 0x63;
				p[o++] = 0x84; p[o++] = 0x24;
				memcpy(p + o, &src, 4);
				o += 4;
				p[o++] = 0x48; p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24;
				p[o++] = disp;
			}
		}
		p[o++] = 0x48; p[o++] = 0xb9; memcpy(p + o, &kgc, 8); o += 8;
		p[o++] = 0x48; p[o++] = 0xba; memcpy(p + o, &variant, 8); o += 8;
		p[o++] = 0x49; p[o++] = 0xb8; memcpy(p + o, &baseline, 8); o += 8;
		p[o++] = 0x4c; p[o++] = 0x8d; p[o++] = 0x4c; p[o++] = 0x24; p[o++] = 0x30;
		p[o++] = 0xc7; p[o++] = 0x44; p[o++] = 0x24; p[o++] = 0x20;
		memcpy(p + o, &nargs, 4); o += 4;
		p[o++] = 0x48; p[o++] = 0xb8; memcpy(p + o, &fp, 8); o += 8;
		p[o++] = 0x48; p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = 0x28;
		p[o++] = 0x48; p[o++] = 0xb8; memcpy(p + o, &calln, 8); o += 8;
		p[o++] = 0xff; p[o++] = 0xd0;
		p[o++] = 0x48; p[o++] = 0x83; p[o++] = 0xc4; p[o++] = 0x60;
		p[o++] = 0x5d;
		p[o++] = 0xc3;
	}
#else
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
#endif
	return p;
}

static void *mccjit_make_kgc_stub_mixed(void *variant, void *baseline,
																				int memoize_ok, uint32_t ngp,
																				uint32_t nsse, int ret_fp, int ret_wide) { MCC_TRACE("enter\n");
#if MCC_HOST_WIN32
	{
		unsigned char *p;
		MccjitKgc *kgc;
		int *flag;
		void *calln = ret_fp ? (void *)mccjit_kgc_calln_mixed_d
												 : (void *)mccjit_kgc_calln_mixed_i;
		uint32_t nargs = ngp + nsse, argclass = 0, qi;
		size_t o = 0;
		if (nargs < 1 || nargs > 4)
			{ MCC_TRACE("br\n"); return NULL; }
		if (!mccjit_mixed_thunk_get())
			{ MCC_TRACE("br\n"); return NULL; }
		for (qi = 0; qi < nargs && qi < MCCJIT_KGC_MAXARG; qi++)
			{ MCC_TRACE("br\n"); if (mccjit_type_fp((int)mccjit_last_param_t[qi]))
				{ MCC_TRACE("br\n"); argclass |= (1u << qi); } }
		kgc = mcc_mallocz(sizeof *kgc);
		if (!kgc)
			{ MCC_TRACE("br\n"); return NULL; }
		if (mccjit_kgc_open(kgc, NULL, mccjit_salt_witness(), nargs) != 0) { MCC_TRACE("br\n");
			mcc_free(kgc);
			return NULL;
		}
		kgc->memoize_ok = memoize_ok;
		kgc->ret_wide = ret_wide;
		kgc->mx_variant = variant;
		kgc->mx_baseline = baseline;
		kgc->mx_ngp = ngp;
		kgc->mx_nsse = nsse;
		kgc->mx_nargs = nargs;
		kgc->mx_argclass = argclass;
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
		p[o++] = 0xc9;
		p[o++] = 0x55;
		p[o++] = 0x48; p[o++] = 0x83; p[o++] = 0xec; p[o++] = 0x60;
		p[o++] = 0x48; p[o++] = 0x89; p[o++] = 0x4c; p[o++] = 0x24; p[o++] = 0x20;
		p[o++] = 0x48; p[o++] = 0x89; p[o++] = 0x54; p[o++] = 0x24; p[o++] = 0x28;
		p[o++] = 0x4c; p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = 0x30;
		p[o++] = 0x4c; p[o++] = 0x89; p[o++] = 0x4c; p[o++] = 0x24; p[o++] = 0x38;
		p[o++] = 0xf2; p[o++] = 0x0f; p[o++] = 0x11; p[o++] = 0x44; p[o++] = 0x24; p[o++] = 0x40;
		p[o++] = 0xf2; p[o++] = 0x0f; p[o++] = 0x11; p[o++] = 0x4c; p[o++] = 0x24; p[o++] = 0x48;
		p[o++] = 0xf2; p[o++] = 0x0f; p[o++] = 0x11; p[o++] = 0x54; p[o++] = 0x24; p[o++] = 0x50;
		p[o++] = 0xf2; p[o++] = 0x0f; p[o++] = 0x11; p[o++] = 0x5c; p[o++] = 0x24; p[o++] = 0x58;
		p[o++] = 0x48; p[o++] = 0xb9; memcpy(p + o, &kgc, 8); o += 8;
		p[o++] = 0x48; p[o++] = 0x8d; p[o++] = 0x54; p[o++] = 0x24; p[o++] = 0x20;
		p[o++] = 0x4c; p[o++] = 0x8d; p[o++] = 0x44; p[o++] = 0x24; p[o++] = 0x40;
		p[o++] = 0x48; p[o++] = 0xb8; memcpy(p + o, &calln, 8); o += 8;
		p[o++] = 0xff; p[o++] = 0xd0;
		p[o++] = 0x48; p[o++] = 0x83; p[o++] = 0xc4; p[o++] = 0x60;
		p[o++] = 0x5d;
		p[o++] = 0xc3;
		return p;
	}
#else
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
#endif
}
#elif defined(MCCJIT_ARM64)
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
	MCCJIT_A64_W(0xa9bf7bfdu);
	MCCJIT_A64_W(0xd100c3ffu);
	for (i = 0; i < nargs; i++) { MCC_TRACE("br\n");
		if (mccjit_type_wide((int)param_t[i]))
			{ MCC_TRACE("br\n"); MCCJIT_A64_W(0xf90003e0u | (i << 10) | i); }
		else { MCC_TRACE("br\n");
			MCCJIT_A64_W(0x93407c00u | (i << 5) | 8);
			MCCJIT_A64_W(0xf90003e8u | (i << 10));
		}
	}
	MCCJIT_A64_LDR(0, D + 0);
	MCCJIT_A64_LDR(1, D + 8);
	MCCJIT_A64_LDR(2, D + 16);
	MCCJIT_A64_W(0x910003e3u);
	MCCJIT_A64_W(0x52800004u | (nargs << 5));
	MCCJIT_A64_LDR(5, D + 24);
	MCCJIT_A64_LDR(16, D + 32);
	MCCJIT_A64_W(0xd63f0200u);
	MCCJIT_A64_W(0x9100c3ffu);
	MCCJIT_A64_W(0xa8c17bfdu);
	MCCJIT_A64_W(0xd65f03c0u);
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
	MCCJIT_A64_W(0xa9bf7bfdu);
	MCCJIT_A64_W(0xd100c3ffu);
	for (i = 0; i < nargs; i++) { MCC_TRACE("br\n");
		MCCJIT_A64_W(0xfd0003e0u | (i << 10) | i);
	}
	MCCJIT_A64_LDR(0, D + 0);
	MCCJIT_A64_LDR(1, D + 8);
	MCCJIT_A64_LDR(2, D + 16);
	MCCJIT_A64_W(0x910003e3u);
	MCCJIT_A64_W(0x52800004u | (nargs << 5));
	MCCJIT_A64_LDR(5, D + 24);
	MCCJIT_A64_LDR(16, D + 32);
	MCCJIT_A64_W(0xd63f0200u);
	MCCJIT_A64_W(0x9100c3ffu);
	MCCJIT_A64_W(0xa8c17bfdu);
	MCCJIT_A64_W(0xd65f03c0u);
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
	void *calln = ret_fp ? (void *)mccjit_kgc_calln_mixed_d
											 : (void *)mccjit_kgc_calln_mixed_i;
	size_t page = host_pagesize();
	const uint32_t D = 256;
	unsigned char *p;
	MccjitKgc *kgc;
	int *flag;
	uint32_t arity = ngp + nsse;
	uint64_t kp, cp;
	uint32_t i, o = 0;
	if (arity < 1 || arity > MCCJIT_KGC_ARITY)
		{ MCC_TRACE("br\n"); return NULL; }
	if (!mccjit_mixed_thunk_get())
		{ MCC_TRACE("br\n"); return NULL; }
	kgc = mcc_mallocz(sizeof *kgc);
	flag = mcc_mallocz(sizeof *flag);
	if (!kgc || !flag) { MCC_TRACE("br\n");
		mcc_free(kgc);
		mcc_free(flag);
		return NULL;
	}
	if (mccjit_kgc_open(kgc, NULL, mccjit_salt_witness(), arity) != 0) { MCC_TRACE("br\n");
		mcc_free(kgc);
		mcc_free(flag);
		return NULL;
	}
	kgc->memoize_ok = memoize_ok;
	kgc->ret_wide = ret_wide;
	kgc->mx_variant = variant;
	kgc->mx_baseline = baseline;
	kgc->mx_ngp = ngp;
	kgc->mx_nsse = nsse;
	kgc->mx_ret_fp = ret_fp;
	p = mmap(0, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) { MCC_TRACE("br\n");
		mccjit_kgc_close(kgc);
		mcc_free(kgc);
		mcc_free(flag);
		return NULL;
	}
	*flag = 0;
	kgc->mx_flag = flag;
	MCCJIT_A64_W(0xa9bf7bfdu);
	MCCJIT_A64_W(0xd10183ffu);
	for (i = 0; i < 6; i++) { MCC_TRACE("br\n");
		MCCJIT_A64_W(0xf90003e0u | (i << 10) | i);
	}
	for (i = 0; i < 6; i++) { MCC_TRACE("br\n");
		MCCJIT_A64_W(0xfd0003e0u | ((6 + i) << 10) | i);
	}
	MCCJIT_A64_LDR(0, D + 0);
	MCCJIT_A64_W(0x910003e1u);
	MCCJIT_A64_W(0x9100c3e2u);
	MCCJIT_A64_LDR(16, D + 8);
	MCCJIT_A64_W(0xd63f0200u);
	MCCJIT_A64_W(0x910183ffu);
	MCCJIT_A64_W(0xa8c17bfdu);
	MCCJIT_A64_W(0xd65f03c0u);
	kp = (uint64_t)(uintptr_t)kgc;
	cp = (uint64_t)(uintptr_t)calln;
	memcpy(p + D + 0, &kp, 8);
	memcpy(p + D + 8, &cp, 8);
	if (host_runmem_protect(p, page, HOST_PROT_RX) != 0) { MCC_TRACE("br\n");
		munmap(p, page);
		mccjit_kgc_close(kgc);
		mcc_free(kgc);
		mcc_free(flag);
		return NULL;
	}
	return p;
}
#undef MCCJIT_A64_W
#undef MCCJIT_A64_LDR
#elif defined(MCCJIT_I386)

static int mccjit_i386_arg8(int t) { MCC_TRACE("enter\n");
	int b = t & VT_BTYPE;
	if (b == VT_LLONG || b == VT_DOUBLE)
		{ MCC_TRACE("br\n"); return 1; }
	if (b == VT_INT && (t & VT_LONG))
		{ MCC_TRACE("br\n"); return 0; }
	return 0;
}

#define MCCJIT_I386_MOVMI(d8, imm)             \
	do { MCC_TRACE("br\n");                       \
		uint32_t imm_ = (uint32_t)(uintptr_t)(imm); \
		p[o++] = 0xc7; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)(d8); \
		memcpy(p + o, &imm_, 4); o += 4;            \
	} while (0)

static void *mccjit_make_kgc_stub_n(void *variant, void *baseline, int memoize_ok,
																		const uint32_t *param_t, uint32_t nargs,
																		int ret_wide) { MCC_TRACE("enter\n");
	unsigned char *p;
	MccjitKgc *kgc;
	int *flag;
	void *calln = (void *)mccjit_kgc_calln;
	size_t o = 0;
	uint32_t i;
	int src;
	const int ARGV = 24;
	if (!mccjit_i386_stubs_enabled())
		{ MCC_TRACE("br\n"); return NULL; }
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
	p[o++] = 0xc9;
	p[o++] = 0x55;
	p[o++] = 0x89; p[o++] = 0xe5;
	p[o++] = 0x81; p[o++] = 0xec;
	{ uint32_t fr = (uint32_t)(ARGV + 8 * MCCJIT_KGC_MAXARG + 16);
		memcpy(p + o, &fr, 4); o += 4; }
	src = 8;
	for (i = 0; i < nargs; i++) { MCC_TRACE("br\n");
		int d = ARGV + (int)i * 8;
		if (mccjit_i386_arg8((int)param_t[i])) { MCC_TRACE("br\n");
			p[o++] = 0x8b; p[o++] = 0x45; p[o++] = (unsigned char)src;
			p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)d;
			p[o++] = 0x8b; p[o++] = 0x45; p[o++] = (unsigned char)(src + 4);
			p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)(d + 4);
			src += 8;
		} else { MCC_TRACE("br\n");
			p[o++] = 0x8b; p[o++] = 0x45; p[o++] = (unsigned char)src;
			p[o++] = 0x99;
			p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)d;
			p[o++] = 0x89; p[o++] = 0x54; p[o++] = 0x24; p[o++] = (unsigned char)(d + 4);
			src += 4;
		}
	}
	MCCJIT_I386_MOVMI(0, kgc);
	MCCJIT_I386_MOVMI(4, variant);
	MCCJIT_I386_MOVMI(8, baseline);
	p[o++] = 0x8d; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)ARGV;
	p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = 12;
	MCCJIT_I386_MOVMI(16, nargs);
	MCCJIT_I386_MOVMI(20, flag);
	p[o++] = 0xb8; { uint32_t c = (uint32_t)(uintptr_t)calln; memcpy(p + o, &c, 4); o += 4; }
	p[o++] = 0xff; p[o++] = 0xd0;
	p[o++] = 0x89; p[o++] = 0xec;
	p[o++] = 0x5d;
	p[o++] = 0xc3;
	return p;
}

static void *mccjit_make_kgc_stub_fp(void *variant, void *baseline,
																		 int memoize_ok, uint32_t nargs) { MCC_TRACE("enter\n");
	unsigned char *p;
	MccjitKgc *kgc;
	int *flag;
	void *calln = (void *)mccjit_kgc_calln_fp;
	size_t o = 0;
	uint32_t i;
	const int ARGV = 24;
	if (!mccjit_i386_stubs_enabled())
		{ MCC_TRACE("br\n"); return NULL; }
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
	p[o++] = 0xc9;
	p[o++] = 0x55;
	p[o++] = 0x89; p[o++] = 0xe5;
	p[o++] = 0x81; p[o++] = 0xec;
	{ uint32_t fr = (uint32_t)(ARGV + 8 * MCCJIT_KGC_MAXARG + 16);
		memcpy(p + o, &fr, 4); o += 4; }
	for (i = 0; i < nargs; i++) { MCC_TRACE("br\n");
		int s = 8 + (int)i * 8;
		int d = ARGV + (int)i * 8;
		p[o++] = 0x8b; p[o++] = 0x45; p[o++] = (unsigned char)s;
		p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)d;
		p[o++] = 0x8b; p[o++] = 0x45; p[o++] = (unsigned char)(s + 4);
		p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)(d + 4);
	}
	MCCJIT_I386_MOVMI(0, kgc);
	MCCJIT_I386_MOVMI(4, variant);
	MCCJIT_I386_MOVMI(8, baseline);
	p[o++] = 0x8d; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)ARGV;
	p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = 12;
	MCCJIT_I386_MOVMI(16, nargs);
	MCCJIT_I386_MOVMI(20, flag);
	p[o++] = 0xb8; { uint32_t c = (uint32_t)(uintptr_t)calln; memcpy(p + o, &c, 4); o += 4; }
	p[o++] = 0xff; p[o++] = 0xd0;
	p[o++] = 0x89; p[o++] = 0xec;
	p[o++] = 0x5d;
	p[o++] = 0xc3;
	return p;
}

static void *mccjit_i386_mixed_thunk_build(uint32_t nargs, uint32_t argclass,
																					 uint32_t argsz) { MCC_TRACE("enter\n");
	unsigned char *p;
	size_t o = 0;
	uint32_t pos;
	int stksz = 0;
	if (nargs > 4)
		{ MCC_TRACE("br\n"); return NULL; }
	for (pos = 0; pos < nargs; pos++)
		{ MCC_TRACE("br\n"); stksz += (argsz & (1u << pos)) ? 8 : 4; }
	if (stksz & 15)
		{ MCC_TRACE("br\n"); stksz = (stksz + 15) & ~15; }
	p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
					 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return NULL; }
	p[o++] = 0x55;
	p[o++] = 0x89; p[o++] = 0xe5;
	p[o++] = 0x56;
	p[o++] = 0x57;
	p[o++] = 0x81; p[o++] = 0xec;
	{ uint32_t fr = (uint32_t)stksz; memcpy(p + o, &fr, 4); o += 4; }
	p[o++] = 0x8b; p[o++] = 0x75; p[o++] = 12;
	p[o++] = 0x8b; p[o++] = 0x7d; p[o++] = 16;
	{ int out = 0; for (pos = 0; pos < nargs; pos++) { MCC_TRACE("br\n");
		int fp_arg = (argclass & (1u << pos)) != 0;
		int wide8 = (argsz & (1u << pos)) != 0;
		unsigned char reg = fp_arg ? 0x7f : 0x7e;
		int soff = (int)pos * 8;
		p[o++] = 0x8b; p[o++] = (unsigned char)(0x80 | (reg & 7));
		{ uint32_t d = (uint32_t)soff; memcpy(p + o, &d, 4); o += 4; }
		p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)out;
		out += 4;
		if (wide8) { MCC_TRACE("br\n");
			p[o++] = 0x8b; p[o++] = (unsigned char)(0x80 | (reg & 7));
			{ uint32_t d = (uint32_t)(soff + 4); memcpy(p + o, &d, 4); o += 4; }
			p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)out;
			out += 4;
		}
	} }
	p[o++] = 0x8b; p[o++] = 0x45; p[o++] = 8;
	p[o++] = 0xff; p[o++] = 0xd0;
	p[o++] = 0x8d; p[o++] = 0x65; p[o++] = 0xf8;
	p[o++] = 0x5f;
	p[o++] = 0x5e;
	p[o++] = 0x5d;
	p[o++] = 0xc3;
	return p;
}

static void *mccjit_make_kgc_stub_mixed(void *variant, void *baseline,
																				int memoize_ok, uint32_t ngp,
																				uint32_t nsse, int ret_fp, int ret_wide) { MCC_TRACE("enter\n");
	unsigned char *p;
	MccjitKgc *kgc;
	int *flag;
	void *calln = ret_fp ? (void *)mccjit_kgc_calln_mixed_d
											 : (void *)mccjit_kgc_calln_mixed_i;
	uint32_t nargs = ngp + nsse, argclass = 0, argsz = 0, qi;
	void *thunk;
	size_t o = 0;
	int src;
	const int GP = 12;
	const int FP = GP + 32;
	if (!mccjit_i386_stubs_enabled())
		{ MCC_TRACE("br\n"); return NULL; }
	if (nargs < 1 || nargs > 4)
		{ MCC_TRACE("br\n"); return NULL; }
	for (qi = 0; qi < nargs && qi < MCCJIT_KGC_MAXARG; qi++) { MCC_TRACE("br\n");
		if (mccjit_type_fp((int)mccjit_last_param_t[qi]))
			{ MCC_TRACE("br\n"); argclass |= (1u << qi); }
		if (mccjit_i386_arg8((int)mccjit_last_param_t[qi]))
			{ MCC_TRACE("br\n"); argsz |= (1u << qi); }
	}
	thunk = mccjit_i386_mixed_thunk_build(nargs, argclass, argsz);
	if (!thunk)
		{ MCC_TRACE("br\n"); return NULL; }
	kgc = mcc_mallocz(sizeof *kgc);
	if (!kgc)
		{ MCC_TRACE("br\n"); munmap(thunk, 4096); return NULL; }
	if (mccjit_kgc_open(kgc, NULL, mccjit_salt_witness(), nargs) != 0) { MCC_TRACE("br\n");
		munmap(thunk, 4096);
		mcc_free(kgc);
		return NULL;
	}
	kgc->memoize_ok = memoize_ok;
	kgc->ret_wide = ret_wide;
	kgc->mx_variant = variant;
	kgc->mx_baseline = baseline;
	kgc->mx_ngp = ngp;
	kgc->mx_nsse = nsse;
	kgc->mx_nargs = nargs;
	kgc->mx_argclass = argclass;
	kgc->mx_ret_fp = ret_fp;
	kgc->mx_thunk = thunk;
	kgc->mx_argsz = argsz;
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
	p[o++] = 0xc9;
	p[o++] = 0x55;
	p[o++] = 0x89; p[o++] = 0xe5;
	p[o++] = 0x81; p[o++] = 0xec;
	{ uint32_t fr = (uint32_t)(FP + 32 + 16);
		memcpy(p + o, &fr, 4); o += 4; }
	p[o++] = 0x31; p[o++] = 0xc0;
	{ int z; for (z = 0; z < 8; z++) { MCC_TRACE("br\n");
		p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)(GP + z * 8);
		p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)(GP + z * 8 + 4);
	} }
	src = 8;
	for (qi = 0; qi < nargs; qi++) { MCC_TRACE("br\n");
		int fp_arg = (argclass & (1u << qi)) != 0;
		int wide8 = mccjit_i386_arg8((int)mccjit_last_param_t[qi]);
		int d = (fp_arg ? FP : GP) + (int)qi * 8;
		p[o++] = 0x8b; p[o++] = 0x45; p[o++] = (unsigned char)src;
		p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)d;
		if (wide8) { MCC_TRACE("br\n");
			p[o++] = 0x8b; p[o++] = 0x45; p[o++] = (unsigned char)(src + 4);
			p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)(d + 4);
			src += 8;
		} else { MCC_TRACE("br\n");
			src += 4;
		}
	}
	MCCJIT_I386_MOVMI(0, kgc);
	p[o++] = 0x8d; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)GP;
	p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = 4;
	p[o++] = 0x8d; p[o++] = 0x44; p[o++] = 0x24; p[o++] = (unsigned char)FP;
	p[o++] = 0x89; p[o++] = 0x44; p[o++] = 0x24; p[o++] = 8;
	p[o++] = 0xb8; { uint32_t c = (uint32_t)(uintptr_t)calln; memcpy(p + o, &c, 4); o += 4; }
	p[o++] = 0xff; p[o++] = 0xd0;
	p[o++] = 0x89; p[o++] = 0xec;
	p[o++] = 0x5d;
	p[o++] = 0xc3;
	return p;
}
#undef MCCJIT_I386_MOVMI
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

PUB_FUNC int mccjit_selftest_kgc(void) { MCC_TRACE("enter\n");
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
#if MCC_HOST_WIN32
		char path[] = "mccjit_kgc_XXXXXX";
#else
		char path[] = "/tmp/mccjit_kgc_XXXXXX";
#endif
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

static void *mccjit_nm_compile(const char *src, const char *name, MCCState **out) { MCC_TRACE("enter\n");
	MCCState *s = mcc_new();
	void *fn = NULL;
	*out = NULL;
	if (!s)
		{ MCC_TRACE("br\n"); return NULL; }
	s->optimize = 1;
	s->nostdlib = 1;
	mcc_set_output_type(s, MCC_OUTPUT_MEMORY);
	if (mcc_compile_string(s, src) == 0 && mcc_relocate(s) == 0)
		{ MCC_TRACE("br\n"); fn = mcc_get_symbol(s, name); }
	if (!fn) { MCC_TRACE("br\n"); mcc_delete(s); return NULL; }
	*out = s;
	return fn;
}

static int mccjit_nm_drive(MccjitKgc *k, void *variant, int (*baseline)(int),
													 int calls, int period, const int64_t *offv, int noff,
													 int distinct) { MCC_TRACE("enter\n");
	int i, wrong = 0;
	for (i = 0; i < calls; i++) { MCC_TRACE("br\n");
		int64_t x = (i % period == 0)
									? (distinct ? (int64_t)(1000 + i)
											: offv[(i / period) % noff])
									: 7;
		int flagged = 0;
		int64_t got = mccjit_kgc_call1(k, variant, (void *)baseline, x, &flagged);
		if (got != (int64_t)baseline((int)x))
			{ MCC_TRACE("br\n"); wrong++; }
	}
	return wrong;
}

static int mccjit_nm_drive_fp(MccjitKgc *k, void *variant, void *baseline,
															double (*base_fn)(double), int calls, int period,
															const double *offv, int noff) { MCC_TRACE("enter\n");
	int i, wrong = 0;
	for (i = 0; i < calls; i++) { MCC_TRACE("br\n");
		double x = (i % period == 0) ? offv[(i / period) % noff] : 7.0;
		int flagged = 0;
		double got = mccjit_kgc_calln_fp(k, variant, baseline, &x, 1, &flagged);
		double want = base_fn(x);
		uint64_t gb, wb;
		memcpy(&gb, &got, sizeof gb);
		memcpy(&wb, &want, sizeof wb);
		if (gb != wb)
			{ MCC_TRACE("br\n"); wrong++; }
	}
	return wrong;
}

PUB_FUNC int mccjit_selftest_nearmatch(void) { MCC_TRACE("enter\n");
	int (*base_slow)(int) = NULL;
	int (*base_fast)(int) = NULL;
	double (*base_dslow)(double) = NULL;
	MCCState *sbs = NULL, *sbf = NULL, *svc = NULL, *svs = NULL;
	MCCState *sbds = NULL, *svdc = NULL;
	void *var_const = NULL;
	void *var_slow = NULL;
	void *var_dconst = NULL;
	MccjitKgc kgc;
	int fails = 0, wrong;
	int64_t offv[4] = {5, 0, 3, 100};
	double offd[4] = {5.5, 0.25, -3.0, 100.0};
	char vsrc[128];

	printf("mccjit-selftest-nearmatch: begin\n");
	base_slow = (int (*)(int))mccjit_nm_compile(
			"int f(int x){int a=x,k;for(k=0;k<160;k++)a=(a*1103515245+12345)&0x7fffffff;return a;}",
			"f", &sbs);
	base_fast = (int (*)(int))mccjit_nm_compile("int f(int x){return x*2+1;}", "f", &sbf);
	if (!base_slow || !base_fast) { MCC_TRACE("br\n");
		printf("mccjit-selftest-nearmatch: baseline build failed\n");
		if (sbs) { MCC_TRACE("br\n"); mcc_delete(sbs); }
		if (sbf) { MCC_TRACE("br\n"); mcc_delete(sbf); }
		return 1;
	}
	snprintf(vsrc, sizeof vsrc, "int v(int x){(void)x;return %d;}", base_slow(7));
	var_const = mccjit_nm_compile(vsrc, "v", &svc);
	snprintf(vsrc, sizeof vsrc,
					 "int v(int x){volatile int a=0;int k;for(k=0;k<600;k++)a++;(void)x;return %d;}",
					 base_fast(7));
	var_slow = mccjit_nm_compile(vsrc, "v", &svs);
	if (!var_const || !var_slow) { MCC_TRACE("br\n");
		printf("mccjit-selftest-nearmatch: variant build failed\n");
		fails = 1;
		goto done;
	}
	base_dslow = (double (*)(double))mccjit_nm_compile(
			"double f(double x){double a=x;int k;for(k=0;k<160;k++)a=a*1.0000001+0.5;return a;}",
			"f", &sbds);
	if (base_dslow) { MCC_TRACE("br\n");
		snprintf(vsrc, sizeof vsrc, "double v(double x){(void)x;return %.17g;}",
						 base_dslow(7.0));
		var_dconst = mccjit_nm_compile(vsrc, "v", &svdc);
	}
	if (!base_dslow || !var_dconst) { MCC_TRACE("br\n");
		printf("mccjit-selftest-nearmatch: fp variant build failed\n");
		fails = 1;
		goto done;
	}

	if (mccjit_kgc_open(&kgc, NULL, mccjit_salt_witness(), 1) == 0) { MCC_TRACE("br\n");
		wrong = mccjit_nm_drive(&kgc, var_const, base_slow, 4000, 50, offv, 4, 0);
		printf("mccjit-selftest-nearmatch: ACCEPT wrong=%d nearmatch=%d poisoned=%d "
					 "corrections=%llu nm=%llu/%llu\n",
					 wrong, kgc.nearmatch, kgc.poisoned, (unsigned long long)kgc.corr_n,
					 (unsigned long long)kgc.nm_match, (unsigned long long)kgc.nm_total);
		if (wrong || !kgc.nearmatch || kgc.poisoned || kgc.corr_n != 4) { MCC_TRACE("br\n"); fails++; }
		{
			int j;
			for (j = 0; j < 4; j++) { MCC_TRACE("br\n");
				int64_t t[MCCJIT_KGC_ARITY], *co;
				uint32_t m;
				for (m = 0; m < MCCJIT_KGC_ARITY; m++)
					{ MCC_TRACE("br\n"); t[m] = 0; }
				t[0] = offv[j];
				co = mccjit_corr_find(&kgc, t);
				if (!co || *co != (int64_t)base_slow((int)offv[j]))
					{ MCC_TRACE("br\n"); fails++; }
			}
		}
		mccjit_kgc_close(&kgc);
	}

	if (mccjit_kgc_open(&kgc, NULL, mccjit_salt_witness(), 1) == 0) { MCC_TRACE("br\n");
		kgc.nearmatch_on = 0;
		wrong = mccjit_nm_drive(&kgc, var_const, base_slow, 4000, 50, offv, 4, 0);
		printf("mccjit-selftest-nearmatch: OFF wrong=%d corrections=%llu nearmatch=%d (expect 0/0/0)\n",
					 wrong, (unsigned long long)kgc.corr_n, kgc.nearmatch);
		if (wrong || kgc.corr_n != 0 || kgc.nearmatch) { MCC_TRACE("br\n"); fails++; }
		mccjit_kgc_close(&kgc);
	}

	if (mccjit_kgc_open(&kgc, NULL, mccjit_salt_witness(), 1) == 0) { MCC_TRACE("br\n");
		wrong = mccjit_nm_drive(&kgc, var_slow, base_fast, 4000, 50, offv, 4, 0);
		printf("mccjit-selftest-nearmatch: SLOW wrong=%d nearmatch=%d poisoned=%d (expect 0/0/1)\n",
					 wrong, kgc.nearmatch, kgc.poisoned);
		if (wrong || kgc.nearmatch || !kgc.poisoned) { MCC_TRACE("br\n"); fails++; }
		mccjit_kgc_close(&kgc);
	}

	if (mccjit_kgc_open(&kgc, NULL, mccjit_salt_witness(), 1) == 0) { MCC_TRACE("br\n");
		wrong = mccjit_nm_drive(&kgc, var_const, base_slow, 4000, 2, offv, 4, 1);
		printf("mccjit-selftest-nearmatch: BIGTABLE wrong=%d nearmatch=%d poisoned=%d corr=%llu (expect 0/0/1)\n",
					 wrong, kgc.nearmatch, kgc.poisoned, (unsigned long long)kgc.corr_n);
		if (wrong || kgc.nearmatch || !kgc.poisoned) { MCC_TRACE("br\n"); fails++; }
		mccjit_kgc_close(&kgc);
	}

	if (mccjit_kgc_open(&kgc, NULL, mccjit_salt_witness(), 1) == 0) { MCC_TRACE("br\n");
		kgc.ret_wide = 1;
		wrong = mccjit_nm_drive_fp(&kgc, var_dconst, (void *)base_dslow, base_dslow,
															 4000, 50, offd, 4);
		printf("mccjit-selftest-nearmatch: FPACCEPT wrong=%d nearmatch=%d poisoned=%d "
					 "corrections=%llu nm=%llu/%llu\n",
					 wrong, kgc.nearmatch, kgc.poisoned, (unsigned long long)kgc.corr_n,
					 (unsigned long long)kgc.nm_match, (unsigned long long)kgc.nm_total);
		if (wrong || !kgc.nearmatch || kgc.poisoned || kgc.corr_n != 4) { MCC_TRACE("br\n"); fails++; }
		{
			int j;
			for (j = 0; j < 4; j++) { MCC_TRACE("br\n");
				int64_t t[MCCJIT_KGC_ARITY], *co;
				uint32_t m;
				double want = base_dslow(offd[j]);
				uint64_t wb;
				for (m = 0; m < MCCJIT_KGC_ARITY; m++)
					{ MCC_TRACE("br\n"); t[m] = 0; }
				memcpy(&t[0], &offd[j], sizeof t[0]);
				memcpy(&wb, &want, sizeof wb);
				co = mccjit_corr_find(&kgc, t);
				if (!co || (uint64_t)*co != wb)
					{ MCC_TRACE("br\n"); fails++; }
			}
		}
		mccjit_kgc_close(&kgc);
	}

done:
	if (svc) { MCC_TRACE("br\n"); mcc_delete(svc); }
	if (svs) { MCC_TRACE("br\n"); mcc_delete(svs); }
	if (svdc) { MCC_TRACE("br\n"); mcc_delete(svdc); }
	if (sbs) { MCC_TRACE("br\n"); mcc_delete(sbs); }
	if (sbf) { MCC_TRACE("br\n"); mcc_delete(sbf); }
	if (sbds) { MCC_TRACE("br\n"); mcc_delete(sbds); }
	printf("mccjit-selftest-nearmatch: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
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
	ast_configure(js);
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
	ast_configure(js);
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

PUB_FUNC int mccjit_selftest_strlit(void) { MCC_TRACE("enter\n");
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

PUB_FUNC int mccjit_selftest_ptrret(void) { MCC_TRACE("enter\n");
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

PUB_FUNC int mccjit_selftest_purity(void) { MCC_TRACE("enter\n");
	static const struct {
		const char *src;
		const char *fn;
		int nostdlib;
		int want;
		int lie;
	} cases[12] = {
			{"int f(int x){return x*2+1;}", "f", 1, AST_PURITY_TIER0,
			 AST_PURITY_IMPURE},
			{"int g(int *p, int x){return p ? *p + x : -1;}", "g", 1,
			 AST_PURITY_TIER1, AST_PURITY_TIER0},
			{"int s(int *p, int x){*p = x; return x;}", "s", 1, AST_PURITY_IMPURE,
			 AST_PURITY_TIER0},
			{"int abs(int); int h(int x){return abs(x) + 1;}", "h", 1,
			 AST_PURITY_IMPURE, AST_PURITY_TIER0},
			{"int gi; int inc(int x){gi++; return x;}", "inc", 1, AST_PURITY_IMPURE,
			 AST_PURITY_TIER0},
			{"int gi; int dec(int x){gi--; return x;}", "dec", 1, AST_PURITY_IMPURE,
			 AST_PURITY_TIER0},
			{"int gi; int post(int x){return gi++ + x;}", "post", 1,
			 AST_PURITY_IMPURE, AST_PURITY_TIER0},
			{"int bump(int *p, int x){return (*p)++ + x;}", "bump", 1,
			 AST_PURITY_IMPURE, AST_PURITY_TIER1},
			{"int deref(const char **x, int u){return *(*x)++ + u;}", "deref", 1,
			 AST_PURITY_IMPURE, AST_PURITY_TIER1},
			{"int gr; int rd(int x){return gr + x;}", "rd", 1, AST_PURITY_TIER1,
			 AST_PURITY_TIER0},
			{"int ga[8]; int idx(int x){return ga[x & 7];}", "idx", 1,
			 AST_PURITY_TIER1, AST_PURITY_TIER0},
			{"int sat(int x){return x < 0 ? 0 : (x > 99 ? 99 : x);}", "sat", 1,
			 AST_PURITY_TIER0, AST_PURITY_IMPURE},
	};
	int ncases = (int)(sizeof cases / sizeof cases[0]);
	int lying = mcc_env_on("MCC_JIT_SELFTEST_PURITY_LIE");
	int caught = 0;
	int fails = 0;
	int i;

	printf("mccjit-selftest-purity: begin\n");
	if (lying) { MCC_TRACE("br\n");
		printf("mccjit-selftest-purity: known-positive: every want below is "
					 "deliberately falsified;\n");
		printf("mccjit-selftest-purity: known-positive: each of the %d rows must "
					 "therefore report FAIL\n",
					 ncases);
	}
	printf("mccjit-selftest-purity: classify  fn     got       want      ok\n");
	for (i = 0; i < ncases; i++) { MCC_TRACE("br\n");
		unsigned char *blob;
		size_t blen;
		MCCState *s1;
		int got, ok, want = lying ? cases[i].lie : cases[i].want;
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
		ok = (got == want);
		printf("mccjit-selftest-purity:           %-6s %-9s %-9s %s\n",
					 cases[i].fn, mccjit_purity_name(got), mccjit_purity_name(want),
					 ok ? "OK" : "FAIL");
		if (!ok)
			{ MCC_TRACE("br\n"); caught++; }
		if (!ok && !lying)
			{ MCC_TRACE("br\n"); fails++; }
		mcc_free(blob);
		mcc_delete(s1);
	}

	if (lying) { MCC_TRACE("br\n");
		int missed = ncases - caught;
		printf("mccjit-selftest-purity: known-positive: %d of %d falsified rows "
					 "reported FAIL, %d slipped through %s\n",
					 caught, ncases, missed, missed ? "FAIL" : "OK");
		if (missed) { MCC_TRACE("br\n");
			printf("mccjit-selftest-purity: known-positive: a row that passes "
						 "against a falsified want is not comparing anything\n");
			fails++;
		}
		printf("mccjit-selftest-purity: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
					 fails, fails == 1 ? "" : "s");
		return fails ? 1 : 0;
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

PUB_FUNC int mccjit_selftest_lazy(void) { MCC_TRACE("enter\n");
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
#if defined(MCCJIT_I386)
	if (!mccjit_stub_tail_active()) { MCC_TRACE("br\n");
		printf("mccjit-selftest-lazy: skipped — i386 stub tail gated off (drop --jit-no-i386-stubs)\n");
		printf("mccjit-selftest-lazy: PASS (0 failures)\n");
		return 0;
	}
#elif !MCCJIT_HAVE_STUB_TAIL
	printf("mccjit-selftest-lazy: skipped — no KGC stub tail on this arch (x86_64/arm64 only)\n");
	printf("mccjit-selftest-lazy: PASS (0 failures)\n");
	return 0;
#endif

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

/* T-mac-30295 slice 2: NON-VACUOUS live proof of the wall-clock final-selection
 * path in mccjit_lazy_search — a real stashed blob (so the vocab yields >=2
 * benchmarkable candidates) plus manually-seeded profiled samples (so nsample>0)
 * drives the full MCC_JIT_SEARCH_WALLCLOCK rank+install, and the selected variant
 * is verified to compute f(x) correctly (selection preserves semantics). */
PUB_FUNC int mccjit_selftest_search_wallclock(void) { MCC_TRACE("enter\n");
	static const char src[] = "int f(int x){return x*2 + x*2 + 1;}";
	unsigned char *blob;
	size_t blen;
	MCCState *s1 = NULL, *bstate;
	int (*baseline)(int) = NULL;
	void *slot = NULL, *best;
	MccjitCounterState st;
	int fails = 0, routed = 0, i;
	int inputs[4] = {5, 12, -3, 100};

	printf("mccjit-selftest-search-wallclock: begin (live mccjit_lazy_search "
				 "wall-clock rank+install)\n");
#if defined(MCCJIT_I386)
	if (!mccjit_stub_tail_active()) { MCC_TRACE("br\n");
		printf("mccjit-selftest-search-wallclock: skipped — i386 stub tail gated off\n");
		printf("mccjit-selftest-search-wallclock: PASS (0 failures)\n");
		return 0;
	}
#elif !MCCJIT_HAVE_STUB_TAIL
	printf("mccjit-selftest-search-wallclock: skipped — no KGC stub tail on this arch\n");
	printf("mccjit-selftest-search-wallclock: PASS (0 failures)\n");
	return 0;
#endif
	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-search-wallclock: stash failed\n");
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return 1;
	}
	baseline = (int (*)(int))mcc_jit_recompile_blob(blob, blen);
	bstate = mccjit_last_state;
	mccjit_last_state = NULL;
	if (!baseline) { MCC_TRACE("br\n");
		printf("mccjit-selftest-search-wallclock: baseline recompile NULL\n");
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
	st.threshold = 8;
	pthread_mutex_init(&st.lock, NULL);
	st.nsample = 4;
	for (i = 0; i < st.nsample; i++)
		{ MCC_TRACE("br\n"); st.sample[i][0] = (int64_t)inputs[i]; }

	setenv("MCC_JIT_SEARCH", "1", 1);
	setenv("MCC_JIT_SEARCH_WALLCLOCK", "1", 1);
	setenv("MCC_JIT_BENCH_ITERS", "2000", 1);
	setenv("MCC_JIT_BENCH_ROUNDS", "5", 1);
	setenv("MCC_JIT_BENCH_MARGIN_PCT", "10", 1);
	best = mccjit_lazy_search(&st, &routed, 0);
	unsetenv("MCC_JIT_SEARCH");
	unsetenv("MCC_JIT_SEARCH_WALLCLOCK");
	unsetenv("MCC_JIT_BENCH_ITERS");
	unsetenv("MCC_JIT_BENCH_ROUNDS");
	unsetenv("MCC_JIT_BENCH_MARGIN_PCT");

	printf("mccjit-selftest-search-wallclock: lazy_search best=%p routed=%d "
				 "(expect non-NULL) %s\n", best, routed, best ? "OK" : "FAIL");
	if (!best)
		{ MCC_TRACE("br\n"); fails++; }
	else { MCC_TRACE("br\n");
		for (i = 0; i < 4; i++) { MCC_TRACE("br\n");
			int x = inputs[i];
			int got = ((int (*)(int))best)(x);
			int want = x * 2 + x * 2 + 1;
			int ok = (got == want);
			printf("mccjit-selftest-search-wallclock: selected f(%d)=%d expect=%d %s\n",
						 x, got, want, ok ? "OK" : "FAIL");
			if (!ok)
				{ MCC_TRACE("br\n"); fails++; }
		}
	}
	pthread_mutex_destroy(&st.lock);
	if (bstate)
		{ MCC_TRACE("br\n"); mcc_delete(bstate); }
	mccjit_last_state = NULL;
	mcc_free(blob);
	mcc_delete(s1);
	printf("mccjit-selftest-search-wallclock: %s (%d failure%s)\n",
				 fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

static void mccjit_pool_nap(void) { MCC_TRACE("enter\n");
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 1000000;
	nanosleep(&ts, NULL);
}

static void *mccjit_dispatch_entry(void **slot, void *fallback) { MCC_TRACE("enter\n");
#if defined(MCCJIT_X64)
	unsigned char *p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
													MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	int o = 0;
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return fallback; }
	p[o++] = 0x55;
	p[o++] = 0x48, p[o++] = 0x89, p[o++] = 0xe5;
	p[o++] = 0x48, p[o++] = 0xb8;
	memcpy(p + o, &slot, 8), o += 8;
	p[o++] = 0x48, p[o++] = 0x8b, p[o++] = 0x00;
	p[o++] = 0xff, p[o++] = 0xe0;
	return p;
#elif defined(MCCJIT_I386)
	unsigned char *p;
	int o = 0;
	uint32_t sp;
	if (!mccjit_i386_stubs_enabled())
		{ MCC_TRACE("br\n"); return fallback; }
	p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
					 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return fallback; }
	p[o++] = 0x55;
	p[o++] = 0x89, p[o++] = 0xe5;
	p[o++] = 0xa1;
	sp = (uint32_t)(uintptr_t)slot;
	memcpy(p + o, &sp, 4), o += 4;
	p[o++] = 0xff, p[o++] = 0xe0;
	return p;
#else
	(void)slot;
	return fallback;
#endif
}

PUB_FUNC int mccjit_selftest_pool(void) { MCC_TRACE("enter\n");
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
#if defined(MCCJIT_I386)
	if (!mccjit_stub_tail_active()) { MCC_TRACE("br\n");
		printf("mccjit-selftest-pool: skipped — i386 stub tail gated off (drop --jit-no-i386-stubs)\n");
		printf("mccjit-selftest-pool: PASS (0 failures)\n");
		return 0;
	}
#elif !MCCJIT_HAVE_STUB_TAIL
	printf("mccjit-selftest-pool: skipped — no KGC stub tail on this arch (x86_64/arm64 only)\n");
	printf("mccjit-selftest-pool: PASS (0 failures)\n");
	return 0;
#endif

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
		MccjitSwapJob *job = mccjit_swap_job_new();
		void *pub = (void *)baseline;
		int spins = 0;
		if (!job) { MCC_TRACE("br\n");
			printf("mccjit-selftest-pool: eager job alloc failed FAIL\n");
			fails++;
		} else { MCC_TRACE("br\n");
			job->slot = &slot_a;
			job->blob = blob;
			job->len = blen;
			job->max_duration = 0;
			job->timed = 0;
			if (!mccjit_pool_submit(mccjit_job_run_eager, job)) { MCC_TRACE("br\n");
				printf("mccjit-selftest-pool: eager job refused by the pool FAIL\n");
				fails++;
			}
			while (spins++ < 5000) { MCC_TRACE("br\n");
				pub = (void *)(uintptr_t)__atomic_load_n(&slot_a, __ATOMIC_ACQUIRE);
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

PUB_FUNC int mccjit_selftest_slicelive(void) { MCC_TRACE("enter\n");
	static const char src[] = "int f(int x){return x*2+1;}";
	static void *slot;
	static MccjitCounterState st;
	unsigned char *blob;
	size_t blen;
	MCCState *s1;
	int (*baseline)(int) = NULL;
	int inputs[6] = {5, 0, -3, 100, 7, -40};
	int fails = 0, i, nw, spins = 0;
	void *promoted = NULL;

	printf("mccjit-selftest-slicelive: begin (STEP 3 live slice hot-swap)\n");
#if !MCCJIT_HAVE_STUB_TAIL
	printf("mccjit-selftest-slicelive: skipped — no KGC stub tail on this arch (x86_64/arm64 only)\n");
	printf("mccjit-selftest-slicelive: PASS (0 failures)\n");
	return 0;
#endif
	setenv("MCC_JIT_SEARCH_SLICE", "1", 1);

	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-slicelive: stash failed FAIL\n");
		if (s1)
			{ MCC_TRACE("br\n"); mcc_delete(s1); }
		mcc_free(blob);
		return 1;
	}
	baseline = (int (*)(int))mcc_jit_recompile_blob(blob, blen);
	mccjit_last_state = NULL;
	if (!baseline) { MCC_TRACE("br\n");
		printf("mccjit-selftest-slicelive: baseline recompile NULL FAIL\n");
		mcc_free(blob);
		mcc_delete(s1);
		return 1;
	}

	nw = mccjit_pool_start(2);
	if (nw <= 0) { MCC_TRACE("br\n");
		printf("mccjit-selftest-slicelive: pool start failed FAIL\n");
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
	st.threshold = 4;
	pthread_mutex_init(&st.lock, NULL);
	st.nsample = 6;
	for (i = 0; i < 6; i++)
		{ MCC_TRACE("br\n"); st.sample[i][0] = inputs[i]; }
	st.argseen = 6;

	for (i = 0; i < 3; i++)
		{ MCC_TRACE("br\n"); (void)mccjit_counter_tick(&st, NULL); }
	(void)mccjit_counter_tick(&st, NULL);

	while (spins++ < 5000) { MCC_TRACE("br\n");
		pthread_mutex_lock(&st.lock);
		promoted = st.promoted;
		pthread_mutex_unlock(&st.lock);
		if (promoted)
			{ MCC_TRACE("br\n"); break; }
		mccjit_pool_nap();
	}
	if (!promoted || promoted == (void *)baseline) { MCC_TRACE("br\n");
		printf("mccjit-selftest-slicelive: slice promote never landed (promoted=%p baseline=%p) FAIL\n",
					 promoted, (void *)baseline);
		fails++;
	} else { MCC_TRACE("br\n");
		void *disp = mccjit_dispatch_entry(&slot, promoted);
		printf("mccjit-selftest-slicelive: slice-kernel promoted=%p slot=%p\n", promoted,
					 (void *)slot);
		for (i = 0; i < 6; i++) { MCC_TRACE("br\n");
			int x = inputs[i];
			int got = ((int (*)(int))disp)(x);
			int want = x * 2 + 1;
			int ok = got == want;
			printf("mccjit-selftest-slicelive: f(%d)=%d expect=%d %s\n", x, got, want,
						 ok ? "OK" : "FAIL");
			if (!ok)
				{ MCC_TRACE("br\n"); fails++; }
		}
	}

	pthread_mutex_destroy(&st.lock);
	mcc_free(blob);
	mcc_delete(s1);
	printf("mccjit-selftest-slicelive: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

PUB_FUNC int mccjit_selftest_eligibility(void) { MCC_TRACE("enter\n");
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
#if defined(MCCJIT_I386)
	if (!mccjit_stub_tail_active()) { MCC_TRACE("br\n");
		printf("mccjit-selftest-eligibility: skipped — i386 stub tail gated off (drop --jit-no-i386-stubs)\n");
		printf("mccjit-selftest-eligibility: PASS (0 failures)\n");
		return 0;
	}
#elif !MCCJIT_HAVE_STUB_TAIL
	printf("mccjit-selftest-eligibility: skipped — no KGC stub tail on this arch (x86_64/arm64 only)\n");
	printf("mccjit-selftest-eligibility: PASS (0 failures)\n");
	return 0;
#endif

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

PUB_FUNC int mccjit_selftest_submit(void) { MCC_TRACE("enter\n");
	unsigned char *base = NULL, *ovr = NULL, *ovcopy = NULL;
	size_t base_len = 0, ov_len = 0;
	MCCState *sb = NULL, *so = NULL;
	MccjitIntent it_base;
	int64_t anchor;
	int fails = 0;
	int (*e)(int) = NULL;
	unsigned char *gb = NULL;
	size_t gl = 0;
	uint64_t gm = 0;
	Sym snull;
	memset(&it_base, 0, sizeof it_base);
	memset(&snull, 0, sizeof snull);

	base = mccjit_stash_one("int f(int x){return x+1000;}", "f", 1, &base_len, &sb);
	ovr = mccjit_stash_one("int f(int x){return x*3;}", "f", 1, &ov_len, &so);
	if (!base || !ovr || mccjit_intent_deserialize(base, base_len, &it_base) != 0) { MCC_TRACE("br\n");
		printf("mccjit-selftest-submit: stash FAIL\n");
		fails = 1;
		goto done;
	}
	anchor = it_base.anchor_sym_v;

	e = (int (*)(int))mcc_jit_recompile_blob(base, base_len);
	if (!e || e(5) != 1005)
		{ MCC_TRACE("br\n"); printf("mccjit-selftest-submit: baseline f(5)!=1005 FAIL\n"); fails++; }

	ovcopy = mcc_malloc(ov_len ? ov_len : 1);
	memcpy(ovcopy, ovr, ov_len);
	mccjit_override_put(anchor, ovcopy, ov_len, 0, 0);
	if (!mccjit_override_get(anchor, &gb, &gl, &gm) || gb != ovcopy || gl != ov_len)
		{ MCC_TRACE("br\n"); printf("mccjit-selftest-submit: table store FAIL\n"); fails++; }

	e = (int (*)(int))mcc_jit_recompile_blob(base, base_len);
	if (!e || e(5) != 15)
		{ MCC_TRACE("br\n");
			printf("mccjit-selftest-submit: override f(5)=%d want 15 FAIL\n", e ? e(5) : -1);
			fails++; }

	mccjit_override_n = 0;
	mcc_free(ovcopy);
	ovcopy = NULL;
	e = (int (*)(int))mcc_jit_recompile_blob(base, base_len);
	if (!e || e(5) != 1005)
		{ MCC_TRACE("br\n"); printf("mccjit-selftest-submit: post-clear f(5)!=1005 FAIL\n"); fails++; }

	if (mcc_jit_submit_ast(NULL, it_base.arena, 0, 0) != -1 ||
			mcc_jit_submit_ast(&snull, NULL, 0, 0) != -1)
		{ MCC_TRACE("br\n"); printf("mccjit-selftest-submit: null-guard FAIL\n"); fails++; }

done:
	mccjit_override_n = 0;
	mcc_free(ovcopy);
	mccjit_intent_release(&it_base);
	mcc_free(base);
	mcc_free(ovr);
	if (sb) { MCC_TRACE("br\n"); mcc_delete(sb); }
	if (so) { MCC_TRACE("br\n"); mcc_delete(so); }
	printf("mccjit-selftest-submit: backend override %s\n", fails ? "FAIL" : "OK");
	return fails ? 1 : 0;
}

PUB_FUNC int mccjit_selftest_fork(void) { MCC_TRACE("enter\n");
#if MCC_HOST_WIN32
	printf("mccjit-selftest-fork: begin\n");
	printf("mccjit-selftest-fork: skipped — no fork() on Windows\n");
	printf("mccjit-selftest-fork: PASS (0 failures)\n");
	return 0;
#else
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
#endif
}

PUB_FUNC int mccjit_selftest_observability(void) { MCC_TRACE("enter\n");
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
		mccjit_perf_map_path(path, sizeof path);
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

PUB_FUNC int mccjit_selftest_liverun(const char *libpath, const char *incpath) { MCC_TRACE("enter\n");
	static const char src[] =
			"int f(int x){return x*3+7;}\nint main(void){return f(11);}\n";
	MCCState *s;
	char *av[] = {"liverun", NULL};
	char path[64];
	int fails = 0;
	int rc;
	int perf_found = 0;

	printf("mccjit-selftest-liverun: begin\n");
#if defined(MCCJIT_I386)
	if (!mccjit_stub_tail_active()) { MCC_TRACE("br\n");
		printf("mccjit-selftest-liverun: skipped — i386 stub tail gated off (drop --jit-no-i386-stubs)\n");
		printf("mccjit-selftest-liverun: PASS (0 failures)\n");
		return 0;
	}
#elif !MCCJIT_HAVE_STUB_TAIL
	printf("mccjit-selftest-liverun: skipped — no KGC stub tail on this arch (x86_64/arm64 only)\n");
	printf("mccjit-selftest-liverun: PASS (0 failures)\n");
	return 0;
#endif

	setenv("MCC_AST_JIT_DISPATCH", "6", 1);
	setenv("MCC_JIT_PERF_MAP", "1", 1);
	mccjit_perf_map_path(path, sizeof path);
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

PUB_FUNC int mccjit_selftest_poison(void) { MCC_TRACE("enter\n");
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
		kgc.nearmatch_on = 0;
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

static volatile long mccjit_bench_barrier;

static long mccjit_bench_fast_fn(long x) { MCC_TRACE("enter\n");
	long s = x;
	int i;
	for (i = 0; i < 30; i++)
		{ MCC_TRACE("br\n"); s = s * 2654435761L + mccjit_bench_barrier; }
	return s;
}

static long mccjit_bench_slow_fn(long x) { MCC_TRACE("enter\n");
	long s = x;
	int i;
	for (i = 0; i < 900; i++)
		{ MCC_TRACE("br\n"); s = s * 2654435761L + mccjit_bench_barrier; }
	return s;
}

static long mccjit_bench_mid_fn(long x) { MCC_TRACE("enter\n");
	long s = x;
	int i;
	for (i = 0; i < 200; i++)
		{ MCC_TRACE("br\n"); s = s * 2654435761L + mccjit_bench_barrier; }
	return s;
}

PUB_FUNC int mccjit_selftest_bench(void) { MCC_TRACE("enter\n");
	int64_t tuples[4 * MCCJIT_KGC_ARITY];
	uint32_t nt = 4, i, j;
	int fails = 0;
	int r_win, r_lose, r_tie;

	printf("mccjit-selftest-bench: begin\n");
	setenv("MCC_JIT_BENCH_ITERS", "4000", 1);
	setenv("MCC_JIT_BENCH_MARGIN_PCT", "30", 1);
	setenv("MCC_JIT_BENCH_ROUNDS", "51", 1);
	for (i = 0; i < nt; i++)
		{ MCC_TRACE("br\n"); for (j = 0; j < MCCJIT_KGC_ARITY; j++)
			{ MCC_TRACE("br\n"); tuples[i * MCCJIT_KGC_ARITY + j] = (int64_t)(i * 7 + 1); } }

	r_win = mccjit_bench_pair((void *)mccjit_bench_fast_fn,
													 (void *)mccjit_bench_slow_fn, tuples, nt, 1, 1, 0);
	r_lose = mccjit_bench_pair((void *)mccjit_bench_slow_fn,
														(void *)mccjit_bench_fast_fn, tuples, nt, 1, 1, 0);
	r_tie = mccjit_bench_pair((void *)mccjit_bench_slow_fn,
													 (void *)mccjit_bench_slow_fn, tuples, nt, 1, 1, 0);
	unsetenv("MCC_JIT_BENCH_ITERS");
	unsetenv("MCC_JIT_BENCH_MARGIN_PCT");
	unsetenv("MCC_JIT_BENCH_ROUNDS");

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

#if defined(MCCJIT_X64)
static unsigned char *mccjit_patch_make_slot(void *target, void ***slotout) { MCC_TRACE("enter\n");
	unsigned char *p = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
													MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	int32_t disp;
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return NULL; }
	p[0] = 0xff;
	p[1] = 0x25;
	disp = 2;
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
#elif defined(MCCJIT_ARM64)
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
	insns[0] = 0xd2800011u | (uint32_t)((a & 0xffff) << 5);
	insns[1] = 0xf2a00011u | (uint32_t)(((a >> 16) & 0xffff) << 5);
	insns[2] = 0xf2c00011u | (uint32_t)(((a >> 32) & 0xffff) << 5);
	insns[3] = 0xf2e00011u | (uint32_t)(((a >> 48) & 0xffff) << 5);
	insns[4] = 0xf9400230u;
	insns[5] = 0xd61f0200u;
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

static void mccjit_patch_tramp_words(uint64_t a, uint32_t w[4]) { MCC_TRACE("enter\n");
	w[0] = 0xd2800010u | (uint32_t)((a & 0xffff) << 5);
	w[1] = 0xf2a00010u | (uint32_t)(((a >> 16) & 0xffff) << 5);
	w[2] = 0xf2c00010u | (uint32_t)(((a >> 32) & 0xffff) << 5);
	w[3] = 0xf2e00010u | (uint32_t)(((a >> 48) & 0xffff) << 5);
}

static unsigned char *mccjit_patch_make_tramp(void *target,
																							unsigned char **codeout) { MCC_TRACE("enter\n");
	size_t page = host_pagesize();
	unsigned char *p;
	uint32_t insns[5];
	uint64_t a = (uint64_t)(uintptr_t)target;
	p = mmap(0, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		{ MCC_TRACE("br\n"); return NULL; }
	mccjit_patch_tramp_words(a, insns);
	insns[4] = 0xd61f0200u;
	memcpy(p, insns, sizeof insns);
	if (host_runmem_protect(p, page, HOST_PROT_RX) != 0) { MCC_TRACE("br\n");
		munmap(p, page);
		return NULL;
	}
	if (codeout)
		{ MCC_TRACE("br\n"); *codeout = p; }
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

#if defined(MCCJIT_X64) || defined(MCCJIT_ARM64)
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

#if defined(MCCJIT_X64)
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

#if defined(MCCJIT_ARM64)
static void mccjit_patch_free_runmem(void *entry) { MCC_TRACE("enter\n");
	munmap(entry, host_pagesize());
}

static void *mccjit_patch_mk_tramp(void *target, void **handle) { MCC_TRACE("enter\n");
	unsigned char *code = NULL;
	void *entry = mccjit_patch_make_tramp(target, &code);
	if (!entry)
		{ MCC_TRACE("br\n"); return NULL; }
	if (handle)
		{ MCC_TRACE("br\n"); *handle = code; }
	return entry;
}

static void mccjit_patch_swap_imm(void *handle, void *target) { MCC_TRACE("enter\n");
	unsigned char *p = handle;
	size_t page = host_pagesize();
	uint32_t words[4];
	mccjit_patch_tramp_words((uint64_t)(uintptr_t)target, words);
	if (host_runmem_protect(p, page, HOST_PROT_RW) != 0)
		{ MCC_TRACE("br\n"); return; }
	memcpy(p, words, sizeof words);
	host_runmem_protect(p, page, HOST_PROT_RX);
}
#endif

static const MccjitPatchStrategy mccjit_patch_reg[] = {
		{"c-indirect", (unsigned)sizeof(void *), mccjit_patch_avail_yes,
		 mccjit_patch_mk_cell, mccjit_patch_swap_store, mccjit_patch_call_cell,
		 mccjit_patch_free_cell},
#if defined(MCCJIT_X64)
		{"ptr-swap-slot", 16, mccjit_patch_avail_yes, mccjit_patch_mk_slot,
		 mccjit_patch_swap_store, mccjit_patch_call_code, mccjit_patch_free_page},
		{"inplace-tramp", 12, mccjit_patch_avail_yes, mccjit_patch_mk_tramp,
		 mccjit_patch_swap_imm, mccjit_patch_call_code, mccjit_patch_free_page},
#elif defined(MCCJIT_ARM64)
		{"ptr-swap-slot", 16, mccjit_patch_avail_yes, mccjit_patch_mk_slot,
		 mccjit_patch_swap_store, mccjit_patch_call_code, mccjit_patch_free_runmem},
		{"inplace-tramp", 20, mccjit_patch_avail_yes, mccjit_patch_mk_tramp,
		 mccjit_patch_swap_imm, mccjit_patch_call_code, mccjit_patch_free_runmem},
#endif
		{"nop-pad-d3b", 8, mccjit_patch_avail_no, NULL, NULL, NULL, NULL},
};

#define MCCJIT_PATCH_NREG (int)(sizeof mccjit_patch_reg / sizeof mccjit_patch_reg[0])

static long mccjit_patch_iters(void) { MCC_TRACE("enter\n");
	return mcc_env_num("MCC_JIT_PATCH_ITERS", 500000);
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

PUB_FUNC int mccjit_selftest_patch(void) { MCC_TRACE("enter\n");
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

PUB_FUNC int mccjit_selftest_qsbr(void) { MCC_TRACE("enter\n");
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

#define MCCJIT_SD_STARTED 0
#define MCCJIT_SD_DRAINED 1
#define MCCJIT_SD_JOINED 2
#define MCCJIT_SD_QUEUE 3
#define MCCJIT_SD_QSBR 4
#define MCCJIT_SD_REFUSED 5
#define MCCJIT_SD_RESTART 6
#define MCCJIT_SD_IDEMPOTENT 7
#define MCCJIT_SD_SURVIVES 8
#define MCCJIT_SD_BOUNDED 9
#define MCCJIT_SD_CONCURRENT 10
#define MCCJIT_SD_NTAG 11
#define MCCJIT_SD_JOBS 64
#define MCCJIT_SD_PROGRESS 4
#define MCCJIT_SD_PROGRESS_SPINS 500
#define MCCJIT_SD_RACERS 3

static const char *const mccjit_sd_tag[MCCJIT_SD_NTAG] = {
		"pool-started",   "jobs-drained",    "workers-joined", "queue-empty",
		"qsbr-reclaimed", "enqueue-refused", "restart-refused", "shutdown-idempotent",
		"pool-survives-flush", "teardown-bounded", "pool-concurrent"};

static char mccjit_sd_fired[MCCJIT_SD_NTAG];

static struct {
	pthread_mutex_t lock;
	unsigned long done;
	const void *blob;
	unsigned long len;
} mccjit_sd = {PTHREAD_MUTEX_INITIALIZER, 0, NULL, 0};

typedef struct {
	int cap;
	unsigned long accepted;
	unsigned long refused;
} MccjitSdRacer;

static int mccjit_sd_check(int tag, int ok, const char *detail) { MCC_TRACE("enter\n");
	printf("mccjit-selftest-shutdown: %s: %s %s\n", mccjit_sd_tag[tag], detail,
				 ok ? "OK" : "FAIL");
	if (!ok)
		{ MCC_TRACE("br\n"); mccjit_sd_fired[tag] = 1; }
	return ok ? 0 : 1;
}

static void mccjit_sd_tick(void) { MCC_TRACE("enter\n");
	pthread_mutex_lock(&mccjit_sd.lock);
	mccjit_sd.done++;
	pthread_mutex_unlock(&mccjit_sd.lock);
}

enum {
	MCCJIT_SD_HEAVY_RECOMPILE = 0,
	MCCJIT_SD_HEAVY_RETIRE = 1,
	MCCJIT_SD_HEAVY_NAP = 2,
	MCCJIT_SD_HEAVY_TICK = 3
};

static int mccjit_sd_job_heavy(void *ctx) { MCC_TRACE("enter\n");
	MccjitSwapJob *job = ctx;
	void *pg;
	switch (job->resume) { MCC_TRACE("br\n");
	case MCCJIT_SD_HEAVY_RECOMPILE:
		if (mccjit_sd.blob) { MCC_TRACE("br\n");
			mccjit_codegen_lock();
			mcc_jit_recompile_blob(mccjit_sd.blob, (size_t)mccjit_sd.len);
			mccjit_codegen_unlock();
		}
		job->resume = MCCJIT_SD_HEAVY_RETIRE;
		return MCC_TASK_YIELDED;
	case MCCJIT_SD_HEAVY_RETIRE:
		pg = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (pg != MAP_FAILED)
			{ MCC_TRACE("br\n"); mccjit_qsbr_retire(pg, 4096); }
		job->resume = MCCJIT_SD_HEAVY_NAP;
		return MCC_TASK_YIELDED;
	case MCCJIT_SD_HEAVY_NAP:
		mccjit_conc_enter();
		mccjit_pool_nap();
		mccjit_conc_leave();
		job->resume = MCCJIT_SD_HEAVY_TICK;
		return MCC_TASK_YIELDED;
	default:
		mccjit_sd_tick();
		return MCC_TASK_DONE;
	}
}

static int mccjit_sd_job_light(void *ctx) { MCC_TRACE("enter\n");
	(void)ctx;
	mccjit_sd_tick();
	return MCC_TASK_DONE;
}

static void *mccjit_sd_racer(void *arg) { MCC_TRACE("enter\n");
	MccjitSdRacer *r = (MccjitSdRacer *)arg;
	int i;
	for (i = 0; i < r->cap; i++) { MCC_TRACE("br\n");
		MccjitSwapJob *job = mccjit_swap_job_new();
		if (!job)
			{ MCC_TRACE("br\n"); break; }
		if (!mccjit_pool_submit(mccjit_sd_job_light, job)) { MCC_TRACE("br\n");
			r->refused++;
			break;
		}
		r->accepted++;
		mccjit_pool_nap();
	}
	return NULL;
}

PUB_FUNC int mccjit_selftest_shutdown(int mode) { MCC_TRACE("enter\n");
	static const char src[] = "int f(int x){return x*2+1;}";
	static const int want[] = {MCCJIT_SD_DRAINED, MCCJIT_SD_JOINED,
														 MCCJIT_SD_QUEUE,   MCCJIT_SD_QSBR,
														 MCCJIT_SD_REFUSED, MCCJIT_SD_RESTART,
														 MCCJIT_SD_BOUNDED};
	int known_positive = (mode == 1);
	int swap_wide = (mode == 2);
	MccjitSdRacer racer[MCCJIT_SD_RACERS];
	pthread_t rth[MCCJIT_SD_RACERS];
	unsigned char *blob = NULL;
	size_t blen = 0;
	MCCState *s1 = NULL;
	unsigned long accepted = 0, refused = 0, heavy = 0, done;
	char msg[224];
	int fails = 0, missing = 0, nw, i, nr = 0;

	printf("mccjit-selftest-shutdown: begin%s\n",
				 known_positive ? " (known-positive: MCC_JIT_SHUTDOWN=0)"
				 : swap_wide		 ? " (swap-wide: MCC_JIT_SWAP_WIDE=1)"
												 : "");
	if (known_positive)
		{ MCC_TRACE("br\n"); setenv("MCC_JIT_SHUTDOWN", "0", 1); }
	if (swap_wide)
		{ MCC_TRACE("br\n"); setenv("MCC_JIT_SWAP_WIDE", "1", 1); }

	mccjit_conc_reset();
	mccjit_qsbr_reset();
	blob = mccjit_stash_one(src, "f", 1, &blen, &s1);
	if (s1 && blob) { MCC_TRACE("br\n");
		mccjit_sd.blob = blob;
		mccjit_sd.len = (unsigned long)blen;
	}
	printf("mccjit-selftest-shutdown: load = %d jobs x (%s + qsbr retire + 1ms)\n",
				 MCCJIT_SD_JOBS, mccjit_sd.blob ? "recompile" : "no-blob");

	nw = mccjit_pool_start(4);
	snprintf(msg, sizeof msg, "workers=%d", nw);
	fails += mccjit_sd_check(MCCJIT_SD_STARTED, nw > 0, msg);
	if (nw <= 0) { MCC_TRACE("br\n");
		printf("mccjit-selftest-shutdown: FAIL (pool would not start)\n");
		return 1;
	}

	for (i = 0; i < MCCJIT_SD_JOBS; i++) { MCC_TRACE("br\n");
		MccjitSwapJob *job = mccjit_swap_job_new();
		if (!job)
			{ MCC_TRACE("br\n"); break; }
		if (!mccjit_pool_submit(mccjit_sd_job_heavy, job))
			{ MCC_TRACE("br\n"); break; }
		heavy++;
	}
	accepted = heavy;

	{
		MCCState *s2 = mcc_new();
		int alive;
		if (s2)
			{ MCC_TRACE("br\n"); mcc_delete(s2); }
		mccjit_kgc_flush_all();
		alive = mccjit_pool_ready();
		snprintf(msg, sizeof msg,
						 "pool ready after a mid-run mcc_delete + kgc flush=%d", alive);
		fails += mccjit_sd_check(MCCJIT_SD_SURVIVES, alive, msg);
	}

	for (i = 0; i < MCCJIT_SD_RACERS; i++) { MCC_TRACE("br\n");
		racer[i].cap = known_positive ? 8 : 5000;
		racer[i].accepted = 0;
		racer[i].refused = 0;
		if (pthread_create(&rth[i], NULL, mccjit_sd_racer, &racer[i]) == 0)
			{ MCC_TRACE("br\n"); nr++; }
	}

	{
		int spins = 0;
		for (;;) { MCC_TRACE("br\n");
			unsigned long d;
			pthread_mutex_lock(&mccjit_sd.lock);
			d = mccjit_sd.done;
			pthread_mutex_unlock(&mccjit_sd.lock);
			if (d >= MCCJIT_SD_PROGRESS || spins >= MCCJIT_SD_PROGRESS_SPINS)
				{ MCC_TRACE("br\n"); break; }
			mccjit_pool_nap();
			spins++;
		}
	}
	mccjit_shutdown();

	for (i = 0; i < nr; i++) { MCC_TRACE("br\n");
		pthread_join(rth[i], NULL);
		accepted += racer[i].accepted;
		refused += racer[i].refused;
	}

	pthread_mutex_lock(&mccjit_sd.lock);
	done = mccjit_sd.done;
	pthread_mutex_unlock(&mccjit_sd.lock);

	snprintf(msg, sizeof msg,
					 "ran=%lu abandoned=%lu discarded=%lu accepted=%lu (racers=%d)", done,
					 mccjit_pool.nabandoned, mccjit_pool.ndiscarded, accepted, nr);
	fails += mccjit_sd_check(MCCJIT_SD_DRAINED,
													 done + mccjit_pool.nabandoned +
																			 mccjit_pool.ndiscarded ==
																		 accepted &&
															 done >= 1 && mccjit_pool.nabandoned >= 1,
													 msg);

	snprintf(msg, sizeof msg, "quit-seen=%d ticks-after-quit=%lu workers=%d",
					 mccjit_pool.quit_seen, mccjit_pool.nticks_after_quit, nw);
	fails += mccjit_sd_check(MCCJIT_SD_BOUNDED,
													 mccjit_pool.quit_seen &&
															 mccjit_pool.nticks_after_quit <= (unsigned long)nw,
													 msg);

	snprintf(msg, sizeof msg, "peak-workers-in-nap=%d workers=%d wide=%d",
					 mccjit_conc.peak, nw, swap_wide);
	if (swap_wide) { MCC_TRACE("br\n");
		fails += mccjit_sd_check(MCCJIT_SD_CONCURRENT, mccjit_conc.peak == 1, msg);
	} else if (known_positive) { MCC_TRACE("br\n");
		printf("mccjit-selftest-shutdown: %s: %s (report-only)\n",
					 mccjit_sd_tag[MCCJIT_SD_CONCURRENT], msg);
	} else { MCC_TRACE("br\n");
		fails += mccjit_sd_check(MCCJIT_SD_CONCURRENT, mccjit_conc.peak >= 2, msg);
	}

	snprintf(msg, sizeof msg, "nworkers=%d nth=%d started=%d",
					 mccjit_pool.nworkers, mccjit_pool.nth, mccjit_pool.started);
	fails += mccjit_sd_check(MCCJIT_SD_JOINED,
													 mccjit_pool.nworkers == 0 && mccjit_pool.nth == 0 &&
															 mccjit_pool.started == 0,
													 msg);

	snprintf(msg, sizeof msg, "head=%p tail=%p", (void *)mccjit_pool.head,
					 (void *)mccjit_pool.tail);
	fails += mccjit_sd_check(MCCJIT_SD_QUEUE,
													 !mccjit_pool.head && !mccjit_pool.tail, msg);

	snprintf(msg, sizeof msg,
					 "nlimbo=%d reclaimed=%llu leaked=%llu retired=%llu heavy-enqueued=%lu",
					 mccjit_qsbr.nlimbo, (unsigned long long)mccjit_qsbr.reclaimed,
					 (unsigned long long)mccjit_qsbr.leaked,
					 (unsigned long long)mccjit_qsbr.retired, heavy);
	fails += mccjit_sd_check(MCCJIT_SD_QSBR,
													 mccjit_qsbr.nlimbo == 0 && mccjit_qsbr.leaked == 0 &&
															 mccjit_qsbr.reclaimed == mccjit_qsbr.retired,
													 msg);

	{
		MccjitSwapJob *late = mccjit_swap_job_new();
		int taken = 0;
		if (late) { MCC_TRACE("br\n");
			taken = mccjit_pool_submit(mccjit_sd_job_light, late);
		}
		snprintf(msg, sizeof msg, "racer-refusals=%lu post-shutdown-accepted=%d",
						 refused, taken);
		fails += mccjit_sd_check(MCCJIT_SD_REFUSED, refused >= 1 && !taken, msg);
	}

	snprintf(msg, sizeof msg, "restart-workers=%d", mccjit_pool_start(2));
	fails += mccjit_sd_check(MCCJIT_SD_RESTART, mccjit_pool.nworkers == 0, msg);

	mccjit_shutdown();
	snprintf(msg, sizeof msg, "second call returned, nworkers=%d",
					 mccjit_pool.nworkers);
	fails += mccjit_sd_check(MCCJIT_SD_IDEMPOTENT, mccjit_pool.nworkers == 0, msg);

	if (known_positive) { MCC_TRACE("br\n");
		setenv("MCC_JIT_SHUTDOWN", "1", 1);
		mccjit_shutdown();
	}
	if (swap_wide)
		{ MCC_TRACE("br\n"); unsetenv("MCC_JIT_SWAP_WIDE"); }

	mccjit_sd.blob = NULL;
	if (s1)
		{ MCC_TRACE("br\n"); mcc_delete(s1); }
	mcc_free(blob);
	mccjit_last_state = NULL;

	if (!known_positive) { MCC_TRACE("br\n");
		printf("mccjit-selftest-shutdown: %s (%d failure%s)\n",
					 fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s");
		return fails ? 1 : 0;
	}

	if (!fails) { MCC_TRACE("br\n");
		printf("mccjit-selftest-shutdown: FAIL known-positive: with the drain "
					 "disabled every check still passed. A cell that cannot fail is "
					 "worse than no cell.\n");
		return 1;
	}
	for (i = 0; i < (int)(sizeof want / sizeof want[0]); i++) { MCC_TRACE("br\n");
		if (!mccjit_sd_fired[want[i]]) { MCC_TRACE("br\n");
			printf("mccjit-selftest-shutdown: FAIL known-positive: %s never fired on "
						 "the undrained pool, so it is unproven and may be inert\n",
						 mccjit_sd_tag[want[i]]);
			missing++;
		}
	}
	if (missing)
		{ MCC_TRACE("br\n"); return 1; }
	printf("mccjit-selftest-shutdown: PASS known-positive: all %d checks fired "
				 "with the drain disabled (%d failure%s)\n",
				 (int)(sizeof want / sizeof want[0]), fails, fails == 1 ? "" : "s");
	return 0;
}

PUB_FUNC int mccjit_selftest_fparg(const char *libpath, const char *incpath) { MCC_TRACE("enter\n");
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
#if defined(MCCJIT_I386)
	if (!mccjit_stub_tail_active()) { MCC_TRACE("br\n");
		printf("mccjit-selftest-fparg: skipped — i386 stub tail gated off (drop --jit-no-i386-stubs)\n");
		printf("mccjit-selftest-fparg: PASS (0 failures)\n");
		return 0;
	}
#elif !MCCJIT_HAVE_STUB_TAIL
	printf("mccjit-selftest-fparg: skipped — no KGC stub tail on this arch (x86_64/arm64 only)\n");
	printf("mccjit-selftest-fparg: PASS (0 failures)\n");
	return 0;
#endif

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
		mccjit_perf_map_path(path, sizeof path);
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

PUB_FUNC int mccjit_selftest_mixed(const char *libpath, const char *incpath) { MCC_TRACE("enter\n");
#if !defined(MCCJIT_X64) && !defined(MCCJIT_I386)
	(void)libpath;
	(void)incpath;
	printf("mccjit-selftest-mixed: non-x86_64 SKIP\n");
	return 0;
#elif MCC_HOST_WIN32 || defined(MCCJIT_I386)
#if defined(MCCJIT_I386)
	if (!mccjit_stub_tail_active()) { MCC_TRACE("br\n");
		(void)libpath;
		(void)incpath;
		printf("mccjit-selftest-mixed: skipped — i386 mixed stub tail gated off "
					 "(drop --jit-no-i386-stubs)\n");
		return 0;
	}
#endif
	static const char src_f[] =
			"long f(long a, double b, long c){ return a + (long)b + c; }";
	static const char src_g[] =
			"long g(long a, double b, long c){ return a + (long)b + c + 1; }";
	static const char src_h[] =
			"double h(long a, double b){ return (double)a + b*2.0; }";
	unsigned char *blob;
	size_t blen;
	MCCState *s1, *fbstate = NULL;
	long (*fbase)(long, double, long) = NULL;
	int fails = 0;
	int mixed_seen, ngp_seen, nsse_seen, retfp_seen, retwide_seen;
	(void)libpath;
	(void)incpath;
	printf("mccjit-selftest-mixed: begin (Win64 positional mixed GP+FP)\n");

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
	retwide_seen = mccjit_last_ret_wide;
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

	{
		int64_t gp[4] = {5, 0, 3, 0};
		double fp[4] = {0, 2.5, 0, 0};
		long direct = fbase(5, 2.5, 3);
#if defined(MCCJIT_I386)
		void *i386_thunk = mccjit_i386_mixed_thunk_build(3, 0x2u,
																										 0x2u);
		int64_t thunked;
		if (!i386_thunk) { MCC_TRACE("br\n");
			printf("mccjit-selftest-mixed: i386 thunk build NULL FAIL\n");
			fails++;
		}
		mccjit_i386_active_thunk = i386_thunk;
		thunked = mccjit_invoke_mixed_i((void *)fbase, gp, fp);
#else
		int64_t thunked = mccjit_invoke_mixed_i((void *)fbase, gp, fp);
#endif
		printf("mccjit-selftest-mixed: positional thunk direct=%ld thunk=%lld "
					 "(expect 10,10) %s\n",
					 direct, (long long)thunked,
					 (direct == 10 && thunked == 10) ? "OK" : "FAIL");
		if (direct != 10 || thunked != 10)
			{ MCC_TRACE("br\n"); fails++; }
#if defined(MCCJIT_I386)
		if (i386_thunk)
			{ MCC_TRACE("br\n"); munmap(i386_thunk, 4096); }
#endif
	}

	{
		unsigned char *stub =
				mccjit_make_kgc_stub_mixed((void *)fbase, (void *)fbase, 0, 2, 1, 0,
																	 retwide_seen);
		if (!stub) { MCC_TRACE("br\n");
			printf("mccjit-selftest-mixed: faithful stub NULL FAIL\n");
			fails++;
		} else { MCC_TRACE("br\n");
			long r = ((long (*)(long, double, long))(stub + 1))(5, 2.5, 3);
			int fl = *(int *)(stub + 256);
			printf("mccjit-selftest-mixed: stub-exec faithful r=%ld flag=%d "
						 "(expect 10,0) %s\n",
						 r, fl, (r == 10 && !fl) ? "OK" : "FAIL");
			if (r != 10 || fl)
				{ MCC_TRACE("br\n"); fails++; }
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
			int gretwide = mccjit_last_ret_wide;
			mccjit_last_state = NULL;
			if (gv) { MCC_TRACE("br\n");
				unsigned char *stub =
						mccjit_make_kgc_stub_mixed((void *)gv, (void *)fbase, 0, 2, 1, 0,
																			 gretwide);
				if (!stub) { MCC_TRACE("br\n");
					printf("mccjit-selftest-mixed: divergent stub NULL FAIL\n");
					fails++;
				} else { MCC_TRACE("br\n");
					long r = ((long (*)(long, double, long))(stub + 1))(5, 2.5, 3);
					int fl = *(int *)(stub + 256);
					printf("mccjit-selftest-mixed: stub-exec divergent r=%ld flag=%d "
								 "(expect 10,1) %s\n",
								 r, fl, (r == 10 && fl) ? "OK" : "FAIL");
					if (r != 10 || !fl)
						{ MCC_TRACE("br\n"); fails++; }
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
			int h_retwide = mccjit_last_ret_wide;
			mccjit_last_state = NULL;
			printf("mccjit-selftest-mixed: h(long,double)->double classified mixed=%d "
						 "ngp=%d nsse=%d ret_fp=%d (expect 1,1,1,1) %s\n",
						 h_mixed, h_ngp, h_nsse, h_retfp,
						 (h_mixed && h_ngp == 1 && h_nsse == 1 && h_retfp) ? "OK" : "FAIL");
			if (!h_mixed || h_ngp != 1 || h_nsse != 1 || !h_retfp)
				{ MCC_TRACE("br\n"); fails++; }
			if (hb) { MCC_TRACE("br\n");
				unsigned char *stub =
						mccjit_make_kgc_stub_mixed((void *)hb, (void *)hb, 0, 1, 1, 1,
																			 h_retwide);
				if (!stub) { MCC_TRACE("br\n");
					printf("mccjit-selftest-mixed: FP-ret stub NULL FAIL\n");
					fails++;
				} else { MCC_TRACE("br\n");
					double r = ((double (*)(long, double))(stub + 1))(3, 1.5);
					int fl = *(int *)(stub + 256);
					printf("mccjit-selftest-mixed: stub-exec FP-ret faithful r=%g flag=%d "
								 "(expect 6,0) %s\n",
								 r, fl, (r == 6.0 && !fl) ? "OK" : "FAIL");
					if (r != 6.0 || fl)
						{ MCC_TRACE("br\n"); fails++; }
				}
			}
			if (hstate)
				{ MCC_TRACE("br\n"); mcc_delete(hstate); }
		}
		mcc_free(hblob);
		if (sh)
			{ MCC_TRACE("br\n"); mcc_delete(sh); }
	}

	if (fbstate)
		{ MCC_TRACE("br\n"); mcc_delete(fbstate); }
	mcc_free(blob);
	mcc_delete(s1);
	printf("mccjit-selftest-mixed: %s (%d checks failed)\n",
				 fails ? "FAIL" : "PASS", fails);
	return fails ? 1 : 0;
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
		mccjit_perf_map_path(path, sizeof path);
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

static long long mccjit_profile_id1(long long x) { MCC_TRACE("enter\n"); return x; }
static long long mccjit_profile_sum2(long long a, long long b) { MCC_TRACE("enter\n"); return a + b; }

PUB_FUNC int mccjit_selftest_vrange(void) { MCC_TRACE("enter\n");
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

PUB_FUNC int mccjit_selftest_profile(void) { MCC_TRACE("enter\n");
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
		long long r = ((long long (*)(long long))stub)((long long)inputs[i]);
		if (r != inputs[i]) { MCC_TRACE("br\n");
			printf("mccjit-selftest-profile: stub(%lld) returned %lld (expect %lld) FAIL\n",
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
		((long long (*)(long long, long long))stub)(3, 7);
		((long long (*)(long long, long long))stub)(-2, 20);
		printf("mccjit-selftest-profile: 2-arg p0=[%lld,%lld] p1=[%lld,%lld]\n",
					 (long long)st.argmin[0], (long long)st.argmax[0],
					 (long long)st.argmin[1], (long long)st.argmax[1]);
#if defined(MCCJIT_I386)
		(void)0;
#else
		if (st.argmin[0] != -2 || st.argmax[0] != 3 || st.argmin[1] != 7 ||
				st.argmax[1] != 20) { MCC_TRACE("br\n");
			printf("mccjit-selftest-profile: 2-arg per-param range mismatch FAIL\n");
			fails++;
		}
#endif
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

PUB_FUNC int mccjit_selftest_evalgate(void) { MCC_TRACE("enter\n");
#if !defined(MCCJIT_X64)
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

PUB_FUNC int mccjit_selftest_slice(void) { MCC_TRACE("enter\n");
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
			ast_sym(A, na) != ast_sym(B, nb) ||
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
	ast_configure(js);
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

PUB_FUNC int mccjit_selftest_sliceextract(void) { MCC_TRACE("enter\n");
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
	ast_configure(js);
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
		ast_configure(js);
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

static int mccjit_consteval_one(const char *src, const char *fn, int want_ok,
																int64_t want_val) { MCC_TRACE("enter\n");
	unsigned char *blob;
	size_t blen;
	MCCState *s1, *js;
	MccjitIntent it;
	int fails = 0;
	blob = mccjit_stash_one(src, fn, 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-consteval: %s stash failed\n", fn);
		if (s1) { MCC_TRACE("br\n"); mcc_delete(s1); }
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
		ast_configure(js);
		anon_sym = SYM_FIRST_ANOM;
		funcname = "";
		func_ind = -1;
		if (mccjit_intent_deserialize(blob, blen, &it) == 0) { MCC_TRACE("br\n");
			int64_t v = 0;
			int got = ast_jit_const_fn(it.arena, &v);
			int ok = (got == want_ok) && (!want_ok || v == want_val);
			printf("mccjit-selftest-consteval: %-3s const=%d(want %d) val=%lld(want %lld) %s\n",
						 fn, got, want_ok, (long long)v, (long long)want_val, ok ? "OK" : "FAIL");
			if (!ok) { MCC_TRACE("br\n"); fails++; }
			mccjit_intent_release(&it);
		} else { MCC_TRACE("br\n");
			printf("mccjit-selftest-consteval: %s deserialize failed\n", fn);
			fails++;
		}
		mcc_exit_state(js);
		mcc_delete(js);
	}
	mcc_free(blob);
	mcc_delete(s1);
	return fails;
}

static int mccjit_foldcheck_one(const char *src, const char *fn, int want_min) { MCC_TRACE("enter\n");
	unsigned char *blob;
	size_t blen;
	MCCState *s1, *js;
	MccjitIntent it;
	int fails = 0;
	blob = mccjit_stash_one(src, fn, 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-consteval: %s fold stash failed\n", fn);
		if (s1) { MCC_TRACE("br\n"); mcc_delete(s1); }
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
		ast_configure(js);
		anon_sym = SYM_FIRST_ANOM;
		funcname = "";
		func_ind = -1;
		if (mccjit_intent_deserialize(blob, blen, &it) == 0) { MCC_TRACE("br\n");
			int folded = ast_jit_fold_consts(it.arena);
			int ok = folded >= want_min;
			printf("mccjit-selftest-consteval: %-3s folded=%d(want>=%d) %s\n", fn, folded,
						 want_min, ok ? "OK" : "FAIL");
			if (!ok) { MCC_TRACE("br\n"); fails++; }
			mccjit_intent_release(&it);
		} else { MCC_TRACE("br\n"); fails++; }
		mcc_exit_state(js);
		mcc_delete(js);
	}
	mcc_free(blob);
	mcc_delete(s1);
	return fails;
}

PUB_FUNC int mccjit_selftest_consteval(void) { MCC_TRACE("enter\n");
	int fails = 0;
	fails += mccjit_consteval_one("int f(int x){(void)x;return 6*7+1;}", "f", 1, 43);
	fails += mccjit_consteval_one("int f(int x){(void)x;return 5;}", "f", 1, 5);
	fails += mccjit_consteval_one("int f(int x){return x+1;}", "f", 0, 0);
	fails += mccjit_consteval_one("long f(long x){(void)x;return (3<<4)|1;}", "f", 1, 49);
	fails += mccjit_foldcheck_one("int f(int x){return x + 6*7;}", "f", 0);
	printf("mccjit-selftest-consteval: %s (%d fail%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

PUB_FUNC int mccjit_selftest_gated(void) { MCC_TRACE("enter\n");
	unsigned char *blob;
	size_t blen;
	MCCState *s1;
	uint64_t masks[4];
	int inputs[4] = {5, 0, -3, 100};
	int fails = 0, m, i;
	masks[0] = 0;
	masks[1] = 1;
	masks[2] = (uint64_t)0x20001;
	masks[3] = (uint64_t)0xffffffffffffULL;
	blob = mccjit_stash_one("int f(int x){return x*2+1;}", "f", 1, &blen, &s1);
	if (!blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-gated: stash failed\n");
		if (s1) { MCC_TRACE("br\n"); mcc_delete(s1); }
		return 1;
	}
	for (m = 0; m < 4; m++) { MCC_TRACE("br\n");
		int (*jitf)(int) =
				(int (*)(int))mcc_jit_recompile_blob_gated(blob, blen, masks[m]);
		if (!jitf) { MCC_TRACE("br\n");
			printf("mccjit-selftest-gated: mask %#llx recompile NULL\n",
						 (unsigned long long)masks[m]);
			fails++;
			continue;
		}
		for (i = 0; i < 4; i++) { MCC_TRACE("br\n");
			int got = jitf(inputs[i]);
			int want = inputs[i] * 2 + 1;
			if (got != want) { MCC_TRACE("br\n");
				printf("mccjit-selftest-gated: mask %#llx f(%d)=%d want %d FAIL\n",
							 (unsigned long long)masks[m], inputs[i], got, want);
				fails++;
			}
		}
	}
	printf("mccjit-selftest-gated: variant diversity %s (%d fail%s)\n",
				 fails ? "FAIL" : "OK", fails, fails == 1 ? "" : "s");
	mcc_free(blob);
	mcc_delete(s1);
	return fails ? 1 : 0;
}

PUB_FUNC int mccjit_selftest_search(void) { MCC_TRACE("enter\n");
	unsigned char *blob;
	size_t blen;
	MCCState *s1;
	uint64_t masks[128];
	int inputs[4] = {5, 0, -3, 100};
	int fails = 0, i, tried = 0, tried2 = 0;
	uint64_t bm = 0, bm2 = 0;
	void *best, *best2;
	for (i = 0; i < 128; i++)
		{ MCC_TRACE("br\n"); masks[i] = (uint64_t)i; }
	blob = mccjit_stash_one("int f(int x){return x*2+1;}", "f", 1, &blen, &s1);
	if (!blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-search: stash failed\n");
		if (s1) { MCC_TRACE("br\n"); mcc_delete(s1); }
		return 1;
	}
	best = mccjit_search_masks(blob, blen, masks, 128, 0.0, &bm, &tried);
	if (!best || tried != 128) { MCC_TRACE("br\n");
		printf("mccjit-selftest-search: enum best=%p tried=%d(want 128) FAIL\n", best, tried);
		fails++;
	} else { MCC_TRACE("br\n");
		for (i = 0; i < 4; i++) { MCC_TRACE("br\n");
			int got = ((int (*)(int))best)(inputs[i]);
			if (got != inputs[i] * 2 + 1) { MCC_TRACE("br\n");
				printf("mccjit-selftest-search: winner(mask %#llx) f(%d)=%d want %d FAIL\n",
							 (unsigned long long)bm, inputs[i], got, inputs[i] * 2 + 1);
				fails++;
			}
		}
	}
	best2 = mccjit_search_masks(blob, blen, masks, 128, 0.003, &bm2, &tried2);
	if (tried2 >= 128 || tried2 < 1) { MCC_TRACE("br\n");
		printf("mccjit-selftest-search: budget tried=%d(want 1..127) FAIL\n", tried2);
		fails++;
	}
	if (best2 && ((int (*)(int))best2)(5) != 11) { MCC_TRACE("br\n");
		printf("mccjit-selftest-search: budget winner incorrect FAIL\n");
		fails++;
	}
	printf("mccjit-selftest-search: enum tried=%d budget tried=%d %s (%d fail%s)\n",
				 tried, tried2, fails ? "FAIL" : "OK", fails, fails == 1 ? "" : "s");
	mcc_free(blob);
	mcc_delete(s1);
	return fails ? 1 : 0;
}

static int mccjit_ladder_pair(const char *s1, const char *f1, const char *s2,
															const char *f2, int want_eq, int want_seed,
															int want_diffw) { MCC_TRACE("enter\n");
	AstArena *a = mccjit_extract_ret_slice(s1, f1);
	AstArena *b = mccjit_extract_ret_slice(s2, f2);
	char why[192];
	int eq, seed, ok;
	if (!a || !b) { MCC_TRACE("br\n");
		printf("mccjit-selftest-sliceladder: %s/%s extract failed FAIL\n", f1, f2);
		ast_arena_free(a);
		ast_arena_free(b);
		return 1;
	}
	ast_slice_ladder_set(0);
	seed = ast_slice_equiv(a, ast_root(a), b, ast_root(b));
	ast_slice_ladder_set(1);
	why[0] = 0;
	eq = ast_slice_ladder_explain(a, ast_root(a), b, ast_root(b), why, sizeof why);
	ast_slice_ladder_set(0);
	ok = (eq == want_eq) && (seed == want_seed);
	if (ok && want_diffw >= 0)
		{ MCC_TRACE("br\n"); ok = strstr(why, "smallest-width=") != NULL; }
	printf("mccjit-selftest-sliceladder: %s vs %s seed=%d(want %d) "
				 "ladder=%d(want %d) [%s] %s\n",
				 f1, f2, seed, want_seed, eq, want_eq, why, ok ? "OK" : "FAIL");
	if (ok && want_diffw >= 0) { MCC_TRACE("br\n");
		char pat[32];
		snprintf(pat, sizeof pat, "smallest-width=%d ", want_diffw);
		if (!strstr(why, pat)) { MCC_TRACE("br\n");
			printf("mccjit-selftest-sliceladder: %s vs %s wanted %s FAIL\n", f1,
						 f2, pat);
			ok = 0;
		}
	}
	ast_arena_free(a);
	ast_arena_free(b);
	return ok ? 0 : 1;
}

PUB_FUNC int mccjit_selftest_sliceladder(void) { MCC_TRACE("enter\n");
	int fails = 0;

	printf("mccjit-selftest-sliceladder: begin (exhaustive width ladder 1..32)\n");

	fails += mccjit_ladder_pair("int l1(int x){return 41;}", "l1",
															"int l2(int x){return 40+1;}", "l2", 1, 1, -1);
	fails += mccjit_ladder_pair("int m1(int x){return 41;}", "m1",
															"int m2(int x){return 42;}", "m2", 0, 0, 0);
	fails += mccjit_ladder_pair("int n1(int x){return x*2+1;}", "n1",
															"int n2(int x){return x+x+1;}", "n2", 1, 1, -1);
	fails += mccjit_ladder_pair("int p1(int x){return x==3;}", "p1",
															"int p2(int x){return 0;}", "p2", 0, 1, 4);
	fails += mccjit_ladder_pair("int q1(int x){return x/2;}", "q1",
															"int q2(int x){return x>>1;}", "q2", 0, 0, 1);
	fails += mccjit_ladder_pair("int r1(unsigned x){return x>>31;}", "r1",
															"int r2(unsigned x){return 0;}", "r2", 0, 0, 1);
	fails += mccjit_ladder_pair("int s1(int a,int b){return a*b;}", "s1",
															"int s2(int a,int b){return b*a;}", "s2", 1, 1, -1);
	fails += mccjit_ladder_pair("int t1(int a,int b){return a-b;}", "t1",
															"int t2(int a,int b){return b-a;}", "t2", 0, 0, 1);

	printf("mccjit-selftest-sliceladder: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

PUB_FUNC int mccjit_selftest_sliceoracle(void) { MCC_TRACE("enter\n");
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
		ast_configure(js);
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
		ast_configure(js);
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

PUB_FUNC int mccjit_selftest_slicekernel(void) { MCC_TRACE("enter\n");
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
	ast_configure(js);
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
	mccjit_error_quiet = 1;
	if (sym && mcc_relocate(js) == 0)
		{ MCC_TRACE("br\n"); entry = mcc_get_symbol(js, it.fn_name); }
	mccjit_error_quiet = 0;
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
	ast_configure(js);
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

typedef struct MccjitObsCtx {
	const int64_t *param_off;
	uint32_t nparam;
	const MccjitCounterState *st;
} MccjitObsCtx;

static int mccjit_obs_tuples(const int32_t *offs, int n, int64_t *out, int maxt,
														 void *user) { MCC_TRACE("enter\n");
	MccjitObsCtx *c = (MccjitObsCtx *)user;
	int idx[MCCJIT_KGC_MAXARG];
	int i, j, t, ns, nt = 0;
	if (!c || !c->st || !c->param_off || n <= 0 || n > MCCJIT_KGC_MAXARG)
		{ MCC_TRACE("br\n"); return 0; }
	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		int found = -1;
		for (j = 0; j < (int)c->nparam && j < MCCJIT_KGC_MAXARG; j++)
			if ((int32_t)c->param_off[j] == offs[i])
				{ MCC_TRACE("br\n"); found = j; break; }
		if (found < 0)
			{ MCC_TRACE("br\n"); return 0; }
		idx[i] = found;
	}
	ns = c->st->nsample;
	if (ns > MCCJIT_PROFILE_SAMPLES)
		{ MCC_TRACE("br\n"); ns = MCCJIT_PROFILE_SAMPLES; }
	for (t = 0; t < ns && nt < maxt; t++) { MCC_TRACE("br\n");
		for (i = 0; i < n; i++)
			{ MCC_TRACE("br\n"); out[nt * n + i] = c->st->sample[t][idx[i]]; }
		nt++;
	}
	if (c->st->argseen > 0 && nt < maxt) { MCC_TRACE("br\n");
		for (i = 0; i < n; i++)
			{ MCC_TRACE("br\n"); out[nt * n + i] = c->st->argmin[idx[i]]; }
		nt++;
	}
	if (c->st->argseen > 0 && nt < maxt) { MCC_TRACE("br\n");
		for (i = 0; i < n; i++)
			{ MCC_TRACE("br\n"); out[nt * n + i] = c->st->argmax[idx[i]]; }
		nt++;
	}
	return nt;
}

static AstArena *mccjit_kernel_search_from_blob(const void *buf, size_t len,
																								const MccjitCounterState *cst,
																								uint32_t *out_np) { MCC_TRACE("enter\n");
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
	ast_configure(js);
	anon_sym = SYM_FIRST_ANOM;
	funcname = "";
	func_ind = -1;
	if (mccjit_intent_deserialize(buf, len, &it) == 0) { MCC_TRACE("br\n");
		AstLocal ret = mccjit_ret_expr(it.arena);
		AstLocal roots[16];
		MccjitObsCtx obs;
		if (out_np)
			{ MCC_TRACE("br\n"); *out_np = it.nparam; }
		int n = (ret != AST_NONE) ? ast_slice_search(it.arena, ret, 2, roots, 16) : 0;
		obs.param_off = it.param_off;
		obs.nparam = it.nparam;
		obs.st = cst;
		if (cst)
			{ MCC_TRACE("br\n"); ast_slice_ladder_observed_source(mccjit_obs_tuples, &obs); }
		if (n >= 1 && ast_fn_purity(it.arena) == AST_PURITY_TIER0
				&& ast_slice_certifiable(it.arena, ret))
			{ MCC_TRACE("br\n"); k = ast_slice_wrap_kernel(it.arena, ret); }
		if (k && ast_slice_ladder_on()) { MCC_TRACE("br\n");
			AstLocal kbb = ast_root(k);
			AstLocal kret = (kbb != AST_NONE) ? ast_first_child(k, kbb) : AST_NONE;
			AstLocal kexpr = (kret != AST_NONE) ? ast_first_child(k, kret) : AST_NONE;
			char why[192];
			int ok = (kexpr != AST_NONE) &&
							 ast_slice_ladder_explain(it.arena, ret, k, kexpr, why, sizeof why) == 1;
			if (!ok) { MCC_TRACE("br\n");
				if (mcc_env_on("MCC_JIT_VERBOSE"))
					{ MCC_TRACE("br\n"); fprintf(stderr, "mccjit-slice[ladder]: %s -- kernel not certified: %s\n",
									it.fn_name ? it.fn_name : "?", why); }
				ast_arena_free(k);
				k = NULL;
			}
		}
		ast_slice_ladder_observed_source(NULL, NULL);
		mccjit_intent_release(&it);
	}
	mcc_exit_state(js);
	mcc_delete(js);
	return k;
}

static void *mccjit_slice_search(MccjitCounterState *st, int *routed, int async) { MCC_TRACE("enter\n");
	uint32_t np = mccjit_last_nparam;
	AstArena *k;
	MCCState *keepk = NULL, *keepf = NULL;
	void *kern, *faithful;
	int i, mism = 0;
	if (routed)
		{ MCC_TRACE("br\n"); *routed = 0; }
	if (!async || mccjit_last_allfp || mccjit_last_ret_wide || st->nsample <= 0
			|| !st->blob)
		{ MCC_TRACE("br\n"); return NULL; }
	np = 0;
	k = mccjit_kernel_search_from_blob(st->blob, st->len, st, &np);
	if (mcc_env_on("MCC_JIT_VERBOSE"))
		{ MCC_TRACE("br\n"); fprintf(stderr,
						"mccjit-slice[gate]: kernel=%p np=%u nsample=%d\n", (void *)k, np,
						st->nsample); }
	if (!k)
		{ MCC_TRACE("br\n"); return NULL; }
	if (np < 1 || np > 3) { MCC_TRACE("br\n");
		ast_arena_free(k);
		return NULL;
	}
	faithful = mccjit_reemit_arena_blob(st->blob, st->len, NULL, &keepf);
	kern = faithful ? mccjit_reemit_arena_blob(st->blob, st->len, k, &keepk) : NULL;
	ast_arena_free(k);
	if (!faithful || !kern) { MCC_TRACE("br\n");
		if (keepf)
			{ MCC_TRACE("br\n"); mcc_delete(keepf); }
		if (keepk)
			{ MCC_TRACE("br\n"); mcc_delete(keepk); }
		return NULL;
	}
	for (i = 0; i < st->nsample; i++) { MCC_TRACE("br\n");
		const int64_t *a = st->sample[i];
		int rf, rk;
		if (np == 1) { MCC_TRACE("br\n");
			rf = ((int (*)(int))faithful)((int)a[0]);
			rk = ((int (*)(int))kern)((int)a[0]);
		} else if (np == 2) { MCC_TRACE("br\n");
			rf = ((int (*)(int, int))faithful)((int)a[0], (int)a[1]);
			rk = ((int (*)(int, int))kern)((int)a[0], (int)a[1]);
		} else { MCC_TRACE("br\n");
			rf = ((int (*)(int, int, int))faithful)((int)a[0], (int)a[1], (int)a[2]);
			rk = ((int (*)(int, int, int))kern)((int)a[0], (int)a[1], (int)a[2]);
		}
		if (rf != rk)
			{ MCC_TRACE("br\n"); mism = 1; break; }
	}
	mcc_delete(keepf);
	if (mism) { MCC_TRACE("br\n");
		mcc_delete(keepk);
		return NULL;
	}
	if (mcc_env_on("MCC_JIT_VERBOSE"))
		{ MCC_TRACE("br\n"); fprintf(stderr,
						"mccjit-slice[promote]: slot=%p slice-kernel verified over %d live-ins\n",
						(void *)st->slot, st->nsample); }
	return mccjit_make_trampoline(kern);
}

static void *mccjit_lazy_entry(MccjitCounterState *st, int *routed, int async) { MCC_TRACE("enter\n");
	if (async && mcc_env_on("MCC_JIT_SEARCH_SLICE")) { MCC_TRACE("br\n");
		void *e = mccjit_slice_search(st, routed, async);
		if (e)
			{ MCC_TRACE("br\n"); return e; }
	}
	if (mcc_env_on("MCC_JIT_SEARCH"))
		{ MCC_TRACE("br\n"); return mccjit_lazy_search(st, routed, async); }
	return mccjit_lazy_build(st->blob, st->len, routed);
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

PUB_FUNC int mccjit_selftest_slicereemit(void) { MCC_TRACE("enter\n");
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

PUB_FUNC int mccjit_selftest_sliceinstall(void) { MCC_TRACE("enter\n");
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
		ast_configure(js);
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

PUB_FUNC int mccjit_selftest_slicesearch(void) { MCC_TRACE("enter\n");
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

static int mccjit_noescape_one(const char *src, const char *fn,
															 int expect_ne_impure, int expect_base_impure) { MCC_TRACE("enter\n");
	unsigned char *blob;
	size_t blen;
	MCCState *s1, *js;
	MccjitIntent it;
	int fails = 0;
	blob = mccjit_stash_one(src, fn, 1, &blen, &s1);
	if (!s1 || !blob) { MCC_TRACE("br\n");
		printf("mccjit-selftest-noescape: %s stash failed FAIL\n", fn);
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
		ast_configure(js);
		anon_sym = SYM_FIRST_ANOM;
		funcname = "";
		func_ind = -1;
		if (mccjit_intent_deserialize(blob, blen, &it) == 0) { MCC_TRACE("br\n");
			int ne = ast_fn_purity_noescape(it.arena);
			int base = ast_fn_purity(it.arena);
			int ne_impure = (ne == AST_PURITY_IMPURE);
			int base_impure = (base == AST_PURITY_IMPURE);
			int ok = (ne_impure == expect_ne_impure) && (base_impure == expect_base_impure);
			printf("mccjit-selftest-noescape: %-4s noescape=%d(impure=%d want %d) "
						 "base=%d(impure=%d want %d) %s\n",
						 fn, ne, ne_impure, expect_ne_impure, base, base_impure,
						 expect_base_impure, ok ? "OK" : "FAIL");
			if (!ok)
				{ MCC_TRACE("br\n"); fails++; }
			mccjit_intent_release(&it);
		} else { MCC_TRACE("br\n");
			printf("mccjit-selftest-noescape: %s deserialize failed FAIL\n", fn);
			fails++;
		}
		mcc_exit_state(js);
		mcc_delete(js);
	}
	mcc_free(blob);
	mcc_delete(s1);
	return fails;
}

PUB_FUNC int mccjit_selftest_noescape(void) { MCC_TRACE("enter\n");
	int fails = 0;

	printf("mccjit-selftest-noescape: begin (escape-aware purity vs conservative purity)\n");
	fails += mccjit_noescape_one(
			"int acc(int n){int s=0,i; for(i=0;i<n;i++)s=s+i; return s;}", "acc", 0, 1);
	fails += mccjit_noescape_one("int t(int x){int a=x+1; int b=a*2; return b-3;}", "t",
															 0, 1);
	fails += mccjit_noescape_one("int p(int x){return x*x+3;}", "p", 0, 0);
	fails += mccjit_noescape_one("int wp(int *p){*p=5; return 0;}", "wp", 1, 1);
	fails += mccjit_noescape_one("int G; int wg(int x){G=x; return G;}", "wg", 1, 1);
	fails += mccjit_noescape_one("int at(int x){int a=x; int *q=&a; *q=7; return a;}",
															 "at", 1, 1);
	fails += mccjit_noescape_one("extern int ext(int); int cl(int x){return ext(x)+1;}",
															 "cl", 1, 1);

	printf("mccjit-selftest-noescape: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

PUB_FUNC int mccjit_selftest_l4a(void) { MCC_TRACE("enter\n");
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
		{ MCC_TRACE("br\n"); ((long long (*)(long long))stub)((long long)inputs[i]); }
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

PUB_FUNC int mccjit_selftest_benchwire(void) { MCC_TRACE("enter\n");
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

/* T-mac-30295 slice 1: proves the N-way wall-clock ranker picks the fastest
 * candidate, the significance gate does not flip on sub-margin noise, and the
 * host-busy gate declines. Uses no fault injector, so it runs on any build
 * (unlike the MCC_DEV-gated bench/benchwire cells). */
PUB_FUNC int mccjit_selftest_benchrank(void) { MCC_TRACE("enter\n");
	int64_t tuples[4 * MCCJIT_KGC_ARITY];
	uint32_t nt = 4, i, j;
	int fails = 0;
	int order[3];
	double secs[3];
	void *cands3[3];
	void *cands_tie[2];
	int n, n_tie, pick_win, pick_tie, gated;

	printf("mccjit-selftest-benchrank: begin (N-way wall-clock rank + "
				 "significance + host gate)\n");
	setenv("MCC_JIT_BENCH_ITERS", "3000", 1);
	setenv("MCC_JIT_BENCH_ROUNDS", "7", 1);
	setenv("MCC_JIT_BENCH_MARGIN_PCT", "30", 1);
	for (i = 0; i < nt; i++)
		{ MCC_TRACE("br\n"); for (j = 0; j < MCCJIT_KGC_ARITY; j++)
			{ MCC_TRACE("br\n"); tuples[i * MCCJIT_KGC_ARITY + j] = (int64_t)(i * 7 + 1); } }

	cands3[0] = (void *)mccjit_bench_slow_fn;
	cands3[1] = (void *)mccjit_bench_mid_fn;
	cands3[2] = (void *)mccjit_bench_fast_fn;
	n = mccjit_bench_rank_n((void *const *)cands3, 3, tuples, nt, 1, 1, 0, order, secs);
	printf("mccjit-selftest-benchrank: ranked n=%d (expect 3) fastest_idx=%d "
				 "slowest_idx=%d\n",
				 n, n >= 1 ? order[0] : -1, n >= 3 ? order[2] : -1);
	if (n != 3)
		{ MCC_TRACE("br\n"); fails++; }
	if (n == 3 && order[0] != 2) { MCC_TRACE("br\n");
		printf("mccjit-selftest-benchrank: fastest should be idx2 (fast_fn) FAIL\n");
		fails++;
	}
	if (n == 3 && order[2] != 0) { MCC_TRACE("br\n");
		printf("mccjit-selftest-benchrank: slowest should be idx0 (slow_fn) FAIL\n");
		fails++;
	}
	if (n == 3 && !(secs[0] < secs[2])) { MCC_TRACE("br\n");
		printf("mccjit-selftest-benchrank: times not ascending FAIL\n");
		fails++;
	}

	pick_win = (n == 3) ? mccjit_bench_pick_significant(order, secs, n, 0) : -1;
	printf("mccjit-selftest-benchrank: significant pick vs ref=slow -> %d "
				 "(expect 2) %s\n",
				 pick_win, pick_win == 2 ? "OK" : "FAIL");
	if (pick_win != 2)
		{ MCC_TRACE("br\n"); fails++; }

	cands_tie[0] = (void *)mccjit_bench_fast_fn;
	cands_tie[1] = (void *)mccjit_bench_fast_fn;
	n_tie = mccjit_bench_rank_n((void *const *)cands_tie, 2, tuples, nt, 1, 1, 0,
														 order, secs);
	pick_tie = (n_tie == 2) ? mccjit_bench_pick_significant(order, secs, n_tie, order[1])
													: -99;
	printf("mccjit-selftest-benchrank: tie pick vs ref=order[1] -> %d "
				 "(expect %d, no flip) %s\n",
				 pick_tie, n_tie == 2 ? order[1] : -99,
				 (n_tie == 2 && pick_tie == order[1]) ? "OK" : "FAIL");
	if (!(n_tie == 2 && pick_tie == order[1]))
		{ MCC_TRACE("br\n"); fails++; }

	setenv("MCC_JIT_BENCH_FORCE_BUSY", "1", 1);
	gated = mccjit_bench_rank_n((void *const *)cands3, 3, tuples, nt, 1, 1, 0, order, secs);
	unsetenv("MCC_JIT_BENCH_FORCE_BUSY");
	printf("mccjit-selftest-benchrank: forced-busy rank -> %d (expect -1) %s\n",
				 gated, gated == -1 ? "OK" : "FAIL");
	if (gated != -1)
		{ MCC_TRACE("br\n"); fails++; }

	unsetenv("MCC_JIT_BENCH_ITERS");
	unsetenv("MCC_JIT_BENCH_ROUNDS");
	unsetenv("MCC_JIT_BENCH_MARGIN_PCT");
	printf("mccjit-selftest-benchrank: %s (%d failure%s)\n",
				 fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}

void mcc_jit_publish(void **slot, void *variant) { MCC_TRACE("enter\n");
	if (!slot)
		{ MCC_TRACE("br\n"); return; }
	MCCJIT_DIAG_NOTE_PUB(slot, variant);
	if (variant)
		{ MCC_TRACE("br\n"); host_icache_flush(variant, (unsigned long)host_pagesize()); }
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
#endif
