#ifndef MCC_AMALGAMATED
#define MCC_AMALGAMATED 1
#endif

#if MCC_AMALGAMATED
#include "mcchost.c"
#include "mccstats.c"
static int pp_diag_set_flag(MCCState *s, const char *name);
#include "mccpp.c"
#include "mccgen.c"
#include "mcccst.c"
#include "mccast.c"
#include "mccgpu.c"
#include "mccrt.c"
#include "mccircap.c"
#include "mccrir.c"
#include "mccdbg.c"
#include "mccasm.c"
#include "mccelf.c"
#include "mccrun.c"
#include "mccdis.c"
#ifdef MCC_EMBED_JIT
#include "mccjit_intent.c"
#include "mccjit_embed.c"
#endif
#ifdef MCC_TARGET_I386
#include "i386-gen.c"
#include "i386-link.c"
#include "i386-dis.c"
#include "i386-asm.c"
#elif defined(MCC_TARGET_ARM)
#include "arm-gen.c"
#include "arm-link.c"
#include "arm-dis.c"
#include "arm-asm.c"
#elif defined(MCC_TARGET_ARM64)
#include "arm64-gen.c"
#include "arm64-link.c"
#include "arm64-dis.c"
#include "arm64-asm.c"
#elif defined(MCC_TARGET_X86_64)
#include "x86_64-gen.c"
#include "x86_64-link.c"
#include "x86_64-dis.c"
#include "i386-asm.c"
#elif defined(MCC_TARGET_RISCV64)
#include "riscv64-gen.c"
#include "riscv64-link.c"
#include "riscv64-dis.c"
#include "riscv64-asm.c"
#else
#error unknown target
#endif
#ifdef MCC_TARGET_PE
#include "mccpe.c"
#endif
#ifdef MCC_TARGET_MACHO
#include "mccmacho.c"
#endif
#endif

#include "mcc.h"
#include "mccstats.h"

ST_DATA struct MCCState *mcc_state;
ST_DATA int mccjit_error_quiet;
ST_DATA int mcc_leakcheck_quiet;
HOST_SEM(mcc_compile_sem);
ST_DATA void **stk_data;
ST_DATA int nb_stk_data;
ST_DATA int stk_data_floor;
ST_DATA int g_debug;

#if defined MCC_TARGET_PE && defined MCC_TARGET_IS_HOST
static void mcc_add_systemdir(MCCState *s) { MCC_TRACE("enter\n");
	char buf[1000];
	if (host_system_dir(buf, sizeof buf) == 0)
		{ MCC_TRACE("br\n"); mcc_add_library_path(s, buf); }
}
#endif

PUB_FUNC void mcc_enter_state(MCCState *s1) { MCC_TRACE("enter\n");
	if (s1->error_set_jmp_enabled)
		{ MCC_TRACE("br\n"); return; }
	HOST_SEM_WAIT(&mcc_compile_sem);
	mcc_state = s1;
	mcc_log_verbose = s1->verbose;
}

PUB_FUNC void mcc_exit_state(MCCState *s1) { MCC_TRACE("enter\n");
	if (s1->error_set_jmp_enabled)
		{ MCC_TRACE("br\n"); return; }
	mcc_state = NULL;
	mcc_log_verbose = 0;
	HOST_SEM_POST(&mcc_compile_sem);
}

ST_FUNC char *pstrcpy(char *buf, size_t buf_size, const char *s) { MCC_TRACE("enter\n");
	char *q, *q_end;
	int c;

	if (buf_size > 0) { MCC_TRACE("br\n");
		q = buf;
		q_end = buf + buf_size - 1;
		while (q < q_end) { MCC_TRACE("br\n");
			c = *s++;
			if (c == '\0')
				{ MCC_TRACE("br\n"); break; }
			*q++ = c;
		}
		*q = '\0';
	}
	return buf;
}

ST_FUNC char *pstrcat(char *buf, size_t buf_size, const char *s) { MCC_TRACE("enter\n");
	size_t len;
	len = strlen(buf);
	if (len < buf_size)
		{ MCC_TRACE("br\n"); pstrcpy(buf + len, buf_size - len, s); }
	return buf;
}

ST_FUNC char *pstrncpy(char *out, size_t buf_size, const char *s, size_t num) { MCC_TRACE("enter\n");
	if (num >= buf_size)
		{ MCC_TRACE("br\n"); num = buf_size - 1; }
	memcpy(out, s, num);
	out[num] = '\0';
	return out;
}

PUB_FUNC char *mcc_basename(const char *name) { MCC_TRACE("enter\n");
	char *p = (char *)strchr(name, 0);
	while (p > name && !HOST_IS_DIRSEP(p[-1]))
		{ MCC_TRACE("br\n"); --p; }
	return p;
}

PUB_FUNC char *mcc_fileextension(const char *name) { MCC_TRACE("enter\n");
	char *b = mcc_basename(name);
	char *e = strrchr(b, '.');
	return e ? e : strchr(b, 0);
}

ST_FUNC char *mcc_load_text(int fd) { MCC_TRACE("enter\n");
	int len = mcc_fd_lseek(fd, 0, SEEK_END);
	char *buf = load_data(fd, 0, len + 1);
	buf[len] = 0;
	return buf;
}

static void mcc_set_str(char **pp, const char *str) { MCC_TRACE("enter\n");
	mcc_free(*pp);
	*pp = str ? mcc_strdup(str) : NULL;
}

static void mcc_concat_str(char **pp, const char *str, int sep) { MCC_TRACE("enter\n");
	int l = *pp ? strlen(*pp) + !!sep : 0;
	*pp = mcc_realloc(*pp, l + strlen(str) + 1);
	if (l && sep)
		{ MCC_TRACE("br\n"); ((*pp)[l - 1] = sep); }
	strcpy(*pp + l, str);
}

#undef free
#undef realloc

static void *default_reallocator(void *ptr, unsigned long size) { MCC_TRACE("enter\n");
	void *ptr1;
	if (size == 0) { MCC_TRACE("br\n");
		free(ptr);
		ptr1 = NULL;
	} else { MCC_TRACE("br\n");
		ptr1 = realloc(ptr, size);
		if (!ptr1) { MCC_TRACE("br\n");
			fprintf(stderr, "mcc: memory full\n");
			exit(1);
		}
	}
	return ptr1;
}

ST_FUNC void libc_free(void *ptr) { MCC_TRACE("enter\n");
	free(ptr);
}

#define free(p) use_mcc_free(p)
#define realloc(p, s) use_mcc_realloc(p, s)

static void *(*reallocator)(void *, unsigned long) = default_reallocator;

LIBMCCAPI void mcc_set_realloc(MCCReallocFunc *my_realloc) { MCC_TRACE("enter\n");
	reallocator = my_realloc ? my_realloc : default_reallocator;
}

#undef mcc_free
#undef mcc_malloc
#undef mcc_realloc
#undef mcc_mallocz
#undef mcc_strdup

PUB_FUNC void mcc_free(void *ptr) { MCC_TRACE("enter\n");
	reallocator(ptr, 0);
}

PUB_FUNC void *mcc_malloc(unsigned long size) { MCC_TRACE("enter\n");
	return reallocator(0, size);
}

PUB_FUNC void *mcc_realloc(void *ptr, unsigned long size) { MCC_TRACE("enter\n");
	return reallocator(ptr, size);
}

ST_FUNC unsigned long mcc_grow_capacity(unsigned long cur, unsigned long need,
																				unsigned long min_cap) { MCC_TRACE("enter\n");
	if (cur < min_cap)
		{ MCC_TRACE("br\n"); cur = min_cap; }
	while (cur < need)
		{ MCC_TRACE("br\n"); cur = cur * 2; }
	return cur;
}

ST_FUNC MAYBE_UNUSED int mcc_uleb128_size(unsigned long long value) { MCC_TRACE("enter\n");
	int size = 0;
	do { MCC_TRACE("br\n");
		value >>= 7;
		size++;
	} while (value != 0);
	return size;
}

ST_FUNC int mcc_sleb128_size(long long value) { MCC_TRACE("enter\n");
	int size = 0;
	long long end = value >> 63;
	unsigned char last = end & 0x40;
	unsigned char byte;

	do { MCC_TRACE("br\n");
		byte = value & 0x7f;
		value >>= 7;
		size++;
	} while (value != end || (byte & 0x40) != last);
	return size;
}

ST_FUNC void mcc_write_uleb128(Section *sec, unsigned long long value) { MCC_TRACE("enter\n");
	do { MCC_TRACE("br\n");
		unsigned char byte = value & 0x7f;

		value >>= 7;
		*(uint8_t *)section_ptr_add(sec, 1) = byte | (value ? 0x80 : 0);
	} while (value != 0);
}

ST_FUNC void mcc_write_sleb128(Section *sec, long long value) { MCC_TRACE("enter\n");
	long long end = value >> 63;
	unsigned char last = end & 0x40;
	int more;

	do { MCC_TRACE("br\n");
		unsigned char byte = value & 0x7f;

		value >>= 7;
		more = value != end || (byte & 0x40) != last;
		*(uint8_t *)section_ptr_add(sec, 1) = byte | (0x80 * more);
	} while (more);
}

PUB_FUNC void *mcc_mallocz(unsigned long size) { MCC_TRACE("enter\n");
	void *ptr;
	ptr = mcc_malloc(size);
	if (size)
		{ MCC_TRACE("br\n"); memset(ptr, 0, size); }
	return ptr;
}

PUB_FUNC char *mcc_strdup(const char *str) { MCC_TRACE("enter\n");
	char *ptr;
	ptr = mcc_malloc(strlen(str) + 1);
	strcpy(ptr, str);
	return ptr;
}

#if MCC_DIAG

#define MCC_MEM_DEBUG_MAGIC1 0xFEEDDEB1
#define MCC_MEM_DEBUG_MAGIC2 0xFEEDDEB2
#define MCC_MEM_DEBUG_MAGIC3 0xFEEDDEB3
#define MCC_MEM_DEBUG_FILE_LEN 40
#define MCC_MEM_DEBUG_CHECK3(header) \
	(((unsigned char *)header->magic3) + header->size)
#define MEM_USER_PTR(header) \
	((char *)header + offsetof(mem_debug_header_t, magic3))
#define MEM_HEADER_PTR(ptr) \
	(mem_debug_header_t *)((char *)ptr - offsetof(mem_debug_header_t, magic3))

struct mem_debug_header {
	unsigned magic1;
	unsigned size;
	struct mem_debug_header *prev;
	struct mem_debug_header *next;
	int line_num;
	char file_name[MCC_MEM_DEBUG_FILE_LEN];
	unsigned magic2;
	ALIGNED(16)
	unsigned char magic3[4];
};

typedef struct mem_debug_header mem_debug_header_t;

HOST_SEM(mem_sem);
static mem_debug_header_t *mem_debug_chain;
static unsigned mem_cur_size;
static unsigned mem_max_size;
static int nb_states;

static mem_debug_header_t *malloc_check(void *ptr, const char *msg) { MCC_TRACE("enter\n");
	mem_debug_header_t *header = MEM_HEADER_PTR(ptr);
	if (header->magic1 != MCC_MEM_DEBUG_MAGIC1 ||
			header->magic2 != MCC_MEM_DEBUG_MAGIC2 ||
			read32le(MCC_MEM_DEBUG_CHECK3(header)) != MCC_MEM_DEBUG_MAGIC3 ||
			header->size == (unsigned)-1) { MCC_TRACE("br\n");
		fprintf(stderr, "%s check failed\n", msg);
		if (header->magic1 == MCC_MEM_DEBUG_MAGIC1)
			{ MCC_TRACE("br\n"); fprintf(stderr, "%s:%u: block allocated here.\n",
							header->file_name, header->line_num); }
		exit(1);
	}
	return header;
}

PUB_FUNC void *mcc_malloc_debug(unsigned long size, const char *file, int line) { MCC_TRACE("enter\n");
	int ofs;
	mem_debug_header_t *header;
	if (!size)
		{ MCC_TRACE("br\n"); return NULL; }
	header = mcc_malloc(sizeof(mem_debug_header_t) + size);
	header->magic1 = MCC_MEM_DEBUG_MAGIC1;
	header->magic2 = MCC_MEM_DEBUG_MAGIC2;
	header->size = size;
	write32le(MCC_MEM_DEBUG_CHECK3(header), MCC_MEM_DEBUG_MAGIC3);
	header->line_num = line;
	ofs = strlen(file) + 1 - MCC_MEM_DEBUG_FILE_LEN;
	strcpy(header->file_name, file + (ofs > 0 ? ofs : 0));
	HOST_SEM_WAIT(&mem_sem);
	header->next = mem_debug_chain;
	header->prev = NULL;
	if (header->next)
		{ MCC_TRACE("br\n"); header->next->prev = header; }
	mem_debug_chain = header;
	mem_cur_size += size;
	if (mem_cur_size > mem_max_size)
		{ MCC_TRACE("br\n"); mem_max_size = mem_cur_size; }
	HOST_SEM_POST(&mem_sem);
	return MEM_USER_PTR(header);
}

PUB_FUNC void mcc_free_debug(void *ptr) { MCC_TRACE("enter\n");
	mem_debug_header_t *header;
	if (!ptr)
		{ MCC_TRACE("br\n"); return; }
	header = malloc_check(ptr, "mcc_free");
	HOST_SEM_WAIT(&mem_sem);
	mem_cur_size -= header->size;
	header->size = (unsigned)-1;
	if (header->next)
		{ MCC_TRACE("br\n"); header->next->prev = header->prev; }
	if (header->prev)
		{ MCC_TRACE("br\n"); header->prev->next = header->next; }
	if (header == mem_debug_chain)
		{ MCC_TRACE("br\n"); mem_debug_chain = header->next; }
	HOST_SEM_POST(&mem_sem);
	mcc_free(header);
}

PUB_FUNC void *mcc_mallocz_debug(unsigned long size, const char *file, int line) { MCC_TRACE("enter\n");
	void *ptr;
	ptr = mcc_malloc_debug(size, file, line);
	if (size)
		{ MCC_TRACE("br\n"); memset(ptr, 0, size); }
	return ptr;
}

PUB_FUNC void *mcc_realloc_debug(void *ptr, unsigned long size, const char *file, int line) { MCC_TRACE("enter\n");
	mem_debug_header_t *header;
	int mem_debug_chain_update = 0;

	if (!ptr)
		{ MCC_TRACE("br\n"); return mcc_malloc_debug(size, file, line); }
	if (!size) { MCC_TRACE("br\n");
		mcc_free_debug(ptr);
		return NULL;
	}
	header = malloc_check(ptr, "mcc_realloc");
	HOST_SEM_WAIT(&mem_sem);
	mem_cur_size -= header->size;
	mem_debug_chain_update = (header == mem_debug_chain);
	header = mcc_realloc(header, sizeof(mem_debug_header_t) + size);
	header->size = size;
	write32le(MCC_MEM_DEBUG_CHECK3(header), MCC_MEM_DEBUG_MAGIC3);
	if (header->next)
		{ MCC_TRACE("br\n"); header->next->prev = header; }
	if (header->prev)
		{ MCC_TRACE("br\n"); header->prev->next = header; }
	if (mem_debug_chain_update)
		{ MCC_TRACE("br\n"); mem_debug_chain = header; }
	mem_cur_size += size;
	if (mem_cur_size > mem_max_size)
		{ MCC_TRACE("br\n"); mem_max_size = mem_cur_size; }
	HOST_SEM_POST(&mem_sem);
	return MEM_USER_PTR(header);
}

PUB_FUNC char *mcc_strdup_debug(const char *str, const char *file, int line) { MCC_TRACE("enter\n");
	char *ptr;
	ptr = mcc_malloc_debug(strlen(str) + 1, file, line);
	strcpy(ptr, str);
	return ptr;
}

PUB_FUNC void mcc_memcheck(int d) { MCC_TRACE("enter\n");
	HOST_SEM_WAIT(&mem_sem);
	nb_states += d;
	if (0 == nb_states && mem_cur_size) { MCC_TRACE("br\n");
		if (!mcc_leakcheck_quiet) { MCC_TRACE("br\n");
			mem_debug_header_t *header = mem_debug_chain;
			fflush(stdout);
			fprintf(stderr, "mcc: mem_leak= %d bytes, mem_max_size= %d bytes\n",
							mem_cur_size, mem_max_size);
			while (header) { MCC_TRACE("br\n");
				fprintf(stderr, "%s:%u: error: %u bytes leaked\n",
								header->file_name, header->line_num, header->size);
				header = header->next;
			}
			fflush(stderr);
		}
		mem_cur_size = 0;
		mem_max_size = 0;
		mem_debug_chain = NULL;
	}
	HOST_SEM_POST(&mem_sem);
}

#define mcc_free(ptr) mcc_free_debug(ptr)
#define mcc_malloc(size) mcc_malloc_debug(size, __FILE__, __LINE__)
#define mcc_mallocz(size) mcc_mallocz_debug(size, __FILE__, __LINE__)
#define mcc_realloc(ptr, size) mcc_realloc_debug(ptr, size, __FILE__, __LINE__)
#define mcc_strdup(str) mcc_strdup_debug(str, __FILE__, __LINE__)

#endif

ST_FUNC int normalized_PATHCMP(const char *f1, const char *f2) { MCC_TRACE("enter\n");
	char *p1, *p2;
	int ret = 1;
	if (!!(p1 = host_path_canonical(f1))) { MCC_TRACE("br\n");
		if (!!(p2 = host_path_canonical(f2))) { MCC_TRACE("br\n");
			ret = HOST_PATHCMP(p1, p2);
			libc_free(p2);
		}
		libc_free(p1);
	}
	return ret;
}

ST_FUNC void dynarray_add(void *ptab, int *nb_ptr, void *data) { MCC_TRACE("enter\n");
	int nb, nb_alloc;
	void **pp;

	nb = *nb_ptr;
	pp = *(void ***)ptab;
	if ((nb & (nb - 1)) == 0) { MCC_TRACE("br\n");
		if (!nb)
			{ MCC_TRACE("br\n"); nb_alloc = 1; }
		else
			{ MCC_TRACE("br\n"); nb_alloc = nb * 2; }
		pp = mcc_realloc(pp, nb_alloc * sizeof(void *));
		*(void ***)ptab = pp;
	}
	pp[nb++] = data;
	*nb_ptr = nb;
}

ST_FUNC void dynarray_reset(void *pp, int *n) { MCC_TRACE("enter\n");
	void **p;
	for (p = *(void ***)pp; *n; ++p, --*n)
		{ MCC_TRACE("br\n"); if (*p)
			{ MCC_TRACE("br\n"); mcc_free(*p); } }
	mcc_free(*(void **)pp);
	*(void **)pp = NULL;
}

static void dynarray_split(char ***argv, int *argc, const char *p, int sep) { MCC_TRACE("enter\n");
	int qot, c;
	CString str;
	for (;;) { MCC_TRACE("br\n");
		while (c = (unsigned char)*p, c <= ' ' && c != '\0')
			{ MCC_TRACE("br\n"); ++p; }
		if (c == '\0')
			{ MCC_TRACE("br\n"); break; }
		cstr_new(&str);
		qot = 0;
		do { MCC_TRACE("br\n");
			++p;
			if (sep) { MCC_TRACE("br\n");
				if (c == sep)
					{ MCC_TRACE("br\n"); break; }
			} else { MCC_TRACE("br\n");
				if (c == '\\' && (*p == '"' || *p == '\\')) { MCC_TRACE("br\n");
					c = *p++;
				} else if (c == '"') { MCC_TRACE("br\n");
					qot ^= 1;
					continue;
				} else if (c <= ' ' && !qot) { MCC_TRACE("br\n");
					break;
				}
			}
			cstr_ccat(&str, c);
		} while (c = (unsigned char)*p, c != '\0');
		cstr_ccat(&str, '\0');
		dynarray_add(argv, argc, str.data);
	}
}

static void mcc_split_path(MCCState *s, void *p_ary, int *p_nb_ary, const char *in) { MCC_TRACE("enter\n");
	const char *p;
	do { MCC_TRACE("br\n");
		int c;
		CString str;

		cstr_new(&str);
		for (p = in; c = *p, c != '\0' && c != HOST_PATHSEP[0]; ++p) { MCC_TRACE("br\n");
			if (c == '{' && p[1] && p[2] == '}') { MCC_TRACE("br\n");
				c = p[1], p += 2;
				if (c == 'B')
					{ MCC_TRACE("br\n"); cstr_cat(&str, s->mcc_lib_path, -1); }
				if (c == 'R')
					{ MCC_TRACE("br\n"); cstr_cat(&str, (s->sysroot && s->sysroot[0]) ? s->sysroot : MCC_CONFIG_SYSROOT, -1); }
				if (c == 'f' && file) { MCC_TRACE("br\n");
					const char *f = file->true_filename;
					const char *b = mcc_basename(f);
					if (b > f)
						{ MCC_TRACE("br\n"); cstr_cat(&str, f, b - f - 1); }
					else
						{ MCC_TRACE("br\n"); cstr_cat(&str, ".", 1); }
				}
			} else { MCC_TRACE("br\n");
				cstr_ccat(&str, c);
			}
		}
		if (str.size) { MCC_TRACE("br\n");
			cstr_ccat(&str, '\0');
			dynarray_add(p_ary, p_nb_ary, str.data);
		}
		in = p + 1;
	} while (*p);
}

#define WARN_ON 1
#define WARN_ERR 2
#define WARN_NOE 4

enum {
	ERROR_WARN,
	ERROR_NOABORT,
	ERROR_ERROR
};

