#define USING_GLOBALS
#include "mcc.h"

#define ACCEPT_LF_IN_STRINGS 0


ST_DATA int tok_flags;
ST_DATA int parse_flags;

ST_DATA struct BufferedFile *file;
ST_DATA int tok;
ST_DATA CValue tokc;
ST_DATA int tok_imaginary;
ST_DATA const int *macro_ptr;
ST_DATA CString tokcstr;

ST_DATA int tok_ident;
ST_DATA TokenSym **table_ident;
ST_DATA int pp_expr;


static TokenSym *hash_ident[TOK_HASH_SIZE];
static char token_buf[STRING_MAX_SIZE + 1];
static CString cstr_buf;
static TokenString tokstr_buf;
static TokenString unget_buf;
static unsigned char isidnum_table[256 - CH_EOF];
static int pp_debug_tok, pp_debug_symv;
static int pp_counter;
static void tok_print(const int *str, const char *msg, ...);
static void next_nomacro(void);
static void parse_number(const char *p);
static void parse_string(const char *p, int len);

static struct TinyAlloc *toksym_alloc;
static struct TinyAlloc *tokstr_alloc;

static TokenString *macro_stack;

static const char mcc_keywords[] = 
#define DEF(id, str) str "\0"
#include "mcctok.h"
#undef DEF
;

static const unsigned char tok_two_chars[] =
 {
    '<','=', TOK_LE,
    '>','=', TOK_GE,
    '!','=', TOK_NE,
    '&','&', TOK_LAND,
    '|','|', TOK_LOR,
    '+','+', TOK_INC,
    '-','-', TOK_DEC,
    '=','=', TOK_EQ,
    '<','<', TOK_SHL,
    '>','>', TOK_SAR,
    '+','=', TOK_A_ADD,
    '-','=', TOK_A_SUB,
    '*','=', TOK_A_MUL,
    '/','=', TOK_A_DIV,
    '%','=', TOK_A_MOD,
    '&','=', TOK_A_AND,
    '^','=', TOK_A_XOR,
    '|','=', TOK_A_OR,
    '-','>', TOK_ARROW,
    '.','.', TOK_TWODOTS,
    '#','#', TOK_TWOSHARPS,
    0
};

ST_FUNC void skip(int c)
{
    if (tok != c) {
        char tmp[40];
        pstrcpy(tmp, sizeof tmp, get_tok_str(c, &tokc));
        mcc_error("'%s' expected (got '%s')", tmp, get_tok_str(tok, &tokc));
	}
    next();
}

ST_FUNC void expect(const char *msg)
{
    mcc_error("%s expected", msg);
}


#define USE_TAL

#ifndef USE_TAL
#define tal_free(al, p) mcc_free(p)
#define tal_realloc(al, p, size) mcc_realloc(p, size)
#define tal_new(a,b)
#define tal_delete(a)
#else
#if !defined(MEM_DEBUG)
#define tal_free(al, p) tal_free_impl(al, p)
#define tal_realloc(al, p, size) tal_realloc_impl(al, p, size)
#define TAL_DEBUG_PARAMS
#else
#define TAL_DEBUG MEM_DEBUG
#define tal_free(al, p) tal_free_impl(al, p, __FILE__, __LINE__)
#define tal_realloc(al, p, size) tal_realloc_impl(al, p, size, __FILE__, __LINE__)
#define TAL_DEBUG_PARAMS , const char *sfile, int sline
#endif

#define TOKSYM_TAL_SIZE (256 * 1024)
#define TOKSTR_TAL_SIZE (256 * 1024)

typedef struct TinyAlloc {
    uint8_t *p;
    uint8_t *bufend;
    struct TinyAlloc *next;
    unsigned nb_allocs;
    unsigned size;
#if TAL_INFO
    unsigned nb_peak;
    unsigned nb_total;
    uint8_t *peak_p;
#endif
    union {
        uint8_t buffer[1];
        size_t _aligner_;
    };
} TinyAlloc;

typedef struct tal_header_t {
    size_t  size;
#if TAL_DEBUG
    int     line_num;
    char    file_name[40];
#endif
} tal_header_t;

#define TAL_ALIGN(size) \
    (((size) + (sizeof (size_t) - 1)) & ~(sizeof (size_t) - 1))


static TinyAlloc *tal_new(TinyAlloc **pal, unsigned size)
{
    TinyAlloc *al = mcc_malloc(sizeof(TinyAlloc) - sizeof (size_t) + size);
    al->p = al->buffer;
    al->bufend = al->buffer + size;
    al->nb_allocs = 0;
    al->next = *pal, *pal = al;
    al->size = al->next ? al->next->size : size;
#if TAL_INFO
    al->nb_peak = 0;
    al->nb_total = 0;
    al->peak_p = al->p;
#endif
    return al;
}

static void tal_delete(TinyAlloc **pal)
{
    TinyAlloc *al = *pal, *next;

#if TAL_INFO
    fprintf(stderr, "tal_delete (&tok%s_alloc):\n", pal == &toksym_alloc ? "sym" : "str");
#endif
tail_call:
#if TAL_DEBUG && TAL_DEBUG != 3
#if TAL_INFO
    fprintf(stderr, "  size %7d  nb_peak %5d  nb_total %6d  usage %5.1f%%\n",
            al->bufend - al->buffer, al->nb_peak, al->nb_total,
            (al->peak_p - al->buffer) * 100.0 / (al->bufend - al->buffer));
#endif
    if (al->nb_allocs > 0) {
        uint8_t *p;
        fprintf(stderr, "TAL_DEBUG: memory leak %d chunk(s)\n", al->nb_allocs);
        p = al->buffer;
        while (p < al->p) {
            tal_header_t *header = (tal_header_t *)p;
            if (header->line_num > 0) {
                fprintf(stderr, "%s:%d: chunk of %d bytes leaked\n",
                        header->file_name, header->line_num, (int)header->size);
            }
            p += header->size + sizeof(tal_header_t);
        }
#if TAL_DEBUG == 2
        exit(2);
#endif
    }
#endif
    next = al->next;
    mcc_free(al);
    al = next;
    if (al)
        goto tail_call;
    *pal = al;
}

static void tal_free_impl(TinyAlloc **pal, void *p TAL_DEBUG_PARAMS)
{
    TinyAlloc *al, **top = pal;
    tal_header_t *header;

    if (!p)
        return;
    header = (tal_header_t *)p - 1;
#if TAL_DEBUG
    if (header->line_num < 0) {
        fprintf(stderr, "%s:%d: TAL_DEBUG: double frees chunk from\n",
                sfile, sline);
        fprintf(stderr, "%s:%d: %d bytes\n",
                header->file_name, (int)-header->line_num, (int)header->size);
    } else
        header->line_num = -header->line_num;
#endif
    al = *pal;
    while ((uint8_t*)p < al->buffer || (uint8_t*)p > al->bufend)
        al = *(pal = &al->next);
    if (0 == --al->nb_allocs) {
        *pal = al->next;
        if ((al->bufend - al->buffer) > al->size) {
            mcc_free(al);
        } else {
            al->p = al->buffer;
            al->next = *top, *top = al;
        }
    } else if ((uint8_t*)p + header->size == al->p) {
        al->p = (uint8_t*)header;
    }
}

static void *tal_realloc_impl(TinyAlloc **pal, void *p, unsigned size TAL_DEBUG_PARAMS)
{
    tal_header_t *header;
    void *ret;
    unsigned adj_size = TAL_ALIGN(size) + sizeof(tal_header_t);
    TinyAlloc *al = *pal;

    if (p) {
        while ((uint8_t*)p < al->buffer || (uint8_t*)p > al->bufend)
            al = al->next;
        header = (tal_header_t *)p - 1;
        if ((uint8_t*)p + header->size == al->p)
            al->p = (uint8_t*)header;
        if (al->p + adj_size > al->bufend) {
            ret = tal_realloc(pal, 0, size);
            memcpy(ret, p, header->size);
            tal_free(pal, p);
            return ret;
        } else if (al->p != (uint8_t*)header) {
            memcpy((tal_header_t*)al->p + 1, p, header->size);
#if TAL_DEBUG
            header->line_num = -header->line_num;
#endif
        }
    } else {
        while (al->p + adj_size > al->bufend) {
            al = al->next;
            if (!al) {
                unsigned new_size = (*pal)->size;
                if (adj_size > new_size) {
                    new_size = adj_size;
                }
                al = tal_new(pal, new_size);
                break;
            }
        }
        al->nb_allocs++;
    }
    header = (tal_header_t *)al->p;
    header->size = adj_size - sizeof(tal_header_t);
    al->p += adj_size;
    ret = header + 1;
#if  TAL_DEBUG
    {
        int ofs = strlen(sfile) + 1 - sizeof header->file_name;
        strcpy(header->file_name, sfile + (ofs > 0 ? ofs : 0));
        header->line_num = sline;
#if TAL_INFO
        if (al->nb_peak < al->nb_allocs)
            al->nb_peak = al->nb_allocs;
        if (al->peak_p < al->p)
            al->peak_p = al->p;
        al->nb_total++;
#endif
    }
#endif
    return ret;
}

#endif

static void cstr_realloc(CString *cstr, int new_size)
{
    int size;

    size = cstr->size_allocated;
    if (size < 8)
        size = 8;
    while (size < new_size)
        size = size * 2;
    cstr->data = mcc_realloc(cstr->data, size);
    cstr->size_allocated = size;
}

ST_INLN void cstr_ccat(CString *cstr, int ch)
{
    int size;
    size = cstr->size + 1;
    if (size > cstr->size_allocated)
        cstr_realloc(cstr, size);
    cstr->data[size - 1] = ch;
    cstr->size = size;
}

ST_INLN char *unicode_to_utf8 (char *b, uint32_t Uc)
{
    if (Uc<0x80) *b++=Uc;
    else if (Uc<0x800) *b++=192+Uc/64, *b++=128+Uc%64;
    else if (Uc-0xd800u<0x800) goto error;
    else if (Uc<0x10000) *b++=224+Uc/4096, *b++=128+Uc/64%64, *b++=128+Uc%64;
    else if (Uc<0x110000) *b++=240+Uc/262144, *b++=128+Uc/4096%64, *b++=128+Uc/64%64, *b++=128+Uc%64;
    else error: mcc_error("0x%x is not a valid universal character", Uc);
    return b;
}

ST_INLN void cstr_u8cat(CString *cstr, int ch)
{
    char buf[4], *e;
    e = unicode_to_utf8(buf, (uint32_t)ch);
    cstr_cat(cstr, buf, e - buf);
}

/* 6.4.3: if *pp points at a universal character name (\uXXXX or \UXXXXXXXX),
   decode it, advance *pp past it and return the code point; otherwise leave
   *pp unchanged and return -1. Reads raw buffer bytes, so a UCN straddling a
   buffer-refill boundary is treated as not-a-UCN (vanishingly rare). */
