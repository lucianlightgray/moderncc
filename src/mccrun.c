#include "mcc.h"

#ifdef MCC_TARGET_IS_HOST

typedef struct rt_context {
	union {
		struct
		{
			Stab_Sym *stab_sym;
			Stab_Sym *stab_sym_end;
			char *stab_str;
		};

		struct
		{
			unsigned char *dwarf_line;
			unsigned char *dwarf_line_end;
			unsigned char *dwarf_line_str;
		};
	};

	ElfW(Sym) * esym_start;
	ElfW(Sym) * esym_end;
	char *elf_str;
	addr_t prog_base;
	void *bounds_start;
	void *top_func;
	struct rt_context *next;
	int num_callers;
	int dwarf;
} rt_context;

static rt_context *g_rc;
static int signal_set;
static void set_exception_handler(void);

typedef struct rt_frame {
	addr_t ip, fp, sp;
} rt_frame;

static MCCState *g_s1;
HOST_SEM(rt_sem);

static void rt_wait_sem(void) { MCC_TRACE("enter\n");
	HOST_SEM_WAIT(&rt_sem);
}

static void rt_post_sem(void) { MCC_TRACE("enter\n");
	HOST_SEM_POST(&rt_sem);
}

static int rt_get_caller_pc(addr_t *paddr, rt_frame *f, int level);
static void rt_exit(rt_frame *f, int code);

#ifndef MCC_CONFIG_BACKTRACE_ONLY

static int mcc_relocate_ex(MCCState *s1, void *ptr, unsigned ptr_diff);
static void st_link(MCCState *s1);
static void st_unlink(MCCState *s1);
static int _mcc_backtrace(rt_frame *f, const char *fmt, va_list ap);
#if MCC_HOST_LINUX
static void run_tls_seed_free(void);
#endif

#define PAGESIZE host_pagesize()
#define PAGEALIGN(n) ((addr_t)n + (-(addr_t)n & (PAGESIZE - 1)))

static int rt_mem(MCCState *s1, int size) { MCC_TRACE("enter\n");
	unsigned sz = size;
	int ptr_diff = 0;
	void *ptr = host_runmem_alloc(&sz, &ptr_diff);
	if (!ptr)
		{ MCC_TRACE("br\n"); return mcc_error_noabort("mccrun: could not allocate memory"); }
	s1->run_ptr = ptr;
	s1->run_size = sz;
	return ptr_diff;
}

LIBMCCAPI int mcc_relocate(MCCState *s1) { MCC_TRACE("enter\n");
	int size, ret, ptr_diff;

	if (s1->run_ptr)
		{ MCC_TRACE("br\n"); exit(mcc_error_noabort("'mcc_relocate()' twice is no longer supported")); }
	if (s1->do_backtrace)
		{ MCC_TRACE("br\n"); mcc_add_symbol(s1, "_mcc_backtrace", _mcc_backtrace); }
	size = mcc_relocate_ex(s1, NULL, 0);
	if (size < 0)
		{ MCC_TRACE("br\n"); return -1; }
	ptr_diff = rt_mem(s1, size);
	if (ptr_diff < 0)
		{ MCC_TRACE("br\n"); return -1; }
	ret = mcc_relocate_ex(s1, s1->run_ptr, ptr_diff);
	if (ret == 0)
		{ MCC_TRACE("br\n"); st_link(s1); }
	return ret;
}

ST_FUNC void mcc_run_free(MCCState *s1) { MCC_TRACE("enter\n");
	unsigned size;
	void *ptr;

	for (int i = 0; i < s1->nb_loaded_dlls; i++) { MCC_TRACE("br\n");
		DLLReference *ref = s1->loaded_dlls[i];
		if (ref->handle)
			{ MCC_TRACE("br\n"); host_dlclose(ref->handle); }
	}
#if MCC_HOST_LINUX
	run_tls_seed_free();
#endif
	ptr = s1->run_ptr;
	if (NULL == ptr)
		{ MCC_TRACE("br\n"); return; }
	st_unlink(s1);
	size = s1->run_size;
	host_unwind_unregister(s1->run_function_table);
	host_runmem_free(ptr, size);
}

#define RT_EXIT_ZERO 0xE0E00E0E

LIBMCCAPI int mcc_run(MCCState *s1, int argc, char **argv) { MCC_TRACE("enter\n");
	int (*prog_main)(int, char **, char **), ret;
	const char *top_sym;
	jmp_buf main_jb;

	char **envp = host_environ();

	if ((s1->dflag & 16) && (addr_t)-1 == get_sym_addr(s1, "main", 0, 1))
		{ MCC_TRACE("br\n"); return 0; }

	mcc_add_symbol(s1, "__rt_exit", rt_exit);
	s1->run_main = "_runmain", top_sym = "main";
	if (s1->elf_entryname)
		{ MCC_TRACE("br\n"); s1->run_main = top_sym = s1->elf_entryname; }
	mcc_add_support(s1, "runmain.o");

	if (mcc_relocate(s1) < 0)
		{ MCC_TRACE("br\n"); return -1; }

	prog_main = (void *)get_sym_addr(s1, s1->run_main, 1, 1);
	if ((addr_t)-1 == (addr_t)prog_main)
		{ MCC_TRACE("br\n"); return -1; }

	if (s1->run_stdin && !freopen(s1->run_stdin, "r", stdin)) { MCC_TRACE("br\n");
		mcc_error_noabort("failed to reopen stdin from '%s'", s1->run_stdin);
		return -1;
	}

	errno = 0;
	fflush(stdout);
	fflush(stderr);

	ret = mcc_setjmp(s1, main_jb, mcc_get_symbol(s1, top_sym));
	if (0 == ret) { MCC_TRACE("br\n");
		ret = prog_main(argc, argv, envp);
	} else if (RT_EXIT_ZERO == ret) { MCC_TRACE("br\n");
		ret = 0;
	}

	if (s1->dflag & 16 && ret)
		{ MCC_TRACE("br\n"); fprintf(s1->ppfp, "[returns %d]\n", ret), fflush(s1->ppfp); }
	return ret;
}

static void cleanup_symbols(MCCState *s1) { MCC_TRACE("enter\n");
	Section *s = s1->symtab;
	int sym_index, end_sym = s->data_offset / sizeof(ElfSym);
	s->data_offset = s->link->data_offset = s->hash->data_offset = 0;
	init_symtab(s);
	for (sym_index = 1; sym_index < end_sym; ++sym_index) { MCC_TRACE("br\n");
		ElfW(Sym) *sym = &((ElfW(Sym) *)s->data)[sym_index];
		const char *name = (char *)s->link->data + sym->st_name;
		if (ELFW(ST_BIND)(sym->st_info) == STB_LOCAL)
			{ MCC_TRACE("br\n"); continue; }
		put_elf_sym(s, sym->st_value, sym->st_size, sym->st_info, sym->st_other, sym->st_shndx, name);
	}
}

static void cleanup_sections(MCCState *s1) { MCC_TRACE("enter\n");
	struct
	{
		Section **secs;
		int nb_secs;
	} *p = (void *)&s1->sections;
	int i, f = 2;
	do { MCC_TRACE("br\n");
		for (i = --f; i < p->nb_secs; i++) { MCC_TRACE("br\n");
			Section *s = p->secs[i];
			if (s == s1->symtab || s == s1->symtab->link || s == s1->symtab->hash) { MCC_TRACE("br\n");
				s->data = mcc_realloc(s->data, s->data_allocated = s->data_offset);
			} else { MCC_TRACE("br\n");
				free_section(s), mcc_free(s), p->secs[i] = NULL;
			}
		}
	} while (++p, f);
}