static int diag_want_color(MCCState *s1) { MCC_TRACE("enter\n");
	if (s1->error_func)
		{ MCC_TRACE("br\n"); return 0; }
	switch (s1->diag_color) { MCC_TRACE("br\n");
	case 1:
		return 1;
	case 2:
		return 0;
	default:
		return host_stderr_isatty();
	}
}

static void append_caret_context(MCCState *s1, CString *cs, BufferedFile *f,
																 int line, int bol_adj, int use_color) { MCC_TRACE("enter\n");
	const unsigned char *start, *end, *p, *cpos, *ls, *le;
	int col, n, i;
	char numbuf[16];

	if (s1->diag_no_caret || !f || line <= 0)
		{ MCC_TRACE("br\n"); return; }
	start = f->buffer;
	end = f->buf_end;
	p = f->buf_ptr;
	if (!start || !end || !p || end < start || p < start || p > end)
		{ MCC_TRACE("br\n"); return; }

	cpos = p;
	if (bol_adj) { MCC_TRACE("br\n");
		if (cpos <= start || cpos[-1] != '\n')
			{ MCC_TRACE("br\n"); return; }
		cpos--;
	} else if (cpos == end) { MCC_TRACE("br\n");
		if (cpos <= start)
			{ MCC_TRACE("br\n"); return; }
		cpos--;
	}

	ls = cpos;
	while (ls > start && ls[-1] != '\n')
		{ MCC_TRACE("br\n"); ls--; }
	le = cpos;
	while (le < end && *le != '\n')
		{ MCC_TRACE("br\n"); le++; }

	n = (int)(le - ls);
	if (n > 0 && ls[n - 1] == '\r')
		{ MCC_TRACE("br\n"); n--; }
	if (n <= 0 || n > 512)
		{ MCC_TRACE("br\n"); return; }

	col = (int)(cpos - ls);
	if (col < 0)
		{ MCC_TRACE("br\n"); col = 0; }
	if (col > n)
		{ MCC_TRACE("br\n"); col = n; }

	snprintf(numbuf, sizeof(numbuf), "%d", line);

	cstr_ccat(cs, '\n');
	cstr_cat(cs, numbuf, (int)strlen(numbuf));
	cstr_cat(cs, " | ", 3);
	cstr_cat(cs, (const char *)ls, n);

	cstr_ccat(cs, '\n');
	for (i = 0; numbuf[i]; i++)
		{ MCC_TRACE("br\n"); cstr_ccat(cs, ' '); }
	cstr_cat(cs, " | ", 3);
	for (i = 0; i < col; i++)
		{ MCC_TRACE("br\n"); cstr_ccat(cs, ls[i] == '\t' ? '\t' : ' '); }
	if (use_color)
		{ MCC_TRACE("br\n"); cstr_cat(cs, "\033[1;32m^\033[0m", 12); }
	else
		{ MCC_TRACE("br\n"); cstr_ccat(cs, '^'); }

	cstr_ccat(cs, '\0');
	cs->size--;
}

static void error1(int mode, const char *fmt, va_list ap) { MCC_TRACE("enter\n");
	BufferedFile **pf, *f;
	MCCState *s1 = mcc_state;
	CString cs;
	int line = 0;
	int explicit_line = 0, bol_adj = 0;
	int driver_note = 0;

	mcc_exit_state(s1);

	if (mode == ERROR_WARN) { MCC_TRACE("br\n");
		int wopt = -1;
		if (s1->warn_num) { MCC_TRACE("br\n");
			driver_note =
					s1->warn_num == (int)(offsetof(MCCState, warn_unsupported) -
																offsetof(MCCState, warn_none)) ||
					s1->warn_num == (int)(offsetof(MCCState, warn_unsupported_option) -
																offsetof(MCCState, warn_none));
			wopt = *(&s1->warn_none + s1->warn_num);
			s1->warn_num = 0;
		}
		if (s1->error_set_jmp_enabled) { MCC_TRACE("br\n");
			BufferedFile *wf;
			for (wf = file; wf && wf->filename[0] == ':'; wf = wf->prev)
				;
			if (wf && wf->system_header)
				{ MCC_TRACE("br\n"); return; }
		}
		if (s1->warn_error && !driver_note)
			{ MCC_TRACE("br\n"); mode = ERROR_ERROR; }
		if (wopt >= 0) { MCC_TRACE("br\n");
			if (0 == (wopt & WARN_ON))
				{ MCC_TRACE("br\n"); return; }
			if (wopt & WARN_ERR)
				{ MCC_TRACE("br\n"); mode = ERROR_ERROR; }
			if (wopt & WARN_NOE)
				{ MCC_TRACE("br\n"); mode = ERROR_WARN; }
		}
		if (s1->warn_none)
			{ MCC_TRACE("br\n"); return; }
	}

	cstr_new(&cs);
	if (fmt[0] == '%' && fmt[1] == 'i' && fmt[2] == ':')
		{ MCC_TRACE("br\n"); line = va_arg(ap, int), fmt += 3, explicit_line = 1; }
	f = NULL;
	if (s1->error_set_jmp_enabled) { MCC_TRACE("br\n");
		for (f = file; f && f->filename[0] == ':'; f = f->prev)
			;
	}
	if (f) { MCC_TRACE("br\n");
		for (pf = s1->include_stack; pf < s1->include_stack_ptr; pf++)
			{ MCC_TRACE("br\n"); cstr_printf(&cs, "In file included from %s:%d:\n",
									(*pf)->filename, (*pf)->line_num - 1); }
		if (0 == line) { MCC_TRACE("br\n");
			bol_adj = (tok_flags & TOK_FLAG_BOL) && !macro_ptr;
			line = f->line_num - bol_adj;
		}
		cstr_printf(&cs, "%s:%d: ", f->filename, line);
	} else if (s1->current_filename) { MCC_TRACE("br\n");
		cstr_printf(&cs, "%s: ", s1->current_filename);
	} else { MCC_TRACE("br\n");
		cstr_printf(&cs, "mcc: ");
	}
	int use_color = diag_want_color(s1);
	if (use_color)
		{ MCC_TRACE("br\n"); cstr_printf(&cs, mode == ERROR_WARN ? "\033[1;35mwarning:\033[0m " : "\033[1;31merror:\033[0m "); }
	else
		{ MCC_TRACE("br\n"); cstr_printf(&cs, mode == ERROR_WARN ? "warning: " : "error: "); }
	if (pp_expr > 1)
		{ MCC_TRACE("br\n"); pp_error(&cs); }
	else
		{ MCC_TRACE("br\n"); cstr_vprintf(&cs, fmt, ap); }
	if (f && !explicit_line && !macro_ptr)
		{ MCC_TRACE("br\n"); append_caret_context(s1, &cs, f, line, bol_adj, use_color); }
	if (mccjit_error_quiet) { MCC_TRACE("br\n");
		;
	} else if (!s1->error_func) { MCC_TRACE("br\n");
		if (s1 && s1->output_type == MCC_OUTPUT_PREPROCESS && s1->ppfp == stdout)
			{ MCC_TRACE("br\n"); printf("\n"); }
		fflush(stdout);
		fprintf(stderr, "%s\n", (char *)cs.data);
		fflush(stderr);
	} else { MCC_TRACE("br\n");
		s1->error_func(s1->error_opaque, (char *)cs.data);
	}
	cstr_free(&cs);
	if (mode != ERROR_WARN)
		{ MCC_TRACE("br\n"); s1->nb_errors++; mcc_leakcheck_quiet = 1; }
	if (mode == ERROR_NOABORT && s1->error_set_jmp_enabled && (s1->warn_fatal_errors || (s1->max_errors && s1->nb_errors >= s1->max_errors)))
		{ MCC_TRACE("br\n"); mode = ERROR_ERROR; }
	if (mode == ERROR_ERROR && s1->error_set_jmp_enabled) { MCC_TRACE("br\n");
		while (nb_stk_data > stk_data_floor)
			{ MCC_TRACE("br\n"); mcc_free(*(void **)stk_data[--nb_stk_data]); }
		longjmp(s1->error_jmp_buf, 1);
	}
}

LIBMCCAPI void mcc_set_error_func(MCCState *s, void *error_opaque, MCCErrorFunc *error_func) { MCC_TRACE("enter\n");
	s->error_opaque = error_opaque;
	s->error_func = error_func;
}

PUB_FUNC int _mcc_error_noabort(const char *fmt, ...) { MCC_TRACE("enter\n");
	va_list ap;
	va_start(ap, fmt);
	error1(ERROR_NOABORT, fmt, ap);
	va_end(ap);
	return -1;
}

#undef _mcc_error
PUB_FUNC void _mcc_error(const char *fmt, ...) { MCC_TRACE("enter\n");
	va_list ap;
	va_start(ap, fmt);
	error1(ERROR_ERROR, fmt, ap);
	exit(1);
}

#define _mcc_error use_mcc_error_noabort

PUB_FUNC void _mcc_warning(const char *fmt, ...) { MCC_TRACE("enter\n");
	va_list ap;
	va_start(ap, fmt);
	error1(ERROR_WARN, fmt, ap);
	va_end(ap);
}

ST_FUNC void mcc_open_bf(MCCState *s1, const char *filename, int initlen) { MCC_TRACE("enter\n");
	BufferedFile *bf;
	int buflen = initlen ? initlen : IO_BUF_SIZE;

	bf = mcc_mallocz(sizeof(BufferedFile) + buflen);
	bf->buf_ptr = bf->buffer;
	bf->buf_end = bf->buffer + initlen;
	bf->buf_end[0] = CH_EOB;
	pstrcpy(bf->filename, sizeof(bf->filename), filename);
	host_path_normalize(bf->filename);
	bf->true_filename = bf->filename;
	bf->line_num = 1;
	bf->ifdef_stack_ptr = s1->ifdef_stack_ptr;
	bf->fd = -1;
	bf->prev = file;
	bf->prev_tok_flags = tok_flags;
	file = bf;
	tok_flags = TOK_FLAG_BOL | TOK_FLAG_BOF;
}

ST_FUNC void mcc_close(void) { MCC_TRACE("enter\n");
	MCCState *s1 = mcc_state;
	BufferedFile *bf = file;
	if (bf->fd > 0) { MCC_TRACE("br\n");
		close(bf->fd);
		total_lines += bf->line_num - 1;
	}
	if (bf->true_filename != bf->filename)
		{ MCC_TRACE("br\n"); mcc_free(bf->true_filename); }
	file = bf->prev;
	tok_flags = bf->prev_tok_flags;
	mcc_free(bf);
}

static int _mcc_open(MCCState *s1, const char *filename, int binary) { MCC_TRACE("enter\n");
	int fd;
	if (strcmp(filename, "-") == 0)
		{ MCC_TRACE("br\n"); fd = 0, filename = "<stdin>"; }
	else if (binary)
		{ MCC_TRACE("br\n"); fd = mcc_winfd_open_ro(filename); }
	else
		{ MCC_TRACE("br\n"); fd = open(filename, O_RDONLY | O_BINARY); }
	if ((MCC_VTIER(s1->verbose) == MCC_V2 && fd >= 0) ||
			MCC_VTIER(s1->verbose) == MCC_V3)
		{ MCC_TRACE("br\n"); printf("%s %*s%s\n", fd < 0 ? "nf" : "->",
					 (int)(s1->include_stack_ptr - s1->include_stack), "", filename); }
	return fd;
}

ST_FUNC int mcc_open(MCCState *s1, const char *filename) { MCC_TRACE("enter\n");
	int fd = _mcc_open(s1, filename, 0);
	if (fd < 0)
		{ MCC_TRACE("br\n"); return -1; }
	if (strcmp(filename, "-") == 0)
		{ MCC_TRACE("br\n"); filename = "<stdin>"; }
	mcc_open_bf(s1, filename, 0);
	file->fd = fd;
	return 0;
}

static void permerror_promote(MCCState *s1, const unsigned short *off, unsigned n) { MCC_TRACE("enter\n");
	unsigned i;
	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		unsigned char *f = (unsigned char *)s1 + off[i];
		if (*f & WARN_ON)
			{ MCC_TRACE("br\n"); *f |= WARN_ERR; }
	}
}

static void mcc_apply_std_permerrors(MCCState *s1) { MCC_TRACE("enter\n");
	static const unsigned short removed_in_c99[] = {
			offsetof(MCCState, warn_implicit_function_declaration),
			offsetof(MCCState, warn_implicit_int)};
	static const unsigned short constraint_violation[] = {
			offsetof(MCCState, warn_incompatible_pointer_types),
			offsetof(MCCState, warn_int_conversion),
			offsetof(MCCState, warn_return_type)};

	if (s1->permissive)
		{ MCC_TRACE("br\n"); return; }
	if (s1->cversion >= 199901)
		{ MCC_TRACE("br\n"); permerror_promote(s1, removed_in_c99,
									sizeof(removed_in_c99) / sizeof(removed_in_c99[0])); }
	if (s1->cversion >= 199901 || s1->pedantic_errors)
		{ MCC_TRACE("br\n"); permerror_promote(s1, constraint_violation,
									sizeof(constraint_violation) / sizeof(constraint_violation[0])); }
}

static int mcc_compile(MCCState *s1, int filetype, const char *str, int fd) { MCC_TRACE("enter\n");
	mcc_apply_std_permerrors(s1);
	mcc_enter_state(s1);
	MCC_TRACE("filetype=%d fd=%d\n", filetype, fd);
	s1->error_set_jmp_enabled = 1;

	if (setjmp(s1->error_jmp_buf) == 0) { MCC_TRACE("br\n");
		if (fd == -1) { MCC_TRACE("br\n");
			int len = strlen(str);
			mcc_open_bf(s1, "<string>", len);
			memcpy(file->buffer, str, len);
		} else { MCC_TRACE("br\n");
			mcc_open_bf(s1, str, 0);
			file->fd = fd;
		}

		int cst_on = (s1->lsp && fd != -1 &&
									s1->output_type != MCC_OUTPUT_PREPROCESS &&
									!(filetype & (AFF_TYPE_ASM | AFF_TYPE_ASMPP)));
		if (cst_on)
			{ MCC_TRACE("br\n"); cst_capture_begin(str); }

		preprocess_start(s1, filetype);
		mccgen_init(s1);
		if (!(filetype & AFF_TYPE_ASM))
			{ MCC_TRACE("br\n"); mccpp_run_imacros(s1); }

		if (s1->output_type == MCC_OUTPUT_PREPROCESS) { MCC_TRACE("br\n");
			mcc_preprocess(s1);
		} else { MCC_TRACE("br\n");
			mccelf_begin_file(s1);
			if (filetype & (AFF_TYPE_ASM | AFF_TYPE_ASMPP)) { MCC_TRACE("br\n");
				mcc_assemble(s1, !!(filetype & AFF_TYPE_ASMPP));
			} else { MCC_TRACE("br\n");
				mccgen_compile(s1);
				if (cst_on)
					{ MCC_TRACE("br\n"); cst_capture_end(); }
			}
			mccelf_end_file(s1);
		}
	}
	mccgen_finish(s1);
	preprocess_end(s1);
	s1->error_set_jmp_enabled = 0;
	mcc_exit_state(s1);
	return s1->nb_errors != 0 ? -1 : 0;
}

LIBMCCAPI int mcc_compile_string(MCCState *s, const char *str) { MCC_TRACE("enter\n");
	return mcc_compile(s, s->filetype, str, -1);
}

LIBMCCAPI void mcc_define_symbol(MCCState *s1, const char *sym, const char *value) { MCC_TRACE("enter\n");
	const char *eq;
	if (NULL == (eq = strchr(sym, '=')))
		{ MCC_TRACE("br\n"); eq = strchr(sym, 0); }
	if (NULL == value)
		{ MCC_TRACE("br\n"); value = *eq ? eq + 1 : "1"; }
	cstr_printf(&s1->cmdline_defs, "#define %.*s %s\n", (int)(eq - sym), sym, value);
}

LIBMCCAPI void mcc_undefine_symbol(MCCState *s1, const char *sym) { MCC_TRACE("enter\n");
	cstr_printf(&s1->cmdline_defs, "#undef %s\n", sym);
}

#if MCC_CONFIG_AUTO_MCCDIR && MCC_HOST_POSIX
#include <sys/stat.h>
static char auto_mccdir_buf[1024];

static int mcc_dir_has_include(const char *base) { MCC_TRACE("enter\n");
	char probe[1024];
	struct stat st;
	if (!base || !base[0])
		{ MCC_TRACE("br\n"); return 0; }
	pstrcpy(probe, sizeof probe, base);
	pstrcat(probe, sizeof probe, "/include/mccdefs.h");
	return stat(probe, &st) == 0 && S_ISREG(st.st_mode);
}

static const char *mcc_auto_mccdir_rel(const char *exedir, const char *suffix) { MCC_TRACE("enter\n");
	pstrcpy(auto_mccdir_buf, sizeof auto_mccdir_buf, exedir);
	pstrcat(auto_mccdir_buf, sizeof auto_mccdir_buf, suffix);
	return mcc_dir_has_include(auto_mccdir_buf) ? auto_mccdir_buf : NULL;
}

static const char *mcc_auto_mccdir(void) { MCC_TRACE("enter\n");
	static const char *const exe_rel[] = {"", "/..", "/../lib/mcc", NULL};
	static const char *const sys_dirs[] = {"/usr/local/lib/mcc",
																				 "/usr/lib/mcc", NULL};
	char exe[1024];
	const char *hit;
	char *p;
	int i;
	if (host_exe_path(exe, sizeof exe) > 0) { MCC_TRACE("br\n");
		p = mcc_basename(exe);
		if (p > exe)
			{ MCC_TRACE("br\n"); --p; }
		*p = 0;
		for (i = 0; exe_rel[i]; i++)
			{ MCC_TRACE("br\n"); if ((hit = mcc_auto_mccdir_rel(exe, exe_rel[i])))
				{ MCC_TRACE("br\n"); return hit; } }
	}
	if (mcc_dir_has_include(MCC_CONFIG_MCCDIR))
		{ MCC_TRACE("br\n"); return MCC_CONFIG_MCCDIR; }
	for (i = 0; sys_dirs[i]; i++)
		{ MCC_TRACE("br\n"); if (mcc_dir_has_include(sys_dirs[i]))
			{ MCC_TRACE("br\n"); return sys_dirs[i]; } }
	return MCC_CONFIG_MCCDIR;
}

#define MCC_MCCDIR_DEFAULT mcc_auto_mccdir()
#else
#define MCC_MCCDIR_DEFAULT MCC_CONFIG_MCCDIR
#endif

#define MCC_ISA_V1 (MCC_ISA_SSE2)
#define MCC_ISA_V2 (MCC_ISA_V1 | MCC_ISA_SSE3 | MCC_ISA_SSSE3 | MCC_ISA_SSE41 | \
                    MCC_ISA_SSE42 | MCC_ISA_POPCNT)
#define MCC_ISA_V3 (MCC_ISA_V2 | MCC_ISA_AVX | MCC_ISA_AVX2 | MCC_ISA_FMA | \
                    MCC_ISA_BMI | MCC_ISA_BMI2 | MCC_ISA_LZCNT | MCC_ISA_F16C)
#define MCC_ISA_V4 (MCC_ISA_V3 | MCC_ISA_AVX512F)

static const struct {
	const char *name;
	uint32_t mask;
} mcc_isa_levels[] = {
		{"x86-64", MCC_ISA_V1},    {"x86-64-v2", MCC_ISA_V2},
		{"x86-64-v3", MCC_ISA_V3}, {"x86-64-v4", MCC_ISA_V4},
		{"nocona", MCC_ISA_V1},    {"core2", MCC_ISA_V1 | MCC_ISA_SSE3 | MCC_ISA_SSSE3},
		{"nehalem", MCC_ISA_V2},   {"westmere", MCC_ISA_V2},
		{"sandybridge", MCC_ISA_V2 | MCC_ISA_AVX},
		{"haswell", MCC_ISA_V3},   {"skylake", MCC_ISA_V3},
		{"x86-64-baseline", MCC_ISA_V1},
};

ST_FUNC int mcc_isa_has(MCCState *s1, uint32_t feat) { MCC_TRACE("enter\n");
	return (s1->isa_mask & feat) == feat;
}

#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_I386)
static void mcc_cpuid(uint32_t leaf, uint32_t sub, uint32_t r[4]) { MCC_TRACE("enter\n");
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__)) && \
    (!defined(__MCC__) || defined(__MCC_ASM__))
	__asm__ volatile("cpuid"
									 : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3])
									 : "a"(leaf), "c"(sub));
#else
	r[0] = r[1] = r[2] = r[3] = 0;
	(void)leaf;
	(void)sub;
#endif
}