static int decode_ucn(uint8_t **pp)
{
    uint8_t *p = *pp;
    int n, i, c;
    unsigned int v = 0;
    if (p[0] != '\\')
        return -1;
    if (p[1] == 'u') n = 4;
    else if (p[1] == 'U') n = 8;
    else return -1;
    for (i = 0; i < n; i++) {
        c = p[2 + i];
        if (c >= '0' && c <= '9') v = v * 16 + (c - '0');
        else if (c >= 'a' && c <= 'f') v = v * 16 + (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v = v * 16 + (c - 'A' + 10);
        else return -1;
    }
    *pp = p + 2 + n;
    /* 6.4.3p2: a UCN in an identifier shall not designate a code point < 00A0
       (except 0024 '$', 0040 '@', 0060 '`') nor one in the surrogate range
       D800–DFFF. Both callers of decode_ucn are identifier contexts (UCNs in
       string/char literals go through parse_string, where gcc is lenient). */
    if ((v < 0xA0 && v != 0x24 && v != 0x40 && v != 0x60)
        || (v >= 0xD800 && v <= 0xDFFF))
        mcc_error("universal character \\u%04x is not valid in an identifier",
                  v);
    return (int)v;
}

ST_FUNC void cstr_cat(CString *cstr, const char *str, int len)
{
    int size;
    if (len <= 0)
        len = strlen(str) + 1 + len;
    size = cstr->size + len;
    if (size > cstr->size_allocated)
        cstr_realloc(cstr, size);
    memmove(cstr->data + cstr->size, str, len);
    cstr->size = size;
}

ST_FUNC void cstr_wccat(CString *cstr, int ch)
{
    int size;
    size = cstr->size + sizeof(nwchar_t);
    if (size > cstr->size_allocated)
        cstr_realloc(cstr, size);
    *(nwchar_t *)(cstr->data + size - sizeof(nwchar_t)) = ch;
    cstr->size = size;
}

ST_FUNC void cstr_new(CString *cstr)
{
    memset(cstr, 0, sizeof(CString));
}

ST_FUNC void cstr_free(CString *cstr)
{
    mcc_free(cstr->data);
}

ST_FUNC void cstr_reset(CString *cstr)
{
    cstr->size = 0;
}

ST_FUNC int cstr_vprintf(CString *cstr, const char *fmt, va_list ap)
{
    va_list v;
    int len, size = 80;
    for (;;) {
        size += cstr->size;
        if (size > cstr->size_allocated)
            cstr_realloc(cstr, size);
        size = cstr->size_allocated - cstr->size;
        va_copy(v, ap);
        len = vsnprintf(cstr->data + cstr->size, size, fmt, v);
        va_end(v);
        if (len >= 0 && len < size)
            break;
        size *= 2;
    }
    cstr->size += len;
    return len;
}

ST_FUNC int cstr_printf(CString *cstr, const char *fmt, ...)
{
    va_list ap; int len;
    va_start(ap, fmt);
    len = cstr_vprintf(cstr, fmt, ap);
    va_end(ap);
    return len;
}

static void add_char(CString *cstr, int c)
{
    if (c == '\'' || c == '\"' || c == '\\') {
        cstr_ccat(cstr, '\\');
    }
    if (c >= 32 && c <= 126) {
        cstr_ccat(cstr, c);
    } else {
        cstr_ccat(cstr, '\\');
        if (c == '\n') {
            cstr_ccat(cstr, 'n');
        } else {
            cstr_ccat(cstr, '0' + ((c >> 6) & 7));
            cstr_ccat(cstr, '0' + ((c >> 3) & 7));
            cstr_ccat(cstr, '0' + (c & 7));
        }
    }
}

static TokenSym *tok_alloc_new(TokenSym **pts, const char *str, int len)
{
    TokenSym *ts, **ptable;
    int i;

    if (tok_ident >= SYM_FIRST_ANOM) 
        mcc_error("memory full (symbols)");

    i = tok_ident - TOK_IDENT;
    if ((i % TOK_ALLOC_INCR) == 0) {
        ptable = mcc_realloc(table_ident, (i + TOK_ALLOC_INCR) * sizeof(TokenSym *));
        table_ident = ptable;
    }

    ts = tal_realloc(&toksym_alloc, 0, sizeof(TokenSym) + len);
    table_ident[i] = ts;
    ts->tok = tok_ident++;
    ts->sym_define = NULL;
    ts->sym_label = NULL;
    ts->sym_struct = NULL;
    ts->sym_identifier = NULL;
    ts->len = len;
    ts->hash_next = NULL;
    memcpy(ts->str, str, len);
    ts->str[len] = '\0';
    *pts = ts;
    return ts;
}

#define TOK_HASH_INIT 1
#define TOK_HASH_FUNC(h, c) ((h) + ((h) << 5) + ((h) >> 27) + (c))


ST_FUNC TokenSym *tok_alloc(const char *str, int len)
{
    TokenSym *ts, **pts;
    unsigned int h;

    h = TOK_HASH_INIT;
    for(int i=0;i<len;i++)
        h = TOK_HASH_FUNC(h, ((unsigned char *)str)[i]);
    h &= (TOK_HASH_SIZE - 1);

    pts = &hash_ident[h];
    for(;;) {
        ts = *pts;
        if (!ts)
            break;
        if (ts->len == len && !memcmp(ts->str, str, len))
            return ts;
        pts = &(ts->hash_next);
    }
    return tok_alloc_new(pts, str, len);
}

ST_FUNC int tok_alloc_const(const char *str)
{
    return tok_alloc(str, strlen(str))->tok;
}


ST_FUNC const char *get_tok_str(int v, CValue *cv)
{
    char *p;
    int len;

    cstr_reset(&cstr_buf);
    /* This function writes short fixed strings straight into cstr_buf.data via
       p (snprintf/strcpy below); make sure the buffer exists and is big enough
       for the longest of them ("<long double>", a 64-bit value, ...). */
    if (cstr_buf.size_allocated < 32)
        cstr_realloc(&cstr_buf, 32);
    p = cstr_buf.data;

    switch(v) {
    case TOK_CINT:
    case TOK_CUINT:
    case TOK_CLONG:
    case TOK_CULONG:
    case TOK_CLLONG:
    case TOK_CULLONG:
        snprintf(p, cstr_buf.size_allocated, "%llu", (unsigned long long)cv->i);
        break;
    case TOK_U16CHAR:
        cstr_ccat(&cstr_buf, 'u');
        goto do_char_const;
    case TOK_U32CHAR:
        cstr_ccat(&cstr_buf, 'U');
        goto do_char_const;
    case TOK_LCHAR:
        cstr_ccat(&cstr_buf, 'L');
        /* fall through */
    case TOK_CCHAR:
    do_char_const:
        cstr_ccat(&cstr_buf, '\'');
        add_char(&cstr_buf, cv->i);
        cstr_ccat(&cstr_buf, '\'');
        cstr_ccat(&cstr_buf, '\0');
        break;
    case TOK_PPNUM:
    case TOK_PPSTR:
        return (char*)cv->str.data;
    case TOK_U16STR:
    case TOK_U32STR:
    case TOK_LSTR:
        cstr_ccat(&cstr_buf, 'L');
        /* fall through */
    case TOK_STR:
        cstr_ccat(&cstr_buf, '\"');
        if (v == TOK_STR) {
            len = cv->str.size - 1;
            for(int i=0;i<len;i++)
                add_char(&cstr_buf, ((unsigned char *)cv->str.data)[i]);
        } else {
            len = (cv->str.size / sizeof(nwchar_t)) - 1;
            for(int i=0;i<len;i++)
                add_char(&cstr_buf, ((nwchar_t *)cv->str.data)[i]);
        }
        cstr_ccat(&cstr_buf, '\"');
        cstr_ccat(&cstr_buf, '\0');
        break;

    case TOK_CFLOAT:
        return strcpy(p, "<float>");
    case TOK_CDOUBLE:
        return strcpy(p, "<double>");
    case TOK_CLDOUBLE:
        return strcpy(p, "<long double>");
    case TOK_LINENUM:
        return strcpy(p, "<linenumber>");

    case TOK_LT:
        v = '<';
        goto addv;
    case TOK_GT:
        v = '>';
        goto addv;
    case TOK_DOTS:
        return strcpy(p, "...");
    case TOK_A_SHL:
        return strcpy(p, "<<=");
    case TOK_A_SAR:
        return strcpy(p, ">>=");
    case TOK_EOF:
        return strcpy(p, "<eof>");
    case 0:
        return strcpy(p, "<no name>");
    default:
        v &= ~(SYM_FIELD | SYM_STRUCT);
        if (v < TOK_IDENT) {
            const unsigned char *q = tok_two_chars;
            while (*q) {
                if (q[2] == v) {
                    *p++ = q[0];
                    *p++ = q[1];
                    *p = '\0';
                    return cstr_buf.data;
                }
                q += 3;
            }
            if (v >= 127 || (v < 32 && !is_space(v) && v != '\n')) {
                snprintf(p, cstr_buf.size_allocated, "<\\x%02x>", v);
                break;
            }
    addv:
            *p++ = v;
            *p = '\0';
        } else if (v < tok_ident) {
            return table_ident[v - TOK_IDENT]->str;
        } else if (v >= SYM_FIRST_ANOM) {
            snprintf(p, cstr_buf.size_allocated, "L.%u", v - SYM_FIRST_ANOM);
        } else {
            return NULL;
        }
        break;
    }
    return cstr_buf.data;
}

/* 5.1.1.2 phase 1: replace the nine trigraph sequences (??=, ??(, ??), ??<,
   ??>, ??/, ??', ??!, ??-) with their single-character equivalents, in place.
   Done at buffer-fill time so the result feeds both the tokenizer and phase-2
   line splicing (a `??/` newline becomes `\` newline → a spliced line). Returns
   the new length.  Off by default; enabled by -trigraphs (gcc-compatible). */
static int trigraph_replace(unsigned char *buf, int len)
{
    unsigned char *src = buf, *dst = buf, *end = buf + len;
    while (src < end) {
        if (src[0] == '?' && src + 2 < end && src[1] == '?') {
            int t = 0;
            switch (src[2]) {
            case '=':  t = '#';  break;  case '(':  t = '[';  break;
            case ')':  t = ']';  break;  case '<':  t = '{';  break;
            case '>':  t = '}';  break;  case '/':  t = '\\'; break;
            case '\'': t = '^';  break;  case '!':  t = '|';  break;
            case '-':  t = '~';  break;
            }
            if (t) { *dst++ = (unsigned char)t; src += 3; continue; }
        }
        *dst++ = *src++;
    }
    return (int)(dst - buf);
}

static int handle_eob(void)
{
    BufferedFile *bf = file;
    int len;

    if (bf->buf_ptr >= bf->buf_end) {
        if (bf->fd >= 0) {
#if defined(PARSE_DEBUG)
            len = 1;
#else
            len = IO_BUF_SIZE;
#endif
            len = read(bf->fd, bf->buffer, len);
            if (len < 0)
                len = 0;
            if (mcc_state->trigraphs && len > 0) {
                len = trigraph_replace(bf->buffer, len);
                /* A trailing run of up to two '?' may begin a trigraph completed
                   by the next chunk; push it back so it is reconsidered (only
                   possible on a seekable fd — boundary-split trigraphs in a pipe
                   are not handled). */
                if (len > 2) {
                    int k = 0;
                    while (k < 2 && bf->buffer[len - 1 - k] == '?')
                        k++;
                    if (k > 0 && lseek(bf->fd, -(long)k, SEEK_CUR) != (off_t)-1)
                        len -= k;
                }
            }
        } else {
            len = 0;
        }
        total_bytes += len;
        bf->buf_ptr = bf->buffer;
        bf->buf_end = bf->buffer + len;
        *bf->buf_end = CH_EOB;
    }
    if (bf->buf_ptr < bf->buf_end) {
        return bf->buf_ptr[0];
    } else {
        bf->buf_ptr = bf->buf_end;
        return CH_EOF;
    }
}

static int next_c(void)
{
    int ch = *++file->buf_ptr;
    if (ch == CH_EOB && file->buf_ptr >= file->buf_end)
        ch = handle_eob();
    return ch;
}

static int handle_stray_noerror(int err)
{
    int ch;
    while ((ch = next_c()) == '\\') {
        ch = next_c();
        if (ch == '\n') {
    newl:
            file->line_num++;
        } else {
            if (ch == '\r') {
                ch = next_c();
                if (ch == '\n')
                    goto newl;
                *--file->buf_ptr = '\r';
            }
            if (err)
                mcc_error("stray '\\' in program");
            return *--file->buf_ptr = '\\';
        }
    }
    return ch;
}

#define ninp() handle_stray_noerror(0)

static int handle_bs(uint8_t **p)
{
    int c;
    file->buf_ptr = *p - 1;
    c = ninp();
    *p = file->buf_ptr;
    return c;
}

static int handle_stray(uint8_t **p)
{
    int c;
    file->buf_ptr = *p - 1;
    c = handle_stray_noerror(!(parse_flags & PARSE_FLAG_ACCEPT_STRAYS));
    *p = file->buf_ptr;
    return c;
}

#define PEEKC(c, p)\
{\
    c = *++p;\
    if (c == '\\')\
        c = handle_stray(&p); \
}

static int skip_spaces(void)
{
    int ch;
    --file->buf_ptr;
    do {
        ch = ninp();
    } while (isidnum_table[ch - CH_EOF] & IS_SPC);
    return ch;
}

static uint8_t *parse_line_comment(uint8_t *p)
{
    int c;
    for(;;) {
        for (;;) {
            c = *++p;
    redo:
            if (c == '\n' || c == '\\')
                break;
            c = *++p;
            if (c == '\n' || c == '\\')
                break;
        }
        if (c == '\n')
            break;
        c = handle_bs(&p);
        if (c == CH_EOF)
            break;
        if (c != '\\')
            goto redo;
    }
    return p;
}

static uint8_t *parse_comment(uint8_t *p)
{
    int c;
    for(;;) {
        for(;;) {
            c = *++p;
        redo:
            if (c == '\n' || c == '*' || c == '\\')
                break;
            c = *++p;
            if (c == '\n' || c == '*' || c == '\\')
                break;
        }
        if (c == '\n') {
            file->line_num++;
        } else if (c == '*') {
            do {
                c = *++p;
            } while (c == '*');
            if (c == '\\')
                c = handle_bs(&p);
            if (c == '/')
                break;
            goto check_eof;
        } else {
            c = handle_bs(&p);
        check_eof:
            if (c == CH_EOF)
                mcc_error("unexpected end of file in comment");
            if (c != '\\')
                goto redo;
        }
    }
    return p + 1;
}

static uint8_t *parse_pp_string(uint8_t *p, int sep, CString *str)
{
    int c;
    for(;;) {
        c = *++p;
    redo:
        if (c == sep) {
            break;
        } else if (c == '\\') {
            c = handle_bs(&p);
            if (c == CH_EOF) {
        unterminated_string:
                tok_flags &= ~TOK_FLAG_BOL;
                mcc_error("missing terminating %c character", sep);
            } else if (c == '\\') {
                if (str)
                    cstr_ccat(str, c);
                c = *++p;
                if (c == '\\') {
                    c = handle_bs(&p);
                    if (c == CH_EOF)
                        goto unterminated_string;
                }
                goto add_char;
            } else {
                goto redo;
            }
        } else if (c == '\n') {
        add_lf:
            if (ACCEPT_LF_IN_STRINGS) {
                file->line_num++;
                goto add_char;
            } else if (str) {
                goto unterminated_string;
            } else {
                return p;
            }
        } else if (c == '\r') {
            c = *++p;
            if (c == '\\')
                c = handle_bs(&p);
            if (c == '\n')
                goto add_lf;
            if (c == CH_EOF)
                goto unterminated_string;
            if (str)
                cstr_ccat(str, '\r');
            goto redo;
        } else {
        add_char:
            if (str)
                cstr_ccat(str, c);
        }
    }
    p++;
    return p;
}

static void preprocess_skip(void)
{
    int a, start_of_line, c, in_warn_or_error;
    uint8_t *p;

    p = file->buf_ptr;
    a = 0;
redo_start:
    start_of_line = 1;
    in_warn_or_error = 0;
    for(;;) {
        c = *p;
        switch(c) {
        case ' ':
        case '\t':
        case '\f':
        case '\v':
        case '\r':
            p++;
            continue;
        case '\n':
            file->line_num++;
            p++;
            goto redo_start;
        case '\\':
            c = handle_bs(&p);
            if (c == CH_EOF)
                expect("#endif");
            if (c == '\\')
                ++p;
            continue;
        case '\"':
        case '\'':
            if (in_warn_or_error)
                goto _default;
            tok_flags &= ~TOK_FLAG_BOL;
            p = parse_pp_string(p, c, NULL);
            break;
        case '/':
            if (in_warn_or_error)
                goto _default;
            ++p;
            c = handle_bs(&p);
            if (c == '*') {
                p = parse_comment(p);
            } else if (c == '/') {
                p = parse_line_comment(p);
            }
            continue;
        case '#':
            p++;
            if (start_of_line) {
                file->buf_ptr = p;
                next_nomacro();
                p = file->buf_ptr;
                if (a == 0 && 
                    (tok == TOK_ELSE || tok == TOK_ELIF || tok == TOK_ENDIF))
                    goto the_end;
                if (tok == TOK_IF || tok == TOK_IFDEF || tok == TOK_IFNDEF)
                    a++;
                else if (tok == TOK_ENDIF)
                    a--;
                else if( tok == TOK_ERROR || tok == TOK_WARNING)
                    in_warn_or_error = 1;
                else if (tok == TOK_LINEFEED)
                    goto redo_start;
                else if (parse_flags & PARSE_FLAG_ASM_FILE)
                    p = parse_line_comment(p - 1);
            }
#if !defined(MCC_TARGET_ARM) && !defined(MCC_TARGET_ARM64)
            else if (parse_flags & PARSE_FLAG_ASM_FILE)
                p = parse_line_comment(p - 1);
#else
#endif
            break;
_default:
        default:
            p++;
            break;
        }
        start_of_line = 0;
    }
 the_end: ;
    file->buf_ptr = p;
}

#if 0
static inline int tok_size(const int *p)
{
    switch(*p) {
    case TOK_CINT:
    case TOK_CUINT:
    case TOK_CCHAR:
    case TOK_LCHAR:
    case TOK_CFLOAT:
    case TOK_LINENUM:
        return 1 + 1;
    case TOK_STR:
    case TOK_LSTR:
    case TOK_PPNUM:
    case TOK_PPSTR:
        return 1 + 1 + (p[1] + 3) / 4;
    case TOK_CLONG:
    case TOK_CULONG:
	return 1 + LONG_SIZE / 4;
    case TOK_CDOUBLE:
    case TOK_CLLONG:
    case TOK_CULLONG:
        return 1 + 2;
    case TOK_CLDOUBLE:
        return 1 + LDOUBLE_WORDS;
    default:
        return 1 + 0;
    }
}
#endif

ST_INLN void tok_str_new(TokenString *s)
{
    s->str = NULL;
    s->len = s->need_spc = 0;
    s->allocated_len = 0;
    s->last_line_num = -1;
}

ST_FUNC TokenString *tok_str_alloc(void)
{
    TokenString *str = tal_realloc(&tokstr_alloc, 0, sizeof *str);
    tok_str_new(str);
    return str;
}

ST_FUNC void tok_str_free_str(int *str)
{
    tal_free(&tokstr_alloc, str);
}

ST_FUNC void tok_str_free(TokenString *str)
{
    tok_str_free_str(str->str);
    tal_free(&tokstr_alloc, str);
}

ST_FUNC int *tok_str_realloc(TokenString *s, int new_size)
{
    int *str, size;

    size = s->allocated_len;
    if (size < 16)
        size = 16;
    while (size < new_size)
        size = size * 2;
    if (size > s->allocated_len) {
        str = tal_realloc(&tokstr_alloc, s->str, size * sizeof(int));
        s->allocated_len = size;
        s->str = str;
    }
    return s->str;
}

ST_FUNC void tok_str_add(TokenString *s, int t)
{
    int len, *str;

    len = s->len;
    str = s->str;
    if (len >= s->allocated_len)
        str = tok_str_realloc(s, len + 1);
    str[len++] = t;
    s->len = len;
}

ST_FUNC void begin_macro(TokenString *str, int alloc)
{
    str->alloc = alloc;
    str->prev = macro_stack;
    str->prev_ptr = macro_ptr;
    str->save_line_num = file->line_num;
    macro_ptr = str->str;
    macro_stack = str;
}

ST_FUNC void end_macro(void)
{
    TokenString *str = macro_stack;
    macro_stack = str->prev;
    macro_ptr = str->prev_ptr;
    file->line_num = str->save_line_num;
    if (str->alloc == 0) {
        str->len = str->need_spc = 0;
    } else {
        if (str->alloc == 2)
            str->str = NULL;
        tok_str_free(str);
    }
}

static void tok_str_add2(TokenString *s, int t, CValue *cv)
{
    int len, *str;

    len = s->len;
    str = s->str;

    if (len + TOK_MAX_SIZE >= s->allocated_len)
        str = tok_str_realloc(s, len + TOK_MAX_SIZE + 1);
    str[len++] = t;
    switch(t) {
    case TOK_CINT:
    case TOK_CUINT:
    case TOK_CCHAR:
    case TOK_LCHAR:
    case TOK_U16CHAR:
    case TOK_U32CHAR:
    case TOK_CFLOAT:
    case TOK_LINENUM:
#if LONG_SIZE == 4
    case TOK_CLONG:
    case TOK_CULONG:
#endif
        str[len++] = cv->tab[0];
        break;
    case TOK_PPNUM:
    case TOK_PPSTR:
    case TOK_STR:
    case TOK_LSTR:
    case TOK_U16STR:
    case TOK_U32STR:
        {
            size_t nb_words =
                1 + (cv->str.size + sizeof(int) - 1) / sizeof(int);
            if (len + nb_words >= s->allocated_len)
                str = tok_str_realloc(s, len + nb_words + 1);
            str[len] = cv->str.size;
            memcpy(&str[len + 1], cv->str.data, cv->str.size);
            len += nb_words;
        }
        break;
    case TOK_CDOUBLE:
    case TOK_CLLONG:
    case TOK_CULLONG:
#if LONG_SIZE == 8
    case TOK_CLONG:
    case TOK_CULONG:
#endif
        str[len++] = cv->tab[0];
        str[len++] = cv->tab[1];
        break;
    case TOK_CLDOUBLE:
        str[len++] = cv->tab[0];
        str[len++] = cv->tab[1];
        if (LDOUBLE_WORDS >= 3)
        str[len++] = cv->tab[2];
        if (LDOUBLE_WORDS >= 4)
        str[len++] = cv->tab[3];
    default:
        break;
    }
    s->len = len;
}

ST_FUNC void tok_str_add_tok(TokenString *s)
{
    CValue cval;

    if (file->line_num != s->last_line_num) {
        s->last_line_num = file->line_num;
        cval.i = s->last_line_num;
        tok_str_add2(s, TOK_LINENUM, &cval);
    }
    tok_str_add2(s, tok, &tokc);
}

static void tok_str_add2_spc(TokenString *s, int t, CValue *cv)
{
    if (s->need_spc == 3)
        tok_str_add(s, ' ');
    s->need_spc = 2;
    tok_str_add2(s, t, cv);
}

static inline void tok_get(int *t, const int **pp, CValue *cv)
{
    const int *p = *pp;
    int n, *tab;

    tab = cv->tab;
    switch(*t = *p++) {
#if LONG_SIZE == 4
    case TOK_CLONG:
#endif
    case TOK_CINT:
    case TOK_CCHAR:
    case TOK_LCHAR:
    case TOK_U16CHAR:
    case TOK_U32CHAR:
    case TOK_LINENUM:
        cv->i = *p++;
        break;
#if LONG_SIZE == 4
    case TOK_CULONG:
#endif
    case TOK_CUINT:
        cv->i = (unsigned)*p++;
        break;
    case TOK_CFLOAT:
	tab[0] = *p++;
	break;
    case TOK_STR:
    case TOK_LSTR:
    case TOK_U16STR:
    case TOK_U32STR:
    case TOK_PPNUM:
    case TOK_PPSTR:
        cv->str.size = *p++;
        cv->str.data = (char*)p;
        p += (cv->str.size + sizeof(int) - 1) / sizeof(int);
        break;
    case TOK_CDOUBLE:
    case TOK_CLLONG:
    case TOK_CULLONG:
#if LONG_SIZE == 8
    case TOK_CLONG:
    case TOK_CULONG:
#endif
        n = 2;
        goto copy;
    case TOK_CLDOUBLE:
        n = LDOUBLE_WORDS;
    copy:
        do
            *tab++ = *p++;
        while (--n);
        break;
    default:
        break;
    }
    *pp = p;
}

#if 0
# define TOK_GET(t,p,c) tok_get(t,p,c)
#else
# define TOK_GET(t,p,c) do { \
    int _t = **(p); \
    if (TOK_HAS_VALUE(_t)) \
        tok_get(t, p, c); \
    else \
        *(t) = _t, ++*(p); \
    } while (0)
