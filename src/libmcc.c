#ifndef ONE_SOURCE
#define ONE_SOURCE 1
#endif

#if ONE_SOURCE
#include "mcchost.c"
#include "mccpp.c"
#include "mccgen.c"
#include "mccdbg.c"
#include "mccasm.c"
#include "mccelf.c"
#include "mccrun.c"
#include "mccdis.c"
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

ST_DATA struct MCCState *mcc_state;
HOST_SEM(mcc_compile_sem);
ST_DATA void **stk_data;
ST_DATA int nb_stk_data;
ST_DATA int g_debug;

#if defined MCC_TARGET_PE && defined MCC_IS_NATIVE
static void mcc_add_systemdir(MCCState *s) {
    char buf[1000];
    if (host_system_dir(buf, sizeof buf) == 0)
        mcc_add_library_path(s, buf);
}
#endif

PUB_FUNC void mcc_enter_state(MCCState *s1) {
    if (s1->error_set_jmp_enabled)
        return;
    HOST_SEM_WAIT(&mcc_compile_sem);
    mcc_state = s1;
}

PUB_FUNC void mcc_exit_state(MCCState *s1) {
    if (s1->error_set_jmp_enabled)
        return;
    mcc_state = NULL;
    HOST_SEM_POST(&mcc_compile_sem);
}

ST_FUNC char *pstrcpy(char *buf, size_t buf_size, const char *s) {
    char *q, *q_end;
    int c;

    if (buf_size > 0) {
        q = buf;
        q_end = buf + buf_size - 1;
        while (q < q_end) {
            c = *s++;
            if (c == '\0')
                break;
            *q++ = c;
        }
        *q = '\0';
    }
    return buf;
}

ST_FUNC char *pstrcat(char *buf, size_t buf_size, const char *s) {
    size_t len;
    len = strlen(buf);
    if (len < buf_size)
        pstrcpy(buf + len, buf_size - len, s);
    return buf;
}

ST_FUNC char *pstrncpy(char *out, size_t buf_size, const char *s, size_t num) {
    if (num >= buf_size)
        num = buf_size - 1;
    memcpy(out, s, num);
    out[num] = '\0';
    return out;
}

PUB_FUNC char *mcc_basename(const char *name) {
    char *p = (char *)strchr(name, 0);
    while (p > name && !HOST_IS_DIRSEP(p[-1]))
        --p;
    return p;
}

PUB_FUNC char *mcc_fileextension(const char *name) {
    char *b = mcc_basename(name);
    char *e = strrchr(b, '.');
    return e ? e : strchr(b, 0);
}

ST_FUNC char *mcc_load_text(int fd) {
    int len = lseek(fd, 0, SEEK_END);
    char *buf = load_data(fd, 0, len + 1);
    buf[len] = 0;
    return buf;
}

static void mcc_set_str(char **pp, const char *str) {
    mcc_free(*pp);
    *pp = str ? mcc_strdup(str) : NULL;
}

static void mcc_concat_str(char **pp, const char *str, int sep) {
    int l = *pp ? strlen(*pp) + !!sep : 0;
    *pp = mcc_realloc(*pp, l + strlen(str) + 1);
    if (l && sep)
        ((*pp)[l - 1] = sep);
    strcpy(*pp + l, str);
}

#undef free
#undef realloc

static void *default_reallocator(void *ptr, unsigned long size) {
    void *ptr1;
    if (size == 0) {
        free(ptr);
        ptr1 = NULL;
    } else {
        ptr1 = realloc(ptr, size);
        if (!ptr1) {
            fprintf(stderr, "mcc: memory full\n");
            exit(1);
        }
    }
    return ptr1;
}

ST_FUNC void libc_free(void *ptr) {
    free(ptr);
}

#define free(p) use_mcc_free(p)
#define realloc(p, s) use_mcc_realloc(p, s)

static void *(*reallocator)(void *, unsigned long) = default_reallocator;

LIBMCCAPI void mcc_set_realloc(MCCReallocFunc *my_realloc) {
    reallocator = my_realloc ? my_realloc : default_reallocator;
}

#undef mcc_free
#undef mcc_malloc
#undef mcc_realloc
#undef mcc_mallocz
#undef mcc_strdup

PUB_FUNC void mcc_free(void *ptr) {
    reallocator(ptr, 0);
}

PUB_FUNC void *mcc_malloc(unsigned long size) {
    return reallocator(0, size);
}

PUB_FUNC void *mcc_realloc(void *ptr, unsigned long size) {
    return reallocator(ptr, size);
}

ST_FUNC unsigned long mcc_grow_capacity(unsigned long cur, unsigned long need,
                                        unsigned long min_cap) {
    if (cur < min_cap)
        cur = min_cap;
    while (cur < need)
        cur = cur * 2;
    return cur;
}

ST_FUNC int mcc_uleb128_size(unsigned long long value) {
    int size = 0;
    do {
        value >>= 7;
        size++;
    } while (value != 0);
    return size;
}

ST_FUNC int mcc_sleb128_size(long long value) {
    int size = 0;
    long long end = value >> 63;
    unsigned char last = end & 0x40;
    unsigned char byte;

    do {
        byte = value & 0x7f;
        value >>= 7;
        size++;
    } while (value != end || (byte & 0x40) != last);
    return size;
}

ST_FUNC void mcc_write_uleb128(Section *sec, unsigned long long value) {
    do {
        unsigned char byte = value & 0x7f;

        value >>= 7;
        *(uint8_t *)section_ptr_add(sec, 1) = byte | (value ? 0x80 : 0);
    } while (value != 0);
}

ST_FUNC void mcc_write_sleb128(Section *sec, long long value) {
    long long end = value >> 63;
    unsigned char last = end & 0x40;
    int more;

    do {
        unsigned char byte = value & 0x7f;

        value >>= 7;
        more = value != end || (byte & 0x40) != last;
        *(uint8_t *)section_ptr_add(sec, 1) = byte | (0x80 * more);
    } while (more);
}

PUB_FUNC void *mcc_mallocz(unsigned long size) {
    void *ptr;
    ptr = mcc_malloc(size);
    if (size)
        memset(ptr, 0, size);
    return ptr;
}

PUB_FUNC char *mcc_strdup(const char *str) {
    char *ptr;
    ptr = mcc_malloc(strlen(str) + 1);
    strcpy(ptr, str);
    return ptr;
}

#ifdef MEM_DEBUG

#define MEM_DEBUG_MAGIC1 0xFEEDDEB1
#define MEM_DEBUG_MAGIC2 0xFEEDDEB2
#define MEM_DEBUG_MAGIC3 0xFEEDDEB3
#define MEM_DEBUG_FILE_LEN 40
#define MEM_DEBUG_CHECK3(header) \
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
    char file_name[MEM_DEBUG_FILE_LEN];
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

static mem_debug_header_t *malloc_check(void *ptr, const char *msg) {
    mem_debug_header_t *header = MEM_HEADER_PTR(ptr);
    if (header->magic1 != MEM_DEBUG_MAGIC1 ||
        header->magic2 != MEM_DEBUG_MAGIC2 ||
        read32le(MEM_DEBUG_CHECK3(header)) != MEM_DEBUG_MAGIC3 ||
        header->size == (unsigned)-1) {
        fprintf(stderr, "%s check failed\n", msg);
        if (header->magic1 == MEM_DEBUG_MAGIC1)
            fprintf(stderr, "%s:%u: block allocated here.\n",
                    header->file_name, header->line_num);
        exit(1);
    }
    return header;
}

PUB_FUNC void *mcc_malloc_debug(unsigned long size, const char *file, int line) {
    int ofs;
    mem_debug_header_t *header;
    if (!size)
        return NULL;
    header = mcc_malloc(sizeof(mem_debug_header_t) + size);
    header->magic1 = MEM_DEBUG_MAGIC1;
    header->magic2 = MEM_DEBUG_MAGIC2;
    header->size = size;
    write32le(MEM_DEBUG_CHECK3(header), MEM_DEBUG_MAGIC3);
    header->line_num = line;
    ofs = strlen(file) + 1 - MEM_DEBUG_FILE_LEN;
    strcpy(header->file_name, file + (ofs > 0 ? ofs : 0));
    HOST_SEM_WAIT(&mem_sem);
    header->next = mem_debug_chain;
    header->prev = NULL;
    if (header->next)
        header->next->prev = header;
    mem_debug_chain = header;
    mem_cur_size += size;
    if (mem_cur_size > mem_max_size)
        mem_max_size = mem_cur_size;
    HOST_SEM_POST(&mem_sem);
    return MEM_USER_PTR(header);
}

PUB_FUNC void mcc_free_debug(void *ptr) {
    mem_debug_header_t *header;
    if (!ptr)
        return;
    header = malloc_check(ptr, "mcc_free");
    HOST_SEM_WAIT(&mem_sem);
    mem_cur_size -= header->size;
    header->size = (unsigned)-1;
    if (header->next)
        header->next->prev = header->prev;
    if (header->prev)
        header->prev->next = header->next;
    if (header == mem_debug_chain)
        mem_debug_chain = header->next;
    HOST_SEM_POST(&mem_sem);
    mcc_free(header);
}