#if MCC_HOST_DARWIN && (defined(__aarch64__) || defined(__x86_64__))
#define MCC_RUN_TLS_MACHO 1
#endif

#if defined(MCC_RUN_TLS_MACHO) || MCC_HOST_LINUX
#include <pthread.h>
#endif

#ifdef MCC_RUN_TLS_MACHO

#if MCC_HOST_DARWIN
#define MCC_ASM_SYM(x) "_" #x
#else
#define MCC_ASM_SYM(x) #x
#endif

struct mcc_tlv_desc {
	void *(*thunk)(struct mcc_tlv_desc *);
	unsigned long key;
	unsigned long off;
};

struct mcc_tls_img {
	pthread_key_t key;
	const void *init;
	unsigned long filesz, memsz;
	int used;
};

struct mcc_tls_var {
	int desc_off;
	int orig_sec;
	addr_t orig_val;
};

#define MCC_TLS_IMG_MAX 64
static struct mcc_tls_img mcc_tls_imgs[MCC_TLS_IMG_MAX];
static int mcc_tls_img_count;
static pthread_mutex_t mcc_tls_img_mtx = PTHREAD_MUTEX_INITIALIZER;

static struct mcc_tls_img *mcc_tls_img_lookup(pthread_key_t key) { MCC_TRACE("enter\n");
	struct mcc_tls_img *r = NULL;
	pthread_mutex_lock(&mcc_tls_img_mtx);
	for (int i = 0; i < mcc_tls_img_count; i++) { MCC_TRACE("br\n");
		if (mcc_tls_imgs[i].used && mcc_tls_imgs[i].key == key)
			{ MCC_TRACE("br\n"); r = &mcc_tls_imgs[i]; break; }
	}
	pthread_mutex_unlock(&mcc_tls_img_mtx);
	return r;
}

__attribute__((used)) void *mcc_tlv_get_addr(struct mcc_tlv_desc *d) { MCC_TRACE("enter\n");
	void *blk = pthread_getspecific((pthread_key_t)d->key);
	if (!blk) { MCC_TRACE("br\n");
		struct mcc_tls_img *img = mcc_tls_img_lookup((pthread_key_t)d->key);
		blk = calloc(1, img->memsz);
		if (img->filesz)
			{ MCC_TRACE("br\n"); memcpy(blk, img->init, img->filesz); }
		pthread_setspecific((pthread_key_t)d->key, blk);
	}
	return (char *)blk + d->off;
}

extern void mcc_tlv_thunk(void);

#if defined(__aarch64__)
__asm__(
	".text\n"
	".p2align 2\n"
	".globl " MCC_ASM_SYM(mcc_tlv_thunk) "\n"
	MCC_ASM_SYM(mcc_tlv_thunk) ":\n"
	"	stp x1, x2, [sp, #-16]!\n"
	"	stp x3, x4, [sp, #-16]!\n"
	"	stp x5, x6, [sp, #-16]!\n"
	"	stp x7, x8, [sp, #-16]!\n"
	"	stp x9, x10, [sp, #-16]!\n"
	"	stp x11, x12, [sp, #-16]!\n"
	"	stp x13, x14, [sp, #-16]!\n"
	"	stp x15, x16, [sp, #-16]!\n"
	"	stp x17, x18, [sp, #-16]!\n"
	"	stp x29, x30, [sp, #-16]!\n"
	"	stp d0, d1, [sp, #-16]!\n"
	"	stp d2, d3, [sp, #-16]!\n"
	"	stp d4, d5, [sp, #-16]!\n"
	"	stp d6, d7, [sp, #-16]!\n"
	"	bl " MCC_ASM_SYM(mcc_tlv_get_addr) "\n"
	"	ldp d6, d7, [sp], #16\n"
	"	ldp d4, d5, [sp], #16\n"
	"	ldp d2, d3, [sp], #16\n"
	"	ldp d0, d1, [sp], #16\n"
	"	ldp x29, x30, [sp], #16\n"
	"	ldp x17, x18, [sp], #16\n"
	"	ldp x15, x16, [sp], #16\n"
	"	ldp x13, x14, [sp], #16\n"
	"	ldp x11, x12, [sp], #16\n"
	"	ldp x9, x10, [sp], #16\n"
	"	ldp x7, x8, [sp], #16\n"
	"	ldp x5, x6, [sp], #16\n"
	"	ldp x3, x4, [sp], #16\n"
	"	ldp x1, x2, [sp], #16\n"
	"	ret\n");
#elif defined(__x86_64__)
__asm__(
	".text\n"
	".p2align 4\n"
	".globl " MCC_ASM_SYM(mcc_tlv_thunk) "\n"
	MCC_ASM_SYM(mcc_tlv_thunk) ":\n"
	"	push %rbp\n"
	"	mov %rsp, %rbp\n"
	"	and $-16, %rsp\n"
	"	sub $128, %rsp\n"
	"	movaps %xmm0, 0(%rsp)\n"
	"	movaps %xmm1, 16(%rsp)\n"
	"	movaps %xmm2, 32(%rsp)\n"
	"	movaps %xmm3, 48(%rsp)\n"
	"	movaps %xmm4, 64(%rsp)\n"
	"	movaps %xmm5, 80(%rsp)\n"
	"	movaps %xmm6, 96(%rsp)\n"
	"	movaps %xmm7, 112(%rsp)\n"
	"	push %rcx\n"
	"	push %rdx\n"
	"	push %rsi\n"
	"	push %r8\n"
	"	push %r9\n"
	"	push %r10\n"
	"	call " MCC_ASM_SYM(mcc_tlv_get_addr) "\n"
	"	pop %r10\n"
	"	pop %r9\n"
	"	pop %r8\n"
	"	pop %rsi\n"
	"	pop %rdx\n"
	"	pop %rcx\n"
	"	movaps 0(%rsp), %xmm0\n"
	"	movaps 16(%rsp), %xmm1\n"
	"	movaps 32(%rsp), %xmm2\n"
	"	movaps 48(%rsp), %xmm3\n"
	"	movaps 64(%rsp), %xmm4\n"
	"	movaps 80(%rsp), %xmm5\n"
	"	movaps 96(%rsp), %xmm6\n"
	"	movaps 112(%rsp), %xmm7\n"
	"	mov %rbp, %rsp\n"
	"	pop %rbp\n"
	"	ret\n");
#endif

static void tls_setup_macho(MCCState *s1) { MCC_TRACE("enter\n");
	int i, sym_end = symtab_section->data_offset / sizeof(ElfW(Sym));
	Section *desc = NULL;
	struct mcc_tls_var *recs = NULL;
	int nrecs = 0;

	for (i = 1; i < sym_end; i++) { MCC_TRACE("br\n");
		ElfW(Sym) *sym = (ElfW(Sym) *)symtab_section->data + i;
		int desc_off;

		if (ELFW(ST_TYPE)(sym->st_info) != STT_TLS)
			{ MCC_TRACE("br\n"); continue; }
		if (sym->st_shndx != tdata_section->sh_num &&
				sym->st_shndx != tbss_section->sh_num) { MCC_TRACE("br\n");
			if (sym->st_shndx == SHN_UNDEF)
				{ MCC_TRACE("br\n"); mcc_error_noabort("TLS import unsupported in -run: '%s'",
									(char *)symtab_section->link->data + sym->st_name); }
			continue;
		}

		if (!desc) { MCC_TRACE("br\n");
			desc = new_section(s1, ".tlv_run_desc", SHT_PROGBITS,
												 SHF_ALLOC | SHF_WRITE);
			desc->sh_addralign = MCC_PTR_SIZE;
			sym = (ElfW(Sym) *)symtab_section->data + i;
		}

		desc_off = desc->data_offset;
		memset(section_ptr_add(desc, 3 * MCC_PTR_SIZE), 0, 3 * MCC_PTR_SIZE);

		recs = mcc_realloc(recs, (nrecs + 1) * sizeof(*recs));
		recs[nrecs].desc_off = desc_off;
		recs[nrecs].orig_sec = sym->st_shndx;
		recs[nrecs].orig_val = sym->st_value;
		nrecs++;

		sym->st_shndx = desc->sh_num;
		sym->st_value = desc_off;
		sym->st_size = 3 * MCC_PTR_SIZE;
		sym->st_info = ELFW(ST_INFO)(ELFW(ST_BIND)(sym->st_info), STT_OBJECT);
	}

	s1->run_tls_desc = desc;
	s1->run_tls_recs = recs;
	s1->run_tls_nrecs = nrecs;
}