#endif

static int macro_is_equal(const int *a, const int *b)
{
    CValue cv;
    int t;

    if (!a || !b)
        return 1;

    while (*a && *b) {
        cstr_reset(&tokcstr);
        TOK_GET(&t, &a, &cv);
        cstr_cat(&tokcstr, get_tok_str(t, &cv), 0);
        TOK_GET(&t, &b, &cv);
        if (strcmp(tokcstr.data, get_tok_str(t, &cv)))
            return 0;
    }
    return !(*a || *b);
}

ST_INLN void define_push(int v, int macro_type, int *str, Sym *first_arg)
{
    Sym *s, *o;

    o = define_find(v);
    s = sym_push2(&define_stack, v, macro_type, 0);
    s->d = str;
    s->next = first_arg;
    table_ident[v - TOK_IDENT]->sym_define = s;

    if (o && !macro_is_equal(o->d, s->d))
	mcc_warning("%s redefined", get_tok_str(v, NULL));
}

ST_FUNC void define_undef(Sym *s)
{
    int v = s->v;
    if (v >= TOK_IDENT && v < tok_ident)
        table_ident[v - TOK_IDENT]->sym_define = NULL;
}

ST_INLN Sym *define_find(int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
        return NULL;
    return table_ident[v]->sym_define;
}

ST_FUNC void free_defines(Sym *b)
{
    while (define_stack != b) {
        Sym *top = define_stack;
        define_stack = top->prev;
        tok_str_free_str(top->d);
        define_undef(top);
        sym_free(top);
    }
}

static void maybe_run_test(MCCState *s)
{
    const char *p;
    if (s->include_stack_ptr != s->include_stack)
        return;
    p = get_tok_str(tok, NULL);
    if (0 != memcmp(p, "test_", 5))
        return;
    if (0 != --s->run_test)
        return;
    fprintf(s->ppfp, &"\n[%s]\n"[!(s->dflag & 32)], p), fflush(s->ppfp);
    define_push(tok, MACRO_OBJ, NULL, NULL);
}

ST_FUNC void skip_to_eol(int warn)
{
    if (tok == TOK_LINEFEED)
        return;
    if (warn)
        mcc_warning("extra tokens after directive");
    while (macro_stack)
        end_macro();
    file->buf_ptr = parse_line_comment(file->buf_ptr - 1);
    next_nomacro();
}

static CachedInclude *
search_cached_include(MCCState *s1, const char *filename, int add);

static int parse_include(MCCState *s1, int do_next, int test)
{
    int c, i;
    char name[1024], buf[1024], *p;
    CachedInclude *e;

    c = skip_spaces();
    if (c == '<' || c == '\"') {
        cstr_reset(&tokcstr);
        file->buf_ptr = parse_pp_string(file->buf_ptr, c == '<' ? '>' : c, &tokcstr);
        i = tokcstr.size;
        pstrncpy(name, sizeof name, tokcstr.data, i);
        next_nomacro();
    } else {
	parse_flags = PARSE_FLAG_PREPROCESS
                    | PARSE_FLAG_LINEFEED
                    | (parse_flags & PARSE_FLAG_ASM_FILE);
        name[0] = 0;
        for (;;) {
            next();
            p = name, i = strlen(p) - 1;
            if (i > 0
                && ((p[0] == '"' && p[i] == '"')
                 || (p[0] == '<' && p[i] == '>')))
                break;
            if (tok == TOK_LINEFEED)
                mcc_error("'#include' expects \"FILENAME\" or <FILENAME>");
            pstrcat(name, sizeof name, get_tok_str(tok, &tokc));
	}
        c = p[0];
        memmove(p, p + 1, i - 1), p[i - 1] = 0;
    }

    if (!test)
        skip_to_eol(1);

    /* track whether the resolved header is a system header: found via a
       -isystem/default system path, or included from a system header. */
    int parent_sys = file ? file->system_header : 0, cand_sys = 0;
    i = do_next ? file->include_next_index : -1;
    for (;;) {
        ++i;
        cand_sys = 0;
        if (i == 0) {
            if (!IS_ABSPATH(name))
                continue;
            buf[0] = '\0';
        } else if (i == 1) {
            if (c != '\"')
                continue;
            p = file->true_filename;
            pstrncpy(buf, sizeof buf, p, mcc_basename(p) - p);
        } else {
            int j = i - 2, k = j - s1->nb_include_paths;
            if (k < 0)
                p = s1->include_paths[j];
            else if (k < s1->nb_sysinclude_paths) {
                p = s1->sysinclude_paths[k];
                cand_sys = 1;
            } else if (test)
                return 0;
            else
                mcc_error("include file '%s' not found", name);
            pstrcpy(buf, sizeof buf, p);
            pstrcat(buf, sizeof buf, "/");
        }
        pstrcat(buf, sizeof buf, name);
        e = search_cached_include(s1, buf, 0);
        if (e && (define_find(e->ifndef_macro) || e->once)) {
#ifdef INC_DEBUG
            printf("%s: skipping cached %s\n", file->filename, buf);
#endif
            if ((s1->verbose | 1) == 3)
                printf("=> %*s%s\n",
                   (int)(s1->include_stack_ptr - s1->include_stack), "", buf);
            return 1;
        }
        if (mcc_open(s1, buf) >= 0)
            break;
    }
    /* mcc_open pushed the new file; mark it system if it came from a system
       path or was included from a system header (gcc/clang propagate this). */
    file->system_header = cand_sys || parent_sys;

    if (test) {
        mcc_close();
    } else {
        if (s1->include_stack_ptr >= s1->include_stack + INCLUDE_STACK_SIZE)
            mcc_error("#include recursion too deep");
        *s1->include_stack_ptr++ = file->prev;
        file->include_next_index = i;
#ifdef INC_DEBUG
        printf("%s: including %s\n", file->prev->filename, file->filename);
#endif
        if (s1->gen_deps) {
            BufferedFile *bf = file;
            while (i == 1 && (bf = bf->prev))
                i = bf->include_next_index;
            if (s1->include_sys_deps || i - 2 < s1->nb_include_paths)
                dynarray_add(&s1->target_deps, &s1->nb_target_deps,
                    mcc_strdup(buf));
        }
        mcc_debug_bincl(s1);
    }
    return 1;
}

static int expr_preprocess(MCCState *s1)
{
    int c, t;
    int t0 = tok;
    TokenString *str;
    
    str = tok_str_alloc();
    pp_expr = 1;
    while (1) {
        next();
        t = tok;
        if (tok < TOK_IDENT) {
            if (tok == TOK_LINEFEED || tok == TOK_EOF)
                break;
            if (tok >= TOK_STR && tok <= TOK_CLDOUBLE)
                mcc_error("invalid constant in preprocessor expression");

        } else if (tok == TOK_DEFINED) {
            parse_flags &= ~PARSE_FLAG_PREPROCESS;
            next();
            t = tok;
            if (t == '(') 
                next();
            parse_flags |= PARSE_FLAG_PREPROCESS;
            if (tok < TOK_IDENT)
                expect("identifier after 'defined'");
            if (s1->run_test)
                maybe_run_test(s1);
            c = 0;
            if (define_find(tok)
                || tok == TOK___HAS_INCLUDE
                || tok == TOK___HAS_INCLUDE_NEXT)
                c = 1;
            if (t == '(') {
                next();
                if (tok != ')')
                    expect("')'");
            }
            goto c_number;
        } else if (tok == TOK___HAS_INCLUDE ||
                   tok == TOK___HAS_INCLUDE_NEXT) {
            t = tok;
            next();
	    if (tok != '(')
		expect("'('");
            c = parse_include(s1, t - TOK___HAS_INCLUDE, 1);
            if (tok != ')')
                expect("')'");
            goto c_number;
        } else {
            c = 0;
        c_number:
            tok = TOK_CLLONG;
            tokc.i = c;
        }
        tok_str_add_tok(str);
    }
    if (0 == str->len)
        mcc_error("#%s with no expression", get_tok_str(t0, 0));
    tok_str_add(str, TOK_EOF);
    pp_expr = t0;
    t = tok;
    begin_macro(str, 1);
    next();
    c = expr_const();
    if (tok != TOK_EOF)
        mcc_error("...");
    pp_expr = 0;
    end_macro();
    tok = t;
    return c != 0;
}

ST_FUNC void pp_error(CString *cs)
{
    cstr_printf(cs, "bad preprocessor expression: #%s", get_tok_str(pp_expr, 0));
    macro_ptr = macro_stack->str;
    while (next(), tok != TOK_EOF)
        cstr_printf(cs, " %s", get_tok_str(tok, &tokc));
}

/* 6.10.8p2: none of the predefined macro names (and the identifier 'defined')
   shall be the subject of a #define or #undef. These four are handled as magic
   builtin tokens (define_push at startup) and so bypass the regular macro-table
   redefinition warning; flag them explicitly. (__COUNTER__ is a GNU builtin
   treated the same way by gcc.) gcc/clang diagnose; mcc warns. */
static int is_predef_macro(int v)
{
    return v == TOK___LINE__ || v == TOK___FILE__ || v == TOK___DATE__
        || v == TOK___TIME__ || v == TOK___COUNTER__;
}

ST_FUNC void parse_define(void)
{
    Sym *s, *first, **ps;
    int v, t, varg, is_vaargs, t0;
    int saved_parse_flags = parse_flags;
    TokenString str;

    v = tok;
    if (v < TOK_IDENT || v == TOK_DEFINED)
        mcc_error("invalid macro name '%s'", get_tok_str(tok, &tokc));
    if (is_predef_macro(v))
        mcc_warning("%s redefined", get_tok_str(v, NULL));
    first = NULL;
    t = MACRO_OBJ;
    parse_flags = ((parse_flags & ~PARSE_FLAG_ASM_FILE) | PARSE_FLAG_SPACES);
    next_nomacro();
    parse_flags &= ~PARSE_FLAG_SPACES;
    is_vaargs = 0;
    if (tok == '(') {
        int dotid = set_idnum('.', 0);
        next_nomacro();
        ps = &first;
        if (tok != ')') for (;;) {
            varg = tok;
            next_nomacro();
            is_vaargs = 0;
            if (varg == TOK_DOTS) {
                varg = TOK___VA_ARGS__;
                is_vaargs = 1;
            } else if (tok == TOK_DOTS && gnu_ext) {
                is_vaargs = 1;
                next_nomacro();
            }
            if (varg < TOK_IDENT)
        bad_list:
                mcc_error("bad macro parameter list");
            s = sym_push2(&define_stack, varg | SYM_FIELD, is_vaargs, 0);
            *ps = s;
            ps = &s->next;
            if (tok == ')')
                break;
            if (tok != ',' || is_vaargs)
                goto bad_list;
            next_nomacro();
        }
        parse_flags |= PARSE_FLAG_SPACES;
        next_nomacro();
        t = MACRO_FUNC;
        set_idnum('.', dotid);
    }

    parse_flags |= PARSE_FLAG_ACCEPT_STRAYS | PARSE_FLAG_SPACES | PARSE_FLAG_LINEFEED;
    tok_str_new(&str);
    t0 = 0;
    while (tok != TOK_LINEFEED && tok != TOK_EOF) {
        if (is_space(tok)) {
            str.need_spc |= 1;
        } else {
            if (TOK_TWOSHARPS == tok) {
                if (0 == t0)
                    goto bad_twosharp;
                tok = TOK_PPJOIN;
                t |= MACRO_JOIN;
            }
            /* C11 6.10.3p5: __VA_ARGS__ shall occur only in the replacement
               list of a variadic (...) macro. gcc warns by default. */
            if (tok == TOK___VA_ARGS__ && !is_vaargs)
                mcc_warning("__VA_ARGS__ can only appear in the expansion of a "
                            "C99 variadic macro");
            tok_str_add2_spc(&str, tok, &tokc);
            t0 = tok;
        }
        next_nomacro();
    }
    parse_flags = saved_parse_flags;
    tok_str_add(&str, 0);
    if (t0 == TOK_PPJOIN)
bad_twosharp:
        mcc_error("'##' cannot appear at either end of macro");
    define_push(v, t, str.str, first);
}

static CachedInclude *search_cached_include(MCCState *s1, const char *filename, int add)
{
    const char *s, *basename;
    unsigned int h;
    CachedInclude *e;
    int c, i, len;

    s = basename = mcc_basename(filename);
    h = TOK_HASH_INIT;
    while ((c = (unsigned char)*s) != 0) {
#ifdef _WIN32
        h = TOK_HASH_FUNC(h, toup(c));
#else
        h = TOK_HASH_FUNC(h, c);
#endif
        s++;
    }
    h &= (CACHED_INCLUDES_HASH_SIZE - 1);

    i = s1->cached_includes_hash[h];
    for(;;) {
        if (i == 0)
            break;
        e = s1->cached_includes[i - 1];
        if (0 == PATHCMP(filename, e->filename))
            return e;
        if (e->once
            && 0 == PATHCMP(basename, mcc_basename(e->filename))
            && 0 == normalized_PATHCMP(filename, e->filename)
            )
            return e;
        i = e->hash_next;
    }
    if (!add)
        return NULL;

    e = mcc_malloc(sizeof(CachedInclude) + (len = strlen(filename)));
    memcpy(e->filename, filename, len + 1);
    e->ifndef_macro = e->once = 0;
    dynarray_add(&s1->cached_includes, &s1->nb_cached_includes, e);
    e->hash_next = s1->cached_includes_hash[h];
    s1->cached_includes_hash[h] = s1->nb_cached_includes;
#ifdef INC_DEBUG
    printf("adding cached '%s'\n", filename);
#endif
    return e;
}