static uint32_t mcc_isa_native(void) { MCC_TRACE("enter\n");
	uint32_t r[4], m = MCC_ISA_V1;
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
	mcc_cpuid(1, 0, r);
	if (r[2] & (1u << 0))  { MCC_TRACE("br\n"); m |= MCC_ISA_SSE3; }
	if (r[2] & (1u << 9))  { MCC_TRACE("br\n"); m |= MCC_ISA_SSSE3; }
	if (r[2] & (1u << 19)) { MCC_TRACE("br\n"); m |= MCC_ISA_SSE41; }
	if (r[2] & (1u << 20)) { MCC_TRACE("br\n"); m |= MCC_ISA_SSE42; }
	if (r[2] & (1u << 23)) { MCC_TRACE("br\n"); m |= MCC_ISA_POPCNT; }
	if (r[2] & (1u << 28)) { MCC_TRACE("br\n"); m |= MCC_ISA_AVX; }
	if (r[2] & (1u << 12)) { MCC_TRACE("br\n"); m |= MCC_ISA_FMA; }
	if (r[2] & (1u << 29)) { MCC_TRACE("br\n"); m |= MCC_ISA_F16C; }
	mcc_cpuid(7, 0, r);
	if (r[1] & (1u << 5))  { MCC_TRACE("br\n"); m |= MCC_ISA_AVX2; }
	if (r[1] & (1u << 3))  { MCC_TRACE("br\n"); m |= MCC_ISA_BMI; }
	if (r[1] & (1u << 8))  { MCC_TRACE("br\n"); m |= MCC_ISA_BMI2; }
	if (r[1] & (1u << 16)) { MCC_TRACE("br\n"); m |= MCC_ISA_AVX512F; }
	mcc_cpuid(0x80000001, 0, r);
	if (r[2] & (1u << 5))  { MCC_TRACE("br\n"); m |= MCC_ISA_LZCNT; }
#else
	(void)r;
#endif
	return m;
}
#endif

ST_FUNC void mcc_isa_init(MCCState *s1) { MCC_TRACE("enter\n");
	if (s1->isa_level)
		{ MCC_TRACE("br\n"); return; }
#if defined(MCC_TARGET_X86_64)
	s1->isa_mask = mcc_isa_native();
	s1->isa_level = "native";
#elif defined(MCC_TARGET_I386)
	s1->isa_mask = 0;
	s1->isa_level = "i686";
#elif defined(MCC_TARGET_ARM)
	s1->isa_mask = (MCC_CONFIG_CPUVER >= 5 ? MCC_ISA_ARM_BLX : 0) |
								 (MCC_CONFIG_CPUVER >= 7 ? MCC_ISA_ARM_MOVT : 0);
	s1->isa_level = MCC_CONFIG_CPUVER >= 7   ? "armv7"
									: MCC_CONFIG_CPUVER >= 5 ? "armv5"
																					 : "armv4";
#else
	s1->isa_mask = 0;
	s1->isa_level = "baseline";
#endif
}

ST_FUNC int mcc_isa_set_arch(MCCState *s1, const char *name) { MCC_TRACE("enter\n");
	size_t i;
	if (!strcmp(name, "native")) { MCC_TRACE("br\n");
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_I386)
		s1->isa_mask = mcc_isa_native();
		s1->isa_level = "native";
		return 0;
#else
		mcc_isa_init(s1);
		return 0;
#endif
	}
#if defined(MCC_TARGET_X86_64)
	for (i = 0; i < sizeof mcc_isa_levels / sizeof *mcc_isa_levels; i++) {
		MCC_TRACE("br\n");
		if (!strcmp(name, mcc_isa_levels[i].name)) { MCC_TRACE("br\n");
			s1->isa_mask = mcc_isa_levels[i].mask;
			s1->isa_level = mcc_isa_levels[i].name;
			return 0;
		}
	}
	return -1;
#else
#if defined(MCC_TARGET_ARM)
	{
		static const struct {
			const char *n;
			uint32_t m;
		} armlv[] = {
				{"armv4", 0},
				{"armv4t", 0},
				{"armv5", MCC_ISA_ARM_BLX},
				{"armv5t", MCC_ISA_ARM_BLX},
				{"armv5te", MCC_ISA_ARM_BLX},
				{"armv6", MCC_ISA_ARM_BLX},
				{"armv6t2", MCC_ISA_ARM_BLX},
				{"armv7", MCC_ISA_ARM_BLX | MCC_ISA_ARM_MOVT},
				{"armv7-a", MCC_ISA_ARM_BLX | MCC_ISA_ARM_MOVT},
				{"armv7-m", MCC_ISA_ARM_BLX | MCC_ISA_ARM_MOVT},
				{"armv7-r", MCC_ISA_ARM_BLX | MCC_ISA_ARM_MOVT},
		};
		for (i = 0; i < sizeof armlv / sizeof *armlv; i++) { MCC_TRACE("br\n");
			if (!strcmp(name, armlv[i].n)) { MCC_TRACE("br\n");
				s1->isa_mask = armlv[i].m;
				s1->isa_level = armlv[i].n;
				return 0;
			}
		}
		if (!strcmp(name, "generic")) { MCC_TRACE("br\n");
			mcc_isa_init(s1);
			s1->isa_level = "generic";
			return 0;
		}
	}
	return -1;
#else
	{
		static const char *const known[] = {
#if defined(MCC_TARGET_ARM64)
			"armv8-a", "armv8.1-a", "armv8.2-a", "armv8.3-a", "armv8.4-a",
			"armv8.5-a", "armv9-a", "generic",
#elif defined(MCC_TARGET_RISCV64)
			"rv64i", "rv64g", "rv64gc", "rv64imac", "rv64imafdc", "generic",
#elif defined(MCC_TARGET_I386)
			"i386", "i486", "i586", "i686", "pentium", "pentiumpro", "generic",
#endif
			NULL,
		};
		mcc_isa_init(s1);
		for (i = 0; known[i]; i++) { MCC_TRACE("br\n");
			if (!strcmp(name, known[i])) { MCC_TRACE("br\n");
				s1->isa_level = known[i];
				return 0;
			}
		}
	}
	return -1;
#endif
#endif
}

ST_FUNC void mcc_isa_print(MCCState *s1) { MCC_TRACE("enter\n");
	static const struct { const char *n; uint32_t b; } f[] = {
			{"sse2", MCC_ISA_SSE2},     {"sse3", MCC_ISA_SSE3},
			{"ssse3", MCC_ISA_SSSE3},   {"sse4.1", MCC_ISA_SSE41},
			{"sse4.2", MCC_ISA_SSE42},  {"popcnt", MCC_ISA_POPCNT},
			{"avx", MCC_ISA_AVX},       {"avx2", MCC_ISA_AVX2},
			{"fma", MCC_ISA_FMA},       {"bmi", MCC_ISA_BMI},
			{"bmi2", MCC_ISA_BMI2},     {"lzcnt", MCC_ISA_LZCNT},
			{"f16c", MCC_ISA_F16C},     {"avx512f", MCC_ISA_AVX512F},
			{"blx", MCC_ISA_ARM_BLX},   {"movt", MCC_ISA_ARM_MOVT},
	};
	size_t i;
	mcc_isa_init(s1);
#ifdef MCC_CONFIG_TRIPLET
	printf("target: %s\n", MCC_CONFIG_TRIPLET);
#else
	printf("target: %s\n",
#if defined(MCC_TARGET_X86_64)
				 "x86_64"
#elif defined(MCC_TARGET_I386)
				 "i386"
#elif defined(MCC_TARGET_ARM64)
				 "arm64"
#elif defined(MCC_TARGET_ARM)
				 "arm"
#elif defined(MCC_TARGET_RISCV64)
				 "riscv64"
#else
				 "unknown"
#endif
	);
#endif
	printf("level: %s\n", s1->isa_level ? s1->isa_level : "baseline");
	printf("features:");
	for (i = 0; i < sizeof f / sizeof *f; i++)
		{ MCC_TRACE("br\n"); if (s1->isa_mask & f[i].b)
			{ MCC_TRACE("br\n"); printf(" %s", f[i].n); } }
	printf("\n");
}

LIBMCCAPI MCCState *mcc_new(void) { MCC_TRACE("enter\n");
	MCCState *s;

	s = mcc_mallocz(sizeof(MCCState));
#if MCC_DIAG
	mcc_memcheck(1);
#endif

	memset(s->optflag, MCC_OPT_UNSET, sizeof s->optflag);

#undef gnu_ext
	s->gnu_ext = 1;
	s->mcc_ext = 1;
	s->nocommon = 1;
	s->dollars_in_identifiers = 1;
	s->cversion = 202311;
	s->pie = -1;
	s->pic = mcc_target_defaults.pic;
	s->warn_implicit_function_declaration = WARN_ON;
	s->warn_deprecated_declarations = WARN_ON;
	s->warn_unused_result = WARN_ON;
	s->warn_nonnull = WARN_ON;
	s->warn_address_of_packed_member = WARN_ON;
	s->warn_sentinel = WARN_ON;
	s->warn_unsupported_option = WARN_ON;
	s->warn_discarded_qualifiers = 1;
	s->warn_sequence_point = 1;
	s->warn_implicit_int = WARN_ON;
	s->warn_incompatible_pointer_types = WARN_ON;
	s->warn_int_conversion = WARN_ON;
	s->warn_excess_initializers = WARN_ON;
	s->warn_attributes = WARN_ON;
	s->warn_return_type = WARN_ON;
	s->warn_cpp = WARN_ON;
	s->warn_shift_count_negative = WARN_ON;
	s->warn_shift_count_overflow = WARN_ON;
	s->warn_div_by_zero = WARN_ON;
	s->warn_overflow = WARN_ON;
	s->warn_old_style_definition = WARN_ON;
	s->warn_undefined_internal = WARN_ON;
	s->warn_return_local_addr = WARN_ON;
	s->warn_varargs = 1;
	s->ms_extensions = 0;
	s->unwind_tables = 1;
	s->embed_jit = 0;
	s->jit = -1;
	s->jit_max_duration = 600;
	s->jit_threads = 0;
	s->jit_always_gpu = 0;
	s->jit_conservative = 0;
	s->jit_cpu_budget = -1;
	s->jit_gpu_budget = -1;
	s->jit_gpu_devices = -1;

#ifdef MCC_CHAR_IS_UNSIGNED
	s->char_is_unsigned = 1;
#endif
#ifdef MCC_TARGET_I386
	s->seg_size = 32;
#endif
#if defined MCC_TARGET_MACHO
	s->leading_underscore = 1;
#endif
#if defined MCC_TARGET_PE
	/* Every native Windows compiler (MSVC, mingw/gcc) lays out bit-fields with
	 * the MS algorithm; mcc's GCC-mode default is cross-TU-incompatible with all
	 * of them on PE. Default ms_bitfields on for PE targets so a struct declared
	 * in a Windows header matches. (T-win-50015; -mno-ms-bitfields still opts
	 * back to GCC mode.) */
	s->ms_bitfields = 1;
#endif
#ifdef MCC_ARM_HARDFLOAT
	s->float_abi = ARM_HARD_FLOAT;
#endif
	s->enable_new_dtags = MCC_CONFIG_NEW_DTAGS;
	s->ppfp = stdout;
	s->include_stack_ptr = s->include_stack;

	mcc_set_lib_path(s, MCC_MCCDIR_DEFAULT);
	if (mcc_target_defaults.switches)
		{ MCC_TRACE("br\n"); mcc_set_options(s, mcc_target_defaults.switches); }
	return s;
}

LIBMCCAPI void mcc_delete(MCCState *s1) { MCC_TRACE("enter\n");
	mcc_stats_finish();
	mccelf_delete(s1);

	dynarray_reset(&s1->library_paths, &s1->nb_library_paths);
	dynarray_reset(&s1->framework_paths, &s1->nb_framework_paths);
	dynarray_reset(&s1->crt_paths, &s1->nb_crt_paths);

	dynarray_reset(&s1->include_paths, &s1->nb_include_paths);
	dynarray_reset(&s1->sysinclude_paths, &s1->nb_sysinclude_paths);
	dynarray_reset(&s1->iquote_paths, &s1->nb_iquote_paths);
	dynarray_reset(&s1->afterinc_paths, &s1->nb_afterinc_paths);
	dynarray_reset(&s1->embed_paths, &s1->nb_embed_paths);

	mcc_free(s1->mcc_lib_path);
	mcc_free(s1->soname);
	mcc_free(s1->sysroot);
	mcc_free(s1->rpath);
	mcc_free(s1->elfint);
	mcc_free(s1->elf_entryname);
	mcc_free(s1->init_symbol);
	mcc_free(s1->fini_symbol);
	mcc_free(s1->mapfile);
	mcc_free(s1->version_script);
	mcc_free(s1->print_query);
	mcc_free(s1->outfile);
	mcc_free(s1->jit_functions);
	mcc_free(s1->deps_outfile);
	mcc_free(s1->dep_target);
#if defined MCC_TARGET_MACHO
	mcc_free(s1->install_name);
#endif
	dynarray_reset(&s1->files, &s1->nb_files);
	dynarray_reset(&s1->target_deps, &s1->nb_target_deps);
	dynarray_reset(&s1->pragma_libs, &s1->nb_pragma_libs);
	dynarray_reset(&s1->argv, &s1->argc);
	dynarray_reset(&s1->link_argv, &s1->link_argc);
	cstr_free(&s1->cmdline_defs);
	cstr_free(&s1->cmdline_incl);
	cstr_free(&s1->cmdline_imacros);
	mcc_free(s1->asm_cfi_st.buf);
	dynarray_reset(&s1->alias_fixups, &s1->nb_alias_fixups);
	mcc_free(s1->ctor_init_prio);
	s1->ctor_init_prio = NULL;
	s1->nb_ctor_init_prio = 0;
	mcc_free(s1->ctor_fini_prio);
	s1->ctor_fini_prio = NULL;
	s1->nb_ctor_fini_prio = 0;
	rir_teardown();
	ast_teardown();
	mcc_free(s1->dState);
#ifdef MCC_TARGET_IS_HOST
	mcc_run_free(s1);
#endif
	dynarray_reset(&s1->loaded_dlls, &s1->nb_loaded_dlls);
#ifdef MCC_TARGET_PE
	dynarray_reset(&s1->pe_imp_alias, &s1->nb_pe_imp_alias);
#endif
	mcc_free(s1);
#if MCC_DIAG
	mcc_memcheck(-1);
#endif
}

LIBMCCAPI int mcc_set_output_type(MCCState *s, int output_type) { MCC_TRACE("enter\n");
	if (output_type == MCC_OUTPUT_EXE) { MCC_TRACE("br\n");
		int pie = s->pie;
		if (pie < 0) { MCC_TRACE("br\n");
			pie = mcc_target_defaults.pie;
			if (s->static_link)
				{ MCC_TRACE("br\n"); pie = 0; }
		}
		if (pie)
			{ MCC_TRACE("br\n"); output_type |= MCC_OUTPUT_DYN; }
	}
	s->output_type = output_type;

	if (!s->nostdinc) { MCC_TRACE("br\n");
		mcc_add_sysinclude_path(s, mcc_target_defaults.sysincludepaths);
#if defined MCC_TARGET_MACHO && (defined MCC_TARGET_IS_HOST || MCC_HOST_DARWIN)
		mcc_add_macos_sdkincludepath(s);
		mcc_add_macos_frameworkpath(s);
#endif
	}

	if (output_type == MCC_OUTPUT_PREPROCESS) { MCC_TRACE("br\n");
		s->do_debug = 0;
		return 0;
	}

	mccelf_new(s);

	if (output_type == MCC_OUTPUT_OBJ || output_type == MCC_OUTPUT_ASM) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_PE
		if (output_type == MCC_OUTPUT_OBJ) { MCC_TRACE("br\n");
#if defined MCC_TARGET_X86_64 || defined MCC_TARGET_I386 || defined MCC_TARGET_ARM64 || defined MCC_TARGET_ARM
			if (!s->output_format_explicit)
				{ MCC_TRACE("br\n"); s->output_format = MCC_OUTPUT_FORMAT_COFF; }
			else if (s->output_format == MCC_OUTPUT_FORMAT_BINARY)
				{ MCC_TRACE("br\n"); s->output_format = MCC_OUTPUT_FORMAT_ELF; }
#else
			if (s->output_format != MCC_OUTPUT_FORMAT_COFF)
				{ MCC_TRACE("br\n"); s->output_format = MCC_OUTPUT_FORMAT_ELF; }
#endif
			return 0;
		}
#endif
		s->output_format = MCC_OUTPUT_FORMAT_ELF;
		return 0;
	}

	if (!s->nostdlib_paths)
		{ MCC_TRACE("br\n"); mcc_add_library_path(s, mcc_target_defaults.libpaths); }

#ifdef MCC_TARGET_PE
#ifdef MCC_TARGET_IS_HOST
	mcc_add_systemdir(s);
#endif

#elif defined MCC_TARGET_MACHO
#ifdef MCC_TARGET_IS_HOST
	mcc_add_macos_sdkpath(s);
#endif

#else
	mcc_split_path(s, &s->crt_paths, &s->nb_crt_paths, mcc_target_defaults.crtprefix);
	if (output_type != MCC_OUTPUT_MEMORY && !s->nostdlib)
		{ MCC_TRACE("br\n"); mccelf_add_crtbegin(s); }
#endif
	return s->nb_errors ? -1 : 0;
}

LIBMCCAPI int mcc_add_include_path(MCCState *s, const char *pathname) { MCC_TRACE("enter\n");
	mcc_split_path(s, &s->include_paths, &s->nb_include_paths, pathname);
	return 0;
}

LIBMCCAPI int mcc_add_sysinclude_path(MCCState *s, const char *pathname) { MCC_TRACE("enter\n");
	mcc_split_path(s, &s->sysinclude_paths, &s->nb_sysinclude_paths, pathname);
	return 0;
}

LIBMCCAPI int mcc_add_iquote_path(MCCState *s, const char *pathname) { MCC_TRACE("enter\n");
	mcc_split_path(s, &s->iquote_paths, &s->nb_iquote_paths, pathname);
	return 0;
}

LIBMCCAPI int mcc_add_afterinc_path(MCCState *s, const char *pathname) { MCC_TRACE("enter\n");
	mcc_split_path(s, &s->afterinc_paths, &s->nb_afterinc_paths, pathname);
	return 0;
}

LIBMCCAPI int mcc_add_embed_path(MCCState *s, const char *pathname) { MCC_TRACE("enter\n");
	mcc_split_path(s, &s->embed_paths, &s->nb_embed_paths, pathname);
	return 0;
}

LIBMCCAPI int mcc_add_library_path(MCCState *s, const char *pathname) { MCC_TRACE("enter\n");
	mcc_split_path(s, &s->library_paths, &s->nb_library_paths, pathname);
	return 0;
}

LIBMCCAPI int mcc_add_framework_path(MCCState *s, const char *pathname) { MCC_TRACE("enter\n");
	mcc_split_path(s, &s->framework_paths, &s->nb_framework_paths, pathname);
	return 0;
}

LIBMCCAPI void mcc_set_lib_path(MCCState *s, const char *path) { MCC_TRACE("enter\n");
	mcc_set_str(&s->mcc_lib_path, path);
}

ST_FUNC DLLReference *mcc_add_dllref(MCCState *s1, const char *dllname, int level) { MCC_TRACE("enter\n");
	DLLReference *ref = NULL;
	for (int i = 0; i < s1->nb_loaded_dlls; i++)
		{ MCC_TRACE("br\n"); if (0 == strcmp(s1->loaded_dlls[i]->name, dllname)) { MCC_TRACE("br\n");
			ref = s1->loaded_dlls[i];
			break;
		} }
	if (level == -1)
		{ MCC_TRACE("br\n"); return ref; }
	if (ref) { MCC_TRACE("br\n");
		if (level < ref->level)
			{ MCC_TRACE("br\n"); ref->level = level; }
		ref->found = 1;
		return ref;
	}
	ref = mcc_mallocz(sizeof(DLLReference) + strlen(dllname));
	strcpy(ref->name, dllname);
	dynarray_add(&s1->loaded_dlls, &s1->nb_loaded_dlls, ref);
	ref->level = level;
	ref->index = s1->nb_loaded_dlls;
	return ref;
}

static int mcc_add_binary(MCCState *s1, int flags, const char *filename, int fd) { MCC_TRACE("enter\n");
	ElfW(Ehdr) ehdr;
	int obj_type;
	const char *saved_filename = s1->current_filename;
	int ret = 0;

	s1->current_filename = filename;
	obj_type = mcc_object_type(fd, &ehdr);
	mcc_fd_lseek(fd, 0, SEEK_SET);

	switch (obj_type) { MCC_TRACE("br\n");
	case AFF_BINTYPE_REL:
		ret = mcc_load_object_file(s1, fd, 0);
		break;

	case AFF_BINTYPE_AR:
		ret = mcc_load_archive(s1, fd, !(flags & AFF_WHOLE_ARCHIVE));
		break;

#if defined MCC_TARGET_UNIX
	case AFF_BINTYPE_DYN:
		if (s1->output_type == MCC_OUTPUT_MEMORY) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_IS_HOST
#ifdef MCC_CONFIG_STATIC
			(void)filename;
#else
			void *dl = host_dlopen(filename);
			if (dl)
				{ MCC_TRACE("br\n"); mcc_add_dllref(s1, filename, 0)->handle = dl; }
			else
				{ MCC_TRACE("br\n"); ret = FILE_NOT_RECOGNIZED; }
#endif
#endif
		} else
			{ MCC_TRACE("br\n"); ret = mcc_load_dll(s1, fd, filename, (flags & AFF_REFERENCED_DLL) != 0); }
		break;

	default:
		ret = mcc_load_ldscript(s1, fd);
		break;

#elif defined MCC_TARGET_MACHO
	case AFF_BINTYPE_DYN:
	case_dyn_or_tbd:
		if (s1->output_type == MCC_OUTPUT_MEMORY) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_IS_HOST
			void *dl;
			const char *soname = filename;
			char *tmp = 0;
			if (obj_type != AFF_BINTYPE_DYN) { MCC_TRACE("br\n");
				tmp = macho_tbd_soname(fd);
				if (tmp)
					{ MCC_TRACE("br\n"); soname = tmp; }
			}
			dl = host_dlopen(soname);
			if (dl)
				{ MCC_TRACE("br\n"); mcc_add_dllref(s1, soname, 0)->handle = dl; }
			else
				{ MCC_TRACE("br\n"); ret = FILE_NOT_RECOGNIZED; }
			mcc_free(tmp);
#endif
		} else if (obj_type == AFF_BINTYPE_DYN) { MCC_TRACE("br\n");
			ret = macho_load_dll(s1, fd, filename, (flags & AFF_REFERENCED_DLL) != 0);
		} else { MCC_TRACE("br\n");
			ret = macho_load_tbd(s1, fd, filename, (flags & AFF_REFERENCED_DLL) != 0);
		}
		if (ret)
			{ MCC_TRACE("br\n"); ret = FILE_NOT_RECOGNIZED; }
		break;

	default: {
		const char *ext = mcc_fileextension(filename);
		if (!strcmp(ext, ".tbd"))
			{ MCC_TRACE("br\n"); goto case_dyn_or_tbd; }
		if (!strcmp(ext, ".dylib")) { MCC_TRACE("br\n");
			obj_type = AFF_BINTYPE_DYN;
			goto case_dyn_or_tbd;
		}
		if (macho_object_type(fd, 0)) { MCC_TRACE("br\n");
			ret = macho_load_object_file(s1, fd, 0);
			break;
		}
		ret = FILE_NOT_RECOGNIZED;
		break;
	}