PUB_FUNC void *mcc_mallocz_debug(unsigned long size, const char *file, int line) {
    void *ptr;
    ptr = mcc_malloc_debug(size, file, line);
    if (size)
        memset(ptr, 0, size);
    return ptr;
}

PUB_FUNC void *mcc_realloc_debug(void *ptr, unsigned long size, const char *file, int line) {
    mem_debug_header_t *header;
    int mem_debug_chain_update = 0;

    if (!ptr)
        return mcc_malloc_debug(size, file, line);
    if (!size) {
        mcc_free_debug(ptr);
        return NULL;
    }
    header = malloc_check(ptr, "mcc_realloc");
    HOST_SEM_WAIT(&mem_sem);
    mem_cur_size -= header->size;
    mem_debug_chain_update = (header == mem_debug_chain);
    header = mcc_realloc(header, sizeof(mem_debug_header_t) + size);
    header->size = size;
    write32le(MEM_DEBUG_CHECK3(header), MEM_DEBUG_MAGIC3);
    if (header->next)
        header->next->prev = header;
    if (header->prev)
        header->prev->next = header;
    if (mem_debug_chain_update)
        mem_debug_chain = header;
    mem_cur_size += size;
    if (mem_cur_size > mem_max_size)
        mem_max_size = mem_cur_size;
    HOST_SEM_POST(&mem_sem);
    return MEM_USER_PTR(header);
}

PUB_FUNC char *mcc_strdup_debug(const char *str, const char *file, int line) {
    char *ptr;
    ptr = mcc_malloc_debug(strlen(str) + 1, file, line);
    strcpy(ptr, str);
    return ptr;
}

PUB_FUNC void mcc_memcheck(int d) {
    HOST_SEM_WAIT(&mem_sem);
    nb_states += d;
    if (0 == nb_states && mem_cur_size) {
        mem_debug_header_t *header = mem_debug_chain;
        fflush(stdout);
        fprintf(stderr, "MEM_DEBUG: mem_leak= %d bytes, mem_max_size= %d bytes\n",
                mem_cur_size, mem_max_size);
        while (header) {
            fprintf(stderr, "%s:%u: error: %u bytes leaked\n",
                    header->file_name, header->line_num, header->size);
            header = header->next;
        }
        fflush(stderr);
        mem_cur_size = 0;
        mem_max_size = 0;
        mem_debug_chain = NULL;
#if MEM_DEBUG - 0 == 2
        exit(2);
#endif
    }
    HOST_SEM_POST(&mem_sem);
}

#define mcc_free(ptr) mcc_free_debug(ptr)
#define mcc_malloc(size) mcc_malloc_debug(size, __FILE__, __LINE__)
#define mcc_mallocz(size) mcc_mallocz_debug(size, __FILE__, __LINE__)
#define mcc_realloc(ptr, size) mcc_realloc_debug(ptr, size, __FILE__, __LINE__)
#define mcc_strdup(str) mcc_strdup_debug(str, __FILE__, __LINE__)

#endif

ST_FUNC int normalized_PATHCMP(const char *f1, const char *f2) {
    char *p1, *p2;
    int ret = 1;
    if (!!(p1 = host_path_canonical(f1))) {
        if (!!(p2 = host_path_canonical(f2))) {
            ret = HOST_PATHCMP(p1, p2);
            libc_free(p2);
        }
        libc_free(p1);
    }
    return ret;
}

ST_FUNC void dynarray_add(void *ptab, int *nb_ptr, void *data) {
    int nb, nb_alloc;
    void **pp;

    nb = *nb_ptr;
    pp = *(void ***)ptab;
    if ((nb & (nb - 1)) == 0) {
        if (!nb)
            nb_alloc = 1;
        else
            nb_alloc = nb * 2;
        pp = mcc_realloc(pp, nb_alloc * sizeof(void *));
        *(void ***)ptab = pp;
    }
    pp[nb++] = data;
    *nb_ptr = nb;
}

ST_FUNC void dynarray_reset(void *pp, int *n) {
    void **p;
    for (p = *(void ***)pp; *n; ++p, --*n)
        if (*p)
            mcc_free(*p);
    mcc_free(*(void **)pp);
    *(void **)pp = NULL;
}

static void dynarray_split(char ***argv, int *argc, const char *p, int sep) {
    int qot, c;
    CString str;
    for (;;) {
        while (c = (unsigned char)*p, c <= ' ' && c != '\0')
            ++p;
        if (c == '\0')
            break;
        cstr_new(&str);
        qot = 0;
        do {
            ++p;
            if (sep) {
                if (c == sep)
                    break;
            } else {
                if (c == '\\' && (*p == '"' || *p == '\\')) {
                    c = *p++;
                } else if (c == '"') {
                    qot ^= 1;
                    continue;
                } else if (c <= ' ' && !qot) {
                    break;
                }
            }
            cstr_ccat(&str, c);
        } while (c = (unsigned char)*p, c != '\0');
        cstr_ccat(&str, '\0');
        dynarray_add(argv, argc, str.data);
    }
}

static void mcc_split_path(MCCState *s, void *p_ary, int *p_nb_ary, const char *in) {
    const char *p;
    do {
        int c;
        CString str;

        cstr_new(&str);
        for (p = in; c = *p, c != '\0' && c != HOST_PATHSEP[0]; ++p) {
            if (c == '{' && p[1] && p[2] == '}') {
                c = p[1], p += 2;
                if (c == 'B')
                    cstr_cat(&str, s->mcc_lib_path, -1);
                if (c == 'R')
                    cstr_cat(&str, (s->sysroot && s->sysroot[0]) ? s->sysroot : CONFIG_SYSROOT, -1);
                if (c == 'f' && file) {
                    const char *f = file->true_filename;
                    const char *b = mcc_basename(f);
                    if (b > f)
                        cstr_cat(&str, f, b - f - 1);
                    else
                        cstr_cat(&str, ".", 1);
                }
            } else {
                cstr_ccat(&str, c);
            }
        }
        if (str.size) {
            cstr_ccat(&str, '\0');
            dynarray_add(p_ary, p_nb_ary, str.data);
        }
        in = p + 1;
    } while (*p);
}

#define WARN_ON 1
#define WARN_ERR 2
#define WARN_NOE 4

enum { ERROR_WARN,
       ERROR_NOABORT,
       ERROR_ERROR };

static void error1(int mode, const char *fmt, va_list ap) {
    BufferedFile **pf, *f;
    MCCState *s1 = mcc_state;
    CString cs;
    int line = 0;

    mcc_exit_state(s1);

    if (mode == ERROR_WARN) {
        int wopt = -1;
        if (s1->warn_num) {
            wopt = *(&s1->warn_none + s1->warn_num);
            s1->warn_num = 0;
        }
        if (s1->error_set_jmp_enabled) {
            BufferedFile *wf;
            for (wf = file; wf && wf->filename[0] == ':'; wf = wf->prev)
                ;
            if (wf && wf->system_header)
                return;
        }
        if (s1->warn_error)
            mode = ERROR_ERROR;
        if (wopt >= 0) {
            if (0 == (wopt & WARN_ON))
                return;
            if (wopt & WARN_ERR)
                mode = ERROR_ERROR;
            if (wopt & WARN_NOE)
                mode = ERROR_WARN;
        }
        if (s1->warn_none)
            return;
    }

    cstr_new(&cs);
    if (fmt[0] == '%' && fmt[1] == 'i' && fmt[2] == ':')
        line = va_arg(ap, int), fmt += 3;
    f = NULL;
    if (s1->error_set_jmp_enabled) {
        for (f = file; f && f->filename[0] == ':'; f = f->prev)
            ;
    }
    if (f) {
        for (pf = s1->include_stack; pf < s1->include_stack_ptr; pf++)
            cstr_printf(&cs, "In file included from %s:%d:\n",
                        (*pf)->filename, (*pf)->line_num - 1);
        if (0 == line)
            line = f->line_num - ((tok_flags & TOK_FLAG_BOL) && !macro_ptr);
        cstr_printf(&cs, "%s:%d: ", f->filename, line);
    } else if (s1->current_filename) {
        cstr_printf(&cs, "%s: ", s1->current_filename);
    } else {
        cstr_printf(&cs, "mcc: ");
    }
    cstr_printf(&cs, mode == ERROR_WARN ? "warning: " : "error: ");
    if (pp_expr > 1)
        pp_error(&cs);
    else
        cstr_vprintf(&cs, fmt, ap);
    if (!s1->error_func) {
        if (s1 && s1->output_type == MCC_OUTPUT_PREPROCESS && s1->ppfp == stdout)
            printf("\n");
        fflush(stdout);
        fprintf(stderr, "%s\n", (char *)cs.data);
        fflush(stderr);
    } else {
        s1->error_func(s1->error_opaque, (char *)cs.data);
    }
    cstr_free(&cs);
    if (mode != ERROR_WARN)
        s1->nb_errors++;
    if (mode == ERROR_NOABORT && s1->error_set_jmp_enabled && (s1->warn_fatal_errors || (s1->max_errors && s1->nb_errors >= s1->max_errors)))
        mode = ERROR_ERROR;
    if (mode == ERROR_ERROR && s1->error_set_jmp_enabled) {
        while (nb_stk_data)
            mcc_free(*(void **)stk_data[--nb_stk_data]);
        longjmp(s1->error_jmp_buf, 1);
    }
}