static int pragma_parse(MCCState *s1)
{
    next_nomacro();
    if (tok == TOK_push_macro || tok == TOK_pop_macro) {
        int t = tok, v;
        Sym *s;

        if (next(), tok != '(')
            goto pragma_err;
        if (next(), tok != TOK_STR)
            goto pragma_err;
        v = tok_alloc(tokc.str.data, tokc.str.size - 1)->tok;
        if (next(), tok != ')')
            goto pragma_err;
        if (t == TOK_push_macro) {
            while (NULL == (s = define_find(v)))
                define_push(v, 0, NULL, NULL);
            s->type.ref = s;
        } else {
            for (s = define_stack; s; s = s->prev)
                if (s->v == v && s->type.ref == s) {
                    s->type.ref = NULL;
                    break;
                }
        }
        if (s)
            table_ident[v - TOK_IDENT]->sym_define = s->d ? s : NULL;
        else
            mcc_warning("unbalanced #pragma pop_macro");
        pp_debug_tok = t, pp_debug_symv = v;

    } else if (tok == TOK_once) {
        search_cached_include(s1, file->true_filename, 1)->once = 1;

    } else if (s1->output_type == MCC_OUTPUT_PREPROCESS) {
        unget_tok(' ');
        unget_tok(TOK_PRAGMA);
        unget_tok('#');
        unget_tok(TOK_LINEFEED);
        return 1;

    } else if (tok == TOK_pack) {
        next();
        skip('(');
        if (tok == TOK_ASM_pop) {
            next();
            if (s1->pack_stack_ptr <= s1->pack_stack) {
            stk_error:
                mcc_error("out of pack stack");
            }
            s1->pack_stack_ptr--;
        } else {
            int val = 0;
            if (tok != ')') {
                if (tok == TOK_ASM_push) {
                    next();
                    if (s1->pack_stack_ptr >= s1->pack_stack + PACK_STACK_SIZE - 1)
                        goto stk_error;
                    val = *s1->pack_stack_ptr++;
                    if (tok != ',')
                        goto pack_set;
                    next();
                }
                if (tok != TOK_CINT)
                    goto pragma_err;
                val = tokc.i;
                if (val < 1 || val > 16 || (val & (val - 1)) != 0)
                    goto pragma_err;
                next();
            }
        pack_set:
            *s1->pack_stack_ptr = val;
        }
        if (tok != ')')
            goto pragma_err;

    } else if (tok == TOK_comment) {
        char *p; int t;
        next();
        skip('(');
        t = tok;
        next();
        skip(',');
        if (tok != TOK_STR)
            goto pragma_err;
        p = mcc_strdup(tokc.str.data);
        next();
        if (tok != ')')
            goto pragma_err;
        if (t == TOK_lib) {
            dynarray_add(&s1->pragma_libs, &s1->nb_pragma_libs, p);
        } else {
            if (t == TOK_option)
                mcc_set_options(s1, p);
            mcc_free(p);
        }

    } else if (tok == TOK_pragma_message) {
        /* 6.10.6: #pragma message("...") — emit the string as a note.
           Common diagnostic pragma (gcc/clang compatible); not gated on
           -Wall. Accepts both #pragma message "str" and
           #pragma message("str"). */
        int paren = 0;
        next();
        if (tok == '(') {
            paren = 1;
            next();
        }
        if (tok != TOK_STR)
            goto pragma_err;
        if (file)
            fprintf(stderr, "%s:%d: note: #pragma message: %s\n",
                    file->filename, file->line_num, (char *)tokc.str.data);
        else
            fprintf(stderr, "note: #pragma message: %s\n",
                    (char *)tokc.str.data);
        next();
        if (paren) {
            if (tok != ')')
                goto pragma_err;
            next();
        }
        while (tok != TOK_LINEFEED && tok != TOK_EOF)
            next_nomacro();
        return 1;

    } else if (tok == TOK_STDC) {
        /* 6.10.6: #pragma STDC <FP_CONTRACT|FENV_ACCESS|CX_LIMITED_RANGE>
           <ON|OFF|DEFAULT>. The switch name selects one of the three
           MCCState.stdc_* fields; the state word maps to STDC_ON/OFF/DEFAULT.
           These pragmas are block-scoped (block() in src/mccgen.c saves and
           restores the fields), so a setting inside a compound statement does
           not leak past its closing brace. STDC pragma tokens are not
           macro-expanded, matching gcc. */
        unsigned char *slot, state;
        const char *sw;
        next_nomacro();
        sw = get_tok_str(tok, &tokc);
        if (!strcmp(sw, "FP_CONTRACT"))
            slot = &s1->stdc_fp_contract;
        else if (!strcmp(sw, "FENV_ACCESS"))
            slot = &s1->stdc_fenv_access;
        else if (!strcmp(sw, "CX_LIMITED_RANGE"))
            slot = &s1->stdc_cx_limited;
        else {
            /* Unknown STDC switch: warn and swallow the rest of the line. */
            mcc_warning_c(warn_all)("unknown #pragma STDC '%s'", sw);
            while (tok != TOK_LINEFEED && tok != TOK_EOF)
                next_nomacro();
            return 1;
        }
        next_nomacro();
        {
            /* ON/OFF/DEFAULT are uppercase identifiers, not the lowercase
               `default` keyword (TOK_DEFAULT) — compare by spelling. */
            const char *st = get_tok_str(tok, &tokc);
            if (!strcmp(st, "ON"))
                state = STDC_ON;
            else if (!strcmp(st, "OFF"))
                state = STDC_OFF;
            else if (!strcmp(st, "DEFAULT"))
                state = STDC_DEFAULT;
            else {
                mcc_warning_c(warn_all)(
                    "malformed #pragma STDC %s (expected ON/OFF/DEFAULT)", sw);
                while (tok != TOK_LINEFEED && tok != TOK_EOF)
                    next_nomacro();
                return 1;
            }
        }
        *slot = state;
        while (tok != TOK_LINEFEED && tok != TOK_EOF)
            next_nomacro();
        return 1;
    } else {
        mcc_warning_c(warn_all)("#pragma %s ignored", get_tok_str(tok, &tokc));
        return 0;
    }
    next();
    return 1;
pragma_err:
    mcc_error("malformed #pragma directive");
}

ST_FUNC void mccpp_putfile(const char *filename)
{
    char buf[1024];
    buf[0] = 0;
    if (!IS_ABSPATH(filename)) {
        pstrcpy(buf, sizeof buf, file->true_filename);
        *mcc_basename(buf) = 0;
    }
    pstrcat(buf, sizeof buf, filename);
#ifdef _WIN32
    normalize_slashes(buf);
#endif
    if (0 == strcmp(file->filename, buf))
        return;
    if (file->true_filename == file->filename)
        file->true_filename = mcc_strdup(file->filename);
    pstrcpy(file->filename, sizeof file->filename, buf);
    mcc_debug_newfile(mcc_state);
}

ST_FUNC void preprocess(int is_bof)
{
    MCCState *s1 = mcc_state;
    int c, n, saved_parse_flags;
    char buf[1024], *q;
    Sym *s;

    saved_parse_flags = parse_flags;
    parse_flags = PARSE_FLAG_PREPROCESS
        | PARSE_FLAG_TOK_NUM
        | PARSE_FLAG_TOK_STR
        | PARSE_FLAG_LINEFEED
        | (parse_flags & PARSE_FLAG_ASM_FILE)
        ;

    next_nomacro();
 redo:
    switch(tok) {
    case TOK_DEFINE:
        pp_debug_tok = tok;
        next_nomacro();
        pp_debug_symv = tok;
        parse_define();
        break;
    case TOK_UNDEF:
        pp_debug_tok = tok;
        next_nomacro();
        pp_debug_symv = tok;
        if (is_predef_macro(tok))           /* 6.10.8p2 */
            mcc_warning("undefining %s", get_tok_str(tok, NULL));
        s = define_find(tok);
        if (s)
            define_undef(s);
        next_nomacro();
        break;
    case TOK_INCLUDE:
    case TOK_INCLUDE_NEXT:
        parse_include(s1, tok - TOK_INCLUDE, 0);
        goto the_end;
    case TOK_IFNDEF:
        c = 1;
        goto do_ifdef;
    case TOK_IF:
        c = expr_preprocess(s1);
        goto do_if;
    case TOK_IFDEF:
        c = 0;
    do_ifdef:
        next_nomacro();
        if (tok < TOK_IDENT)
            mcc_error("invalid argument for '#if%sdef'", c ? "n" : "");
        if (is_bof) {
            if (c) {
#ifdef INC_DEBUG
                printf("#ifndef %s\n", get_tok_str(tok, NULL));
#endif
                file->ifndef_macro = tok;
            }
        }
        if (define_find(tok)
            || tok == TOK___HAS_INCLUDE
            || tok == TOK___HAS_INCLUDE_NEXT)
            c ^= 1;
        next_nomacro();
    do_if:
        if (s1->ifdef_stack_ptr >= s1->ifdef_stack + IFDEF_STACK_SIZE)
            mcc_error("memory full (ifdef)");
        *s1->ifdef_stack_ptr++ = c;
        goto test_skip;
    case TOK_ELSE:
        next_nomacro();
        if (s1->ifdef_stack_ptr == s1->ifdef_stack)
            mcc_error("#else without matching #if");
        if (s1->ifdef_stack_ptr[-1] & 2)
            mcc_error("#else after #else");
        c = (s1->ifdef_stack_ptr[-1] ^= 3);
        goto test_else;
    case TOK_ELIF:
        if (s1->ifdef_stack_ptr == s1->ifdef_stack)
            mcc_error("#elif without matching #if");
        c = s1->ifdef_stack_ptr[-1];
        if (c > 1)
            mcc_error("#elif after #else");
        if (c == 1) {
            skip_to_eol(0);
            c = 0;
        } else {
            c = expr_preprocess(s1);
            s1->ifdef_stack_ptr[-1] = c;
        }
    test_else:
        if (s1->ifdef_stack_ptr == file->ifdef_stack_ptr + 1)
            file->ifndef_macro = 0;
    test_skip:
        if (!(c & 1)) {
            skip_to_eol(1);
            preprocess_skip();
            is_bof = 0;
            goto redo;
        }
        break;
    case TOK_ENDIF:
        next_nomacro();
        if (s1->ifdef_stack_ptr <= file->ifdef_stack_ptr)
            mcc_error("#endif without matching #if");
        s1->ifdef_stack_ptr--;
        if (file->ifndef_macro &&
            s1->ifdef_stack_ptr == file->ifdef_stack_ptr) {
            file->ifndef_macro_saved = file->ifndef_macro;
            file->ifndef_macro = 0;
            tok_flags |= TOK_FLAG_ENDIF;
        }
        break;

    case TOK_LINE:
        parse_flags &= ~PARSE_FLAG_TOK_NUM;
        next();
        if (tok != TOK_PPNUM) {
    _line_err:
            mcc_error("wrong #line format");
        }
        c = 1;
        goto _line_num;
    case TOK_PPNUM:
        if (parse_flags & PARSE_FLAG_ASM_FILE)
            goto ignore;
        c = 0;
    _line_num:
        {
            /* 6.10.4p3: the digit sequence shall not be zero nor greater than
               2147483647. Accumulate in 64-bit so an out-of-range value is
               detected rather than silently wrapping __LINE__ negative; clamp
               the carried value to INT_MAX. */
            uint64_t nn = 0;
            int line_ovf = 0;
            for (q = tokc.str.data; *q; ++q) {
                if (!isnum(*q))
                    goto _line_err;
                nn = nn * 10 + (*q - '0');
                if (nn > 2147483647)
                    line_ovf = 1;
            }
            if ((line_ovf || nn == 0) && mcc_state->warn_pedantic) {
                if (mcc_state->pedantic_errors)
                    mcc_error("line number out of range");
                else
                    mcc_warning("line number out of range");
            }
            n = line_ovf ? 2147483647 : (int)nn;
        }
        parse_flags &= ~PARSE_FLAG_TOK_STR;
        next();
        if (tok != TOK_LINEFEED) {
            if (tok != TOK_PPSTR || tokc.str.data[0] != '"')
                goto _line_err;
            tokc.str.data[tokc.str.size - 2] = 0;
            mccpp_putfile(tokc.str.data + 1);
            next();
            skip_to_eol(c);
        }
        if (file->fd > 0)
            total_lines += file->line_num - n;
        file->line_num = n;
        break;

    case TOK_ERROR:
    case TOK_WARNING:
    {
        q = buf;
        c = skip_spaces();
        while (c != '\n' && c != CH_EOF) {
            if ((q - buf) < sizeof(buf) - 1)
                *q++ = c;
            c = ninp();
        }
        *q = '\0';
        if (tok == TOK_ERROR)
            mcc_error("#error %s", buf);
        else
            mcc_warning("#warning %s", buf);
        next_nomacro();
        break;
    }
    case TOK_PRAGMA:
        if (!pragma_parse(s1))
            goto ignore;
        break;
    case TOK_LINEFEED:
        goto the_end;
    default:
        if (saved_parse_flags & PARSE_FLAG_ASM_FILE)
            goto ignore;
        if (tok == '!' && is_bof)
            goto ignore;
        /* gcc/clang accept #ident and #sccs as extensions (ignored). */
        if (tok >= TOK_IDENT) {
            const char *d = get_tok_str(tok, &tokc);
            if (!strcmp(d, "ident") || !strcmp(d, "sccs"))
                goto ignore;
        }
        /* C11 6.10p1: a non-directive (unknown #-directive) is a constraint
           violation requiring a diagnostic; gcc and clang both make it a hard
           error. */
        mcc_error("invalid preprocessing directive #%s", get_tok_str(tok, &tokc));
    ignore:
        skip_to_eol(0);
        goto the_end;
    }
    skip_to_eol(1);
 the_end:
    parse_flags = saved_parse_flags;
}