static void tls_finalize_macho(MCCState *s1) { MCC_TRACE("enter\n");
	struct mcc_tls_var *recs = s1->run_tls_recs;
	Section *desc = s1->run_tls_desc;
	addr_t base = (addr_t)-1;
	unsigned long td_sz, tb_sz, filesz, memsz;
	pthread_key_t key;
	int i;

	if (!desc || !s1->run_tls_nrecs)
		{ MCC_TRACE("br\n"); return; }

	td_sz = tdata_section->sh_size ? tdata_section->sh_size : tdata_section->data_offset;
	tb_sz = tbss_section->sh_size ? tbss_section->sh_size : tbss_section->data_offset;

	if (td_sz && tdata_section->sh_addr < base)
		{ MCC_TRACE("br\n"); base = tdata_section->sh_addr; }
	if (tb_sz && tbss_section->sh_addr < base)
		{ MCC_TRACE("br\n"); base = tbss_section->sh_addr; }

	filesz = td_sz ? (unsigned long)(tdata_section->sh_addr + td_sz - base) : 0;
	memsz = filesz;
	if (tb_sz) { MCC_TRACE("br\n");
		unsigned long end = (unsigned long)(tbss_section->sh_addr + tb_sz - base);
		if (end > memsz)
			{ MCC_TRACE("br\n"); memsz = end; }
	}

	if (pthread_key_create(&key, free) != 0)
		{ MCC_TRACE("br\n"); mcc_error_noabort("mccrun: pthread_key_create failed for TLS"); return; }

	pthread_mutex_lock(&mcc_tls_img_mtx);
	if (mcc_tls_img_count < MCC_TLS_IMG_MAX) { MCC_TRACE("br\n");
		struct mcc_tls_img *img = &mcc_tls_imgs[mcc_tls_img_count++];
		img->key = key;
		img->init = (const void *)(addr_t)tdata_section->sh_addr;
		img->filesz = filesz;
		img->memsz = memsz;
		img->used = 1;
	} else { MCC_TRACE("br\n");
		pthread_mutex_unlock(&mcc_tls_img_mtx);
		mcc_error_noabort("mccrun: too many TLS images");
		return;
	}
	pthread_mutex_unlock(&mcc_tls_img_mtx);

	for (i = 0; i < s1->run_tls_nrecs; i++) { MCC_TRACE("br\n");
		Section *os = s1->sections[recs[i].orig_sec];
		addr_t var_addr = os->sh_addr + recs[i].orig_val;
		unsigned char *p = desc->data + recs[i].desc_off;
		write64le(p, (uint64_t)(addr_t)&mcc_tlv_thunk);
		write64le(p + MCC_PTR_SIZE, (uint64_t)key);
		write64le(p + 2 * MCC_PTR_SIZE, (uint64_t)(var_addr - base));
	}

	mcc_free(recs);
	s1->run_tls_recs = NULL;
	s1->run_tls_nrecs = 0;
}
#endif

#if MCC_HOST_LINUX
static void tls_setup_linux(MCCState *s1) { MCC_TRACE("enter\n");
	int i;
	int have_tls = 0;
	unsigned long total = 0;

	for (i = 1; i < s1->nb_sections; i++) { MCC_TRACE("br\n");
		Section *s = s1->sections[i];
		if (!(s->sh_flags & SHF_TLS))
			{ MCC_TRACE("br\n"); continue; }
		have_tls = 1;
		total += s->data_offset;
	}
	if (!have_tls)
		{ MCC_TRACE("br\n"); return; }
	if (total > host_run_tls_slab_size()) { MCC_TRACE("br\n");
		mcc_error_noabort("mccrun: TLS size %lu exceeds -run slab", total);
		return;
	}
	s1->run_tls_slab_tpoff = host_run_tls_slab_tpoff();
	s1->run_tls_active = 1;
}

/* The -run TLS slab is a __thread array inside mcc, seeded below for the thread
   that calls -run. A thread the program creates gets its own zeroed copy and
   nothing seeds it, so every TLS variable with a non-zero initializer reads 0 in
   the child -- which is exactly what tls_threads catches. Keep a copy of the
   seed image and re-apply it on thread entry, by binding the program's
   pthread_create to the wrapper below (see relocate_syms). */
static unsigned char *mcc_run_tls_seed;
static unsigned long mcc_run_tls_seed_len;

/* The snapshot is module-level so mcc_run_thr_start can reach it, which means
   nothing state-owned frees it; without this, every -run of a TLS-using
   program reports it leaked under MCC_DIAG. */
static void run_tls_seed_free(void) { MCC_TRACE("enter\n");
	mcc_free(mcc_run_tls_seed);
	mcc_run_tls_seed = NULL;
	mcc_run_tls_seed_len = 0;
}

struct mcc_run_thr {
	void *(*fn)(void *);
	void *arg;
};

static void *mcc_run_thr_start(void *p) { MCC_TRACE("enter\n");
	struct mcc_run_thr t = *(struct mcc_run_thr *)p;
	mcc_free(p);
	if (mcc_run_tls_seed && mcc_run_tls_seed_len) { MCC_TRACE("br\n");
		unsigned char *slab = host_run_tls_slab_base();
		memset(slab, 0, host_run_tls_slab_size());
		memcpy(slab, mcc_run_tls_seed, mcc_run_tls_seed_len);
	}
	return t.fn(t.arg);
}

ST_FUNC int mcc_run_pthread_create(void *th, const void *attr,
																	 void *(*fn)(void *), void *arg) { MCC_TRACE("enter\n");
	struct mcc_run_thr *t = mcc_malloc(sizeof *t);
	int rc;
	if (!t)
		{ MCC_TRACE("br\n"); return pthread_create((pthread_t *)th, (const pthread_attr_t *)attr, fn, arg); }
	t->fn = fn;
	t->arg = arg;
	rc = pthread_create((pthread_t *)th, (const pthread_attr_t *)attr,
										mcc_run_thr_start, t);
	if (rc != 0)
		{ MCC_TRACE("br\n"); mcc_free(t); }
	return rc;
}