#elif defined MCC_TARGET_PE
	default:
		if (coff_object_type(fd, 0)) { MCC_TRACE("br\n");
			ret = coff_load_object_file(s1, fd, 0);
			break;
		}
		if (pe_load_file(s1, fd, filename))
			{ MCC_TRACE("br\n"); ret = FILE_NOT_RECOGNIZED; }
		break;
#endif
	}

	mcc_fd_close(fd);
	s1->current_filename = saved_filename;
	if (ret == FILE_NOT_RECOGNIZED)
		{ MCC_TRACE("br\n"); return mcc_error_noabort("%s: unrecognized file type", filename); }
	return ret;
}

#if defined MCC_TARGETOS_OpenBSD && MCC_HOST_POSIX
#include <glob.h>
static int mcc_glob_so(MCCState *s1, const char *pattern, char *buf, int size) { MCC_TRACE("enter\n");
	const char *star;
	glob_t g;
	char *p;
	int i, v, v1, v2, v3;

	star = strchr(pattern, '*');
	if (!star || glob(pattern, 0, NULL, &g))
		{ MCC_TRACE("br\n"); return -1; }
	for (v = -1, i = 0; i < g.gl_pathc; ++i) { MCC_TRACE("br\n");
		p = g.gl_pathv[i];
		if (2 != sscanf(p + (star - pattern), "%d.%d.%d", &v1, &v2, &v3))
			{ MCC_TRACE("br\n"); continue; }
		if ((v1 = v1 * 1000 + v2) > v)
			{ MCC_TRACE("br\n"); v = v1, pstrcpy(buf, size, p); }
	}
	globfree(&g);
	return v;
}
#endif

static int guess_filetype(const char *filename) { MCC_TRACE("enter\n");
	int filetype = 0;
	if (1) { MCC_TRACE("br\n");
		const char *ext = mcc_fileextension(filename);
		if (ext[0]) { MCC_TRACE("br\n");
			ext++;
			if (!strcmp(ext, "S"))
				{ MCC_TRACE("br\n"); filetype = AFF_TYPE_ASMPP; }
			else if (!strcmp(ext, "s"))
				{ MCC_TRACE("br\n"); filetype = AFF_TYPE_ASM; }
			else if (!HOST_PATHCMP(ext, "c") || !HOST_PATHCMP(ext, "h") || !HOST_PATHCMP(ext, "i"))
				{ MCC_TRACE("br\n"); filetype = AFF_TYPE_C; }
			else
				{ MCC_TRACE("br\n"); filetype |= AFF_TYPE_BIN; }
		} else { MCC_TRACE("br\n");
			filetype = AFF_TYPE_C;
		}
	}
	return filetype;
}

ST_FUNC int mcc_add_file_internal(MCCState *s1, const char *filename, int flags) { MCC_TRACE("enter\n");
	int fd;

#if defined MCC_TARGETOS_OpenBSD && MCC_HOST_POSIX
	char buf[1024];
	if (mcc_glob_so(s1, filename, buf, sizeof buf) >= 0)
		{ MCC_TRACE("br\n"); filename = buf; }
#endif

	if (0 == (flags & AFF_TYPE_MASK))
		{ MCC_TRACE("br\n"); flags |= guess_filetype(filename); }

	if (s1->output_type == MCC_OUTPUT_PREPROCESS && (flags & AFF_TYPE_BIN))
		{ MCC_TRACE("br\n"); return 0; }

	fd = _mcc_open(s1, filename, (flags & AFF_TYPE_BIN) != 0);
	if (fd < 0) { MCC_TRACE("br\n");
		if (flags & AFF_PRINT_ERROR)
			{ MCC_TRACE("br\n"); mcc_error_noabort("file '%s' not found", filename); }
		return FILE_NOT_FOUND;
	}
	if (!(flags & AFF_TYPE_BIN) && strcmp(filename, "-") == 0)
		{ MCC_TRACE("br\n"); filename = "<stdin>"; }

	if (flags & AFF_TYPE_BIN)
		{ MCC_TRACE("br\n"); return mcc_add_binary(s1, flags, filename, fd); }

	dynarray_add(&s1->target_deps, &s1->nb_target_deps, mcc_strdup(filename));
	return mcc_compile(s1, flags, filename, fd);
}

LIBMCCAPI int mcc_add_file(MCCState *s, const char *filename) { MCC_TRACE("enter\n");
	return mcc_add_file_internal(s, filename, s->filetype | AFF_PRINT_ERROR);
}

static int mcc_add_library_internal(MCCState *s1, const char *fmt,
																		const char *filename, int flags, char **paths, int nb_paths) { MCC_TRACE("enter\n");
	char buf[1024];
	int ret;

	for (int i = 0; i < nb_paths; i++) { MCC_TRACE("br\n");
		snprintf(buf, sizeof(buf), fmt, paths[i], filename);
		ret = mcc_add_file_internal(s1, buf, flags & ~AFF_PRINT_ERROR);
		if (ret != FILE_NOT_FOUND)
			{ MCC_TRACE("br\n"); return ret; }
	}
	if (flags & AFF_PRINT_ERROR)
		{ MCC_TRACE("br\n"); mcc_error_noabort("%s '%s' not found",
											flags & AFF_TYPE_LIB ? "library" : "file", filename); }
	return FILE_NOT_FOUND;
}

ST_FUNC int mcc_add_dll(MCCState *s, const char *filename, int flags) { MCC_TRACE("enter\n");
	return mcc_add_library_internal(s, "%s/%s", filename, flags,
																	s->library_paths, s->nb_library_paths);
}

#if defined MCC_TARGET_X86_64
#define MCC_SUPPORT_ARCH "x86_64"
#elif defined MCC_TARGET_I386
#define MCC_SUPPORT_ARCH "i386"
#elif defined MCC_TARGET_ARM64
#define MCC_SUPPORT_ARCH "arm64"
#elif defined MCC_TARGET_ARM
#define MCC_SUPPORT_ARCH "arm"
#elif defined MCC_TARGET_RISCV64
#define MCC_SUPPORT_ARCH "riscv64"
#else
#define MCC_SUPPORT_ARCH ""
#endif
#if defined MCC_TARGET_PE
#define MCC_SUPPORT_OS "-win32"
#elif defined MCC_TARGET_MACHO
#define MCC_SUPPORT_OS "-osx"
#else
#define MCC_SUPPORT_OS ""
#endif

ST_FUNC int mcc_add_support_opt(MCCState *s1, const char *filename) { MCC_TRACE("enter\n");
	char arch[128];
	if (mcc_support_arch_match(s1, filename) == 1)
		{ MCC_TRACE("br\n"); return mcc_add_dll(s1, filename, 0); }
	if (MCC_SUPPORT_ARCH[0] &&
			(size_t)snprintf(arch, sizeof arch, "%s%s-%s", MCC_SUPPORT_ARCH,
											 MCC_SUPPORT_OS, filename) < sizeof arch &&
			mcc_support_arch_match(s1, arch) == 1)
		{ MCC_TRACE("br\n"); return mcc_add_dll(s1, arch, 0); }
	return 0;
}

ST_FUNC int mcc_add_support(MCCState *s1, const char *filename) { MCC_TRACE("enter\n");
	char plain[128], arch[128];
	int arch_ok;

	if (MCC_CONFIG_CROSSPREFIX[0])
		{ MCC_TRACE("br\n"); strcat(strcpy(plain, MCC_CONFIG_CROSSPREFIX), filename); }
	else
		{ MCC_TRACE("br\n"); snprintf(plain, sizeof plain, "%s", filename); }
	arch_ok = MCC_SUPPORT_ARCH[0] &&
			(size_t)snprintf(arch, sizeof arch, "%s%s-%s", MCC_SUPPORT_ARCH,
											 MCC_SUPPORT_OS, filename) < sizeof arch;

	if (mcc_support_arch_match(s1, plain) == 1) { MCC_TRACE("br\n");
		int ret = mcc_add_dll(s1, plain, AFF_PRINT_ERROR);
		size_t fl = strlen(filename);
		if (arch_ok && fl >= 2 && !strcmp(filename + fl - 2, ".a"))
			{ MCC_TRACE("br\n"); mcc_add_dll(s1, arch, 0); }
		return ret;
	}

	if (arch_ok && mcc_add_dll(s1, arch, 0) == 0)
		{ MCC_TRACE("br\n"); return 0; }
	return mcc_add_dll(s1, plain, AFF_PRINT_ERROR);
}

#ifdef MCC_EMBED_MCCRT
extern const unsigned char mccrt_blob[];
extern const unsigned int mccrt_blob_len;

ST_FUNC int mcc_add_mccrt_embedded(MCCState *s1) { MCC_TRACE("enter\n");
	char tmp[] = "/tmp/.mccrtXXXXXX";
	size_t off;
	int fd, ret;

	fd = mkstemp(tmp);
	if (fd < 0)
		{ MCC_TRACE("br\n"); return mcc_error_noabort("embedded " MCC_MCCRT ": cannot create temp fd"); }
	unlink(tmp);
	for (off = 0; off < mccrt_blob_len;) { MCC_TRACE("br\n");
		ssize_t w = write(fd, mccrt_blob + off, mccrt_blob_len - off);
		if (w <= 0) { MCC_TRACE("br\n");
			close(fd);
			return mcc_error_noabort("embedded " MCC_MCCRT ": write failed");
		}
		off += (size_t)w;
	}
	lseek(fd, 0, SEEK_SET);
	ret = mcc_add_binary(s1, AFF_PRINT_ERROR, "<embedded " MCC_MCCRT ">", fd);
	return ret;
}
#endif

#ifdef MCC_EMBED_JIT_BLOB
extern const unsigned char mccjit_blob[];
extern const unsigned int mccjit_blob_len;

#if defined MCC_TARGET_X86_64
#define MCC_EMBED_RT_ARCH "x86_64"
#elif defined MCC_TARGET_I386
#define MCC_EMBED_RT_ARCH "i386"
#elif defined MCC_TARGET_ARM64
#define MCC_EMBED_RT_ARCH "aarch64"
#elif defined MCC_TARGET_ARM
#define MCC_EMBED_RT_ARCH "arm"
#else
#define MCC_EMBED_RT_ARCH "x86_64"
#endif

ST_FUNC int mcc_add_jit_engine_embedded(MCCState *s1) { MCC_TRACE("enter\n");
	char tmp[1024];
	size_t off;
	int fd, ret;

	fd = host_temp_file(tmp, sizeof tmp);
	if (fd < 0)
		{ MCC_TRACE("br\n"); return mcc_error_noabort("embedded jit engine: cannot create temp fd"); }
	for (off = 0; off < mccjit_blob_len;) { MCC_TRACE("br\n");
		ssize_t w = write(fd, mccjit_blob + off, mccjit_blob_len - off);
		if (w <= 0) { MCC_TRACE("br\n");
			close(fd);
			unlink(tmp);
			return mcc_error_noabort("embedded jit engine: write failed");
		}
		off += (size_t)w;
	}
	lseek(fd, 0, SEEK_SET);
	ret = mcc_add_binary(s1, AFF_PRINT_ERROR | AFF_WHOLE_ARCHIVE,
											 "<embedded jit engine>", fd);
	unlink(tmp);
#if defined MCC_HOST_WIN32 && defined MCC_EMBED_JIT_MINGW_LIBDIR
	if (ret == 0) { MCC_TRACE("br\n");
		int saved = s1->filetype;
		s1->filetype &= ~AFF_WHOLE_ARCHIVE;
#ifdef MCC_EMBED_JIT_MCC_ENGINE
		mcc_add_support(s1, "libmccrt.a");
		mcc_add_library(s1, "msvcrt");
		mcc_add_library(s1, "kernel32");
#else
		mcc_add_library_path(s1, MCC_EMBED_JIT_MINGW_LIBDIR);
#ifdef MCC_EMBED_JIT_GCC_LIBDIR
		mcc_add_library_path(s1, MCC_EMBED_JIT_GCC_LIBDIR);
#endif
		mcc_add_library(s1, "mingwex");
		mcc_add_library(s1, "mingw32");
#ifdef MCC_EMBED_JIT_GCC_LIBDIR
		{
			char lp[1024];
			FILE *lf;
			snprintf(lp, sizeof lp, "%s/libgcc.a", MCC_EMBED_JIT_GCC_LIBDIR);
			if ((lf = fopen(lp, "rb"))) { MCC_TRACE("br\n");
				fclose(lf);
				mcc_add_library(s1, "gcc");
				snprintf(lp, sizeof lp, "%s/libgcc_eh.a", MCC_EMBED_JIT_GCC_LIBDIR);
				if ((lf = fopen(lp, "rb"))) { MCC_TRACE("br\n");
					fclose(lf);
					mcc_add_library(s1, "gcc_eh");
				}
			} else { MCC_TRACE("br\n");
				snprintf(lp, sizeof lp,
								 "%s/libclang_rt.builtins-" MCC_EMBED_RT_ARCH ".a",
								 MCC_EMBED_JIT_GCC_LIBDIR);
				if ((lf = fopen(lp, "rb"))) { MCC_TRACE("br\n");
					fclose(lf);
					mcc_add_library(s1, "clang_rt.builtins-" MCC_EMBED_RT_ARCH);
				} else { MCC_TRACE("br\n");
					mcc_add_library(s1, "gcc");
				}
			}
		}
#else
		mcc_add_library(s1, "gcc");
#endif
		{
			char lp[1024];
			FILE *lf;
			snprintf(lp, sizeof lp, "%s/libwinpthread.a", MCC_EMBED_JIT_MINGW_LIBDIR);
			if ((lf = fopen(lp, "rb"))) { MCC_TRACE("br\n");
				fclose(lf);
				mcc_add_library(s1, "winpthread");
			}
		}
		mcc_add_library(s1, "mingw32");
		mcc_add_library(s1, "mingwex");
		mcc_add_library(s1, "ucrt");
		mcc_add_library(s1, "kernel32");
#ifdef MCC_TARGET_I386
		{
			char lp[1024];
			FILE *lf;
			snprintf(lp, sizeof lp, "%s/libkernel32.a", MCC_EMBED_JIT_MINGW_LIBDIR);
			if ((lf = fopen(lp, "rb"))) { MCC_TRACE("br\n");
				fclose(lf);
				mcc_add_file(s1, lp);
			}
		}
#endif
#endif
		s1->filetype = saved;
	}
#endif
	return ret;
}
#endif

#ifdef MCC_TARGET_UNIX
ST_FUNC int mcc_add_crt(MCCState *s1, const char *filename) { MCC_TRACE("enter\n");
	int ret = mcc_add_library_internal(s1, "%s/%s",
																		 filename, 0, s1->crt_paths, s1->nb_crt_paths);
	if (ret == FILE_NOT_FOUND)
		{ MCC_TRACE("br\n"); ret = mcc_add_library_internal(s1, "%s/%s",
																	 filename, AFF_PRINT_ERROR, s1->library_paths, s1->nb_library_paths); }
	return ret;
}
#endif

LIBMCCAPI int mcc_add_library(MCCState *s, const char *libraryname) { MCC_TRACE("enter\n");
	static const char *const libs[] = {
#if defined MCC_TARGET_PE
			"%s/%s.def", "%s/lib%s.def", "%s/%s.dll", "%s/lib%s.dll",
#elif defined MCC_TARGET_MACHO
			"%s/lib%s.dylib", "%s/lib%s.tbd",
#elif defined MCC_TARGETOS_OpenBSD
			"%s/lib%s.so.*",
#else
			"%s/lib%s.so",
#endif
			"%s/lib%s.a",
			NULL};
	int flags = AFF_TYPE_LIB | (s->filetype & AFF_WHOLE_ARCHIVE);
#if defined MCC_TARGET_PE
	if (!strcmp(libraryname, "m"))
		{ MCC_TRACE("br\n"); return 0; }
#endif
	if (*libraryname == ':') { MCC_TRACE("br\n");
		libraryname++;
	} else { MCC_TRACE("br\n");
		const char *const *pp = libs;
		if (s->static_link)
			{ MCC_TRACE("br\n"); pp += sizeof(libs) / sizeof(*libs) - 2; }
		while (*pp) { MCC_TRACE("br\n");
			int ret = mcc_add_library_internal(s, *pp,
																				 libraryname, flags, s->library_paths, s->nb_library_paths);
			if (ret != FILE_NOT_FOUND)
				{ MCC_TRACE("br\n"); return ret; }
			++pp;
		}
	}
	return mcc_add_dll(s, libraryname, flags | AFF_PRINT_ERROR);
}

LIBMCCAPI int mcc_add_framework(MCCState *s1, const char *name) { MCC_TRACE("enter\n");
#ifdef MCC_TARGET_MACHO
	static const char *const pat[] = {
			"%s/%s.framework/%s.tbd",
			"%s/%s.framework/%s",
			NULL};
	char buf[1024];
	int whole = s1->filetype & AFF_WHOLE_ARCHIVE;

	for (int i = 0; i < s1->nb_framework_paths; i++) { MCC_TRACE("br\n");
		for (const char *const *pp = pat; *pp; ++pp) { MCC_TRACE("br\n");
			int ret;
			snprintf(buf, sizeof(buf), *pp, s1->framework_paths[i], name, name);
			ret = mcc_add_file_internal(s1, buf, AFF_TYPE_BIN | whole);
			if (ret != FILE_NOT_FOUND)
				{ MCC_TRACE("br\n"); return ret; }
		}
	}
	return mcc_error_noabort("framework '%s' not found", name);
#else
	return mcc_error_noabort("-framework '%s' is only supported on macOS targets", name);
#endif
}

ST_FUNC void mcc_add_pragma_libs(MCCState *s1) { MCC_TRACE("enter\n");
	for (int i = 0; i < s1->nb_pragma_libs; i++)
		{ MCC_TRACE("br\n"); mcc_add_library(s1, s1->pragma_libs[i]); }
}

static int strstart(const char *val, const char **str) { MCC_TRACE("enter\n");
	const char *p, *q;
	p = *str;
	q = val;
	while (*q) { MCC_TRACE("br\n");
		if (*p != *q)
			{ MCC_TRACE("br\n"); return 0; }
		p++;
		q++;
	}
	*str = p;
	return 1;
}

struct lopt {
	MCCState *s;
	const char *opt, *arg;
	int match;
};

static int link_option(struct lopt *o, const char *q) { MCC_TRACE("enter\n");
	const char *p;
	int c;
redo:
	p = o->opt;
	if (*p++ != '-')
		{ MCC_TRACE("br\n"); return 0; }
	if (*p == '-')
		{ MCC_TRACE("br\n"); p++; }
	while ((c = *q) == *p) { MCC_TRACE("br\n");
		if (c == '\0')
			{ MCC_TRACE("br\n"); goto succ; }
		++p;
		if (c == '=')
			{ MCC_TRACE("br\n"); goto succ; }
		++q;
	}
	if (*p == '\0') { MCC_TRACE("br\n");
		if (c == '|')
			{ MCC_TRACE("br\n"); goto succ; }
		if (c == '=' || c == ':') { MCC_TRACE("br\n");
			if (o->s->link_optind + 1 < o->s->link_argc) { MCC_TRACE("br\n");
				p = o->s->link_argv[++o->s->link_optind];
				goto succ;
			}
			o->match = 1;
			return 0;
		}
	} else if (c == ':')
		{ MCC_TRACE("br\n"); goto succ; }
	while (*q)
		{ MCC_TRACE("br\n"); if (*q++ == '|')
			{ MCC_TRACE("br\n"); goto redo; } }
	return 0;
succ:
	o->arg = p;
	return 1;
}

#if defined MCC_TARGET_MACHO
static uint32_t parse_version(MCCState *s1, const char *version);
#endif

static void args_parser_add_file(MCCState *s, const char *filename, int filetype);

#ifdef MCC_TARGET_PE
static void mcc_pe_set_dll_characteristics(MCCState *s, unsigned flags) { MCC_TRACE("enter\n");
	s->pe_dll_characteristics |= flags;
	s->pe_dll_characteristics_clear &= ~flags;
}