static void parse_escape_string(CString *outstr, const uint8_t *buf, int is_long)
{
    int c, n, i;
    const uint8_t *p;

    p = buf;
    for(;;) {
        c = *p;
        if (c == '\0')
            break;
        if (c == '\\') {
            p++;
            c = *p;
            switch(c) {
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7':
                n = c - '0';
                p++;
                c = *p;
                if (isoct(c)) {
                    n = n * 8 + c - '0';
                    p++;
                    c = *p;
                    if (isoct(c)) {
                        n = n * 8 + c - '0';
                        p++;
                    }
                }
                /* 6.4.4.4p9: a non-wide octal escape shall be representable in
                   unsigned char. gcc warns, clang errors; mcc warns. */
                if (!is_long && n > 0xFF)
                    mcc_warning("octal escape sequence out of range");
                c = n;
                goto add_char_nonext;
            case 'x': i = 0; goto parse_hex_or_ucn;
            case 'u': i = 4; goto parse_hex_or_ucn;
            case 'U': i = 8; goto parse_hex_or_ucn;
    parse_hex_or_ucn:
                p++;
                n = 0;
                do {
                    c = *p;
                    if (c >= 'a' && c <= 'f')
                        c = c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F')
                        c = c - 'A' + 10;
                    else if (isnum(c))
                        c = c - '0';
                    else if (i >= 0)
                        expect("more hex digits in universal-character-name");
                    else {
                        /* 6.4.4.4p9: a non-wide hex escape shall be
                           representable in unsigned char (i<0 ⇒ this is a \x
                           escape, not a \u/\U universal-character-name). */
                        if (!is_long && n > 0xFF)
                            mcc_warning("hex escape sequence out of range");
                        goto add_hex_or_ucn;
                    }
                    n = (unsigned) n * 16 + c;
                    p++;
                } while (--i);
		if (is_long) {
    add_hex_or_ucn:
                    c = n;
		    goto add_char_nonext;
		}
                cstr_u8cat(outstr, n);
                continue;
            case 'a':
                c = '\a';
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'v':
                c = '\v';
                break;
            case 'e':
                if (!gnu_ext)
                    goto invalid_escape;
                c = 27;
                break;
            case '\'':
            case '\"':
            case '\\': 
            case '?':
                break;
            default:
            invalid_escape:
                if (c >= '!' && c <= '~')
                    mcc_warning("unknown escape sequence: \'\\%c\'", c);
                else
                    mcc_warning("unknown escape sequence: \'\\x%x\'", c);
                break;
            }
        } else if (is_long && c >= 0x80) {

            int cont;
            int skip;
            int i;

            if (c < 0xC2) {
	            skip = 1; goto invalid_utf8_sequence;
            } else if (c <= 0xDF) {
	            cont = 1; n = c & 0x1f;
            } else if (c <= 0xEF) {
	            cont = 2; n = c & 0xf;
            } else if (c <= 0xF4) {
	            cont = 3; n = c & 0x7;
            } else {
	            skip = 1; goto invalid_utf8_sequence;
            }

            for (i = 1; i <= cont; i++) {
                int l = 0x80, h = 0xBF;

                if (i == 1) {
                    switch (c) {
                    case 0xE0: l = 0xA0; break;
                    case 0xED: h = 0x9F; break;
                    case 0xF0: l = 0x90; break;
                    case 0xF4: h = 0x8F; break;
                    }
                }

                if (p[i] < l || p[i] > h) {
                    skip = i; goto invalid_utf8_sequence;
                }

                n = (n << 6) | (p[i] & 0x3f);
            }

            p += 1 + cont;
            c = n;
            goto add_char_nonext;

        invalid_utf8_sequence:
            mcc_warning("ill-formed UTF-8 subsequence starting with: \'\\x%x\'", c);
            c = 0xFFFD;
            p += skip;
            goto add_char_nonext;

        }
        p++;
    add_char_nonext:
        if (!is_long)
            cstr_ccat(outstr, c);
        else {
#ifdef MCC_TARGET_PE
            if (c < 0x10000) {
                cstr_wccat(outstr, c);
            } else {
                c -= 0x10000;
                cstr_wccat(outstr, (c >> 10) + 0xD800);
                cstr_wccat(outstr, (c & 0x3FF) + 0xDC00);
            }
#else
            cstr_wccat(outstr, c);
#endif
        }
    }
    if (!is_long)
        cstr_ccat(outstr, '\0');
    else
        cstr_wccat(outstr, '\0');
}

static void parse_string(const char *s, int len)
{
    uint8_t buf[1000], *p = buf;
    int is_long, sep, prefix = 0;

    if (*s == 'L' || *s == 'u' || *s == 'U')
        prefix = *s++, --len;
    is_long = (prefix != 0);     /* wide/code-point decoding for L/u/U */
    sep = *s++;
    len -= 2;
    if (len >= sizeof buf)
        p = mcc_malloc(len + 1);
    memcpy(p, s, len);
    p[len] = 0;

    cstr_reset(&tokcstr);
    parse_escape_string(&tokcstr, p, is_long);
    if (p != buf)
        mcc_free(p);

    if (sep == '\'') {
        int char_size, i, n, c;
        if (!is_long)
            tok = TOK_CCHAR, char_size = 1;
        else
            tok = TOK_LCHAR, char_size = sizeof(nwchar_t);
        if (prefix == 'u')
            tok = TOK_U16CHAR;
        else if (prefix == 'U')
            tok = TOK_U32CHAR;
        n = tokcstr.size / char_size - 1;
        if (n < 1)
            mcc_error("empty character constant");
        if (prefix == 'U') {
            /* char32_t: recombine UTF-16 surrogate pairs (PE splits astral
               code points) so U'\U0001F600' yields one 32-bit value. */
            int nchars = 0;
            for (c = i = 0; i < n; ++i) {
                /* nwchar_t units: 2-byte (unsigned) on PE, 4-byte on others —
                   read at native width, no masking, so non-PE keeps the full
                   32-bit code point. */
                unsigned int u = (unsigned int)((nwchar_t *)tokcstr.data)[i];
                if (u >= 0xD800 && u <= 0xDBFF && i + 1 < n) {
                    unsigned int lo = (unsigned int)((nwchar_t *)tokcstr.data)[i + 1] & 0xFFFFu;
                    if (lo >= 0xDC00 && lo <= 0xDFFF) {
                        u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                        i++;
                    }
                }
                c = (int)u;
                nchars++;
            }
            if (nchars > 1)
                mcc_warning("multi-character character constant");
        } else {
            if (n > 1)
                mcc_warning("multi-character character constant");
            for (c = i = 0; i < n; ++i) {
                if (is_long)
                    c = ((nwchar_t *)tokcstr.data)[i];
                else
                    c = (c << 8) | ((char *)tokcstr.data)[i];
            }
            if (prefix == 'u')
                c &= 0xFFFF;          /* char16_t range */
        }
        tokc.i = c;
    } else if (prefix == 'u') {
        /* u"..." char16_t string: re-encode the parsed code points (nwchar_t
           units) as UTF-16 (2-byte units, surrogate pairs above U+FFFF). */
        int i, ncp = tokcstr.size / sizeof(nwchar_t);
        nwchar_t *cps = mcc_malloc((ncp ? ncp : 1) * sizeof(nwchar_t));
        memcpy(cps, tokcstr.data, ncp * sizeof(nwchar_t));
        cstr_reset(&tokcstr);
        for (i = 0; i < ncp; i++) {
            unsigned int cp = (unsigned int)cps[i];
            if (cp < 0x10000) {
                cstr_ccat(&tokcstr, cp & 0xff);
                cstr_ccat(&tokcstr, (cp >> 8) & 0xff);
            } else {
                unsigned int hi, lo;
                cp -= 0x10000;
                hi = 0xD800 + (cp >> 10);
                lo = 0xDC00 + (cp & 0x3FF);
                cstr_ccat(&tokcstr, hi & 0xff); cstr_ccat(&tokcstr, (hi >> 8) & 0xff);
                cstr_ccat(&tokcstr, lo & 0xff); cstr_ccat(&tokcstr, (lo >> 8) & 0xff);
            }
        }
        mcc_free(cps);
        tokc.str.size = tokcstr.size;
        tokc.str.data = tokcstr.data;
        tok = TOK_U16STR;
    } else if (prefix == 'U') {
        /* U"..." char32_t string: parse_escape_string stored code points as
           nwchar_t units — 4 bytes (full code points) off PE, but 2-byte
           UTF-16 (astral code points split into surrogate pairs) on PE.
           Recombine any surrogate pairs and emit explicit 4-byte char32_t
           units, independent of the target's wchar_t width. */
        int i, ncp = tokcstr.size / sizeof(nwchar_t);
        nwchar_t *cps = mcc_malloc((ncp ? ncp : 1) * sizeof(nwchar_t));
        memcpy(cps, tokcstr.data, ncp * sizeof(nwchar_t));
        cstr_reset(&tokcstr);
        for (i = 0; i < ncp; i++) {
            unsigned int cp = (unsigned int)cps[i] & 0xFFFFFFFFu;
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < ncp) {
                unsigned int lo = (unsigned int)cps[i + 1] & 0xFFFFu;
                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    i++;
                }
            }
            cstr_ccat(&tokcstr, cp & 0xff);
            cstr_ccat(&tokcstr, (cp >> 8) & 0xff);
            cstr_ccat(&tokcstr, (cp >> 16) & 0xff);
            cstr_ccat(&tokcstr, (cp >> 24) & 0xff);
        }
        mcc_free(cps);
        tokc.str.size = tokcstr.size;
        tokc.str.data = tokcstr.data;
        tok = TOK_U32STR;
    } else {
        tokc.str.size = tokcstr.size;
        tokc.str.data = tokcstr.data;
        if (!is_long)
            tok = TOK_STR;
        else
            tok = TOK_LSTR;
    }
}

#define BN_SIZE 4

static int bn_lshift(unsigned int *bn, int shift, int or_val)
{
    unsigned int v;
    if (bn[BN_SIZE - 1] >> (32 - shift))
	return shift;
    for(int i=0;i<BN_SIZE;i++) {
        v = bn[i];
        bn[i] = (v << shift) | or_val;
        or_val = v >> (32 - shift);
    }
    return 0;
}

static void bn_zero(unsigned int *bn)
{
    for(int i=0;i<BN_SIZE;i++) {
        bn[i] = 0;
    }
}

static void parse_number(const char *p)
{
    int b, t, shift, frac_bits, s, exp_val, ch;
    char *q;
    unsigned int bn[BN_SIZE];
    long double d;

    tok_imaginary = 0;
    q = token_buf;
    ch = *p++;
    t = ch;
    ch = *p++;
    *q++ = t;
    b = 10;
    if (t == '.') {
        goto float_frac_parse;
    } else if (t == '0') {
        if (ch == 'x' || ch == 'X') {
            q--;
            ch = *p++;
            b = 16;
        } else if (mcc_state->mcc_ext && (ch == 'b' || ch == 'B')) {
            q--;
            ch = *p++;
            b = 2;
        }
    }
    while (1) {
        if (ch >= 'a' && ch <= 'f')
            t = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F')
            t = ch - 'A' + 10;
        else if (isnum(ch))
            t = ch - '0';
        else
            break;
        if (t >= b)
            break;
        if (q >= token_buf + STRING_MAX_SIZE) {
        num_too_long:
            mcc_error("number too long");
        }
        *q++ = ch;
        ch = *p++;
    }
    if (ch == '.' ||
        ((ch == 'e' || ch == 'E') && b == 10) ||
        ((ch == 'p' || ch == 'P') && (b == 16 || b == 2))) {
        if (b != 10) {
            frac_bits = 0;
            *q = '\0';
            if (b == 16)
                shift = 4;
            else 
                shift = 1;
            bn_zero(bn);
            q = token_buf;
            while (1) {
                t = *q++;
                if (t == '\0') {
                    break;
                } else if (t >= 'a') {
                    t = t - 'a' + 10;
                } else if (t >= 'A') {
                    t = t - 'A' + 10;
                } else {
                    t = t - '0';
                }
                frac_bits -= bn_lshift(bn, shift, t);
            }
            if (ch == '.') {
                ch = *p++;
                while (1) {
                    t = ch;
                    if (t >= 'a' && t <= 'f') {
                        t = t - 'a' + 10;
                    } else if (t >= 'A' && t <= 'F') {
                        t = t - 'A' + 10;
                    } else if (t >= '0' && t <= '9') {
                        t = t - '0';
                    } else {
                        break;
                    }
                    if (t >= b)
                        mcc_error("invalid digit");
                    frac_bits -= bn_lshift(bn, shift, t);
                    frac_bits += shift;
                    ch = *p++;
                }
            }
            if (ch != 'p' && ch != 'P')
                expect("exponent");
            ch = *p++;
            s = 1;
            exp_val = 0;
            if (ch == '+') {
                ch = *p++;
            } else if (ch == '-') {
                s = -1;
                ch = *p++;
            }
            if (ch < '0' || ch > '9')
                expect("exponent digits");
            while (ch >= '0' && ch <= '9') {
		if (exp_val < 100000000)
                    exp_val = exp_val * 10 + ch - '0';
                ch = *p++;
            }
            exp_val = exp_val * s;

            d = (long double)bn[3] * 79228162514264337593543950336.0L +
	        (long double)bn[2] * 18446744073709551616.0L +
	        (long double)bn[1] * 4294967296.0L +
	        (long double)bn[0];
            d = ldexpl(d, exp_val - frac_bits);
            t = toup(ch);
            if (t == 'F') {
                ch = *p++;
                tok = TOK_CFLOAT;
                tokc.f = (float)d;
            } else if (t == 'L') {
                ch = *p++;
                tok = TOK_CLDOUBLE;
                tokc.ld = d;
            } else {
                tok = TOK_CDOUBLE;
                tokc.d = (double)d;
            }
        } else {
            if (ch == '.') {
                if (q >= token_buf + STRING_MAX_SIZE)
                    goto num_too_long;
                *q++ = ch;
                ch = *p++;
            float_frac_parse:
                while (ch >= '0' && ch <= '9') {
                    if (q >= token_buf + STRING_MAX_SIZE)
                        goto num_too_long;
                    *q++ = ch;
                    ch = *p++;
                }
            }
            if (ch == 'e' || ch == 'E') {
                if (q >= token_buf + STRING_MAX_SIZE)
                    goto num_too_long;
                *q++ = ch;
                ch = *p++;
                if (ch == '-' || ch == '+') {
                    if (q >= token_buf + STRING_MAX_SIZE)
                        goto num_too_long;
                    *q++ = ch;
                    ch = *p++;
                }
                if (ch < '0' || ch > '9')
                    expect("exponent digits");
                while (ch >= '0' && ch <= '9') {
                    if (q >= token_buf + STRING_MAX_SIZE)
                        goto num_too_long;
                    *q++ = ch;
                    ch = *p++;
                }
            }
            *q = '\0';
            t = toup(ch);
            errno = 0;
            if (t == 'F') {
                ch = *p++;
                tok = TOK_CFLOAT;
                tokc.f = strtof(token_buf, NULL);
            } else if (t == 'L') {
                ch = *p++;
                tok = TOK_CLDOUBLE;
                tokc.ld = strtold(token_buf, NULL);
            } else {
                tok = TOK_CDOUBLE;
                tokc.d = strtod(token_buf, NULL);
            }
        }
    } else {
        unsigned long long n, n1;
        int lcount, ucount, l0, ov = 0;
        const char *p1;

        *q = '\0';
        q = token_buf;
        if (b == 10 && *q == '0') {
            b = 8;
            q++;
        }
        n = 0;
        while(1) {
            t = *q++;
            if (t == '\0')
                break;
            else if (t >= 'a')
                t = t - 'a' + 10;
            else if (t >= 'A')
                t = t - 'A' + 10;
            else
                t = t - '0';
            if (t >= b)
                mcc_error("invalid digit");
            n1 = n;
            n = n * b + t;
            if (n1 >= 0x1000000000000000ULL && n / b != n1)
                ov = 1;
        }

        lcount = ucount = 0;
        l0 = 0;
        p1 = p;
        for(;;) {
            t = toup(ch);
            if (t == 'L') {
                if (lcount >= 2)
                    mcc_error("three 'l's in integer constant");
                /* 6.4.4.1: the ll/LL suffix must be case-uniform. */
                if (lcount == 1 && ch != l0)
                    mcc_error("incorrect integer suffix: %s", p1);
                if (lcount == 0)
                    l0 = ch;
                lcount++;
                ch = *p++;
            } else if (t == 'U') {
                if (ucount >= 1)
                    mcc_error("two 'u's in integer constant");
                ucount++;
                ch = *p++;
            } else {
                break;
            }
        }

        if (pp_expr)
            lcount = 2;

        if (ucount == 0 && b == 10) {
            if (lcount <= (LONG_SIZE == 4)) {
                if (n >= 0x80000000U)
                    lcount = (LONG_SIZE == 4) + 1;
            }
            if (n >= 0x8000000000000000ULL)
                ov = 1, ucount = 1;
        } else {
            if (lcount <= (LONG_SIZE == 4)) {
                if (n >= 0x100000000ULL)
                    lcount = (LONG_SIZE == 4) + 1;
                else if (n >= 0x80000000U)
                    ucount = 1;
            }
            if (n >= 0x8000000000000000ULL)
                ucount = 1;
        }

        if (ov)
            mcc_warning("integer constant overflow");

        tok = TOK_CINT;
	if (lcount) {
            tok = TOK_CLONG;
            if (lcount == 2)
                tok = TOK_CLLONG;
	}
	if (ucount)
	    ++tok;
        tokc.i = n;
    }
    if ((ch == 'i' || ch == 'I' || ch == 'j' || ch == 'J')
        && (tok == TOK_CFLOAT || tok == TOK_CDOUBLE || tok == TOK_CLDOUBLE)) {
        tok_imaginary = 1;
        ch = *p++;
    }
    if (ch)
        mcc_error("invalid number");
}


#define PARSE2(c1, tok1, c2, tok2)              \
    case c1:                                    \
        PEEKC(c, p);                            \
        if (c == c2) {                          \
            p++;                                \
            tok = tok2;                         \
        } else {                                \
            tok = tok1;                         \
        }                                       \
        break;