static void tls_seed_linux(MCCState *s1) { MCC_TRACE("enter\n");
	int i;
	addr_t base = (addr_t)-1;
	unsigned char *slab;

	mcc_run_tls_seed_len = 0;

	if (!s1->run_tls_active)
		{ MCC_TRACE("br\n"); return; }
	for (i = 1; i < s1->nb_sections; i++) { MCC_TRACE("br\n");
		Section *s = s1->sections[i];
		addr_t ssz = s->sh_size ? s->sh_size : s->data_offset;
		if ((s->sh_flags & SHF_TLS) && ssz && s->sh_addr < base)
			{ MCC_TRACE("br\n"); base = s->sh_addr; }
	}
	slab = host_run_tls_slab_base();
	memset(slab, 0, host_run_tls_slab_size());
	for (i = 1; i < s1->nb_sections; i++) { MCC_TRACE("br\n");
		Section *s = s1->sections[i];
		if (!(s->sh_flags & SHF_TLS) || s->sh_type == SHT_NOBITS || !s->data)
			{ MCC_TRACE("br\n"); continue; }
		memcpy(slab + (s->sh_addr - base), s->data, s->data_offset);
		if ((unsigned long)(s->sh_addr - base) + s->data_offset > mcc_run_tls_seed_len)
			{ MCC_TRACE("br\n"); mcc_run_tls_seed_len =
					(unsigned long)(s->sh_addr - base) + s->data_offset; }
	}
	/* Snapshot what a fresh thread has to start from. */
	mcc_free(mcc_run_tls_seed);
	mcc_run_tls_seed = NULL;
	if (mcc_run_tls_seed_len) { MCC_TRACE("br\n");
		mcc_run_tls_seed = mcc_malloc(mcc_run_tls_seed_len);
		if (mcc_run_tls_seed)
			{ MCC_TRACE("br\n"); memcpy(mcc_run_tls_seed, slab, mcc_run_tls_seed_len); }
		else
			{ MCC_TRACE("br\n"); mcc_run_tls_seed_len = 0; }
	}
}
#endif

static int mcc_relocate_ex(MCCState *s1, void *ptr, unsigned ptr_diff) { MCC_TRACE("enter\n");
	Section *s;
	unsigned offset, length, align, i, k, f;
	unsigned n, copy;
	addr_t mem, addr;

	if (NULL == ptr) { MCC_TRACE("br\n");
		s1->nb_errors = 0;
#ifdef MCC_TARGET_PE
		pe_output_file(s1, NULL);
#else
		mcc_add_runtime(s1);
		resolve_common_syms(s1);
		build_got_entries(s1, 0);
#if defined(MCC_TARGET_ARM64)
		arm64_veneer_memory_calls(s1);
#endif
#if defined(MCC_TARGET_ARM)
		arm_veneer_memory_calls(s1);
#endif
#if defined(MCC_TARGET_RISCV64)
		riscv64_veneer_memory_calls(s1);
#endif
#if defined(MCC_RUN_TLS_MACHO)
		tls_setup_macho(s1);
#elif MCC_HOST_LINUX
		tls_setup_linux(s1);
#endif
#endif
	}

	offset = copy = 0;
	mem = (addr_t)ptr;
redo:
	if (MCC_VTIER(s1->verbose) == MCC_V2 && copy)
		{ MCC_TRACE("br\n"); printf(&"-----------------------------------------------------\n"[MCC_PTR_SIZE * 2 - 8]); }
	if (s1->nb_errors)
		{ MCC_TRACE("br\n"); return -1; }
	if (copy == 3)
		{ MCC_TRACE("br\n"); return 0; }

	for (k = 0; k < 3; ++k) { MCC_TRACE("br\n");
		n = 0;
		addr = 0;
		for (i = 1; i < s1->nb_sections; i++) { MCC_TRACE("br\n");
			static const char shf[] = {
					SHF_ALLOC | SHF_EXECINSTR, SHF_ALLOC, SHF_ALLOC | SHF_WRITE};
			s = s1->sections[i];
			if (shf[k] != (s->sh_flags & (SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR)))
				{ MCC_TRACE("br\n"); continue; }
			length = s->data_offset;
			if (copy == 2) { MCC_TRACE("br\n");
				if (addr == 0)
					{ MCC_TRACE("br\n"); addr = s->sh_addr; }
				n = (s->sh_addr - addr) + length;
				continue;
			}
			if (copy) { MCC_TRACE("br\n");
				if (MCC_VTIER(s1->verbose) == MCC_V2)
					{ MCC_TRACE("br\n"); printf("%d: %-16s %p  len %05x  align %04x\n",
								 k, s->name, (void *)s->sh_addr, length, s->sh_addralign); }
				ptr = (void *)s->sh_addr;
				if (k == 0)
					{ MCC_TRACE("br\n"); ptr = (void *)(s->sh_addr + ptr_diff); }
				if (NULL == s->data || s->sh_type == SHT_NOBITS)
					{ MCC_TRACE("br\n"); memset(ptr, 0, length); }
				else
					{ MCC_TRACE("br\n"); memcpy(ptr, s->data, length); }
				continue;
			}

			align = s->sh_addralign;
			if (++n == 1) { MCC_TRACE("br\n");
#if defined MCC_TARGET_I386 || defined MCC_TARGET_X86_64
				if (align < 64)
					{ MCC_TRACE("br\n"); align = 64; }
#endif
				if (k <= HOST_RUNMEM_RO)
					{ MCC_TRACE("br\n"); align = PAGESIZE; }
			}
			s->sh_addralign = align;
			addr = k ? mem + ptr_diff : mem;
			offset += -(addr + offset) & (align - 1);
			s->sh_addr = mem ? addr + offset : 0;
			offset += length;
		}
		if (copy == 2) { MCC_TRACE("br\n");
			if (n == 0)
				{ MCC_TRACE("br\n"); continue; }
			if (k == 0 && host_runmem_dual())
				{ MCC_TRACE("br\n"); continue; }
			f = k;
			if (f >= HOST_RUNMEM_RO) { MCC_TRACE("br\n");
				if (f != 0)
					{ MCC_TRACE("br\n"); continue; }
				f = 3;
			}
			n = PAGEALIGN(n);
			if (MCC_VTIER(s1->verbose) == MCC_V2) { MCC_TRACE("br\n");
				printf("protect         %3s %p  len %05x\n",
							 &"rx\0ro\0rw\0rwx"[f * 3], (void *)addr, (unsigned)n);
			}
			if (host_runmem_protect((void *)addr, n, f) < 0)
				{ MCC_TRACE("br\n"); return mcc_error_noabort(HOST_MPROTECT_FAILMSG); }
		}
	}

	if (0 == mem)
		{ MCC_TRACE("br\n"); return PAGEALIGN(offset); }

	if (++copy == 2) { MCC_TRACE("br\n");
		goto redo;
	}
	if (copy == 3) { MCC_TRACE("br\n");
#if defined MCC_TARGET_PE && (defined MCC_TARGET_X86_64 || defined MCC_TARGET_ARM64)
		if (s1->uw_pdata) { MCC_TRACE("br\n");
			s1->run_function_table = host_unwind_register(
					(void *)s1->uw_pdata->sh_addr,
					(unsigned)s1->uw_pdata->data_offset,
					s1->pe_imagebase);
			if (!s1->run_function_table)
				{ MCC_TRACE("br\n"); mcc_error_noabort("RtlAddFunctionTable failed"); }
			s1->uw_pdata = NULL;
		}
#endif
		cleanup_symbols(s1);
		cleanup_sections(s1);
		goto redo;
	}

	relocate_syms(s1, s1->symtab, 1);
	if (s1->nb_errors)
		{ MCC_TRACE("br\n"); goto redo; }
#ifdef MCC_TARGET_PE
	s1->pe_imagebase = mem;
#else
	relocate_plt(s1);
#endif
	relocate_sections(s1);
#if defined(MCC_RUN_TLS_MACHO)
	tls_finalize_macho(s1);
#elif MCC_HOST_LINUX
	tls_seed_linux(s1);
#endif
	goto redo;
}