static void mcc_pe_clear_dll_characteristics(MCCState *s, unsigned flags) { MCC_TRACE("enter\n");
	s->pe_dll_characteristics &= ~flags;
	s->pe_dll_characteristics_clear |= flags;
}
#endif

static int mcc_set_linker(MCCState *s, const char *optarg) { MCC_TRACE("enter\n");
	MCCState *s1 = s;

	dynarray_split(&s1->link_argv, &s1->link_argc, optarg, ',');

	while (s->link_optind < s->link_argc) { MCC_TRACE("br\n");
		char *end = NULL;
		int ignoring = 0;
		struct lopt o = {0};
		o.s = s;
		o.opt = s->link_argv[s->link_optind];

		if (link_option(&o, "Bsymbolic")) { MCC_TRACE("br\n");
			s->symbolic = 1;
		} else if (link_option(&o, "nostdlib")) { MCC_TRACE("br\n");
			s->nostdlib_paths = 1;
		} else if (link_option(&o, "e=|entry=")) { MCC_TRACE("br\n");
			mcc_set_str(&s->elf_entryname, o.arg);
		} else if (link_option(&o, "image-base=|Ttext=")) { MCC_TRACE("br\n");
			s->text_addr = strtoull(o.arg, &end, 16);
			s->has_text_addr = 1;
		} else if (link_option(&o, "init=")) { MCC_TRACE("br\n");
			mcc_set_str(&s->init_symbol, o.arg);
			ignoring = 1;
		} else if (link_option(&o, "fini=")) { MCC_TRACE("br\n");
			mcc_set_str(&s->fini_symbol, o.arg);
			ignoring = 1;
		} else if (link_option(&o, "Map=")) { MCC_TRACE("br\n");
			mcc_set_str(&s->mapfile, o.arg);
			ignoring = 1;
		} else if (link_option(&o, "oformat=")) { MCC_TRACE("br\n");
			s->output_format_explicit = 1;
#if defined MCC_TARGET_PE
			if (0 == strncmp("pe-", o.arg, 3))
#elif MCC_PTR_SIZE == 8
			if (0 == strncmp("elf64-", o.arg, 6))
#else
			if (0 == strncmp("elf32-", o.arg, 6))
#endif
				s->output_format = MCC_OUTPUT_FORMAT_ELF;
			else if (0 == strcmp("binary", o.arg))
				{ MCC_TRACE("br\n"); s->output_format = MCC_OUTPUT_FORMAT_BINARY; }
#if defined MCC_TARGET_PE
			else if (0 == strcmp("coff", o.arg))
				{ MCC_TRACE("br\n"); s->output_format = MCC_OUTPUT_FORMAT_COFF; }
#endif
			else
				{ MCC_TRACE("br\n"); goto err; }
		} else if (link_option(&o, "export-all-symbols|export-dynamic|E")) { MCC_TRACE("br\n");
			s->rdynamic = 1;
		} else if (link_option(&o, "rpath=")) { MCC_TRACE("br\n");
			mcc_concat_str(&s->rpath, o.arg, ':');
		} else if (link_option(&o, "dynamic-linker=|I:")) { MCC_TRACE("br\n");
			mcc_set_str(&s->elfint, o.arg);
		} else if (link_option(&o, "enable-new-dtags")) { MCC_TRACE("br\n");
			s->enable_new_dtags = 1;
		} else if (link_option(&o, "disable-new-dtags")) { MCC_TRACE("br\n");
			s->enable_new_dtags = 0;
		} else if (link_option(&o, "section-alignment=")) { MCC_TRACE("br\n");
			s->section_align = strtoul(o.arg, &end, 16);
		} else if (link_option(&o, "soname=|install_name=")) { MCC_TRACE("br\n");
			mcc_set_str(&s->soname, o.arg);
		} else if (link_option(&o, "whole-archive")) { MCC_TRACE("br\n");
			s->filetype |= AFF_WHOLE_ARCHIVE;
		} else if (link_option(&o, "no-whole-archive")) { MCC_TRACE("br\n");
			s->filetype &= ~AFF_WHOLE_ARCHIVE;
		} else if (link_option(&o, "start-group|(")) { MCC_TRACE("br\n");
			s->link_in_group = 1;
		} else if (link_option(&o, "end-group|)")) { MCC_TRACE("br\n");
			s->link_in_group = 0;
		} else if (link_option(&o, "version-script=|version_script=")) { MCC_TRACE("br\n");
			mcc_set_str(&s->version_script, o.arg);
			ignoring = 1;
#if defined MCC_TARGET_MACHO
		} else if (link_option(&o, "current_version=")) { MCC_TRACE("br\n");
			s->current_version = parse_version(s, o.arg);
		} else if (link_option(&o, "compatibility_version=")) { MCC_TRACE("br\n");
			s->compatibility_version = parse_version(s, o.arg);
#endif
		} else if (link_option(&o, "znodelete")) { MCC_TRACE("br\n");
			s->znodelete = 1;
#ifdef MCC_TARGET_PE
		} else if (link_option(&o, "large-address-aware")) { MCC_TRACE("br\n");
			s->pe_characteristics |= PE_IMAGE_FILE_LARGE_ADDRESS_AWARE;
		} else if (link_option(&o, "dynamicbase")) { MCC_TRACE("br\n");
			mcc_pe_set_dll_characteristics(s, PE_DLLCHARACTERISTICS_DYNAMIC_BASE);
		} else if (link_option(&o, "disable-dynamicbase|no-dynamicbase")) { MCC_TRACE("br\n");
			mcc_pe_clear_dll_characteristics(s,
																			 PE_DLLCHARACTERISTICS_DYNAMIC_BASE |
																					 PE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA);
		} else if (link_option(&o, "nxcompat")) { MCC_TRACE("br\n");
			mcc_pe_set_dll_characteristics(s, PE_DLLCHARACTERISTICS_NX_COMPAT);
		} else if (link_option(&o, "disable-nxcompat|no-nxcompat")) { MCC_TRACE("br\n");
			mcc_pe_clear_dll_characteristics(s, PE_DLLCHARACTERISTICS_NX_COMPAT);
		} else if (link_option(&o, "high-entropy-va")) { MCC_TRACE("br\n");
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
			mcc_pe_set_dll_characteristics(s,
																		 PE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA |
																				 PE_DLLCHARACTERISTICS_DYNAMIC_BASE);
#else
			goto err;
#endif
		} else if (link_option(&o, "disable-high-entropy-va|no-high-entropy-va")) { MCC_TRACE("br\n");
			mcc_pe_clear_dll_characteristics(s, PE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA);
		} else if (link_option(&o, "tsaware")) { MCC_TRACE("br\n");
			mcc_pe_set_dll_characteristics(s, PE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE);
		} else if (link_option(&o, "disable-tsaware|no-tsaware")) { MCC_TRACE("br\n");
			mcc_pe_clear_dll_characteristics(s, PE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE);
		} else if (link_option(&o, "file-alignment=")) { MCC_TRACE("br\n");
			s->pe_file_align = strtoul(o.arg, &end, 16);
		} else if (link_option(&o, "stack=")) { MCC_TRACE("br\n");
			s->pe_stack_size = strtoul(o.arg, &end, 10);
		} else if (link_option(&o, "subsystem=")) { MCC_TRACE("br\n");
			if (pe_setsubsy(s, o.arg) < 0)
				{ MCC_TRACE("br\n"); goto err; }
#elif defined MCC_TARGET_MACHO
		} else if (link_option(&o, "all_load")) { MCC_TRACE("br\n");
			s->filetype |= AFF_WHOLE_ARCHIVE;
		} else if (link_option(&o, "force_load=")) { MCC_TRACE("br\n");
			args_parser_add_file(s, o.arg, AFF_TYPE_LIB | AFF_WHOLE_ARCHIVE);
		} else if (link_option(&o, "single_module")) { MCC_TRACE("br\n");
			ignoring = 1;
		} else if (link_option(&o, "search_paths_first")) { MCC_TRACE("br\n");
		} else if (link_option(&o, "headerpad_max_install_names")) { MCC_TRACE("br\n");
#endif
		} else if (link_option(&o, "x")) { MCC_TRACE("br\n");
		} else if (link_option(&o, "as-needed")) { MCC_TRACE("br\n");
			ignoring = 1;
		} else if (link_option(&o, "O")) { MCC_TRACE("br\n");
			ignoring = 1;
		} else if (link_option(&o, "z=")) { MCC_TRACE("br\n");
			ignoring = 1;
		} else if (link_option(&o, "L:")) { MCC_TRACE("br\n");
			mcc_add_library_path(s, o.arg);
		} else if (link_option(&o, "l:")) { MCC_TRACE("br\n");
			args_parser_add_file(s, o.arg, AFF_TYPE_LIB | (s->filetype & ~AFF_TYPE_MASK));
		} else if (o.match) { MCC_TRACE("br\n");
			return 0;
		} else { MCC_TRACE("br\n");
		err:
			return mcc_error_noabort("unsupported linker option '%s'", o.opt);
		}
		if (ignoring)
			{ MCC_TRACE("br\n"); mcc_warning_c(warn_unsupported)("unsupported linker option '%s'", o.opt); }
		++s->link_optind;
	}
	return 0;
}

typedef struct MCCOption {
	const char *name;
	uint16_t index;
	uint16_t flags;
} MCCOption;

enum {
	MCC_OPTION_ignored = 0,
	MCC_OPTION_HELP,
	MCC_OPTION_HELP2,
	MCC_OPTION_v,
	MCC_OPTION_I,
	MCC_OPTION_D,
	MCC_OPTION_U,
	MCC_OPTION_P,
	MCC_OPTION_L,
	MCC_OPTION_B,
	MCC_OPTION_l,
	MCC_OPTION_bench,
	MCC_OPTION_bt,
	MCC_OPTION_b,
	MCC_OPTION_g,
	MCC_OPTION_embed_jit,
	MCC_OPTION_no_embed_jit,
	MCC_OPTION_jit,
	MCC_OPTION_no_jit,
	MCC_OPTION_jit_max_duration,
	MCC_OPTION_jit_threads,
	MCC_OPTION_jit_functions,
	MCC_OPTION_jit_always_gpu,
	MCC_OPTION_no_jit_always_gpu,
	MCC_OPTION_jit_conservative,
	MCC_OPTION_jit_cpu_budget,
	MCC_OPTION_jit_gpu_budget,
	MCC_OPTION_jit_gpu_devices,
	MCC_OPTION_clear_cache,
	MCC_OPTION_stats,
	MCC_OPTION_c,
	MCC_OPTION_dumpmachine,
	MCC_OPTION_dumpversion,
	MCC_OPTION_print_multiarch,
	MCC_OPTION_d,
	MCC_OPTION_debug,
	MCC_OPTION_static,
	MCC_OPTION_std,
	MCC_OPTION_ansi,
	MCC_OPTION_shared,
	MCC_OPTION_bundle,
	MCC_OPTION_soname,
	MCC_OPTION_o,
	MCC_OPTION_r,
	MCC_OPTION_Wl,
	MCC_OPTION_Wp,
	MCC_OPTION_Wa,
	MCC_OPTION_Xpreprocessor,
	MCC_OPTION_Xassembler,
	MCC_OPTION_Xlinker,
	MCC_OPTION_save_temps,
	MCC_OPTION_W,
	MCC_OPTION_O,
	MCC_OPTION_mfloat_abi,
	MCC_OPTION_m,
	MCC_OPTION_f,
	MCC_OPTION_pg,
	MCC_OPTION_isystem,
	MCC_OPTION_sysroot,
	MCC_OPTION_isysroot,
	MCC_OPTION_iwithprefix,
	MCC_OPTION_iquote,
	MCC_OPTION_idirafter,
	MCC_OPTION_imacros,
	MCC_OPTION_include,
	MCC_OPTION_nostdinc,
	MCC_OPTION_trigraphs,
	MCC_OPTION_nostdlib,
	MCC_OPTION_print_search_dirs,
	MCC_OPTION_print_isa,
	MCC_OPTION_print_prog_name,
	MCC_OPTION_print_file_name,
	MCC_OPTION_rdynamic,
	MCC_OPTION_pthread,
	MCC_OPTION_run,
	MCC_OPTION_rstdin,
	MCC_OPTION_w,
	MCC_OPTION_E,
	MCC_OPTION_M,
	MCC_OPTION_MD,
	MCC_OPTION_MF,
	MCC_OPTION_MM,
	MCC_OPTION_MMD,
	MCC_OPTION_MG,
	MCC_OPTION_MP,
	MCC_OPTION_MT,
	MCC_OPTION_MQ,
	MCC_OPTION_S,
	MCC_OPTION_x,
	MCC_OPTION_C,
	MCC_OPTION_ar,
	MCC_OPTION_impdef,
	MCC_OPTION_pie,
	MCC_OPTION_nopie,
	MCC_OPTION_pedantic,
	MCC_OPTION_pedantic_errors,
	MCC_OPTION_s,
	MCC_OPTION_dynamiclib,
	MCC_OPTION_flat_namespace,
	MCC_OPTION_two_levelnamespace,
	MCC_OPTION_undefined,
	MCC_OPTION_install_name,
	MCC_OPTION_compatibility_version,
	MCC_OPTION_current_version,
	MCC_OPTION_mmacosx_version_min,
	MCC_OPTION_framework,
	MCC_OPTION_F,
	MCC_OPTION_lsp,
	MCC_OPTION_embed_dir,
};

#define MCC_OPTION_HAS_ARG 0x0001
#define MCC_OPTION_NOSEP 0x0002