static void next_nomacro(void)
{
    int t, c, str_prefix, len, uc;
    TokenSym *ts;
    uint8_t *p, *p1;
    unsigned int h;

    p = file->buf_ptr;
 redo_no_start:
    c = *p;
    switch(c) {
    case ' ':
    case '\t':
        tok = c;
        p++;
 maybe_space:
        if (parse_flags & PARSE_FLAG_SPACES)
            goto keep_tok_flags;
        while (isidnum_table[*p - CH_EOF] & IS_SPC)
            ++p;
        goto redo_no_start;
    case '\f':
    case '\v':
    case '\r':
        p++;
        goto redo_no_start;
    case '\\':
        /* 6.4.2.1/6.4.3: an identifier may begin with a universal character
           name (\uXXXX / \UXXXXXXXX). Decode it and parse the rest as an
           identifier rather than diagnosing a stray backslash. */
        if ((uc = decode_ucn(&p)) >= 0) {
            cstr_reset(&tokcstr);
            cstr_u8cat(&tokcstr, uc);
            c = *p;
            goto parse_ident_ucn;
        }
        c = handle_stray(&p);
        if (c == '\\')
            goto parse_simple;
        if (c == CH_EOF) {
            MCCState *s1 = mcc_state;
            if (!(tok_flags & TOK_FLAG_BOL)) {
                goto maybe_newline;
            } else if (!(parse_flags & PARSE_FLAG_PREPROCESS)) {
                tok = TOK_EOF;
            } else if (s1->ifdef_stack_ptr != file->ifdef_stack_ptr) {
                mcc_error("missing #endif");
            } else if (s1->include_stack_ptr == s1->include_stack) {
                tok = TOK_EOF;
            } else {

                if (tok_flags & TOK_FLAG_ENDIF) {
#ifdef INC_DEBUG
                    printf("#endif %s\n", get_tok_str(file->ifndef_macro_saved, NULL));
#endif
                    search_cached_include(s1, file->true_filename, 1)
                        ->ifndef_macro = file->ifndef_macro_saved;
                    tok_flags &= ~TOK_FLAG_ENDIF;
                }

                mcc_debug_eincl(mcc_state);
                mcc_close();
                s1->include_stack_ptr--;
                p = file->buf_ptr;
                goto maybe_newline;
            }
        } else {
            goto redo_no_start;
        }
        break;

    case '\n':
        file->line_num++;
        p++;
maybe_newline:
        tok_flags |= TOK_FLAG_BOL;
        if (0 == (parse_flags & PARSE_FLAG_LINEFEED))
            goto redo_no_start;
        tok = TOK_LINEFEED;
        goto keep_tok_flags;

    case '#':
        PEEKC(c, p);
        if ((tok_flags & TOK_FLAG_BOL) && 
            (parse_flags & PARSE_FLAG_PREPROCESS)) {
            tok_flags &= ~TOK_FLAG_BOL;
            file->buf_ptr = p;
            preprocess(tok_flags & TOK_FLAG_BOF);
            p = file->buf_ptr;
            goto maybe_newline;
        } else {
            if (c == '#') {
                p++;
                tok = TOK_TWOSHARPS;
            } else {
#if !defined(MCC_TARGET_ARM) && !defined(MCC_TARGET_ARM64)
                if (parse_flags & PARSE_FLAG_ASM_FILE) {
                    p = parse_line_comment(p - 1);
                    goto redo_no_start;
                } else
#endif
                {
                    tok = '#';
                }
            }
        }
        break;
    
    case '$':
        if (!(isidnum_table['$' - CH_EOF] & IS_ID)
         || (parse_flags & PARSE_FLAG_ASM_FILE))
            goto parse_simple;
        /* fall through */
    case 'a': case 'b': case 'c': case 'd':
    case 'e': case 'f': case 'g': case 'h':
    case 'i': case 'j': case 'k': case 'l':
    case 'm': case 'n': case 'o': case 'p':
    case 'q': case 'r': case 's': case 't':
    case 'v': case 'w': case 'x':
    case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D':
    case 'E': case 'F': case 'G': case 'H':
    case 'I': case 'J': case 'K': 
    case 'M': case 'N': case 'O': case 'P':
    case 'Q': case 'R': case 'S': case 'T':
    case 'V': case 'W': case 'X':
    case 'Y': case 'Z':
    case '_':
    parse_ident_fast:
        p1 = p;
        h = TOK_HASH_INIT;
        h = TOK_HASH_FUNC(h, c);
        while (c = *++p, isidnum_table[c - CH_EOF] & (IS_ID|IS_NUM))
            h = TOK_HASH_FUNC(h, c);
        len = p - p1;
        if (c != '\\') {
            TokenSym **pts;

            h &= (TOK_HASH_SIZE - 1);
            pts = &hash_ident[h];
            for(;;) {
                ts = *pts;
                if (!ts)
                    break;
                if (ts->len == len && !memcmp(ts->str, p1, len))
                    goto token_found;
                pts = &(ts->hash_next);
            }
            ts = tok_alloc_new(pts, (char *) p1, len);
        token_found: ;
        } else {
            cstr_reset(&tokcstr);
            cstr_cat(&tokcstr, (char *) p1, len);
        parse_ident_ucn:
            /* identifier containing a universal character name (6.4.3) or a
               line-spliced backslash. */
            for (;;) {
                if (c == '\\') {
                    if ((uc = decode_ucn(&p)) >= 0) {
                        cstr_u8cat(&tokcstr, uc);   /* UCN -> UTF-8 in the name */
                        c = *p;
                        continue;
                    }
                    p--;
                    PEEKC(c, p);                    /* consume a line splice, else stray '\' */
                    /* A stray '\' (e.g. '\'+space+newline under -E, where
                       handle_stray re-presents the '\' without consuming it)
                       ends the identifier; otherwise we re-read the same '\'
                       forever. The '\' is then tokenized separately. */
                    if (c == '\\')
                        break;
                    continue;
                }
                if (isidnum_table[c - CH_EOF] & (IS_ID|IS_NUM)) {
                    cstr_ccat(&tokcstr, c);
                    c = *++p;
                    continue;
                }
                break;
            }
            ts = tok_alloc(tokcstr.data, tokcstr.size);
        }
        tok = ts->tok;
        break;
    case 'u':
        /* 6.4.5: u8"..." UTF-8 string -> char array; 6.4.4.4: u'...' -> char16_t.
           (u"..." char16_t string literals are not yet supported.) */
        if (p[1] == '8' && p[2] == '\"') {
            p += 2;           /* advance to the opening '"' */
            c = *p;
            str_prefix = 0;
            goto str_const;
        }
        if (p[1] == '\'' || p[1] == '\"') {
            PEEKC(c, p);      /* c = the quote */
            str_prefix = 'u';
            goto str_const;
        }
        goto parse_ident_fast;

    case 'U':
        /* 6.4.4.4/6.4.5: U'...' / U"..." -> char32_t. */
        if (p[1] == '\'' || p[1] == '\"') {
            PEEKC(c, p);      /* c = the quote */
            str_prefix = 'U';
            goto str_const;
        }
        goto parse_ident_fast;

    case 'L':
        t = p[1];
        if (t == '\'' || t == '\"' || t == '\\') {
            PEEKC(c, p);
            if (c == '\'' || c == '\"') {
                str_prefix = 'L';
                goto str_const;
            }
            *--p = c = 'L';
        }
        goto parse_ident_fast;

    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7':
    case '8': case '9':
        t = c;
        PEEKC(c, p);
    parse_num:
        cstr_reset(&tokcstr);
        for(;;) {
            cstr_ccat(&tokcstr, t);
            if (!((isidnum_table[c - CH_EOF] & (IS_ID|IS_NUM))
                  || c == '.'
                  || ((c == '+' || c == '-')
                      && (((t == 'e' || t == 'E')
                            && !(parse_flags & PARSE_FLAG_ASM_FILE
                                && ((char*)tokcstr.data)[0] == '0'
                                && toup(((char*)tokcstr.data)[1]) == 'X'))
                          || t == 'p' || t == 'P'))))
                break;
            t = c;
            PEEKC(c, p);
        }
        cstr_ccat(&tokcstr, '\0');
        tokc.str.size = tokcstr.size;
        tokc.str.data = tokcstr.data;
        tok = TOK_PPNUM;
        break;

    case '.':
        PEEKC(c, p);
        if (isnum(c)) {
            t = '.';
            goto parse_num;
        } else if ((isidnum_table['.' - CH_EOF] & IS_ID)
                   && (isidnum_table[c - CH_EOF] & (IS_ID|IS_NUM))) {
            *--p = c = '.';
            goto parse_ident_fast;
        } else if (c == '.') {
            PEEKC(c, p);
            if (c == '.') {
                p++;
                tok = TOK_DOTS;
            } else {
                *--p = '.';
                tok = '.';
            }
        } else {
            tok = '.';
        }
        break;
    case '\'':
    case '\"':
        str_prefix = 0;
    str_const:
        cstr_reset(&tokcstr);
        if (str_prefix)
            cstr_ccat(&tokcstr, str_prefix);
        cstr_ccat(&tokcstr, c);
        p = parse_pp_string(p, c, &tokcstr);
        cstr_ccat(&tokcstr, c);
        cstr_ccat(&tokcstr, '\0');
        tokc.str.size = tokcstr.size;
        tokc.str.data = tokcstr.data;
        tok = TOK_PPSTR;
        break;

    case '<':
        PEEKC(c, p);
        if (c == '=') {
            p++;
            tok = TOK_LE;
        } else if (c == '<') {
            PEEKC(c, p);
            if (c == '=') {
                p++;
                tok = TOK_A_SHL;
            } else {
                tok = TOK_SHL;
            }
        } else if (c == ':') {     /* 6.4.6 digraph <: == [ */
            p++;
            tok = '[';
        } else if (c == '%') {     /* 6.4.6 digraph <% == { */
            p++;
            tok = '{';
        } else {
            tok = TOK_LT;
        }
        break;
    case '>':
        PEEKC(c, p);
        if (c == '=') {
            p++;
            tok = TOK_GE;
        } else if (c == '>') {
            PEEKC(c, p);
            if (c == '=') {
                p++;
                tok = TOK_A_SAR;
            } else {
                tok = TOK_SAR;
            }
        } else {
            tok = TOK_GT;
        }
        break;
        
    case '&':
        PEEKC(c, p);
        if (c == '&') {
            p++;
            tok = TOK_LAND;
        } else if (c == '=') {
            p++;
            tok = TOK_A_AND;
        } else {
            tok = '&';
        }
        break;
        
    case '|':
        PEEKC(c, p);
        if (c == '|') {
            p++;
            tok = TOK_LOR;
        } else if (c == '=') {
            p++;
            tok = TOK_A_OR;
        } else {
            tok = '|';
        }
        break;

    case '+':
        PEEKC(c, p);
        if (c == '+') {
            p++;
            tok = TOK_INC;
        } else if (c == '=') {
            p++;
            tok = TOK_A_ADD;
        } else {
            tok = '+';
        }
        break;
        
    case '-':
        PEEKC(c, p);
        if (c == '-') {
            p++;
            tok = TOK_DEC;
        } else if (c == '=') {
            p++;
            tok = TOK_A_SUB;
        } else if (c == '>') {
            p++;
            tok = TOK_ARROW;
        } else {
            tok = '-';
        }
        break;

    PARSE2('!', '!', '=', TOK_NE)
    PARSE2('=', '=', '=', TOK_EQ)
    PARSE2('*', '*', '=', TOK_A_MUL)
    case '%':
        PEEKC(c, p);
        if (c == '=') {
            p++;
            tok = TOK_A_MOD;
        } else if (c == '>') {     /* 6.4.6 digraph %> == } */
            p++;
            tok = '}';
        } else if (c == ':') {     /* 6.4.6 digraph %: == # , %:%: == ## */
            PEEKC(c, p);           /* c = char after the ':' */
            if (c == '%' && p[1] == ':') {
                p += 2;            /* consume the second "%:" */
                tok = TOK_TWOSHARPS;
            } else if ((tok_flags & TOK_FLAG_BOL)
                       && (parse_flags & PARSE_FLAG_PREPROCESS)) {
                tok_flags &= ~TOK_FLAG_BOL;
                file->buf_ptr = p;
                preprocess(tok_flags & TOK_FLAG_BOF);
                p = file->buf_ptr;
                goto maybe_newline;
            } else {
                tok = '#';
            }
        } else {
            tok = '%';
        }
        break;
    PARSE2('^', '^', '=', TOK_A_XOR)
        
    case '/':
        PEEKC(c, p);
        if (c == '*') {
            p = parse_comment(p);
            tok = ' ';
            goto maybe_space;
        } else if (c == '/') {
            p = parse_line_comment(p);
            tok = ' ';
            goto maybe_space;
        } else if (c == '=') {
            p++;
            tok = TOK_A_DIV;
        } else {
            tok = '/';
        }
        break;
        
    case '@':
#ifdef MCC_TARGET_ARM
        if (parse_flags & PARSE_FLAG_ASM_FILE) {
            p = parse_line_comment(p);
            goto redo_no_start;
        }
#endif
    /* fall through */
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case ',':
    case ';':
    case '?':
    case '~':
    parse_simple:
        tok = c;
        p++;
        break;
    case ':':
        PEEKC(c, p);
        if (c == '>') {            /* 6.4.6 digraph :> == ] */
            p++;
            tok = ']';
        } else {
            tok = ':';
        }
        break;
    case 0xEF:
        if (p[1] == 0xBB && p[2] == 0xBF && p == file->buffer) {
            p += 3;
            goto redo_no_start;
        }
        /* fall through */
    default:
        if (c >= 0x80 && c <= 0xFF)
	    goto parse_ident_fast;
        if (parse_flags & PARSE_FLAG_ASM_FILE)
            goto parse_simple;
        mcc_error("unrecognized character \\x%02x", c);
        break;
    }
    tok_flags = 0;
keep_tok_flags:
    file->buf_ptr = p;
#if defined(PARSE_DEBUG)
    printf("token = %d %s\n", tok, get_tok_str(tok, &tokc));
#endif
}

#ifdef PP_DEBUG
static int indent;
static void define_print(MCCState *s1, int v);
static void pp_print(const char *msg, int v, const int *str)
{
    FILE *fp = mcc_state->ppfp;

    if (msg[0] == '#' && indent == 0)
        fprintf(fp, "\n");
    else if (msg[0] == '+')
         ++indent, ++msg;
    else if (msg[0] == '-')
        --indent, ++msg;

    fprintf(fp, "%*s", indent, "");
    if (msg[0] == '#') {
        define_print(mcc_state, v);
    } else {
        tok_print(str, v ? "%s %s" : "%s", msg, get_tok_str(v, 0));
    }
}
#define PP_PRINT(x) pp_print x
#else
#define PP_PRINT(x)
#endif

static int macro_subst(
    TokenString *tok_str,
    Sym **nested_list,
    const int *macro_str
    );