static void bt_link(MCCState *s1) { MCC_TRACE("enter\n");
	rt_context *rc;
	if (!s1->do_backtrace)
		{ MCC_TRACE("br\n"); return; }
	rc = mcc_get_symbol(s1, "__rt_info");
	if (!rc)
		{ MCC_TRACE("br\n"); return; }
	rc->esym_start = (ElfW(Sym) *)(symtab_section->data);
	rc->esym_end = (ElfW(Sym) *)(symtab_section->data + symtab_section->data_offset);
	rc->elf_str = (char *)symtab_section->link->data;
	if (MCC_PTR_SIZE == 8 && !s1->dwarf)
		{ MCC_TRACE("br\n"); rc->prog_base &= 0xffffffff00000000ULL; }
	if (s1->do_bounds_check) { MCC_TRACE("br\n");
		void *p;
		if ((p = mcc_get_symbol(s1, "__bound_init")))
			{ MCC_TRACE("br\n"); ((void (*)(void *, int))p)(rc->bounds_start, 1); }
	}
	rc->next = g_rc, g_rc = rc, s1->rc = rc;
	if (0 == signal_set)
		{ MCC_TRACE("br\n"); set_exception_handler(), signal_set = 1; }
}

static void st_link(MCCState *s1) { MCC_TRACE("enter\n");
	rt_wait_sem();
	s1->next = g_s1, g_s1 = s1;
	bt_link(s1);
	rt_post_sem();
}

static void ptr_unlink(void *list, void *e, unsigned next) { MCC_TRACE("enter\n");
	void **pp, **nn, *p;
	for (pp = list; !!(p = *pp); pp = nn) { MCC_TRACE("br\n");
		nn = (void *)((char *)p + next);
		if (p == e) { MCC_TRACE("br\n");
			*pp = *nn;
			break;
		}
	}
}

static void st_unlink(MCCState *s1) { MCC_TRACE("enter\n");
	rt_wait_sem();
	ptr_unlink(&g_rc, s1->rc, offsetof(rt_context, next));
	ptr_unlink(&g_s1, s1, offsetof(MCCState, next));
	rt_post_sem();
}

LIBMCCAPI void *_mcc_setjmp(MCCState *s1, void *p_jmp_buf, void *func, void *p_longjmp) { MCC_TRACE("enter\n");
	s1->run_lj = p_longjmp;
	s1->run_jb = p_jmp_buf;
	if (s1->rc)
		{ MCC_TRACE("br\n"); s1->rc->top_func = func; }
	return p_jmp_buf;
}

LIBMCCAPI void mcc_set_backtrace_func(MCCState *s1, void *data, MCCBtFunc *func) { MCC_TRACE("enter\n");
	s1->bt_func = func;
	s1->bt_data = data;
}

static MCCState *rt_find_state(rt_frame *f) { MCC_TRACE("enter\n");
	MCCState *s;
	addr_t pc;

	s = g_s1;
	if (NULL == s || NULL == s->next) { MCC_TRACE("br\n");
		return s;
	}
	for (int level = 0; level < 8; ++level) { MCC_TRACE("br\n");
		if (rt_get_caller_pc(&pc, f, level) < 0)
			{ MCC_TRACE("br\n"); break; }
		for (s = g_s1; s; s = s->next) { MCC_TRACE("br\n");
			if (pc >= (addr_t)s->run_ptr && pc < (addr_t)s->run_ptr + s->run_size)
				{ MCC_TRACE("br\n"); return s; }
		}
	}
	return NULL;
}

static void rt_exit(rt_frame *f, int code) { MCC_TRACE("enter\n");
	MCCState *s;
	rt_wait_sem();
	s = rt_find_state(f);
	rt_post_sem();
	if (s && s->run_lj) { MCC_TRACE("br\n");
		if (f->fp) { MCC_TRACE("br\n");
			void *p = mcc_get_symbol(s, "__bound_exit");
			if (p)
				{ MCC_TRACE("br\n"); ((void (*)(void))p)(); }
		}
		if (code == 0)
			{ MCC_TRACE("br\n"); code = RT_EXIT_ZERO; }
		((void (*)(void *, int))s->run_lj)(s->run_jb, code);
	}
	exit(code);
}

#else
static void rt_exit(rt_frame *f, int code) { MCC_TRACE("enter\n");
	exit(code);
}
#endif

static int rt_vprintf(const char *fmt, va_list ap) { MCC_TRACE("enter\n");
	int ret = vfprintf(stderr, fmt, ap);
	fflush(stderr);
	return ret;
}

static int rt_printf(const char *fmt, ...) { MCC_TRACE("enter\n");
	va_list ap;
	int r;
	va_start(ap, fmt);
	r = rt_vprintf(fmt, ap);
	va_end(ap);
	return r;
}

static char *rt_elfsym(rt_context *rc, addr_t wanted_pc, addr_t *func_addr) { MCC_TRACE("enter\n");
	ElfW(Sym) * esym;
	for (esym = rc->esym_start + 1; esym < rc->esym_end; ++esym) { MCC_TRACE("br\n");
		int type = ELFW(ST_TYPE)(esym->st_info);
		if ((type == STT_FUNC || type == STT_GNU_IFUNC) && wanted_pc >= esym->st_value && wanted_pc < esym->st_value + esym->st_size) { MCC_TRACE("br\n");
			*func_addr = esym->st_value;
			return rc->elf_str + esym->st_name;
		}
	}
	return NULL;
}

typedef struct bt_info {
	char file[100];
	int line;
	char func[100];
	addr_t func_pc;
} bt_info;

static addr_t rt_printline(rt_context *rc, addr_t wanted_pc, bt_info *bi) { MCC_TRACE("enter\n");
	char func_name[128];
	addr_t func_addr, last_pc, pc;
	const char *incl_files[INCLUDE_STACK_SIZE];
	int incl_index, last_incl_index, len, last_line_num, i;
	const char *str, *p;
	Stab_Sym *sym;

	func_name[0] = '\0';
	func_addr = 0;
	incl_index = 0;
	last_pc = (addr_t)-1;
	last_line_num = 1;
	last_incl_index = 0;

	for (sym = rc->stab_sym + 1; sym < rc->stab_sym_end; ++sym) { MCC_TRACE("br\n");
		str = rc->stab_str + sym->n_strx;
		pc = sym->n_value;

		switch (sym->n_type) { MCC_TRACE("br\n");
		case N_SLINE:
			if (func_addr)
				{ MCC_TRACE("br\n"); goto rel_pc; }
		case N_SO:
		case N_SOL:
			goto abs_pc;
		case N_FUN:
			if (sym->n_strx == 0)
				{ MCC_TRACE("br\n"); goto rel_pc; }
		abs_pc:
#if MCC_PTR_SIZE == 8
			pc += rc->prog_base;
#endif
			goto check_pc;
		rel_pc:
			pc += func_addr;
		check_pc:
			if (pc >= wanted_pc && wanted_pc >= last_pc)
				{ MCC_TRACE("br\n"); goto found; }
			break;
		}

		switch (sym->n_type) { MCC_TRACE("br\n");
		case N_FUN:
			if (sym->n_strx == 0)
				{ MCC_TRACE("br\n"); goto reset_func; }
			p = strchr(str, ':');
			if (0 == p || (len = p - str + 1, len > sizeof func_name))
				{ MCC_TRACE("br\n"); len = sizeof func_name; }
			pstrcpy(func_name, len, str);
			func_addr = pc;
			break;
		case N_SLINE:
			last_pc = pc;
			last_line_num = sym->n_desc;
			last_incl_index = incl_index;
			break;
		case N_BINCL:
			if (incl_index < INCLUDE_STACK_SIZE)
				{ MCC_TRACE("br\n"); incl_files[incl_index++] = str; }
			break;
		case N_EINCL:
			if (incl_index > 1)
				{ MCC_TRACE("br\n"); incl_index--; }
			break;
		case N_SO:
			incl_index = 0;
			if (sym->n_strx) { MCC_TRACE("br\n");
				len = strlen(str);
				if (len > 0 && str[len - 1] != '/')
					{ MCC_TRACE("br\n"); incl_files[incl_index++] = str; }
			}
		reset_func:
			func_name[0] = '\0';
			func_addr = 0;
			last_pc = (addr_t)-1;
			break;
		case N_SOL:
			if (incl_index)
				{ MCC_TRACE("br\n"); incl_files[incl_index - 1] = str; }
			break;
		}
	}
	last_incl_index = 0, func_name[0] = 0, func_addr = 0;
found:
	i = last_incl_index;
	if (i > 0) { MCC_TRACE("br\n");
		pstrcpy(bi->file, sizeof bi->file, incl_files[--i]);
		bi->line = last_line_num;
	}
	pstrcpy(bi->func, sizeof bi->func, func_name);
	bi->func_pc = func_addr;
	return func_addr;
}