LIBMCCAPI void mcc_set_error_func(MCCState *s, void *error_opaque, MCCErrorFunc *error_func) {
    s->error_opaque = error_opaque;
    s->error_func = error_func;
}

PUB_FUNC int _mcc_error_noabort(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    error1(ERROR_NOABORT, fmt, ap);
    va_end(ap);
    return -1;
}

#undef _mcc_error
PUB_FUNC void _mcc_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    error1(ERROR_ERROR, fmt, ap);
    exit(1);
}
#define _mcc_error use_mcc_error_noabort

PUB_FUNC void _mcc_warning(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    error1(ERROR_WARN, fmt, ap);
    va_end(ap);
}

ST_FUNC void mcc_open_bf(MCCState *s1, const char *filename, int initlen) {
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

ST_FUNC void mcc_close(void) {
    MCCState *s1 = mcc_state;
    BufferedFile *bf = file;
    if (bf->fd > 0) {
        close(bf->fd);
        total_lines += bf->line_num - 1;
    }
    if (bf->true_filename != bf->filename)
        mcc_free(bf->true_filename);
    file = bf->prev;
    tok_flags = bf->prev_tok_flags;
    mcc_free(bf);
}

static int _mcc_open(MCCState *s1, const char *filename) {
    int fd;
    if (strcmp(filename, "-") == 0)
        fd = 0, filename = "<stdin>";
    else
        fd = open(filename, O_RDONLY | O_BINARY);
    if ((s1->verbose == 2 && fd >= 0) || s1->verbose == 3)
        printf("%s %*s%s\n", fd < 0 ? "nf" : "->",
               (int)(s1->include_stack_ptr - s1->include_stack), "", filename);
    return fd;
}

ST_FUNC int mcc_open(MCCState *s1, const char *filename) {
    int fd = _mcc_open(s1, filename);
    if (fd < 0)
        return -1;
    mcc_open_bf(s1, filename, 0);
    file->fd = fd;
    return 0;
}

static int mcc_compile(MCCState *s1, int filetype, const char *str, int fd) {

    mcc_enter_state(s1);
    s1->error_set_jmp_enabled = 1;

    if (setjmp(s1->error_jmp_buf) == 0) {

        if (fd == -1) {
            int len = strlen(str);
            mcc_open_bf(s1, "<string>", len);
            memcpy(file->buffer, str, len);
        } else {
            mcc_open_bf(s1, str, 0);
            file->fd = fd;
        }

        preprocess_start(s1, filetype);
        mccgen_init(s1);

        if (s1->output_type == MCC_OUTPUT_PREPROCESS) {
            mcc_preprocess(s1);
        } else {
            mccelf_begin_file(s1);
            if (filetype & (AFF_TYPE_ASM | AFF_TYPE_ASMPP)) {
#ifdef CONFIG_MCC_ASM
                mcc_assemble(s1, !!(filetype & AFF_TYPE_ASMPP));
#else
                mcc_error_noabort("assembler source not supported (built without CONFIG_MCC_ASM)");
#endif
            } else {
                mccgen_compile(s1);
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

LIBMCCAPI int mcc_compile_string(MCCState *s, const char *str) {
    return mcc_compile(s, s->filetype, str, -1);
}

LIBMCCAPI void mcc_define_symbol(MCCState *s1, const char *sym, const char *value) {
    const char *eq;
    if (NULL == (eq = strchr(sym, '=')))
        eq = strchr(sym, 0);
    if (NULL == value)
        value = *eq ? eq + 1 : "1";
    cstr_printf(&s1->cmdline_defs, "#define %.*s %s\n", (int)(eq - sym), sym, value);
}

LIBMCCAPI void mcc_undefine_symbol(MCCState *s1, const char *sym) {
    cstr_printf(&s1->cmdline_defs, "#undef %s\n", sym);
}

#if defined CONFIG_MCC_AUTO_MCCDIR && MCC_HOST_POSIX
#include <sys/stat.h>
static char auto_mccdir_buf[1024];
static const char *mcc_auto_mccdir(void) {
    char exe[1024], probe[1024];
    struct stat st;
    char *p;
    if (host_exe_path(exe, sizeof exe) <= 0)
        return CONFIG_MCCDIR;
    p = mcc_basename(exe);
    if (p > exe)
        --p;
    *p = 0;
    pstrcpy(probe, sizeof probe, exe);
    pstrcat(probe, sizeof probe, "/include");
    if (stat(probe, &st) != 0 || !S_ISDIR(st.st_mode))
        return CONFIG_MCCDIR;
    pstrcpy(auto_mccdir_buf, sizeof auto_mccdir_buf, exe);
    return auto_mccdir_buf;
}
#define CONFIG_MCCDIR_DEFAULT mcc_auto_mccdir()
#else
#define CONFIG_MCCDIR_DEFAULT CONFIG_MCCDIR
#endif

LIBMCCAPI MCCState *mcc_new(void) {
    MCCState *s;

    s = mcc_mallocz(sizeof(MCCState));
#ifdef MEM_DEBUG
    mcc_memcheck(1);
#endif

#undef gnu_ext
    s->gnu_ext = 1;
    s->mcc_ext = 1;
    s->nocommon = 1;
    s->dollars_in_identifiers = 1;
    s->cversion = 201112;
    s->pie = -1;
#if defined CONFIG_MCC_PIC
    s->pic = 2;
#endif
    s->warn_implicit_function_declaration = 1;
    s->warn_discarded_qualifiers = 1;
    s->warn_sequence_point = 1;
    s->warn_implicit_int = 1;
    s->warn_varargs = 1;
    s->ms_extensions = 1;
    s->unwind_tables = 1;

#ifdef CHAR_IS_UNSIGNED
    s->char_is_unsigned = 1;
#endif
#ifdef MCC_TARGET_I386
    s->seg_size = 32;
#endif
#if defined MCC_TARGET_MACHO
    s->leading_underscore = 1;
#endif
#ifdef MCC_ARM_HARDFLOAT
    s->float_abi = ARM_HARD_FLOAT;
#endif
#ifdef CONFIG_NEW_DTAGS
    s->enable_new_dtags = 1;
#endif
    s->ppfp = stdout;
    s->include_stack_ptr = s->include_stack;

    mcc_set_lib_path(s, CONFIG_MCCDIR_DEFAULT);
#ifdef CONFIG_MCC_SWITCHES
    mcc_set_options(s, CONFIG_MCC_SWITCHES);
#endif
    return s;
}

LIBMCCAPI void mcc_delete(MCCState *s1) {
    mccelf_delete(s1);

    dynarray_reset(&s1->library_paths, &s1->nb_library_paths);
    dynarray_reset(&s1->crt_paths, &s1->nb_crt_paths);

    dynarray_reset(&s1->include_paths, &s1->nb_include_paths);
    dynarray_reset(&s1->sysinclude_paths, &s1->nb_sysinclude_paths);
    dynarray_reset(&s1->iquote_paths, &s1->nb_iquote_paths);
    dynarray_reset(&s1->afterinc_paths, &s1->nb_afterinc_paths);

    mcc_free(s1->mcc_lib_path);
    mcc_free(s1->soname);
    mcc_free(s1->sysroot);
    mcc_free(s1->rpath);
    mcc_free(s1->elfint);
    mcc_free(s1->elf_entryname);
    mcc_free(s1->init_symbol);
    mcc_free(s1->fini_symbol);
    mcc_free(s1->mapfile);
    mcc_free(s1->outfile);
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
    mcc_free(s1->dState);
#ifdef MCC_IS_NATIVE
    mcc_run_free(s1);
#endif
    dynarray_reset(&s1->loaded_dlls, &s1->nb_loaded_dlls);
    mcc_free(s1);
#ifdef MEM_DEBUG
    mcc_memcheck(-1);
#endif
}

LIBMCCAPI int mcc_set_output_type(MCCState *s, int output_type) {
    if (output_type == MCC_OUTPUT_EXE) {
        int pie = s->pie;
        if (pie < 0)
#ifdef CONFIG_MCC_PIE
            pie = 1;
#else
            pie = 0;
#endif
        if (pie)
            output_type |= MCC_OUTPUT_DYN;
    }
    s->output_type = output_type;

    if (!s->nostdinc) {
        mcc_add_sysinclude_path(s, CONFIG_MCC_SYSINCLUDEPATHS);
#if defined MCC_TARGET_MACHO && defined MCC_IS_NATIVE
        mcc_add_macos_sdkincludepath(s);
#endif
    }

    if (output_type == MCC_OUTPUT_PREPROCESS) {
        s->do_debug = 0;
        return 0;
    }

    mccelf_new(s);

    if (output_type == MCC_OUTPUT_OBJ || output_type == MCC_OUTPUT_ASM) {

        s->output_format = MCC_OUTPUT_FORMAT_ELF;
        return 0;
    }

    if (!s->nostdlib_paths)
        mcc_add_library_path(s, CONFIG_MCC_LIBPATHS);

#ifdef MCC_TARGET_PE
#ifdef MCC_IS_NATIVE
    mcc_add_systemdir(s);
#endif

#elif defined MCC_TARGET_MACHO
#ifdef MCC_IS_NATIVE
    mcc_add_macos_sdkpath(s);
#endif

#else
    mcc_split_path(s, &s->crt_paths, &s->nb_crt_paths, CONFIG_MCC_CRTPREFIX);
    if (output_type != MCC_OUTPUT_MEMORY && !s->nostdlib)
        mccelf_add_crtbegin(s);
#endif
    return s->nb_errors ? -1 : 0;
}

LIBMCCAPI int mcc_add_include_path(MCCState *s, const char *pathname) {
    mcc_split_path(s, &s->include_paths, &s->nb_include_paths, pathname);
    return 0;
}

LIBMCCAPI int mcc_add_sysinclude_path(MCCState *s, const char *pathname) {
    mcc_split_path(s, &s->sysinclude_paths, &s->nb_sysinclude_paths, pathname);
    return 0;
}

LIBMCCAPI int mcc_add_iquote_path(MCCState *s, const char *pathname) {
    mcc_split_path(s, &s->iquote_paths, &s->nb_iquote_paths, pathname);
    return 0;
}

LIBMCCAPI int mcc_add_afterinc_path(MCCState *s, const char *pathname) {
    mcc_split_path(s, &s->afterinc_paths, &s->nb_afterinc_paths, pathname);
    return 0;
}

LIBMCCAPI int mcc_add_library_path(MCCState *s, const char *pathname) {
    mcc_split_path(s, &s->library_paths, &s->nb_library_paths, pathname);
    return 0;
}

LIBMCCAPI void mcc_set_lib_path(MCCState *s, const char *path) {
    mcc_set_str(&s->mcc_lib_path, path);
}

ST_FUNC DLLReference *mcc_add_dllref(MCCState *s1, const char *dllname, int level) {
    DLLReference *ref = NULL;
    for (int i = 0; i < s1->nb_loaded_dlls; i++)
        if (0 == strcmp(s1->loaded_dlls[i]->name, dllname)) {
            ref = s1->loaded_dlls[i];
            break;
        }
    if (level == -1)
        return ref;
    if (ref) {
        if (level < ref->level)
            ref->level = level;
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

static int mcc_add_binary(MCCState *s1, int flags, const char *filename, int fd) {
    ElfW(Ehdr) ehdr;
    int obj_type;
    const char *saved_filename = s1->current_filename;
    int ret = 0;

    s1->current_filename = filename;
    obj_type = mcc_object_type(fd, &ehdr);
    lseek(fd, 0, SEEK_SET);

    switch (obj_type) {

    case AFF_BINTYPE_REL:
        ret = mcc_load_object_file(s1, fd, 0);
        break;

    case AFF_BINTYPE_AR:
        ret = mcc_load_archive(s1, fd, !(flags & AFF_WHOLE_ARCHIVE));
        break;

#if defined MCC_TARGET_UNIX
    case AFF_BINTYPE_DYN:
        if (s1->output_type == MCC_OUTPUT_MEMORY) {
#ifdef MCC_IS_NATIVE
#ifdef CONFIG_MCC_STATIC
            (void)filename;
#else
            void *dl = host_dlopen(filename);
            if (dl)
                mcc_add_dllref(s1, filename, 0)->handle = dl;
            else
                ret = FILE_NOT_RECOGNIZED;
#endif
#endif
        } else
            ret = mcc_load_dll(s1, fd, filename, (flags & AFF_REFERENCED_DLL) != 0);
        break;

    default:
        ret = mcc_load_ldscript(s1, fd);
        break;

#elif defined MCC_TARGET_MACHO
    case AFF_BINTYPE_DYN:
    case_dyn_or_tbd:
        if (s1->output_type == MCC_OUTPUT_MEMORY) {
#ifdef MCC_IS_NATIVE
            void *dl;
            const char *soname = filename;
            char *tmp = 0;
            if (obj_type != AFF_BINTYPE_DYN) {
                tmp = macho_tbd_soname(fd);
                if (tmp)
                    soname = tmp;
            }
            dl = host_dlopen(soname);
            if (dl)
                mcc_add_dllref(s1, soname, 0)->handle = dl;
            else
                ret = FILE_NOT_RECOGNIZED;
            mcc_free(tmp);
#endif
        } else if (obj_type == AFF_BINTYPE_DYN) {
            ret = macho_load_dll(s1, fd, filename, (flags & AFF_REFERENCED_DLL) != 0);
        } else {
            ret = macho_load_tbd(s1, fd, filename, (flags & AFF_REFERENCED_DLL) != 0);
        }
        if (ret)
            ret = FILE_NOT_RECOGNIZED;
        break;

    default: {
        const char *ext = mcc_fileextension(filename);
        if (!strcmp(ext, ".tbd"))
            goto case_dyn_or_tbd;
        if (!strcmp(ext, ".dylib")) {
            obj_type = AFF_BINTYPE_DYN;
            goto case_dyn_or_tbd;
        }
        ret = FILE_NOT_RECOGNIZED;
        break;
    }

#elif defined MCC_TARGET_PE
    default:
        if (pe_load_file(s1, fd, filename))
            ret = FILE_NOT_RECOGNIZED;
        break;
#endif
    }

    close(fd);
    s1->current_filename = saved_filename;
    if (ret == FILE_NOT_RECOGNIZED)
        return mcc_error_noabort("%s: unrecognized file type", filename);
    return ret;
}

#if defined TARGETOS_OpenBSD && MCC_HOST_POSIX
#include <glob.h>
static int mcc_glob_so(MCCState *s1, const char *pattern, char *buf, int size) {
    const char *star;
    glob_t g;
    char *p;
    int i, v, v1, v2, v3;

    star = strchr(pattern, '*');
    if (!star || glob(pattern, 0, NULL, &g))
        return -1;
    for (v = -1, i = 0; i < g.gl_pathc; ++i) {
        p = g.gl_pathv[i];
        if (2 != sscanf(p + (star - pattern), "%d.%d.%d", &v1, &v2, &v3))
            continue;
        if ((v1 = v1 * 1000 + v2) > v)
            v = v1, pstrcpy(buf, size, p);
    }
    globfree(&g);
    return v;
}
#endif

static int guess_filetype(const char *filename) {
    int filetype = 0;
    if (1) {
        const char *ext = mcc_fileextension(filename);
        if (ext[0]) {
            ext++;
            if (!strcmp(ext, "S"))
                filetype = AFF_TYPE_ASMPP;
            else if (!strcmp(ext, "s"))
                filetype = AFF_TYPE_ASM;
            else if (!HOST_PATHCMP(ext, "c") || !HOST_PATHCMP(ext, "h") || !HOST_PATHCMP(ext, "i"))
                filetype = AFF_TYPE_C;
            else
                filetype |= AFF_TYPE_BIN;
        } else {
            filetype = AFF_TYPE_C;
        }
    }
    return filetype;
}

ST_FUNC int mcc_add_file_internal(MCCState *s1, const char *filename, int flags) {
    int fd;

#if defined TARGETOS_OpenBSD && MCC_HOST_POSIX
    char buf[1024];
    if (mcc_glob_so(s1, filename, buf, sizeof buf) >= 0)
        filename = buf;
#endif

    if (0 == (flags & AFF_TYPE_MASK))
        flags |= guess_filetype(filename);

    if (s1->output_type == MCC_OUTPUT_PREPROCESS && (flags & AFF_TYPE_BIN))
        return 0;

    fd = _mcc_open(s1, filename);
    if (fd < 0) {
        if (flags & AFF_PRINT_ERROR)
            mcc_error_noabort("file '%s' not found", filename);
        return FILE_NOT_FOUND;
    }

    if (flags & AFF_TYPE_BIN)
        return mcc_add_binary(s1, flags, filename, fd);

    dynarray_add(&s1->target_deps, &s1->nb_target_deps, mcc_strdup(filename));
    return mcc_compile(s1, flags, filename, fd);
}

LIBMCCAPI int mcc_add_file(MCCState *s, const char *filename) {
    return mcc_add_file_internal(s, filename, s->filetype | AFF_PRINT_ERROR);
}

static int mcc_add_library_internal(MCCState *s1, const char *fmt,
                                    const char *filename, int flags, char **paths, int nb_paths) {
    char buf[1024];
    int ret;

    for (int i = 0; i < nb_paths; i++) {
        snprintf(buf, sizeof(buf), fmt, paths[i], filename);
        ret = mcc_add_file_internal(s1, buf, flags & ~AFF_PRINT_ERROR);
        if (ret != FILE_NOT_FOUND)
            return ret;
    }
    if (flags & AFF_PRINT_ERROR)
        mcc_error_noabort("%s '%s' not found",
                          flags & AFF_TYPE_LIB ? "library" : "file", filename);
    return FILE_NOT_FOUND;
}

ST_FUNC int mcc_add_dll(MCCState *s, const char *filename, int flags) {
    return mcc_add_library_internal(s, "%s/%s", filename, flags,
                                    s->library_paths, s->nb_library_paths);
}

ST_FUNC int mcc_add_support(MCCState *s1, const char *filename) {
    char buf[100];
    if (CONFIG_MCC_CROSSPREFIX[0])
        filename = strcat(strcpy(buf, CONFIG_MCC_CROSSPREFIX), filename);
    return mcc_add_dll(s1, filename, AFF_PRINT_ERROR);
}

#ifdef MCC_TARGET_UNIX
ST_FUNC int mcc_add_crt(MCCState *s1, const char *filename) {
    int ret = mcc_add_library_internal(s1, "%s/%s",
                                       filename, 0, s1->crt_paths, s1->nb_crt_paths);
    if (ret == FILE_NOT_FOUND)
        ret = mcc_add_library_internal(s1, "%s/%s",
                                       filename, AFF_PRINT_ERROR, s1->library_paths, s1->nb_library_paths);
    return ret;
}
#endif

LIBMCCAPI int mcc_add_library(MCCState *s, const char *libraryname) {
    static const char *const libs[] = {
#if defined MCC_TARGET_PE
        "%s/%s.def", "%s/lib%s.def", "%s/%s.dll", "%s/lib%s.dll",
#elif defined MCC_TARGET_MACHO
        "%s/lib%s.dylib", "%s/lib%s.tbd",
#elif defined TARGETOS_OpenBSD
        "%s/lib%s.so.*",
#else
        "%s/lib%s.so",
#endif
        "%s/lib%s.a",
        NULL};
    int flags = AFF_TYPE_LIB | (s->filetype & AFF_WHOLE_ARCHIVE);
#if defined MCC_TARGET_PE
    if (!strcmp(libraryname, "m"))
        return 0;
#endif
    if (*libraryname == ':') {
        libraryname++;
    } else {
        const char *const *pp = libs;
        if (s->static_link)
            pp += sizeof(libs) / sizeof(*libs) - 2;
        while (*pp) {
            int ret = mcc_add_library_internal(s, *pp,
                                               libraryname, flags, s->library_paths, s->nb_library_paths);
            if (ret != FILE_NOT_FOUND)
                return ret;
            ++pp;
        }
    }
    return mcc_add_dll(s, libraryname, flags | AFF_PRINT_ERROR);
}

ST_FUNC void mcc_add_pragma_libs(MCCState *s1) {
    for (int i = 0; i < s1->nb_pragma_libs; i++)
        mcc_add_library(s1, s1->pragma_libs[i]);
}

static int strstart(const char *val, const char **str) {
    const char *p, *q;
    p = *str;
    q = val;
    while (*q) {
        if (*p != *q)
            return 0;
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

static int link_option(struct lopt *o, const char *q) {
    const char *p;
    int c;
redo:
    p = o->opt;
    if (*p++ != '-')
        return 0;
    if (*p == '-')
        p++;
    while ((c = *q) == *p) {
        if (c == '\0')
            goto succ;
        ++p;
        if (c == '=')
            goto succ;
        ++q;
    }
    if (*p == '\0') {
        if (c == '|')
            goto succ;
        if (c == '=' || c == ':') {
            if (o->s->link_optind + 1 < o->s->link_argc) {
                p = o->s->link_argv[++o->s->link_optind];
                goto succ;
            }
            o->match = 1;
            return 0;
        }
    } else if (c == ':')
        goto succ;
    while (*q)
        if (*q++ == '|')
            goto redo;
    return 0;
succ:
    o->arg = p;
    return 1;
}

static void args_parser_add_file(MCCState *s, const char *filename, int filetype);

#ifdef MCC_TARGET_PE
static void mcc_pe_set_dll_characteristics(MCCState *s, unsigned flags) {
    s->pe_dll_characteristics |= flags;
    s->pe_dll_characteristics_clear &= ~flags;
}

static void mcc_pe_clear_dll_characteristics(MCCState *s, unsigned flags) {
    s->pe_dll_characteristics &= ~flags;
    s->pe_dll_characteristics_clear |= flags;
}
#endif

static int mcc_set_linker(MCCState *s, const char *optarg) {
    MCCState *s1 = s;

    dynarray_split(&s1->link_argv, &s1->link_argc, optarg, ',');

    while (s->link_optind < s->link_argc) {
        char *end = NULL;
        int ignoring = 0;
        struct lopt o = {0};
        o.s = s;
        o.opt = s->link_argv[s->link_optind];

        if (link_option(&o, "Bsymbolic")) {
            s->symbolic = 1;
        } else if (link_option(&o, "nostdlib")) {
            s->nostdlib_paths = 1;
        } else if (link_option(&o, "e=|entry=")) {
            mcc_set_str(&s->elf_entryname, o.arg);
        } else if (link_option(&o, "image-base=|Ttext=")) {
            s->text_addr = strtoull(o.arg, &end, 16);
            s->has_text_addr = 1;
        } else if (link_option(&o, "init=")) {
            mcc_set_str(&s->init_symbol, o.arg);
            ignoring = 1;
        } else if (link_option(&o, "fini=")) {
            mcc_set_str(&s->fini_symbol, o.arg);
            ignoring = 1;
        } else if (link_option(&o, "Map=")) {
            mcc_set_str(&s->mapfile, o.arg);
            ignoring = 1;
        } else if (link_option(&o, "oformat=")) {
#if defined MCC_TARGET_PE
            if (0 == strncmp("pe-", o.arg, 3))
#elif PTR_SIZE == 8
            if (0 == strncmp("elf64-", o.arg, 6))
#else
            if (0 == strncmp("elf32-", o.arg, 6))
#endif
                s->output_format = MCC_OUTPUT_FORMAT_ELF;
            else if (0 == strcmp("binary", o.arg))
                s->output_format = MCC_OUTPUT_FORMAT_BINARY;
            else
                goto err;
        } else if (link_option(&o, "export-all-symbols|export-dynamic|E")) {
            s->rdynamic = 1;
        } else if (link_option(&o, "rpath=")) {
            mcc_concat_str(&s->rpath, o.arg, ':');
        } else if (link_option(&o, "dynamic-linker=|I:")) {
            mcc_set_str(&s->elfint, o.arg);
        } else if (link_option(&o, "enable-new-dtags")) {
            s->enable_new_dtags = 1;
        } else if (link_option(&o, "disable-new-dtags")) {
            s->enable_new_dtags = 0;
        } else if (link_option(&o, "section-alignment=")) {
            s->section_align = strtoul(o.arg, &end, 16);
        } else if (link_option(&o, "soname=|install_name=")) {
            mcc_set_str(&s->soname, o.arg);
        } else if (link_option(&o, "whole-archive")) {
            s->filetype |= AFF_WHOLE_ARCHIVE;
        } else if (link_option(&o, "no-whole-archive")) {
            s->filetype &= ~AFF_WHOLE_ARCHIVE;
        } else if (link_option(&o, "znodelete")) {
            s->znodelete = 1;
#ifdef MCC_TARGET_PE
        } else if (link_option(&o, "large-address-aware")) {
            s->pe_characteristics |= PE_IMAGE_FILE_LARGE_ADDRESS_AWARE;
        } else if (link_option(&o, "dynamicbase")) {
            mcc_pe_set_dll_characteristics(s, PE_DLLCHARACTERISTICS_DYNAMIC_BASE);
        } else if (link_option(&o, "disable-dynamicbase|no-dynamicbase")) {
            mcc_pe_clear_dll_characteristics(s,
                                             PE_DLLCHARACTERISTICS_DYNAMIC_BASE |
                                                 PE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA);
        } else if (link_option(&o, "nxcompat")) {
            mcc_pe_set_dll_characteristics(s, PE_DLLCHARACTERISTICS_NX_COMPAT);
        } else if (link_option(&o, "disable-nxcompat|no-nxcompat")) {
            mcc_pe_clear_dll_characteristics(s, PE_DLLCHARACTERISTICS_NX_COMPAT);
        } else if (link_option(&o, "high-entropy-va")) {
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
            mcc_pe_set_dll_characteristics(s,
                                           PE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA |
                                               PE_DLLCHARACTERISTICS_DYNAMIC_BASE);
#else
            goto err;
#endif
        } else if (link_option(&o, "disable-high-entropy-va|no-high-entropy-va")) {
            mcc_pe_clear_dll_characteristics(s, PE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA);
        } else if (link_option(&o, "tsaware")) {
            mcc_pe_set_dll_characteristics(s, PE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE);
        } else if (link_option(&o, "disable-tsaware|no-tsaware")) {
            mcc_pe_clear_dll_characteristics(s, PE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE);
        } else if (link_option(&o, "file-alignment=")) {
            s->pe_file_align = strtoul(o.arg, &end, 16);
        } else if (link_option(&o, "stack=")) {
            s->pe_stack_size = strtoul(o.arg, &end, 10);
        } else if (link_option(&o, "subsystem=")) {
            if (pe_setsubsy(s, o.arg) < 0)
                goto err;
#elif defined MCC_TARGET_MACHO
        } else if (link_option(&o, "all_load")) {
            s->filetype |= AFF_WHOLE_ARCHIVE;
        } else if (link_option(&o, "force_load=")) {
            args_parser_add_file(s, o.arg, AFF_TYPE_LIB | AFF_WHOLE_ARCHIVE);
        } else if (link_option(&o, "single_module")) {
            ignoring = 1;
#endif
        } else if (link_option(&o, "as-needed")) {
            ignoring = 1;
        } else if (link_option(&o, "O")) {
            ignoring = 1;
        } else if (link_option(&o, "z=")) {
            ignoring = 1;
        } else if (link_option(&o, "L:")) {
            mcc_add_library_path(s, o.arg);
        } else if (link_option(&o, "l:")) {
            args_parser_add_file(s, o.arg, AFF_TYPE_LIB | (s->filetype & ~AFF_TYPE_MASK));
        } else if (o.match) {
            return 0;
        } else {
        err:
            return mcc_error_noabort("unsupported linker option '%s'", o.opt);
        }
        if (ignoring)
            mcc_warning_c(warn_unsupported)("unsupported linker option '%s'", o.opt);
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
    MCC_OPTION_c,
    MCC_OPTION_dumpmachine,
    MCC_OPTION_dumpversion,
    MCC_OPTION_d,
    MCC_OPTION_static,
    MCC_OPTION_std,
    MCC_OPTION_shared,
    MCC_OPTION_soname,
    MCC_OPTION_o,
    MCC_OPTION_r,
    MCC_OPTION_Wl,
    MCC_OPTION_Wp,
    MCC_OPTION_W,
    MCC_OPTION_O,
    MCC_OPTION_mfloat_abi,
    MCC_OPTION_m,
    MCC_OPTION_f,
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
    MCC_OPTION_MP,
    MCC_OPTION_MT,
    MCC_OPTION_MQ,
    MCC_OPTION_S,
    MCC_OPTION_x,
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
    {"g", MCC_OPTION_g, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
#ifdef MCC_TARGET_MACHO
    {"compatibility_version", MCC_OPTION_compatibility_version, MCC_OPTION_HAS_ARG},
    {"current_version", MCC_OPTION_current_version, MCC_OPTION_HAS_ARG},
    {"dynamiclib", MCC_OPTION_dynamiclib, 0},
    {"flat_namespace", MCC_OPTION_flat_namespace, 0},
    {"install_name", MCC_OPTION_install_name, MCC_OPTION_HAS_ARG},
    {"mmacosx-version-min=", MCC_OPTION_mmacosx_version_min, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
    {"mmacos-version-min=", MCC_OPTION_mmacosx_version_min, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
    {"two_levelnamespace", MCC_OPTION_two_levelnamespace, 0},
    {"undefined", MCC_OPTION_undefined, MCC_OPTION_HAS_ARG},
#endif
    {"c", MCC_OPTION_c, 0},
    {"dumpmachine", MCC_OPTION_dumpmachine, 0},
    {"dumpversion", MCC_OPTION_dumpversion, 0},
    {"d", MCC_OPTION_d, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
    {"static", MCC_OPTION_static, 0},
    {"std", MCC_OPTION_std, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
    {"shared", MCC_OPTION_shared, 0},
    {"soname", MCC_OPTION_soname, MCC_OPTION_HAS_ARG},
    {"o", MCC_OPTION_o, MCC_OPTION_HAS_ARG},
    {"pthread", MCC_OPTION_pthread, 0},
    {"run", MCC_OPTION_run, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
    {"rstdin", MCC_OPTION_rstdin, MCC_OPTION_HAS_ARG},
    {"rdynamic", MCC_OPTION_rdynamic, 0},
    {"r", MCC_OPTION_r, 0},
    {"Wl,", MCC_OPTION_Wl, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
    {"Wp,", MCC_OPTION_Wp, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
    {"W", MCC_OPTION_W, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
    {"O", MCC_OPTION_O, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
#ifdef MCC_TARGET_ARM
    {"mfloat-abi", MCC_OPTION_mfloat_abi, MCC_OPTION_HAS_ARG},
#endif
    {"m", MCC_OPTION_m, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
    {"f", MCC_OPTION_f, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
    {"isystem", MCC_OPTION_isystem, MCC_OPTION_HAS_ARG},
    {"iquote", MCC_OPTION_iquote, MCC_OPTION_HAS_ARG},
    {"idirafter", MCC_OPTION_idirafter, MCC_OPTION_HAS_ARG},
    {"imacros", MCC_OPTION_imacros, MCC_OPTION_HAS_ARG},
    {"-sysroot", MCC_OPTION_sysroot, MCC_OPTION_HAS_ARG},
    {"isysroot", MCC_OPTION_isysroot, MCC_OPTION_HAS_ARG},
    {"include", MCC_OPTION_include, MCC_OPTION_HAS_ARG},
    {"nostdinc", MCC_OPTION_nostdinc, 0},
    {"trigraphs", MCC_OPTION_trigraphs, 0},
    {"nostdlib", MCC_OPTION_nostdlib, 0},
    {"print-search-dirs", MCC_OPTION_print_search_dirs, 0},
    {"w", MCC_OPTION_w, 0},
    {"E", MCC_OPTION_E, 0},
    {"M", MCC_OPTION_M, 0},
    {"MM", MCC_OPTION_MM, 0},
    {"MD", MCC_OPTION_MD, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
    {"MMD", MCC_OPTION_MMD, MCC_OPTION_HAS_ARG | MCC_OPTION_NOSEP},
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
    {"C", 0, 0},
    {"-param", 0, MCC_OPTION_HAS_ARG},
    {"pedantic", MCC_OPTION_pedantic, 0},
    {"pedantic-errors", MCC_OPTION_pedantic_errors, 0},
    {"pie", MCC_OPTION_pie, 0},
    {"no-pie", MCC_OPTION_nopie, 0},
    {"nopie", MCC_OPTION_nopie, 0},
    {"pipe", 0, 0},
    {"s", MCC_OPTION_s, 0},
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

static const FlagDef options_W[] = {
    {offsetof(MCCState, warn_all), WD_ALL, "all"},
    {offsetof(MCCState, warn_error), 0, "error"},
    {offsetof(MCCState, warn_write_strings), 0, "write-strings"},
    {offsetof(MCCState, warn_unsupported), 0, "unsupported"},
    {offsetof(MCCState, warn_implicit_function_declaration), WD_ALL, "implicit-function-declaration"},
    {offsetof(MCCState, warn_discarded_qualifiers), WD_ALL, "discarded-qualifiers"},
    {offsetof(MCCState, warn_sequence_point), WD_ALL, "sequence-point"},
    {offsetof(MCCState, warn_format), WD_ALL, "format"},
    {offsetof(MCCState, warn_vla), 0, "vla"},
    {offsetof(MCCState, warn_undef), 0, "undef"},
    {offsetof(MCCState, warn_implicit_int), 0, "implicit-int"},
    {offsetof(MCCState, warn_sign_compare), 0, "sign-compare"},
    {offsetof(MCCState, warn_parentheses), WD_ALL, "parentheses"},
    {offsetof(MCCState, warn_switch), WD_ALL, "switch"},
    {offsetof(MCCState, warn_unused_variable), WD_ALL, "unused-variable"},
    {offsetof(MCCState, warn_unused_parameter), 0, "unused-parameter"},
    {offsetof(MCCState, warn_unused_function), WD_ALL, "unused-function"},
    {offsetof(MCCState, warn_unknown_pragmas), WD_ALL, "unknown-pragmas"},
    {offsetof(MCCState, warn_pedantic), 0, "pedantic"},
    {offsetof(MCCState, warn_fatal_errors), 0, "fatal-errors"},
    {offsetof(MCCState, warn_shadow), 0, "shadow"},
    {offsetof(MCCState, warn_unused_value), WD_ALL, "unused-value"},
    {offsetof(MCCState, warn_uninitialized), WD_ALL, "uninitialized"},
    {offsetof(MCCState, warn_varargs), 0, "varargs"},
    {offsetof(MCCState, warn_strict_prototypes), 0, "strict-prototypes"},
    {0, 0, NULL}};

static const FlagDef options_f[] = {
    {offsetof(MCCState, char_is_unsigned), 0, "unsigned-char"},
    {offsetof(MCCState, char_is_unsigned), FD_INVERT, "signed-char"},
    {offsetof(MCCState, nocommon), FD_INVERT, "common"},
    {offsetof(MCCState, leading_underscore), 0, "leading-underscore"},
    {offsetof(MCCState, ms_extensions), 0, "ms-extensions"},
    {offsetof(MCCState, dollars_in_identifiers), 0, "dollars-in-identifiers"},
    {offsetof(MCCState, test_coverage), 0, "test-coverage"},
    {offsetof(MCCState, reverse_funcargs), 0, "reverse-funcargs"},
    {offsetof(MCCState, gnu89_inline), 0, "gnu89-inline"},
    {offsetof(MCCState, unwind_tables), 0, "asynchronous-unwind-tables"},
    {offsetof(MCCState, short_enums), 0, "short-enums"},
    {offsetof(MCCState, nobuiltin), FD_INVERT, "builtin"},
    {offsetof(MCCState, omit_frame_pointer), 0, "omit-frame-pointer"},
    {offsetof(MCCState, function_sections), 0, "function-sections"},
    {offsetof(MCCState, data_sections), 0, "data-sections"},
    {offsetof(MCCState, wrapv), 0, "wrapv"},
    {offsetof(MCCState, trigraphs), 0, "trigraphs"},
    {offsetof(MCCState, cx_limited_range), 0, "cx-limited-range"},
    {offsetof(MCCState, freestanding), 0, "freestanding"},
    {offsetof(MCCState, freestanding), FD_INVERT, "hosted"},
    {offsetof(MCCState, syntax_only), 0, "syntax-only"},
    {0, 0, NULL}};

static const FlagDef options_m[] = {
    {offsetof(MCCState, ms_bitfields), 0, "ms-bitfields"},
#ifdef MCC_TARGET_X86_64
    {offsetof(MCCState, nosse), FD_INVERT, "sse"},
#endif
    {0, 0, NULL}};

static int set_flag(MCCState *s, const FlagDef *flags, const char *name) {
    int value, mask, ret;
    const FlagDef *p;
    const char *r;
    unsigned char *f;

    r = name, value = !strstart("no-", &r), mask = 0;

    if ((flags->flags & WD_ALL) && strstart("error=", &r))
        value = value ? WARN_ON | WARN_ERR : WARN_NOE, mask = WARN_ON;

    for (ret = -1, p = flags; p->name; ++p) {
        if (ret) {
            if (strcmp(r, p->name))
                continue;
        } else {
            if (0 == (p->flags & WD_ALL))
                continue;
        }

        f = (unsigned char *)s + p->offset;
        *f = (*f & mask) | (value ^ !!(p->flags & FD_INVERT));

        if (ret) {
            ret = 0;
            if (strcmp(r, "all"))
                break;
        }
    }
    return ret;
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
#elif TARGETOS_FreeBSD || TARGETOS_FreeBSD_kernel
    "freebsd"
#elif TARGETOS_OpenBSD
    "openbsd"
#elif TARGETOS_NetBSD
    "netbsd"
#elif CONFIG_MCC_MUSL
    "linux-musl"
#else
    "linux-gnu"
#endif
    ;

#if defined MCC_TARGET_MACHO
static uint32_t parse_version(MCCState *s1, const char *version) {
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    char *last;

    a = strtoul(version, &last, 10);
    if (*last == '.') {
        b = strtoul(&last[1], &last, 10);
        if (*last == '.')
            c = strtoul(&last[1], &last, 10);
    }
    if (*last || a > 0xffff || b > 0xff || c > 0xff)
        mcc_error_noabort("version a.b.c not correct: %s", version);
    return (a << 16) | (b << 8) | c;
}
#endif

static void insert_args(MCCState *s1, char ***pargv, int *pargc, int optind, const char *p, int sep) {
    int argc = 0;
    char **argv = NULL;
    for (int i = 0; i < *pargc; ++i)
        if (i == optind)
            dynarray_split(&argv, &argc, p, sep);
        else
            dynarray_add(&argv, &argc, mcc_strdup((*pargv)[i]));
    dynarray_reset(&s1->argv, &s1->argc);
    *pargc = s1->argc = argc;
    *pargv = s1->argv = argv;
}

static void args_parser_add_file(MCCState *s, const char *filename, int filetype) {
    struct filespec *f = mcc_malloc(sizeof *f + strlen(filename));
    f->type = filetype;
    strcpy(f->name, filename);
    dynarray_add(&s->files, &s->nb_files, f);
    if (filetype & AFF_TYPE_LIB)
        ++s->nb_libraries;
}

PUB_FUNC int mcc_parse_args(MCCState *s, int *pargc, char ***pargv) {
    MCCState *s1 = s;
    const MCCOption *popt;
    const char *optarg, *r;
    const char *run = NULL;
    int optind = 1, empty = 1, x;
    char **argv = *pargv;
    int argc = *pargc;

    s->link_optind = s->link_argc;

    while (optind < argc) {
        r = argv[optind];
        if (r[0] == '@' && r[1] != '\0') {
            int fd;
            char *p;
            fd = open(++r, O_RDONLY | O_BINARY);
            if (fd < 0)
                return mcc_error_noabort("listfile '%s' not found", r);
            p = mcc_load_text(fd);
            insert_args(s1, &argv, &argc, optind, p, 0);
            close(fd), mcc_free(p);
            continue;
        }
        optind++;
        if (r[0] != '-' || r[1] == '\0') {
            args_parser_add_file(s, r, s->filetype);
            empty = 0;
        dorun:
            if (run)
                break;
            continue;
        }
        if (r[1] == '-' && r[2] == '\0')
            goto dorun;

        for (popt = mcc_options;; ++popt) {
            const char *p1 = popt->name;
            const char *r1 = r + 1;
            if (p1 == NULL)
                return mcc_error_noabort("invalid option -- '%s'", r);
            if (!strstart(p1, &r1))
                continue;
            optarg = r1;
            if (popt->flags & MCC_OPTION_HAS_ARG) {
                if (*r1 == '\0' && !(popt->flags & MCC_OPTION_NOSEP)) {
                    if (optind >= argc)
                        return mcc_error_noabort("argument to '%s' is missing", r);
                    optarg = argv[optind++];
                }
            } else if (*r1 != '\0')
                continue;
            break;
        }

        switch (popt->index) {
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
        case MCC_OPTION_bt:
#ifdef CONFIG_MCC_BACKTRACE
            s->rt_num_callers = atoi(optarg);
#endif
            goto enable_backtrace;
        enable_backtrace:
#ifdef CONFIG_MCC_BACKTRACE
            s->do_backtrace = 1;
            if (0 == s->do_debug)
                s->do_debug = 1;
            s->dwarf = CONFIG_DWARF_VERSION;
#else
            return mcc_error_noabort("backtrace (-bt) support was not built into this mcc");
#endif
            break;
        case MCC_OPTION_b:
#ifdef CONFIG_MCC_BCHECK
            s->do_bounds_check = 1;
            goto enable_backtrace;
#else
            return mcc_error_noabort("the bounds checker (-b) was not built into this mcc");
#endif
            break;
        case MCC_OPTION_g:
            s->do_debug = 2;
            s->dwarf = CONFIG_DWARF_VERSION;
        g_redo:
            if (strstart("dwarf", &optarg)) {
                s->dwarf = (*optarg) ? (0 - atoi(optarg)) : DEFAULT_DWARF_VERSION;
            } else if (0 == strcmp("stabs", optarg)) {
                s->dwarf = 0;
            } else if (isnum(*optarg)) {
                x = *optarg++ - '0';
                s->do_debug = x > 2 ? 2 : x == 0 && s->do_backtrace ? 1
                                                                    : x;
                goto g_redo;
#ifdef MCC_TARGET_PE
            } else if (0 == strcmp(".pdb", optarg)) {
                s->dwarf = 5, s->do_debug |= 16;
#endif
            }
            break;
        case MCC_OPTION_c:
            x = MCC_OUTPUT_OBJ;
        set_output_type:
            if (s->output_type)
                mcc_warning("-%s: overriding compiler action already specified", popt->name);
            s->output_type = x;
            break;
        case MCC_OPTION_d:
            if (*optarg == 'D')
                s->dflag = 3;
            else if (*optarg == 'M')
                s->dflag = 7;
            else if (*optarg == 't')
                s->dflag = 16;
            else if (isnum(*optarg))
                g_debug |= atoi(optarg);
            else
                goto unsupported_option;
            break;
        case MCC_OPTION_static:
            s->static_link = 1;
            break;
        case MCC_OPTION_std: {
            const char *std = optarg;
            if (*std == '=')
                std++;
            const char *disp = std;
            int strict_iso = 0;
            if (strstart("gnu", &std))
                ;
            else if (strstart("c", &std))
                strict_iso = 1;
            else if (strstart("iso9899:", &std)) {
                strict_iso = 1;
                if (!strcmp(std, "1990"))
                    std = "90";
                else if (!strcmp(std, "199409"))
                    std = "94";
                else if (!strcmp(std, "1999") || !strcmp(std, "199901"))
                    std = "99";
                else if (!strcmp(std, "2011"))
                    std = "11";
                else if (!strcmp(std, "2017") || !strcmp(std, "2018"))
                    std = "17";
            } else {
                mcc_warning("unsupported language standard '%s'", disp);
                break;
            }
            if (!strcmp(std, "89") || !strcmp(std, "90"))
                s->cversion = 0;
            else if (!strcmp(std, "94"))
                s->cversion = 199409;
            else if (!strcmp(std, "99"))
                s->cversion = 199901;
            else if (!strcmp(std, "11"))
                s->cversion = 201112;
            else if (!strcmp(std, "17") || !strcmp(std, "18"))
                s->cversion = 201710;
            else if (!strcmp(std, "23") || !strcmp(std, "2x"))
                s->cversion = 202311;
            else
                mcc_warning("unsupported language standard '%s'", disp);
            if (strict_iso)
                s->trigraphs = !(s->cversion >= 202311);
        } break;
        case MCC_OPTION_shared:
            x = MCC_OUTPUT_DLL;
            goto set_output_type;
        case MCC_OPTION_soname:
            mcc_set_str(&s->soname, optarg);
            break;
        case MCC_OPTION_o:
            if (s->outfile) {
                mcc_warning("multiple -o option");
            }
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
                optarg++;
            mcc_set_str(&s->sysroot, optarg);
            break;
        case MCC_OPTION_include:
            cstr_printf(&s->cmdline_incl, "#include \"%s\"\n", optarg);
            break;
        case MCC_OPTION_imacros:
            cstr_printf(&s->cmdline_incl, "#include \"%s\"\n", optarg);
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
#ifdef MCC_IS_NATIVE
            run = optarg;
            x = MCC_OUTPUT_MEMORY;
            goto set_output_type;
#else
            return mcc_error_noabort("-run is not available in a cross compiler");
#endif
#ifdef MCC_IS_NATIVE
        case MCC_OPTION_rstdin:
            s->run_stdin = optarg;
            break;
#endif
        case MCC_OPTION_v:
            do
                ++s->verbose;
            while (*optarg++ == 'v');
            continue;
        case MCC_OPTION_f: {
            const char *vis = optarg;
            if (strstart("max-errors=", &vis)) {
                s->max_errors = atoi(vis);
            } else if (strstart("visibility=", &vis)) {
                if (!strcmp(vis, "default"))
                    s->visibility = STV_DEFAULT;
                else if (!strcmp(vis, "hidden"))
                    s->visibility = STV_HIDDEN;
                else if (!strcmp(vis, "internal"))
                    s->visibility = STV_INTERNAL;
                else if (!strcmp(vis, "protected"))
                    s->visibility = STV_PROTECTED;
                else
                    mcc_warning("unsupported visibility '%s'", vis);
            } else if (!strcmp(optarg, "PIC") || !strcmp(optarg, "PIE")) {
                s->pic = 2;
            } else if (!strcmp(optarg, "pic") || !strcmp(optarg, "pie")) {
                s->pic = 1;
            } else if (!strcmp(optarg, "no-pic") || !strcmp(optarg, "no-PIC") || !strcmp(optarg, "no-pie") || !strcmp(optarg, "no-PIE")) {
                s->pic = 0;
            } else if (!strcmp(optarg, "stack-protector") || !strcmp(optarg, "stack-protector-strong") || !strcmp(optarg, "stack-protector-all")) {
#if (defined MCC_TARGET_X86_64 && !defined MCC_TARGET_PE) || (defined MCC_TARGET_ARM64 && defined MCC_TARGET_MACHO)
                s->stack_protector = 1;
#else
                mcc_warning_c(warn_unsupported)(
                    "-f%s: stack protection is only implemented on x86_64 (ELF/Mach-O) "
                    "and arm64 Mach-O",
                    optarg);
#endif
            } else if (!strcmp(optarg, "no-stack-protector")) {
                s->stack_protector = 0;
            } else if (set_flag(s, options_f, optarg) < 0)
                goto unsupported_option;
        } break;
#ifdef MCC_TARGET_ARM
        case MCC_OPTION_mfloat_abi:
            if (!strcmp(optarg, "softfp")) {
                s->float_abi = ARM_SOFTFP_FLOAT;
            } else if (!strcmp(optarg, "hard"))
                s->float_abi = ARM_HARD_FLOAT;
            else
                return mcc_error_noabort("unsupported float abi '%s'", optarg);
            continue;
#endif
        case MCC_OPTION_m:
            if (set_flag(s, options_m, optarg) < 0) {
                const char *marg = optarg;
                if (strstart("arch=", &marg) || strstart("tune=", &marg) || strstart("cpu=", &marg) || strstart("cmodel=", &marg) || strstart("fpmath=", &marg))
                    break;
                if (x = atoi(optarg), x != 32 && x != 64)
                    goto unsupported_option;
                if (PTR_SIZE != x / 8)
                    return x;
                continue;
            }
            break;
        case MCC_OPTION_W:
            if (!strcmp(optarg, "extra") || !strcmp(optarg, "no-extra")) {
                unsigned char on = optarg[0] == 'e' ? WARN_ON : 0;
                s->warn_sign_compare = on;
                s->warn_unused_parameter = on;
                break;
            }
            if (optarg[0] && set_flag(s, options_W, optarg) < 0)
                goto unsupported_option;
            break;
        case MCC_OPTION_w:
            s->warn_none = 1;
            break;
        case MCC_OPTION_rdynamic:
            s->rdynamic = 1;
            break;
        case MCC_OPTION_Wl:
            if (mcc_set_linker(s, optarg) < 0)
                return -1;
            break;
        case MCC_OPTION_Wp:
            if (argv[0])
                insert_args(s, &argv, &argc, --optind, optarg, ',');
            break;
        case MCC_OPTION_E:
            x = MCC_OUTPUT_PREPROCESS;
            goto set_output_type;
        case MCC_OPTION_P:
            s->Pflag = atoi(optarg) + 1;
            break;

        case MCC_OPTION_M:
            s->include_sys_deps = 1;
            FALLTHROUGH;
        case MCC_OPTION_MM:
            s->just_deps = 1;
            s->gen_deps = 1;
            if (!s->deps_outfile)
                mcc_set_str(&s->deps_outfile, "-");
            break;
        case MCC_OPTION_MD:
            s->include_sys_deps = 1;
            FALLTHROUGH;
        case MCC_OPTION_MMD:
            s->gen_deps = 1;
            if (*optarg != ',')
                break;
            ++optarg;
            FALLTHROUGH;
        case MCC_OPTION_MF:
            mcc_set_str(&s->deps_outfile, optarg);
            break;
        case MCC_OPTION_MP:
            s->gen_phony_deps = 1;
            break;
        case MCC_OPTION_S:
            x = MCC_OUTPUT_ASM;
            goto set_output_type;
        case MCC_OPTION_MT:
        case MCC_OPTION_MQ: {
            const char *src = optarg;
            int extra = 0, sep = s->dep_target ? 1 : 0;
            if (popt->index == MCC_OPTION_MQ)
                for (const char *q = src; *q; q++)
                    if (*q == '$')
                        extra++;
            {
                int oldlen = s->dep_target ? (int)strlen(s->dep_target) : 0;
                char *nt = mcc_malloc(oldlen + sep + (int)strlen(src) + extra + 1);
                char *d = nt;
                if (s->dep_target) {
                    memcpy(d, s->dep_target, oldlen);
                    d += oldlen;
                    *d++ = ' ';
                }
                for (const char *q = src; *q; q++) {
                    if (popt->index == MCC_OPTION_MQ && *q == '$')
                        *d++ = '$';
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
        case MCC_OPTION_dumpversion:
            printf("%s\n", MCC_VERSION);
            exit(0);

        case MCC_OPTION_x:
            x = 0;
            if (*optarg == 'c')
                x = AFF_TYPE_C;
            else if (*optarg == 'a')
                x = AFF_TYPE_ASMPP;
            else if (*optarg == 'b')
                x = AFF_TYPE_BIN;
            else if (*optarg == 'n')
                x = AFF_TYPE_NONE;
            else
                mcc_warning("unsupported language '%s'", optarg);
            s->filetype = x | (s->filetype & ~AFF_TYPE_MASK);
            break;
        case MCC_OPTION_O:
            s->optimize_size = 0;
            if (optarg[0] == '\0')
                s->optimize = 1;
            else if (isnum(optarg[0]))
                s->optimize = optarg[0] - '0';
            else if (!strcmp(optarg, "s") || !strcmp(optarg, "z")) {
                s->optimize = 2;
                s->optimize_size = 1;
            } else if (!strcmp(optarg, "g"))
                s->optimize = 1;
            else if (!strcmp(optarg, "fast"))
                s->optimize = 3;
            else {
                mcc_warning("unsupported optimization level '-O%s'", optarg);
                s->optimize = 1;
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
        case MCC_OPTION_impdef:
            x = OPT_IMPDEF;
            goto extra_action;
        case MCC_OPTION_ar:
            x = OPT_AR;
        extra_action:
            if (NULL == argv[0])
                return -1;
            if (!empty && x)
                return mcc_error_noabort("cannot parse %s here", r);
            --optind;
            *pargc = argc - optind;
            *pargv = argv + optind;
            return x;
        default:
        unsupported_option:
            mcc_warning_c(warn_unsupported)("unsupported option '%s'", r);
            break;
        }
        empty = 0;
    }
    if (s->link_optind < s->link_argc)
        return mcc_error_noabort("argument to '-Wl,%s' is missing", s->link_argv[s->link_optind]);
    if (run) {
        if (*run && mcc_set_options(s, run) < 0)
            return -1;
        x = 0, r = 0;
        goto extra_action;
    }
    if (!empty)
        return 0;
    if (s->verbose == 2)
        return OPT_PRINT_DIRS;
    if (s->verbose)
        return OPT_V;
    return OPT_HELP;
}

LIBMCCAPI int mcc_set_options(MCCState *s, const char *r) {
    char **argv = NULL;
    int argc = 0, ret;
    dynarray_add(&argv, &argc, 0);
    dynarray_split(&argv, &argc, r, 0);
    ret = mcc_parse_args(s, &argc, &argv);
    dynarray_reset(&argv, &argc);
    return ret;
}

PUB_FUNC void mcc_print_stats(MCCState *s1, unsigned total_time) {
    if (!total_time)
        total_time = 1;
    fprintf(stderr, "# %d idents, %d lines, %u bytes\n"
                    "# %0.3f s, %u lines/s, %0.1f MB/s\n",
            total_idents, total_lines, total_bytes,
            (double)total_time / 1000,
            (unsigned)total_lines * 1000 / total_time,
            (double)total_bytes / 1000 / total_time);
    fprintf(stderr, "# text %u, data.rw %u, data.ro %u, bss %u bytes\n",
            s1->total_output[0],
            s1->total_output[1],
            s1->total_output[2],
            s1->total_output[3]);
#ifdef MEM_DEBUG
    fprintf(stderr, "# memory usage");
#ifdef MCC_IS_NATIVE
    if (s1->run_size) {
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