static const MCCOption mcc_options[] = {
		{"h", MCC_OPTION_HELP, 0},
		{"-help", MCC_OPTION_HELP, 0},
		{"?", MCC_OPTION_HELP, 0},
		{"hh", MCC_OPTION_HELP2, 0},
		{"v", MCC_OPTION_v, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"-version", MCC_OPTION_v, 0},
		{"-lsp", MCC_OPTION_lsp, 0},
		{"I", MCC_OPTION_I, MCC_OPTION_HAS_ARG},
		{"D", MCC_OPTION_D, MCC_OPTION_HAS_ARG},
		{"U", MCC_OPTION_U, MCC_OPTION_HAS_ARG},
		{"P", MCC_OPTION_P, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"L", MCC_OPTION_L, MCC_OPTION_HAS_ARG},
		{"B", MCC_OPTION_B, MCC_OPTION_HAS_ARG},
		{"l", MCC_OPTION_l, MCC_OPTION_HAS_ARG},
		{"bench", MCC_OPTION_bench, 0},
		{"bt", MCC_OPTION_bt, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"b", MCC_OPTION_b, 0},
		{"-embed-jit", MCC_OPTION_embed_jit, 0},
		{"-no-embed-jit", MCC_OPTION_no_embed_jit, 0},
		{"-jit", MCC_OPTION_jit, 0},
		{"-no-jit", MCC_OPTION_no_jit, 0},
		{"-jit-max-duration", MCC_OPTION_jit_max_duration, MCC_OPTION_HAS_ARG},
		{"-jit-threads", MCC_OPTION_jit_threads, MCC_OPTION_HAS_ARG},
		{"-jit-functions", MCC_OPTION_jit_functions, MCC_OPTION_HAS_ARG},
		{"-jit-always-gpu", MCC_OPTION_jit_always_gpu, 0},
		{"-no-jit-always-gpu", MCC_OPTION_no_jit_always_gpu, 0},
		{"-jit-conservative", MCC_OPTION_jit_conservative, 0},
		{"-jit-cpu-budget", MCC_OPTION_jit_cpu_budget, MCC_OPTION_HAS_ARG},
		{"-jit-gpu-budget", MCC_OPTION_jit_gpu_budget, MCC_OPTION_HAS_ARG},
		{"-jit-gpu-devices", MCC_OPTION_jit_gpu_devices, MCC_OPTION_HAS_ARG},
		{"-clear-cache", MCC_OPTION_clear_cache, 0},
		{"-stats", MCC_OPTION_stats, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"g", MCC_OPTION_g, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
#ifdef MCC_TARGET_MACHO
		{"compatibility_version", MCC_OPTION_compatibility_version, MCC_OPTION_HAS_ARG},
		{"current_version", MCC_OPTION_current_version, MCC_OPTION_HAS_ARG},
		{"dynamiclib", MCC_OPTION_dynamiclib, 0},
		{"flat_namespace", MCC_OPTION_flat_namespace, 0},
		{"framework", MCC_OPTION_framework, MCC_OPTION_HAS_ARG},
		{"F", MCC_OPTION_F, MCC_OPTION_HAS_ARG},
		{"install_name", MCC_OPTION_install_name, MCC_OPTION_HAS_ARG},
		{"mmacosx-version-min=", MCC_OPTION_mmacosx_version_min, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"mmacos-version-min=", MCC_OPTION_mmacosx_version_min, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"two_levelnamespace", MCC_OPTION_two_levelnamespace, 0},
		{"undefined", MCC_OPTION_undefined, MCC_OPTION_HAS_ARG},
#endif
		{"c", MCC_OPTION_c, 0},
		{"dumpmachine", MCC_OPTION_dumpmachine, 0},
		{"dumpversion", MCC_OPTION_dumpversion, 0},
		{"print-multiarch", MCC_OPTION_print_multiarch, 0},
		{"debug=", MCC_OPTION_debug, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"-debug=", MCC_OPTION_debug, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"d", MCC_OPTION_d, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"static", MCC_OPTION_static, 0},
		{"std", MCC_OPTION_std, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"ansi", MCC_OPTION_ansi, 0},
		{"shared", MCC_OPTION_shared, 0},
		{"bundle", MCC_OPTION_bundle, 0},
		{"soname", MCC_OPTION_soname, MCC_OPTION_HAS_ARG},
		{"o", MCC_OPTION_o, MCC_OPTION_HAS_ARG},
		{"pthread", MCC_OPTION_pthread, 0},
		{"run", MCC_OPTION_run, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"rstdin", MCC_OPTION_rstdin, MCC_OPTION_HAS_ARG},
		{"rdynamic", MCC_OPTION_rdynamic, 0},
		{"r", MCC_OPTION_r, 0},
		{"Wl,", MCC_OPTION_Wl, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"Wp,", MCC_OPTION_Wp, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"Wa,", MCC_OPTION_Wa, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"Xpreprocessor", MCC_OPTION_Xpreprocessor, MCC_OPTION_HAS_ARG},
		{"Xassembler", MCC_OPTION_Xassembler, MCC_OPTION_HAS_ARG},
		{"Xlinker", MCC_OPTION_Xlinker, MCC_OPTION_HAS_ARG},
		{"save-temps", MCC_OPTION_save_temps, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"W", MCC_OPTION_W, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"O", MCC_OPTION_O, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
#ifdef MCC_TARGET_ARM
		{"mfloat-abi", MCC_OPTION_mfloat_abi, MCC_OPTION_HAS_ARG},
#endif
		{"m", MCC_OPTION_m, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"f", MCC_OPTION_f, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"pg", MCC_OPTION_pg, 0},
		{"isystem", MCC_OPTION_isystem, MCC_OPTION_HAS_ARG},
		{"iquote", MCC_OPTION_iquote, MCC_OPTION_HAS_ARG},
		{"idirafter", MCC_OPTION_idirafter, MCC_OPTION_HAS_ARG},
		{"imacros", MCC_OPTION_imacros, MCC_OPTION_HAS_ARG},
		{"-sysroot", MCC_OPTION_sysroot, MCC_OPTION_HAS_ARG},
		{"isysroot", MCC_OPTION_isysroot, MCC_OPTION_HAS_ARG},
		{"-embed-dir", MCC_OPTION_embed_dir, MCC_OPTION_HAS_ARG},
		{"-embed-directory", MCC_OPTION_embed_dir, MCC_OPTION_HAS_ARG},
		{"include", MCC_OPTION_include, MCC_OPTION_HAS_ARG},
		{"nostdinc", MCC_OPTION_nostdinc, 0},
		{"trigraphs", MCC_OPTION_trigraphs, 0},
		{"nostdlib", MCC_OPTION_nostdlib, 0},
		{"print-search-dirs", MCC_OPTION_print_search_dirs, 0},
		{"print-isa", MCC_OPTION_print_isa, 0},
		{"print-prog-name=", MCC_OPTION_print_prog_name, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"print-file-name=", MCC_OPTION_print_file_name, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"-print-prog-name=", MCC_OPTION_print_prog_name, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"-print-file-name=", MCC_OPTION_print_file_name, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"print-prog-name", MCC_OPTION_print_prog_name, MCC_OPTION_HAS_ARG},
		{"print-file-name", MCC_OPTION_print_file_name, MCC_OPTION_HAS_ARG},
		{"-print-prog-name", MCC_OPTION_print_prog_name, MCC_OPTION_HAS_ARG},
		{"-print-file-name", MCC_OPTION_print_file_name, MCC_OPTION_HAS_ARG},
		{"w", MCC_OPTION_w, 0},
		{"E", MCC_OPTION_E, 0},
		{"M", MCC_OPTION_M, 0},
		{"MM", MCC_OPTION_MM, 0},
		{"MD", MCC_OPTION_MD, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"MMD", MCC_OPTION_MMD, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
		{"MG", MCC_OPTION_MG, 0},
		{"MF", MCC_OPTION_MF, MCC_OPTION_HAS_ARG},
		{"MP", MCC_OPTION_MP, 0},
		{"MT", MCC_OPTION_MT, MCC_OPTION_HAS_ARG},
		{"MQ", MCC_OPTION_MQ, MCC_OPTION_HAS_ARG},
		{"S", MCC_OPTION_S, 0},
		{"x", MCC_OPTION_x, MCC_OPTION_HAS_ARG},
		{"ar", MCC_OPTION_ar, 0},
#ifdef MCC_TARGET_PE
		{"impdef", MCC_OPTION_impdef, 0},
#endif
		{"arch", 0, MCC_OPTION_HAS_ARG},
		{"CC", MCC_OPTION_C, 0},
		{"C", MCC_OPTION_C, 0},
		{"-param", 0, MCC_OPTION_HAS_ARG},
		{"pedantic", MCC_OPTION_pedantic, 0},
		{"pedantic-errors", MCC_OPTION_pedantic_errors, 0},
		{"pie", MCC_OPTION_pie, 0},
		{"no-pie", MCC_OPTION_nopie, 0},
		{"nopie", MCC_OPTION_nopie, 0},
		{"pipe", 0, 0},
		{"s", MCC_OPTION_s, 0},
		{"static-libgcc", 0, 0},
		{"static-libstdc++", 0, 0},
		{"traditional", 0, 0},
		{NULL, 0, 0},
};

typedef struct FlagDef {
	uint16_t offset;
	uint16_t flags;
	const char *name;
} FlagDef;

#define WD_ALL 0x0001
#define FD_INVERT 0x0002
#define FD_DEV 0x0004

static const FlagDef options_W[] = {
		{offsetof(MCCState, warn_all), WD_ALL, "all"},
		{offsetof(MCCState, warn_error), 0, "error"},
		{offsetof(MCCState, warn_main), WD_ALL, "main"},
		{offsetof(MCCState, warn_write_strings), 0, "write-strings"},
		{offsetof(MCCState, warn_unsupported), 0, "unsupported"},
		{offsetof(MCCState, warn_attributes), 0, "attributes"},
		{offsetof(MCCState, warn_attributes), 0, "unknown-attributes"},
		{offsetof(MCCState, warn_unsupported_option), 0, "unsupported-option"},
		{offsetof(MCCState, warn_implicit_function_declaration), WD_ALL, "implicit-function-declaration"},
		{offsetof(MCCState, warn_discarded_qualifiers), WD_ALL, "discarded-qualifiers"},
		{offsetof(MCCState, warn_sequence_point), WD_ALL, "sequence-point"},
		{offsetof(MCCState, warn_format), WD_ALL, "format"},
		{offsetof(MCCState, warn_vla), 0, "vla"},
		{offsetof(MCCState, warn_undef), 0, "undef"},
		{offsetof(MCCState, warn_implicit_int), 0, "implicit-int"},
		{offsetof(MCCState, warn_incompatible_pointer_types), 0, "incompatible-pointer-types"},
		{offsetof(MCCState, warn_int_conversion), 0, "int-conversion"},
		{offsetof(MCCState, warn_excess_initializers), 0, "excess-initializers"},
		{offsetof(MCCState, warn_sign_compare), 0, "sign-compare"},
		{offsetof(MCCState, warn_type_limits), 0, "type-limits"},
		{offsetof(MCCState, warn_bool_compare), WD_ALL, "bool-compare"},
		{offsetof(MCCState, warn_char_subscripts), WD_ALL, "char-subscripts"},
		{offsetof(MCCState, warn_enum_compare), WD_ALL, "enum-compare"},
		{offsetof(MCCState, warn_address), WD_ALL, "address"},
		{offsetof(MCCState, warn_pointer_sign), WD_ALL, "pointer-sign"},
		{offsetof(MCCState, warn_parentheses), WD_ALL, "parentheses"},
		{offsetof(MCCState, warn_switch), WD_ALL, "switch"},
		{offsetof(MCCState, warn_implicit_fallthrough), 0, "implicit-fallthrough"},
		{offsetof(MCCState, warn_override_init), 0, "override-init"},
		{offsetof(MCCState, warn_missing_field_initializers), 0, "missing-field-initializers"},
		{offsetof(MCCState, warn_empty_body), 0, "empty-body"},
		{offsetof(MCCState, warn_unused_variable), WD_ALL, "unused-variable"},
		{offsetof(MCCState, warn_unused_parameter), 0, "unused-parameter"},
		{offsetof(MCCState, warn_unused_function), WD_ALL, "unused-function"},
		{offsetof(MCCState, warn_cpp), WD_ALL, "cpp"},
		{offsetof(MCCState, warn_unused_label), WD_ALL, "unused-label"},
		{offsetof(MCCState, warn_unknown_pragmas), WD_ALL, "unknown-pragmas"},
		{offsetof(MCCState, warn_pedantic), 0, "pedantic"},
		{offsetof(MCCState, warn_fatal_errors), 0, "fatal-errors"},
		{offsetof(MCCState, warn_shadow), 0, "shadow"},
		{offsetof(MCCState, warn_unused_value), WD_ALL, "unused-value"},
		{offsetof(MCCState, warn_uninitialized), WD_ALL, "uninitialized"},
		{offsetof(MCCState, warn_varargs), 0, "varargs"},
		{offsetof(MCCState, warn_strict_prototypes), 0, "strict-prototypes"},
		{offsetof(MCCState, warn_return_type), WD_ALL, "return-type"},
		{offsetof(MCCState, warn_return_type), WD_ALL, "return-mismatch"},
		{offsetof(MCCState, warn_shift_count_negative), 0, "shift-count-negative"},
		{offsetof(MCCState, warn_shift_count_overflow), 0, "shift-count-overflow"},
		{offsetof(MCCState, warn_overflow), 0, "overflow"},
		{offsetof(MCCState, warn_div_by_zero), 0, "div-by-zero"},
		{offsetof(MCCState, warn_old_style_definition), 0, "old-style-definition"},
		{offsetof(MCCState, warn_undefined_internal), 0, "undefined-internal"},
		{offsetof(MCCState, warn_return_local_addr), 0, "return-local-addr"},
		{offsetof(MCCState, warn_deprecated_declarations), WD_ALL, "deprecated-declarations"},
		{offsetof(MCCState, warn_unused_result), WD_ALL, "unused-result"},
		{offsetof(MCCState, warn_nonnull), WD_ALL, "nonnull"},
		{offsetof(MCCState, warn_sentinel), WD_ALL, "sentinel"},
		{offsetof(MCCState, warn_address_of_packed_member), WD_ALL, "address-of-packed-member"},
		{0, 0, NULL}};

typedef char mcc_optflag_offsets_fit
		[(offsetof(MCCState, optflag) + MCC_OPT_COUNT <= 0xffffu) ? 1 : -1];

static const FlagDef options_f[] = {
		{offsetof(MCCState, char_is_unsigned), 0, "unsigned-char"},
		{offsetof(MCCState, char_is_unsigned), FD_INVERT, "signed-char"},
		{offsetof(MCCState, nocommon), FD_INVERT, "common"},
		{offsetof(MCCState, leading_underscore), 0, "leading-underscore"},
		{offsetof(MCCState, ms_extensions), 0, "ms-extensions"},
		{offsetof(MCCState, permissive), 0, "permissive"},
		{offsetof(MCCState, dollars_in_identifiers), 0, "dollars-in-identifiers"},
		{offsetof(MCCState, test_coverage), 0, "test-coverage"},
		{offsetof(MCCState, loop_census), 0, "loop-census"},
		{offsetof(MCCState, depth_census), 0, "depth-census"},
		{offsetof(MCCState, reverse_funcargs), 0, "reverse-funcargs"},
		{offsetof(MCCState, macro_eval), 0, "macro-eval"},
		{offsetof(MCCState, gnu89_inline), 0, "gnu89-inline"},
		{offsetof(MCCState, c99_inline_body), 0, "c99-inline-body"},
		{offsetof(MCCState, unwind_tables), 0, "asynchronous-unwind-tables"},
		{offsetof(MCCState, short_enums), 0, "short-enums"},
		{offsetof(MCCState, nobuiltin), FD_INVERT, "builtin"},
		{offsetof(MCCState, noasm), FD_INVERT, "asm"},
		{offsetof(MCCState, omit_frame_pointer), 0, "omit-frame-pointer"},
		{offsetof(MCCState, function_sections), 0, "function-sections"},
		{offsetof(MCCState, data_sections), 0, "data-sections"},
		{offsetof(MCCState, wrapv), 0, "wrapv"},
		{offsetof(MCCState, trigraphs), 0, "trigraphs"},
		{offsetof(MCCState, cx_limited_range), 0, "cx-limited-range"},
		{offsetof(MCCState, stdc_fenv_access), 0, "rounding-math"},
		{offsetof(MCCState, fold_math), 0, "fold-math"},
		{offsetof(MCCState, no_math_errno), FD_INVERT, "math-errno"},
		{offsetof(MCCState, do_asan_shadow), 0, "asan-shadow"},
		{offsetof(MCCState, freestanding), 0, "freestanding"},
		{offsetof(MCCState, freestanding), FD_INVERT, "hosted"},
		{offsetof(MCCState, syntax_only), 0, "syntax-only"},
		{offsetof(MCCState, diag_no_caret), FD_INVERT, "diagnostics-show-caret"},
#define MCC_OPT_ROW(id, name, dflt)                              \
	{(uint16_t)(offsetof(MCCState, optflag) + MCC_OPT_##id),        \
	 (uint16_t)(MCC_OPTD_IS_DEV(dflt) ? FD_DEV : 0), name},
		MCC_OPT_LIST(MCC_OPT_ROW)
#undef MCC_OPT_ROW
		{0, 0, NULL}};

static const FlagDef options_m[] = {
		{offsetof(MCCState, ms_bitfields), 0, "ms-bitfields"},
#ifdef MCC_TARGET_X86_64
		{offsetof(MCCState, nosse), FD_INVERT, "sse"},
#endif
		{0, 0, NULL}};

static int san_tok_eq(const char *tok, int n, const char *name) { MCC_TRACE("enter\n");
	return (int)strlen(name) == n && !memcmp(tok, name, n);
}

static unsigned san_recover_bits(const char *tok, int n) { MCC_TRACE("enter\n");
	if (san_tok_eq(tok, n, "undefined") || san_tok_eq(tok, n, "all"))
		{ MCC_TRACE("br\n"); return MCC_SANR_ALL; }
	if (san_tok_eq(tok, n, "signed-integer-overflow"))
		{ MCC_TRACE("br\n"); return MCC_SANR_OVERFLOW; }
	if (san_tok_eq(tok, n, "shift") || san_tok_eq(tok, n, "shift-exponent") ||
			san_tok_eq(tok, n, "shift-base"))
		{ MCC_TRACE("br\n"); return MCC_SANR_SHIFT; }
	if (san_tok_eq(tok, n, "integer-divide-by-zero") ||
			san_tok_eq(tok, n, "divide-by-zero"))
		{ MCC_TRACE("br\n"); return MCC_SANR_DIVREM; }
	if (san_tok_eq(tok, n, "null") || san_tok_eq(tok, n, "alignment") ||
			san_tok_eq(tok, n, "object-size"))
		{ MCC_TRACE("br\n"); return MCC_SANR_NULLPTR; }
	return 0;
}

static void san_recover_apply(MCCState *s1, const char *arg, int enable) { MCC_TRACE("enter\n");
	unsigned bits;
	if (*arg == '=')
		{ MCC_TRACE("br\n"); arg++; }
	if (!*arg) { MCC_TRACE("br\n");
		bits = MCC_SANR_ALL;
	} else { MCC_TRACE("br\n");
		const char *tok = arg;
		bits = 0;
		while (*tok) { MCC_TRACE("br\n");
			const char *end = tok;
			unsigned b;
			int n;
			while (*end && *end != ',')
				{ MCC_TRACE("br\n"); end++; }
			n = (int)(end - tok);
			b = san_recover_bits(tok, n);
			if (!b)
				{ MCC_TRACE("br\n");
					mcc_warning_c(warn_unsupported)(
							"-fsanitize-recover: unknown check '%.*s'", n, tok); }
			bits |= b;
			tok = *end ? end + 1 : end;
		}
	}
#if defined MCC_TARGET_X86_64 || defined MCC_TARGET_ARM64 || defined MCC_TARGET_RISCV64 || \
		defined MCC_TARGET_I386 || defined MCC_TARGET_ARM
	if (enable) { MCC_TRACE("br\n"); s1->do_sanitize_recover |= bits; }
	else { MCC_TRACE("br\n"); s1->do_sanitize_recover &= ~bits; }
#else
	if (enable && bits)
		{ MCC_TRACE("br\n");
			mcc_warning_c(warn_unsupported)(
					"UBSan recover (diagnostic handlers) is not implemented on this "
					"target; keeping trap-on-error"); }
	(void)s1;
#endif
}

static int set_flag(MCCState *s, const FlagDef *flags, const char *name) { MCC_TRACE("enter\n");
	int value, mask, ret;
	const FlagDef *p;
	const char *r;
	unsigned char *f;

	r = name, value = !strstart("no-", &r), mask = 0;

	if ((flags->flags & WD_ALL) && strstart("error=", &r))
		{ MCC_TRACE("br\n"); value = value ? WARN_ON | WARN_ERR : WARN_NOE, mask = WARN_ON; }

	for (ret = -1, p = flags; p->name; ++p) { MCC_TRACE("br\n");
		if (ret) { MCC_TRACE("br\n");
			if (strcmp(r, p->name))
				{ MCC_TRACE("br\n"); continue; }
		} else { MCC_TRACE("br\n");
			if (0 == (p->flags & WD_ALL))
				{ MCC_TRACE("br\n"); continue; }
		}

		if ((p->flags & FD_DEV) && value && !mcc_dev_enabled())
			{ MCC_TRACE("br\n"); return -2; }

		f = (unsigned char *)s + p->offset;
		*f = (*f & mask) | (value ^ !!(p->flags & FD_INVERT));

		if (ret) { MCC_TRACE("br\n");
			ret = 0;
			if (strcmp(r, "all"))
				{ MCC_TRACE("br\n"); break; }
		}
	}
	return ret;
}

static int pp_diag_set_flag(MCCState *s, const char *name) { MCC_TRACE("enter\n");
	return set_flag(s, options_W, name);
}

static int opt_level_top(void) { MCC_TRACE("enter\n");
	static const int dflt[] = {
#define MCC_OPT_ROW(id, name, d) (d),
			MCC_OPT_LIST(MCC_OPT_ROW)
#undef MCC_OPT_ROW
	};
	int i, dev = mcc_dev_enabled(), top = 0;
	for (i = 0; i < MCC_OPT_COUNT; i++) { MCC_TRACE("br\n");
		int d = dflt[i];
		if (MCC_OPTD_IS_DEV(d) && !dev)
			{ MCC_TRACE("br\n"); continue; }
		d = MCC_OPTD_BASE(d);
		if (!MCC_OPTD_IS_LEVEL(d))
			{ MCC_TRACE("br\n"); continue; }
		if (MCC_OPTD_LEVEL_OF(d) > top)
			{ MCC_TRACE("br\n"); top = MCC_OPTD_LEVEL_OF(d); }
	}
	return top;
}

static int opt_level_reject(MCCState *s1, const char *spelling, unsigned lvl) { MCC_TRACE("enter\n");
	int top = opt_level_top();
	if (lvl <= (unsigned)top || lvl == MCC_OPT_SEARCH_LEVEL)
		{ MCC_TRACE("br\n"); return 0; }
	if (lvl > MCC_OPT_SEARCH_LEVEL)
		{ MCC_TRACE("br\n"); return mcc_error_noabort(
					"'-O%s' is not an optimization level: this build has -O0 through -O%d, "
					"and -O%d, the optimization search",
					spelling, top, MCC_OPT_SEARCH_LEVEL); }
	return mcc_error_noabort(
			"'-O%s' reaches no optimization this build can enable: every knob above -O%d "
			"is experimental and gated behind MCC_DEV, so -O%s would compile exactly as "
			"-O%d and silently claim to be more. Use -O%d, or -O%d to search; set MCC_DEV=1 "
			"in the environment to reach -O%d through -O%d",
			spelling, top, spelling, top, top, MCC_OPT_SEARCH_LEVEL, top + 1,
			MCC_OPT_SEARCH_LEVEL - 1);
}

static const char dumpmachine_str[] =
#ifdef MCC_TARGET_I386
		"i386-pc"
#elif defined MCC_TARGET_X86_64
		"x86_64-pc"
#elif defined MCC_TARGET_ARM
		"arm"
#elif defined MCC_TARGET_ARM64
		"aarch64"
#elif defined MCC_TARGET_RISCV64
		"riscv64"
#endif
		"-"
#ifdef MCC_TARGET_PE
		"mingw32"
#elif defined(MCC_TARGET_MACHO)
		"apple-darwin"
#elif MCC_TARGETOS_FreeBSD || MCC_TARGETOS_FreeBSD_kernel
		"freebsd"
#elif MCC_TARGETOS_OpenBSD
		"openbsd"
#elif MCC_TARGETOS_NetBSD
		"netbsd"
#elif MCC_CONFIG_MUSL
		"linux-musl"
#else
		"linux-gnu"
#endif
		;

#if defined MCC_TARGET_MACHO
static uint32_t parse_version(MCCState *s1, const char *version) { MCC_TRACE("enter\n");
	uint32_t a = 0;
	uint32_t b = 0;
	uint32_t c = 0;
	char *last;

	a = strtoul(version, &last, 10);
	if (*last == '.') { MCC_TRACE("br\n");
		b = strtoul(&last[1], &last, 10);
		if (*last == '.')
			{ MCC_TRACE("br\n"); c = strtoul(&last[1], &last, 10); }
	}
	if (*last || a > 0xffff || b > 0xff || c > 0xff)
		{ MCC_TRACE("br\n"); mcc_error_noabort("version a.b.c not correct: %s", version); }
	return (a << 16) | (b << 8) | c;
}
#endif

static void insert_args(MCCState *s1, char ***pargv, int *pargc, int optind, const char *p, int sep) { MCC_TRACE("enter\n");
	int argc = 0;
	char **argv = NULL;
	for (int i = 0; i < *pargc; ++i)
		{ MCC_TRACE("br\n"); if (i == optind)
			{ MCC_TRACE("br\n"); dynarray_split(&argv, &argc, p, sep); }
		else
			{ MCC_TRACE("br\n"); dynarray_add(&argv, &argc, mcc_strdup((*pargv)[i])); } }
	dynarray_reset(&s1->argv, &s1->argc);
	*pargc = s1->argc = argc;
	*pargv = s1->argv = argv;
}

static void args_parser_add_file(MCCState *s, const char *filename, int filetype) { MCC_TRACE("enter\n");
	struct filespec *f = mcc_malloc(sizeof *f + strlen(filename));
	f->type = filetype;
	f->group = s->link_in_group;
	strcpy(f->name, filename);
	dynarray_add(&s->files, &s->nb_files, f);
	if (filetype & (AFF_TYPE_LIB | AFF_TYPE_FRAMEWORK))
		{ MCC_TRACE("br\n"); ++s->nb_libraries; }
}

/* T-lin-10393: parse a --jit-*-budget value. "auto" -> -2, "N" or "N%" (0..100) -> N,
 * anything else -> -3 (caller reports the error). */
static signed char parse_jit_budget(const char *v) { MCC_TRACE("enter\n");
	const char *p = v;
	int n;
	if (!strcmp(v, "auto"))
		{ MCC_TRACE("br\n"); return -2; }
	if (*p < '0' || *p > '9')
		{ MCC_TRACE("br\n"); return -3; }
	n = atoi(v);
	while (*p >= '0' && *p <= '9')
		{ MCC_TRACE("br\n"); p++; }
	if (*p == '%')
		{ MCC_TRACE("br\n"); p++; }
	if (*p != 0 || n < 0 || n > 100)
		{ MCC_TRACE("br\n"); return -3; }
	return (signed char)n;
}

PUB_FUNC int mcc_parse_args(MCCState *s, int *pargc, char ***pargv) { MCC_TRACE("enter\n");
	MCCState *s1 = s;
	const MCCOption *popt;
	const char *optarg, *r;
	const char *run = NULL;
	int optind = 1, empty = 1, x;
	char **argv = *pargv;
	int argc = *pargc;

	s->link_optind = s->link_argc;

	while (optind < argc) { MCC_TRACE("br\n");
		r = argv[optind];
		if (r[0] == '@' && r[1] != '\0') { MCC_TRACE("br\n");
			int fd;
			char *p;
			fd = open(++r, O_RDONLY | O_BINARY);
			if (fd < 0)
				{ MCC_TRACE("br\n"); return mcc_error_noabort("listfile '%s' not found", r); }
			p = mcc_load_text(fd);
			insert_args(s1, &argv, &argc, optind, p, 0);
			close(fd), mcc_free(p);
			continue;
		}
		optind++;
		if (r[0] != '-' || r[1] == '\0') { MCC_TRACE("br\n");
			args_parser_add_file(s, r, s->filetype);
			empty = 0;
		dorun:
			if (run)
				{ MCC_TRACE("br\n"); break; }
			continue;
		}
		if (r[1] == '-' && r[2] == '\0')
			{ MCC_TRACE("br\n"); goto dorun; }

		for (popt = mcc_options;; ++popt) { MCC_TRACE("br\n");
			const char *p1 = popt->name;
			const char *r1 = r + 1;
			if (p1 == NULL)
				{ MCC_TRACE("br\n"); return mcc_error_noabort("invalid option -- '%s'", r); }
			if (!strstart(p1, &r1))
				{ MCC_TRACE("br\n"); continue; }
			optarg = r1;
			if (popt->flags & MCC_OPTION_HAS_ARG) { MCC_TRACE("br\n");
				if (*r1 == '\0' && !(popt->flags & MCC_OPTION_NOSEP)) { MCC_TRACE("br\n");
					if (optind >= argc)
						{ MCC_TRACE("br\n"); return mcc_error_noabort("argument to '%s' is missing", r); }
					optarg = argv[optind++];
				}
			} else if (*r1 != '\0')
				{ MCC_TRACE("br\n"); continue; }
			break;
		}

		switch (popt->index) { MCC_TRACE("br\n");
		case MCC_OPTION_I:
			mcc_add_include_path(s, optarg);
			break;
		case MCC_OPTION_D:
			mcc_define_symbol(s, optarg, NULL);
			break;
		case MCC_OPTION_U:
			mcc_undefine_symbol(s, optarg);
			break;
		case MCC_OPTION_L:
			mcc_add_library_path(s, optarg);
			break;
		case MCC_OPTION_B:
			if (s->mcc_lib_path && s->mcc_lib_path[0]) { MCC_TRACE("br\n");
				char sub[2048];
				const char *b = s->mcc_lib_path;
				mcc_add_library_path(s, b);
				if ((size_t)snprintf(sub, sizeof sub, "%s/lib", b) < sizeof sub)
					{ MCC_TRACE("br\n"); mcc_add_library_path(s, sub); }
				if ((size_t)snprintf(sub, sizeof sub, "%s/include", b) < sizeof sub)
					{ MCC_TRACE("br\n"); mcc_add_sysinclude_path(s, sub); }
				if ((size_t)snprintf(sub, sizeof sub, "%s/include/winapi", b) < sizeof sub)
					{ MCC_TRACE("br\n"); mcc_add_sysinclude_path(s, sub); }
			}
			mcc_set_lib_path(s, optarg);
			continue;
		case MCC_OPTION_l:
			args_parser_add_file(s, optarg, AFF_TYPE_LIB | (s->filetype & ~AFF_TYPE_MASK));
			break;
		case MCC_OPTION_pthread:
			s->option_pthread = 1;
			break;
		case MCC_OPTION_bench:
			s->do_bench = 1;
			break;
		case MCC_OPTION_pie:
			s->pie = 1;
			break;
		case MCC_OPTION_nopie:
			s->pie = 0;
			break;
		case MCC_OPTION_pedantic:
			s->warn_pedantic = 1;
			break;
		case MCC_OPTION_pedantic_errors:
			s->warn_pedantic = 1;
			s->pedantic_errors = 1;
			break;
		case MCC_OPTION_s:
			s->do_strip = 1;
			break;
		case MCC_OPTION_lsp:
			s->lsp = 1;
			break;
		case MCC_OPTION_bt: {
			const char *bp = optarg;
			while (isnum(*bp))
				{ MCC_TRACE("br\n"); bp++; }
			if (*bp)
				{ MCC_TRACE("br\n"); goto unsupported_option; }
		}
			s->rt_num_callers = atoi(optarg);
			goto enable_backtrace;
		enable_backtrace:
			s->do_backtrace = 1;
			if (0 == s->do_debug)
				{ MCC_TRACE("br\n"); s->do_debug = 1; }
			s->dwarf = MCC_CONFIG_DWARF_VERSION;
			break;
		case MCC_OPTION_embed_jit:
			s->embed_jit = 1;
			break;
		case MCC_OPTION_no_embed_jit:
			s->embed_jit = 0;
			break;
		case MCC_OPTION_jit:
			s->jit = 1;
			break;
		case MCC_OPTION_no_jit:
			s->jit = 0;
			break;
		case MCC_OPTION_jit_max_duration:
			if (optarg[0] == '=')
				{ MCC_TRACE("br\n"); optarg++; }
			s->jit_max_duration = (unsigned)atoi(optarg);
			break;
		case MCC_OPTION_jit_threads:
			if (optarg[0] == '=')
				{ MCC_TRACE("br\n"); optarg++; }
			s->jit_threads = (unsigned)atoi(optarg);
			break;
		case MCC_OPTION_jit_functions:
			if (optarg[0] == '=')
				{ MCC_TRACE("br\n"); optarg++; }
			mcc_set_str(&s->jit_functions, optarg);
			break;
		case MCC_OPTION_jit_always_gpu:
			s->jit_always_gpu = 1;
			break;
		case MCC_OPTION_no_jit_always_gpu:
			s->jit_always_gpu = 0;
			break;
		case MCC_OPTION_jit_conservative:
			s->jit_conservative = 1;
			break;
		case MCC_OPTION_jit_cpu_budget:
			if (optarg[0] == '=')
				{ MCC_TRACE("br\n"); optarg++; }
			s->jit_cpu_budget = parse_jit_budget(optarg);
			if (s->jit_cpu_budget == -3)
				{ MCC_TRACE("br\n"); return mcc_error_noabort(
					"--jit-cpu-budget wants a percentage like '50%%' or 'auto', not '%s'", optarg); }
			break;
		case MCC_OPTION_jit_gpu_budget:
			if (optarg[0] == '=')
				{ MCC_TRACE("br\n"); optarg++; }
			s->jit_gpu_budget = parse_jit_budget(optarg);
			if (s->jit_gpu_budget == -3)
				{ MCC_TRACE("br\n"); return mcc_error_noabort(
					"--jit-gpu-budget wants a percentage like '50%%' or 'auto', not '%s'", optarg); }
			break;
		case MCC_OPTION_jit_gpu_devices:
			if (optarg[0] == '=')
				{ MCC_TRACE("br\n"); optarg++; }
			s->jit_gpu_devices = (signed char)atoi(optarg);
			if (s->jit_gpu_devices < 1)
				{ MCC_TRACE("br\n"); return mcc_error_noabort(
					"--jit-gpu-devices wants a device count >= 1, not '%s'", optarg); }
			break;
		case MCC_OPTION_clear_cache:
			s->clear_cache = 1;
			break;
		case MCC_OPTION_stats:
			if (optarg[0] == '=')
				{ MCC_TRACE("br\n"); optarg++; }
			if (optarg[0]) { MCC_TRACE("br\n");
				char *send;
				unsigned long sval = strtoul(optarg, &send, 0);
				if (*send)
					{ MCC_TRACE("br\n"); goto unsupported_option; }
				s->stats = (unsigned)sval;
			} else
				{ MCC_TRACE("br\n"); s->stats = MCC_STATS_ALL; }
			mcc_stats_enable(s->stats);
			break;
		case MCC_OPTION_b:
			s->do_bounds_check = 1;
			goto enable_backtrace;
			break;
		case MCC_OPTION_g:
			s->do_debug = 2;
			s->dwarf = MCC_CONFIG_DWARF_VERSION;
		g_redo:
			if (strstart("dwarf", &optarg)) { MCC_TRACE("br\n");
				s->dwarf = (*optarg) ? (0 - atoi(optarg)) : DEFAULT_DWARF_VERSION;
			} else if (0 == strcmp("stabs", optarg)) { MCC_TRACE("br\n");
				s->dwarf = 0;
			} else if (strstart("gdb", &optarg)) { MCC_TRACE("br\n");
				goto g_redo;
			} else if (isnum(*optarg)) { MCC_TRACE("br\n");
				x = *optarg++ - '0';
				s->do_debug = x > 2
													? 2
											: x == 0 && s->do_backtrace
													? 1
													: x;
				goto g_redo;
#ifdef MCC_TARGET_PE
			} else if (0 == strcmp(".pdb", optarg)) { MCC_TRACE("br\n");
				s->dwarf = 5, s->do_debug |= 16;
			} else if (0 == strcmp("codeview", optarg)) { MCC_TRACE("br\n");
				s->cv_debug = 1;
				if (s->dwarf == 0)
					{ MCC_TRACE("br\n"); s->dwarf = DEFAULT_DWARF_VERSION; }
#endif
			} else if (*optarg)
				{ MCC_TRACE("br\n"); goto unsupported_option; }
			break;
		case MCC_OPTION_c:
			x = MCC_OUTPUT_OBJ;
		set_output_type:
			if (s->output_type && s->output_type != x)
				{ MCC_TRACE("br\n"); mcc_warning("-%s: overriding compiler action already specified", popt->name); }
			s->output_type = x;
			break;
		case MCC_OPTION_d:
			if (*optarg == 'D')
				{ MCC_TRACE("br\n"); s->dflag = 3; }
			else if (*optarg == 'M')
				{ MCC_TRACE("br\n"); s->dflag = 7; }
			else if (*optarg == 't')
				{ MCC_TRACE("br\n"); s->dflag = 16; }
			else if (isnum(*optarg))
				{ MCC_TRACE("br\n"); g_debug |= atoi(optarg); }
			else
				{ MCC_TRACE("br\n"); goto unsupported_option; }
			break;
		case MCC_OPTION_debug: {
			static const struct {
				const char *name;
				int bit;
			} debug_cats[] = {
					{"reloc", MCC_DBG_RELOC}, {"inc", MCC_DBG_INC},
					{"pp", MCC_DBG_PP}, {"struct", MCC_DBG_STRUCT},
					{"tok", MCC_DBG_TOK}, {"pe", MCC_DBG_PE},
					{"ver", MCC_DBG_VER}, {"asm", MCC_DBG_ASM},
					{"sym", MCC_DBG_SYM},
			};
			const char *p = optarg;
			while (*p) { MCC_TRACE("br\n");
				size_t n = strcspn(p, ",");
				unsigned i;
				for (i = 0; i < countof(debug_cats); i++)
					{ MCC_TRACE("br\n"); if (strlen(debug_cats[i].name) == n &&
							!memcmp(debug_cats[i].name, p, n))
						{ MCC_TRACE("br\n"); break; } }
				if (i == countof(debug_cats))
					{ MCC_TRACE("br\n"); return mcc_error_noabort("unknown debug category '%.*s'", (int)n, p); }
				g_debug |= debug_cats[i].bit;
				p += n;
				if (*p)
					{ MCC_TRACE("br\n"); p++; }
			}
			break;
		}
		case MCC_OPTION_static:
			s->static_link = 1;
			break;
		case MCC_OPTION_ansi:
			optarg = "c90";
			FALLTHROUGH;
		case MCC_OPTION_std: {
			const char *std = optarg;
			if (*std == '=')
				{ MCC_TRACE("br\n"); std++; }
			const char *disp = std;
			int strict_iso = 0;
			if (strstart("gnu", &std))
				{ MCC_TRACE("br\n"); ; }
			else if (strstart("c", &std))
				{ MCC_TRACE("br\n"); strict_iso = 1; }
			else if (strstart("iso9899:", &std)) { MCC_TRACE("br\n");
				strict_iso = 1;
				if (!strcmp(std, "1990"))
					{ MCC_TRACE("br\n"); std = "90"; }
				else if (!strcmp(std, "199409"))
					{ MCC_TRACE("br\n"); std = "94"; }
				else if (!strcmp(std, "1999") || !strcmp(std, "199901"))
					{ MCC_TRACE("br\n"); std = "99"; }
				else if (!strcmp(std, "2011"))
					{ MCC_TRACE("br\n"); std = "11"; }
				else if (!strcmp(std, "2017") || !strcmp(std, "2018"))
					{ MCC_TRACE("br\n"); std = "17"; }
				else if (!strcmp(std, "2024"))
					{ MCC_TRACE("br\n"); std = "23"; }
			} else { MCC_TRACE("br\n");
				mcc_warning("unsupported language standard '%s'", disp);
				break;
			}
			if (!strcmp(std, "89") || !strcmp(std, "90"))
				{ MCC_TRACE("br\n"); s->cversion = 0; }
			else if (!strcmp(std, "94"))
				{ MCC_TRACE("br\n"); s->cversion = 199409; }
			else if (!strcmp(std, "99"))
				{ MCC_TRACE("br\n"); s->cversion = 199901; }
			else if (!strcmp(std, "11"))
				{ MCC_TRACE("br\n"); s->cversion = 201112; }
			else if (!strcmp(std, "17") || !strcmp(std, "18"))
				{ MCC_TRACE("br\n"); s->cversion = 201710; }
			else if (!strcmp(std, "23") || !strcmp(std, "2x"))
				{ MCC_TRACE("br\n"); s->cversion = 202311; }
			else if (!strcmp(std, "2y"))
				{ MCC_TRACE("br\n"); s->cversion = 202400; }
			else
				{ MCC_TRACE("br\n"); mcc_warning("unsupported language standard '%s'", disp); }
			s->trigraphs = strict_iso && !(s->cversion >= 202311);
			s->std_strict_ansi = (unsigned char)strict_iso;
		} break;
		case MCC_OPTION_shared:
			x = MCC_OUTPUT_DLL;
			goto set_output_type;
		case MCC_OPTION_bundle:
			s->macho_bundle = 1;
			x = MCC_OUTPUT_DLL;
			goto set_output_type;
		case MCC_OPTION_soname:
			mcc_set_str(&s->soname, optarg);
			break;
		case MCC_OPTION_o:
			if (s->outfile) { MCC_TRACE("br\n");
				mcc_warning("multiple -o option");
			}
#if MCC_HOST_WIN32
			if (!strcmp(optarg, "/dev/null"))
				optarg = "NUL";
#endif
			mcc_set_str(&s->outfile, optarg);
			break;
		case MCC_OPTION_r:
			s->option_r = 1;
			x = MCC_OUTPUT_OBJ;
			goto set_output_type;
		case MCC_OPTION_isystem:
			mcc_add_sysinclude_path(s, optarg);
			break;
		case MCC_OPTION_iquote:
			mcc_add_iquote_path(s, optarg);
			break;
		case MCC_OPTION_idirafter:
			mcc_add_afterinc_path(s, optarg);
			break;
		case MCC_OPTION_sysroot:
		case MCC_OPTION_isysroot:
			if (*optarg == '=')
				{ MCC_TRACE("br\n"); optarg++; }
			mcc_set_str(&s->sysroot, optarg);
			break;
		case MCC_OPTION_embed_dir:
			if (*optarg == '=')
				{ MCC_TRACE("br\n"); optarg++; }
			mcc_add_embed_path(s, optarg);
			break;
		case MCC_OPTION_include:
			cstr_printf(&s->cmdline_incl, "#include \"%s\"\n", optarg);
			break;
		case MCC_OPTION_imacros:
			cstr_printf(&s->cmdline_imacros, "#include \"%s\"\n", optarg);
			break;
		case MCC_OPTION_nostdinc:
			s->nostdinc = 1;
			break;
		case MCC_OPTION_trigraphs:
			s->trigraphs = 1;
			break;
		case MCC_OPTION_nostdlib:
			s->nostdlib = 1;
			break;
		case MCC_OPTION_run:
#ifdef MCC_TARGET_IS_HOST
			run = optarg;
			x = MCC_OUTPUT_MEMORY;
			goto set_output_type;
#else
			return mcc_error_noabort("-run is not available in a cross compiler");
#endif
#ifdef MCC_TARGET_IS_HOST
		case MCC_OPTION_rstdin:
			s->run_stdin = optarg;
			break;
#endif
		case MCC_OPTION_v: {
			const char *vp = optarg;
			if (optarg[0] >= '0' && optarg[0] <= '9') { MCC_TRACE("br\n");
				char *vend;
				unsigned long vlvl = strtoul(optarg, &vend, 0);
				if (*vend)
					{ MCC_TRACE("br\n"); goto unsupported_option; }
				s->verbose |= (unsigned char)vlvl;
			} else { MCC_TRACE("br\n");
				while (*vp == 'v')
					{ MCC_TRACE("br\n"); vp++; }
				if (*vp)
					{ MCC_TRACE("br\n"); goto unsupported_option; }
				do
					{ MCC_TRACE("br\n"); s->verbose = (unsigned char)(s->verbose | (s->verbose + 1)); }
				while (*optarg++ == 'v');
			}
			continue;
		}
		case MCC_OPTION_f: {
			const char *vis = optarg;
			if (strstart("max-errors=", &vis)) { MCC_TRACE("br\n");
				s->max_errors = atoi(vis);
			} else if (strstart("opt-search-ticks=", &vis)) { MCC_TRACE("br\n");
				int t = atoi(vis);
				if (t < 0)
					{ MCC_TRACE("br\n"); mcc_warning("-fopt-search-ticks=%s: negative tick "
																					 "count, using 0", vis); t = 0; }
				s->optimize_search_ticks = (unsigned)t;
				s->optimize_search_ticks_set = 1;
			} else if (strstart("visibility=", &vis)) { MCC_TRACE("br\n");
				if (!strcmp(vis, "default"))
					{ MCC_TRACE("br\n"); s->visibility = STV_DEFAULT; }
				else if (!strcmp(vis, "hidden"))
					{ MCC_TRACE("br\n"); s->visibility = STV_HIDDEN; }
				else if (!strcmp(vis, "internal"))
					{ MCC_TRACE("br\n"); s->visibility = STV_INTERNAL; }
				else if (!strcmp(vis, "protected"))
					{ MCC_TRACE("br\n"); s->visibility = STV_PROTECTED; }
				else
					{ MCC_TRACE("br\n"); mcc_warning("unsupported visibility '%s'", vis); }
			} else if (!strcmp(optarg, "PIC") || !strcmp(optarg, "PIE")) { MCC_TRACE("br\n");
				s->pic = 2;
			} else if (!strcmp(optarg, "pic") || !strcmp(optarg, "pie")) { MCC_TRACE("br\n");
				s->pic = 1;
			} else if (!strcmp(optarg, "no-pic") || !strcmp(optarg, "no-PIC") || !strcmp(optarg, "no-pie") || !strcmp(optarg, "no-PIE")) { MCC_TRACE("br\n");
				s->pic = 0;
			} else if (!strcmp(optarg, "stack-protector") || !strcmp(optarg, "stack-protector-strong") || !strcmp(optarg, "stack-protector-all")) { MCC_TRACE("br\n");
#if (defined MCC_TARGET_X86_64 && !defined MCC_TARGET_PE) || (defined MCC_TARGET_ARM64 && !defined MCC_TARGET_PE) || (defined MCC_TARGET_I386 && !defined MCC_TARGET_PE) || defined MCC_TARGET_ARM || defined MCC_TARGET_RISCV64
				s->stack_protector = 1;
#else
				mcc_warning_c(warn_unsupported)(
						"-f%s: stack protection is only implemented on x86_64 (ELF/Mach-O), "
						"arm64 (ELF/Mach-O), i386 ELF, arm ELF and riscv64",
						optarg);
#endif
			} else if (!strcmp(optarg, "no-stack-protector")) { MCC_TRACE("br\n");
				s->stack_protector = 0;
			} else if (!strcmp(optarg, "building-libgcc")) { MCC_TRACE("br\n");
#if defined MCC_TARGET_X86_64 && !defined MCC_TARGET_PE && !defined MCC_TARGET_MACHO
				mcc_define_symbol(s, "__LIBGCC_DWARF_FRAME_REGISTERS__", "17");
				mcc_define_symbol(s, "__LIBGCC_DWARF_CIE_DATA_ALIGNMENT__", "-8");
				mcc_define_symbol(s, "__LIBGCC_EH_RETURN_STACKADJ_RTX__", "1");
				mcc_define_symbol(s, "__LIBGCC_STACK_POINTER_REGNUM__", "7");
				mcc_define_symbol(s, "__LIBGCC_TRAMPOLINE_SIZE__", "28");
				mcc_define_symbol(s, "__LIBGCC_STACK_GROWS_DOWNWARD__", "1");
				mcc_define_symbol(s, "__LIBGCC_EH_FRAME_SECTION_NAME__", "\".eh_frame\"");
				mcc_define_symbol(s, "__LIBGCC_EH_TABLES_CAN_BE_READ_ONLY__", "1");
				mcc_define_symbol(s, "__LIBGCC_TEXT_SECTION_ASM_OP__", "\"\\t.text\"");
				mcc_define_symbol(s, "__LIBGCC_INIT_ARRAY_SECTION_ASM_OP__", "1");
				mcc_define_symbol(s, "__LIBGCC_VTABLE_USES_DESCRIPTORS__", "0");
				mcc_define_symbol(s, "__LIBGCC_HAVE_LIBATOMIC", "0");
				mcc_define_symbol(s, "__LIBGCC_HAVE_HWDBL__", "1");
#endif
			} else if (strstart("sanitize=", &vis)) { MCC_TRACE("br\n");
				const char *tok = vis;
				while (*tok) { MCC_TRACE("br\n");
					const char *end = tok;
					int n;
					while (*end && *end != ',')
						{ MCC_TRACE("br\n"); end++; }
					n = (int)(end - tok);
					if (san_tok_eq(tok, n, "undefined") ||
							san_tok_eq(tok, n, "signed-integer-overflow") ||
							san_tok_eq(tok, n, "integer-divide-by-zero") ||
							san_tok_eq(tok, n, "shift") ||
							san_tok_eq(tok, n, "shift-exponent") ||
							san_tok_eq(tok, n, "shift-base")) { MCC_TRACE("br\n");
#if defined MCC_TARGET_X86_64 || defined MCC_TARGET_ARM64 || \
		defined MCC_TARGET_RISCV64 || defined MCC_TARGET_I386 || \
		defined MCC_TARGET_ARM
						s->do_sanitize_undefined = 1;
#else
						mcc_warning_c(warn_unsupported)(
								"-fsanitize=undefined is only implemented on x86_64, arm64, "
								"riscv64, i386, and arm");
#endif
					} else if (san_tok_eq(tok, n, "address") ||
										 san_tok_eq(tok, n, "bounds")) { MCC_TRACE("br\n");
						s->do_bounds_check = 1;
						s->do_sanitize_address = 1;
						s->do_backtrace = 1;
						if (0 == s->do_debug)
							{ MCC_TRACE("br\n"); s->do_debug = 1; }
						s->dwarf = MCC_CONFIG_DWARF_VERSION;
					} else if (san_tok_eq(tok, n, "thread") ||
										 san_tok_eq(tok, n, "memory")) { MCC_TRACE("br\n");
						return mcc_error_noabort(
								"-fsanitize=%.*s is not yet implemented in this mcc", n, tok);
					} else { MCC_TRACE("br\n");
						return mcc_error_noabort("unknown -fsanitize= check '%.*s'", n, tok);
					}
					tok = *end ? end + 1 : end;
				}
			} else if (!strcmp(optarg, "no-sanitize") || strstart("no-sanitize=", &vis)) { MCC_TRACE("br\n");
				s->do_sanitize_undefined = 0;
				s->do_sanitize_address = 0;
			} else if (!strcmp(optarg, "sanitize-undefined-trap-on-error")) { MCC_TRACE("br\n");
				s->do_sanitize_recover = 0;
			} else if (strstart("sanitize-recover", &vis) ||
								 strstart("no-sanitize-trap", &vis)) { MCC_TRACE("br\n");
				san_recover_apply(s, vis, 1);
			} else if (strstart("no-sanitize-recover", &vis) ||
								 strstart("sanitize-trap", &vis)) { MCC_TRACE("br\n");
				san_recover_apply(s, vis, 0);
			} else if (!strcmp(optarg, "diagnostics-color") || !strcmp(optarg, "diagnostics-color=always") || !strcmp(optarg, "color-diagnostics")) { MCC_TRACE("br\n");
				s->diag_color = 1;
			} else if (!strcmp(optarg, "diagnostics-color=never") || !strcmp(optarg, "no-diagnostics-color") || !strcmp(optarg, "no-color-diagnostics")) { MCC_TRACE("br\n");
				s->diag_color = 2;
			} else if (!strcmp(optarg, "diagnostics-color=auto")) { MCC_TRACE("br\n");
				s->diag_color = 0;
			} else if (!strcmp(optarg, "fast-math") ||
								 !strcmp(optarg, "no-fast-math")) { MCC_TRACE("br\n");
				unsigned char on = optarg[0] != 'n';
				s->no_math_errno = on;
				s->fold_math = on;
				s->cx_limited_range = on;
				s->fast_math = on;
				s->optflag[MCC_OPT_BUILTIN_MATH] = on ? 1 : MCC_OPT_UNSET;
				s->optflag[MCC_OPT_BUILTIN_MATH_FABS] = on ? 1 : MCC_OPT_UNSET;
			} else if (!strncmp(optarg, "fp-contract=", 12) ||
								 !strcmp(optarg, "unsafe-math-optimizations") ||
								 !strcmp(optarg, "no-unsafe-math-optimizations") ||
								 !strcmp(optarg, "unroll-loops") ||
								 !strcmp(optarg, "no-unroll-loops") ||
								 !strcmp(optarg, "plt") || !strcmp(optarg, "no-plt") ||
								 !strcmp(optarg, "semantic-interposition") ||
								 !strcmp(optarg, "no-semantic-interposition") ||
								 !strcmp(optarg, "visibility-inlines-hidden") ||
								 !strcmp(optarg, "delete-null-pointer-checks") ||
								 !strcmp(optarg, "no-delete-null-pointer-checks") ||
								 !strcmp(optarg, "ident") || !strcmp(optarg, "no-ident")) { MCC_TRACE("br\n");
			} else { MCC_TRACE("br\n");
				int sf = set_flag(s, options_f, optarg);
				if (sf == -2)
					{ MCC_TRACE("br\n"); return mcc_error_noabort(
							"-f%s is an experimental optimization and is gated off in this "
							"build. It is refused rather than ignored so that it cannot be "
							"mistaken for a flag that does not exist. Set MCC_DEV=1 in the "
							"environment to enable it; -fno-%s is always accepted",
							optarg, optarg); }
				if (sf < 0)
					{ MCC_TRACE("br\n"); goto unsupported_option; }
			}
		} break;
		case MCC_OPTION_pg:
			goto unsupported_option;
#ifdef MCC_TARGET_ARM
		case MCC_OPTION_mfloat_abi:
			if (!strcmp(optarg, "softfp")) { MCC_TRACE("br\n");
				s->float_abi = ARM_SOFTFP_FLOAT;
			} else if (!strcmp(optarg, "hard"))
				{ MCC_TRACE("br\n"); s->float_abi = ARM_HARD_FLOAT; }
			else
				{ MCC_TRACE("br\n"); return mcc_error_noabort("unsupported float abi '%s'", optarg); }
			continue;
#endif
		case MCC_OPTION_m:
			if (set_flag(s, options_m, optarg) < 0) { MCC_TRACE("br\n");
				const char *marg = optarg;
				if (strstart("arch=", &marg)) { MCC_TRACE("br\n");
					if (mcc_isa_set_arch(s, marg) < 0)
						{ MCC_TRACE("br\n"); return mcc_error_noabort("unknown -march= value '%s'", marg); }
					break;
				}
				if (strstart("tune=", &marg) || strstart("cpu=", &marg) ||
						strstart("cmodel=", &marg) || strstart("fpmath=", &marg))
					{ MCC_TRACE("br\n"); break; }
				if (x = atoi(optarg), x != 32 && x != 64)
					{ MCC_TRACE("br\n"); goto unsupported_option; }
				if (MCC_PTR_SIZE != x / 8)
					{ MCC_TRACE("br\n"); return x; }
				continue;
			}
			break;
		case MCC_OPTION_W:
			if (!strcmp(optarg, "extra") || !strcmp(optarg, "no-extra")) { MCC_TRACE("br\n");
				unsigned char on = optarg[0] == 'e' ? WARN_ON : 0;
				s->warn_sign_compare = on;
				s->warn_unused_parameter = on;
				s->warn_extra_ptr_zero_cmp = on;
				s->warn_type_limits = on;
				s->warn_missing_field_initializers = on;
				s->warn_override_init = on;
				s->warn_empty_body = on;
				break;
			}
			if (optarg[0] && set_flag(s, options_W, optarg) < 0)
				{ MCC_TRACE("br\n"); goto unsupported_option; }
			break;
		case MCC_OPTION_w:
			s->warn_none = 1;
			break;
		case MCC_OPTION_rdynamic:
			s->rdynamic = 1;
			break;
		case MCC_OPTION_Wl:
			if (mcc_set_linker(s, optarg) < 0)
				{ MCC_TRACE("br\n"); return -1; }
			break;
		case MCC_OPTION_Wp:
			if (argv[0])
				{ MCC_TRACE("br\n"); insert_args(s, &argv, &argc, --optind, optarg, ','); }
			break;
		case MCC_OPTION_Xpreprocessor:
			if (argv[0])
				{ MCC_TRACE("br\n"); insert_args(s, &argv, &argc, --optind, optarg, ','); }
			break;
		case MCC_OPTION_Xassembler:
			break;
		case MCC_OPTION_Wa:
			break;
		case MCC_OPTION_Xlinker:
			/* gcc's `-Xlinker arg` passes a single argument to the linker;
			 * reuse the -Wl, machinery so linker options (-rpath, -soname, ...)
			 * get recognized. libtool/autotools rely on this not hard-erroring. */
			if (mcc_set_linker(s, optarg) < 0)
				{ MCC_TRACE("br\n"); return -1; }
			break;
		case MCC_OPTION_save_temps:
			break;
		case MCC_OPTION_E:
			x = MCC_OUTPUT_PREPROCESS;
			goto set_output_type;
		case MCC_OPTION_P: {
			const char *pp = optarg;
			while (isnum(*pp))
				{ MCC_TRACE("br\n"); pp++; }
			if (*pp)
				{ MCC_TRACE("br\n"); goto unsupported_option; }
			s->Pflag = atoi(optarg) + 1;
		} break;

		case MCC_OPTION_M:
			s->include_sys_deps = 1;
			FALLTHROUGH;
		case MCC_OPTION_MM:
			s->just_deps = 1;
			s->gen_deps = 1;
			if (!s->deps_outfile)
				{ MCC_TRACE("br\n"); mcc_set_str(&s->deps_outfile, "-"); }
			break;
		case MCC_OPTION_MD:
			s->include_sys_deps = 1;
			FALLTHROUGH;
		case MCC_OPTION_MMD:
			s->gen_deps = 1;
			if (*optarg != ',') { MCC_TRACE("br\n");
				if (*optarg)
					{ MCC_TRACE("br\n"); goto unsupported_option; }
				break;
			}
			++optarg;
			FALLTHROUGH;
		case MCC_OPTION_MF:
			mcc_set_str(&s->deps_outfile, optarg);
			break;
		case MCC_OPTION_MP:
			s->gen_phony_deps = 1;
			break;
		case MCC_OPTION_MG:
			s->gen_deps_missing_ok = 1;
			break;
		case MCC_OPTION_S:
			x = MCC_OUTPUT_ASM;
			goto set_output_type;
		case MCC_OPTION_MT:
		case MCC_OPTION_MQ: {
			const char *src = optarg;
			int extra = 0, sep = s->dep_target ? 1 : 0;
			if (popt->index == MCC_OPTION_MQ)
				{ MCC_TRACE("br\n"); for (const char *q = src; *q; q++)
					{ MCC_TRACE("br\n"); if (*q == '$')
						{ MCC_TRACE("br\n"); extra++; } } }
			{
				int oldlen = s->dep_target ? (int)strlen(s->dep_target) : 0;
				char *nt = mcc_malloc(oldlen + sep + (int)strlen(src) + extra + 1);
				char *d = nt;
				if (s->dep_target) { MCC_TRACE("br\n");
					memcpy(d, s->dep_target, oldlen);
					d += oldlen;
					*d++ = ' ';
				}
				for (const char *q = src; *q; q++) { MCC_TRACE("br\n");
					if (popt->index == MCC_OPTION_MQ && *q == '$')
						{ MCC_TRACE("br\n"); *d++ = '$'; }
					*d++ = *q;
				}
				*d = '\0';
				mcc_free(s->dep_target);
				s->dep_target = nt;
			}
		} break;

		case MCC_OPTION_dumpmachine:
			printf("%s\n", dumpmachine_str);
			exit(0);
		case MCC_OPTION_print_multiarch:
			printf("\n");
			exit(0);
		case MCC_OPTION_dumpversion:
			/* Report the emulated GCC version (matches the __GNUC__* predefines),
			 * not the date-based MCC_VERSION which breaks CMake/autoconf probes. */
			printf("%d.%d.%d\n", MCC_GNUC_MAJOR, MCC_GNUC_MINOR, MCC_GNUC_PATCHLEVEL);
			exit(0);

		case MCC_OPTION_x:
			x = 0;
			if (*optarg == 'c')
				{ MCC_TRACE("br\n"); x = AFF_TYPE_C; }
			else if (*optarg == 'a')
				/* gcc: "assembler" is assembled directly, "assembler-with-cpp"
				 * is preprocessed first; mcc matched only the leading 'a' and
				 * always enabled cpp. */
				{ MCC_TRACE("br\n"); x = !strcmp(optarg, "assembler") ? AFF_TYPE_ASM : AFF_TYPE_ASMPP; }
			else if (*optarg == 'b')
				{ MCC_TRACE("br\n"); x = AFF_TYPE_BIN; }
			else if (*optarg == 'n')
				{ MCC_TRACE("br\n"); x = AFF_TYPE_NONE; }
			else
				{ MCC_TRACE("br\n"); mcc_warning("unsupported language '%s'", optarg); }
			s->filetype = x | (s->filetype & ~AFF_TYPE_MASK);
			break;
		case MCC_OPTION_O:
			s->optimize_size = 0;
			if (!s->optimize_search_ticks_set)
				{ MCC_TRACE("br\n"); s->optimize_search_ticks = 0; }
			s->optimize_search_all = 0;
			s->optimize_level = 0;
			if (optarg[0] == '\0')
				{ MCC_TRACE("br\n"); s->optimize = 1, s->optimize_level = 1; }
			else if (isnum(optarg[0])) { MCC_TRACE("br\n");
				unsigned long got = strtoul(optarg, NULL, 10);
				unsigned lvl = got > 255 ? 255u : (unsigned)got;
				if (opt_level_reject(s, optarg, lvl) < 0)
					{ MCC_TRACE("br\n"); return -1; }
				s->optimize_level = (unsigned char)lvl;
				s->optimize = (unsigned char)(lvl > 3 ? 3 : lvl);
				if (lvl >= MCC_OPT_SEARCH_LEVEL) { MCC_TRACE("br\n");
					s->optimize_search_all = 1;
					if (!s->optimize_search_ticks_set)
						{ MCC_TRACE("br\n"); s->optimize_search_ticks =
									mcc_search_ticks(MCC_OPT_SEARCH_TICKS); }
				}
			} else if (!strcmp(optarg, "s") || !strcmp(optarg, "z")) { MCC_TRACE("br\n");
				s->optimize = 2;
				s->optimize_level = 2;
				s->optimize_size = 1;
			} else if (!strcmp(optarg, "g"))
				{ MCC_TRACE("br\n"); s->optimize = 1, s->optimize_level = 1; }
			else if (!strcmp(optarg, "fast"))
				{ MCC_TRACE("br\n"); s->optimize = 3, s->optimize_level = 3; }
			else { MCC_TRACE("br\n");
				mcc_warning("unsupported optimization level '-O%s'", optarg);
				s->optimize = 1;
				s->optimize_level = 1;
			}
			break;
#if defined MCC_TARGET_MACHO
		case MCC_OPTION_dynamiclib:
			x = MCC_OUTPUT_DLL;
			goto set_output_type;
		case MCC_OPTION_flat_namespace:
			break;
		case MCC_OPTION_two_levelnamespace:
			break;
		case MCC_OPTION_undefined:
			break;
		case MCC_OPTION_install_name:
			mcc_set_str(&s->install_name, optarg);
			break;
		case MCC_OPTION_compatibility_version:
			s->compatibility_version = parse_version(s, optarg);
			break;
		case MCC_OPTION_current_version:
			s->current_version = parse_version(s, optarg);
			;
			break;
		case MCC_OPTION_mmacosx_version_min:
			s->macos_version_min = parse_version(s, optarg);
			break;
		case MCC_OPTION_framework:
			args_parser_add_file(s, optarg, AFF_TYPE_FRAMEWORK | (s->filetype & ~AFF_TYPE_MASK));
			break;
		case MCC_OPTION_F:
			mcc_add_framework_path(s, optarg);
			break;
#endif
		case MCC_OPTION_HELP:
			x = OPT_HELP;
			goto extra_action;
		case MCC_OPTION_HELP2:
			x = OPT_HELP2;
			goto extra_action;
		case MCC_OPTION_print_search_dirs:
			x = OPT_PRINT_DIRS;
			goto extra_action;
		case MCC_OPTION_print_isa:
			s->print_isa = 1;
			continue;
		case MCC_OPTION_print_prog_name:
			mcc_set_str(&s->print_query, optarg);
			x = OPT_PRINT_PROG;
			goto query_action;
		case MCC_OPTION_print_file_name:
			mcc_set_str(&s->print_query, optarg);
			x = OPT_PRINT_FILE;
		query_action:
			if (NULL == argv[0])
				{ MCC_TRACE("br\n"); return -1; }
			--optind;
			*pargc = argc - optind;
			*pargv = argv + optind;
			return x;
		case MCC_OPTION_C:
			s->keep_comments = 1;
			break;
		case MCC_OPTION_impdef:
			x = OPT_IMPDEF;
			goto extra_action;
		case MCC_OPTION_ar:
			x = OPT_AR;
		extra_action:
			if (NULL == argv[0])
				{ MCC_TRACE("br\n"); return -1; }
			if (!empty && x)
				{ MCC_TRACE("br\n"); return mcc_error_noabort("cannot parse %s here", r); }
			--optind;
			*pargc = argc - optind;
			*pargv = argv + optind;
			return x;
		default:
			mcc_warning_c(warn_unsupported)("unsupported option '%s'", r);
			break;
		unsupported_option:
			mcc_warning_c(warn_unsupported_option)("unsupported option '%s'", r);
			break;
		}
		empty = 0;
	}
	/* T-lin-10393: resolve the JIT/GPU resource budgets after all args are parsed
	 * (so --jit-conservative fills only the axes an explicit override left unset).
	 * Slice 1 = percent-of-hardware; `auto` (live-load adaptation) is not yet built. */
	if (s->jit_conservative) { MCC_TRACE("br\n");
		if (s->jit_cpu_budget == -1)
			{ MCC_TRACE("br\n"); s->jit_cpu_budget = 50; }
		if (s->jit_gpu_budget == -1)
			{ MCC_TRACE("br\n"); s->jit_gpu_budget = 50; }
	}
	if (s->jit_cpu_budget >= 0 || s->jit_cpu_budget == -2) { MCC_TRACE("br\n");
		int np = host_nproc(), w;
		if (s->jit_cpu_budget == -2) { MCC_TRACE("br\n");
			/* auto: size to the idle capacity = cores not currently under load. */
			double la = host_loadavg();
			if (la < 0)   /* no live-load source (e.g. Windows): conservative half */
				{ MCC_TRACE("br\n"); w = (np + 1) / 2; }
			else
				{ MCC_TRACE("br\n"); w = (int)((double)np - la + 0.5); }
		} else { MCC_TRACE("br\n");
			w = (np * s->jit_cpu_budget + 50) / 100;   /* percent, rounded */
		}
		if (w < 1)
			{ MCC_TRACE("br\n"); w = 1; }
		if (w > 64)   /* MCCJIT_POOL_MAX */
			{ MCC_TRACE("br\n"); w = 64; }
		s->jit_threads = (unsigned)w;
	}
	if (s->jit_gpu_budget >= 0 || s->jit_gpu_budget == -2)
		{ MCC_TRACE("br\n"); mcc_gpu_vram_budget_pct = s->jit_gpu_budget; }
	if (s->jit_gpu_devices >= 1)
		{ MCC_TRACE("br\n"); mcc_gpu_max_devices = s->jit_gpu_devices; }
	if ((s->jit_conservative || s->jit_cpu_budget >= 0 || s->jit_cpu_budget == -2 ||
			 s->jit_gpu_budget >= 0 || s->jit_gpu_budget == -2 || s->jit_gpu_devices >= 1) &&
			getenv("MCC_JIT_BUDGET_DEBUG"))
		{ MCC_TRACE("br\n"); fprintf(stderr,
			"jit-budget: cpu-workers=%u gpu-vram-pct=%d gpu-devices=%d\n",
			s->jit_threads, mcc_gpu_vram_budget_pct, mcc_gpu_max_devices); }
	if (s->link_optind < s->link_argc)
		{ MCC_TRACE("br\n"); return mcc_error_noabort("argument to '-Wl,%s' is missing", s->link_argv[s->link_optind]); }
	if (run) { MCC_TRACE("br\n");
		if (*run && mcc_set_options(s, run) < 0)
			{ MCC_TRACE("br\n"); return -1; }
		x = 0, r = 0;
		goto extra_action;
	}
	if (!empty)
		{ MCC_TRACE("br\n"); return 0; }
	if (MCC_VTIER(s->verbose) == MCC_V2)
		{ MCC_TRACE("br\n"); return OPT_PRINT_DIRS; }
	if (s->verbose)
		{ MCC_TRACE("br\n"); return OPT_V; }
	return 0;
}

LIBMCCAPI int mcc_set_options(MCCState *s, const char *r) { MCC_TRACE("enter\n");
	char **argv = NULL;
	int argc = 0, ret;
	dynarray_add(&argv, &argc, 0);
	dynarray_split(&argv, &argc, r, 0);
	ret = mcc_parse_args(s, &argc, &argv);
	dynarray_reset(&argv, &argc);
	return ret;
}

LIBMCCAPI int mcc_cache_dir(char *buf, int len) { MCC_TRACE("enter\n");
	return host_cache_dir(buf, len);
}

LIBMCCAPI unsigned long long mcc_intention_hash(MCCState *s) { MCC_TRACE("enter\n");
	(void)s;
	return ast_intention_value();
}

PUB_FUNC void mcc_print_stats(MCCState *s1, unsigned total_time) { MCC_TRACE("enter\n");
	if (!total_time)
		{ MCC_TRACE("br\n"); total_time = 1; }
	fprintf(stderr, "# %d idents, %d lines, %d functions, %u bytes\n"
									"# %0.3f s, %u lines/s, %0.1f MB/s\n",
					total_idents, total_lines, total_funcs, total_bytes,
					(double)total_time / 1000,
					(unsigned)total_lines * 1000 / total_time,
					(double)total_bytes / 1000 / total_time);
	fprintf(stderr, "# text %u, data.rw %u, data.ro %u, bss %u bytes\n",
					s1->total_output[0],
					s1->total_output[1],
					s1->total_output[2],
					s1->total_output[3]);
#if MCC_DIAG
	fprintf(stderr, "# memory usage");
#ifdef MCC_TARGET_IS_HOST
	if (s1->run_size) { MCC_TRACE("br\n");
		Section *s = s1->symtab;
		unsigned ms = s->data_offset + s->link->data_offset + s->hash->data_offset;
		unsigned rs = s1->run_size;
		fprintf(stderr, ": %d to run, %d symbols, %d other,",
						rs, ms, mem_cur_size - rs - ms);
	}
#endif
	fprintf(stderr, " %d max (bytes)\n", mem_max_size);
#endif
}