#define DIR_TABLE_SIZE (64)
#define FILE_TABLE_SIZE (512)

#define dwarf_ignore_type(ln, end)    \
	switch (entry_format[j].form) {     \
	case DW_FORM_data1:                 \
		(ln) += 1;                        \
		break;                            \
	case DW_FORM_data2:                 \
		(ln) += 2;                        \
		break;                            \
	case DW_FORM_data4:                 \
		(ln) += 3;                        \
		break;                            \
	case DW_FORM_data8:                 \
		(ln) += 8;                        \
		break;                            \
	case DW_FORM_data16:                \
		(ln) += 16;                       \
		break;                            \
	case DW_FORM_udata:                 \
		dwarf_read_uleb128(&(ln), (end)); \
		break;                            \
	default:                            \
		goto next_line;                   \
	}

static addr_t rt_printline_dwarf(rt_context *rc, addr_t wanted_pc, bt_info *bi) { MCC_TRACE("enter\n");
	unsigned char *ln;
	unsigned char *cp;
	unsigned char *end;
	unsigned char *opcode_length;
	unsigned long long size;
	unsigned int length;
	unsigned char version;
	unsigned int min_insn_length;
	unsigned int max_ops_per_insn;
	int line_base;
	unsigned int line_range;
	unsigned int opcode_base;
	unsigned int opindex;
	unsigned int col;
	unsigned int i;
	unsigned int j;
	unsigned int len;
	unsigned long long value;
	struct
	{
		unsigned int type;
		unsigned int form;
	} entry_format[256];
	unsigned int dir_size;
	char *dirs[DIR_TABLE_SIZE];
	unsigned int filename_size;
	struct
	{
		unsigned int dir_entry;
		char *name;
	} filename_table[FILE_TABLE_SIZE];
	addr_t last_pc;
	addr_t pc;
	addr_t func_addr;
	int line;
	char *filename;
	char *function;
	unsigned int file_dir;

	filename = NULL;
	function = NULL;
	func_addr = 0;
	line = 0;
	file_dir = 0;

	ln = rc->dwarf_line;
	while (ln < rc->dwarf_line_end) { MCC_TRACE("br\n");
		dir_size = 0;
		filename_size = 0;
		last_pc = 0;
		pc = 0;
		func_addr = 0;
		line = 1;
		filename = NULL;
		function = NULL;
		file_dir = 0;
		length = 4;
		size = dwarf_read_4(ln, rc->dwarf_line_end);
		if (size == 0xffffffffu)
			{ MCC_TRACE("br\n"); length = 8, size = dwarf_read_8(ln, rc->dwarf_line_end); }
		end = ln + size;
		if (end < ln || end > rc->dwarf_line_end)
			{ MCC_TRACE("br\n"); break; }
		version = dwarf_read_2(ln, end);
		if (version >= 5)
			{ MCC_TRACE("br\n"); ln += length + 2; }
		else
			{ MCC_TRACE("br\n"); ln += length; }
		min_insn_length = dwarf_read_1(ln, end);
		if (version >= 4)
			{ MCC_TRACE("br\n"); max_ops_per_insn = dwarf_read_1(ln, end); }
		else
			{ MCC_TRACE("br\n"); max_ops_per_insn = 1; }
		ln++;
		line_base = dwarf_read_1(ln, end);
		line_base |= line_base >= 0x80 ? ~0xff : 0;
		line_range = dwarf_read_1(ln, end);
		opcode_base = dwarf_read_1(ln, end);
		opcode_length = ln;
		ln += opcode_base - 1;
		opindex = 0;
		if (version >= 5) { MCC_TRACE("br\n");
			col = dwarf_read_1(ln, end);
			for (i = 0; i < col; i++) { MCC_TRACE("br\n");
				entry_format[i].type = dwarf_read_uleb128(&ln, end);
				entry_format[i].form = dwarf_read_uleb128(&ln, end);
			}
			dir_size = dwarf_read_uleb128(&ln, end);
			for (i = 0; i < dir_size; i++) { MCC_TRACE("br\n");
				for (j = 0; j < col; j++) { MCC_TRACE("br\n");
					if (entry_format[j].type == DW_LNCT_path) { MCC_TRACE("br\n");
						if (entry_format[j].form != DW_FORM_line_strp)
							{ MCC_TRACE("br\n"); goto next_line; }
						value = length == 4
												? dwarf_read_4(ln, end)
												: dwarf_read_8(ln, end);
						if (i < DIR_TABLE_SIZE)
							{ MCC_TRACE("br\n"); dirs[i] = (char *)rc->dwarf_line_str + value; }
					} else
						{ MCC_TRACE("br\n"); dwarf_ignore_type(ln, end); }
				}
			}
			col = dwarf_read_1(ln, end);
			for (i = 0; i < col; i++) { MCC_TRACE("br\n");
				entry_format[i].type = dwarf_read_uleb128(&ln, end);
				entry_format[i].form = dwarf_read_uleb128(&ln, end);
			}
			filename_size = dwarf_read_uleb128(&ln, end);
			for (i = 0; i < filename_size && i < FILE_TABLE_SIZE; i++)
				{ MCC_TRACE("br\n"); filename_table[i].dir_entry = 0; }
			for (i = 0; i < filename_size; i++)
				{ MCC_TRACE("br\n"); for (j = 0; j < col; j++) { MCC_TRACE("br\n");
					if (entry_format[j].type == DW_LNCT_path) { MCC_TRACE("br\n");
						if (entry_format[j].form != DW_FORM_line_strp)
							{ MCC_TRACE("br\n"); goto next_line; }
						value = length == 4
												? dwarf_read_4(ln, end)
												: dwarf_read_8(ln, end);
						if (i < FILE_TABLE_SIZE)
							{ MCC_TRACE("br\n"); filename_table[i].name =
									(char *)rc->dwarf_line_str + value; }
					} else if (entry_format[j].type == DW_LNCT_directory_index) { MCC_TRACE("br\n");
						switch (entry_format[j].form) { MCC_TRACE("br\n");
						case DW_FORM_data1:
							value = dwarf_read_1(ln, end);
							break;
						case DW_FORM_data2:
							value = dwarf_read_2(ln, end);
							break;
						case DW_FORM_data4:
							value = dwarf_read_4(ln, end);
							break;
						case DW_FORM_udata:
							value = dwarf_read_uleb128(&ln, end);
							break;
						default:
							goto next_line;
						}
						if (i < FILE_TABLE_SIZE)
							{ MCC_TRACE("br\n"); filename_table[i].dir_entry = value; }
					} else
						{ MCC_TRACE("br\n"); dwarf_ignore_type(ln, end); }
				} }
		} else { MCC_TRACE("br\n");
			while ((dwarf_read_1(ln, end))) { MCC_TRACE("br\n");
				if (++dir_size < DIR_TABLE_SIZE)
					{ MCC_TRACE("br\n"); dirs[dir_size] = (char *)ln - 1; }
				while (dwarf_read_1(ln, end)) { MCC_TRACE("br\n");
				}
			}
			while ((dwarf_read_1(ln, end))) { MCC_TRACE("br\n");
				if (++filename_size < FILE_TABLE_SIZE) { MCC_TRACE("br\n");
					filename_table[filename_size - 1].name = (char *)ln - 1;
					while (dwarf_read_1(ln, end)) { MCC_TRACE("br\n");
					}
					filename_table[filename_size - 1].dir_entry =
							dwarf_read_uleb128(&ln, end);
				} else { MCC_TRACE("br\n");
					while (dwarf_read_1(ln, end)) { MCC_TRACE("br\n");
					}
					dwarf_read_uleb128(&ln, end);
				}
				dwarf_read_uleb128(&ln, end);
				dwarf_read_uleb128(&ln, end);
			}
		}
		if (filename_size >= 1) { MCC_TRACE("br\n");
			filename = filename_table[0].name;
			file_dir = filename_table[0].dir_entry;
		}
		while (ln < end) { MCC_TRACE("br\n");
			last_pc = pc;
			i = dwarf_read_1(ln, end);
			if (i >= opcode_base) { MCC_TRACE("br\n");
				if (max_ops_per_insn == 1)
					{ MCC_TRACE("br\n"); pc += ((i - opcode_base) / line_range) * min_insn_length; }
				else { MCC_TRACE("br\n");
					pc += (opindex + (i - opcode_base) / line_range) /
								max_ops_per_insn * min_insn_length;
					opindex = (opindex + (i - opcode_base) / line_range) %
										max_ops_per_insn;
				}
				i = (int)((i - opcode_base) % line_range) + line_base;
			check_pc:
				if (pc >= wanted_pc && wanted_pc >= last_pc)
					{ MCC_TRACE("br\n"); goto found; }
				line += i;
			} else { MCC_TRACE("br\n");
				switch (i) { MCC_TRACE("br\n");
				case 0:
					len = dwarf_read_uleb128(&ln, end);
					cp = ln;
					ln += len;
					if (len == 0)
						{ MCC_TRACE("br\n"); goto next_line; }
					switch (dwarf_read_1(cp, end)) { MCC_TRACE("br\n");
					case DW_LNE_end_sequence:
						break;
					case DW_LNE_set_address:
#if MCC_PTR_SIZE == 4
						pc = dwarf_read_4(cp, end);
#else
						pc = dwarf_read_8(cp, end);
#endif
#if defined MCC_TARGET_MACHO
						pc += rc->prog_base;
#endif
						opindex = 0;
						break;
					case DW_LNE_define_file:
						if (++filename_size < FILE_TABLE_SIZE) { MCC_TRACE("br\n");
							filename_table[filename_size - 1].name = (char *)ln - 1;
							while (dwarf_read_1(ln, end)) { MCC_TRACE("br\n");
							}
							filename_table[filename_size - 1].dir_entry =
									dwarf_read_uleb128(&ln, end);
						} else { MCC_TRACE("br\n");
							while (dwarf_read_1(ln, end)) { MCC_TRACE("br\n");
							}
							dwarf_read_uleb128(&ln, end);
						}
						dwarf_read_uleb128(&ln, end);
						dwarf_read_uleb128(&ln, end);
						break;
					case DW_LNE_hi_user - 1:
						function = (char *)cp;
						func_addr = pc;
						break;
					default:
						break;
					}
					break;
				case DW_LNS_advance_pc:
					if (max_ops_per_insn == 1)
						{ MCC_TRACE("br\n"); pc += dwarf_read_uleb128(&ln, end) * min_insn_length; }
					else { MCC_TRACE("br\n");
						unsigned long long off = dwarf_read_uleb128(&ln, end);

						pc += (opindex + off) / max_ops_per_insn *
									min_insn_length;
						opindex = (opindex + off) % max_ops_per_insn;
					}
					i = 0;
					goto check_pc;
				case DW_LNS_advance_line:
					line += dwarf_read_sleb128(&ln, end);
					break;
				case DW_LNS_set_file:
					i = dwarf_read_uleb128(&ln, end);
					i -= i > 0 && version < 5;
					if (i < FILE_TABLE_SIZE && i < filename_size) { MCC_TRACE("br\n");
						filename = filename_table[i].name;
						file_dir = filename_table[i].dir_entry;
					}
					break;
				case DW_LNS_const_add_pc:
					if (max_ops_per_insn == 1)
						{ MCC_TRACE("br\n"); pc += ((255 - opcode_base) / line_range) * min_insn_length; }
					else { MCC_TRACE("br\n");
						unsigned int off = (255 - opcode_base) / line_range;

						pc += ((opindex + off) / max_ops_per_insn) *
									min_insn_length;
						opindex = (opindex + off) % max_ops_per_insn;
					}
					i = 0;
					goto check_pc;
				case DW_LNS_fixed_advance_pc:
					i = dwarf_read_2(ln, end);
					pc += i;
					opindex = 0;
					i = 0;
					goto check_pc;
				default:
					for (j = 0; j < opcode_length[i - 1]; j++)
						{ MCC_TRACE("br\n"); dwarf_read_uleb128(&ln, end); }
					break;
				}
			}
		}
	next_line:
		ln = end;
	}
	filename = function = NULL, func_addr = 0;
found:
	if (filename) { MCC_TRACE("br\n");
		if (file_dir && file_dir < DIR_TABLE_SIZE &&
				file_dir < dir_size + (version < 5) && filename[0] != '/')
			{ MCC_TRACE("br\n"); snprintf(bi->file, sizeof bi->file, "%s/%s",
							 dirs[file_dir], filename); }
		else
			{ MCC_TRACE("br\n"); pstrcpy(bi->file, sizeof bi->file, filename); }
		bi->line = line;
	}
	if (function)
		{ MCC_TRACE("br\n"); pstrcpy(bi->func, sizeof bi->func, function); }
	bi->func_pc = func_addr;
	return (addr_t)func_addr;
}
#ifndef MCC_CONFIG_BACKTRACE_ONLY
static
#endif
		int _mcc_backtrace(rt_frame *f, const char *fmt, va_list ap) { MCC_TRACE("enter\n");
	rt_context *rc, *rc2;
	addr_t pc;
	char skip[40], msg[200];
	int i, level, ret, n, one;
	const char *a, *b;
	bt_info bi;
	addr_t (*getinfo)(rt_context *, addr_t, bt_info *);

	skip[0] = 0;
	if (fmt[0] == '^' && (b = strchr(a = fmt + 1, fmt[0]))) { MCC_TRACE("br\n");
		memcpy(skip, a, b - a), skip[b - a] = 0;
		fmt = b + 1;
	}
	one = 0;
	if (fmt[0] == '\001')
		{ MCC_TRACE("br\n"); ++fmt, one = 1; }
	vsnprintf(msg, sizeof msg, fmt, ap);

	rt_wait_sem();
	rc = g_rc;
	getinfo = rt_printline, n = 6;
	if (rc) { MCC_TRACE("br\n");
		if (rc->dwarf)
			{ MCC_TRACE("br\n"); getinfo = rt_printline_dwarf; }
		if (rc->num_callers)
			{ MCC_TRACE("br\n"); n = rc->num_callers; }
	}

	for (i = level = 0; level < n; i++) { MCC_TRACE("br\n");
		ret = rt_get_caller_pc(&pc, f, i);
		if (ret == -1)
			{ MCC_TRACE("br\n"); break; }
		memset(&bi, 0, sizeof bi);
		for (rc2 = rc; rc2; rc2 = rc2->next) { MCC_TRACE("br\n");
			if (getinfo(rc2, pc, &bi))
				{ MCC_TRACE("br\n"); break; }
			if (!!(a = rt_elfsym(rc2, pc, &bi.func_pc))) { MCC_TRACE("br\n");
				pstrcpy(bi.func, sizeof bi.func, a);
				break;
			}
		}
		if (skip[0] && strstr(bi.file, skip))
			{ MCC_TRACE("br\n"); continue; }

		if (skip[0] && !bi.file[0] && !strncmp(bi.func, "__bound_", 8))
			{ MCC_TRACE("br\n"); continue; }
#ifndef MCC_CONFIG_BACKTRACE_ONLY
		{
			MCCState *s = rt_find_state(f);
			if (s && s->bt_func) { MCC_TRACE("br\n");
				ret = s->bt_func(
						s->bt_data,
						(void *)pc,
						bi.file[0] ? bi.file : NULL,
						bi.line,
						bi.func[0] ? bi.func : NULL,
						level == 0 ? msg : NULL);
				if (ret == 0)
					{ MCC_TRACE("br\n"); break; }
				goto check_break;
			}
		}
#endif
		if (bi.file[0]) { MCC_TRACE("br\n");
			rt_printf("%s:%d", bi.file, bi.line);
		} else { MCC_TRACE("br\n");
			rt_printf("0x%08llx", (long long)pc);
		}
		rt_printf(": %s %s", level ? "by" : "at", bi.func[0] ? bi.func : "???");
		if (level == 0) { MCC_TRACE("br\n");
			rt_printf(": %s", msg);
			if (one)
				{ MCC_TRACE("br\n"); break; }
		}
		rt_printf("\n");

#ifndef MCC_CONFIG_BACKTRACE_ONLY
	check_break:
#endif
		if (rc2 && bi.func_pc && bi.func_pc == (addr_t)rc2->top_func)
			{ MCC_TRACE("br\n"); break; }
		++level;
	}
	rt_post_sem();
	return 0;
}