static int *macro_arg_subst(Sym **nested_list, const int *macro_str, Sym *args)
{
    int t, t0, t1, t2, n;
    const int *st;
    Sym *s;
    CValue cval;
    TokenString str;

#ifdef PP_DEBUG
    PP_PRINT(("asubst:", 0, macro_str));
    for (s = args, n = 0; s; s = s->prev, ++n);
    while (n--) {
        for (s = args, t = 0; t < n; s = s->prev, ++t);
        tok_print(s->d, "%*s - arg: %s:", indent, "", get_tok_str(s->v, 0));
    }
#endif

    tok_str_new(&str);
    t0 = t1 = 0;
    while(1) {
        TOK_GET(&t, &macro_str, &cval);
        if (!t)
            break;
        if (t == '#') {
            do t = *macro_str++; while (t == ' ');
            s = sym_find2(args, t);
            if (s) {
                cstr_reset(&tokcstr);
                cstr_ccat(&tokcstr, '\"');
                st = s->d;
                while (*st != TOK_EOF) {
                    const char *s;
                    TOK_GET(&t, &st, &cval);
                    s = get_tok_str(t, &cval);
                    while (*s) {
                        if (t == TOK_PPSTR && *s != '\'')
                            add_char(&tokcstr, *s);
                        else
                            cstr_ccat(&tokcstr, *s);
                        ++s;
                    }
                }
                /* 6.10.3.2p2: a stringized argument ending in a stray backslash
                   (one not closing a string/char escape) would have the trailing
                   '\' escape the closing quote, yielding an invalid string
                   literal. gcc/clang drop the final backslash; do the same with a
                   warning. Count trailing backslashes (excluding the opening '"'
                   at index 0): an odd run means the last one is dangling. */
                {
                    int nb = 0;
                    while (tokcstr.size - 1 - nb >= 1
                           && tokcstr.data[tokcstr.size - 1 - nb] == '\\')
                        ++nb;
                    if (nb & 1) {
                        --tokcstr.size;
                        mcc_warning("invalid string literal, ignoring final '\\'");
                    }
                }
                cstr_ccat(&tokcstr, '\"');
                cstr_ccat(&tokcstr, '\0');
                cval.str.size = tokcstr.size;
                cval.str.data = tokcstr.data;
                tok_str_add2(&str, TOK_PPSTR, &cval);
#ifdef MCC_TARGET_ARM
            } else if ((parse_flags & PARSE_FLAG_ASM_FILE) && t == TOK_PPNUM) {
                --macro_str, tok_str_add(&str, '#');
#endif
            } else {
                expect("macro parameter after '#'");
            }
        } else if (t >= TOK_IDENT) {
            s = sym_find2(args, t);
            if (s) {
                st = s->d;
                n = 0;
                while ((t2 = macro_str[n]) == ' ')
                    ++n;
                if (t2 == TOK_PPJOIN || t1 == TOK_PPJOIN) {
                    if (t1 == TOK_PPJOIN && t0 == ',' && gnu_ext && s->type.t) {
                        int c = str.str[str.len - 1];
                        while (str.str[--str.len] != ',')
                            ;
                        if (*st == TOK_EOF) {
                        } else {
                            str.len++;
                            if (c == ' ')
                                str.str[str.len++] = c;
                            goto add_var;
                        }
                    } else {
                        if (*st == TOK_EOF)
                            tok_str_add(&str, TOK_PLCHLDR);
                    }
                } else {
            add_var:
		    if (!s->e) {
			TokenString str2;
			tok_str_new(&str2);
			macro_subst(&str2, nested_list, st);
			tok_str_add(&str2, TOK_EOF);
			s->e = str2.str;
		    }
		    st = s->e;
                }
                while (*st != TOK_EOF) {
                    TOK_GET(&t2, &st, &cval);
                    tok_str_add2(&str, t2, &cval);
                }
            } else {
                tok_str_add(&str, t);
            }
        } else {
            tok_str_add2(&str, t, &cval);
        }
        if (t != ' ')
            t0 = t1, t1 = t;
    }
    tok_str_add(&str, 0);
    PP_PRINT(("areslt:", 0, str.str));
    return str.str;
}

static inline int *macro_twosharps(const int *ptr0)
{
    int t1, t2, n, l;
    CValue cv1, cv2;
    TokenString macro_str1;
    const int *ptr;

    tok_str_new(&macro_str1);
    cstr_reset(&tokcstr);
    for (ptr = ptr0;;) {
        TOK_GET(&t1, &ptr, &cv1);
        if (t1 == 0)
            break;
        for (;;) {
            n = 0;
            while ((t2 = ptr[n]) == ' ')
                ++n;
            if (t2 != TOK_PPJOIN)
                break;
            ptr += n;
            while ((t2 = *++ptr) == ' ' || t2 == TOK_PPJOIN)
                ;
            TOK_GET(&t2, &ptr, &cv2);
            if (t2 == TOK_PLCHLDR)
                continue;
            if (t1 != TOK_PLCHLDR) {
                cstr_cat(&tokcstr, get_tok_str(t1, &cv1), -1);
                t1 = TOK_PLCHLDR;
            }
            cstr_cat(&tokcstr, get_tok_str(t2, &cv2), -1);
        }
        if (tokcstr.size) {
            int ci;
            cstr_ccat(&tokcstr, 0);
            /* 6.10.3.3p3: if ## produces a comment introducer (a line- or
               block-comment opener) the result is not a valid preprocessing
               token. Re-lexing it would run the comment scanner off the end of
               the synthetic :paste: buffer (no terminating newline) and loop
               forever, so diagnose and stop here as gcc/clang do, instead of
               re-lexing. */
            for (ci = 0; ci + 1 < tokcstr.size - 1; ci++) {
                char *d = (char *)tokcstr.data;
                if (d[ci] == '/' && (d[ci + 1] == '/' || d[ci + 1] == '*'))
                    mcc_error("pasting formed '%s', an invalid preprocessing token",
                              (char *)tokcstr.data);
            }
            mcc_open_bf(mcc_state, ":paste:", tokcstr.size);
            memcpy(file->buffer, tokcstr.data, tokcstr.size);
            tok_flags = 0;
            for (n = 0;;n = l) {
                next_nomacro();
                tok_str_add2(&macro_str1, tok, &tokc);
                if (*file->buf_ptr == 0)
                    break;
                tok_str_add(&macro_str1, ' ');
                l = file->buf_ptr - file->buffer;
                mcc_warning("pasting \"%.*s\" and \"%s\" does not give a valid"
                    " preprocessing token", l - n, file->buffer + n, file->buf_ptr);
            }
            mcc_close();
            cstr_reset(&tokcstr);
        }
        if (t1 != TOK_PLCHLDR)
            tok_str_add2(&macro_str1, t1, &cv1);
    }
    tok_str_add(&macro_str1, 0);
    PP_PRINT(("pasted:", 0, macro_str1.str));
    return macro_str1.str;
}

static int peek_file (TokenString *ws_str)
{
    uint8_t *p = file->buf_ptr - 1;
    int c;
    for (;;) {
        PEEKC(c, p);
        switch (c) {
        case '/':
            PEEKC(c, p);
            if (c == '*')
                p = parse_comment(p);
            else if (c == '/')
                p = parse_line_comment(p);
            else {
                c = *--p = '/';
                goto leave;
            }
            --p, c = ' ';
            break;
        case ' ': case '\t':
            break;
        case '\f': case '\v': case '\r':
            continue;
        case '\n':
            file->line_num++, tok_flags |= TOK_FLAG_BOL;
            break;
        default: leave:
            file->buf_ptr = p;
            return c;
        }
        if (ws_str)
            tok_str_add(ws_str, c);
    }
}

static int next_argstream(Sym **nested_list, TokenString *ws_str)
{
    int t;
    Sym *sa;

    while (macro_ptr) {
        const int *m = macro_ptr;
        while ((t = *m) != 0) {
            if (ws_str) {
                if (t != ' ')
                    return t;
                ++m;
            } else {
                TOK_GET(&tok, &macro_ptr, &tokc);
                return tok;
            }
        }
        end_macro();
        sa = *nested_list;
        if (sa)
            *nested_list = sa->prev, sym_free(sa);
    }
    if (ws_str) {
        return peek_file(ws_str);
    } else {
        next_nomacro();
        if (tok == '\t' || tok == TOK_LINEFEED)
            tok = ' ';
        return tok;
    }
}

static int macro_subst_tok(
    TokenString *tok_str,
    Sym **nested_list,
    Sym *s)
{
    int t;
    int v = s->v;

    PP_PRINT(("#", v, s->d));
    if (s->d) {
        int *mstr = s->d;
        int *jstr;
        Sym *sa;
        int ret;

        if (s->type.t & MACRO_FUNC) {
            int saved_parse_flags = parse_flags;
            TokenString str;
            int parlevel, i;
            Sym *sa1, *args;

            parse_flags |= PARSE_FLAG_SPACES | PARSE_FLAG_LINEFEED
                | PARSE_FLAG_ACCEPT_STRAYS;

            tok_str_new(&str);
            t = next_argstream(nested_list, &str);
            if (t != '(') {
                parse_flags = saved_parse_flags;
                tok_str_add2_spc(tok_str, v, 0);
                if (parse_flags & PARSE_FLAG_SPACES)
                    for (i = 0; i < str.len; i++)
                        tok_str_add(tok_str, str.str[i]);
                tok_str_free_str(str.str);
                return 0;
            } else {
                tok_str_free_str(str.str);
            }

            args = NULL;
            sa = s->next;
            i = 2;
            for(;;) {
                do {
                    t = next_argstream(nested_list, NULL);
                } while (t == ' ' || --i);

                if (!sa) {
                    if (t == ')')
                        break;
                    mcc_error("macro '%s' used with too many args",
                        get_tok_str(v, 0));
                }
            empty_arg:
                tok_str_new(&str);
                parlevel = 0;
                while (parlevel > 0
                        || (t != ')' && (t != ',' || sa->type.t))) {
                    if (t == TOK_EOF)
                        mcc_error("EOF in invocation of macro '%s'",
                            get_tok_str(v, 0));
                    if (t == '(')
                        parlevel++;
                    if (t == ')')
                        parlevel--;
                    if (t == ' ')
                        str.need_spc |= 1;
                    else
                        tok_str_add2_spc(&str, t, &tokc);
                    t = next_argstream(nested_list, NULL);
                }
                tok_str_add(&str, TOK_EOF);
                sa1 = sym_push2(&args, sa->v & ~SYM_FIELD, sa->type.t, 0);
                sa1->d = str.str;
                sa = sa->next;
                if (t == ')') {
                    if (!sa)
                        break;
                    if (sa->type.t && gnu_ext) {
                        /* 6.10.3p4 (pre-C23): when the parameter list ends with
                           '...' there must be more arguments than named
                           parameters. Invoking with no argument for the '...' is
                           a constraint violation; gcc/clang diagnose it under
                           -pedantic (it is made legal in C23), so do the same. */
                        if (mcc_state->warn_pedantic) {
                            if (mcc_state->pedantic_errors)
                                mcc_error("ISO C does not permit a variadic macro "
                                    "to be invoked with no argument for the '...'");
                            else
                                mcc_warning("ISO C does not permit a variadic macro "
                                    "to be invoked with no argument for the '...'");
                        }
                        goto empty_arg;
                    }
                    mcc_error("macro '%s' used with too few args",
                        get_tok_str(v, 0));
                }
                i = 1;
            }

            mstr = macro_arg_subst(nested_list, mstr, args);
            sa = args;
            while (sa) {
                sa1 = sa->prev;
                tok_str_free_str(sa->d);
                tok_str_free_str(sa->e);
                sym_free(sa);
                sa = sa1;
            }
            parse_flags = saved_parse_flags;
        }

        jstr = mstr;
        if (s->type.t & MACRO_JOIN)
            jstr = macro_twosharps(mstr);

        sa = sym_push2(nested_list, v, 0, 0);
        ret = macro_subst(tok_str, nested_list, jstr);
        if (sa == *nested_list)
            *nested_list = sa->prev, sym_free(sa);

        if (jstr != mstr)
            tok_str_free_str(jstr);
        if (mstr != s->d)
            tok_str_free_str(mstr);
        return ret;

    } else {
        CValue cval;
        char buf[32], *cstrval = buf;

        if (v == TOK___LINE__ || v == TOK___COUNTER__) {
            t = v == TOK___LINE__ ? file->line_num : pp_counter++;
            snprintf(buf, sizeof(buf), "%d", t);
            t = TOK_PPNUM;
            goto add_cstr1;

        } else if (v == TOK___FILE__) {
            cstrval = file->filename;
            goto add_cstr;

        } else if (v == TOK___DATE__ || v == TOK___TIME__) {
            time_t ti;
            struct tm *tm;
            time(&ti);
            tm = localtime(&ti);
            if (v == TOK___DATE__) {
                static char const ab_month_name[12][4] = {
                    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
                };
                snprintf(buf, sizeof(buf), "%s %2d %d",
                    ab_month_name[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900);
            } else {
                snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                    tm->tm_hour, tm->tm_min, tm->tm_sec);
            }
        add_cstr:
            t = TOK_STR;
        add_cstr1:
            cval.str.size = strlen(cstrval) + 1;
            cval.str.data = cstrval;
            tok_str_add2_spc(tok_str, t, &cval);
        }
        return 0;
    }
}

static int macro_subst(
    TokenString *tok_str,
    Sym **nested_list,
    const int *macro_str
    )
{
    Sym *s;
    int t, nosubst = 0;
    CValue cval;
    TokenString *str;

#ifdef PP_DEBUG
    int tlen = tok_str->len;
    PP_PRINT(("+expand:", 0, macro_str));
#endif

    while (1) {
        TOK_GET(&t, &macro_str, &cval);
        if (t == 0 || t == TOK_EOF)
            break;
        if (t >= TOK_IDENT) {
            s = define_find(t);
            if (s == NULL || nosubst)
                goto no_subst;
            if (sym_find2(*nested_list, t)) {
                t |= SYM_FIELD;
                goto no_subst;
            }
            str = tok_str_alloc();
            str->str = (int*)macro_str;
            begin_macro(str, 2);
            nosubst = macro_subst_tok(tok_str, nested_list, s);
            if (macro_stack != str) {
                break;
            }
            macro_str = macro_ptr;
            end_macro ();
        } else if (t == ' ') {
            if (parse_flags & PARSE_FLAG_SPACES)
                tok_str->need_spc |= 1;
        } else {
    no_subst:
            tok_str_add2_spc(tok_str, t, &cval);
            if (nosubst && t != '(')
                nosubst = 0;
            if (t == TOK_DEFINED && pp_expr)
                nosubst = 1;
        }
    }

#ifdef PP_DEBUG
    tok_str_add(tok_str, 0), --tok_str->len;
    PP_PRINT(("-result:", 0, tok_str->str + tlen));
#endif
    return nosubst;
}

/* 6.10.9: process `_Pragma ( string-literal )` as the corresponding #pragma.
   The string content is already destringized by parse_string, so it is fed to
   the pragma directive handler via a temporary buffer (compile mode only; in -E
   mode _Pragma is left as tokens for the existing pass-through). */
static void pragma_operator(void)
{
    MCCState *s1 = mcc_state;
    const int *saved_macro_ptr;
    char *content;
    int n;

    next();                              /* '(' */
    if (tok != '(')
        return;
    next();                              /* string-literal */
    if (tok != TOK_STR) {
        while (tok != ')' && tok != TOK_EOF && tok != TOK_LINEFEED)
            next();
        return;
    }
    n = tokc.str.size - 1;               /* content length, excluding NUL */
    content = mcc_malloc(n + 2);
    memcpy(content, tokc.str.data, n);
    content[n] = '\n';                   /* the lexer wants a line terminator */
    content[n + 1] = 0;
    next();                              /* ')' */

    saved_macro_ptr = macro_ptr;
    mcc_open_bf(s1, ":pragma:", n + 1);
    memcpy(file->buffer, content, n + 1);
    macro_ptr = NULL;
    pragma_parse(s1);
    mcc_close();
    macro_ptr = saved_macro_ptr;
    mcc_free(content);
}

ST_FUNC void next(void)
{
    int t;
    while (macro_ptr) {
redo:
        t = *macro_ptr;
        if (TOK_HAS_VALUE(t)) {
            tok_get(&tok, &macro_ptr, &tokc);
            if (t == TOK_LINENUM) {
                file->line_num = tokc.i;
                goto redo;
            }
            goto convert;
        } else if (t == 0) {
            end_macro();
            continue;
        } else if (t == TOK_EOF) {
        } else {
            ++macro_ptr;
            t &= ~SYM_FIELD;
            if (t == '\\') {
                if (!(parse_flags & PARSE_FLAG_ACCEPT_STRAYS))
                    mcc_error("stray '\\' in program");
            }
        }
        tok = t;
        if (t == TOK__Pragma
            && (parse_flags & PARSE_FLAG_PREPROCESS)
            && mcc_state->output_type != MCC_OUTPUT_PREPROCESS) {
            pragma_operator();
            goto redo;
        }
        return;
    }

    next_nomacro();
    t = tok;
    if (t == TOK__Pragma
        && (parse_flags & PARSE_FLAG_PREPROCESS)
        && mcc_state->output_type != MCC_OUTPUT_PREPROCESS) {
        pragma_operator();
        next();
        return;
    }
    if (t >= TOK_IDENT && (parse_flags & PARSE_FLAG_PREPROCESS)) {
        Sym *s = define_find(t);
        if (s) {
            Sym *nested_list = NULL;
            macro_subst_tok(&tokstr_buf, &nested_list, s);
            tok_str_add(&tokstr_buf, 0);
            begin_macro(&tokstr_buf, 0);
            goto redo;
        }
        return;
    }

convert:
    if (t == TOK_PPNUM) {
        if  (parse_flags & PARSE_FLAG_TOK_NUM)
            parse_number(tokc.str.data);
    } else if (t == TOK_PPSTR) {
        if (parse_flags & PARSE_FLAG_TOK_STR)
            parse_string(tokc.str.data, tokc.str.size - 1);
    }
}