static int rt_error(rt_frame *f, const char *fmt, ...) { MCC_TRACE("enter\n");
	va_list ap;
	char msg[200];
	int ret;
	va_start(ap, fmt);
	snprintf(msg, sizeof msg, "RUNTIME ERROR: %s", fmt);
	ret = _mcc_backtrace(f, msg, ap);
	va_end(ap);
	return ret;
}

static int rt_fault(int code, unsigned detail, HostFaultRegs *hr) { MCC_TRACE("enter\n");
	rt_frame f;

	f.ip = hr->pc, f.fp = hr->fp, f.sp = hr->sp;
	switch (code) { MCC_TRACE("br\n");
	case HOST_FAULT_DIVZERO:
		rt_error(&f, "division by zero");
		break;
	case HOST_FAULT_FPE:
		rt_error(&f, "floating point exception");
		break;
	case HOST_FAULT_MEM:
		rt_error(&f, "invalid memory access");
		break;
	case HOST_FAULT_ILL:
		rt_error(&f, "illegal instruction");
		break;
	case HOST_FAULT_ABORT:
		rt_error(&f, "abort() called");
		break;
	case HOST_FAULT_STACK:
		rt_error(&f, "stack overflow");
		break;
	case HOST_FAULT_TRAP:
		f.ip = *(addr_t *)f.sp;
		rt_error(&f, "breakpoint/single-step exception:");
		return 1;
	default:
		rt_error(&f, HOST_FAULT_OTHER_FMT, detail);
		break;
	}
	host_fault_unblock(detail);
	rt_exit(&f, 255);
	return 0;
}

static void set_exception_handler(void) { MCC_TRACE("enter\n");
	host_fault_install(rt_fault);
}

#if defined(__i386__) || defined(__x86_64__)
static int rt_get_caller_pc(addr_t *paddr, rt_frame *rc, int level) { MCC_TRACE("enter\n");
	if (level == 0) { MCC_TRACE("br\n");
		*paddr = rc->ip;
	} else { MCC_TRACE("br\n");
		addr_t fp = rc->fp;
		while (1) { MCC_TRACE("br\n");
			if (fp < 0x1000)
				{ MCC_TRACE("br\n"); return -1; }
			if (0 == --level)
				{ MCC_TRACE("br\n"); break; }
			fp = ((addr_t *)fp)[0];
		}
		*paddr = ((addr_t *)fp)[1];
	}
	return 0;
}

#elif defined(__arm__) && !MCC_HOST_WIN32
static int rt_get_caller_pc(addr_t *paddr, rt_frame *rc, int level) { MCC_TRACE("enter\n");
	if (level == 0) { MCC_TRACE("br\n");
		*paddr = rc->ip;
	} else { MCC_TRACE("br\n");
		addr_t fp = rc->fp;
		while (1) { MCC_TRACE("br\n");
			if (fp < 0x1000)
				{ MCC_TRACE("br\n"); return -1; }
			if (0 == --level)
				{ MCC_TRACE("br\n"); break; }
			fp = ((addr_t *)fp)[0];
		}
		*paddr = ((addr_t *)fp)[2];
	}
	return 0;
}

#elif defined(__aarch64__)
static int rt_get_caller_pc(addr_t *paddr, rt_frame *rc, int level) { MCC_TRACE("enter\n");
	if (level == 0) { MCC_TRACE("br\n");
		*paddr = rc->ip;
	} else { MCC_TRACE("br\n");
		addr_t fp = rc->fp;
		while (1) { MCC_TRACE("br\n");
			if (fp < 0x1000)
				{ MCC_TRACE("br\n"); return -1; }
			if (0 == --level)
				{ MCC_TRACE("br\n"); break; }
			fp = ((addr_t *)fp)[0];
		}
		*paddr = ((addr_t *)fp)[1];
	}
	return 0;
}

#elif defined(__riscv)
static int rt_get_caller_pc(addr_t *paddr, rt_frame *rc, int level) { MCC_TRACE("enter\n");
	if (level == 0) { MCC_TRACE("br\n");
		*paddr = rc->ip;
	} else { MCC_TRACE("br\n");
		addr_t fp = rc->fp;
		while (1) { MCC_TRACE("br\n");
			if (fp < 0x1000)
				{ MCC_TRACE("br\n"); return -1; }
			if (0 == --level)
				{ MCC_TRACE("br\n"); break; }
			fp = ((addr_t *)fp)[-2];
		}
		*paddr = ((addr_t *)fp)[-1];
	}
	return 0;
}

#else
#warning add arch specific rt_get_caller_pc()
static int rt_get_caller_pc(addr_t *paddr, rt_frame *rc, int level) { MCC_TRACE("enter\n");
	return -1;
}

#endif

#elif defined MCC_EMBED_JIT
LIBMCCAPI int mcc_relocate(MCCState *s1) { MCC_TRACE("enter\n");
	(void)s1;
	mcc_error_noabort("internal: mcc_relocate unavailable in a cross-target build");
	return -1;
}

LIBMCCAPI int mcc_run(MCCState *s1, int argc, char **argv) { MCC_TRACE("enter\n");
	(void)s1;
	(void)argc;
	(void)argv;
	return mcc_error_noabort("internal: mcc_run unavailable in a cross-target build");
}
#endif