ST_INLN void unget_tok(int last_tok)
{
    TokenString *str = &unget_buf;
    int alloc = 0;
    if (str->len)
        str = tok_str_alloc(), alloc = 1;
    if (tok != TOK_EOF)
        tok_str_add2(str, tok, &tokc);
    tok_str_add(str, 0);
    begin_macro(str, alloc);
    tok = last_tok;
}


static const char * const target_os_defs =
#ifdef MCC_TARGET_PE
    "_WIN32\0"
# if PTR_SIZE == 8
    "_WIN64\0"
# endif
#else
# if defined MCC_TARGET_MACHO
    "__APPLE__\0"
# elif TARGETOS_FreeBSD
    "__FreeBSD__ 12\0"
# elif TARGETOS_FreeBSD_kernel
    "__FreeBSD_kernel__\0"
# elif TARGETOS_NetBSD
    "__NetBSD__\0"
# elif TARGETOS_OpenBSD
    "__OpenBSD__\0"
# else
    "__linux__\0"
    "__linux\0"
#  if TARGETOS_ANDROID
    "__ANDROID__\0"
#  endif
# endif
    "__unix__\0"
    "__unix\0"
#endif
    ;

static void putdef(CString *cs, const char *p)
{
    cstr_printf(cs, "#define %s%s\n", p, &" 1"[!!strchr(p, ' ')*2]);
}

static void putdefs(CString *cs, const char *p)
{
    while (*p)
        putdef(cs, p), p = strchr(p, 0) + 1;
}

static void mcc_predefs(MCCState *s1, CString *cs, int is_asm)
{
    cstr_printf(cs, "#define __MCC__ 9%.2s\n", &MCC_VERSION[4]);
    cstr_printf(cs, "#define __TINYC__ 9%.2s\n", &MCC_VERSION[4]); // glibc's <sys/cdefs.h> needs this
    putdefs(cs, target_machine_defs);
    putdefs(cs, target_os_defs);

#ifdef MCC_TARGET_ARM
    if (s1->float_abi == ARM_HARD_FLOAT)
      putdef(cs, "__ARM_PCS_VFP");
#endif
    if (is_asm)
      putdef(cs, "__ASSEMBLER__");
    if (s1->output_type == MCC_OUTPUT_PREPROCESS)
      putdef(cs, "__MCC_PP__");
    if (s1->output_type == MCC_OUTPUT_MEMORY)
      putdef(cs, "__MCC_RUN__");
#ifdef CONFIG_MCC_BACKTRACE
    if (s1->do_backtrace)
      putdef(cs, "__MCC_BACKTRACE__");
#endif
#ifdef CONFIG_MCC_BCHECK
    if (s1->do_bounds_check)
      putdef(cs, "__MCC_BCHECK__");
#endif
    if (s1->char_is_unsigned)
      putdef(cs, "__CHAR_UNSIGNED__");
    if (s1->optimize > 0)
      putdef(cs, "__OPTIMIZE__");
    if (s1->optimize_size)
      putdef(cs, "__OPTIMIZE_SIZE__");
    if (s1->option_pthread)
      putdef(cs, "_REENTRANT");
    if (s1->leading_underscore)
      putdef(cs, "__leading_underscore");
    cstr_printf(cs, "#define __SIZEOF_POINTER__ %d\n", PTR_SIZE);
    cstr_printf(cs, "#define __SIZEOF_LONG__ %d\n", LONG_SIZE);
    if (!is_asm) {
      putdef(cs, "__STDC__");
      cstr_printf(cs, "#define __STDC_HOSTED__ %d\n",
                  (s1->nostdlib || s1->freestanding) ? 0 : 1);
      if (s1->cversion)        /* undefined under -std=c89 */
          cstr_printf(cs, "#define __STDC_VERSION__ %dL\n", s1->cversion);
      cstr_cat(cs,
#if CONFIG_MCC_PREDEFS
        #include "mccdefs_.h"
#else
        "#include <mccdefs.h>\n"
#endif
        , -1);
    }
    cstr_printf(cs, "#define __BASE_FILE__ \"%s\"\n", file->filename);
}

ST_FUNC void preprocess_start(MCCState *s1, int filetype)
{
    int is_asm = !!(filetype & (AFF_TYPE_ASM|AFF_TYPE_ASMPP));

    mccpp_new(s1);

    s1->include_stack_ptr = s1->include_stack;
    s1->ifdef_stack_ptr = s1->ifdef_stack;
    file->ifdef_stack_ptr = s1->ifdef_stack_ptr;
    pp_expr = 0;
    pp_counter = 0;
    pp_debug_tok = pp_debug_symv = 0;
    s1->pack_stack[0] = 0;
    s1->pack_stack_ptr = s1->pack_stack;

    set_idnum('$', s1->dollars_in_identifiers ? IS_ID : 0);
    set_idnum('.', is_asm ? IS_ID : 0);

    if (!(filetype & AFF_TYPE_ASM)) {
        CString cstr;
        cstr_new(&cstr);
        mcc_predefs(s1, &cstr, is_asm);
        if (s1->cmdline_defs.size)
          cstr_cat(&cstr, s1->cmdline_defs.data, s1->cmdline_defs.size);
        if (s1->cmdline_incl.size)
          cstr_cat(&cstr, s1->cmdline_incl.data, s1->cmdline_incl.size);
        *s1->include_stack_ptr++ = file;
        mcc_open_bf(s1, "<command line>", cstr.size);
        /* the predef buffer (mcc's builtin defs + bundled mccdefs.h: va_list,
           __int128, the __builtin macros, ...) is a system context — suppress
           warnings/-pedantic originating in it (e.g. the va_list anon union). */
        file->system_header = 1;
        memcpy(file->buffer, cstr.data, cstr.size);
        cstr_free(&cstr);
    }
    parse_flags = is_asm ? PARSE_FLAG_ASM_FILE : 0;
}

ST_FUNC void preprocess_end(MCCState *s1)
{
    while (macro_stack)
        end_macro();
    macro_ptr = NULL;
    while (file)
        mcc_close();
    mccpp_delete(s1);
}

ST_FUNC int set_idnum(int c, int val)
{
    int prev = isidnum_table[c - CH_EOF];
    isidnum_table[c - CH_EOF] = val;
    return prev;
}

ST_FUNC void mccpp_new(MCCState *s)
{
    int c;
    const char *p, *r;

    for(int i = CH_EOF; i<128; i++)
        set_idnum(i,
            is_space(i) ? IS_SPC
            : isid(i) ? IS_ID
            : isnum(i) ? IS_NUM
            : 0);

    for(int i = 128; i<256; i++)
        set_idnum(i, IS_ID);

    tal_new(&toksym_alloc, TOKSYM_TAL_SIZE);
    tal_new(&tokstr_alloc, TOKSTR_TAL_SIZE);

    memset(hash_ident, 0, TOK_HASH_SIZE * sizeof(TokenSym *));
    memset(s->cached_includes_hash, 0, sizeof s->cached_includes_hash);

    cstr_new(&tokcstr);
    cstr_new(&cstr_buf);
    cstr_realloc(&cstr_buf, STRING_MAX_SIZE);
    tok_str_new(&unget_buf);
    tok_str_realloc(&unget_buf, TOKSTR_MAX_SIZE);
    tok_str_new(&tokstr_buf);
    tok_str_realloc(&tokstr_buf, TOKSTR_MAX_SIZE);

    tok_ident = TOK_IDENT;
    p = mcc_keywords;
    while (*p) {
        r = p;
        for(;;) {
            c = *r++;
            if (c == '\0')
                break;
        }
        tok_alloc(p, r - p - 1);
        p = r;
    }

    define_push(TOK___LINE__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___FILE__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___DATE__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___TIME__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___COUNTER__, MACRO_OBJ, NULL, NULL);
}

ST_FUNC void mccpp_delete(MCCState *s)
{
    int n;

    dynarray_reset(&s->cached_includes, &s->nb_cached_includes);

    n = tok_ident - TOK_IDENT;
    if (n > total_idents)
        total_idents = n;
    for (int i = n; --i >= 0;)
        tal_free(&toksym_alloc, table_ident[i]);
    mcc_free(table_ident);
    table_ident = NULL;

    cstr_free(&tokcstr);
    cstr_free(&cstr_buf);
    tok_str_free_str(tokstr_buf.str);
    tok_str_free_str(unget_buf.str);

    tal_delete(&toksym_alloc);
    tal_delete(&tokstr_alloc);
}


static int pp_need_space(int a, int b);

static void tok_print(const int *str, const char *msg, ...)
{
    FILE *fp = mcc_state->ppfp;
    va_list ap;
    int t, t0, s;
    CValue cval;

    va_start(ap, msg);
    vfprintf(fp, msg, ap);
    va_end(ap);

    s = t0 = 0;
    while (str) {
	TOK_GET(&t, &str, &cval);
	if (t == 0 || t == TOK_EOF)
	    break;
        if (pp_need_space(t0, t))
            s = 0;
	fprintf(fp, &" %s"[s], t == TOK_PLCHLDR ? "<>" : get_tok_str(t, &cval));
        s = 1, t0 = t;
    }
    fprintf(fp, "\n");
}

static void pp_line(MCCState *s1, BufferedFile *f, int level)
{
    int d = f->line_num - f->line_ref;

    if (s1->dflag & 4)
	return;

    if (s1->Pflag == LINE_MACRO_OUTPUT_FORMAT_NONE) {
        ;
    } else if (level == 0 && f->line_ref && d < 8) {
	while (d > 0)
	    fputs("\n", s1->ppfp), --d;
    } else if (s1->Pflag == LINE_MACRO_OUTPUT_FORMAT_STD) {
	fprintf(s1->ppfp, "#line %d \"%s\"\n", f->line_num, f->filename);
    } else {
	fprintf(s1->ppfp, "# %d \"%s\"%s\n", f->line_num, f->filename,
	    level > 0 ? " 1" : level < 0 ? " 2" : "");
    }
    f->line_ref = f->line_num;
}

static void define_print(MCCState *s1, int v)
{
    FILE *fp;
    Sym *s;

    s = define_find(v);
    if (NULL == s || NULL == s->d)
        return;

    fp = s1->ppfp;
    fprintf(fp, "#define %s", get_tok_str(v, NULL));
    if (s->type.t & MACRO_FUNC) {
        Sym *a = s->next;
        fprintf(fp,"(");
        if (a)
            for (;;) {
                fprintf(fp,"%s", get_tok_str(a->v, NULL));
                if (!(a = a->next))
                    break;
                fprintf(fp,",");
            }
        fprintf(fp,")");
    }
    tok_print(s->d, "");
}

static void pp_debug_defines(MCCState *s1)
{
    int v, t;
    const char *vs;
    FILE *fp;

    t = pp_debug_tok;
    if (t == 0)
        return;

    file->line_num--;
    pp_line(s1, file, 0);
    file->line_ref = ++file->line_num;

    fp = s1->ppfp;
    v = pp_debug_symv;
    vs = get_tok_str(v, NULL);
    if (t == TOK_DEFINE) {
        define_print(s1, v);
    } else if (t == TOK_UNDEF) {
        fprintf(fp, "#undef %s\n", vs);
    } else if (t == TOK_push_macro) {
        fprintf(fp, "#pragma push_macro(\"%s\")\n", vs);
    } else if (t == TOK_pop_macro) {
        fprintf(fp, "#pragma pop_macro(\"%s\")\n", vs);
    }
    pp_debug_tok = 0;
}

static int pp_need_space(int a, int b)
{
    return 'E' == a ? '+' == b || '-' == b
        : '+' == a ? TOK_INC == b || '+' == b
        : '-' == a ? TOK_DEC == b || '-' == b
        : a >= TOK_IDENT || a == TOK_PPNUM ? b >= TOK_IDENT || b == TOK_PPNUM
        : 0;
}

static int pp_check_he0xE(int t, const char *p)
{
    if (t == TOK_PPNUM && toup(strchr(p, 0)[-1]) == 'E')
        return 'E';
    return t;
}

/* 6.10.9: under -E, re-emit a _Pragma("...") operator as a '#pragma <text>'
   directive on its own line (instead of passing the _Pragma ( "..." ) tokens
   through verbatim).  Consumes the '(' string ')' that follow the already-read
   TOK__Pragma; the string is used as-is (parse_string already destringized
   it, matching the compile-mode pragma_operator()).  *ptoken_seen is left as
   TOK_LINEFEED so the next real token re-syncs the line marker via pp_line. */
static void pp_pragma_operator(MCCState *s1, int *ptoken_seen)
{
    const char *raw;
    char *content, *q;

    next();                              /* '(' */
    if (tok != '(')
        return;
    next();                              /* string-literal */
    /* In -E the lexer leaves a string as TOK_PPSTR holding its raw spelling
       (PARSE_FLAG_TOK_STR is off), so destringize here per 6.10.9 rather than
       reading a parsed tokc.str. */
    if (tok != TOK_PPSTR && tok != TOK_STR) {
        while (tok != ')' && tok != TOK_EOF && tok != TOK_LINEFEED)
            next();
        return;
    }
    raw = get_tok_str(tok, &tokc);
    /* 6.10.9 destringize: drop an encoding prefix (L/u/U/u8) and the enclosing
       double-quotes, replace \" with " and \\ with \. */
    while (*raw && *raw != '"')
        raw++;                           /* skip past prefix to opening quote */
    if (*raw == '"')
        raw++;
    content = mcc_malloc(strlen(raw) + 1);
    q = content;
    while (*raw && *raw != '"') {
        if (*raw == '\\' && (raw[1] == '"' || raw[1] == '\\'))
            raw++;
        *q++ = *raw++;
    }
    *q = 0;
    next();                              /* ')' */

    if (*ptoken_seen != TOK_LINEFEED)    /* break off the current line first */
        fputc('\n', s1->ppfp);
    fputs("#pragma ", s1->ppfp);
    fputs(content, s1->ppfp);
    fputc('\n', s1->ppfp);
    mcc_free(content);
    *ptoken_seen = TOK_LINEFEED;
}

ST_FUNC int mcc_preprocess(MCCState *s1)
{
    BufferedFile **iptr;
    int token_seen, spcs, level;
    const char *p;
    char white[400];

    parse_flags = PARSE_FLAG_PREPROCESS
                | (parse_flags & PARSE_FLAG_ASM_FILE)
                | PARSE_FLAG_LINEFEED
                | PARSE_FLAG_SPACES
                | PARSE_FLAG_ACCEPT_STRAYS
                ;
    if (s1->Pflag == LINE_MACRO_OUTPUT_FORMAT_P10)
        parse_flags |= PARSE_FLAG_TOK_NUM, s1->Pflag = 1;

    if (s1->do_bench) {
	do next(); while (tok != TOK_EOF);
	return 0;
    }

    token_seen = TOK_LINEFEED, spcs = 0, level = 0;
    if (file->prev)
        pp_line(s1, file->prev, level++);
    pp_line(s1, file, level);

    for (;;) {
        iptr = s1->include_stack_ptr;
        next();
        if (tok == TOK_EOF)
            break;

        level = s1->include_stack_ptr - iptr;
        if (level) {
            if (level > 0)
                pp_line(s1, *iptr, 0);
            pp_line(s1, file, level);
        }
        if (s1->dflag & 7) {
            pp_debug_defines(s1);
            if (s1->dflag & 4)
                continue;
        }

        if (tok == TOK__Pragma) {
            spcs = 0;                    /* drop any pending leading whitespace */
            pp_pragma_operator(s1, &token_seen);
            continue;
        }

        if (is_space(tok)) {
            if (spcs < sizeof white - 1)
                white[spcs++] = tok;
            continue;
        } else if (tok == TOK_LINEFEED) {
            spcs = 0;
            if (token_seen == TOK_LINEFEED)
                continue;
            ++file->line_ref;
        } else if (token_seen == TOK_LINEFEED) {
            pp_line(s1, file, 0);
        } else if (spcs == 0 && pp_need_space(token_seen, tok)) {
            white[spcs++] = ' ';
        }

        white[spcs] = 0, fputs(white, s1->ppfp), spcs = 0;
        fputs(p = get_tok_str(tok, &tokc), s1->ppfp);
        token_seen = pp_check_he0xE(tok, p);
    }
    return 0;
}

