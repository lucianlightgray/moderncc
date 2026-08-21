#define USING_GLOBALS
#include "mcc.h"

#define ACCEPT_LF_IN_STRINGS 0

ST_DATA int tok_flags;
ST_DATA int parse_flags;

ST_DATA struct BufferedFile *file;
ST_DATA int tok;
ST_DATA CValue tokc;
#define tok_ts (mcc_state->tok_ts)
ST_DATA int tok_imaginary;
ST_DATA int tok_bitint_width;
ST_DATA const int *macro_ptr;
ST_DATA CString tokcstr;

ST_DATA int tok_ident;
ST_DATA TokenSym **table_ident;
ST_DATA int pp_expr;

static int64_t tok_line_num;
static struct BufferedFile *tok_line_file;
static int tok_va_opt;

static int *pp_poison;
static int pp_npoison;

/* T-mac-30219: set once the <command line> preamble crosses the internal marker
 * that separates the built-in predefs (suppressed) from the user's -D options
 * (warnable), so a redefinition in the user region warns like gcc/clang. */
static int pp_cmdline_user;
static int is_predef_macro(int v);

static int pp_is_poisoned(int t) { MCC_TRACE("enter\n");
	int i;
	for (i = 0; i < pp_npoison; i++)
		{ MCC_TRACE("br\n"); if (pp_poison[i] == t)
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int tok_has_attribute;
static int tok_has_c_attribute;
static int tok_has_cpp_attribute;
static int tok_has_builtin;
static int tok_has_feature;
static int tok_has_extension;

static int tok_c23_bool;
static int tok_c23_true;
static int tok_c23_false;
static int tok_c23_static_assert;
static int tok_c23_alignas;
static int tok_c23_alignof;
static int tok_c23_thread_local;

#define hash_ident (mcc_state->hash_ident)
#define token_buf (mcc_state->token_buf)
#define cstr_buf (mcc_state->cstr_buf)
#define tokstr_buf (mcc_state->tokstr_buf)
#define unget_buf (mcc_state->unget_buf)
#define isidnum_table (mcc_state->isidnum_table)
#define pp_debug_tok (mcc_state->pp_debug_tok)
#define pp_debug_symv (mcc_state->pp_debug_symv)
#define pp_counter (mcc_state->pp_counter)
static void tok_print(const int *str, const char *msg, ...);
static void next_nomacro(void);
static void parse_number(const char *p);
static void parse_string(const char *p, int len);

ST_FUNC int pp_in_system_header(void) { MCC_TRACE("enter\n");
	BufferedFile *wf;
	if (!mcc_state->error_set_jmp_enabled)
		{ MCC_TRACE("br\n"); return 0; }
	for (wf = file; wf && wf->filename[0] == ':'; wf = wf->prev)
		;
	return wf && wf->system_header;
}

#define toksym_alloc (mcc_state->toksym_alloc)
#define tokstr_alloc (mcc_state->tokstr_alloc)

#define macro_stack (mcc_state->macro_stack)

static const char mcc_keywords[] =
#define DEF(id, str) str "\0"
#include "mcctok.h"

#undef DEF
		;

static const unsigned char tok_two_chars[] =
		{
				'<', '=', TOK_LE,
				'>', '=', TOK_GE,
				'!', '=', TOK_NE,
				'&', '&', TOK_LAND,
				'|', '|', TOK_LOR,
				'+', '+', TOK_INC,
				'-', '-', TOK_DEC,
				'=', '=', TOK_EQ,
				'<', '<', TOK_SHL,
				'>', '>', TOK_SAR,
				'+', '=', TOK_A_ADD,
				'-', '=', TOK_A_SUB,
				'*', '=', TOK_A_MUL,
				'/', '=', TOK_A_DIV,
				'%', '=', TOK_A_MOD,
				'&', '=', TOK_A_AND,
				'^', '=', TOK_A_XOR,
				'|', '=', TOK_A_OR,
				'-', '>', TOK_ARROW,
				'.', '.', TOK_TWODOTS,
				'#', '#', TOK_TWOSHARPS,
				0};

ST_FUNC void skip(int c) { MCC_TRACE("enter\n");
	if (tok != c) { MCC_TRACE("br\n");
		char tmp[40];
		pstrcpy(tmp, sizeof tmp, get_tok_str(c, &tokc));
		mcc_error("'%s' expected (got '%s')", tmp, get_tok_str(tok, &tokc));
	}
	next();
}

ST_FUNC void expect(const char *msg) { MCC_TRACE("enter\n");
	mcc_error("%s expected", msg);
}

#define MCC_USE_TAL

#ifndef MCC_USE_TAL
#define tal_free(al, p) mcc_free(p)
#define tal_realloc(al, p, size) mcc_realloc(p, size)
#define tal_new(a, b)
#define tal_delete(a)
#else
#if MCC_DIAG
#define tal_free(al, p) tal_free_impl(al, p, __FILE__, __LINE__)
#define tal_realloc(al, p, size) tal_realloc_impl(al, p, size, __FILE__, __LINE__)
#define MCC_TAL_DEBUG_PARAMS , const char *sfile, int sline
#else
#define tal_free(al, p) tal_free_impl(al, p)
#define tal_realloc(al, p, size) tal_realloc_impl(al, p, size)
#define MCC_TAL_DEBUG_PARAMS
#endif

#define TOKSYM_TAL_SIZE (256 * 1024)
#define TOKSTR_TAL_SIZE (256 * 1024)

typedef struct TinyAlloc {
	uint8_t *p;
	uint8_t *bufend;
	struct TinyAlloc *next;
	unsigned nb_allocs;
	unsigned size;
	union {
		uint8_t buffer[1];
		size_t _aligner_;
	};
} TinyAlloc;

typedef struct tal_header_t {
	size_t size;
#if MCC_DIAG
	int line_num;
	char file_name[40];
#endif
} tal_header_t;

#define TAL_ALIGN(size) \
	(((size) + (sizeof(size_t) - 1)) & ~(sizeof(size_t) - 1))

static TinyAlloc *tal_new(TinyAlloc **pal, unsigned size) { MCC_TRACE("enter\n");
	TinyAlloc *al = mcc_malloc(sizeof(TinyAlloc) - sizeof(size_t) + size);
	al->p = al->buffer;
	al->bufend = al->buffer + size;
	al->nb_allocs = 0;
	al->next = *pal, *pal = al;
	al->size = al->next ? al->next->size : size;
	return al;
}

static void tal_delete(TinyAlloc **pal) { MCC_TRACE("enter\n");
	TinyAlloc *al = *pal, *next;

tail_call:
#if MCC_DIAG
	if (al->nb_allocs > 0 && !mcc_leakcheck_quiet) { MCC_TRACE("br\n");
		uint8_t *p;
		fprintf(stderr, "mcc: tiny-alloc leak %d chunk(s)\n", al->nb_allocs);
		p = al->buffer;
		while (p < al->p) { MCC_TRACE("br\n");
			tal_header_t *header = (tal_header_t *)p;
			if (header->line_num > 0) { MCC_TRACE("br\n");
				fprintf(stderr, "%s:%d: chunk of %d bytes leaked\n",
								header->file_name, header->line_num, (int)header->size);
			}
			p += header->size + sizeof(tal_header_t);
		}
	}
#endif
	next = al->next;
	mcc_free(al);
	al = next;
	if (al)
		{ MCC_TRACE("br\n"); goto tail_call; }
	*pal = al;
}

static void tal_free_impl(TinyAlloc **pal, void *p MCC_TAL_DEBUG_PARAMS) { MCC_TRACE("enter\n");
	TinyAlloc *al, **top = pal;
	tal_header_t *header;

	if (!p)
		{ MCC_TRACE("br\n"); return; }
	header = (tal_header_t *)p - 1;
#if MCC_DIAG
	if (header->line_num < 0) { MCC_TRACE("br\n");
		fprintf(stderr, "%s:%d: mcc: tiny-alloc double free of chunk from\n",
						sfile, sline);
		fprintf(stderr, "%s:%d: %d bytes\n",
						header->file_name, (int)-header->line_num, (int)header->size);
	} else
		{ MCC_TRACE("br\n"); header->line_num = -header->line_num; }
#endif
	al = *pal;
	while ((uint8_t *)p < al->buffer || (uint8_t *)p > al->bufend)
		{ MCC_TRACE("br\n"); al = *(pal = &al->next); }
	if (0 == --al->nb_allocs) { MCC_TRACE("br\n");
		*pal = al->next;
		if ((al->bufend - al->buffer) > al->size) { MCC_TRACE("br\n");
			mcc_free(al);
		} else { MCC_TRACE("br\n");
			al->p = al->buffer;
			al->next = *top, *top = al;
		}
	} else if ((uint8_t *)p + header->size == al->p) { MCC_TRACE("br\n");
		al->p = (uint8_t *)header;
	}
}

static void *tal_realloc_impl(TinyAlloc **pal, void *p, unsigned size MCC_TAL_DEBUG_PARAMS) { MCC_TRACE("enter\n");
	tal_header_t *header;
	void *ret;
	unsigned adj_size = TAL_ALIGN(size) + sizeof(tal_header_t);
	TinyAlloc *al = *pal;

	if (p) { MCC_TRACE("br\n");
		while ((uint8_t *)p < al->buffer || (uint8_t *)p > al->bufend)
			{ MCC_TRACE("br\n"); al = al->next; }
		header = (tal_header_t *)p - 1;
		if ((uint8_t *)p + header->size == al->p)
			{ MCC_TRACE("br\n"); al->p = (uint8_t *)header; }
		if (al->p + adj_size > al->bufend) { MCC_TRACE("br\n");
			ret = tal_realloc(pal, 0, size);
			memcpy(ret, p, header->size);
			tal_free(pal, p);
			return ret;
		} else if (al->p != (uint8_t *)header) { MCC_TRACE("br\n");
			memcpy((tal_header_t *)al->p + 1, p, header->size);
#if MCC_DIAG
			header->line_num = -header->line_num;
#endif
		}
	} else { MCC_TRACE("br\n");
		while (al->p + adj_size > al->bufend) { MCC_TRACE("br\n");
			al = al->next;
			if (!al) { MCC_TRACE("br\n");
				unsigned new_size = (*pal)->size;
				if (adj_size > new_size) { MCC_TRACE("br\n");
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
#if MCC_DIAG
	{
		int ofs = strlen(sfile) + 1 - sizeof header->file_name;
		strcpy(header->file_name, sfile + (ofs > 0 ? ofs : 0));
		header->line_num = sline;
	}
#endif
	return ret;
}

#endif

static void cstr_realloc(CString *cstr, int new_size) { MCC_TRACE("enter\n");
	int size;

	size = mcc_grow_capacity(cstr->size_allocated, new_size, 8);
	cstr->data = mcc_realloc(cstr->data, size);
	cstr->size_allocated = size;
}

ST_INLN void cstr_ccat(CString *cstr, int ch) { MCC_TRACE("enter\n");
	int size;
	size = cstr->size + 1;
	if (size > cstr->size_allocated)
		{ MCC_TRACE("br\n"); cstr_realloc(cstr, size); }
	cstr->data[size - 1] = ch;
	cstr->size = size;
}

ST_INLN char *unicode_to_utf8(char *b, uint32_t Uc) { MCC_TRACE("enter\n");
	if (Uc < 0x80)
		{ MCC_TRACE("br\n"); *b++ = Uc; }
	else if (Uc < 0x800)
		{ MCC_TRACE("br\n"); *b++ = 192 + Uc / 64, *b++ = 128 + Uc % 64; }
	else if (Uc - 0xd800u < 0x800)
		{ MCC_TRACE("br\n"); goto error; }
	else if (Uc < 0x10000)
		{ MCC_TRACE("br\n"); *b++ = 224 + Uc / 4096, *b++ = 128 + Uc / 64 % 64, *b++ = 128 + Uc % 64; }
	else if (Uc < 0x110000)
		{ MCC_TRACE("br\n"); *b++ = 240 + Uc / 262144, *b++ = 128 + Uc / 4096 % 64, *b++ = 128 + Uc / 64 % 64, *b++ = 128 + Uc % 64; }
	else
	{ MCC_TRACE("br\n"); error:
		mcc_error("0x%x is not a valid universal character", Uc); }
	return b;
}

ST_INLN void cstr_u8cat(CString *cstr, int ch) { MCC_TRACE("enter\n");
	char buf[4], *e;
	e = unicode_to_utf8(buf, (uint32_t)ch);
	cstr_cat(cstr, buf, e - buf);
}

static int ucn_allowed_in_identifier(unsigned int v) { MCC_TRACE("enter\n");
	static const struct
	{
		unsigned lo, hi;
	} r[] = {
			{0x00A8, 0x00A8},
			{0x00AA, 0x00AA},
			{0x00AD, 0x00AD},
			{0x00AF, 0x00AF},
			{0x00B2, 0x00B5},
			{0x00B7, 0x00BA},
			{0x00BC, 0x00BE},
			{0x00C0, 0x00D6},
			{0x00D8, 0x00F6},
			{0x00F8, 0x00FF},
			{0x0100, 0x167F},
			{0x1681, 0x180D},
			{0x180F, 0x1FFF},
			{0x200B, 0x200D},
			{0x202A, 0x202E},
			{0x203F, 0x2040},
			{0x2054, 0x2054},
			{0x2060, 0x206F},
			{0x2070, 0x218F},
			{0x2460, 0x24FF},
			{0x2776, 0x2793},
			{0x2C00, 0x2DFF},
			{0x2E80, 0x2FFF},
			{0x3004, 0x3007},
			{0x3021, 0x302F},
			{0x3031, 0x303F},
			{0x3040, 0xD7FF},
			{0xF900, 0xFD3D},
			{0xFD40, 0xFDCF},
			{0xFDF0, 0xFE44},
			{0xFE47, 0xFFFD},
	};
	unsigned i;
	for (i = 0; i < sizeof r / sizeof r[0]; i++)
		{ MCC_TRACE("br\n"); if (v >= r[i].lo && v <= r[i].hi)
			{ MCC_TRACE("br\n"); return 1; } }
	if (v >= 0x10000 && v <= 0xEFFFD && (v & 0xFFFF) <= 0xFFFD)
		{ MCC_TRACE("br\n"); return 1; }
	return 0;
}

static int decode_ucn(uint8_t **pp) { MCC_TRACE("enter\n");
	uint8_t *p = *pp;
	int n, i, c;
	unsigned int v = 0;
	if (p[0] != '\\')
		{ MCC_TRACE("br\n"); return -1; }
	if (p[1] == 'u')
		{ MCC_TRACE("br\n"); n = 4; }
	else if (p[1] == 'U')
		{ MCC_TRACE("br\n"); n = 8; }
	else
		{ MCC_TRACE("br\n"); return -1; }
	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		c = p[2 + i];
		if (c >= '0' && c <= '9')
			{ MCC_TRACE("br\n"); v = v * 16 + (c - '0'); }
		else if (c >= 'a' && c <= 'f')
			{ MCC_TRACE("br\n"); v = v * 16 + (c - 'a' + 10); }
		else if (c >= 'A' && c <= 'F')
			{ MCC_TRACE("br\n"); v = v * 16 + (c - 'A' + 10); }
		else
			{ MCC_TRACE("br\n"); return -1; }
	}
	*pp = p + 2 + n;
	if ((v < 0xA0 && v != 0x24 && v != 0x40 && v != 0x60) || (v >= 0xD800 && v <= 0xDFFF))
		{ MCC_TRACE("br\n"); mcc_error("universal character \\u%04x is not valid in an identifier",
							v); }
	if (v >= 0xA0 && !ucn_allowed_in_identifier(v))
		{ MCC_TRACE("br\n"); mcc_error("universal character \\u%04x is not valid in an identifier",
							v); }
	return (int)v;
}

static int ucn_disallowed_initial(unsigned int v) { MCC_TRACE("enter\n");
	return (v >= 0x0300 && v <= 0x036F) || (v >= 0x1DC0 && v <= 0x1DFF) || (v >= 0x20D0 && v <= 0x20FF) || (v >= 0xFE20 && v <= 0xFE2F);
}

static int utf8_next_cp(const uint8_t **pp, const uint8_t *end) { MCC_TRACE("enter\n");
	const uint8_t *p = *pp;
	unsigned c = *p, cp;
	int i, n;
	if (c < 0x80) { MCC_TRACE("br\n");
		*pp = p + 1;
		return (int)c;
	}
	if ((c & 0xE0) == 0xC0) { MCC_TRACE("br\n");
		cp = c & 0x1F;
		n = 1;
	} else if ((c & 0xF0) == 0xE0) { MCC_TRACE("br\n");
		cp = c & 0x0F;
		n = 2;
	} else if ((c & 0xF8) == 0xF0) { MCC_TRACE("br\n");
		cp = c & 0x07;
		n = 3;
	} else { MCC_TRACE("br\n");
		*pp = p + 1;
		return -1;
	}
	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		if (p + 1 + i >= end || (p[1 + i] & 0xC0) != 0x80) { MCC_TRACE("br\n");
			*pp = p + 1;
			return -1;
		}
		cp = (cp << 6) | (p[1 + i] & 0x3F);
	}
	*pp = p + 1 + n;
	return (int)cp;
}

static void validate_utf8_identifier(const char *s, int len) { MCC_TRACE("enter\n");
	const uint8_t *p = (const uint8_t *)s, *end = p + len;
	int first = 1;
	int warned = 0;
	while (p < end) { MCC_TRACE("br\n");
		int leadbyte = *p;
		int cp = utf8_next_cp(&p, end);
		if (cp >= 0 && leadbyte >= 0x80) { MCC_TRACE("br\n");
			if ((cp < 0xA0 && cp != 0x24 && cp != 0x40 && cp != 0x60) || (cp >= 0xD800 && cp <= 0xDFFF) || (cp >= 0xA0 && !ucn_allowed_in_identifier(cp)))
				{ MCC_TRACE("br\n"); mcc_error("universal character \\u%04x is not valid in an "
									"identifier",
									cp); }
			else if (!warned && mcc_state->warn_pedantic && mcc_state->cversion < 199901) { MCC_TRACE("br\n");
				warned = 1;
				if (mcc_state->pedantic_errors)
					{ MCC_TRACE("br\n"); mcc_error("extended identifiers are a C99 feature"); }
				else
					{ MCC_TRACE("br\n"); mcc_warning("extended identifiers are a C99 feature"); }
			}
			if (first && ucn_disallowed_initial(cp))
				{ MCC_TRACE("br\n"); mcc_error("universal character \\u%04x is not valid as the "
									"first character of an identifier",
									cp); }
		}
		first = 0;
	}
}

ST_FUNC void cstr_cat(CString *cstr, const char *str, int len) { MCC_TRACE("enter\n");
	int size;
	if (len <= 0)
		{ MCC_TRACE("br\n"); len = strlen(str) + 1 + len; }
	size = cstr->size + len;
	if (size > cstr->size_allocated)
		{ MCC_TRACE("br\n"); cstr_realloc(cstr, size); }
	if (len)
		{ MCC_TRACE("br\n"); memmove(cstr->data + cstr->size, str, len); }
	cstr->size = size;
}

ST_FUNC void cstr_wccat(CString *cstr, int ch) { MCC_TRACE("enter\n");
	int size;
	size = cstr->size + sizeof(nwchar_t);
	if (size > cstr->size_allocated)
		{ MCC_TRACE("br\n"); cstr_realloc(cstr, size); }
	*(nwchar_t *)(cstr->data + size - sizeof(nwchar_t)) = ch;
	cstr->size = size;
}

ST_FUNC void cstr_new(CString *cstr) { MCC_TRACE("enter\n");
	memset(cstr, 0, sizeof(CString));
}

ST_FUNC void cstr_free(CString *cstr) { MCC_TRACE("enter\n");
	mcc_free(cstr->data);
}

ST_FUNC void cstr_reset(CString *cstr) { MCC_TRACE("enter\n");
	cstr->size = 0;
}

ST_FUNC int cstr_vprintf(CString *cstr, const char *fmt, va_list ap) { MCC_TRACE("enter\n");
	va_list v;
	int len, size = 80;
	for (;;) { MCC_TRACE("br\n");
		size += cstr->size;
		if (size > cstr->size_allocated)
			{ MCC_TRACE("br\n"); cstr_realloc(cstr, size); }
		size = cstr->size_allocated - cstr->size;
		va_copy(v, ap);
		len = vsnprintf(cstr->data + cstr->size, size, fmt, v);
		va_end(v);
		if (len >= 0 && len < size)
			{ MCC_TRACE("br\n"); break; }
		size *= 2;
	}
	cstr->size += len;
	return len;
}

ST_FUNC int cstr_printf(CString *cstr, const char *fmt, ...) { MCC_TRACE("enter\n");
	va_list ap;
	int len;
	va_start(ap, fmt);
	len = cstr_vprintf(cstr, fmt, ap);
	va_end(ap);
	return len;
}

static void add_char(CString *cstr, int c) { MCC_TRACE("enter\n");
	if (c == '\'' || c == '\"' || c == '\\') { MCC_TRACE("br\n");
		cstr_ccat(cstr, '\\');
	}
	if (c >= 32 && c <= 126) { MCC_TRACE("br\n");
		cstr_ccat(cstr, c);
	} else { MCC_TRACE("br\n");
		cstr_ccat(cstr, '\\');
		if (c == '\n') { MCC_TRACE("br\n");
			cstr_ccat(cstr, 'n');
		} else { MCC_TRACE("br\n");
			cstr_ccat(cstr, '0' + ((c >> 6) & 7));
			cstr_ccat(cstr, '0' + ((c >> 3) & 7));
			cstr_ccat(cstr, '0' + (c & 7));
		}
	}
}

static TokenSym *tok_alloc_new(TokenSym **pts, const char *str, int len) { MCC_TRACE("enter\n");
	TokenSym *ts, **ptable;
	int i;

	if (tok_ident >= SYM_FIRST_ANOM)
		{ MCC_TRACE("br\n"); mcc_error("memory full (symbols)"); }

	i = tok_ident - TOK_IDENT;
	if ((i % TOK_ALLOC_INCR) == 0) { MCC_TRACE("br\n");
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

ST_FUNC TokenSym *tok_alloc(const char *str, int len) { MCC_TRACE("enter\n");
	TokenSym *ts, **pts;
	unsigned int h;

	h = TOK_HASH_INIT;
	for (int i = 0; i < len; i++)
		{ MCC_TRACE("br\n"); h = TOK_HASH_FUNC(h, ((unsigned char *)str)[i]); }
	h &= (TOK_HASH_SIZE - 1);

	pts = &hash_ident[h];
	for (;;) { MCC_TRACE("br\n");
		ts = *pts;
		if (!ts)
			{ MCC_TRACE("br\n"); break; }
		if (ts->len == len && !memcmp(ts->str, str, len))
			{ MCC_TRACE("br\n"); return ts; }
		pts = &(ts->hash_next);
	}
	return tok_alloc_new(pts, str, len);
}

ST_FUNC int tok_alloc_const(const char *str) { MCC_TRACE("enter\n");
	return tok_alloc(str, strlen(str))->tok;
}

static int digraphs_enabled(void) { MCC_TRACE("enter\n");
	return mcc_state->cversion >= 199409 || !mcc_state->std_strict_ansi;
}

static int digraph_primary(int t) { MCC_TRACE("enter\n");
	switch (t) { MCC_TRACE("br\n");
	case TOK_DIG_LBRACK: return '[';
	case TOK_DIG_RBRACK: return ']';
	case TOK_DIG_LBRACE: return '{';
	case TOK_DIG_RBRACE: return '}';
	case TOK_DIG_HASH: return '#';
	case TOK_DIG_TWOSHARPS: return TOK_TWOSHARPS;
	}
	return t;
}

static const char *digraph_spelling(int t) { MCC_TRACE("enter\n");
	switch (t) { MCC_TRACE("br\n");
	case TOK_DIG_LBRACK: return "<:";
	case TOK_DIG_RBRACK: return ":>";
	case TOK_DIG_LBRACE: return "<%";
	case TOK_DIG_RBRACE: return "%>";
	case TOK_DIG_HASH: return "%:";
	case TOK_DIG_TWOSHARPS: return "%:%:";
	}
	return NULL;
}
ST_FUNC const char *get_tok_str(int v, CValue *cv) { MCC_TRACE("enter\n");
	char *p;
	int len;

	cstr_reset(&cstr_buf);
	if (cstr_buf.size_allocated < 32)
		{ MCC_TRACE("br\n"); cstr_realloc(&cstr_buf, 32); }
	p = cstr_buf.data;

	switch (v) { MCC_TRACE("br\n");
	case TOK_CINT:
	case TOK_CUINT:
	case TOK_CLONG:
	case TOK_CULONG:
	case TOK_CLLONG:
	case TOK_CULLONG:
		snprintf(p, cstr_buf.size_allocated, "%llu", (unsigned long long)cv->i);
		break;
	case TOK_U8CHAR:
		cstr_ccat(&cstr_buf, 'u');
		cstr_ccat(&cstr_buf, '8');
		goto do_char_const;
	case TOK_U16CHAR:
		cstr_ccat(&cstr_buf, 'u');
		goto do_char_const;
	case TOK_U32CHAR:
		cstr_ccat(&cstr_buf, 'U');
		goto do_char_const;
	case TOK_LCHAR:
		cstr_ccat(&cstr_buf, 'L');
		FALLTHROUGH;
	case TOK_CCHAR:
	do_char_const:
		cstr_ccat(&cstr_buf, '\'');
		add_char(&cstr_buf, cv->i);
		cstr_ccat(&cstr_buf, '\'');
		cstr_ccat(&cstr_buf, '\0');
		break;
	case TOK_PPNUM:
	case TOK_PPSTR:
		return (char *)cv->str.data;
	case TOK_U16STR:
	case TOK_U32STR:
	case TOK_LSTR:
		cstr_ccat(&cstr_buf, 'L');
		FALLTHROUGH;
	case TOK_U8STR:
		if (v == TOK_U8STR)
			{ MCC_TRACE("br\n"); cstr_ccat(&cstr_buf, 'u'), cstr_ccat(&cstr_buf, '8'); }
		FALLTHROUGH;
	case TOK_STR:
		cstr_ccat(&cstr_buf, '\"');
		if (v == TOK_STR || v == TOK_U8STR) { MCC_TRACE("br\n");
			len = cv->str.size - 1;
			for (int i = 0; i < len; i++)
				{ MCC_TRACE("br\n"); add_char(&cstr_buf, ((unsigned char *)cv->str.data)[i]); }
		} else { MCC_TRACE("br\n");
			len = (cv->str.size / sizeof(nwchar_t)) - 1;
			for (int i = 0; i < len; i++)
				{ MCC_TRACE("br\n"); add_char(&cstr_buf, ((nwchar_t *)cv->str.data)[i]); }
		}
		cstr_ccat(&cstr_buf, '\"');
		cstr_ccat(&cstr_buf, '\0');
		break;

	case TOK_CFLOAT:
		return strcpy(p, "<float>");
	case TOK_CFLOAT16:
		return strcpy(p, "<_Float16>");
	case TOK_CFLOAT128:
		return strcpy(p, "<__float128>");
	case TOK_CDOUBLE:
		return strcpy(p, "<double>");
	case TOK_CLDOUBLE:
		return strcpy(p, "<long double>");
	case TOK_CINT256:
	case TOK_CUINT256:
		snprintf(p, cstr_buf.size_allocated, "0x%016llx%016llx%016llx%016llx%s",
						 (unsigned long long)cv->q.w3, (unsigned long long)cv->q.w2,
						 (unsigned long long)cv->q.hi, (unsigned long long)cv->q.lo,
						 v == TOK_CUINT256 ? "ui256" : "i256");
		break;
	case TOK_LINENUM:
		return strcpy(p, "<linenumber>");

	case TOK_LT:
		v = '<';
		goto addv;
	case TOK_GT:
		v = '>';
		goto addv;
	case TOK_DIG_LBRACK:
	case TOK_DIG_RBRACK:
	case TOK_DIG_LBRACE:
	case TOK_DIG_RBRACE:
	case TOK_DIG_HASH:
	case TOK_DIG_TWOSHARPS:
		return strcpy(p, digraph_spelling(v));
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
		if (v < TOK_IDENT) { MCC_TRACE("br\n");
			const unsigned char *q = tok_two_chars;
			while (*q) { MCC_TRACE("br\n");
				if (q[2] == v) { MCC_TRACE("br\n");
					*p++ = q[0];
					*p++ = q[1];
					*p = '\0';
					return cstr_buf.data;
				}
				q += 3;
			}
			if (v >= 127 || (v < 32 && !is_space(v) && v != '\n')) { MCC_TRACE("br\n");
				snprintf(p, cstr_buf.size_allocated, "<\\x%02x>", v);
				break;
			}
		addv:
			*p++ = v;
			*p = '\0';
		} else if (v < tok_ident) { MCC_TRACE("br\n");
			return table_ident[v - TOK_IDENT]->str;
		} else if (v >= SYM_FIRST_ANOM) { MCC_TRACE("br\n");
			snprintf(p, cstr_buf.size_allocated, "L.%u", v - SYM_FIRST_ANOM);
		} else { MCC_TRACE("br\n");
			return NULL;
		}
		break;
	}
	return cstr_buf.data;
}

static int trigraph_replace(unsigned char *buf, int len) { MCC_TRACE("enter\n");
	unsigned char *src = buf, *dst = buf, *end = buf + len;
	while (src < end) { MCC_TRACE("br\n");
		if (src[0] == '?' && src + 2 < end && src[1] == '?') { MCC_TRACE("br\n");
			int t = 0;
			switch (src[2]) { MCC_TRACE("br\n");
			case '=':
				t = '#';
				break;
			case '(':
				t = '[';
				break;
			case ')':
				t = ']';
				break;
			case '<':
				t = '{';
				break;
			case '>':
				t = '}';
				break;
			case '/':
				t = '\\';
				break;
			case '\'':
				t = '^';
				break;
			case '!':
				t = '|';
				break;
			case '-':
				t = '~';
				break;
			}
			if (t) { MCC_TRACE("br\n");
				*dst++ = (unsigned char)t;
				src += 3;
				continue;
			}
		}
		*dst++ = *src++;
	}
	return (int)(dst - buf);
}

static int handle_eob(void) { MCC_TRACE("enter\n");
	BufferedFile *bf = file;
	int len;

	if (bf->buf_ptr >= bf->buf_end) { MCC_TRACE("br\n");
		if (bf->fd >= 0) { MCC_TRACE("br\n");
#if defined(MCC_PARSE_DEBUG)
			len = 1;
#else
			len = IO_BUF_SIZE;
#endif
			len = read(bf->fd, bf->buffer, len);
			if (len < 0)
				{ MCC_TRACE("br\n"); len = 0; }
			if (mcc_state->trigraphs && len > 0) { MCC_TRACE("br\n");
				len = trigraph_replace(bf->buffer, len);
				if (len > 2) { MCC_TRACE("br\n");
					int k = 0;
					while (k < 2 && bf->buffer[len - 1 - k] == '?')
						{ MCC_TRACE("br\n"); k++; }
					if (k > 0 && lseek(bf->fd, -(long)k, SEEK_CUR) != (off_t)-1)
						{ MCC_TRACE("br\n"); len -= k; }
				}
			}
		} else { MCC_TRACE("br\n");
			len = 0;
		}
		total_bytes += len;
		if (bf->fd >= 0)
			{ MCC_TRACE("br\n"); bf->cst_base += (unsigned long)(bf->buf_end - bf->buffer); }
		bf->buf_ptr = bf->buffer;
		bf->buf_end = bf->buffer + len;
		*bf->buf_end = CH_EOB;
	}
	if (bf->buf_ptr < bf->buf_end) { MCC_TRACE("br\n");
		return bf->buf_ptr[0];
	} else { MCC_TRACE("br\n");
		bf->buf_ptr = bf->buf_end;
		return CH_EOF;
	}
}

static int next_c(void) { MCC_TRACE("enter\n");
	int ch = *++file->buf_ptr;
	if (ch == CH_EOB && file->buf_ptr >= file->buf_end)
		{ MCC_TRACE("br\n"); ch = handle_eob(); }
	return ch;
}

static int handle_stray_noerror(int err) { MCC_TRACE("enter\n");
	int ch;
	while ((ch = next_c()) == '\\') { MCC_TRACE("br\n");
		ch = next_c();
		if (ch == '\n') { MCC_TRACE("br\n");
		newl:
			file->line_num++;
		} else { MCC_TRACE("br\n");
			if (ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f') { MCC_TRACE("br\n");
				uint8_t *q = file->buf_ptr;
				while (q < file->buf_end &&
							 (*q == ' ' || *q == '\t' || *q == '\v' || *q == '\f'))
					{ MCC_TRACE("br\n"); q++; }
				if (q < file->buf_end && *q == '\r')
					{ MCC_TRACE("br\n"); q++; }
				if (q < file->buf_end && *q == '\n') { MCC_TRACE("br\n");
					file->buf_ptr = q;
					mcc_warning("backslash and newline separated by space");
					goto newl;
				}
			}
			if (ch == '\r') { MCC_TRACE("br\n");
				ch = next_c();
				if (ch == '\n')
					{ MCC_TRACE("br\n"); goto newl; }
				*--file->buf_ptr = '\r';
			}
			if (err)
				{ MCC_TRACE("br\n"); mcc_error("stray '\\' in program"); }
			return *--file->buf_ptr = '\\';
		}
	}
	return ch;
}

#define ninp() handle_stray_noerror(0)

static int handle_bs(uint8_t **p) { MCC_TRACE("enter\n");
	int c;
	file->buf_ptr = *p - 1;
	c = ninp();
	*p = file->buf_ptr;
	return c;
}

static int handle_stray(uint8_t **p) { MCC_TRACE("enter\n");
	int c;
	file->buf_ptr = *p - 1;
	c = handle_stray_noerror(!(parse_flags & PARSE_FLAG_ACCEPT_STRAYS));
	*p = file->buf_ptr;
	return c;
}

#define PEEKC(c, p)                          \
	{                                          \
		c = *++p;                                \
		if (c == '\\' && p[1] != 'u' && p[1] != 'U') \
			c = handle_stray(&p);                  \
	}

static int skip_spaces(void) { MCC_TRACE("enter\n");
	int ch;
	--file->buf_ptr;
	do { MCC_TRACE("br\n");
		ch = ninp();
	} while (isidnum_table[ch - CH_EOF] & IS_SPC);
	return ch;
}

static uint8_t *parse_line_comment(uint8_t *p) { MCC_TRACE("enter\n");
	int c;
	for (;;) { MCC_TRACE("br\n");
		for (;;) { MCC_TRACE("br\n");
			c = *++p;
		redo:
			if (c == '\n' || c == '\r' || c == '\\')
				{ MCC_TRACE("br\n"); break; }
			c = *++p;
			if (c == '\n' || c == '\r' || c == '\\')
				{ MCC_TRACE("br\n"); break; }
		}
		if (c == '\n' || c == '\r')
			{ MCC_TRACE("br\n"); break; }
		c = handle_bs(&p);
		if (c == CH_EOF)
			{ MCC_TRACE("br\n"); break; }
		if (c != '\\')
			{ MCC_TRACE("br\n"); goto redo; }
	}
	return p;
}

static uint8_t *parse_comment(uint8_t *p) { MCC_TRACE("enter\n");
	int c;
	for (;;) { MCC_TRACE("br\n");
		for (;;) { MCC_TRACE("br\n");
			c = *++p;
		redo:
			if (c == '\n' || c == '*' || c == '\\')
				{ MCC_TRACE("br\n"); break; }
			c = *++p;
			if (c == '\n' || c == '*' || c == '\\')
				{ MCC_TRACE("br\n"); break; }
		}
		if (c == '\n') { MCC_TRACE("br\n");
			file->line_num++;
		} else if (c == '*') { MCC_TRACE("br\n");
			do { MCC_TRACE("br\n");
				c = *++p;
			} while (c == '*');
			if (c == '\\')
				{ MCC_TRACE("br\n"); c = handle_bs(&p); }
			if (c == '/')
				{ MCC_TRACE("br\n"); break; }
			goto check_eof;
		} else { MCC_TRACE("br\n");
			c = handle_bs(&p);
		check_eof:
			if (c == CH_EOF)
				{ MCC_TRACE("br\n"); mcc_error("unexpected end of file in comment"); }
			if (c != '\\')
				{ MCC_TRACE("br\n"); goto redo; }
		}
	}
	return p + 1;
}

static uint8_t *parse_pp_string(uint8_t *p, int sep, CString *str) { MCC_TRACE("enter\n");
	int c;
	for (;;) { MCC_TRACE("br\n");
		c = *++p;
	redo:
		if (c == sep) { MCC_TRACE("br\n");
			break;
		} else if (c == '\\') { MCC_TRACE("br\n");
			c = handle_bs(&p);
			if (c == CH_EOF) { MCC_TRACE("br\n");
			unterminated_string:
				tok_flags &= ~TOK_FLAG_BOL;
				if (sep != '>' && !pp_expr &&
						mcc_state->output_type == MCC_OUTPUT_PREPROCESS) { MCC_TRACE("br\n");
					mcc_warning("missing terminating %c character", sep);
					return p;
				}
				mcc_error("missing terminating %c character", sep);
			} else if (c == '\\') { MCC_TRACE("br\n");
				if (str)
					{ MCC_TRACE("br\n"); cstr_ccat(str, c); }
				c = *++p;
				if (c == '\\') { MCC_TRACE("br\n");
					c = handle_bs(&p);
					if (c == CH_EOF)
						{ MCC_TRACE("br\n"); goto unterminated_string; }
				}
				goto add_char;
			} else { MCC_TRACE("br\n");
				goto redo;
			}
		} else if (c == '\n') { MCC_TRACE("br\n");
		add_lf:
			if (ACCEPT_LF_IN_STRINGS) { MCC_TRACE("br\n");
				file->line_num++;
				goto add_char;
			} else if (str) { MCC_TRACE("br\n");
				goto unterminated_string;
			} else { MCC_TRACE("br\n");
				return p;
			}
		} else if (c == '\r') { MCC_TRACE("br\n");
			c = *++p;
			if (c == '\\')
				{ MCC_TRACE("br\n"); c = handle_bs(&p); }
			if (c == '\n')
				{ MCC_TRACE("br\n"); goto add_lf; }
			if (c == CH_EOF)
				{ MCC_TRACE("br\n"); goto unterminated_string; }
			if (str)
				{ MCC_TRACE("br\n"); cstr_ccat(str, '\r'); }
			goto redo;
		} else { MCC_TRACE("br\n");
		add_char:
			if (str)
				{ MCC_TRACE("br\n"); cstr_ccat(str, c); }
		}
	}
	p++;
	return p;
}

static void preprocess_skip(void) { MCC_TRACE("enter\n");
	int a, start_of_line, c, in_warn_or_error;
	uint8_t *p;

	p = file->buf_ptr;
	a = 0;
redo_start:
	start_of_line = 1;
	in_warn_or_error = 0;
	for (;;) { MCC_TRACE("br\n");
		c = *p;
		switch (c) { MCC_TRACE("br\n");
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
				{ MCC_TRACE("br\n"); expect("#endif"); }
			if (c == '\\')
				{ MCC_TRACE("br\n"); ++p; }
			continue;
		case '\"':
		case '\'':
			if (in_warn_or_error)
				{ MCC_TRACE("br\n"); goto _default; }
			tok_flags &= ~TOK_FLAG_BOL;
			p = parse_pp_string(p, c, NULL);
			break;
		case '/':
			if (in_warn_or_error)
				{ MCC_TRACE("br\n"); goto _default; }
			++p;
			c = handle_bs(&p);
			if (c == '*') { MCC_TRACE("br\n");
				p = parse_comment(p);
			} else if (c == '/') { MCC_TRACE("br\n");
				p = parse_line_comment(p);
			}
			continue;
		case '#':
			p++;
			if (start_of_line) { MCC_TRACE("br\n");
				file->buf_ptr = p;
				next_nomacro();
				p = file->buf_ptr;
				if (a == 0 &&
						(tok == TOK_ELSE || tok == TOK_ELIF || tok == TOK_ELIFDEF ||
						 tok == TOK_ELIFNDEF || tok == TOK_ENDIF))
					{ MCC_TRACE("br\n"); goto the_end; }
				if (tok == TOK_IF || tok == TOK_IFDEF || tok == TOK_IFNDEF)
					{ MCC_TRACE("br\n"); a++; }
				else if (tok == TOK_ENDIF)
					{ MCC_TRACE("br\n"); a--; }
				else if (tok == TOK_ERROR || tok == TOK_WARNING)
					{ MCC_TRACE("br\n"); in_warn_or_error = 1; }
				else if (tok == TOK_LINEFEED)
					{ MCC_TRACE("br\n"); goto redo_start; }
				else if (parse_flags & PARSE_FLAG_ASM_FILE)
					{ MCC_TRACE("br\n"); p = parse_line_comment(p - 1); }
			}
#if !defined(MCC_TARGET_ARM) && !defined(MCC_TARGET_ARM64)
			else if (parse_flags & PARSE_FLAG_ASM_FILE)
				{ MCC_TRACE("br\n"); p = parse_line_comment(p - 1); }
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
the_end:;
	file->buf_ptr = p;
}

ST_INLN void tok_str_new(TokenString *s) { MCC_TRACE("enter\n");
	s->str = NULL;
	s->len = s->need_spc = 0;
	s->allocated_len = 0;
	s->last_line_num = -1;
}

ST_FUNC TokenString *tok_str_alloc(void) { MCC_TRACE("enter\n");
	TokenString *str = tal_realloc(&tokstr_alloc, 0, sizeof *str);
	tok_str_new(str);
	return str;
}

ST_FUNC void tok_str_free_str(int *str) { MCC_TRACE("enter\n");
	tal_free(&tokstr_alloc, str);
}

ST_FUNC void tok_str_free(TokenString *str) { MCC_TRACE("enter\n");
	tok_str_free_str(str->str);
	tal_free(&tokstr_alloc, str);
}

ST_FUNC int *tok_str_realloc(TokenString *s, int new_size) { MCC_TRACE("enter\n");
	int *str, size;

	size = s->allocated_len;
	if (size < 16)
		{ MCC_TRACE("br\n"); size = 16; }
	while (size < new_size)
		{ MCC_TRACE("br\n"); size = size * 2; }
	if (size > s->allocated_len) { MCC_TRACE("br\n");
		str = tal_realloc(&tokstr_alloc, s->str, size * sizeof(int));
		s->allocated_len = size;
		s->str = str;
	}
	return s->str;
}

ST_FUNC void tok_str_add(TokenString *s, int t) { MCC_TRACE("enter\n");
	int len, *str;

	len = s->len;
	str = s->str;
	if (len >= s->allocated_len)
		{ MCC_TRACE("br\n"); str = tok_str_realloc(s, len + 1); }
	str[len++] = t;
	s->len = len;
}

ST_FUNC void begin_macro(TokenString *str, int alloc) { MCC_TRACE("enter\n");
	str->alloc = alloc;
	str->prev = macro_stack;
	str->prev_ptr = macro_ptr;
	str->save_line_num = file->line_num;
	macro_ptr = str->str;
	macro_stack = str;
}

ST_FUNC void end_macro(void) { MCC_TRACE("enter\n");
	TokenString *str = macro_stack;
	macro_stack = str->prev;
	macro_ptr = str->prev_ptr;
	file->line_num = tok_line_num = str->save_line_num;
	tok_line_file = file;
	if (str->alloc == 0) { MCC_TRACE("br\n");
		str->len = str->need_spc = 0;
	} else { MCC_TRACE("br\n");
		if (str->alloc == 2)
			{ MCC_TRACE("br\n"); str->str = NULL; }
		tok_str_free(str);
	}
}

static void tok_str_add2(TokenString *s, int t, CValue *cv) { MCC_TRACE("enter\n");
	int len, *str;

	len = s->len;
	str = s->str;

	if (len + TOK_MAX_SIZE >= s->allocated_len)
		{ MCC_TRACE("br\n"); str = tok_str_realloc(s, len + TOK_MAX_SIZE + 1); }
	str[len++] = t;
	switch (t) { MCC_TRACE("br\n");
	case TOK_CINT:
	case TOK_CUINT:
	case TOK_CCHAR:
	case TOK_LCHAR:
	case TOK_U8CHAR:
	case TOK_U16CHAR:
	case TOK_U32CHAR:
	case TOK_CFLOAT:
	case TOK_CFLOAT16:
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
	case TOK_U8STR: {
		size_t nb_words =
				1 + (cv->str.size + sizeof(int) - 1) / sizeof(int);
		if (len + nb_words >= s->allocated_len)
			{ MCC_TRACE("br\n"); str = tok_str_realloc(s, len + nb_words + 1); }
		str[len] = cv->str.size;
		memcpy(&str[len + 1], cv->str.data, cv->str.size);
		len += nb_words;
	} break;
	case TOK_CDOUBLE:
	case TOK_CFLOAT128:
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
			{ MCC_TRACE("br\n"); str[len++] = cv->tab[2]; }
		if (LDOUBLE_WORDS >= 4)
			{ MCC_TRACE("br\n"); str[len++] = cv->tab[3]; }
		break;
	case TOK_CINT256:
	case TOK_CUINT256:
		memcpy(&str[len], &cv->q, 32);
		len += 8;
		break;
	default:
		break;
	}
	s->len = len;
}

ST_FUNC void tok_str_add_tok(TokenString *s) { MCC_TRACE("enter\n");
	CValue cval;

	if (file->line_num != s->last_line_num) { MCC_TRACE("br\n");
		s->last_line_num = file->line_num;
		cval.i = s->last_line_num;
		tok_str_add2(s, TOK_LINENUM, &cval);
	}
	tok_str_add2(s, tok, &tokc);
}

static void tok_str_add2_spc(TokenString *s, int t, CValue *cv) { MCC_TRACE("enter\n");
	if (s->need_spc == 3)
		{ MCC_TRACE("br\n"); tok_str_add(s, ' '); }
	s->need_spc = 2;
	tok_str_add2(s, t, cv);
}

static inline void tok_get(int *t, const int **pp, CValue *cv) { MCC_TRACE("enter\n");
	const int *p = *pp;
	int n, *tab;

	tab = cv->tab;
	switch (*t = *p++) { MCC_TRACE("br\n");
#if LONG_SIZE == 4
	case TOK_CLONG:
#endif
	case TOK_CINT:
	case TOK_CCHAR:
	case TOK_LCHAR:
	case TOK_U8CHAR:
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
	case TOK_CFLOAT16:
		cv->i = (unsigned)*p++;
		break;
	case TOK_STR:
	case TOK_LSTR:
	case TOK_U16STR:
	case TOK_U32STR:
	case TOK_U8STR:
	case TOK_PPNUM:
	case TOK_PPSTR:
		cv->str.size = *p++;
		cv->str.data = (char *)p;
		p += (cv->str.size + sizeof(int) - 1) / sizeof(int);
		break;
	case TOK_CDOUBLE:
	case TOK_CFLOAT128:
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
		goto copy;
	case TOK_CINT256:
	case TOK_CUINT256:
		memcpy(&cv->q, p, 32);
		p += 8;
		break;
	copy:
		do
			{ MCC_TRACE("br\n"); *tab++ = *p++; }
		while (--n);
		break;
	default:
		break;
	}
	*pp = p;
}

#define TOK_GET(t, p, c)   \
	do {                     \
		int _t = **(p);        \
		if (TOK_HAS_VALUE(_t)) \
			tok_get(t, p, c);    \
		else                   \
			*(t) = _t, ++*(p);   \
	} while (0)

static int macro_is_equal(const int *a, const int *b) { MCC_TRACE("enter\n");
	CValue cv;
	int t;

	if (!a || !b)
		{ MCC_TRACE("br\n"); return 1; }

	while (*a && *b) { MCC_TRACE("br\n");
		cstr_reset(&tokcstr);
		TOK_GET(&t, &a, &cv);
		cstr_cat(&tokcstr, get_tok_str(t, &cv), 0);
		TOK_GET(&t, &b, &cv);
		if (strcmp(tokcstr.data, get_tok_str(t, &cv)))
			{ MCC_TRACE("br\n"); return 0; }
	}
	return !(*a || *b);
}

/* T-mac-30219: emit the "%s redefined" warning, un-suppressing it inside the
 * <command line> pseudo-buffer's user (-D) region. Real system headers stay
 * suppressed; the built-in predef region (before the marker) stays suppressed
 * because pp_cmdline_user is still 0 there. */
static void warn_macro_redefined(int v) { MCC_TRACE("enter\n");
	BufferedFile *wf;
	int saved = -2;
	for (wf = file; wf && wf->filename[0] == ':'; wf = wf->prev)
		;
	if (pp_cmdline_user && wf && wf->system_header && wf->filename[0] == '<') { MCC_TRACE("br\n");
		saved = wf->system_header;
		wf->system_header = 0;
	}
	mcc_warning("%s redefined", get_tok_str(v, NULL));
	if (saved != -2)
		{ MCC_TRACE("br\n"); wf->system_header = saved; }
}

ST_INLN void define_push(int v, int macro_type, int *str, Sym *first_arg) { MCC_TRACE("enter\n");
	Sym *s, *o;

	o = define_find(v);
	s = sym_push2(&define_stack, v, macro_type, 0);
	s->d = str;
	s->next = first_arg;
	table_ident[v - TOK_IDENT]->sym_define = s;

	if (o && !macro_is_equal(o->d, s->d) && !is_predef_macro(v))
		{ MCC_TRACE("br\n"); warn_macro_redefined(v); }
}

ST_FUNC void define_undef(Sym *s) { MCC_TRACE("enter\n");
	int v = s->v;
	if (v >= TOK_IDENT && v < tok_ident)
		{ MCC_TRACE("br\n"); table_ident[v - TOK_IDENT]->sym_define = NULL; }
}

ST_INLN Sym *define_find(int v) { MCC_TRACE("enter\n");
	v -= TOK_IDENT;
	if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
		{ MCC_TRACE("br\n"); return NULL; }
	return table_ident[v]->sym_define;
}

ST_FUNC void free_defines(Sym *b) { MCC_TRACE("enter\n");
	while (define_stack != b) { MCC_TRACE("br\n");
		Sym *top = define_stack;
		define_stack = top->prev;
		tok_str_free_str(top->d);
		define_undef(top);
		sym_free(top);
	}
}

static void maybe_run_test(MCCState *s) { MCC_TRACE("enter\n");
	const char *p;
	if (s->include_stack_ptr != s->include_stack)
		{ MCC_TRACE("br\n"); return; }
	p = get_tok_str(tok, NULL);
	if (0 != memcmp(p, "test_", 5))
		{ MCC_TRACE("br\n"); return; }
	if (0 != --s->run_test)
		{ MCC_TRACE("br\n"); return; }
	fprintf(s->ppfp, &"\n[%s]\n"[!(s->dflag & 32)], p), fflush(s->ppfp);
	define_push(tok, MACRO_OBJ, NULL, NULL);
}

ST_FUNC void skip_to_eol(int warn) { MCC_TRACE("enter\n");
	if (tok == TOK_LINEFEED)
		{ MCC_TRACE("br\n"); return; }
	if (warn) { MCC_TRACE("br\n");
		if (mcc_state->pedantic_errors && !pp_in_system_header())
			{ MCC_TRACE("br\n"); mcc_error("extra tokens after directive"); }
		else
			{ MCC_TRACE("br\n"); mcc_warning("extra tokens after directive"); }
	}
	while (macro_stack)
		{ MCC_TRACE("br\n"); end_macro(); }
	file->buf_ptr = parse_line_comment(file->buf_ptr - 1);
	next_nomacro();
}

static CachedInclude *
search_cached_include(MCCState *s1, const char *filename, int add);
static int once_seen_by_file_id(MCCState *s1, const char *path);

static BufferedFile *cst_main_bf;

static int parse_include(MCCState *s1, int do_next, int test, int is_import) { MCC_TRACE("enter\n");
	int c, i;
	char name[1024], buf[1024], *p;
	CachedInclude *e;

	c = skip_spaces();
	if (c == '<' || c == '\"') { MCC_TRACE("br\n");
		cstr_reset(&tokcstr);
		file->buf_ptr = parse_pp_string(file->buf_ptr, c == '<' ? '>' : c, &tokcstr);
		i = tokcstr.size;
		pstrncpy(name, sizeof name, tokcstr.data, i);
		next_nomacro();
	} else { MCC_TRACE("br\n");
		parse_flags = PARSE_FLAG_PREPROCESS | PARSE_FLAG_LINEFEED | (parse_flags & PARSE_FLAG_ASM_FILE);
		name[0] = 0;
		for (;;) { MCC_TRACE("br\n");
			next();
			p = name, i = strlen(p) - 1;
			if (i > 0 && ((p[0] == '"' && p[i] == '"') || (p[0] == '<' && p[i] == '>')))
				{ MCC_TRACE("br\n"); break; }
			if (tok == TOK_LINEFEED)
				{ MCC_TRACE("br\n"); mcc_error("'#include' expects \"FILENAME\" or <FILENAME>"); }
			pstrcat(name, sizeof name, get_tok_str(tok, &tokc));
		}
		c = p[0];
		memmove(p, p + 1, i - 1), p[i - 1] = 0;
	}

	if (!test)
		{ MCC_TRACE("br\n"); skip_to_eol(1); }

	int parent_sys = file ? file->system_header : 0, cand_sys = 0, fw = 0;
	i = do_next ? file->include_next_index : -1;
	for (;;) { MCC_TRACE("br\n");
		++i;
		cand_sys = 0;
		fw = 0;
		if (i == 0) { MCC_TRACE("br\n");
			if (!HOST_IS_ABSPATH(name))
				{ MCC_TRACE("br\n"); continue; }
			buf[0] = '\0';
		} else if (i == 1) { MCC_TRACE("br\n");
			if (c != '\"')
				{ MCC_TRACE("br\n"); continue; }
			p = file->true_filename;
			pstrncpy(buf, sizeof buf, p, mcc_basename(p) - p);
		} else { MCC_TRACE("br\n");
			int j = i - 2;
			if (j < s1->nb_iquote_paths) { MCC_TRACE("br\n");
				if (c != '\"')
					{ MCC_TRACE("br\n"); continue; }
				p = s1->iquote_paths[j];
			} else if ((j -= s1->nb_iquote_paths) < s1->nb_include_paths) { MCC_TRACE("br\n");
				p = s1->include_paths[j];
			} else if ((j -= s1->nb_include_paths) < s1->nb_sysinclude_paths) { MCC_TRACE("br\n");
				p = s1->sysinclude_paths[j];
				cand_sys = 1;
			} else if ((j -= s1->nb_sysinclude_paths) < s1->nb_afterinc_paths) { MCC_TRACE("br\n");
				p = s1->afterinc_paths[j];
				cand_sys = 1;
#ifdef MCC_TARGET_MACHO
			} else if ((j -= s1->nb_afterinc_paths) < s1->nb_framework_paths) { MCC_TRACE("br\n");
				const char *slash = strchr(name, '/');
				char comp[256];
				size_t cl;
				if (c == '\"' || !slash)
					{ MCC_TRACE("br\n"); continue; }
				cl = (size_t)(slash - name);
				if (cl >= sizeof comp)
					{ MCC_TRACE("br\n"); continue; }
				memcpy(comp, name, cl), comp[cl] = 0;
				pstrcpy(buf, sizeof buf, s1->framework_paths[j]);
				pstrcat(buf, sizeof buf, "/");
				pstrcat(buf, sizeof buf, comp);
				pstrcat(buf, sizeof buf, ".framework/Headers/");
				pstrcat(buf, sizeof buf, slash + 1);
				cand_sys = 1;
				fw = 1;
#endif
			} else if (test)
				{ MCC_TRACE("br\n"); return 0; }
			else if (s1->gen_deps && s1->gen_deps_missing_ok) { MCC_TRACE("br\n");
				dynarray_add(&s1->target_deps, &s1->nb_target_deps,
										 mcc_strdup(name));
				return 1;
			}
			else
				{ MCC_TRACE("br\n"); mcc_error("include file '%s' not found", name); }
			if (!fw) { MCC_TRACE("br\n");
				pstrcpy(buf, sizeof buf, p);
				pstrcat(buf, sizeof buf, "/");
			}
		}
		if (!fw)
			{ MCC_TRACE("br\n"); pstrcat(buf, sizeof buf, name); }
		e = search_cached_include(s1, buf, 0);
		if (e && is_import)
			{ MCC_TRACE("br\n"); e->once = 1; }
		if (e && (define_find(e->ifndef_macro) || e->once)) { MCC_TRACE("br\n");
			if ((s1->verbose | 1) == 3)
				{ MCC_TRACE("br\n"); printf("=> %*s%s (cached)\n",
							 (int)(s1->include_stack_ptr - s1->include_stack), "", buf); }
			if (!test)
				{ MCC_TRACE("br\n"); cst_hook_include(buf, file == cst_main_bf); }
			return 1;
		}
		if (once_seen_by_file_id(s1, buf)) { MCC_TRACE("br\n");
			if (!test)
				{ MCC_TRACE("br\n"); cst_hook_include(buf, file == cst_main_bf); }
			return 1;
		}
		if (mcc_open(s1, buf) >= 0)
			{ MCC_TRACE("br\n"); break; }
	}
	file->system_header = cand_sys || parent_sys;

	if (test) { MCC_TRACE("br\n");
		mcc_close();
	} else { MCC_TRACE("br\n");
		if (s1->include_stack_ptr >= s1->include_stack + INCLUDE_STACK_SIZE)
			{ MCC_TRACE("br\n"); mcc_error("#include recursion too deep"); }
		{ MCC_TRACE("br\n");
			CachedInclude *ce = search_cached_include(s1, file->true_filename, 1);
			if (ce->dev == 0 && ce->ino == 0)
				{ MCC_TRACE("br\n"); host_file_id(file->true_filename, &ce->dev, &ce->ino); }
			if (is_import)
				{ MCC_TRACE("br\n"); ce->once = 1; }
		}
		*s1->include_stack_ptr++ = file->prev;
		file->include_next_index = i;
		if (s1->gen_deps

				&& !(c == '<' && 0 == strcmp(name, "mccdefs.h") && 0 == strcmp(file->prev->filename, "<command line>"))) { MCC_TRACE("br\n");
			BufferedFile *bf = file;
			while (i == 1 && (bf = bf->prev))
				{ MCC_TRACE("br\n"); i = bf->include_next_index; }
			if (s1->include_sys_deps || i - 2 < s1->nb_include_paths)
				{ MCC_TRACE("br\n"); dynarray_add(&s1->target_deps, &s1->nb_target_deps,
										 mcc_strdup(buf)); }
		}
		mcc_debug_bincl(s1);
		if (!(c == '<' && 0 == strcmp(name, "mccdefs.h") &&
					0 == strcmp(file->prev->filename, "<command line>")))
			{ MCC_TRACE("br\n"); cst_hook_include(buf, file->prev == cst_main_bf); }
	}
	return 1;
}

static MCCAssertion *find_assertion(MCCState *s1, int pred_tok) { MCC_TRACE("enter\n");
	int i;
	for (i = 0; i < s1->nb_assertions; i++)
		{ MCC_TRACE("br\n"); if (s1->assertions[i]->pred_tok == pred_tok)
			{ MCC_TRACE("br\n"); return s1->assertions[i]; } }
	return NULL;
}

static MCCAssertion *get_assertion(MCCState *s1, int pred_tok) { MCC_TRACE("enter\n");
	MCCAssertion *a = find_assertion(s1, pred_tok);
	if (a)
		{ MCC_TRACE("br\n"); return a; }
	a = mcc_mallocz(sizeof *a);
	a->pred_tok = pred_tok;
	dynarray_add(&s1->assertions, &s1->nb_assertions, a);
	return a;
}

static int assertion_has_answer(MCCAssertion *a, const char *answer) { MCC_TRACE("enter\n");
	int i;
	if (!answer)
		{ MCC_TRACE("br\n"); return a->nb_answers > 0; }
	for (i = 0; i < a->nb_answers; i++)
		{ MCC_TRACE("br\n"); if (!strcmp(a->answers[i], answer))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static void assertion_add(MCCState *s1, int pred_tok, char *answer) { MCC_TRACE("enter\n");
	MCCAssertion *a = get_assertion(s1, pred_tok);
	if (assertion_has_answer(a, answer)) { MCC_TRACE("br\n");
		mcc_free(answer);
		return;
	}
	dynarray_add(&a->answers, &a->nb_answers, answer);
}

static void assertion_remove(MCCState *s1, int pred_tok, const char *answer) { MCC_TRACE("enter\n");
	int i, j;
	MCCAssertion *a = find_assertion(s1, pred_tok);
	if (!a)
		{ MCC_TRACE("br\n"); return; }
	if (!answer) { MCC_TRACE("br\n");
		dynarray_reset(&a->answers, &a->nb_answers);
		return;
	}
	for (i = 0; i < a->nb_answers; i++) { MCC_TRACE("br\n");
		if (!strcmp(a->answers[i], answer)) { MCC_TRACE("br\n");
			mcc_free(a->answers[i]);
			for (j = i + 1; j < a->nb_answers; j++)
				{ MCC_TRACE("br\n"); a->answers[j - 1] = a->answers[j]; }
			a->nb_answers--;
			break;
		}
	}
}

static void free_assertions(MCCState *s1) { MCC_TRACE("enter\n");
	int i;
	for (i = 0; i < s1->nb_assertions; i++)
		{ MCC_TRACE("br\n"); dynarray_reset(&s1->assertions[i]->answers, &s1->assertions[i]->nb_answers); }
	dynarray_reset(&s1->assertions, &s1->nb_assertions);
}

static char *pp_capture_parens_text(void) { MCC_TRACE("enter\n");
	CString cs;
	int depth = 1;

	cstr_new(&cs);
	next_nomacro();
	while (depth > 0) { MCC_TRACE("br\n");
		if (tok == TOK_EOF || tok == TOK_LINEFEED)
			{ MCC_TRACE("br\n"); mcc_error("missing ')'"); }
		if (tok == '(')
			{ MCC_TRACE("br\n"); depth++; }
		else if (tok == ')') { MCC_TRACE("br\n");
			depth--;
			if (depth == 0)
				{ MCC_TRACE("br\n"); break; }
		}
		if (cs.size)
			{ MCC_TRACE("br\n"); cstr_ccat(&cs, ' '); }
		cstr_cat(&cs, get_tok_str(tok, &tokc), -1);
		next_nomacro();
	}
	cstr_ccat(&cs, 0);
	return (char *)cs.data;
}

typedef struct EmbedParams {
	int64_t limit;
	int64_t offset;
	char *prefix;
	char *suffix;
	char *if_empty;
} EmbedParams;

static void embed_params_init(EmbedParams *ep) { MCC_TRACE("enter\n");
	ep->limit = -1;
	ep->offset = 0;
	ep->prefix = ep->suffix = ep->if_empty = NULL;
}

static void embed_params_free(EmbedParams *ep) { MCC_TRACE("enter\n");
	mcc_free(ep->prefix);
	mcc_free(ep->suffix);
	mcc_free(ep->if_empty);
}

static int64_t embed_read_paren_const(void) { MCC_TRACE("enter\n");
	int64_t v;
	TokenString *str;
	int depth = 1, t;
	next_nomacro();
	if (tok != '(')
		{ MCC_TRACE("br\n"); expect("'('"); }
	str = tok_str_alloc();
	next();
	while (depth > 0) { MCC_TRACE("br\n");
		if (tok == TOK_EOF || tok == TOK_LINEFEED)
			{ MCC_TRACE("br\n"); mcc_error("missing ')'"); }
		if (tok == '(')
			{ MCC_TRACE("br\n"); depth++; }
		else if (tok == ')') { MCC_TRACE("br\n");
			if (--depth == 0)
				{ MCC_TRACE("br\n"); break; }
		}
		tok_str_add2(str, tok, &tokc);
		next();
	}
	tok_str_add(str, TOK_EOF);
	t = tok;
	begin_macro(str, 1);
	next();
	v = expr_const64_pub();
	if (tok != TOK_EOF)
		{ MCC_TRACE("br\n"); mcc_error("embed parameter expects an integer constant expression"); }
	end_macro();
	tok = t;
	return v;
}

static void embed_parse_params(EmbedParams *ep, int stop_at_paren) { MCC_TRACE("enter\n");
	for (;;) { MCC_TRACE("br\n");
		if (tok == TOK_LINEFEED || tok == TOK_EOF)
			{ MCC_TRACE("br\n"); return; }
		if (stop_at_paren && tok == ')')
			{ MCC_TRACE("br\n"); return; }
		if (tok == TOK_EMBED_LIMIT) { MCC_TRACE("br\n");
			ep->limit = embed_read_paren_const();
		} else if (tok == TOK_EMBED_PREFIX) { MCC_TRACE("br\n");
			next_nomacro();
			if (tok != '(')
				{ MCC_TRACE("br\n"); expect("'('"); }
			mcc_free(ep->prefix);
			ep->prefix = pp_capture_parens_text();
		} else if (tok == TOK_EMBED_SUFFIX) { MCC_TRACE("br\n");
			next_nomacro();
			if (tok != '(')
				{ MCC_TRACE("br\n"); expect("'('"); }
			mcc_free(ep->suffix);
			ep->suffix = pp_capture_parens_text();
		} else if (tok == TOK_EMBED_IF_EMPTY) { MCC_TRACE("br\n");
			next_nomacro();
			if (tok != '(')
				{ MCC_TRACE("br\n"); expect("'('"); }
			mcc_free(ep->if_empty);
			ep->if_empty = pp_capture_parens_text();
		} else if (tok == TOK_KW_GNU) { MCC_TRACE("br\n");
			next_nomacro();
			if (tok != ':')
				{ MCC_TRACE("br\n"); expect("'::'"); }
			next_nomacro();
			if (tok != ':')
				{ MCC_TRACE("br\n"); expect("'::'"); }
			next_nomacro();
			if (tok != TOK_EMBED_OFFSET)
				{ MCC_TRACE("br\n"); mcc_error("unsupported 'gnu::' embed parameter"); }
			ep->offset = embed_read_paren_const();
		} else { MCC_TRACE("br\n");
			mcc_error("unsupported embed parameter '%s'", get_tok_str(tok, &tokc));
		}
		next_nomacro();
	}
}

static int embed_parse_name(char *name, int namesize) { MCC_TRACE("enter\n");
	int c, i;
	char *p;
	int saved_parse_flags = parse_flags;

	c = skip_spaces();
	if (c == '<' || c == '\"') { MCC_TRACE("br\n");
		cstr_reset(&tokcstr);
		file->buf_ptr = parse_pp_string(file->buf_ptr, c == '<' ? '>' : c, &tokcstr);
		i = tokcstr.size;
		pstrncpy(name, namesize, tokcstr.data, i);
		next_nomacro();
	} else { MCC_TRACE("br\n");
		parse_flags = PARSE_FLAG_PREPROCESS | PARSE_FLAG_LINEFEED | (parse_flags & PARSE_FLAG_ASM_FILE);
		name[0] = 0;
		for (;;) { MCC_TRACE("br\n");
			next();
			p = name, i = strlen(p) - 1;
			if (i > 0 && ((p[0] == '"' && p[i] == '"') || (p[0] == '<' && p[i] == '>')))
				{ MCC_TRACE("br\n"); break; }
			if (tok == TOK_LINEFEED)
				{ MCC_TRACE("br\n"); mcc_error("'#embed' expects \"FILENAME\" or <FILENAME>"); }
			pstrcat(name, namesize, get_tok_str(tok, &tokc));
		}
		c = p[0];
		memmove(p, p + 1, i - 1), p[i - 1] = 0;
		parse_flags = saved_parse_flags;
	}
	return c;
}

#include <sys/stat.h>
#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

#define EMBED_FOUND 0
#define EMBED_MISSING 1
#define EMBED_NOT_REGULAR 2
#define EMBED_TOO_LARGE 3
#define EMBED_MAX_SIZE ((int64_t)1 << 30)

static unsigned char *embed_read_file(const char *path, int64_t want, long *out_size,
																			int *status) { MCC_TRACE("enter\n");
	int fd;
	struct stat st;
	int64_t size;
	long got;
	unsigned char *data;

	fd = open(path, O_RDONLY | O_BINARY);
	if (fd < 0)
		{ MCC_TRACE("br\n"); return NULL; }
	if (fstat(fd, &st) < 0)
		{ MCC_TRACE("br\n"); close(fd); return NULL; }
	if (!S_ISREG(st.st_mode)) { MCC_TRACE("br\n");
		close(fd);
		*status = EMBED_NOT_REGULAR;
		return NULL;
	}
	size = (int64_t)st.st_size;
	if (size < 0)
		{ MCC_TRACE("br\n"); close(fd); return NULL; }
	if (want >= 0 && size > want)
		{ MCC_TRACE("br\n"); size = want; }
	if (size > EMBED_MAX_SIZE) { MCC_TRACE("br\n");
		close(fd);
		*status = EMBED_TOO_LARGE;
		return NULL;
	}
	data = mcc_malloc(size ? (size_t)size : 1);
	for (got = 0; got < (long)size; ) { MCC_TRACE("br\n");
		long n = (long)read(fd, data + got, (size_t)((long)size - got));
		if (n <= 0)
			{ MCC_TRACE("br\n"); break; }
		got += n;
	}
	close(fd);
	if (got != (long)size) { MCC_TRACE("br\n"); mcc_free(data); return NULL; }
	*status = EMBED_FOUND;
	*out_size = (long)size;
	return data;
}

static unsigned char *embed_resolve(MCCState *s1, const char *name, int delim, int64_t want,
																		long *out_size, int *status) { MCC_TRACE("enter\n");
	char buf[1024];
	unsigned char *data;
	int i;

	*status = EMBED_MISSING;
	if (HOST_IS_ABSPATH(name))
		{ MCC_TRACE("br\n"); return embed_read_file(name, want, out_size, status); }
	if (delim == '\"') { MCC_TRACE("br\n");
		char *p = file->true_filename;
		pstrncpy(buf, sizeof buf, p, mcc_basename(p) - p);
		pstrcat(buf, sizeof buf, name);
		data = embed_read_file(buf, want, out_size, status);
		if (data || *status != EMBED_MISSING)
			{ MCC_TRACE("br\n"); return data; }
	}
	for (i = 0; i < s1->nb_embed_paths; i++) { MCC_TRACE("br\n");
		pstrcpy(buf, sizeof buf, s1->embed_paths[i]);
		pstrcat(buf, sizeof buf, "/");
		pstrcat(buf, sizeof buf, name);
		data = embed_read_file(buf, want, out_size, status);
		if (data || *status != EMBED_MISSING)
			{ MCC_TRACE("br\n"); return data; }
	}
	return NULL;
}

static int64_t embed_want(EmbedParams *ep, long *out_off) { MCC_TRACE("enter\n");
	int64_t off = ep->offset < 0 ? 0 : ep->offset;

	*out_off = (long)off;
	if (ep->limit < 0 || off > INT64_MAX - ep->limit)
		{ MCC_TRACE("br\n"); return -1; }
	return off + ep->limit;
}

static char *embed_build_text(EmbedParams *ep, unsigned char *data, long off, long count) { MCC_TRACE("enter\n");
	CString cs;
	long i;
	char numbuf[16];

	cstr_new(&cs);
	if (count == 0) { MCC_TRACE("br\n");
		if (ep->if_empty)
			{ MCC_TRACE("br\n"); cstr_cat(&cs, ep->if_empty, -1); }
	} else { MCC_TRACE("br\n");
		if (ep->prefix) { MCC_TRACE("br\n");
			cstr_cat(&cs, ep->prefix, -1);
			cstr_ccat(&cs, ' ');
		}
		for (i = 0; i < count; i++) { MCC_TRACE("br\n");
			if (i)
				{ MCC_TRACE("br\n"); cstr_ccat(&cs, ','); }
			snprintf(numbuf, sizeof numbuf, "%d", (int)data[off + i]);
			cstr_cat(&cs, numbuf, -1);
		}
		if (ep->suffix) { MCC_TRACE("br\n");
			cstr_ccat(&cs, ' ');
			cstr_cat(&cs, ep->suffix, -1);
		}
	}
	cstr_ccat(&cs, 0);
	return (char *)cs.data;
}

static void embed_emit_text(MCCState *s1, char *text) { MCC_TRACE("enter\n");
	int len = strlen(text);
	if (s1->include_stack_ptr >= s1->include_stack + INCLUDE_STACK_SIZE)
		{ MCC_TRACE("br\n"); mcc_error("#include recursion too deep"); }
	*s1->include_stack_ptr++ = file;
	mcc_open_bf(s1, "<embed>", len);
	memcpy(file->buffer, text, len);
	mcc_free(text);
}

static void embed_directive(MCCState *s1) { MCC_TRACE("enter\n");
	char name[1024];
	int delim, status;
	EmbedParams ep;
	unsigned char *data;
	long size = 0, count, off;
	int64_t want;
	char *text;

	delim = embed_parse_name(name, sizeof name);
	embed_params_init(&ep);
	embed_parse_params(&ep, 0);

	want = embed_want(&ep, &off);
	data = embed_resolve(s1, name, delim, want, &size, &status);
	if (!data) { MCC_TRACE("br\n");
		embed_params_free(&ep);
		if (status == EMBED_NOT_REGULAR)
			{ MCC_TRACE("br\n"); mcc_error("device files are not yet supported by '#embed' directive"); }
		if (status == EMBED_TOO_LARGE)
			{ MCC_TRACE("br\n"); mcc_error("embed file '%s' exceeds the %d MiB '#embed' limit",
																		 name, (int)(EMBED_MAX_SIZE >> 20)); }
		mcc_error("embed file '%s' not found", name);
	}

	count = size - off;
	if (count < 0)
		{ MCC_TRACE("br\n"); count = 0; }
	if (ep.limit >= 0 && count > ep.limit)
		{ MCC_TRACE("br\n"); count = ep.limit; }

	text = embed_build_text(&ep, data, off, count);
	mcc_free(data);
	embed_params_free(&ep);

	embed_emit_text(s1, text);
}

static int has_embed_test(MCCState *s1) { MCC_TRACE("enter\n");
	char name[1024];
	int delim, c, status;
	EmbedParams ep;
	unsigned char *data;
	long size = 0, count, off;
	int64_t want;

	delim = embed_parse_name(name, sizeof name);
	embed_params_init(&ep);
	embed_parse_params(&ep, 1);

	want = embed_want(&ep, &off);
	data = embed_resolve(s1, name, delim, want, &size, &status);
	if (!data) { MCC_TRACE("br\n");
		c = status == EMBED_NOT_REGULAR ? 1 : 0;
	} else { MCC_TRACE("br\n");
		count = size - off;
		if (count < 0)
			{ MCC_TRACE("br\n"); count = 0; }
		if (ep.limit >= 0 && count > ep.limit)
			{ MCC_TRACE("br\n"); count = ep.limit; }
		c = count > 0 ? 1 : 2;
		mcc_free(data);
	}
	embed_params_free(&ep);
	return c;
}

static int pp_builtin_func(int v) { MCC_TRACE("enter\n");
	const char *n;
	if (v < TOK_IDENT)
		{ MCC_TRACE("br\n"); return 0; }
	n = get_tok_str(v, NULL);
	return !strcmp(n, "__has_feature") || !strcmp(n, "__has_extension") ||
				 !strcmp(n, "__has_builtin") || !strcmp(n, "__has_attribute") ||
				 !strcmp(n, "__has_cpp_attribute") || !strcmp(n, "__has_c_attribute") ||
				 !strcmp(n, "__has_declspec_attribute") || !strcmp(n, "__has_warning") ||
				 !strcmp(n, "__building_module") || !strcmp(n, "__is_target_arch") ||
				 !strcmp(n, "__is_target_os") || !strcmp(n, "__is_target_vendor") ||
				 !strcmp(n, "__is_target_environment");
}

static int pp_target_kind(int v) { MCC_TRACE("enter\n");
	const char *n;
	if (v < TOK_IDENT)
		{ MCC_TRACE("br\n"); return 0; }
	n = get_tok_str(v, NULL);
	if (!strcmp(n, "__is_target_arch"))
		{ MCC_TRACE("br\n"); return 1; }
	if (!strcmp(n, "__is_target_os"))
		{ MCC_TRACE("br\n"); return 2; }
	if (!strcmp(n, "__is_target_vendor"))
		{ MCC_TRACE("br\n"); return 3; }
	if (!strcmp(n, "__is_target_environment"))
		{ MCC_TRACE("br\n"); return 4; }
	return 0;
}

static int pp_streq_ci(const char *a, const char *b) { MCC_TRACE("enter\n");
	for (; *a && *b; a++, b++) { MCC_TRACE("br\n");
		int ca = *a, cb = *b;
		if (ca >= 'A' && ca <= 'Z')
			{ MCC_TRACE("br\n"); ca += 32; }
		if (cb >= 'A' && cb <= 'Z')
			{ MCC_TRACE("br\n"); cb += 32; }
		if (ca != cb)
			{ MCC_TRACE("br\n"); return 0; }
	}
	return *a == 0 && *b == 0;
}

static int pp_target_match(int kind, const char *arg) { MCC_TRACE("enter\n");
	static const char *const arch[] = {
#if defined MCC_TARGET_X86_64
			"x86_64", "amd64", "x86-64",
#elif defined MCC_TARGET_I386
			"i386", "i486", "i586", "i686", "x86",
#elif defined MCC_TARGET_ARM64
			"aarch64", "arm64",
#elif defined MCC_TARGET_ARM
			"arm", "armv7", "armv7a",
#elif defined MCC_TARGET_RISCV64
			"riscv64",
#endif
			0 };
	static const char *const os[] = {
#if defined MCC_TARGET_PE
			"windows", "win32",
#elif defined MCC_TARGET_MACHO
			"darwin", "macos", "macosx",
#else
			"linux",
#endif
			0 };
	/* vendor / environment: only the unambiguous components are answered; mcc
	 * carries no canonical triple, so a config where the vendor (pc vs w64) or
	 * environment (msvc vs gnu on PE) is genuinely ambiguous stays 0 rather than
	 * risk a wrong positive that would enable code the target does not match. */
	static const char *const vendor[] = {
#if defined MCC_TARGET_MACHO
			"apple",
#endif
			0 };
	static const char *const environment[] = {
#if !defined MCC_TARGET_PE && !defined MCC_TARGET_MACHO
			"gnu",
#endif
			0 };
	const char *const *tbl =
			kind == 1 ? arch : kind == 2 ? os : kind == 3 ? vendor : environment;
	int i;
	if (!arg[0])
		{ MCC_TRACE("br\n"); return 0; }
	for (i = 0; tbl[i]; i++) { MCC_TRACE("br\n");
		if (pp_streq_ci(tbl[i], arg))
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

static int pp_has_builtin_arg(int v) { MCC_TRACE("enter\n");
	static const char * const untokenized[] = {
		"__builtin_va_start", "__builtin_c23_va_start", "__builtin_va_arg",
		"__builtin_va_end", "__builtin_va_copy", "__builtin_va_list",
		"__builtin___memcpy_chk", "__builtin___memmove_chk",
		"__builtin___mempcpy_chk", "__builtin___memset_chk",
		"__builtin___snprintf_chk", "__builtin___sprintf_chk",
		"__builtin___stpcpy_chk", "__builtin___stpncpy_chk",
		"__builtin___strcat_chk", "__builtin___strcpy_chk",
		"__builtin___strncat_chk", "__builtin___strncpy_chk",
		"__builtin___vsnprintf_chk", "__builtin___vsprintf_chk"
	};
	const char *n;
	size_t i;

	if (v < TOK_IDENT)
		{ MCC_TRACE("br\n"); return 0; }
	n = get_tok_str(v, NULL);
	if (strncmp(n, "__builtin_", 10))
		{ MCC_TRACE("br\n"); return 0; }
	if (v > TOK_LAST && v < TOK_PREDEF_END)
		{ MCC_TRACE("br\n"); return 1; }
	for (i = 0; i < sizeof untokenized / sizeof untokenized[0]; i++) { MCC_TRACE("br\n");
		if (!strcmp(n, untokenized[i]))
			{ MCC_TRACE("br\n"); return 1; }
	}
	if (define_find(v) != NULL)
		{ MCC_TRACE("br\n"); return 1; }
	if (builtin_libm_is(n + 10))
		{ MCC_TRACE("br\n"); return 1; }
	return sym_find(v) != NULL;
}

static const char *pp_attr_canon(const char *n, char *buf, size_t bufsize) { MCC_TRACE("enter\n");
	size_t l = strlen(n);
	if (l > 4 && n[0] == '_' && n[1] == '_' && n[l - 1] == '_' && n[l - 2] == '_' &&
			l - 4 < bufsize) { MCC_TRACE("br\n");
		memcpy(buf, n + 2, l - 4);
		buf[l - 4] = '\0';
		return buf;
	}
	return n;
}

static int pp_attr_in_table(const char *n) { MCC_TRACE("enter\n");
	static const char * const attrs[] = {
		"alias", "aligned", "always_inline", "cdecl", "cleanup", "const",
		"constructor", "deprecated", "destructor", "dllexport", "dllimport",
		"fallthrough", "fastcall", "format", "gnu_inline", "maybe_unused",
		"mode", "nodebug", "nodecorate", "nodiscard", "noinline", "noreturn",
		"packed", "pure", "regparm", "reproducible", "section", "stdcall",
		"thiscall", "transparent_union", "unsequenced", "unused", "used",
		"vector_size", "visibility", "weak"
	};
	size_t i;
	for (i = 0; i < sizeof attrs / sizeof attrs[0]; i++) { MCC_TRACE("br\n");
		if (!strcmp(n, attrs[i]))
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

static int pp_attr_std_value(const char *n) { MCC_TRACE("enter\n");
	static const char * const std[] = {
		"deprecated", "fallthrough", "maybe_unused", "nodiscard", "noreturn",
		"reproducible", "unsequenced"
	};
	size_t i;
	if (!strcmp(n, "_Noreturn"))
		{ MCC_TRACE("br\n"); return 202311; }
	for (i = 0; i < sizeof std / sizeof std[0]; i++) { MCC_TRACE("br\n");
		if (!strcmp(n, std[i]))
			{ MCC_TRACE("br\n"); return 202311; }
	}
	return 0;
}

static int pp_has_attribute_arg(int v) { MCC_TRACE("enter\n");
	char buf[64];

	if (v < TOK_IDENT)
		{ MCC_TRACE("br\n"); return 0; }
	return pp_attr_in_table(pp_attr_canon(get_tok_str(v, NULL), buf, sizeof buf));
}

static int pp_has_feature_arg(int v, int strict) { MCC_TRACE("enter\n");
	static const char * const always[] = {
		"attribute_deprecated_with_message", "attribute_unavailable_with_message",
		"enumerator_attributes", "tls"
	};
	static const char * const only_ext[] = {
		"gnu_asm_goto_with_outputs", "gnu_asm_goto_with_outputs_full"
	};
	static const char * const c11[] = {
		"c_alignas", "c_alignof", "c_atomic", "c_generic_selections",
		"c_static_assert", "c_thread_local"
	};
	MCCState *s1 = mcc_state;
	char buf[64];
	const char *n;
	size_t i;

	if (v < TOK_IDENT)
		{ MCC_TRACE("br\n"); return 0; }
	n = pp_attr_canon(get_tok_str(v, NULL), buf, sizeof buf);
	if (s1->pedantic_errors)
		{ MCC_TRACE("br\n"); strict = 1; }
	for (i = 0; i < sizeof always / sizeof always[0]; i++) { MCC_TRACE("br\n");
		if (!strcmp(n, always[i]))
			{ MCC_TRACE("br\n"); return 1; }
	}
	for (i = 0; i < sizeof only_ext / sizeof only_ext[0]; i++) { MCC_TRACE("br\n");
		if (!strcmp(n, only_ext[i]))
			{ MCC_TRACE("br\n"); return !strict; }
	}
	for (i = 0; i < sizeof c11 / sizeof c11[0]; i++) { MCC_TRACE("br\n");
		if (!strcmp(n, c11[i]))
			{ MCC_TRACE("br\n"); return !strict || s1->cversion >= 201112; }
	}
	if (!strcmp(n, "cxx_binary_literals"))
		{ MCC_TRACE("br\n"); return !strict || s1->cversion >= 202311; }
	if (!strcmp(n, "address_sanitizer"))
		{ MCC_TRACE("br\n"); return !!s1->do_sanitize_address; }
	if (!strcmp(n, "undefined_behavior_sanitizer"))
		{ MCC_TRACE("br\n"); return !!s1->do_sanitize_undefined; }
	return 0;
}

static int pp_builtin_macro(int v) { MCC_TRACE("enter\n");
	return v == tok_has_attribute || v == tok_has_c_attribute ||
				 v == tok_has_cpp_attribute ||
				 v == tok_has_builtin || v == tok_has_feature ||
				 v == tok_has_extension;
}

static int pp_builtin_value(int v, const int *args) { MCC_TRACE("enter\n");
	char nbuf[64], sbuf[64];
	CValue cv;
	const char *n;
	int t, scope = 0, name = 0, c;

	for (;;) { MCC_TRACE("br\n");
		TOK_GET(&t, &args, &cv);
		if (t == 0 || t == TOK_EOF)
			{ MCC_TRACE("br\n"); break; }
		if (t == ' ')
			{ MCC_TRACE("br\n"); continue; }
		if (t == ':') { MCC_TRACE("br\n");
			if (name)
				{ MCC_TRACE("br\n"); scope = name, name = 0; }
			continue;
		}
		if (t >= TOK_IDENT && !name)
			{ MCC_TRACE("br\n"); name = t; }
	}

	if (v == tok_has_builtin)
		{ MCC_TRACE("br\n"); return pp_has_builtin_arg(name); }
	if (v == tok_has_feature)
		{ MCC_TRACE("br\n"); return pp_has_feature_arg(name, 1); }
	if (v == tok_has_extension)
		{ MCC_TRACE("br\n"); return pp_has_feature_arg(name, 0); }
	if (name < TOK_IDENT)
		{ MCC_TRACE("br\n"); return 0; }
	n = pp_attr_canon(get_tok_str(name, NULL), nbuf, sizeof nbuf);
	if (scope >= TOK_IDENT) { MCC_TRACE("br\n");
		if (strcmp(pp_attr_canon(get_tok_str(scope, NULL), sbuf, sizeof sbuf), "gnu"))
			{ MCC_TRACE("br\n"); return 0; }
		return pp_attr_in_table(n);
	}
	c = pp_attr_std_value(n);
	if (c || v == tok_has_c_attribute)
		{ MCC_TRACE("br\n"); return c; }
	return pp_attr_in_table(n);
}

static int expr_preprocess(MCCState *s1) { MCC_TRACE("enter\n");
	int t;
	int64_t c;
	int t0 = tok;
	TokenString *str;

	str = tok_str_alloc();
	pp_expr = 1;
	while (1) { MCC_TRACE("br\n");
		next();
		t = tok;
		if (tok == '#') { MCC_TRACE("br\n");
			int pred_tok;
			char *answer = NULL;
			MCCAssertion *a;
			next_nomacro();
			if (tok < TOK_IDENT)
				{ MCC_TRACE("br\n"); expect("identifier after '#'"); }
			pred_tok = tok;
			next_nomacro();
			if (tok == '(') { MCC_TRACE("br\n");
				answer = pp_capture_parens_text();
			} else { MCC_TRACE("br\n");
				unget_tok(tok);
			}
			a = find_assertion(s1, pred_tok);
			c = a ? assertion_has_answer(a, answer) : 0;
			mcc_free(answer);
			goto c_number;
		} else if (tok < TOK_IDENT) { MCC_TRACE("br\n");
			if (tok == TOK_LINEFEED || tok == TOK_EOF)
				{ MCC_TRACE("br\n"); break; }
			if ((tok >= TOK_STR && tok <= TOK_CLDOUBLE) || tok == TOK_CFLOAT16 || tok == TOK_CFLOAT128)
				{ MCC_TRACE("br\n"); mcc_error("invalid constant in preprocessor expression"); }
		} else if (tok == TOK_DEFINED) { MCC_TRACE("br\n");
			parse_flags &= ~PARSE_FLAG_PREPROCESS;
			next();
			t = tok;
			if (t == '(')
				{ MCC_TRACE("br\n"); next(); }
			parse_flags |= PARSE_FLAG_PREPROCESS;
			if (tok < TOK_IDENT)
				{ MCC_TRACE("br\n"); expect("identifier after 'defined'"); }
			if (s1->run_test)
				{ MCC_TRACE("br\n"); maybe_run_test(s1); }
			c = 0;
			if (define_find(tok) || tok == TOK___HAS_INCLUDE || tok == TOK___HAS_INCLUDE_NEXT ||
					tok == TOK___HAS_EMBED)
				{ MCC_TRACE("br\n"); c = 1; }
			if (t == '(') { MCC_TRACE("br\n");
				next();
				if (tok != ')')
					{ MCC_TRACE("br\n"); expect("')'"); }
			}
			goto c_number;
		} else if (tok == TOK___HAS_INCLUDE ||
							 tok == TOK___HAS_INCLUDE_NEXT) { MCC_TRACE("br\n");
			t = tok;
			next();
			if (tok != '(')
				{ MCC_TRACE("br\n"); expect("'('"); }
			c = parse_include(s1, t - TOK___HAS_INCLUDE, 1, 0);
			if (tok != ')')
				{ MCC_TRACE("br\n"); expect("')'"); }
			goto c_number;
		} else if (tok == TOK___HAS_EMBED) { MCC_TRACE("br\n");
			next();
			if (tok != '(')
				{ MCC_TRACE("br\n"); expect("'('"); }
			c = has_embed_test(s1);
			if (tok != ')')
				{ MCC_TRACE("br\n"); expect("')'"); }
			goto c_number;
		} else if (s1->cversion >= 202311 &&
							 (tok == tok_c23_true || tok == tok_c23_false)) { MCC_TRACE("br\n");
			c = tok == tok_c23_true;
			goto c_number;
		} else if (pp_builtin_func(tok)) { MCC_TRACE("br\n");
			int depth = 1, kind = pp_target_kind(tok), first = 1;
			char arg[64];
			arg[0] = 0;
			c = 0;
			next();
			if (tok != '(')
				{ MCC_TRACE("br\n"); expect("'('"); }
			while (depth) { MCC_TRACE("br\n");
				next();
				if (tok == TOK_EOF || tok == TOK_LINEFEED)
					{ MCC_TRACE("br\n"); expect("')'"); }
				if (tok == '(')
					{ MCC_TRACE("br\n"); depth++; }
				else if (tok == ')')
					{ MCC_TRACE("br\n"); depth--; }
				else if (first && kind) { MCC_TRACE("br\n");
					const char *s = get_tok_str(tok, &tokc);
					if (s[0] == '"')
						{ MCC_TRACE("br\n"); s++; }
					pstrcpy(arg, sizeof arg, s);
					{
						int al = (int)strlen(arg);
						if (al && arg[al - 1] == '"')
							{ MCC_TRACE("br\n"); arg[al - 1] = 0; }
					}
				}
				first = 0;
			}
			if (kind)
				{ MCC_TRACE("br\n"); c = pp_target_match(kind, arg); }
			goto c_number;
		} else { MCC_TRACE("br\n");
			mcc_warning_c(warn_undef)("\"%s\" is not defined, evaluates to 0",
																get_tok_str(tok, &tokc));
			c = 0;
		c_number:
			tok = TOK_CLLONG;
			tokc.i = c;
		}
		tok_str_add_tok(str);
	}
	if (0 == str->len)
		{ MCC_TRACE("br\n"); mcc_error("#%s with no expression", get_tok_str(t0, 0)); }
	tok_str_add(str, TOK_EOF);
	pp_expr = t0;
	t = tok;
	begin_macro(str, 1);
	next();
	c = expr_const64_pub();
	if (tok != TOK_EOF)
		{ MCC_TRACE("br\n"); mcc_error("..."); }
	pp_expr = 0;
	end_macro();
	tok = t;
	return c != 0;
}

ST_FUNC void pp_error(CString *cs) { MCC_TRACE("enter\n");
	cstr_printf(cs, "bad preprocessor expression: #%s", get_tok_str(pp_expr, 0));
	macro_ptr = macro_stack->str;
	while (next(), tok != TOK_EOF)
		{ MCC_TRACE("br\n"); cstr_printf(cs, " %s", get_tok_str(tok, &tokc)); }
}

static int is_predef_macro(int v) { MCC_TRACE("enter\n");
	const char *n;
	if (v == TOK___LINE__ || v == TOK___FILE__ || v == TOK___DATE__ || v == TOK___TIME__ ||
			v == TOK___COUNTER__ || v == TOK___INCLUDE_LEVEL__ ||
			v == TOK___FILE_NAME__ || v == TOK___TIMESTAMP__)
		{ MCC_TRACE("br\n"); return 1; }
	if (v >= TOK_IDENT) { MCC_TRACE("br\n");
		n = get_tok_str(v, NULL);
		if (!strcmp(n, "__STDC__") || !strcmp(n, "__STDC_VERSION__") || !strcmp(n, "__STDC_HOSTED__"))
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

ST_FUNC void parse_define(void) { MCC_TRACE("enter\n");
	Sym *s, *first, **ps;
	int v, t, varg, is_vaargs, t0;
	int func_like, hash_pending = 0;
	int va_opt_pending = 0, va_opt_level = 0, va_opt_start = 0;
	int saved_parse_flags = parse_flags;
	TokenString str;

	v = tok;
	if (v < TOK_IDENT || v == TOK_DEFINED || v == TOK___VA_ARGS__)
		{ MCC_TRACE("br\n"); mcc_error("invalid macro name '%s'", get_tok_str(tok, &tokc)); }
	if (is_predef_macro(v))
		{ MCC_TRACE("br\n"); warn_macro_redefined(v); }
	first = NULL;
	t = MACRO_OBJ;
	parse_flags = ((parse_flags & ~PARSE_FLAG_ASM_FILE) | PARSE_FLAG_SPACES);
	next_nomacro();
	parse_flags &= ~PARSE_FLAG_SPACES;
	is_vaargs = 0;
	if (tok == '(') { MCC_TRACE("br\n");
		int dotid = set_idnum('.', 0);
		next_nomacro();
		ps = &first;
		if (tok != ')')
			{ MCC_TRACE("br\n"); for (;;) { MCC_TRACE("br\n");
				varg = tok;
				next_nomacro();
				is_vaargs = 0;
				if (varg == TOK_DOTS) { MCC_TRACE("br\n");
					varg = TOK___VA_ARGS__;
					is_vaargs = 1;
					if (mcc_state->warn_pedantic && mcc_state->cversion < 199901 && !pp_in_system_header()) { MCC_TRACE("br\n");
						if (mcc_state->pedantic_errors)
							{ MCC_TRACE("br\n"); mcc_error("variadic macros are a C99 feature"); }
						else
							{ MCC_TRACE("br\n"); mcc_warning("variadic macros are a C99 feature"); }
					}
				} else if (tok == TOK_DOTS && gnu_ext) { MCC_TRACE("br\n");
					is_vaargs = 1;
					next_nomacro();
				}
				if (varg < TOK_IDENT)
				{ MCC_TRACE("br\n"); bad_list:
					mcc_error("bad macro parameter list"); }
				{
					Sym *pp;
					for (pp = first; pp; pp = pp->next)
						{ MCC_TRACE("br\n"); if ((pp->v & ~SYM_FIELD) == (varg & ~SYM_FIELD))
							{ MCC_TRACE("br\n"); mcc_error("duplicate macro parameter \"%s\"",
												get_tok_str(varg, NULL)); } }
				}
				s = sym_push2(&define_stack, varg | SYM_FIELD, is_vaargs, 0);
				*ps = s;
				ps = &s->next;
				if (tok == ')')
					{ MCC_TRACE("br\n"); break; }
				if (tok != ',' || is_vaargs)
					{ MCC_TRACE("br\n"); goto bad_list; }
				next_nomacro();
			} }
		parse_flags |= PARSE_FLAG_SPACES;
		next_nomacro();
		t = MACRO_FUNC;
		set_idnum('.', dotid);
	}

	func_like = (t == MACRO_FUNC);
	parse_flags |= PARSE_FLAG_ACCEPT_STRAYS | PARSE_FLAG_SPACES | PARSE_FLAG_LINEFEED;
	tok_str_new(&str);
	t0 = 0;
	while (tok != TOK_LINEFEED && tok != TOK_EOF) { MCC_TRACE("br\n");
		if (is_space(tok)) { MCC_TRACE("br\n");
			str.need_spc |= 1;
		} else { MCC_TRACE("br\n");
			if (TOK_TWOSHARPS == tok || TOK_DIG_TWOSHARPS == tok) { MCC_TRACE("br\n");
				if (0 == t0)
					{ MCC_TRACE("br\n"); goto bad_twosharp; }
				tok = TOK_PPJOIN;
				t |= MACRO_JOIN;
			}
			if (va_opt_pending) { MCC_TRACE("br\n");
				if (tok != '(')
					{ MCC_TRACE("br\n"); mcc_error("expected '(' after '__VA_OPT__'"); }
				va_opt_pending = 0;
				va_opt_level = 1;
				va_opt_start = 1;
			} else if (va_opt_level) { MCC_TRACE("br\n");
				if (tok == '(')
					{ MCC_TRACE("br\n"); va_opt_level++; }
				else if (tok == ')')
					{ MCC_TRACE("br\n"); va_opt_level--; }
				if (va_opt_start && tok == TOK_PPJOIN)
					{ MCC_TRACE("br\n"); mcc_error("'##' cannot appear at either end of '__VA_OPT__'"); }
				if (va_opt_level == 0 && t0 == TOK_PPJOIN)
					{ MCC_TRACE("br\n"); mcc_error("'##' cannot appear at either end of '__VA_OPT__'"); }
				va_opt_start = 0;
			}
			if (tok == tok_va_opt) { MCC_TRACE("br\n");
				if (va_opt_level)
					{ MCC_TRACE("br\n"); mcc_error("'__VA_OPT__' may not appear in a '__VA_OPT__' operand"); }
				if (!func_like || !is_vaargs)
					{ MCC_TRACE("br\n"); mcc_warning("'__VA_OPT__' can only appear in the expansion of a "
											"C23 variadic macro"); }
				else
					{ MCC_TRACE("br\n"); va_opt_pending = 1; }
			}
			if (hash_pending) { MCC_TRACE("br\n");
				Sym *pp;
				int isparam = tok == tok_va_opt;
				for (pp = first; pp && !isparam; pp = pp->next)
					{ MCC_TRACE("br\n"); if ((pp->v & ~SYM_FIELD) == (tok & ~SYM_FIELD)) { MCC_TRACE("br\n");
						isparam = 1;
						break;
					} }
				if (!isparam)
					{ MCC_TRACE("br\n"); mcc_error("'#' is not followed by a macro parameter"); }
				hash_pending = 0;
			}
			hash_pending = (func_like && (tok == '#' || tok == TOK_DIG_HASH));
			if (tok == TOK___VA_ARGS__ && !is_vaargs)
				{ MCC_TRACE("br\n"); mcc_warning("__VA_ARGS__ can only appear in the expansion of a "
										"C99 variadic macro"); }
			tok_str_add2_spc(&str, tok, &tokc);
			t0 = tok;
		}
		next_nomacro();
	}
	if (hash_pending)
		{ MCC_TRACE("br\n"); mcc_error("'#' is not followed by a macro parameter"); }
	if (va_opt_pending || va_opt_level)
		{ MCC_TRACE("br\n"); mcc_error("unterminated '__VA_OPT__'"); }
	parse_flags = saved_parse_flags;
	tok_str_add(&str, 0);
	if (t0 == TOK_PPJOIN)
	{ MCC_TRACE("br\n"); bad_twosharp:
		mcc_error("'##' cannot appear at either end of macro"); }
	define_push(v, t, str.str, first);
}

static CachedInclude *search_cached_include(MCCState *s1, const char *filename, int add) { MCC_TRACE("enter\n");
	const char *s, *basename;
	unsigned int h;
	CachedInclude *e;
	int c, i, len;

	s = basename = mcc_basename(filename);
	h = TOK_HASH_INIT;
	while ((c = (unsigned char)*s) != 0) { MCC_TRACE("br\n");
		h = TOK_HASH_FUNC(h, host_path_hash_fold(c));
		s++;
	}
	h &= (CACHED_INCLUDES_HASH_SIZE - 1);

	i = s1->cached_includes_hash[h];
	for (;;) { MCC_TRACE("br\n");
		if (i == 0)
			{ MCC_TRACE("br\n"); break; }
		e = s1->cached_includes[i - 1];
		if (0 == HOST_PATHCMP(filename, e->filename))
			{ MCC_TRACE("br\n"); return e; }
		if (e->once && 0 == HOST_PATHCMP(basename, mcc_basename(e->filename)) && 0 == normalized_PATHCMP(filename, e->filename))
			{ MCC_TRACE("br\n"); return e; }
		i = e->hash_next;
	}
	if (!add)
		{ MCC_TRACE("br\n"); return NULL; }

	e = mcc_malloc(sizeof(CachedInclude) + (len = strlen(filename)));
	memcpy(e->filename, filename, len + 1);
	e->ifndef_macro = e->once = 0;
	e->dev = e->ino = 0;
	dynarray_add(&s1->cached_includes, &s1->nb_cached_includes, e);
	e->hash_next = s1->cached_includes_hash[h];
	s1->cached_includes_hash[h] = s1->nb_cached_includes;
	return e;
}

static int once_seen_by_file_id(MCCState *s1, const char *path) { MCC_TRACE("enter\n");
	unsigned long long dev = 0, ino = 0;
	int i;
	if (host_file_id(path, &dev, &ino))
		{ MCC_TRACE("br\n"); return 0; }
	if (ino == 0)
		{ MCC_TRACE("br\n"); return 0; }
	for (i = 0; i < s1->nb_cached_includes; i++) { MCC_TRACE("br\n");
		CachedInclude *e = s1->cached_includes[i];
		if (e->once && e->ino == ino && e->dev == dev)
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

struct pp_diag_snap {
	unsigned char a[offsetof(MCCState, warn_extra_ptr_zero_cmp) - offsetof(MCCState, warn_none) + 1];
	unsigned char b[offsetof(MCCState, pedantic_errors) - offsetof(MCCState, warn_pedantic) + 1];
};
static struct pp_diag_snap *pp_diag_stack;
static int pp_diag_stack_n;

static void pp_diag_push(MCCState *s) { MCC_TRACE("enter\n");
	struct pp_diag_snap snap;
	memcpy(snap.a, (char *)s + offsetof(MCCState, warn_none), sizeof snap.a);
	memcpy(snap.b, (char *)s + offsetof(MCCState, warn_pedantic), sizeof snap.b);
	pp_diag_stack = mcc_realloc(pp_diag_stack, (pp_diag_stack_n + 1) * sizeof(*pp_diag_stack));
	pp_diag_stack[pp_diag_stack_n++] = snap;
}

static void pp_diag_pop(MCCState *s) { MCC_TRACE("enter\n");
	struct pp_diag_snap *snap;
	if (pp_diag_stack_n <= 0) { MCC_TRACE("br\n");
		mcc_warning("#pragma GCC diagnostic pop without matching push");
		return;
	}
	snap = &pp_diag_stack[--pp_diag_stack_n];
	memcpy((char *)s + offsetof(MCCState, warn_none), snap->a, sizeof snap->a);
	memcpy((char *)s + offsetof(MCCState, warn_pedantic), snap->b, sizeof snap->b);
}

static int pragma_parse(MCCState *s1) { MCC_TRACE("enter\n");
	next_nomacro();
	if (tok == TOK_push_macro || tok == TOK_pop_macro) { MCC_TRACE("br\n");
		int t = tok, v;
		Sym *s;

		if (next(), tok != '(')
			{ MCC_TRACE("br\n"); goto pragma_err; }
		if (next(), tok != TOK_STR)
			{ MCC_TRACE("br\n"); goto pragma_err; }
		v = tok_alloc(tokc.str.data, tokc.str.size - 1)->tok;
		if (next(), tok != ')')
			{ MCC_TRACE("br\n"); goto pragma_err; }
		if (t == TOK_push_macro) { MCC_TRACE("br\n");
			while (NULL == (s = define_find(v)))
				{ MCC_TRACE("br\n"); define_push(v, 0, NULL, NULL); }
			s->type.ref = s;
		} else { MCC_TRACE("br\n");
			for (s = define_stack; s; s = s->prev)
				{ MCC_TRACE("br\n"); if (s->v == v && s->type.ref == s) { MCC_TRACE("br\n");
					s->type.ref = NULL;
					break;
				} }
		}
		if (s)
			{ MCC_TRACE("br\n"); table_ident[v - TOK_IDENT]->sym_define = s->d ? s : NULL; }
		else
			{ MCC_TRACE("br\n"); mcc_warning("unbalanced #pragma pop_macro"); }
		pp_debug_tok = t, pp_debug_symv = v;
	} else if (tok == TOK_once) { MCC_TRACE("br\n");
		search_cached_include(s1, file->true_filename, 1)->once = 1;
	} else if (tok == TOK_WEAK1) { MCC_TRACE("br\n");
		next_nomacro();
		if (tok >= TOK_IDENT) { MCC_TRACE("br\n");
			int alias_tok = tok;
			const char *nm = get_tok_str(tok, NULL);
			int i, dup = 0;
			for (i = 0; i < s1->nb_pragma_weak_syms; i++)
				{ MCC_TRACE("br\n"); if (0 == strcmp(s1->pragma_weak_syms[i], nm)) { MCC_TRACE("br\n"); dup = 1; break; } }
			if (!dup)
				{ MCC_TRACE("br\n"); dynarray_add(&s1->pragma_weak_syms, &s1->nb_pragma_weak_syms, mcc_strdup(nm)); }
			next_nomacro();
			if (tok == '=') { MCC_TRACE("br\n");
				next_nomacro();
				if (tok >= TOK_IDENT) { MCC_TRACE("br\n");
					AliasFixup *af = mcc_malloc(sizeof *af);
					af->alias_v = alias_tok;
					af->target_v = tok;
					dynarray_add(&s1->alias_fixups, &s1->nb_alias_fixups, af);
				}
			}
		}
		while (tok != TOK_LINEFEED && tok != TOK_EOF)
			{ MCC_TRACE("br\n"); next_nomacro(); }
		return 1;
	} else if (tok >= TOK_IDENT &&
						 !strcmp(get_tok_str(tok, NULL), "__mcc_cmdline_defs__")) { MCC_TRACE("br\n");
		/* T-mac-30219: internal marker injected after the built-in predefs; from
		 * here on the <command line> buffer holds the user's -D options, whose
		 * redefinitions warn (built-in predefs before it stay suppressed). */
		pp_cmdline_user = 1;
		while (tok != TOK_LINEFEED && tok != TOK_EOF)
			{ MCC_TRACE("br\n"); next_nomacro(); }
		return 1;
	} else if (s1->output_type == MCC_OUTPUT_PREPROCESS) { MCC_TRACE("br\n");
		unget_tok(' ');
		unget_tok(TOK_PRAGMA);
		unget_tok('#');
		unget_tok(TOK_LINEFEED);
		return 1;
	} else if (tok == TOK_pack) { MCC_TRACE("br\n");
		next();
		skip('(');
		if (tok == TOK_ASM_pop) { MCC_TRACE("br\n");
			next();
			if (s1->pack_stack_ptr <= s1->pack_stack) { MCC_TRACE("br\n");
				mcc_warning("#pragma pack(pop) without matching push, ignored");
			} else { MCC_TRACE("br\n");
				s1->pack_stack_ptr--;
			}
		} else { MCC_TRACE("br\n");
			int val = 0;
			if (tok != ')') { MCC_TRACE("br\n");
				if (tok == TOK_ASM_push) { MCC_TRACE("br\n");
					next();
					if (s1->pack_stack_ptr >= s1->pack_stack + PACK_STACK_SIZE - 1)
						{ MCC_TRACE("br\n"); mcc_error("#pragma pack(push) nested too deeply"); }
					val = *s1->pack_stack_ptr++;
					if (tok != ',')
						{ MCC_TRACE("br\n"); goto pack_set; }
					next();
				}
				if (tok >= TOK_IDENT) { MCC_TRACE("br\n");
					mcc_warning_c(warn_unknown_pragmas)(
							"unknown action '%s' for '#pragma pack' - ignored",
							get_tok_str(tok, &tokc));
					while (tok != TOK_LINEFEED && tok != TOK_EOF)
						{ MCC_TRACE("br\n"); next_nomacro(); }
					return 1;
				}
				if (tok != TOK_CINT)
					{ MCC_TRACE("br\n"); goto pragma_err; }
				val = tokc.i;
				if (val < 1 || val > 16 || (val & (val - 1)) != 0)
					{ MCC_TRACE("br\n"); goto pragma_err; }
				next();
			}
		pack_set:
			*s1->pack_stack_ptr = val;
		}
		if (tok != ')')
			{ MCC_TRACE("br\n"); goto pragma_err; }
	} else if (tok == TOK_comment) { MCC_TRACE("br\n");
		char *p;
		int t;
		next();
		skip('(');
		t = tok;
		next();
		skip(',');
		if (tok != TOK_STR)
			{ MCC_TRACE("br\n"); goto pragma_err; }
		p = mcc_strdup(tokc.str.data);
		next();
		if (tok != ')')
			{ MCC_TRACE("br\n"); goto pragma_err; }
		if (t == TOK_lib) { MCC_TRACE("br\n");
			dynarray_add(&s1->pragma_libs, &s1->nb_pragma_libs, p);
		} else { MCC_TRACE("br\n");
			if (t == TOK_option)
				{ MCC_TRACE("br\n"); mcc_set_options(s1, p); }
			mcc_free(p);
		}
	} else if (tok == TOK_pragma_message) { MCC_TRACE("br\n");
		int paren = 0;
		CString pmsg;
		next();
		if (tok == '(') { MCC_TRACE("br\n");
			paren = 1;
			next();
		}
		if (tok != TOK_STR)
			{ MCC_TRACE("br\n"); goto pragma_err; }
		cstr_new(&pmsg);
		/* concatenate adjacent string literals, e.g. #pragma message("a " "b") */
		while (tok == TOK_STR) { MCC_TRACE("br\n");
			cstr_cat(&pmsg, (char *)tokc.str.data, -1);
			next();
		}
		cstr_ccat(&pmsg, '\0');
		if (file)
			{ MCC_TRACE("br\n"); fprintf(stderr, "%s:%d: note: #pragma message: %s\n",
							file->filename, file->line_num, (char *)pmsg.data); }
		else
			{ MCC_TRACE("br\n"); fprintf(stderr, "note: #pragma message: %s\n",
							(char *)pmsg.data); }
		cstr_free(&pmsg);
		if (paren) { MCC_TRACE("br\n");
			if (tok != ')')
				{ MCC_TRACE("br\n"); goto pragma_err; }
			next();
		}
		while (tok != TOK_LINEFEED && tok != TOK_EOF)
			{ MCC_TRACE("br\n"); next_nomacro(); }
		return 1;
	} else if (tok == TOK_STDC) { MCC_TRACE("br\n");
		unsigned char *slot, state;
		const char *sw;
		next_nomacro();
		sw = get_tok_str(tok, &tokc);
		if (!strcmp(sw, "FP_CONTRACT"))
			{ MCC_TRACE("br\n"); slot = &s1->stdc_fp_contract; }
		else if (!strcmp(sw, "FENV_ACCESS"))
			{ MCC_TRACE("br\n"); slot = &s1->stdc_fenv_access; }
		else if (!strcmp(sw, "CX_LIMITED_RANGE"))
			{ MCC_TRACE("br\n"); slot = &s1->stdc_cx_limited; }
		else { MCC_TRACE("br\n");
			mcc_warning_c(warn_all)("unknown #pragma STDC '%s'", sw);
			while (tok != TOK_LINEFEED && tok != TOK_EOF)
				{ MCC_TRACE("br\n"); next_nomacro(); }
			return 1;
		}
		next_nomacro();
		{
			const char *st = get_tok_str(tok, &tokc);
			if (!strcmp(st, "ON"))
				{ MCC_TRACE("br\n"); state = STDC_ON; }
			else if (!strcmp(st, "OFF"))
				{ MCC_TRACE("br\n"); state = STDC_OFF; }
			else if (!strcmp(st, "DEFAULT"))
				{ MCC_TRACE("br\n"); state = STDC_DEFAULT; }
			else { MCC_TRACE("br\n");
				mcc_warning_c(warn_all)(
						"malformed #pragma STDC %s (expected ON/OFF/DEFAULT)", sw);
				while (tok != TOK_LINEFEED && tok != TOK_EOF)
					{ MCC_TRACE("br\n"); next_nomacro(); }
				return 1;
			}
		}
		*slot = state;
		while (tok != TOK_LINEFEED && tok != TOK_EOF)
			{ MCC_TRACE("br\n"); next_nomacro(); }
		return 1;
	} else if (tok >= TOK_IDENT && !strcmp(get_tok_str(tok, &tokc), "options")) { MCC_TRACE("br\n");
		const char *am;
		int val = -1;
		next_nomacro();
		if (tok < TOK_IDENT || strcmp(get_tok_str(tok, &tokc), "align")) { MCC_TRACE("br\n");
			mcc_warning("expected 'align' following '#pragma options'");
			goto pragma_align_eol;
		}
		next_nomacro();
		if (tok != '=') { MCC_TRACE("br\n");
			mcc_warning("expected '=' following '#pragma options align'");
			goto pragma_align_eol;
		}
		next_nomacro();
		if (tok < TOK_IDENT) { MCC_TRACE("br\n");
			mcc_warning("expected identifier in '#pragma options'");
			goto pragma_align_eol;
		}
		am = get_tok_str(tok, &tokc);
		if (!strcmp(am, "natural") || !strcmp(am, "native") || !strcmp(am, "power"))
			{ MCC_TRACE("br\n"); val = 0; }
		else if (!strcmp(am, "packed"))
			{ MCC_TRACE("br\n"); val = 1; }
		else if (!strcmp(am, "mac68k")) { MCC_TRACE("br\n");
			mcc_warning_c(warn_unknown_pragmas)(
					"mac68k alignment pragma is not supported, ignored");
			goto pragma_align_eol;
		}
		else if (strcmp(am, "reset")) { MCC_TRACE("br\n");
			mcc_warning("invalid alignment option in '#pragma options align'");
			goto pragma_align_eol;
		}
		next_nomacro();
		if (tok != TOK_LINEFEED && tok != TOK_EOF) { MCC_TRACE("br\n");
			mcc_warning("extra tokens at end of '#pragma options'");
			goto pragma_align_eol;
		}
		if (val < 0) { MCC_TRACE("br\n");
			if (s1->pack_stack_ptr <= s1->pack_stack)
				{ MCC_TRACE("br\n"); mcc_warning("#pragma options align=reset failed: stack empty"); }
			else
				{ MCC_TRACE("br\n"); s1->pack_stack_ptr--; }
		} else if (s1->pack_stack_ptr >= s1->pack_stack + PACK_STACK_SIZE - 1) { MCC_TRACE("br\n");
			mcc_error("out of pack stack");
		} else { MCC_TRACE("br\n");
			s1->pack_stack_ptr++;
			*s1->pack_stack_ptr = val;
		}
	pragma_align_eol:
		while (tok != TOK_LINEFEED && tok != TOK_EOF)
			{ MCC_TRACE("br\n"); next_nomacro(); }
		return 1;
	} else if (tok >= TOK_IDENT && !strcmp(get_tok_str(tok, NULL), "GCC")) { MCC_TRACE("br\n");
		next_nomacro();
		if (tok >= TOK_IDENT && !strcmp(get_tok_str(tok, NULL), "poison")) { MCC_TRACE("br\n");
			for (;;) { MCC_TRACE("br\n");
				next_nomacro();
				if (tok < TOK_IDENT)
					{ MCC_TRACE("br\n"); break; }
				if (!pp_is_poisoned(tok)) { MCC_TRACE("br\n");
					pp_poison = mcc_realloc(pp_poison, (pp_npoison + 1) * sizeof(int));
					pp_poison[pp_npoison++] = tok;
				}
			}
			return 1;
		}
		if (tok >= TOK_IDENT && (!strcmp(get_tok_str(tok, NULL), "warning") ||
														 !strcmp(get_tok_str(tok, NULL), "error"))) { MCC_TRACE("br\n");
			int is_err = !strcmp(get_tok_str(tok, NULL), "error");
			char *msg = NULL;
			next_nomacro();
			if (tok == TOK_STR)
				{ MCC_TRACE("br\n"); msg = mcc_strdup((char *)tokc.str.data); }
			while (tok != TOK_LINEFEED && tok != TOK_EOF)
				{ MCC_TRACE("br\n"); next_nomacro(); }
			if (is_err)
				{ MCC_TRACE("br\n"); mcc_error("%s", msg ? msg : "#pragma GCC error"); }
			else
				{ MCC_TRACE("br\n"); mcc_warning("%s", msg ? msg : "#pragma GCC warning"); }
			if (msg)
				{ MCC_TRACE("br\n"); mcc_free(msg); }
			return 1;
		}
		if (tok >= TOK_IDENT && !strcmp(get_tok_str(tok, NULL), "diagnostic")) { MCC_TRACE("br\n");
			next_nomacro();
			const char *sub = tok >= TOK_IDENT ? get_tok_str(tok, NULL) : "";
			if (!strcmp(sub, "push")) { MCC_TRACE("br\n"); pp_diag_push(s1); }
			else if (!strcmp(sub, "pop")) { MCC_TRACE("br\n"); pp_diag_pop(s1); }
			else { MCC_TRACE("br\n");
				int mode = !strcmp(sub, "ignored") ? 0
						: !strcmp(sub, "error") ? 2
						: (!strcmp(sub, "warning") || !strcmp(sub, "default")) ? 1
						: -1;
				if (mode >= 0) { MCC_TRACE("br\n");
					next_nomacro();
					if (tok == TOK_STR || tok == TOK_PPSTR) { MCC_TRACE("br\n");
						const char *raw = (const char *)tokc.str.data;
						char nm[128];
						int k = 0;
						if (tok == TOK_PPSTR && raw[0] == '"') { MCC_TRACE("br\n");
							raw++;
							while (*raw && *raw != '"' && k < (int)sizeof nm - 1)
								{ MCC_TRACE("br\n"); nm[k++] = *raw++; }
							nm[k] = 0;
						} else { MCC_TRACE("br\n");
							snprintf(nm, sizeof nm, "%s", raw);
						}
						if (nm[0] == '-' && nm[1] == 'W') { MCC_TRACE("br\n");
							char buf[128];
							if (mode == 0)
								{ MCC_TRACE("br\n"); snprintf(buf, sizeof buf, "no-%s", nm + 2); }
							else if (mode == 2)
								{ MCC_TRACE("br\n"); snprintf(buf, sizeof buf, "error=%s", nm + 2); }
							else
								{ MCC_TRACE("br\n"); snprintf(buf, sizeof buf, "%s", nm + 2); }
							pp_diag_set_flag(s1, buf);
						}
					}
				}
			}
			while (tok != TOK_LINEFEED && tok != TOK_EOF)
				{ MCC_TRACE("br\n"); next_nomacro(); }
			return 1;
		}
		while (tok != TOK_LINEFEED && tok != TOK_EOF)
			{ MCC_TRACE("br\n"); next_nomacro(); }
		return 1;
	} else if (tok >= TOK_IDENT && (!strcmp(get_tok_str(tok, NULL), "region") ||
									!strcmp(get_tok_str(tok, NULL), "endregion"))) { MCC_TRACE("br\n");
		while (tok != TOK_LINEFEED && tok != TOK_EOF)
			{ MCC_TRACE("br\n"); next_nomacro(); }
		return 1;
	} else { MCC_TRACE("br\n");
		mcc_warning_c(warn_unknown_pragmas)("#pragma %s ignored",
																				get_tok_str(tok, &tokc));
		return 0;
	}
	next();
	return 1;
pragma_err:
	mcc_warning_c(warn_unknown_pragmas)("malformed #pragma directive, ignored");
	while (tok != TOK_LINEFEED && tok != TOK_EOF)
		{ MCC_TRACE("br\n"); next_nomacro(); }
	return 1;
}

ST_FUNC void mccpp_putfile(const char *filename) { MCC_TRACE("enter\n");
	char buf[1024];

	pstrcpy(buf, sizeof buf, filename);
	host_path_normalize(buf);
	if (0 == strcmp(file->filename, buf))
		{ MCC_TRACE("br\n"); return; }
	if (file->true_filename == file->filename)
		{ MCC_TRACE("br\n"); file->true_filename = mcc_strdup(file->filename); }
	pstrcpy(file->filename, sizeof file->filename, buf);
	mcc_debug_newfile(mcc_state);
}

static uint16_t cst_pp_dir_kind(int t) { MCC_TRACE("enter\n");
	switch (t) { MCC_TRACE("br\n");
	case TOK_INCLUDE:
	case TOK_INCLUDE_NEXT:
		return CST_IncludeDirective;
	case TOK_IF:
	case TOK_IFDEF:
	case TOK_IFNDEF:
	case TOK_ELSE:
	case TOK_ELIF:
	case TOK_ENDIF:
		return CST_PPConditional;
	case TOK_LINEFEED:
		return 0;
	default:
		return CST_PPDirective;
	}
}

static int pp_directive_depth;
static int pp_comment_ready;
static CString pp_comment_text;

ST_FUNC void preprocess(int is_bof) { MCC_TRACE("enter\n");
	MCCState *s1 = mcc_state;
	int c, n, saved_parse_flags;
	char *q;
	Sym *s;
	uint32_t cst_pp_first = 0;
	uint16_t cst_pp_kind = 0;

	saved_parse_flags = parse_flags;
	parse_flags = PARSE_FLAG_PREPROCESS | PARSE_FLAG_TOK_NUM | PARSE_FLAG_TOK_STR | PARSE_FLAG_LINEFEED | (parse_flags & PARSE_FLAG_ASM_FILE);
	pp_directive_depth++;

	next_nomacro();
redo:
	cst_pp_first = cst_leafcount() ? cst_leafcount() - 1 : 0;
	cst_pp_kind = (file == cst_main_bf) ? cst_pp_dir_kind(tok) : 0;
	switch (tok) { MCC_TRACE("br\n");
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
		if (tok == TOK_DEFINED || tok == TOK___VA_ARGS__)
			{ MCC_TRACE("br\n"); mcc_error("invalid macro name '%s'", get_tok_str(tok, NULL)); }
		if (tok < TOK_IDENT)
			{ MCC_TRACE("br\n"); mcc_error("macro name must be an identifier"); }
		if (is_predef_macro(tok))
			{ MCC_TRACE("br\n"); mcc_warning("undefining %s", get_tok_str(tok, NULL)); }
		s = define_find(tok);
		if (s)
			{ MCC_TRACE("br\n"); define_undef(s); }
		next_nomacro();
		break;
	case TOK_INCLUDE_NEXT:
		/* T-mac-30217: gcc and clang warn (by default) when #include_next is
		 * used in the primary source file -- there is no including header whose
		 * search-path position it can continue after, so it just restarts from
		 * the top of the path. The primary source is the one whose parent is
		 * the command line (or none). */
		if (!file->prev || 0 == strcmp(file->prev->filename, "<command line>"))
			{ MCC_TRACE("br\n"); mcc_warning("'#include_next' in primary source file"); }
		/* falls through to the shared #include handling */
	case TOK_INCLUDE:
		parse_include(s1, tok - TOK_INCLUDE, 0, 0);
		goto the_end;
	case TOK_IMPORT:
		parse_include(s1, 0, 0, 1);
		goto the_end;
	case TOK_EMBED:
		embed_directive(s1);
		goto the_end;
	case TOK_ASSERT: { MCC_TRACE("br\n");
		int pred;
		char *answer;
		mcc_warning("'#assert' is a deprecated GCC extension");
		next_nomacro();
		if (tok < TOK_IDENT)
			{ MCC_TRACE("br\n"); mcc_error("predicate must be an identifier"); }
		pred = tok;
		next_nomacro();
		if (tok != '(')
			{ MCC_TRACE("br\n"); mcc_error("missing '(' after predicate"); }
		answer = pp_capture_parens_text();
		assertion_add(s1, pred, answer);
		next_nomacro();
		break;
	}
	case TOK_UNASSERT: { MCC_TRACE("br\n");
		int pred;
		char *answer = NULL;
		next_nomacro();
		if (tok < TOK_IDENT)
			{ MCC_TRACE("br\n"); mcc_error("predicate must be an identifier"); }
		pred = tok;
		next_nomacro();
		if (tok == '(') { MCC_TRACE("br\n");
			answer = pp_capture_parens_text();
			next_nomacro();
		}
		assertion_remove(s1, pred, answer);
		mcc_free(answer);
		break;
	}
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
		if (tok < TOK_IDENT || tok == TOK_DEFINED || tok == TOK___VA_ARGS__)
			{ MCC_TRACE("br\n"); mcc_error("invalid argument for '#if%sdef'", c ? "n" : ""); }
		if (is_bof) { MCC_TRACE("br\n");
			if (c) { MCC_TRACE("br\n");
				file->ifndef_macro = tok;
			}
		}
		if (define_find(tok) || tok == TOK___HAS_INCLUDE || tok == TOK___HAS_INCLUDE_NEXT ||
				tok == TOK___HAS_EMBED)
			{ MCC_TRACE("br\n"); c ^= 1; }
		next_nomacro();
	do_if:
		if (s1->ifdef_stack_ptr >= s1->ifdef_stack + IFDEF_STACK_SIZE)
			{ MCC_TRACE("br\n"); mcc_error("memory full (ifdef)"); }
		*s1->ifdef_stack_ptr++ = c;
		goto test_skip;
	case TOK_ELSE:
		next_nomacro();
		if (s1->ifdef_stack_ptr == s1->ifdef_stack)
			{ MCC_TRACE("br\n"); mcc_error("#else without matching #if"); }
		if (s1->ifdef_stack_ptr[-1] & 2)
			{ MCC_TRACE("br\n"); mcc_error("#else after #else"); }
		c = (s1->ifdef_stack_ptr[-1] ^= 3);
		goto test_else;
	case TOK_ELIF:
		if (s1->ifdef_stack_ptr == s1->ifdef_stack)
			{ MCC_TRACE("br\n"); mcc_error("#elif without matching #if"); }
		c = s1->ifdef_stack_ptr[-1];
		if (c > 1)
			{ MCC_TRACE("br\n"); mcc_error("#elif after #else"); }
		if (c == 1) { MCC_TRACE("br\n");
			skip_to_eol(0);
			c = 0;
		} else { MCC_TRACE("br\n");
			c = expr_preprocess(s1);
			s1->ifdef_stack_ptr[-1] = c;
		}
		goto test_else;
	case TOK_ELIFDEF:
	case TOK_ELIFNDEF:
		if (mcc_state->cversion < 202311 && mcc_state->warn_pedantic) { MCC_TRACE("br\n");
			if (mcc_state->pedantic_errors)
				{ MCC_TRACE("br\n"); mcc_error("'#%s' is a C23 feature", get_tok_str(tok, NULL)); }
			else
				{ MCC_TRACE("br\n"); mcc_warning("'#%s' is a C23 feature", get_tok_str(tok, NULL)); }
		}
		if (s1->ifdef_stack_ptr == s1->ifdef_stack)
			{ MCC_TRACE("br\n"); mcc_error("'#%s' without matching #if", get_tok_str(tok, NULL)); }
		c = s1->ifdef_stack_ptr[-1];
		if (c > 1)
			{ MCC_TRACE("br\n"); mcc_error("'#%s' after #else", get_tok_str(tok, NULL)); }
		if (c == 1) { MCC_TRACE("br\n");
			skip_to_eol(0);
			c = 0;
		} else { MCC_TRACE("br\n");
			int want_ndef = tok == TOK_ELIFNDEF;
			next_nomacro();
			if (tok < TOK_IDENT || tok == TOK_DEFINED || tok == TOK___VA_ARGS__)
				{ MCC_TRACE("br\n"); mcc_error("macro name must be an identifier"); }
			c = define_find(tok) || tok == TOK___HAS_INCLUDE ||
					tok == TOK___HAS_INCLUDE_NEXT || tok == TOK___HAS_EMBED;
			if (want_ndef)
				{ MCC_TRACE("br\n"); c ^= 1; }
			next_nomacro();
			s1->ifdef_stack_ptr[-1] = c;
		}
	test_else:
		if (s1->ifdef_stack_ptr == file->ifdef_stack_ptr + 1)
			{ MCC_TRACE("br\n"); file->ifndef_macro = 0; }
	test_skip:
		if (!(c & 1)) { MCC_TRACE("br\n");
			if (cst_pp_kind && cst_leafcount() > cst_pp_first)
				{ MCC_TRACE("br\n"); cst_hook_wrap(cst_pp_kind, cst_pp_first, cst_leafcount()); }
			skip_to_eol(1);
			preprocess_skip();
			is_bof = 0;
			goto redo;
		}
		break;
	case TOK_ENDIF:
		next_nomacro();
		if (s1->ifdef_stack_ptr <= file->ifdef_stack_ptr)
			{ MCC_TRACE("br\n"); mcc_error("#endif without matching #if"); }
		s1->ifdef_stack_ptr--;
		if (file->ifndef_macro &&
				s1->ifdef_stack_ptr == file->ifdef_stack_ptr) { MCC_TRACE("br\n");
			file->ifndef_macro_saved = file->ifndef_macro;
			file->ifndef_macro = 0;
			tok_flags |= TOK_FLAG_ENDIF;
		}
		break;

	case TOK_LINE:
		parse_flags &= ~PARSE_FLAG_TOK_NUM;
		next();
		if (tok != TOK_PPNUM) { MCC_TRACE("br\n");
		_line_err:
			mcc_error("wrong #line format");
		}
		c = 1;
		goto _line_num;
	case TOK_PPNUM:
		if (parse_flags & PARSE_FLAG_ASM_FILE)
			{ MCC_TRACE("br\n"); goto ignore; }
		c = 0;
	_line_num: {
		uint64_t nn = 0;
		int line_ovf = 0;
		for (q = tokc.str.data; *q; ++q) { MCC_TRACE("br\n");
			if (!isnum(*q))
				{ MCC_TRACE("br\n"); goto _line_err; }
			nn = nn * 10 + (*q - '0');
			if (nn > 2147483647)
				{ MCC_TRACE("br\n"); line_ovf = 1; }
		}
		if ((line_ovf || nn == 0) && mcc_state->warn_pedantic) { MCC_TRACE("br\n");
			if (mcc_state->pedantic_errors)
				{ MCC_TRACE("br\n"); mcc_error("line number out of range"); }
			else
				{ MCC_TRACE("br\n"); mcc_warning("line number out of range"); }
		}
		n = line_ovf ? 2147483647 : (int)nn;
	}
		parse_flags &= ~PARSE_FLAG_TOK_STR;
		next();
		if (tok != TOK_LINEFEED) { MCC_TRACE("br\n");
			if (tok != TOK_PPSTR || tokc.str.data[0] != '"')
				{ MCC_TRACE("br\n"); goto _line_err; }
			tokc.str.data[tokc.str.size - 2] = 0;
			mccpp_putfile(tokc.str.data + 1);
			next();
			skip_to_eol(c);
		}
		if (file->fd > 0)
			{ MCC_TRACE("br\n"); total_lines += file->line_num - n; }
		file->line_num = tok_line_num = n;
		tok_line_file = file;
		break;

	case TOK_ERROR:
	case TOK_WARNING: {
		CString ecstr;
		cstr_new(&ecstr);
		c = skip_spaces();
		while (c != '\n' && c != CH_EOF) { MCC_TRACE("br\n");
			cstr_ccat(&ecstr, c);
			c = ninp();
		}
		cstr_ccat(&ecstr, '\0');
		if (tok == TOK_ERROR)
			{ MCC_TRACE("br\n"); mcc_error("#error %s", (char *)ecstr.data); }
		else
			{ MCC_TRACE("br\n"); mcc_warning_c(warn_cpp)("#warning %s", (char *)ecstr.data); }
		cstr_free(&ecstr);
		next_nomacro();
		break;
	}
	case TOK_PRAGMA:
		if (!pragma_parse(s1))
			{ MCC_TRACE("br\n"); goto ignore; }
		break;
	case TOK_LINEFEED:
		goto the_end;
	default:
		if (saved_parse_flags & PARSE_FLAG_ASM_FILE)
			{ MCC_TRACE("br\n"); goto ignore; }
		if (tok == '!' && is_bof)
			{ MCC_TRACE("br\n"); goto ignore; }
		if (tok >= TOK_IDENT) { MCC_TRACE("br\n");
			const char *d = get_tok_str(tok, &tokc);
			if (!strcmp(d, "ident") || !strcmp(d, "sccs"))
				{ MCC_TRACE("br\n"); goto ignore; }
		}
		mcc_error("invalid preprocessing directive #%s", get_tok_str(tok, &tokc));
	ignore:
		skip_to_eol(0);
		goto the_end;
	}
	skip_to_eol(1);
the_end:
	if (cst_pp_kind && cst_leafcount() > cst_pp_first)
		{ MCC_TRACE("br\n"); cst_hook_wrap(cst_pp_kind, cst_pp_first, cst_leafcount()); }
	parse_flags = saved_parse_flags;
	pp_directive_depth--;
}

static void parse_escape_string(CString *outstr, const uint8_t *buf, int is_long, int prefix, int is_char) { MCC_TRACE("enter\n");
	int c, n, i, is_ucn;
	const uint8_t *p;

	p = buf;
	for (;;) { MCC_TRACE("br\n");
		c = *p;
		if (c == '\0')
			{ MCC_TRACE("br\n"); break; }
		if (c == '\\') { MCC_TRACE("br\n");
			p++;
			c = *p;
			switch (c) { MCC_TRACE("br\n");
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				n = c - '0';
				p++;
				c = *p;
				if (isoct(c)) { MCC_TRACE("br\n");
					n = n * 8 + c - '0';
					p++;
					c = *p;
					if (isoct(c)) { MCC_TRACE("br\n");
						n = n * 8 + c - '0';
						p++;
					}
				}
				if (!is_long && n > 0xFF)
					{ MCC_TRACE("br\n"); mcc_warning("octal escape sequence out of range"); }
				c = n;
				goto add_char_nonext;
			case 'x':
				i = 0;
				goto parse_hex_or_ucn;
			case 'u':
				i = 4;
				goto parse_hex_or_ucn;
			case 'U':
				i = 8;
				goto parse_hex_or_ucn;
			parse_hex_or_ucn:
				is_ucn = (i != 0);
				p++;
				n = 0;
				do { MCC_TRACE("br\n");
					c = *p;
					if (c >= 'a' && c <= 'f')
						{ MCC_TRACE("br\n"); c = c - 'a' + 10; }
					else if (c >= 'A' && c <= 'F')
						{ MCC_TRACE("br\n"); c = c - 'A' + 10; }
					else if (isnum(c))
						{ MCC_TRACE("br\n"); c = c - '0'; }
					else if (i >= 0) { MCC_TRACE("br\n");
						/* T-mac-30148(4): `\x` is a hex escape, not a universal
						 * character name -- give gcc/clang's specific message
						 * instead of the misleading UCN wording. `\u`/`\U` (is_ucn)
						 * keep the UCN message. */
						if (is_ucn)
							{ MCC_TRACE("br\n"); expect("more hex digits in universal-character-name"); }
						else
							{ MCC_TRACE("br\n"); mcc_error("'\\x' used with no following hex digits"); }
					}
					else { MCC_TRACE("br\n");
						if (!is_long && n > 0xFF)
							{ MCC_TRACE("br\n"); mcc_warning("hex escape sequence out of range"); }
						goto add_hex_or_ucn;
					}
					n = (unsigned)n * 16 + c;
					p++;
				} while (--i);
				/* T-mac-30146: a surrogate (D800..DFFF) or out-of-range (>10FFFF)
				 * code point is never a valid universal character, in ANY language
				 * mode and for wide as well as narrow literals — the narrow/u8 path
				 * catches it in unicode_to_utf8, but the wide (is_long) path stores
				 * the raw code unit, so reject it here for both. */
				if (is_ucn && (((unsigned)n >= 0xD800 && (unsigned)n <= 0xDFFF) || (unsigned)n > 0x10FFFF))
					{ MCC_TRACE("br\n"); mcc_error("\\U%08x is not a valid universal character", (unsigned)n); }
				if (is_ucn && mcc_state->warn_pedantic && mcc_state->cversion < 202311 && ((n < 0xA0 && n != 0x24 && n != 0x40 && n != 0x60) || (n >= 0xD800 && n <= 0xDFFF))) { MCC_TRACE("br\n");
					if (mcc_state->pedantic_errors)
						{ MCC_TRACE("br\n"); mcc_error("\\u%04x is not a valid universal character", n); }
					else
						{ MCC_TRACE("br\n"); mcc_warning("\\u%04x is not a valid universal character", n); }
				}
				if (is_long) { MCC_TRACE("br\n");
				add_hex_or_ucn:
					if (!is_ucn && prefix == 'u' && (unsigned)n > 0xFFFF) { MCC_TRACE("br\n");
						mcc_warning("hex escape sequence out of range");
						n &= 0xFFFF;
					} else if (is_ucn && is_char && prefix == 'u' && (unsigned)n > 0xFFFF) { MCC_TRACE("br\n");
						mcc_warning("character not encodable in a single code unit");
					}
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
			case 'E':
				if (!gnu_ext)
					{ MCC_TRACE("br\n"); goto invalid_escape; }
				if (mcc_state->warn_pedantic) { MCC_TRACE("br\n");
					if (mcc_state->pedantic_errors)
						{ MCC_TRACE("br\n"); mcc_error("\\%c is a non-ISO escape sequence", c); }
					else
						{ MCC_TRACE("br\n"); mcc_warning("\\%c is a non-ISO escape sequence", c); }
				}
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
					{ MCC_TRACE("br\n"); mcc_warning("unknown escape sequence: \'\\%c\'", c); }
				else
					{ MCC_TRACE("br\n"); mcc_warning("unknown escape sequence: \'\\x%x\'", c); }
				break;
			}
		} else if (is_long && c >= 0x80) { MCC_TRACE("br\n");
			int cont;
			int skip;
			int i;

			if (c < 0xC2) { MCC_TRACE("br\n");
				skip = 1;
				goto invalid_utf8_sequence;
			} else if (c <= 0xDF) { MCC_TRACE("br\n");
				cont = 1;
				n = c & 0x1f;
			} else if (c <= 0xEF) { MCC_TRACE("br\n");
				cont = 2;
				n = c & 0xf;
			} else if (c <= 0xF4) { MCC_TRACE("br\n");
				cont = 3;
				n = c & 0x7;
			} else { MCC_TRACE("br\n");
				skip = 1;
				goto invalid_utf8_sequence;
			}

			for (i = 1; i <= cont; i++) { MCC_TRACE("br\n");
				int l = 0x80, h = 0xBF;

				if (i == 1) { MCC_TRACE("br\n");
					switch (c) { MCC_TRACE("br\n");
					case 0xE0:
						l = 0xA0;
						break;
					case 0xED:
						h = 0x9F;
						break;
					case 0xF0:
						l = 0x90;
						break;
					case 0xF4:
						h = 0x8F;
						break;
					}
				}

				if (p[i] < l || p[i] > h) { MCC_TRACE("br\n");
					skip = i;
					goto invalid_utf8_sequence;
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
			{ MCC_TRACE("br\n"); cstr_ccat(outstr, c); }
		else { MCC_TRACE("br\n");
#ifdef MCC_TARGET_PE
			if (prefix == 'U') { MCC_TRACE("br\n");
				/* char32_t is 32-bit on every target, PE included: store the
				 * full code point as one 4-byte unit with no UTF-16 surrogate
				 * split, so a hex-escape surrogate value stays independent and
				 * an astral code point needs no recombine downstream. Storing
				 * it as a 16-bit surrogate pair (as u"" requires) is lossy: a
				 * split astral char and two separate hex-escape units become
				 * indistinguishable (T-mac-30244 PE path). */
				cstr_ccat(outstr, c & 0xff);
				cstr_ccat(outstr, (c >> 8) & 0xff);
				cstr_ccat(outstr, (c >> 16) & 0xff);
				cstr_ccat(outstr, (c >> 24) & 0xff);
			} else if (c < 0x10000) { MCC_TRACE("br\n");
				cstr_wccat(outstr, c);
			} else { MCC_TRACE("br\n");
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
		{ MCC_TRACE("br\n"); cstr_ccat(outstr, '\0'); }
#ifdef MCC_TARGET_PE
	else if (prefix == 'U') { MCC_TRACE("br\n");
		/* 4-byte char32_t NUL to match the 32-bit units stored above */
		cstr_ccat(outstr, 0);
		cstr_ccat(outstr, 0);
		cstr_ccat(outstr, 0);
		cstr_ccat(outstr, 0);
	}
#endif
	else
		{ MCC_TRACE("br\n"); cstr_wccat(outstr, '\0'); }
}

static void parse_string(const char *s, int len) { MCC_TRACE("enter\n");
	uint8_t buf[1000], *p = buf;
	int is_long, sep, prefix = 0;

	if (*s == 'L' || *s == 'u' || *s == 'U' || *s == '8')
		{ MCC_TRACE("br\n"); prefix = *s++, --len; }
	if (prefix == 'u' && *s == '8')
		{ MCC_TRACE("br\n"); prefix = '8', ++s, --len; }
	is_long = (prefix == 'L' || prefix == 'u' || prefix == 'U');
	sep = *s++;
	len -= 2;
	if (len >= sizeof buf)
		{ MCC_TRACE("br\n"); p = mcc_malloc(len + 1); }
	memcpy(p, s, len);
	p[len] = 0;

	cstr_reset(&tokcstr);
	parse_escape_string(&tokcstr, p, is_long, prefix, sep == '\'');
	if (p != buf)
		{ MCC_TRACE("br\n"); mcc_free(p); }

	if (sep == '\'') { MCC_TRACE("br\n");
		int char_size, i, n, c;
		if (!is_long)
			{ MCC_TRACE("br\n"); tok = TOK_CCHAR, char_size = 1; }
		else
			{ MCC_TRACE("br\n"); tok = TOK_LCHAR, char_size = sizeof(nwchar_t); }
		if (prefix == 'u')
			{ MCC_TRACE("br\n"); tok = TOK_U16CHAR; }
		else if (prefix == 'U') { MCC_TRACE("br\n"); tok = TOK_U32CHAR;
#ifdef MCC_TARGET_PE
			char_size = 4;   /* char32_t units are 32-bit on PE too */
#endif
		}
		else if (prefix == '8')
			{ MCC_TRACE("br\n"); tok = TOK_U8CHAR; }
		n = tokcstr.size / char_size - 1;
		if (n < 1)
			{ MCC_TRACE("br\n"); mcc_error("empty character constant"); }
		if (prefix == '8' && n > 1)
			{ MCC_TRACE("br\n"); mcc_error("u8 character constant must contain a single UTF-8 code unit"); }
		if (prefix == 'U') { MCC_TRACE("br\n");
			int nchars = 0;
			for (c = i = 0; i < n; ++i) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_PE
				/* char32_t units are stored full-width (32-bit) on PE, so read them
				 * directly with no UTF-16 surrogate recombine: a hex-escape surrogate
				 * value is an independent unit (T-mac-30244 PE path). */
				c = (int)((uint32_t *)tokcstr.data)[i];
				nchars++;
#else
				unsigned int u = (unsigned int)((nwchar_t *)tokcstr.data)[i];
				if (u >= 0xD800 && u <= 0xDBFF && i + 1 < n) { MCC_TRACE("br\n");
					unsigned int lo = (unsigned int)((nwchar_t *)tokcstr.data)[i + 1] & 0xFFFFu;
					if (lo >= 0xDC00 && lo <= 0xDFFF) { MCC_TRACE("br\n");
						u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
						i++;
					}
				}
				c = (int)u;
				nchars++;
#endif
			}
			if (nchars > 1)
				{ MCC_TRACE("br\n"); mcc_warning("multi-character character constant"); }
		} else { MCC_TRACE("br\n");
			if (n > 1)
				{ MCC_TRACE("br\n"); mcc_warning("multi-character character constant"); }
			for (c = i = 0; i < n; ++i) { MCC_TRACE("br\n");
				if (is_long)
					{ MCC_TRACE("br\n"); c = ((nwchar_t *)tokcstr.data)[i]; }
				else if (n == 1)
					{ MCC_TRACE("br\n"); c = (mcc_state->char_is_unsigned || prefix == '8') ? ((unsigned char *)tokcstr.data)[i] : ((char *)tokcstr.data)[i]; }
				else
					{ MCC_TRACE("br\n"); c = (c << 8) | ((unsigned char *)tokcstr.data)[i]; }
			}
			if (prefix == 'u')
				{ MCC_TRACE("br\n"); c &= 0xFFFF; }
		}
		tokc.i = c;
	} else if (prefix == 'u') { MCC_TRACE("br\n");
		int i, ncp = tokcstr.size / sizeof(nwchar_t);
		nwchar_t *cps = mcc_malloc((ncp ? ncp : 1) * sizeof(nwchar_t));
		memcpy(cps, tokcstr.data, ncp * sizeof(nwchar_t));
		cstr_reset(&tokcstr);
		for (i = 0; i < ncp; i++) { MCC_TRACE("br\n");
			unsigned int cp = (unsigned int)cps[i];
			if (cp < 0x10000) { MCC_TRACE("br\n");
				cstr_ccat(&tokcstr, cp & 0xff);
				cstr_ccat(&tokcstr, (cp >> 8) & 0xff);
			} else { MCC_TRACE("br\n");
				unsigned int hi, lo;
				cp -= 0x10000;
				hi = 0xD800 + (cp >> 10);
				lo = 0xDC00 + (cp & 0x3FF);
				cstr_ccat(&tokcstr, hi & 0xff);
				cstr_ccat(&tokcstr, (hi >> 8) & 0xff);
				cstr_ccat(&tokcstr, lo & 0xff);
				cstr_ccat(&tokcstr, (lo >> 8) & 0xff);
			}
		}
		mcc_free(cps);
		tokc.str.size = tokcstr.size;
		tokc.str.data = tokcstr.data;
		tok = TOK_U16STR;
	} else if (prefix == 'U') { MCC_TRACE("br\n");
#ifdef MCC_TARGET_PE
		/* char32_t units are stored full-width (32-bit) on PE (T-mac-30244 PE
		 * path): copy each 4-byte unit out as one code point, with no UTF-16
		 * surrogate recombine, so a hex-escape surrogate stays independent. */
		int i, ncp = tokcstr.size / 4;
		uint32_t *cps = mcc_malloc((ncp ? ncp : 1) * 4);
		memcpy(cps, tokcstr.data, ncp * 4);
		cstr_reset(&tokcstr);
		for (i = 0; i < ncp; i++) { MCC_TRACE("br\n");
			unsigned int cp = cps[i];
			cstr_ccat(&tokcstr, cp & 0xff);
			cstr_ccat(&tokcstr, (cp >> 8) & 0xff);
			cstr_ccat(&tokcstr, (cp >> 16) & 0xff);
			cstr_ccat(&tokcstr, (cp >> 24) & 0xff);
		}
		mcc_free(cps);
#else
		int i, ncp = tokcstr.size / sizeof(nwchar_t);
		nwchar_t *cps = mcc_malloc((ncp ? ncp : 1) * sizeof(nwchar_t));
		memcpy(cps, tokcstr.data, ncp * sizeof(nwchar_t));
		cstr_reset(&tokcstr);
		for (i = 0; i < ncp; i++) { MCC_TRACE("br\n");
			unsigned int cp = (unsigned int)cps[i] & 0xFFFFFFFFu;
			cstr_ccat(&tokcstr, cp & 0xff);
			cstr_ccat(&tokcstr, (cp >> 8) & 0xff);
			cstr_ccat(&tokcstr, (cp >> 16) & 0xff);
			cstr_ccat(&tokcstr, (cp >> 24) & 0xff);
		}
		mcc_free(cps);
#endif
		tokc.str.size = tokcstr.size;
		tokc.str.data = tokcstr.data;
		tok = TOK_U32STR;
	} else { MCC_TRACE("br\n");
		tokc.str.size = tokcstr.size;
		tokc.str.data = tokcstr.data;
		if (prefix == '8')
			{ MCC_TRACE("br\n"); tok = TOK_U8STR; }
		else if (!is_long)
			{ MCC_TRACE("br\n"); tok = TOK_STR; }
		else
			{ MCC_TRACE("br\n"); tok = TOK_LSTR; }
	}
}

#define BN_SIZE 4

static int bn_lshift(unsigned int *bn, int shift, int or_val) { MCC_TRACE("enter\n");
	unsigned int v;
	if (bn[BN_SIZE - 1] >> (32 - shift))
		{ MCC_TRACE("br\n"); return shift; }
	for (int i = 0; i < BN_SIZE; i++) { MCC_TRACE("br\n");
		v = bn[i];
		bn[i] = (v << shift) | or_val;
		or_val = v >> (32 - shift);
	}
	return 0;
}

static void bn_zero(unsigned int *bn) { MCC_TRACE("enter\n");
	for (int i = 0; i < BN_SIZE; i++) { MCC_TRACE("br\n");
		bn[i] = 0;
	}
}

static void pp_c11_prefix_pedantic(const char *what) { MCC_TRACE("enter\n");
	if (mcc_state->cversion >= 201112 || !mcc_state->warn_pedantic)
		{ MCC_TRACE("br\n"); return; }
	if (mcc_state->pedantic_errors)
		{ MCC_TRACE("br\n"); mcc_error("%s is a C11 feature", what); }
	else
		{ MCC_TRACE("br\n"); mcc_warning("%s is a C11 feature", what); }
}

static void parse_number(const char *p) { MCC_TRACE("enter\n");
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
	if (t == '.') { MCC_TRACE("br\n");
		goto float_frac_parse;
	} else if (t == '0') { MCC_TRACE("br\n");
		if (ch == 'x' || ch == 'X') { MCC_TRACE("br\n");
			q--;
			ch = *p++;
			b = 16;
		} else if (mcc_state->mcc_ext && (ch == 'b' || ch == 'B')) { MCC_TRACE("br\n");
			if (mcc_state->cversion < 202311 && mcc_state->warn_pedantic) { MCC_TRACE("br\n");
				if (mcc_state->pedantic_errors)
					{ MCC_TRACE("br\n"); mcc_error("binary integer constants are a C23/GNU extension"); }
				else
					{ MCC_TRACE("br\n"); mcc_warning("binary integer constants are a C23/GNU extension"); }
			}
			q--;
			ch = *p++;
			b = 2;
		} else if (mcc_state->mcc_ext && (ch == 'o' || ch == 'O')) { MCC_TRACE("br\n");
			if (mcc_state->cversion < 202400 && mcc_state->warn_pedantic &&
					!(file && file->system_header)) { MCC_TRACE("br\n");
				if (mcc_state->pedantic_errors)
					{ MCC_TRACE("br\n"); mcc_error("'0o' prefixed constants are a C2Y feature or GCC extension"); }
				else
					{ MCC_TRACE("br\n"); mcc_warning("'0o' prefixed constants are a C2Y feature or GCC extension"); }
			}
			q--;
			ch = *p++;
			b = 8;
		}
	}
	char *radix_digits_start = q;
	while (1) { MCC_TRACE("br\n");
		if (ch >= 'a' && ch <= 'f')
			{ MCC_TRACE("br\n"); t = ch - 'a' + 10; }
		else if (ch >= 'A' && ch <= 'F')
			{ MCC_TRACE("br\n"); t = ch - 'A' + 10; }
		else if (isnum(ch))
			{ MCC_TRACE("br\n"); t = ch - '0'; }
		else
			{ MCC_TRACE("br\n"); break; }
		if (t >= b)
			{ MCC_TRACE("br\n"); break; }
		if (q >= token_buf + STRING_MAX_SIZE) { MCC_TRACE("br\n");
		num_too_long:
			mcc_error("number too long");
		}
		*q++ = ch;
		ch = *p++;
	}
	if (b != 10 && q == radix_digits_start && ch != '.' && ch != 'p' && ch != 'P') { MCC_TRACE("br\n");
		mcc_error("invalid suffix '%c' on integer constant",
							b == 16 ? 'x' : b == 2 ? 'b' : 'o');
	}
	if (ch == '.' ||
			((ch == 'e' || ch == 'E') && b == 10) ||
			((ch == 'p' || ch == 'P') && (b == 16 || b == 2))) { MCC_TRACE("br\n");
		if (b == 16 && mcc_state->cversion < 199901 && mcc_state->warn_pedantic) { MCC_TRACE("br\n");
			if (mcc_state->pedantic_errors)
				{ MCC_TRACE("br\n"); mcc_error("hexadecimal floating constants are a C99 feature"); }
			else
				{ MCC_TRACE("br\n"); mcc_warning("hexadecimal floating constants are a C99 feature"); }
		}
		if (b != 10) { MCC_TRACE("br\n");
			frac_bits = 0;
			*q = '\0';
			if (b == 16)
				{ MCC_TRACE("br\n"); shift = 4; }
			else
				{ MCC_TRACE("br\n"); shift = 1; }
			bn_zero(bn);
			q = token_buf;
			while (1) { MCC_TRACE("br\n");
				t = *q++;
				if (t == '\0') { MCC_TRACE("br\n");
					break;
				} else if (t >= 'a') { MCC_TRACE("br\n");
					t = t - 'a' + 10;
				} else if (t >= 'A') { MCC_TRACE("br\n");
					t = t - 'A' + 10;
				} else { MCC_TRACE("br\n");
					t = t - '0';
				}
				frac_bits -= bn_lshift(bn, shift, t);
			}
			if (ch == '.') { MCC_TRACE("br\n");
				ch = *p++;
				while (1) { MCC_TRACE("br\n");
					t = ch;
					if (t >= 'a' && t <= 'f') { MCC_TRACE("br\n");
						t = t - 'a' + 10;
					} else if (t >= 'A' && t <= 'F') { MCC_TRACE("br\n");
						t = t - 'A' + 10;
					} else if (t >= '0' && t <= '9') { MCC_TRACE("br\n");
						t = t - '0';
					} else { MCC_TRACE("br\n");
						break;
					}
					if (t >= b)
						{ MCC_TRACE("br\n"); mcc_error("invalid digit"); }
					frac_bits -= bn_lshift(bn, shift, t);
					frac_bits += shift;
					ch = *p++;
				}
			}
			if (ch != 'p' && ch != 'P')
				{ MCC_TRACE("br\n"); expect("exponent"); }
			ch = *p++;
			s = 1;
			exp_val = 0;
			if (ch == '+') { MCC_TRACE("br\n");
				ch = *p++;
			} else if (ch == '-') { MCC_TRACE("br\n");
				s = -1;
				ch = *p++;
			}
			if (ch < '0' || ch > '9')
				{ MCC_TRACE("br\n"); expect("exponent digits"); }
			while (ch >= '0' && ch <= '9') { MCC_TRACE("br\n");
				if (exp_val < 100000000)
					{ MCC_TRACE("br\n"); exp_val = exp_val * 10 + ch - '0'; }
				ch = *p++;
			}
			exp_val = exp_val * s;

			d = (long double)bn[3] * 79228162514264337593543950336.0L +
					(long double)bn[2] * 18446744073709551616.0L +
					(long double)bn[1] * 4294967296.0L +
					(long double)bn[0];
			d = ldexpl(d, exp_val - frac_bits);
			if (ch == 'i' || ch == 'I' || ch == 'j' || ch == 'J') { MCC_TRACE("br\n");
				tok_imaginary = 1;
				ch = *p++;
			}
			t = toup(ch);
			if (t == 'F' && p[0] == '1' && p[1] == '6') { MCC_TRACE("br\n");
				p += 2;
				ch = *p++;
				tok = TOK_CFLOAT16;
				tokc.i = f32_to_f16_bits(f16_round(d));
#ifdef MCC_HAVE_FLOAT128
			} else if (t == 'F' && p[0] == '1' && p[1] == '2' && p[2] == '8') { MCC_TRACE("br\n");
				p += 3;
				ch = *p++;
				tok = TOK_CFLOAT128;
				tokc.d = (double)d;
			} else if (t == 'Q') { MCC_TRACE("br\n");
				ch = *p++;
				tok = TOK_CFLOAT128;
				tokc.d = (double)d;
#endif
			} else if (t == 'F' && p[0] == '3' && p[1] == '2') { MCC_TRACE("br\n");
				p += 2;
				ch = *p++;
				tok = TOK_CFLOAT;
				tokc.f = (float)d;
			} else if (t == 'F' && p[0] == '6' && p[1] == '4') { MCC_TRACE("br\n");
				p += 2;
				ch = *p++;
				tok = TOK_CDOUBLE;
				tokc.d = (double)d;
			} else if (t == 'F') { MCC_TRACE("br\n");
				ch = *p++;
				tok = TOK_CFLOAT;
				tokc.f = (float)d;
			} else if (t == 'L') { MCC_TRACE("br\n");
				ch = *p++;
				tok = TOK_CLDOUBLE;
				tokc.ld = d;
			} else { MCC_TRACE("br\n");
				tok = TOK_CDOUBLE;
				tokc.d = (double)d;
			}
		} else { MCC_TRACE("br\n");
			if (ch == '.') { MCC_TRACE("br\n");
				if (q >= token_buf + STRING_MAX_SIZE)
					{ MCC_TRACE("br\n"); goto num_too_long; }
				*q++ = ch;
				ch = *p++;
			float_frac_parse:
				while (ch >= '0' && ch <= '9') { MCC_TRACE("br\n");
					if (q >= token_buf + STRING_MAX_SIZE)
						{ MCC_TRACE("br\n"); goto num_too_long; }
					*q++ = ch;
					ch = *p++;
				}
			}
			if (ch == 'e' || ch == 'E') { MCC_TRACE("br\n");
				if (q >= token_buf + STRING_MAX_SIZE)
					{ MCC_TRACE("br\n"); goto num_too_long; }
				*q++ = ch;
				ch = *p++;
				if (ch == '-' || ch == '+') { MCC_TRACE("br\n");
					if (q >= token_buf + STRING_MAX_SIZE)
						{ MCC_TRACE("br\n"); goto num_too_long; }
					*q++ = ch;
					ch = *p++;
				}
				if (ch < '0' || ch > '9')
					{ MCC_TRACE("br\n"); expect("exponent digits"); }
				while (ch >= '0' && ch <= '9') { MCC_TRACE("br\n");
					if (q >= token_buf + STRING_MAX_SIZE)
						{ MCC_TRACE("br\n"); goto num_too_long; }
					*q++ = ch;
					ch = *p++;
				}
			}
			*q = '\0';
			if (ch == 'i' || ch == 'I' || ch == 'j' || ch == 'J') { MCC_TRACE("br\n");
				tok_imaginary = 1;
				ch = *p++;
			}
			t = toup(ch);
			errno = 0;
			if (t == 'F' && p[0] == '1' && p[1] == '6') { MCC_TRACE("br\n");
				p += 2;
				ch = *p++;
				tok = TOK_CFLOAT16;
				tokc.i = f32_to_f16_bits(f16_round(strtold(token_buf, NULL)));
#ifdef MCC_HAVE_FLOAT128
			} else if (t == 'F' && p[0] == '1' && p[1] == '2' && p[2] == '8') { MCC_TRACE("br\n");
				p += 3;
				ch = *p++;
				tok = TOK_CFLOAT128;
				tokc.d = strtod(token_buf, NULL);
			} else if (t == 'Q') { MCC_TRACE("br\n");
				ch = *p++;
				tok = TOK_CFLOAT128;
				tokc.d = strtod(token_buf, NULL);
#endif
			} else if (t == 'F' && p[0] == '3' && p[1] == '2') { MCC_TRACE("br\n");
				p += 2;
				ch = *p++;
				tok = TOK_CFLOAT;
				tokc.f = strtof(token_buf, NULL);
			} else if (t == 'F' && p[0] == '6' && p[1] == '4') { MCC_TRACE("br\n");
				p += 2;
				ch = *p++;
				tok = TOK_CDOUBLE;
				tokc.d = strtod(token_buf, NULL);
			} else if (t == 'F') { MCC_TRACE("br\n");
				ch = *p++;
				tok = TOK_CFLOAT;
				tokc.f = strtof(token_buf, NULL);
			} else if (t == 'L') { MCC_TRACE("br\n");
				ch = *p++;
				tok = TOK_CLDOUBLE;
				tokc.ld = strtold(token_buf, NULL);
			} else { MCC_TRACE("br\n");
				tok = TOK_CDOUBLE;
				tokc.d = strtod(token_buf, NULL);
			}
		}
	} else { MCC_TRACE("br\n");
		unsigned long long n, n1;
		int lcount, ucount, l0, ov = 0, i256sfx = 0, wbsfx = 0;
		const char *p1;

		*q = '\0';
		q = token_buf;
		if (b == 10 && *q == '0') { MCC_TRACE("br\n");
			b = 8;
			q++;
		}
		n = 0;
		while (1) { MCC_TRACE("br\n");
			t = *q++;
			if (t == '\0')
				{ MCC_TRACE("br\n"); break; }
			else if (t >= 'a')
				{ MCC_TRACE("br\n"); t = t - 'a' + 10; }
			else if (t >= 'A')
				{ MCC_TRACE("br\n"); t = t - 'A' + 10; }
			else
				{ MCC_TRACE("br\n"); t = t - '0'; }
			if (t >= b)
				{ MCC_TRACE("br\n"); mcc_error("invalid digit"); }
			n1 = n;
			n = n * b + t;
			if (n1 >= 0x1000000000000000ULL && n / b != n1)
				{ MCC_TRACE("br\n"); ov = 1; }
		}

		lcount = ucount = 0;
		l0 = 0;
		p1 = p;
		for (;;) { MCC_TRACE("br\n");
			t = toup(ch);
			if (t == 'L') { MCC_TRACE("br\n");
				if (lcount >= 2)
					{ MCC_TRACE("br\n"); mcc_error("three 'l's in integer constant"); }
				if (lcount == 1 && ch != l0)
					{ MCC_TRACE("br\n"); mcc_error("incorrect integer suffix: %s", p1); }
				if (lcount == 0)
					{ MCC_TRACE("br\n"); l0 = ch; }
				lcount++;
				if (lcount == 2 && mcc_state->cversion < 199901 &&
						mcc_state->warn_pedantic) { MCC_TRACE("br\n");
					if (mcc_state->pedantic_errors)
						{ MCC_TRACE("br\n"); mcc_error("ISO C90 does not support 'long long'"); }
					else
						{ MCC_TRACE("br\n"); mcc_warning("ISO C90 does not support 'long long'"); }
				}
				ch = *p++;
			} else if (t == 'U') { MCC_TRACE("br\n");
				if (ucount >= 1)
					{ MCC_TRACE("br\n"); mcc_error("two 'u's in integer constant"); }
				ucount++;
				ch = *p++;
			} else if (t == 'I' && p[0] == '2' && p[1] == '5' && p[2] == '6' &&
								 (p[3] == 'u' || p[3] == 'U' || p[3] == 'l' || p[3] == 'L' ||
									!((p[3] >= '0' && p[3] <= '9') || (p[3] >= 'a' && p[3] <= 'z') ||
										(p[3] >= 'A' && p[3] <= 'Z') || p[3] == '_'))) { MCC_TRACE("br\n");
				/* the MSVC-family __int256 literal suffix: [u|U]i256 / i256[u|U]; must be
				   sniffed before the bare `i` imaginary suffix eats the 'i' — a trailing
				   suffix char (u/l) continues the suffix, any other ident char is not i256 */
				if (i256sfx)
					{ MCC_TRACE("br\n"); mcc_error("incorrect integer suffix: %s", p1); }
				i256sfx = 1;
				p += 3;
				ch = *p++;
			} else if (t == 'W' && (p[0] == 'b' || p[0] == 'B') &&
								 (p[1] == 'u' || p[1] == 'U' ||
									!((p[1] >= '0' && p[1] <= '9') || (p[1] >= 'a' && p[1] <= 'z') ||
										(p[1] >= 'A' && p[1] <= 'Z') || p[1] == '_'))) { MCC_TRACE("br\n");
				/* C23 6.4.4.1 bit-precise integer suffix wb/uwb (u in either order);
				   sniffed like i256 so a trailing ident char is not mistaken for it */
				if (wbsfx)
					{ MCC_TRACE("br\n"); mcc_error("incorrect integer suffix: %s", p1); }
				wbsfx = 1;
				p += 1;
				ch = *p++;
			} else if (t == 'I' || t == 'J') { MCC_TRACE("br\n");
				tok_imaginary = 1;
				ch = *p++;
			} else { MCC_TRACE("br\n");
				break;
			}
		}

		if (wbsfx) { MCC_TRACE("br\n");
			uint32_t w[16];
			uint64_t cur, carry;
			int i, bw, N;
			if (lcount || tok_imaginary || i256sfx)
				{ MCC_TRACE("br\n"); mcc_error("incorrect integer suffix: %s", p1); }
			if (pp_expr)
				{ MCC_TRACE("br\n"); mcc_error("'wb' constant in preprocessor expression"); }
			ov = 0;
			memset(w, 0, sizeof w);
			for (q = token_buf; (t = *q++) != '\0';) { MCC_TRACE("br\n");
				if (t >= 'a')
					{ MCC_TRACE("br\n"); t = t - 'a' + 10; }
				else if (t >= 'A')
					{ MCC_TRACE("br\n"); t = t - 'A' + 10; }
				else
					{ MCC_TRACE("br\n"); t = t - '0'; }
				carry = t;
				for (i = 0; i < 16; i++) { MCC_TRACE("br\n");
					cur = (uint64_t)w[i] * b + carry;
					w[i] = (uint32_t)cur;
					carry = cur >> 32;
				}
				if (carry)
					{ MCC_TRACE("br\n"); ov = 1; }
			}
			bw = 0;
			for (i = 15; i >= 0; i--) { MCC_TRACE("br\n");
				if (w[i]) { MCC_TRACE("br\n");
					uint32_t v = w[i];
					bw = i * 32;
					while (v) { MCC_TRACE("br\n"); bw++; v >>= 1; }
					break;
				}
			}
			if (ucount)
				{ MCC_TRACE("br\n"); N = bw < 1 ? 1 : bw; }
			else
				{ MCC_TRACE("br\n"); N = (bw + 1) < 2 ? 2 : bw + 1; }
			if (ov || N > 512)
				{ MCC_TRACE("br\n"); mcc_error("'%swb' integer constant exceeds the %d-bit "
									"_BitInt maximum this target supports", ucount ? "u" : "", 512); }
			tok = ucount ? TOK_CUBITINT : TOK_CBITINT;
			tok_bitint_width = N;
			tokc.q.lo = w[0] | ((uint64_t)w[1] << 32);
			tokc.q.hi = w[2] | ((uint64_t)w[3] << 32);
			tokc.q.w2 = w[4] | ((uint64_t)w[5] << 32);
			tokc.q.w3 = w[6] | ((uint64_t)w[7] << 32);
			tokc.q.w4 = w[8] | ((uint64_t)w[9] << 32);
			tokc.q.w5 = w[10] | ((uint64_t)w[11] << 32);
			tokc.q.w6 = w[12] | ((uint64_t)w[13] << 32);
			tokc.q.w7 = w[14] | ((uint64_t)w[15] << 32);
			goto int_suffix_done;
		}

		if (i256sfx) { MCC_TRACE("br\n");
			/* re-accumulate the digit string (still NUL-terminated in token_buf; any
			   leading octal '0' just contributes zero) into 8x32-bit words — the host
			   may lack a 128-bit type, so the mul-add carries through 64-bit halves */
			uint32_t w[8];
			uint64_t cur, carry;
			int i;
			if (lcount || tok_imaginary)
				{ MCC_TRACE("br\n"); mcc_error("incorrect integer suffix: %s", p1); }
			if (pp_expr)
				{ MCC_TRACE("br\n"); mcc_error("'i256' constant in preprocessor expression"); }
			ov = 0;
			memset(w, 0, sizeof w);
			for (q = token_buf; (t = *q++) != '\0';) { MCC_TRACE("br\n");
				if (t >= 'a')
					{ MCC_TRACE("br\n"); t = t - 'a' + 10; }
				else if (t >= 'A')
					{ MCC_TRACE("br\n"); t = t - 'A' + 10; }
				else
					{ MCC_TRACE("br\n"); t = t - '0'; }
				carry = t;
				for (i = 0; i < 8; i++) { MCC_TRACE("br\n");
					cur = (uint64_t)w[i] * b + carry;
					w[i] = (uint32_t)cur;
					carry = cur >> 32;
				}
				if (carry)
					{ MCC_TRACE("br\n"); ov = 1; }
			}
			if (ov)
				{ MCC_TRACE("br\n"); mcc_warning("integer constant overflow"); }
			tok = ucount ? TOK_CUINT256 : TOK_CINT256;
			tokc.q.lo = w[0] | ((uint64_t)w[1] << 32);
			tokc.q.hi = w[2] | ((uint64_t)w[3] << 32);
			tokc.q.w2 = w[4] | ((uint64_t)w[5] << 32);
			tokc.q.w3 = w[6] | ((uint64_t)w[7] << 32);
			goto int_suffix_done;
		}

		if (pp_expr)
			{ MCC_TRACE("br\n"); lcount = 2; }

		if (ucount == 0 && b == 10) { MCC_TRACE("br\n");
			if (lcount <= (LONG_SIZE == 4)) { MCC_TRACE("br\n");
				if (n >= 0x80000000U)
					{ MCC_TRACE("br\n"); lcount = (LONG_SIZE == 4) + 1; }
			}
			if (n >= 0x8000000000000000ULL)
				{ MCC_TRACE("br\n"); ov = 1, ucount = 1, lcount = 2; }
		} else { MCC_TRACE("br\n");
			if (lcount <= (LONG_SIZE == 4)) { MCC_TRACE("br\n");
				if (n >= 0x100000000ULL)
					{ MCC_TRACE("br\n"); lcount = (LONG_SIZE == 4) + 1; }
				else if (n >= 0x80000000U)
					{ MCC_TRACE("br\n"); ucount = 1; }
			}
			if (n >= 0x8000000000000000ULL)
				{ MCC_TRACE("br\n"); ucount = 1; }
		}

		if (ov)
			{ MCC_TRACE("br\n"); mcc_warning("integer constant overflow"); }

		tok = TOK_CINT;
		if (lcount) { MCC_TRACE("br\n");
			tok = TOK_CLONG;
			if (lcount == 2)
				{ MCC_TRACE("br\n"); tok = TOK_CLLONG; }
		}
		if (ucount)
			{ MCC_TRACE("br\n"); ++tok; }
		tokc.i = n;
	int_suffix_done:;
	}
	if ((ch == 'i' || ch == 'I' || ch == 'j' || ch == 'J') && (tok == TOK_CFLOAT || tok == TOK_CDOUBLE || tok == TOK_CLDOUBLE || (tok >= TOK_CINT && tok <= TOK_CULONG))) { MCC_TRACE("br\n");
		tok_imaginary = 1;
		ch = *p++;
		if (tok == TOK_CDOUBLE) { MCC_TRACE("br\n");
			t = toup(ch);
			if (t == 'F') { MCC_TRACE("br\n");
				ch = *p++;
				tok = TOK_CFLOAT;
				tokc.f = (float)tokc.d;
			} else if (t == 'L') { MCC_TRACE("br\n");
				ch = *p++;
				tok = TOK_CLDOUBLE;
				tokc.ld = (long double)tokc.d;
			}
		}
	}
	if (ch)
		{ MCC_TRACE("br\n"); mcc_error("invalid number"); }
}

#define PARSE2(c1, tok1, c2, tok2) \
	case c1:                         \
		PEEKC(c, p);                   \
		if (c == c2) {                 \
			p++;                         \
			tok = tok2;                  \
		} else {                       \
			tok = tok1;                  \
		}                              \
		break;

static unsigned long cst_prev_end;

ST_FUNC void cst_capture_begin(const char *filename) { MCC_TRACE("enter\n");
	cst_hook_begin(filename);
	cst_main_bf = file;
	cst_prev_end = 0;
}

ST_FUNC CstArena *cst_capture_end(void) { MCC_TRACE("enter\n");
	cst_main_bf = NULL;
	CstArena *a = cst_hook_end();
	if (a && getenv("MCC_CST_SELFCHECK")) { MCC_TRACE("br\n");
		char msg[128];
		int rc = cst_validate(a, msg, sizeof msg);
		uint32_t slen, n, nrefs = 0, nn = cst_node_count(a);
		cst_source(a, &slen);
		for (n = 0; n < nn; n++)
			{ MCC_TRACE("br\n"); if (cst_sym_ref(a, n) != (CstId)0xffffffffffffffffull)
				{ MCC_TRACE("br\n"); nrefs++; } }
		fprintf(stderr, "CST selfcheck: %s (%u bytes, %u nodes, %u sym-refs)%s%s\n",
						rc == 0 ? "round-trip OK" : "MISMATCH", slen, nn, nrefs,
						rc == 0 ? "" : ": ", rc == 0 ? "" : msg);
	}
	if (a && getenv("MCC_CST_TREE")) { MCC_TRACE("br\n");
		static const char *kn[] = {
				"TU", "Decl", "Func", "Declarator",
				"ParamList", "Struct", "Enum", "TypeName", "Init", "Compound",
				"If", "While", "For", "Do", "Switch", "Return", "Goto", "Label",
				"ExprStmt", "Binary", "Unary", "Call", "Member", "Index", "Cast",
				"Cond", "Comma", "Paren", "Primary", "MacroInv", "Include",
				"PPDirective", "PPCond", "Token", "Comment", "Error", "Missing"};
		uint32_t slen;
		const uint8_t *src = cst_source(a, &slen);
		CstLocal stackn[256], depthn[256];
		int sp = 0;
		stackn[sp] = cst_root(a);
		depthn[sp] = 0;
		sp++;
		while (sp > 0) { MCC_TRACE("br\n");
			CstLocal n = stackn[--sp];
			int d = depthn[sp];
			uint16_t k = cst_kind(a, n);
			uint32_t o = cst_abs_offset(a, n), w = cst_width(a, n);
			if (k == CST_Token)
				{ MCC_TRACE("br\n"); fprintf(stderr, "%*sToken '%.*s'\n", d * 2, "",
								w > 24 ? 24 : (int)w, src + o); }
			else
				{ MCC_TRACE("br\n"); fprintf(stderr, "%*s%s [%u,%u)\n", d * 2, "",
								k < CST_KIND_COUNT ? kn[k] : "?", o, o + w); }
			CstLocal kids[256];
			int nk = 0;
			for (CstLocal c = cst_first_child(a, n); c != CST_NONE;
					 c = cst_next_sib(a, c))
				{ MCC_TRACE("br\n"); if (nk < 256)
					{ MCC_TRACE("br\n"); kids[nk++] = c; } }
			while (nk-- > 0 && sp < 256) { MCC_TRACE("br\n");
				stackn[sp] = kids[nk];
				depthn[sp] = d + 1;
				sp++;
			}
		}
	}
	if (a && getenv("MCC_CST_HASHDUMP")) { MCC_TRACE("br\n");
		CstHash h = cst_struct_hash(a, cst_root(a));
		fprintf(stderr, "CST roothash: %016llx%016llx\n",
						(unsigned long long)h.hi, (unsigned long long)h.lo);
	}
	if (a && getenv("MCC_CST_SYMDUMP")) { MCC_TRACE("br\n");
		uint32_t slen, n, nn = cst_node_count(a);
		const uint8_t *src = cst_source(a, &slen);
		for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
			CstId d = cst_sym_ref(a, n);
			if (d == (CstId)0xffffffffffffffffull)
				{ MCC_TRACE("br\n"); continue; }
			CstLocal dn = cst_id_local(d);
			uint32_t uo = cst_abs_offset(a, n), uw = cst_width(a, n);
			uint32_t vo = cst_abs_offset(a, dn), vw = cst_width(a, dn);
			fprintf(stderr, "  use[%u] '%.*s' -> def[%u] '%.*s'\n", n,
							(int)uw, src + uo, dn, (int)vw, src + vo);
		}
	}
	if (a && getenv("MCC_CST_SNAPSHOT")) { MCC_TRACE("br\n");
		const char *path = getenv("MCC_CST_SNAPSHOT");
		char m2[128];
		int ok = 0;
		if (cst_snapshot_save(a, path) == 0) { MCC_TRACE("br\n");
			CstArena *b = cst_snapshot_load(path);
			if (b) { MCC_TRACE("br\n");
				ok = (cst_validate(b, m2, sizeof m2) == 0) &&
						 cst_hash_eq(cst_struct_hash(a, cst_root(a)),
												 cst_struct_hash(b, cst_root(b)));
				cst_arena_free(b);
			}
		}
		fprintf(stderr, "CST snapshot: %s\n", ok ? "reload OK" : "reload FAIL");
	}
	if (a && getenv("MCC_CST_STORE")) { MCC_TRACE("br\n");
		CstStore *st = cst_hook_store();
		uint32_t nn = cst_node_count(a), n, nt = st ? cst_store_count(st) : 0, ti;
		fprintf(stderr, "CST store: %u templates\n", nt);
		for (ti = 0; ti < nt; ti++) { MCC_TRACE("br\n");
			CstArena *tmpl = cst_store_get(st, ti);
			uint32_t tn = cst_node_count(tmpl), k, cc = 0, slen = 0;
			for (k = 0; k < tn; k++)
				{ MCC_TRACE("br\n"); if (cst_kind(tmpl, k) == CST_PPConditional)
					{ MCC_TRACE("br\n"); cc++; } }
			cst_source(tmpl, &slen);
			size_t rid = cst_render_identity(tmpl, NULL, 0);
			fprintf(stderr,
							"  template %u: %u nodes, %u PPConditional, render_identity %zu/%u %s\n",
							ti, tn, cc, rid, slen, rid == slen ? "OK" : "MISMATCH");
		}
		for (n = 0; n < nn; n++)
			{ MCC_TRACE("br\n"); if (cst_kind(a, n) == CST_IncludeDirective)
				{ MCC_TRACE("br\n"); fprintf(stderr, "  include node %u -> template %u\n", n,
								cst_include_target(a, n)); } }
	}
	if (a)
		{ MCC_TRACE("br\n"); cst_arena_free(a); }
	return NULL;
}

static void cst_capture_tok(void) { MCC_TRACE("enter\n");
	if (file != cst_main_bf)
		{ MCC_TRACE("br\n"); return; }
	unsigned long end =
			file->cst_base + (unsigned long)(file->buf_ptr - file->buffer);
	if (end > cst_prev_end) { MCC_TRACE("br\n");
		cst_hook_token((uint32_t)cst_prev_end, (uint32_t)end);
		cst_prev_end = end;
	}
}

static int digit_sep_enabled(void) { MCC_TRACE("enter\n");
	return mcc_state->cversion >= 202311;
}

static int digit_of_base(int c, int base) { MCC_TRACE("enter\n");
	if (c >= '0' && c <= '9')
		{ MCC_TRACE("br\n"); return c - '0' < base || base == 16; }
	if (base == 16)
		{ MCC_TRACE("br\n"); return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
	return 0;
}

static int num_base_so_far(const char *s, int len) { MCC_TRACE("enter\n");
	if (len >= 2 && s[0] == '0') { MCC_TRACE("br\n");
		int c = toup(s[1]);
		if (c == 'X')
			{ MCC_TRACE("br\n"); return 16; }
		if (c == 'B')
			{ MCC_TRACE("br\n"); return 2; }
		if (c == 'O')
			{ MCC_TRACE("br\n"); return 8; }
	}
	return 10;
}

static int num_at_base_indicator(const char *s, int len) { MCC_TRACE("enter\n");
	int c;
	if (len != 2 || s[0] != '0')
		{ MCC_TRACE("br\n"); return 0; }
	c = toup(s[1]);
	return c == 'X' || c == 'B' || c == 'O';
}

static void next_nomacro(void) { MCC_TRACE("enter\n");
	int t, c, str_prefix, len, uc, num_sep_cur, num_sep_next;
	TokenSym *ts;
	uint8_t *p, *p1;
	unsigned int h;

	p = file->buf_ptr;
redo_no_start:
	tok_line_num = file->line_num, tok_line_file = file;
	c = *p;
	switch (c) { MCC_TRACE("br\n");
	case ' ':
	case '\t':
		tok = c;
		p++;
	maybe_space:
		if (parse_flags & PARSE_FLAG_SPACES)
			{ MCC_TRACE("br\n"); goto keep_tok_flags; }
		while (isidnum_table[*p - CH_EOF] & IS_SPC)
			{ MCC_TRACE("br\n"); ++p; }
		goto redo_no_start;
	case '\f':
	case '\v':
		p++;
		goto redo_no_start;
	case '\r':
		p++;
		if (*p == '\n' || *p == CH_EOB)
			{ MCC_TRACE("br\n"); goto redo_no_start; }
		file->line_num++;
		goto maybe_newline;
	case '\\':
		if ((uc = decode_ucn(&p)) >= 0) { MCC_TRACE("br\n");
			if (ucn_disallowed_initial(uc))
				{ MCC_TRACE("br\n"); mcc_error("universal character \\u%04x is not valid as the "
									"first character of an identifier",
									uc); }
			if (mcc_state->warn_pedantic && mcc_state->cversion < 199901) { MCC_TRACE("br\n");
				if (mcc_state->pedantic_errors)
					{ MCC_TRACE("br\n"); mcc_error("extended identifiers are a C99 feature"); }
				else
					{ MCC_TRACE("br\n"); mcc_warning("extended identifiers are a C99 feature"); }
			}
			cstr_reset(&tokcstr);
			cstr_u8cat(&tokcstr, uc);
			c = *p;
			goto parse_ident_ucn;
		}
		c = handle_stray(&p);
		if (c == '\\')
			{ MCC_TRACE("br\n"); goto parse_simple; }
		if (c == CH_EOF) { MCC_TRACE("br\n");
			MCCState *s1 = mcc_state;
			if (!(tok_flags & TOK_FLAG_BOL)) { MCC_TRACE("br\n");
				goto maybe_newline;
			} else if (!(parse_flags & PARSE_FLAG_PREPROCESS)) { MCC_TRACE("br\n");
				tok = TOK_EOF;
			} else if (s1->ifdef_stack_ptr != file->ifdef_stack_ptr) { MCC_TRACE("br\n");
				mcc_error("missing #endif");
			} else if (s1->include_stack_ptr == s1->include_stack) { MCC_TRACE("br\n");
				tok = TOK_EOF;
			} else { MCC_TRACE("br\n");
				if (tok_flags & TOK_FLAG_ENDIF) { MCC_TRACE("br\n");
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
		} else { MCC_TRACE("br\n");
			goto redo_no_start;
		}
		break;

	case '\n':
		file->line_num++;
		p++;
	maybe_newline:
		tok_flags |= TOK_FLAG_BOL;
		if (0 == (parse_flags & PARSE_FLAG_LINEFEED))
			{ MCC_TRACE("br\n"); goto redo_no_start; }
		tok = TOK_LINEFEED;
		goto keep_tok_flags;

	case '#':
		PEEKC(c, p);
		if ((tok_flags & TOK_FLAG_BOL) &&
				(parse_flags & PARSE_FLAG_PREPROCESS)) { MCC_TRACE("br\n");
			tok_flags &= ~TOK_FLAG_BOL;
			file->buf_ptr = p;
			preprocess(tok_flags & TOK_FLAG_BOF);
			p = file->buf_ptr;
			goto maybe_newline;
		} else { MCC_TRACE("br\n");
			if (c == '#') { MCC_TRACE("br\n");
				p++;
				tok = TOK_TWOSHARPS;
			} else { MCC_TRACE("br\n");
#if !defined(MCC_TARGET_ARM) && !defined(MCC_TARGET_ARM64)
				if (parse_flags & PARSE_FLAG_ASM_FILE) { MCC_TRACE("br\n");
					p = parse_line_comment(p - 1);
					goto redo_no_start;
				} else
#endif
				{ MCC_TRACE("br\n");
					tok = '#';
				}
			}
		}
		break;

	case '$':
		if (!(isidnum_table['$' - CH_EOF] & IS_ID) || (parse_flags & PARSE_FLAG_ASM_FILE))
			{ MCC_TRACE("br\n"); goto parse_simple; }
		FALLTHROUGH;
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
	case 'g':
	case 'h':
	case 'i':
	case 'j':
	case 'k':
	case 'l':
	case 'm':
	case 'n':
	case 'o':
	case 'p':
	case 'q':
	case 'r':
	case 's':
	case 't':
	case 'v':
	case 'w':
	case 'x':
	case 'y':
	case 'z':
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
	case 'G':
	case 'H':
	case 'I':
	case 'J':
	case 'K':
	case 'M':
	case 'N':
	case 'O':
	case 'P':
	case 'Q':
	case 'R':
	case 'S':
	case 'T':
	case 'V':
	case 'W':
	case 'X':
	case 'Y':
	case 'Z':
	case '_':
	parse_ident_fast:
		p1 = p;
		h = TOK_HASH_INIT;
		h = TOK_HASH_FUNC(h, c);
		{
			int hi = c;
			while (c = *++p, isidnum_table[c - CH_EOF] & (IS_ID | IS_NUM)) { MCC_TRACE("br\n");
				h = TOK_HASH_FUNC(h, c);
				hi |= c;
			}
			if (hi & 0x80)
				{ MCC_TRACE("br\n"); validate_utf8_identifier((const char *)p1, p - p1); }
		}
		len = p - p1;
		if (mcc_state->warn_pedantic && !pp_in_system_header() && memchr(p1, '$', len)) { MCC_TRACE("br\n");
			if (mcc_state->pedantic_errors)
				{ MCC_TRACE("br\n"); mcc_error("'$' in identifier"); }
			else
				{ MCC_TRACE("br\n"); mcc_warning("'$' in identifier"); }
		}
		if (c != '\\') { MCC_TRACE("br\n");
			TokenSym **pts;

			h &= (TOK_HASH_SIZE - 1);
			pts = &hash_ident[h];
			for (;;) { MCC_TRACE("br\n");
				ts = *pts;
				if (!ts)
					{ MCC_TRACE("br\n"); break; }
				if (ts->len == len && !memcmp(ts->str, p1, len))
					{ MCC_TRACE("br\n"); goto token_found; }
				pts = &(ts->hash_next);
			}
			ts = tok_alloc_new(pts, (char *)p1, len);
		token_found:;
		} else { MCC_TRACE("br\n");
			cstr_reset(&tokcstr);
			cstr_cat(&tokcstr, (char *)p1, len);
		parse_ident_ucn:
			for (;;) { MCC_TRACE("br\n");
				if (c == '\\') { MCC_TRACE("br\n");
					if ((uc = decode_ucn(&p)) >= 0) { MCC_TRACE("br\n");
						cstr_u8cat(&tokcstr, uc);
						c = *p;
						continue;
					}
					p--;
					PEEKC(c, p);
					if (c == '\\')
						{ MCC_TRACE("br\n"); break; }
					continue;
				}
				if (isidnum_table[c - CH_EOF] & (IS_ID | IS_NUM)) { MCC_TRACE("br\n");
					cstr_ccat(&tokcstr, c);
					c = *++p;
					continue;
				}
				break;
			}
			ts = tok_alloc(tokcstr.data, tokcstr.size);
		}
		tok_ts = ts;
		tok = ts->tok;
		if (pp_npoison && pp_is_poisoned(tok))
			{ MCC_TRACE("br\n"); mcc_error("attempt to use poisoned identifier '%s'", ts->str); }
		break;
	case 'u':
		if (p[1] == '8' && p[2] == '\"') { MCC_TRACE("br\n");
			pp_c11_prefix_pedantic("the 'u8' string-literal prefix");
			p += 2;
			c = *p;
			str_prefix = '8';
			goto str_const;
		}
		if (p[1] == '8' && p[2] == '\'' && mcc_state->cversion >= 202311) { MCC_TRACE("br\n");
			p += 2;
			c = *p;
			str_prefix = '8';
			goto str_const;
		}
		if (p[1] == '\'' || p[1] == '\"') { MCC_TRACE("br\n");
			pp_c11_prefix_pedantic("the 'u' character/string prefix");
			PEEKC(c, p);
			str_prefix = 'u';
			goto str_const;
		}
		goto parse_ident_fast;

	case 'U':
		if (p[1] == '\'' || p[1] == '\"') { MCC_TRACE("br\n");
			pp_c11_prefix_pedantic("the 'U' character/string prefix");
			PEEKC(c, p);
			str_prefix = 'U';
			goto str_const;
		}
		goto parse_ident_fast;

	case 'L':
		t = p[1];
		if (t == '\'' || t == '\"' || t == '\\') { MCC_TRACE("br\n");
			PEEKC(c, p);
			if (c == '\'' || c == '\"') { MCC_TRACE("br\n");
				str_prefix = 'L';
				goto str_const;
			}
			*--p = c = 'L';
		}
		goto parse_ident_fast;

	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		t = c;
		PEEKC(c, p);
	parse_num:
		cstr_reset(&tokcstr);
		num_sep_next = num_sep_cur = 0;
		for (;;) { MCC_TRACE("br\n");
			cstr_ccat(&tokcstr, t);
			while (c == '\'' && digit_sep_enabled() &&
					!(parse_flags & PARSE_FLAG_ASM_FILE)) { MCC_TRACE("br\n");
				int nc = p[1];
				if (digit_of_base(nc, num_base_so_far((char *)tokcstr.data,
						tokcstr.size))) { MCC_TRACE("br\n");
					if (num_at_base_indicator((char *)tokcstr.data, tokcstr.size))
						{ MCC_TRACE("br\n"); mcc_error("digit separator after base indicator"); }
					num_sep_next = 1;
					PEEKC(c, p);
				} else if (nc == '\'') { MCC_TRACE("br\n");
					mcc_error("adjacent digit separators");
				} else if (isidnum_table[nc - CH_EOF] & (IS_ID | IS_NUM)) { MCC_TRACE("br\n");
					mcc_error("digit separator outside digit sequence");
				} else { MCC_TRACE("br\n");
					break;
				}
			}
			if (!((isidnum_table[c - CH_EOF] & (IS_ID | IS_NUM)) || c == '.' || ((c == '+' || c == '-') && !num_sep_cur && (((t == 'e' || t == 'E') && !(parse_flags & PARSE_FLAG_ASM_FILE && ((char *)tokcstr.data)[0] == '0' && toup(((char *)tokcstr.data)[1]) == 'X')) || ((t == 'p' || t == 'P') && !(mcc_state->std_strict_ansi && mcc_state->cversion < 199901))))))
				{ MCC_TRACE("br\n"); break; }
			t = c;
			num_sep_cur = num_sep_next;
			num_sep_next = 0;
			PEEKC(c, p);
		}
		cstr_ccat(&tokcstr, '\0');
		tokc.str.size = tokcstr.size;
		tokc.str.data = tokcstr.data;
		tok = TOK_PPNUM;
		break;

	case '.':
		PEEKC(c, p);
		if (isnum(c)) { MCC_TRACE("br\n");
			t = '.';
			goto parse_num;
		} else if ((isidnum_table['.' - CH_EOF] & IS_ID) && (isidnum_table[c - CH_EOF] & (IS_ID | IS_NUM))) { MCC_TRACE("br\n");
			*--p = c = '.';
			goto parse_ident_fast;
		} else if (c == '.') { MCC_TRACE("br\n");
			PEEKC(c, p);
			if (c == '.') { MCC_TRACE("br\n");
				p++;
				tok = TOK_DOTS;
			} else { MCC_TRACE("br\n");
				*--p = '.';
				tok = '.';
			}
		} else { MCC_TRACE("br\n");
			tok = '.';
		}
		break;
	case '\'':
	case '\"':
		str_prefix = 0;
	str_const:
		cstr_reset(&tokcstr);
		if (str_prefix == '8')
			{ MCC_TRACE("br\n"); cstr_ccat(&tokcstr, 'u'), cstr_ccat(&tokcstr, '8'); }
		else if (str_prefix)
			{ MCC_TRACE("br\n"); cstr_ccat(&tokcstr, str_prefix); }
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
		if (c == '=') { MCC_TRACE("br\n");
			p++;
			tok = TOK_LE;
		} else if (c == '<') { MCC_TRACE("br\n");
			PEEKC(c, p);
			if (c == '=') { MCC_TRACE("br\n");
				p++;
				tok = TOK_A_SHL;
			} else { MCC_TRACE("br\n");
				tok = TOK_SHL;
			}
		} else if (c == ':' && digraphs_enabled()) { MCC_TRACE("br\n");
			p++;
			tok = TOK_DIG_LBRACK;
		} else if (c == '%' && digraphs_enabled()) { MCC_TRACE("br\n");
			p++;
			tok = TOK_DIG_LBRACE;
		} else { MCC_TRACE("br\n");
			tok = TOK_LT;
		}
		break;
	case '>':
		PEEKC(c, p);
		if (c == '=') { MCC_TRACE("br\n");
			p++;
			tok = TOK_GE;
		} else if (c == '>') { MCC_TRACE("br\n");
			PEEKC(c, p);
			if (c == '=') { MCC_TRACE("br\n");
				p++;
				tok = TOK_A_SAR;
			} else { MCC_TRACE("br\n");
				tok = TOK_SAR;
			}
		} else { MCC_TRACE("br\n");
			tok = TOK_GT;
		}
		break;

	case '&':
		PEEKC(c, p);
		if (c == '&') { MCC_TRACE("br\n");
			p++;
			tok = TOK_LAND;
		} else if (c == '=') { MCC_TRACE("br\n");
			p++;
			tok = TOK_A_AND;
		} else { MCC_TRACE("br\n");
			tok = '&';
		}
		break;

	case '|':
		PEEKC(c, p);
		if (c == '|') { MCC_TRACE("br\n");
			p++;
			tok = TOK_LOR;
		} else if (c == '=') { MCC_TRACE("br\n");
			p++;
			tok = TOK_A_OR;
		} else { MCC_TRACE("br\n");
			tok = '|';
		}
		break;

	case '+':
		PEEKC(c, p);
		if (c == '+') { MCC_TRACE("br\n");
			p++;
			tok = TOK_INC;
		} else if (c == '=') { MCC_TRACE("br\n");
			p++;
			tok = TOK_A_ADD;
		} else { MCC_TRACE("br\n");
			tok = '+';
		}
		break;

	case '-':
		PEEKC(c, p);
		if (c == '-') { MCC_TRACE("br\n");
			p++;
			tok = TOK_DEC;
		} else if (c == '=') { MCC_TRACE("br\n");
			p++;
			tok = TOK_A_SUB;
		} else if (c == '>') { MCC_TRACE("br\n");
			p++;
			tok = TOK_ARROW;
		} else { MCC_TRACE("br\n");
			tok = '-';
		}
		break;

		PARSE2('!', '!', '=', TOK_NE)
		PARSE2('=', '=', '=', TOK_EQ)
		PARSE2('*', '*', '=', TOK_A_MUL)
	case '%':
		PEEKC(c, p);
		if (c == '=') { MCC_TRACE("br\n");
			p++;
			tok = TOK_A_MOD;
		} else if (c == '>' && digraphs_enabled()) { MCC_TRACE("br\n");
			p++;
			tok = TOK_DIG_RBRACE;
		} else if (c == ':' && digraphs_enabled()) { MCC_TRACE("br\n");
			PEEKC(c, p);
			if (c == '%' && p[1] == ':') { MCC_TRACE("br\n");
				p += 2;
				tok = TOK_DIG_TWOSHARPS;
			} else if ((tok_flags & TOK_FLAG_BOL) && (parse_flags & PARSE_FLAG_PREPROCESS)) { MCC_TRACE("br\n");
				tok_flags &= ~TOK_FLAG_BOL;
				file->buf_ptr = p;
				preprocess(tok_flags & TOK_FLAG_BOF);
				p = file->buf_ptr;
				goto maybe_newline;
			} else { MCC_TRACE("br\n");
				tok = TOK_DIG_HASH;
			}
		} else { MCC_TRACE("br\n");
			tok = '%';
		}
		break;
		PARSE2('^', '^', '=', TOK_A_XOR)

	case '/':
		PEEKC(c, p);
		if (c == '*') { MCC_TRACE("br\n");
			uint8_t *cs = p - 1;
			p = parse_comment(p);
			if (mcc_state->keep_comments && mcc_state->output_type == MCC_OUTPUT_PREPROCESS && !pp_directive_depth) { MCC_TRACE("br\n");
				cstr_reset(&pp_comment_text);
				cstr_cat(&pp_comment_text, (const char *)cs, p - cs);
				pp_comment_ready = 1;
			}
			tok = ' ';
			goto maybe_space;
		} else if (c == '/') { MCC_TRACE("br\n");
			if (mcc_state->std_strict_ansi && mcc_state->cversion < 199901 &&
					!pp_in_system_header()) { MCC_TRACE("br\n");
				tok = '/';
				break;
			}
			if (mcc_state->warn_pedantic && mcc_state->cversion < 199901 &&
					!pp_in_system_header()) { MCC_TRACE("br\n");
				if (mcc_state->pedantic_errors)
					{ MCC_TRACE("br\n"); mcc_error("C++ style comments are a C99 feature"); }
				else
					{ MCC_TRACE("br\n"); mcc_warning("C++ style comments are a C99 feature"); }
			}
			uint8_t *cs = p - 1;
			p = parse_line_comment(p);
			if (mcc_state->keep_comments && mcc_state->output_type == MCC_OUTPUT_PREPROCESS && !pp_directive_depth) { MCC_TRACE("br\n");
				cstr_reset(&pp_comment_text);
				cstr_cat(&pp_comment_text, (const char *)cs, p - cs);
				pp_comment_ready = 1;
			}
			tok = ' ';
			goto maybe_space;
		} else if (c == '=') { MCC_TRACE("br\n");
			p++;
			tok = TOK_A_DIV;
		} else { MCC_TRACE("br\n");
			tok = '/';
		}
		break;

	case '@':
#ifdef MCC_TARGET_ARM
		if (parse_flags & PARSE_FLAG_ASM_FILE) { MCC_TRACE("br\n");
			p = parse_line_comment(p);
			goto redo_no_start;
		}
#endif
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
		if (c == '>' && digraphs_enabled()) { MCC_TRACE("br\n");
			p++;
			tok = TOK_DIG_RBRACK;
		} else { MCC_TRACE("br\n");
			tok = ':';
		}
		break;
	case 0xEF:
		if (p[1] == 0xBB && p[2] == 0xBF && p == file->buffer) { MCC_TRACE("br\n");
			p += 3;
			goto redo_no_start;
		}
		FALLTHROUGH;
	default:
		if (c >= 0x80 && c <= 0xFF)
			{ MCC_TRACE("br\n"); goto parse_ident_fast; }
		if (parse_flags & PARSE_FLAG_ASM_FILE)
			{ MCC_TRACE("br\n"); goto parse_simple; }
		mcc_error("unrecognized character \\x%02x", c);
		break;
	}
	tok_flags = 0;
keep_tok_flags:
	file->buf_ptr = p;
	cst_capture_tok();
	if (g_debug & MCC_DBG_TOK)
		{ MCC_TRACE("br\n"); printf("token = %d %s\n", tok, get_tok_str(tok, &tokc)); }
}

static void define_print(MCCState *s1, int v);
static void pp_print(const char *msg, int v, const int *str) { MCC_TRACE("enter\n");
	FILE *fp = mcc_state->ppfp;
	int *indent = &mcc_state->pp_debug_indent;

	if (!fp)
		{ MCC_TRACE("br\n"); fp = stdout; }
	if (msg[0] == '#' && *indent == 0)
		{ MCC_TRACE("br\n"); fprintf(fp, "\n"); }
	else if (msg[0] == '+')
		{ MCC_TRACE("br\n"); ++*indent, ++msg; }
	else if (msg[0] == '-')
		{ MCC_TRACE("br\n"); --*indent, ++msg; }

	fprintf(fp, "%*s", *indent, "");
	if (msg[0] == '#') { MCC_TRACE("br\n");
		define_print(mcc_state, v);
	} else { MCC_TRACE("br\n");
		tok_print(str, v ? "%s %s" : "%s", msg, get_tok_str(v, 0));
	}
}
#define PP_PRINT(x)          \
	do {                       \
		if (g_debug & MCC_DBG_PP) \
			pp_print x;            \
	} while (0)

static int macro_subst(
		TokenString *tok_str,
		Sym **nested_list,
		const int *macro_str);

static int *macro_arg_subst2(Sym **nested_list, const int *macro_str, Sym *args,
														 int join_pre, int join_post);
static int *macro_twosharps(const int *ptr0);

static int *macro_arg_subst(Sym **nested_list, const int *macro_str, Sym *args) { MCC_TRACE("enter\n");
	return macro_arg_subst2(nested_list, macro_str, args, 0, 0);
}

static int tok_str_has_join(const int *p) { MCC_TRACE("enter\n");
	int t;
	CValue cval;

	while (*p) { MCC_TRACE("br\n");
		TOK_GET(&t, &p, &cval);
		if (t == TOK_PPJOIN)
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

static int has_va_args(Sym *args) { MCC_TRACE("enter\n");
	Sym *s;

	for (s = args; s; s = s->prev) { MCC_TRACE("br\n");
		if (s->type.t)
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

static int va_args_nonempty(Sym **nested_list, Sym *args) { MCC_TRACE("enter\n");
	Sym *s;
	const int *st;

	for (s = args; s; s = s->prev) { MCC_TRACE("br\n");
		if (!s->type.t)
			{ MCC_TRACE("br\n"); continue; }
		if (!s->d)
			{ MCC_TRACE("br\n"); return 0; }
		if (!s->e) { MCC_TRACE("br\n");
			TokenString str2;
			tok_str_new(&str2);
			str2.need_spc = 2;
			macro_subst(&str2, nested_list, s->d);
			if (str2.need_spc & 1)
				{ MCC_TRACE("br\n"); tok_str_add(&str2, ' '); }
			tok_str_add(&str2, TOK_EOF);
			s->e = str2.str;
		}
		st = s->e;
		while (*st == ' ')
			{ MCC_TRACE("br\n"); ++st; }
		return *st != TOK_EOF;
	}
	return 0;
}

static const int *va_opt_group(const int *p, TokenString *out) { MCC_TRACE("enter\n");
	int t, level;
	CValue cval;

	while (*p == ' ')
		{ MCC_TRACE("br\n"); ++p; }
	if (*p != '(')
		{ MCC_TRACE("br\n"); return p; }
	++p;
	level = 1;
	while (*p) { MCC_TRACE("br\n");
		TOK_GET(&t, &p, &cval);
		if (t == '(')
			{ MCC_TRACE("br\n"); ++level; }
		else if (t == ')') { MCC_TRACE("br\n");
			if (--level == 0)
				{ MCC_TRACE("br\n"); break; }
		}
		tok_str_add2(out, t, &cval);
	}
	return p;
}

static int *va_opt_subst(Sym **nested_list, const int **pmacro_str, Sym *args,
												 int join_pre, int outer_join_post) { MCC_TRACE("enter\n");
	TokenString grp;
	int *res;
	int n, t2;

	tok_str_new(&grp);
	*pmacro_str = va_opt_group(*pmacro_str, &grp);
	tok_str_add(&grp, 0);
	n = 0;
	while ((t2 = (*pmacro_str)[n]) == ' ')
		{ MCC_TRACE("br\n"); ++n; }
	if (t2 == 0 && outer_join_post)
		{ MCC_TRACE("br\n"); t2 = TOK_PPJOIN; }
	if (va_args_nonempty(nested_list, args))
		{ MCC_TRACE("br\n"); res = macro_arg_subst2(nested_list, grp.str, args, join_pre,
																								t2 == TOK_PPJOIN); }
	else { MCC_TRACE("br\n");
		TokenString empty;
		tok_str_new(&empty);
		tok_str_add(&empty, 0);
		res = empty.str;
	}
	tok_str_free_str(grp.str);
	return res;
}

static int *macro_arg_subst2(Sym **nested_list, const int *macro_str, Sym *args,
														 int join_pre, int join_post) { MCC_TRACE("enter\n");
	int t, t0, t1, t2, n;
	int at_first, bnd;
	const int *st;
	Sym *s;
	CValue cval;
	TokenString str;

	if (g_debug & MCC_DBG_PP) { MCC_TRACE("br\n");
		PP_PRINT(("asubst:", 0, macro_str));
		for (s = args, n = 0; s; s = s->prev, ++n)
			;
		while (n--) { MCC_TRACE("br\n");
			for (s = args, t = 0; t < n; s = s->prev, ++t)
				;
			tok_print(s->d, "%*s - arg: %s:", mcc_state->pp_debug_indent, "",
								get_tok_str(s->v, 0));
		}
	}

	tok_str_new(&str);
	t0 = t1 = 0;
	at_first = 1;
	while (1) { MCC_TRACE("br\n");
		TOK_GET(&t, &macro_str, &cval);
		if (!t)
			{ MCC_TRACE("br\n"); break; }
		if (t == '#' || t == TOK_DIG_HASH) { MCC_TRACE("br\n");
			int *vstr = NULL;
			do
				{ MCC_TRACE("br\n"); t = *macro_str++; }
			while (t == ' ');
			s = sym_find2(args, t);
			if (!s && t == tok_va_opt && has_va_args(args)) { MCC_TRACE("br\n");
				int *sub = va_opt_subst(nested_list, &macro_str, args, 0, 0);
				TokenString vs;
				const int *sp;
				if (tok_str_has_join(sub)) { MCC_TRACE("br\n");
					int *joined = macro_twosharps(sub);
					tok_str_free_str(sub);
					sub = joined;
				}
				tok_str_new(&vs);
				for (sp = sub; *sp;) { MCC_TRACE("br\n");
					TOK_GET(&t2, &sp, &cval);
					if (t2 == TOK_PLCHLDR)
						{ MCC_TRACE("br\n"); continue; }
					if (t2 == ' ' && vs.len == 0)
						{ MCC_TRACE("br\n"); continue; }
					tok_str_add2(&vs, t2, &cval);
				}
				while (vs.len > 0 && vs.str[vs.len - 1] == ' ')
					{ MCC_TRACE("br\n"); vs.len--; }
				tok_str_add(&vs, TOK_EOF);
				tok_str_free_str(sub);
				vstr = vs.str;
			}
			if (s || vstr) { MCC_TRACE("br\n");
				cstr_reset(&tokcstr);
				cstr_ccat(&tokcstr, '\"');
				st = vstr ? vstr : s->d;
				while (*st != TOK_EOF) { MCC_TRACE("br\n");
					const char *s;
					TOK_GET(&t, &st, &cval);
					s = get_tok_str(t, &cval);
					while (*s) { MCC_TRACE("br\n");
						if (t == TOK_PPSTR && *s != '\'') { MCC_TRACE("br\n");
							if (*s == '\"' || *s == '\\')
								{ MCC_TRACE("br\n"); cstr_ccat(&tokcstr, '\\'); }
							cstr_ccat(&tokcstr, *s);
						} else
							{ MCC_TRACE("br\n"); cstr_ccat(&tokcstr, *s); }
						++s;
					}
				}
				{
					int nb = 0;
					while (tokcstr.size - 1 - nb >= 1 && tokcstr.data[tokcstr.size - 1 - nb] == '\\')
						{ MCC_TRACE("br\n"); ++nb; }
					if (nb & 1) { MCC_TRACE("br\n");
						--tokcstr.size;
						mcc_warning("invalid string literal, ignoring final '\\'");
					}
				}
				cstr_ccat(&tokcstr, '\"');
				cstr_ccat(&tokcstr, '\0');
				cval.str.size = tokcstr.size;
				cval.str.data = tokcstr.data;
				tok_str_add2(&str, TOK_PPSTR, &cval);
				if (vstr)
					{ MCC_TRACE("br\n"); tok_str_free_str(vstr); }
#ifdef MCC_TARGET_ARM
			} else if ((parse_flags & PARSE_FLAG_ASM_FILE) && t == TOK_PPNUM) { MCC_TRACE("br\n");
				--macro_str, tok_str_add(&str, '#');
#endif
			} else { MCC_TRACE("br\n");
				expect("macro parameter after '#'");
			}
		} else if (t == tok_va_opt && has_va_args(args) && !sym_find2(args, t)) { MCC_TRACE("br\n");
			int *sub;
			const int *sp;
			int emitted = 0;
			bnd = at_first && join_pre;
			sub = va_opt_subst(nested_list, &macro_str, args,
												 t1 == TOK_PPJOIN || bnd, join_post);
			for (sp = sub; *sp;) { MCC_TRACE("br\n");
				TOK_GET(&t2, &sp, &cval);
				tok_str_add2(&str, t2, &cval);
				if (t2 != ' ')
					{ MCC_TRACE("br\n"); emitted = 1; }
			}
			tok_str_free_str(sub);
			if (!emitted) { MCC_TRACE("br\n");
				n = 0;
				while ((t2 = macro_str[n]) == ' ')
					{ MCC_TRACE("br\n"); ++n; }
				if (t2 == 0 && join_post)
					{ MCC_TRACE("br\n"); t2 = TOK_PPJOIN; }
				if (t2 == TOK_PPJOIN || t1 == TOK_PPJOIN || bnd)
					{ MCC_TRACE("br\n"); tok_str_add(&str, TOK_PLCHLDR); }
			}
		} else if (t >= TOK_IDENT) { MCC_TRACE("br\n");
			s = sym_find2(args, t);
			if (s) { MCC_TRACE("br\n");
				st = s->d;
				n = 0;
				while ((t2 = macro_str[n]) == ' ')
					{ MCC_TRACE("br\n"); ++n; }
				bnd = (at_first && join_pre) || (t2 == 0 && join_post);
				if (t2 == TOK_PPJOIN || t1 == TOK_PPJOIN) { MCC_TRACE("br\n");
					if (t1 == TOK_PPJOIN && t0 == ',' && gnu_ext && s->type.t) { MCC_TRACE("br\n");
						int c = str.str[str.len - 1];
						while (str.str[--str.len] != ',')
							;
						if (*st == TOK_EOF) { MCC_TRACE("br\n");
						} else { MCC_TRACE("br\n");
							str.len++;
							if (c == ' ')
								{ MCC_TRACE("br\n"); str.str[str.len++] = c; }
							goto add_var;
						}
					} else { MCC_TRACE("br\n");
						if (*st == TOK_EOF)
							{ MCC_TRACE("br\n"); tok_str_add(&str, TOK_PLCHLDR); }
					}
				} else { MCC_TRACE("br\n");
				add_var:
					if (!s->e) { MCC_TRACE("br\n");
						TokenString str2;
						tok_str_new(&str2);
						str2.need_spc = 2;
						macro_subst(&str2, nested_list, st);
						if (str2.need_spc & 1)
							{ MCC_TRACE("br\n"); tok_str_add(&str2, ' '); }
						tok_str_add(&str2, TOK_EOF);
						s->e = str2.str;
					}
					st = s->e;
					if (bnd) { MCC_TRACE("br\n");
						const int *q = st;
						while (*q == ' ')
							{ MCC_TRACE("br\n"); ++q; }
						if (*q == TOK_EOF)
							{ MCC_TRACE("br\n"); tok_str_add(&str, TOK_PLCHLDR); }
					}
				}
				while (*st != TOK_EOF) { MCC_TRACE("br\n");
					TOK_GET(&t2, &st, &cval);
					tok_str_add2(&str, t2, &cval);
				}
			} else { MCC_TRACE("br\n");
				tok_str_add(&str, t);
			}
		} else { MCC_TRACE("br\n");
			tok_str_add2(&str, t, &cval);
		}
		if (t != ' ')
			{ MCC_TRACE("br\n"); t0 = t1, t1 = t, at_first = 0; }
	}
	tok_str_add(&str, 0);
	PP_PRINT(("areslt:", 0, str.str));
	return str.str;
}

static inline int *macro_twosharps(const int *ptr0) { MCC_TRACE("enter\n");
	int t1, t2, n, l;
	CValue cv1, cv2;
	TokenString macro_str1;
	const int *ptr;

	tok_str_new(&macro_str1);
	cstr_reset(&tokcstr);
	for (ptr = ptr0;;) { MCC_TRACE("br\n");
		TOK_GET(&t1, &ptr, &cv1);
		if (t1 == 0)
			{ MCC_TRACE("br\n"); break; }
		for (;;) { MCC_TRACE("br\n");
			n = 0;
			while ((t2 = ptr[n]) == ' ')
				{ MCC_TRACE("br\n"); ++n; }
			if (t2 != TOK_PPJOIN)
				{ MCC_TRACE("br\n"); break; }
			ptr += n;
			while ((t2 = *++ptr) == ' ' || t2 == TOK_PPJOIN)
				;
			if (t2 == 0)
				{ MCC_TRACE("br\n"); break; }
			TOK_GET(&t2, &ptr, &cv2);
			if (t2 == TOK_PLCHLDR)
				{ MCC_TRACE("br\n"); continue; }
			if (t1 != TOK_PLCHLDR) { MCC_TRACE("br\n");
				cstr_cat(&tokcstr, get_tok_str(t1, &cv1), -1);
				t1 = TOK_PLCHLDR;
			}
			cstr_cat(&tokcstr, get_tok_str(t2, &cv2), -1);
		}
		if (tokcstr.size) { MCC_TRACE("br\n");
			int ci;
			cstr_ccat(&tokcstr, 0);
			for (ci = 0; ci + 1 < tokcstr.size - 1; ci++) { MCC_TRACE("br\n");
				char *d = (char *)tokcstr.data;
				if (d[ci] == '/' && (d[ci + 1] == '/' || d[ci + 1] == '*'))
					{ MCC_TRACE("br\n"); mcc_error("pasting formed '%s', an invalid preprocessing token",
										(char *)tokcstr.data); }
			}
			mcc_open_bf(mcc_state, ":paste:", tokcstr.size);
			memcpy(file->buffer, tokcstr.data, tokcstr.size);
			tok_flags = 0;
			for (n = 0;; n = l) { MCC_TRACE("br\n");
				next_nomacro();
				tok_str_add2(&macro_str1, tok, &tokc);
				if (*file->buf_ptr == 0)
					{ MCC_TRACE("br\n"); break; }
				tok_str_add(&macro_str1, ' ');
				l = file->buf_ptr - file->buffer;
				mcc_error_noabort("pasting \"%.*s\" and \"%s\" does not give a valid"
													" preprocessing token",
													l - n, file->buffer + n, file->buf_ptr);
			}
			mcc_close();
			cstr_reset(&tokcstr);
		}
		if (t1 != TOK_PLCHLDR)
			{ MCC_TRACE("br\n"); tok_str_add2(&macro_str1, t1, &cv1); }
	}
	tok_str_add(&macro_str1, 0);
	PP_PRINT(("pasted:", 0, macro_str1.str));
	return macro_str1.str;
}

static int peek_file(TokenString *ws_str) { MCC_TRACE("enter\n");
	uint8_t *p = file->buf_ptr - 1;
	int c;
	for (;;) { MCC_TRACE("br\n");
		PEEKC(c, p);
		switch (c) { MCC_TRACE("br\n");
		case '/':
			PEEKC(c, p);
			if (c == '*')
				{ MCC_TRACE("br\n"); p = parse_comment(p); }
			else if (c == '/')
				{ MCC_TRACE("br\n"); p = parse_line_comment(p); }
			else { MCC_TRACE("br\n");
				c = *--p = '/';
				goto leave;
			}
			--p, c = ' ';
			break;
		case ' ':
		case '\t':
			break;
		case '\f':
		case '\v':
		case '\r':
			continue;
		case '\n':
			file->line_num++, tok_flags |= TOK_FLAG_BOL;
			break;
		default:
		leave:
			file->buf_ptr = p;
			return c;
		}
		if (ws_str)
			{ MCC_TRACE("br\n"); tok_str_add(ws_str, c); }
	}
}

static int next_argstream(Sym **nested_list, TokenString *ws_str) { MCC_TRACE("enter\n");
	int t;
	Sym *sa;

	while (macro_ptr) { MCC_TRACE("br\n");
		const int *m = macro_ptr;
		while ((t = *m) != 0) { MCC_TRACE("br\n");
			if (ws_str) { MCC_TRACE("br\n");
				if (t != ' ')
					{ MCC_TRACE("br\n"); return t; }
				++m;
			} else { MCC_TRACE("br\n");
				TOK_GET(&tok, &macro_ptr, &tokc);
				return tok;
			}
		}
		end_macro();
		sa = *nested_list;
		if (sa)
			{ MCC_TRACE("br\n"); *nested_list = sa->prev, sym_free(sa); }
	}
	if (ws_str) { MCC_TRACE("br\n");
		return peek_file(ws_str);
	} else { MCC_TRACE("br\n");
		next_nomacro();
		if (tok == '\t' || tok == TOK_LINEFEED)
			{ MCC_TRACE("br\n"); tok = ' '; }
		return tok;
	}
}

static void pp_builtin_subst(
		TokenString *tok_str,
		Sym **nested_list,
		int v) { MCC_TRACE("enter\n");
	int saved_parse_flags = parse_flags;
	TokenString ws, args;
	CValue cval;
	char buf[32];
	int t, parlevel, i, c;

	parse_flags |= PARSE_FLAG_SPACES | PARSE_FLAG_LINEFEED | PARSE_FLAG_ACCEPT_STRAYS;
	tok_str_new(&ws);
	t = next_argstream(nested_list, &ws);
	if (t != '(') { MCC_TRACE("br\n");
		parse_flags = saved_parse_flags;
		tok_str_add2_spc(tok_str, v, 0);
		if (parse_flags & PARSE_FLAG_SPACES)
			{ MCC_TRACE("br\n"); for (i = 0; i < ws.len; i++)
				{ MCC_TRACE("br\n"); tok_str_add(tok_str, ws.str[i]); } }
		tok_str_free_str(ws.str);
		return;
	}
	tok_str_free_str(ws.str);

	tok_str_new(&args);
	parlevel = 0;
	i = 2;
	do { MCC_TRACE("br\n");
		t = next_argstream(nested_list, NULL);
	} while (t == ' ' || --i);
	while (parlevel > 0 || t != ')') { MCC_TRACE("br\n");
		if (t == TOK_EOF)
			{ MCC_TRACE("br\n"); mcc_error("EOF in invocation of macro '%s'",
								get_tok_str(v, 0)); }
		if (t == '(')
			{ MCC_TRACE("br\n"); parlevel++; }
		if (t == ')')
			{ MCC_TRACE("br\n"); parlevel--; }
		tok_str_add2_spc(&args, t, &tokc);
		t = next_argstream(nested_list, NULL);
	}
	tok_str_add(&args, TOK_EOF);
	parse_flags = saved_parse_flags;

	if (v == tok_has_builtin) { MCC_TRACE("br\n");
		c = pp_builtin_value(v, args.str);
	} else { MCC_TRACE("br\n");
		TokenString exp;
		tok_str_new(&exp);
		macro_subst(&exp, nested_list, args.str);
		tok_str_add(&exp, TOK_EOF);
		c = pp_builtin_value(v, exp.str);
		tok_str_free_str(exp.str);
	}
	tok_str_free_str(args.str);

	snprintf(buf, sizeof buf, "%d", c);
	cval.str.size = strlen(buf) + 1;
	cval.str.data = buf;
	tok_str_add2_spc(tok_str, TOK_PPNUM, &cval);
}

static int macro_subst_tok(
		TokenString *tok_str,
		Sym **nested_list,
		Sym *s) { MCC_TRACE("enter\n");
	int t;
	int v = s->v;

	PP_PRINT(("#", v, s->d));
	if (pp_builtin_macro(v) && !s->d) { MCC_TRACE("br\n");
		pp_builtin_subst(tok_str, nested_list, v);
		return 0;
	}
	if (s->d) { MCC_TRACE("br\n");
		int *mstr = s->d;
		int *jstr;
		Sym *sa;
		int ret;

		if (s->type.t & MACRO_FUNC) { MCC_TRACE("br\n");
			int saved_parse_flags = parse_flags;
			TokenString str;
			int parlevel, i;
			Sym *sa1, *args;

			parse_flags |= PARSE_FLAG_SPACES | PARSE_FLAG_LINEFEED | PARSE_FLAG_ACCEPT_STRAYS;

			tok_str_new(&str);
			t = next_argstream(nested_list, &str);
			if (t != '(') { MCC_TRACE("br\n");
				parse_flags = saved_parse_flags;
				tok_str_add2_spc(tok_str, v, 0);
				if (parse_flags & PARSE_FLAG_SPACES)
					{ MCC_TRACE("br\n"); for (i = 0; i < str.len; i++)
						{ MCC_TRACE("br\n"); tok_str_add(tok_str, str.str[i]); } }
				tok_str_free_str(str.str);
				return 0;
			} else { MCC_TRACE("br\n");
				tok_str_free_str(str.str);
			}

			args = NULL;
			sa = s->next;
			i = 2;
			for (;;) { MCC_TRACE("br\n");
				do { MCC_TRACE("br\n");
					t = next_argstream(nested_list, NULL);
				} while (t == ' ' || --i);

				if (!sa) { MCC_TRACE("br\n");
					if (t == ')')
						{ MCC_TRACE("br\n"); break; }
					mcc_error("macro '%s' used with too many args",
										get_tok_str(v, 0));
				}
			empty_arg:
				tok_str_new(&str);
				parlevel = 0;
				while (parlevel > 0 || (t != ')' && (t != ',' || sa->type.t))) { MCC_TRACE("br\n");
					if (t == TOK_EOF)
						{ MCC_TRACE("br\n"); mcc_error("EOF in invocation of macro '%s'",
											get_tok_str(v, 0)); }
					if (t == '(')
						{ MCC_TRACE("br\n"); parlevel++; }
					if (t == ')')
						{ MCC_TRACE("br\n"); parlevel--; }
					if (t == ' ')
						{ MCC_TRACE("br\n"); str.need_spc |= 1; }
					else if (t == TOK___LINE__) { MCC_TRACE("br\n");
						CValue lcv;
						char lbuf[32];
						snprintf(lbuf, sizeof(lbuf), "%lld",
												 (long long)(tok_line_file == file ? tok_line_num : file->line_num));
						lcv.str.size = strlen(lbuf) + 1;
						lcv.str.data = lbuf;
						tok_str_add2_spc(&str, TOK_PPNUM, &lcv);
					} else
						{ MCC_TRACE("br\n"); tok_str_add2_spc(&str, t, &tokc); }
					t = next_argstream(nested_list, NULL);
				}
				tok_str_add(&str, TOK_EOF);
				sa1 = sym_push2(&args, sa->v & ~SYM_FIELD, sa->type.t, 0);
				sa1->d = str.str;
				sa = sa->next;
				if (t == ')') { MCC_TRACE("br\n");
					if (!sa)
						{ MCC_TRACE("br\n"); break; }
					if (sa->type.t && gnu_ext) { MCC_TRACE("br\n");
						if (mcc_state->warn_pedantic && mcc_state->cversion < 202311) { MCC_TRACE("br\n");
							if (mcc_state->pedantic_errors)
								{ MCC_TRACE("br\n"); mcc_error("ISO C does not permit a variadic macro "
													"to be invoked with no argument for the '...'"); }
							else
								{ MCC_TRACE("br\n"); mcc_warning("ISO C does not permit a variadic macro "
														"to be invoked with no argument for the '...'"); }
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
			while (sa) { MCC_TRACE("br\n");
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
			{ MCC_TRACE("br\n"); jstr = macro_twosharps(mstr); }

		sa = sym_push2(nested_list, v, 0, 0);
		ret = macro_subst(tok_str, nested_list, jstr);
		if (sa == *nested_list)
			{ MCC_TRACE("br\n"); *nested_list = sa->prev, sym_free(sa); }

		if (jstr != mstr)
			{ MCC_TRACE("br\n"); tok_str_free_str(jstr); }
		if (mstr != s->d)
			{ MCC_TRACE("br\n"); tok_str_free_str(mstr); }
		return ret;
	} else { MCC_TRACE("br\n");
		CValue cval;
		char buf[32], *cstrval = buf;

		if (v == TOK___LINE__) { MCC_TRACE("br\n");
			long long ln = tok_line_file == file ? tok_line_num : file->line_num;
			snprintf(buf, sizeof(buf), "%lld", ln);
			t = TOK_PPNUM;
			goto add_cstr1;
		} else if (v == TOK___COUNTER__ || v == TOK___INCLUDE_LEVEL__) { MCC_TRACE("br\n");
			t = v == TOK___LINE__	 ? (tok_line_file == file ? tok_line_num
																				 : file->line_num)
					: v == TOK___COUNTER__ ? pp_counter++
																 : (int)(mcc_state->include_stack_ptr - mcc_state->include_stack);
			snprintf(buf, sizeof(buf), "%d", t);
			t = TOK_PPNUM;
			goto add_cstr1;
		} else if (v == TOK___FILE__) { MCC_TRACE("br\n");
			cstrval = file->filename;
			goto add_cstr;
		} else if (v == TOK___FILE_NAME__) { MCC_TRACE("br\n");
			/* T-mac-30153: basename of the current file (tracks includes). */
			cstrval = mcc_basename(file->filename);
			goto add_cstr;
		} else if (v == TOK___TIMESTAMP__) { MCC_TRACE("br\n");
			/* T-mac-30153: last-modification time of the current file, ctime
			 * format "Www Mmm dd hh:mm:ss yyyy"; honor SOURCE_DATE_EPOCH for
			 * reproducible builds, and fall back to gcc's placeholder when the
			 * file cannot be stat'd (e.g. <stdin>/<command line>). */
			struct stat ts_st;
			time_t ts_ti;
			struct tm *ts_tm = NULL;
			const char *sde = getenv("SOURCE_DATE_EPOCH");
			if (sde && *sde) { MCC_TRACE("br\n");
				char *end;
				long long secs = strtoll(sde, &end, 10);
				if (*end == '\0' && secs >= 0) { MCC_TRACE("br\n");
					ts_ti = (time_t)secs;
					ts_tm = gmtime(&ts_ti);
				}
			}
			if (!ts_tm && stat(file->filename, &ts_st) == 0) { MCC_TRACE("br\n");
				ts_ti = ts_st.st_mtime;
				ts_tm = localtime(&ts_ti);
			}
			if (ts_tm) { MCC_TRACE("br\n");
				static char const ts_wday[7][4] = {
						"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
				static char const ts_mon[12][4] = {
						"Jan", "Feb", "Mar", "Apr", "May", "Jun",
						"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
				snprintf(buf, sizeof(buf), "%s %s %2d %02d:%02d:%02d %d",
								 ts_wday[ts_tm->tm_wday], ts_mon[ts_tm->tm_mon], ts_tm->tm_mday,
								 ts_tm->tm_hour, ts_tm->tm_min, ts_tm->tm_sec, ts_tm->tm_year + 1900);
			} else { MCC_TRACE("br\n");
				snprintf(buf, sizeof(buf), "??? ??? ?? ??:??:?? ????");
			}
			goto add_cstr;
		} else if (v == TOK___DATE__ || v == TOK___TIME__) { MCC_TRACE("br\n");
			time_t ti;
			struct tm *tm = NULL;
			const char *sde = getenv("SOURCE_DATE_EPOCH");
			if (sde && *sde) { MCC_TRACE("br\n");
				char *end;
				long long secs = strtoll(sde, &end, 10);
				if (*end == '\0' && secs >= 0) { MCC_TRACE("br\n");
					ti = (time_t)secs;
					tm = gmtime(&ti);
				}
			}
			if (!tm) { MCC_TRACE("br\n");
				time(&ti);
				tm = localtime(&ti);
			}
			if (v == TOK___DATE__) { MCC_TRACE("br\n");
				static char const ab_month_name[12][4] = {
						"Jan", "Feb", "Mar", "Apr", "May", "Jun",
						"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
				snprintf(buf, sizeof(buf), "%s %2d %d",
								 ab_month_name[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900);
			} else { MCC_TRACE("br\n");
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

static int macro_subst_nested(
		TokenString *tok_str,
		Sym **nested_list,
		const int *macro_str);

static int macro_subst(
		TokenString *tok_str,
		Sym **nested_list,
		const int *macro_str) { MCC_TRACE("enter\n");
	int r;
	mcc_parse_depth_enter();
	r = macro_subst_nested(tok_str, nested_list, macro_str);
	mcc_parse_depth_leave();
	return r;
}

static int macro_subst_nested(
		TokenString *tok_str,
		Sym **nested_list,
		const int *macro_str) { MCC_TRACE("enter\n");
	Sym *s;
	int t, nosubst = 0;
	CValue cval;
	TokenString *str;

	int tlen = tok_str->len;
	PP_PRINT(("+expand:", 0, macro_str));

	while (1) { MCC_TRACE("br\n");
		TOK_GET(&t, &macro_str, &cval);
		if (t == 0 || t == TOK_EOF)
			{ MCC_TRACE("br\n"); break; }
		if (t >= TOK_IDENT) { MCC_TRACE("br\n");
			s = define_find(t);
			if (s == NULL || nosubst)
				{ MCC_TRACE("br\n"); goto no_subst; }
			if (sym_find2(*nested_list, t)) { MCC_TRACE("br\n");
				t |= SYM_FIELD;
				goto no_subst;
			}
			str = tok_str_alloc();
			str->str = (int *)macro_str;
			begin_macro(str, 2);
			nosubst = macro_subst_tok(tok_str, nested_list, s);
			if (macro_stack != str) { MCC_TRACE("br\n");
				break;
			}
			macro_str = macro_ptr;
			end_macro();
		} else if (t == ' ') { MCC_TRACE("br\n");
			if (parse_flags & PARSE_FLAG_SPACES)
				{ MCC_TRACE("br\n"); tok_str->need_spc |= 1; }
		} else { MCC_TRACE("br\n");
		no_subst:
			tok_str_add2_spc(tok_str, t, &cval);
			if (nosubst && t != '(')
				{ MCC_TRACE("br\n"); nosubst = 0; }
			if (t == TOK_DEFINED && pp_expr)
				{ MCC_TRACE("br\n"); nosubst = 1; }
		}
	}

	if (g_debug & MCC_DBG_PP) { MCC_TRACE("br\n");
		tok_str_add(tok_str, 0), --tok_str->len;
		PP_PRINT(("-result:", 0, tok_str->str + tlen));
	}
	return nosubst;
}

static void pragma_operator(void) { MCC_TRACE("enter\n");
	MCCState *s1 = mcc_state;
	const int *saved_macro_ptr;
	char *content;
	int n;
	const char *pragma_file;
	int pragma_line;

	next();
	if (tok != '(')
		{ MCC_TRACE("br\n"); return; }
	next();
	if (tok != TOK_STR) { MCC_TRACE("br\n");
		mcc_error("_Pragma takes a parenthesized string literal");
		while (tok != ')' && tok != TOK_EOF && tok != TOK_LINEFEED)
			{ MCC_TRACE("br\n"); next(); }
		return;
	}
	n = tokc.str.size - 1;
	content = mcc_malloc(n + 2);
	memcpy(content, tokc.str.data, n);
	content[n] = '\n';
	content[n + 1] = 0;
	next();

	saved_macro_ptr = macro_ptr;
	/* T-mac-30163: report the real source location of a macro-built _Pragma
	 * (e.g. #pragma message), not the synthetic ":pragma:" buffer name.
	 * mcc_open_bf pstrcpy's the name, so passing the real filename is safe. */
	pragma_file = file->filename;
	pragma_line = file->line_num;
	mcc_open_bf(s1, pragma_file, n + 1);
	file->line_num = pragma_line;
	memcpy(file->buffer, content, n + 1);
	macro_ptr = NULL;
	pragma_parse(s1);
	mcc_close();
	macro_ptr = saved_macro_ptr;
	mcc_free(content);
}

static int c23_keyword_subst(int t) { MCC_TRACE("enter\n");
	TokenString *str;
	CValue cv;

	if (mcc_state->cversion < 202311 || pp_expr ||
			(parse_flags & PARSE_FLAG_ASM_FILE) ||
			mcc_state->output_type == MCC_OUTPUT_PREPROCESS)
		{ MCC_TRACE("br\n"); return 0; }
	if (t == tok_c23_bool)
		{ MCC_TRACE("br\n"); tok = TOK_BOOL; return 0; }
	if (t == tok_c23_static_assert)
		{ MCC_TRACE("br\n"); tok = TOK_STATIC_ASSERT; return 0; }
	if (t == tok_c23_alignas)
		{ MCC_TRACE("br\n"); tok = TOK_ALIGNAS; return 0; }
	if (t == tok_c23_alignof)
		{ MCC_TRACE("br\n"); tok = TOK_ALIGNOF3; return 0; }
	if (t == tok_c23_thread_local)
		{ MCC_TRACE("br\n"); tok = TOK_THREAD_LOCAL; return 0; }
	if (t != tok_c23_true && t != tok_c23_false)
		{ MCC_TRACE("br\n"); return 0; }
	str = tok_str_alloc();
	tok_str_add(str, '(');
	tok_str_add(str, '(');
	tok_str_add(str, TOK_BOOL);
	tok_str_add(str, ')');
	cv.i = t == tok_c23_true;
	tok_str_add2(str, TOK_CINT, &cv);
	tok_str_add(str, ')');
	tok_str_add(str, 0);
	begin_macro(str, 1);
	return 1;
}

ST_FUNC void next(void) { MCC_TRACE("enter\n");
	int t;
tail:
	total_toks++;
	while (macro_ptr) { MCC_TRACE("br\n");
	redo:
		t = *macro_ptr;
		if (TOK_HAS_VALUE(t)) { MCC_TRACE("br\n");
			tok_get(&tok, &macro_ptr, &tokc);
			if (t == TOK_LINENUM) { MCC_TRACE("br\n");
				file->line_num = tok_line_num = tokc.i;
				tok_line_file = file;
				goto redo;
			}
			goto convert;
		} else if (t == 0) { MCC_TRACE("br\n");
			end_macro();
			continue;
		} else if (t == TOK_EOF) { MCC_TRACE("br\n");
		} else { MCC_TRACE("br\n");
			++macro_ptr;
			t &= ~SYM_FIELD;
			if (t == '\\') { MCC_TRACE("br\n");
				if (!(parse_flags & PARSE_FLAG_ACCEPT_STRAYS))
					{ MCC_TRACE("br\n"); mcc_error("stray '\\' in program"); }
			}
		}
		tok = t;
		if (TOK_IS_DIGRAPH(t) && mcc_state->output_type != MCC_OUTPUT_PREPROCESS)
			{ MCC_TRACE("br\n"); tok = digraph_primary(t); }
		if (t == TOK__Pragma && (parse_flags & PARSE_FLAG_PREPROCESS) && mcc_state->output_type != MCC_OUTPUT_PREPROCESS) { MCC_TRACE("br\n");
			pragma_operator();
			goto redo;
		}
		if (t >= TOK_IDENT && (parse_flags & PARSE_FLAG_PREPROCESS) && c23_keyword_subst(t))
			{ MCC_TRACE("br\n"); goto redo; }
		return;
	}

	next_nomacro();
	t = tok;
	if (t == TOK__Pragma && (parse_flags & PARSE_FLAG_PREPROCESS) && mcc_state->output_type != MCC_OUTPUT_PREPROCESS) { MCC_TRACE("br\n");
		pragma_operator();
		goto tail;
	}
	if (t >= TOK_IDENT && (parse_flags & PARSE_FLAG_PREPROCESS)) { MCC_TRACE("br\n");
		Sym *s = tok_ts->sym_define;
		if (s) { MCC_TRACE("br\n");
			Sym *nested_list = NULL;
			uint32_t cst_mfirst = cst_mark();
			uint32_t cst_mbefore = cst_leafcount();
			macro_subst_tok(&tokstr_buf, &nested_list, s);
			tok_str_add(&tokstr_buf, 0);
			begin_macro(&tokstr_buf, 0);
			if (file == cst_main_bf) { MCC_TRACE("br\n");
				uint32_t cst_mafter = cst_leafcount();
				uint32_t cst_mlast = cst_mafter > cst_mbefore
																 ? cst_mafter - 1
																 : cst_mbefore;
				cst_hook_wrap(CST_MacroInvocation, cst_mfirst, cst_mlast);
			}
			goto redo;
		}
		if (c23_keyword_subst(t))
			{ MCC_TRACE("br\n"); goto redo; }
		return;
	}

convert:
	if (TOK_IS_DIGRAPH(t) && mcc_state->output_type != MCC_OUTPUT_PREPROCESS)
		{ MCC_TRACE("br\n"); tok = digraph_primary(t); }
	if (t == TOK_PPNUM) { MCC_TRACE("br\n");
		if (parse_flags & PARSE_FLAG_TOK_NUM)
			{ MCC_TRACE("br\n"); parse_number(tokc.str.data); }
	} else if (t == TOK_PPSTR) { MCC_TRACE("br\n");
		if (parse_flags & PARSE_FLAG_TOK_STR)
			{ MCC_TRACE("br\n"); parse_string(tokc.str.data, tokc.str.size - 1); }
	}
}

ST_INLN void unget_tok(int last_tok) { MCC_TRACE("enter\n");
	TokenString *str = &unget_buf;
	int alloc = 0;
	if (str->len)
		{ MCC_TRACE("br\n"); str = tok_str_alloc(), alloc = 1; }
	if (tok != TOK_EOF)
		{ MCC_TRACE("br\n"); tok_str_add2(str, tok, &tokc); }
	tok_str_add(str, 0);
	begin_macro(str, alloc);
	tok = last_tok;
}

static const char *const target_os_defs =
#ifdef MCC_TARGET_PE
		"_WIN32\0"
#if MCC_PTR_SIZE == 8
		"_WIN64\0"
#endif
#else
#if defined MCC_TARGET_MACHO
		"__APPLE__\0"
#elif MCC_TARGETOS_FreeBSD
		"__FreeBSD__ 12\0"
#elif MCC_TARGETOS_FreeBSD_kernel
		"__FreeBSD_kernel__\0"
#elif MCC_TARGETOS_NetBSD
		"__NetBSD__\0"
#elif MCC_TARGETOS_OpenBSD
		"__OpenBSD__\0"
#else
		"__linux__\0"
		"__linux\0"
#if MCC_TARGETOS_ANDROID
		"__ANDROID__\0"
#endif
#endif
		"__unix__\0"
		"__unix\0"
#endif
		;

#ifdef MCC_TARGET_X86_64
static const char *const target_feature_defs =
		"__SSE__\0"
		"__SSE2__\0"
		"__SSE_MATH__\0"
		"__SSE2_MATH__\0";
#endif

static void putdef(CString *cs, const char *p) { MCC_TRACE("enter\n");
	cstr_printf(cs, "#define %s%s\n", p, &" 1"[!!strchr(p, ' ') * 2]);
}

static void putdefs(CString *cs, const char *p) { MCC_TRACE("enter\n");
	while (*p)
		{ MCC_TRACE("br\n"); putdef(cs, p), p = strchr(p, 0) + 1; }
}

static void mcc_predefs(MCCState *s1, CString *cs, int is_asm) { MCC_TRACE("enter\n");
	{
		int _maj = MCC_VERSION_MAJOR;
		int _min = MCC_VERSION_MINOR;
		cstr_printf(cs, "#define __MCC__ %d\n", _maj);
		cstr_printf(cs, "#define __MCC_MINOR__ %d\n", _min);
		cstr_printf(cs, "#define __TINYC__ %d\n", _maj);
	}
	cstr_printf(cs, "#define __GNUC__ %d\n", MCC_GNUC_MAJOR);
	cstr_printf(cs, "#define __GNUC_MINOR__ %d\n", MCC_GNUC_MINOR);
	cstr_printf(cs, "#define __GNUC_PATCHLEVEL__ %d\n", MCC_GNUC_PATCHLEVEL);
	if (gnu89_inline_semantics(s1))
		{ MCC_TRACE("br\n"); putdef(cs, "__GNUC_GNU_INLINE__"); }
	else
		{ MCC_TRACE("br\n"); putdef(cs, "__GNUC_STDC_INLINE__"); }
	putdefs(cs, target_machine_defs);
	putdefs(cs, target_os_defs);
#ifdef MCC_TARGET_X86_64
	putdefs(cs, target_feature_defs);
#endif

#ifdef MCC_TARGET_ARM
	if (s1->float_abi == ARM_HARD_FLOAT)
		{ MCC_TRACE("br\n"); putdef(cs, "__ARM_PCS_VFP"); }
#endif
	if (is_asm)
		{ MCC_TRACE("br\n"); putdef(cs, "__ASSEMBLER__"); }
	putdef(cs, "__MCC_ASM__");
	if (s1->output_type == MCC_OUTPUT_PREPROCESS)
		{ MCC_TRACE("br\n"); putdef(cs, "__MCC_PP__"); }
	if (s1->output_type == MCC_OUTPUT_MEMORY)
		{ MCC_TRACE("br\n"); putdef(cs, "__MCC_RUN__"); }
	if (s1->do_backtrace)
		{ MCC_TRACE("br\n"); putdef(cs, "__MCC_BACKTRACE__"); }
	if (s1->do_bounds_check)
		{ MCC_TRACE("br\n"); putdef(cs, "__MCC_BCHECK__"); }
	if (s1->do_sanitize_undefined)
		{ MCC_TRACE("br\n"); putdef(cs, "__MCC_SANITIZE_UNDEFINED__"); }
	if (s1->do_sanitize_address)
		{ MCC_TRACE("br\n"); putdef(cs, "__SANITIZE_ADDRESS__"); }
	if (s1->char_is_unsigned)
		{ MCC_TRACE("br\n"); putdef(cs, "__CHAR_UNSIGNED__"); }
	if (ast_math_errno_folds(s1))
		{ MCC_TRACE("br\n"); putdef(cs, "__NO_MATH_ERRNO__"); }
	if (s1->fast_math)
		{ MCC_TRACE("br\n"); putdef(cs, "__FAST_MATH__"); }
	if (s1->optimize > 0)
		{ MCC_TRACE("br\n"); putdef(cs, "__OPTIMIZE__"); }
	else
		{ MCC_TRACE("br\n"); putdef(cs, "__NO_INLINE__"); }
	if (s1->optimize_size)
		{ MCC_TRACE("br\n"); putdef(cs, "__OPTIMIZE_SIZE__"); }
	if (s1->option_pthread)
		{ MCC_TRACE("br\n"); putdef(cs, "_REENTRANT"); }
	if (s1->leading_underscore)
		{ MCC_TRACE("br\n"); putdef(cs, "__leading_underscore"); }
	if (s1->leading_underscore)
		{ MCC_TRACE("br\n"); cstr_printf(cs, "#define __USER_LABEL_PREFIX__ _\n"); }
	else
		{ MCC_TRACE("br\n"); cstr_printf(cs, "#define __USER_LABEL_PREFIX__\n"); }
	cstr_printf(cs, "#define __SIZEOF_POINTER__ %d\n", MCC_PTR_SIZE);
	cstr_printf(cs, "#define __SIZEOF_LONG__ %d\n", LONG_SIZE);
	cstr_printf(cs, "#define __SIZEOF_SHORT__ 2\n");
	cstr_printf(cs, "#define __SIZEOF_FLOAT__ 4\n");
	cstr_printf(cs, "#define __SIZEOF_FLOAT16__ 2\n");
	cstr_printf(cs, "#define __SIZEOF_DOUBLE__ 8\n");
#ifdef MCC_USING_DOUBLE_FOR_LDOUBLE
	cstr_printf(cs, "#define __SIZEOF_LONG_DOUBLE__ %d\n", 8);
#else
	cstr_printf(cs, "#define __SIZEOF_LONG_DOUBLE__ %d\n", MCC_LDOUBLE_SIZE);
#endif
	cstr_printf(cs, "#define __SIZEOF_SIZE_T__ %d\n", MCC_PTR_SIZE);
	cstr_printf(cs, "#define __SIZEOF_PTRDIFF_T__ %d\n", MCC_PTR_SIZE);
	cstr_printf(cs, "#define __SIZEOF_WCHAR_T__ %d\n", (int)sizeof(nwchar_t));
#ifdef MCC_TARGET_PE
	cstr_printf(cs, "#define __SIZEOF_WINT_T__ 2\n");
#else
	cstr_printf(cs, "#define __SIZEOF_WINT_T__ 4\n");
#endif
	cstr_printf(cs, "#define __SIZEOF_INT256__ %d\n", MCC_WIDE256_SIZE);
	/* C23 _BitInt: the widest _BitInt this implementation supports.  Slice 1
	 * implements N<=64 (single storage integer); slice 2 implements 64<N<=128
	 * (2-limb 16-byte struct), 128 < N <= 256 (4-limb 32-byte struct, __int256
	 * kernel), and 256 < N <= 512 (8-limb 64-byte struct, __int512 kernel).
	 * Wider _BitInt is refused, so the macro reports 512 rather than gcc's 65535. */
	cstr_printf(cs, "#define __BITINT_MAXWIDTH__ 512\n");
#if MCC_HAVE_INT128
	cstr_printf(cs, "#define __SIZEOF_INT128__ 16\n");
#endif
	cstr_printf(cs, "#define __SCHAR_WIDTH__ 8\n");
	cstr_printf(cs, "#define __SHRT_WIDTH__ 16\n");
	cstr_printf(cs, "#define __INT_WIDTH__ 32\n");
	cstr_printf(cs, "#define __LONG_WIDTH__ %d\n", LONG_SIZE * 8);
	cstr_printf(cs, "#define __LONG_LONG_WIDTH__ 64\n");
	cstr_printf(cs, "#define __INTMAX_WIDTH__ 64\n");
	cstr_printf(cs, "#define __WCHAR_WIDTH__ %d\n", (int)sizeof(nwchar_t) * 8);
	cstr_printf(cs, "#define __SIG_ATOMIC_WIDTH__ 32\n");
	cstr_printf(cs, "#define __PTRDIFF_WIDTH__ %d\n", MCC_PTR_SIZE * 8);
	cstr_printf(cs, "#define __SIZE_WIDTH__ %d\n", MCC_PTR_SIZE * 8);
	cstr_printf(cs, "#define __INTPTR_WIDTH__ %d\n", MCC_PTR_SIZE * 8);
	cstr_printf(cs, "#define __INT_LEAST8_WIDTH__ 8\n");
	cstr_printf(cs, "#define __INT_LEAST16_WIDTH__ 16\n");
	cstr_printf(cs, "#define __INT_LEAST32_WIDTH__ 32\n");
	cstr_printf(cs, "#define __INT_LEAST64_WIDTH__ 64\n");
	cstr_printf(cs, "#define __INT_FAST8_WIDTH__ 8\n");
	cstr_printf(cs, "#define __INT_FAST16_WIDTH__ 32\n");
	cstr_printf(cs, "#define __INT_FAST32_WIDTH__ 32\n");
	cstr_printf(cs, "#define __INT_FAST64_WIDTH__ 64\n");
#ifdef MCC_TARGET_PE
	cstr_printf(cs, "#define __WINT_WIDTH__ 16\n");
#else
	cstr_printf(cs, "#define __WINT_WIDTH__ 32\n");
#endif
	if (LONG_SIZE == 8)
		{ MCC_TRACE("br\n"); cstr_printf(cs, "#define __INTPTR_MAX__ 0x7fffffffffffffffL\n");
			cstr_printf(cs, "#define __UINTPTR_MAX__ 0xffffffffffffffffUL\n"); }
	else if (MCC_PTR_SIZE == 8)
		{ MCC_TRACE("br\n"); cstr_printf(cs, "#define __INTPTR_MAX__ 0x7fffffffffffffffLL\n");
			cstr_printf(cs, "#define __UINTPTR_MAX__ 0xffffffffffffffffULL\n"); }
	else
		{ MCC_TRACE("br\n"); cstr_printf(cs, "#define __INTPTR_MAX__ 0x7fffffffL\n");
			cstr_printf(cs, "#define __UINTPTR_MAX__ 0xffffffffUL\n"); }
	if (!is_asm) { MCC_TRACE("br\n");
		putdef(cs, "__STDC__");
		if (s1->std_strict_ansi)
			{ MCC_TRACE("br\n"); putdef(cs, "__STRICT_ANSI__"); }
		cstr_printf(cs, "#define __STDC_HOSTED__ %d\n",
								(s1->nostdlib || s1->freestanding) ? 0 : 1);
		if (s1->cversion)
			{ MCC_TRACE("br\n"); cstr_printf(cs, "#define __STDC_VERSION__ %dL\n", s1->cversion); }
		cstr_printf(cs, "#define __STDC_EMBED_NOT_FOUND__ 0\n");
		cstr_printf(cs, "#define __STDC_EMBED_FOUND__ 1\n");
		cstr_printf(cs, "#define __STDC_EMBED_EMPTY__ 2\n");
		cstr_cat(cs,
#if MCC_CONFIG_PREDEFS
#include "mccdefs_.h"

#else
						 "#include <mccdefs.h>\n"
#endif
						 ,
						 -1);
	}
	cstr_printf(cs, "#define __BASE_FILE__ \"%s\"\n", file->filename);
}

ST_FUNC void preprocess_start(MCCState *s1, int filetype) { MCC_TRACE("enter\n");
	int is_asm = !!(filetype & (AFF_TYPE_ASM | AFF_TYPE_ASMPP));

	mccpp_new(s1);

	s1->include_stack_ptr = s1->include_stack;
	s1->ifdef_stack_ptr = s1->ifdef_stack;
	file->ifdef_stack_ptr = s1->ifdef_stack_ptr;
	mcc_parse_depth = 0;
	pp_directive_depth = 0;
	pp_comment_ready = 0;
	pp_expr = 0;
	pp_counter = 0;
	pp_diag_stack_n = 0;
	pp_debug_tok = pp_debug_symv = 0;
	pp_cmdline_user = 0;
	s1->pack_stack[0] = 0;
	s1->pack_stack_ptr = s1->pack_stack;

	set_idnum('$', s1->dollars_in_identifiers ? IS_ID : 0);
	set_idnum('.', is_asm ? IS_ID : 0);

	if (!(filetype & AFF_TYPE_ASM)) { MCC_TRACE("br\n");
		CString cstr;
		cstr_new(&cstr);
		mcc_predefs(s1, &cstr, is_asm);
		cstr_cat(&cstr, "#pragma __mcc_cmdline_defs__\n", -1);
		if (s1->cmdline_defs.size)
			{ MCC_TRACE("br\n"); cstr_cat(&cstr, s1->cmdline_defs.data, s1->cmdline_defs.size); }
		if (!is_asm)
			{ MCC_TRACE("br\n"); cstr_cat(&cstr, "#undef _FORTIFY_SOURCE\n#define _FORTIFY_SOURCE 0\n", -1); }
		if (s1->cmdline_imacros.size) { MCC_TRACE("br\n");
			cstr_cat(&cstr, s1->cmdline_imacros.data, s1->cmdline_imacros.size);
			cstr_cat(&cstr, "#undef __mcc_imacros_end__\n__mcc_imacros_end__\n", -1);
		}
		if (s1->cmdline_incl.size)
			{ MCC_TRACE("br\n"); cstr_cat(&cstr, s1->cmdline_incl.data, s1->cmdline_incl.size); }
		*s1->include_stack_ptr++ = file;
		mcc_open_bf(s1, "<command line>", cstr.size);
		file->system_header = 1;
		memcpy(file->buffer, cstr.data, cstr.size);
		cstr_free(&cstr);
	}
	parse_flags = is_asm ? PARSE_FLAG_ASM_FILE : 0;
}

ST_FUNC void mccpp_run_imacros(MCCState *s1) { MCC_TRACE("enter\n");
	int btok, saved_flags;
	if (!s1->cmdline_imacros.size)
		{ MCC_TRACE("br\n"); return; }
	btok = tok_alloc("__mcc_imacros_end__", 19)->tok;
	saved_flags = parse_flags;
	parse_flags = PARSE_FLAG_PREPROCESS | PARSE_FLAG_TOK_NUM | PARSE_FLAG_TOK_STR;
	do { MCC_TRACE("br\n"); next(); }
	while (tok != btok && tok != TOK_EOF);
	parse_flags = saved_flags;
}

ST_FUNC void preprocess_end(MCCState *s1) { MCC_TRACE("enter\n");
	while (macro_stack)
		{ MCC_TRACE("br\n"); end_macro(); }
	macro_ptr = NULL;
	while (file)
		{ MCC_TRACE("br\n"); mcc_close(); }
	mccpp_delete(s1);
}

ST_FUNC int set_idnum(int c, int val) { MCC_TRACE("enter\n");
	int prev = isidnum_table[c - CH_EOF];
	isidnum_table[c - CH_EOF] = val;
	return prev;
}

ST_FUNC void mccpp_new(MCCState *s) { MCC_TRACE("enter\n");
	int c;
	const char *p, *r;

	for (int i = CH_EOF; i < 128; i++)
		{ MCC_TRACE("br\n"); set_idnum(i,
							is_space(i)
									? IS_SPC
							: isid(i)
									? IS_ID
							: isnum(i)
									? IS_NUM
									: 0); }

	for (int i = 128; i < 256; i++)
		{ MCC_TRACE("br\n"); set_idnum(i, IS_ID); }

	tal_new(&toksym_alloc, TOKSYM_TAL_SIZE);
	tal_new(&tokstr_alloc, TOKSTR_TAL_SIZE);

	memset(hash_ident, 0, TOK_HASH_SIZE * sizeof(TokenSym *));
	memset(s->cached_includes_hash, 0, sizeof s->cached_includes_hash);

	cstr_new(&tokcstr);
	cstr_new(&pp_comment_text);
	cstr_new(&cstr_buf);
	cstr_realloc(&cstr_buf, STRING_MAX_SIZE);
	tok_str_new(&unget_buf);
	tok_str_realloc(&unget_buf, TOKSTR_MAX_SIZE);
	tok_str_new(&tokstr_buf);
	tok_str_realloc(&tokstr_buf, TOKSTR_MAX_SIZE);

	tok_ident = TOK_IDENT;
	p = mcc_keywords;
	while (*p) { MCC_TRACE("br\n");
		r = p;
		for (;;) { MCC_TRACE("br\n");
			c = *r++;
			if (c == '\0')
				{ MCC_TRACE("br\n"); break; }
		}
		tok_alloc(p, r - p - 1);
		p = r;
	}

	tok_va_opt = tok_alloc_const("__VA_OPT__");

	tok_has_attribute = tok_alloc_const("__has_attribute");
	tok_has_c_attribute = tok_alloc_const("__has_c_attribute");
	tok_has_cpp_attribute = tok_alloc_const("__has_cpp_attribute");
	tok_has_builtin = tok_alloc_const("__has_builtin");
	tok_has_feature = tok_alloc_const("__has_feature");
	tok_has_extension = tok_alloc_const("__has_extension");

	tok_c23_bool = tok_alloc_const("bool");
	tok_c23_true = tok_alloc_const("true");
	tok_c23_false = tok_alloc_const("false");
	tok_c23_static_assert = tok_alloc_const("static_assert");
	tok_c23_alignas = tok_alloc_const("alignas");
	tok_c23_alignof = tok_alloc_const("alignof");
	tok_c23_thread_local = tok_alloc_const("thread_local");

	define_push(TOK___LINE__, MACRO_OBJ, NULL, NULL);
	define_push(TOK___FILE__, MACRO_OBJ, NULL, NULL);
	define_push(TOK___DATE__, MACRO_OBJ, NULL, NULL);
	define_push(TOK___TIME__, MACRO_OBJ, NULL, NULL);
	define_push(TOK___COUNTER__, MACRO_OBJ, NULL, NULL);
	define_push(TOK___INCLUDE_LEVEL__, MACRO_OBJ, NULL, NULL);
	define_push(TOK___FILE_NAME__, MACRO_OBJ, NULL, NULL);
	define_push(TOK___TIMESTAMP__, MACRO_OBJ, NULL, NULL);
	define_push(tok_has_attribute, MACRO_FUNC, NULL, NULL);
	define_push(tok_has_c_attribute, MACRO_FUNC, NULL, NULL);
	define_push(tok_has_cpp_attribute, MACRO_FUNC, NULL, NULL);
	define_push(tok_has_builtin, MACRO_FUNC, NULL, NULL);
	define_push(tok_has_feature, MACRO_FUNC, NULL, NULL);
	define_push(tok_has_extension, MACRO_FUNC, NULL, NULL);
}

ST_FUNC void mccpp_delete(MCCState *s) { MCC_TRACE("enter\n");
	int n;

	dynarray_reset(&s->cached_includes, &s->nb_cached_includes);
	free_assertions(s);

	n = tok_ident - TOK_IDENT;
	if (n > total_idents)
		{ MCC_TRACE("br\n"); total_idents = n; }
	for (int i = n; --i >= 0;)
		{ MCC_TRACE("br\n"); tal_free(&toksym_alloc, table_ident[i]); }
	mcc_free(table_ident);
	table_ident = NULL;

	cstr_free(&tokcstr);
	cstr_free(&pp_comment_text);
	cstr_free(&cstr_buf);
	tok_str_free_str(tokstr_buf.str);
	tok_str_free_str(unget_buf.str);

	tal_delete(&toksym_alloc);
	tal_delete(&tokstr_alloc);
}

static int pp_need_space(int a, int b);

static void tok_print(const int *str, const char *msg, ...) { MCC_TRACE("enter\n");
	FILE *fp = mcc_state->ppfp;
	va_list ap;
	int t, t0, s;
	CValue cval;

	va_start(ap, msg);
	vfprintf(fp, msg, ap);
	va_end(ap);

	s = t0 = 0;
	while (str) { MCC_TRACE("br\n");
		TOK_GET(&t, &str, &cval);
		if (t == 0 || t == TOK_EOF)
			{ MCC_TRACE("br\n"); break; }
		if (pp_need_space(t0, t))
			{ MCC_TRACE("br\n"); s = 0; }
		fprintf(fp, &" %s"[s], t == TOK_PLCHLDR ? "<>" : get_tok_str(t, &cval));
		s = 1, t0 = t;
	}
	fprintf(fp, "\n");
}

static void pp_line(MCCState *s1, BufferedFile *f, int level) { MCC_TRACE("enter\n");
	int d = f->line_num - f->line_ref;
	const char *fn = f->filename;

	if (0 == strcmp(fn, "<command line>"))
		{ MCC_TRACE("br\n"); fn = "<command-line>"; }

	if (s1->dflag & 4)
		{ MCC_TRACE("br\n"); return; }

	if (s1->Pflag == LINE_MACRO_OUTPUT_FORMAT_NONE) { MCC_TRACE("br\n");
		;
	} else if (level == 0 && f->line_ref && d < 8) { MCC_TRACE("br\n");
		while (d > 0)
			{ MCC_TRACE("br\n"); fputs("\n", s1->ppfp), --d; }
	} else if (s1->Pflag == LINE_MACRO_OUTPUT_FORMAT_STD) { MCC_TRACE("br\n");
		fprintf(s1->ppfp, "#line %d \"%s\"\n", f->line_num, fn);
	} else { MCC_TRACE("br\n");
		fprintf(s1->ppfp, "# %d \"%s\"%s%s\n", f->line_num, fn,
						level > 0
								? " 1"
						: level < 0
								? " 2"
								: "",
						(f->system_header && fn[0] != '<') ? " 3" : "");
	}
	f->line_ref = f->line_num;
}

static void define_print(MCCState *s1, int v) { MCC_TRACE("enter\n");
	FILE *fp;
	Sym *s;

	s = define_find(v);
	if (NULL == s || NULL == s->d)
		{ MCC_TRACE("br\n"); return; }

	fp = s1->ppfp;
	fprintf(fp, "#define %s", get_tok_str(v, NULL));
	if (s->type.t & MACRO_FUNC) { MCC_TRACE("br\n");
		Sym *a = s->next;
		fprintf(fp, "(");
		if (a)
			{ MCC_TRACE("br\n"); for (;;) { MCC_TRACE("br\n");
				fprintf(fp, "%s", get_tok_str(a->v, NULL));
				if (!(a = a->next))
					{ MCC_TRACE("br\n"); break; }
				fprintf(fp, ",");
			} }
		fprintf(fp, ")");
	}
	tok_print(s->d, "");
}

static void pp_write_toks(FILE *f, const int *str) { MCC_TRACE("enter\n");
	int t, t0 = 0, sp = 1;
	CValue cval;
	while (str) { MCC_TRACE("br\n");
		TOK_GET(&t, &str, &cval);
		if (t == 0 || t == TOK_EOF)
			{ MCC_TRACE("br\n"); break; }
		if (pp_need_space(t0, t))
			{ MCC_TRACE("br\n"); sp = 0; }
		fprintf(f, &" %s"[sp], t == TOK_PLCHLDR ? "" : get_tok_str(t, &cval));
		sp = 1, t0 = t;
	}
}

static void pp_write_define(FILE *f, Sym *s) { MCC_TRACE("enter\n");
	fprintf(f, "#define %s", get_tok_str(s->v, NULL));
	if (s->type.t & MACRO_FUNC) { MCC_TRACE("br\n");
		Sym *a = s->next;
		fprintf(f, "(");
		if (a)
			{ MCC_TRACE("br\n"); for (;;) { MCC_TRACE("br\n");
				fprintf(f, "%s", get_tok_str(a->v, NULL));
				if (!(a = a->next))
					{ MCC_TRACE("br\n"); break; }
				fprintf(f, ",");
			} }
		fprintf(f, ")");
	}
	fprintf(f, " ");
	pp_write_toks(f, s->d);
	fprintf(f, "\n");
}

ST_FUNC int pp_macro_is_func(int v) { MCC_TRACE("enter\n");
	Sym *s = define_find(v);
	return s && s->d && (s->type.t & MACRO_FUNC);
}

ST_FUNC int pp_macro_eval(int v, const int64_t *args, int nargs, int64_t *res) { MCC_TRACE("enter\n");
	Sym *m = define_find(v), *a, *d, **older = NULL;
	int n, i, nolder = 0, ret = -1;
	FILE *f;
	char path[1024], exe[1024], barg[1024];
	char *out = NULL, *end;
	const char *name = get_tok_str(v, NULL);
	const char *argv[6];

	if (!m || !m->d || !(m->type.t & MACRO_FUNC))
		{ MCC_TRACE("br\n"); return -1; }
	for (n = 0, a = m->next; a; a = a->next, n++)
		{ MCC_TRACE("br\n"); if (a->type.t)
			{ MCC_TRACE("br\n"); return -1; } }
	if (n != nargs)
		{ MCC_TRACE("br\n"); return -1; }
	if (host_exe_path(exe, sizeof exe) < 0)
		{ MCC_TRACE("br\n"); return -1; }
	f = host_temp_c_file(path, sizeof path);
	if (!f)
		{ MCC_TRACE("br\n"); return -1; }

	for (d = define_stack; d; d = d->prev)
		{ MCC_TRACE("br\n"); if (d->d && !(d->v & SYM_FIELD) && d->v != v && d->v >= TOK_IDENT &&
				!is_predef_macro(d->v))
			{ MCC_TRACE("br\n"); dynarray_add((void ***)&older, &nolder, d); } }
	for (i = nolder; i-- > 0;)
		{ MCC_TRACE("br\n"); pp_write_define(f, older[i]); }
	mcc_free(older);

	fprintf(f, "static long long %s(", name);
	for (i = 0, a = m->next; a; a = a->next, i++)
		{ MCC_TRACE("br\n"); fprintf(f, "%slong long %s", i ? ", " : "",
						get_tok_str(a->v, NULL)); }
	fprintf(f, "%s) {\n\treturn (", n ? "" : "void");
	pp_write_toks(f, m->d);
	fprintf(f, ");\n}\nextern int printf(const char *, ...);\n"
						 "int main(void) {\n\tprintf(\"%%lld\", %s(", name);
	for (i = 0; i < nargs; i++)
		{ MCC_TRACE("br\n"); fprintf(f, "%s%lldLL", i ? ", " : "", (long long)args[i]); }
	fprintf(f, "));\n\treturn 0;\n}\n");
	fclose(f);

	snprintf(barg, sizeof barg, "-B%s",
					 mcc_state->mcc_lib_path ? mcc_state->mcc_lib_path : ".");
	argv[0] = exe;
	argv[1] = barg;
	argv[2] = "-w";
	argv[3] = "-run";
	argv[4] = path;
	argv[5] = NULL;
	{
		HostSpawnOpts o;
		memset(&o, 0, sizeof o);
		o.stdout_buf = &out;
		if (host_spawn_ex(argv, &o) == 0 && out && *out) { MCC_TRACE("br\n");
			*res = strtoll(out, &end, 10);
			if (end != out)
				{ MCC_TRACE("br\n"); ret = 0; }
		}
	}
	mcc_free(out);
	unlink(path);
	return ret;
}

static void pp_debug_defines(MCCState *s1) { MCC_TRACE("enter\n");
	int v, t;
	const char *vs;
	FILE *fp;

	t = pp_debug_tok;
	if (t == 0)
		{ MCC_TRACE("br\n"); return; }

	file->line_num--;
	pp_line(s1, file, 0);
	file->line_ref = ++file->line_num;

	fp = s1->ppfp;
	v = pp_debug_symv;
	vs = get_tok_str(v, NULL);
	if (t == TOK_DEFINE) { MCC_TRACE("br\n");
		define_print(s1, v);
	} else if (t == TOK_UNDEF) { MCC_TRACE("br\n");
		fprintf(fp, "#undef %s\n", vs);
	} else if (t == TOK_push_macro) { MCC_TRACE("br\n");
		fprintf(fp, "#pragma push_macro(\"%s\")\n", vs);
	} else if (t == TOK_pop_macro) { MCC_TRACE("br\n");
		fprintf(fp, "#pragma pop_macro(\"%s\")\n", vs);
	}
	pp_debug_tok = 0;
}

static int pp_need_space(int a, int b) { MCC_TRACE("enter\n");
	int fb;

	if ('E' == a)
		{ MCC_TRACE("br\n"); return '+' == b || '-' == b; }
	if (a >= TOK_IDENT || a == TOK_PPNUM)
		{ MCC_TRACE("br\n"); return b >= TOK_IDENT || b == TOK_PPNUM; }

	/* Operator/punctuator paste avoidance: inserting a space is required
	 * whenever the last character of token a followed by the first character
	 * of token b would re-lex as a longer pp-token (worst case the block/line
	 * comment introducers '/' '*' and '/' '/'). a here is a single-character
	 * punctuator; fb is the first character of b's spelling. */
	if (b > ' ' && b < 127)
		{ MCC_TRACE("br\n"); fb = b; }
	else if (b == TOK_INC || b == TOK_A_ADD)
		{ MCC_TRACE("br\n"); fb = '+'; }
	else if (b == TOK_DEC || b == TOK_A_SUB || b == TOK_ARROW)
		{ MCC_TRACE("br\n"); fb = '-'; }
	else if (b == TOK_LT)
		{ MCC_TRACE("br\n"); fb = '<'; }
	else if (b == TOK_GT)
		{ MCC_TRACE("br\n"); fb = '>'; }
	else
		{ MCC_TRACE("br\n"); fb = 0; }

	switch (a) { MCC_TRACE("br\n");
	case '/': return fb == '/' || fb == '*' || fb == '=';
	case TOK_LT: return fb == '<' || fb == '=' || fb == ':' || fb == '%';
	case TOK_GT: return fb == '>' || fb == '=';
	case '+': return fb == '+' || fb == '=';
	case '-': return fb == '-' || fb == '=' || fb == '>';
	case '*': return fb == '=';
	case '%': return fb == '=' || fb == '>' || fb == ':';
	case '^': return fb == '=';
	case '!': return fb == '=';
	case '=': return fb == '=';
	case '&': return fb == '&' || fb == '=';
	case '|': return fb == '|' || fb == '=';
	case ':': return fb == '>' || fb == ':';
	case '#': return fb == '#';
	case '.': return fb == '.' || (fb >= '0' && fb <= '9') || b == TOK_PPNUM;
	default: return 0;
	}
}

static int pp_check_he0xE(int t, const char *p) { MCC_TRACE("enter\n");
	if (t == TOK_PPNUM && toup(strchr(p, 0)[-1]) == 'E')
		{ MCC_TRACE("br\n"); return 'E'; }
	return t;
}

static void pp_pragma_operator(MCCState *s1, int *ptoken_seen) { MCC_TRACE("enter\n");
	const char *raw;
	char *content, *q;

	next();
	if (tok != '(')
		{ MCC_TRACE("br\n"); return; }
	next();
	if (tok != TOK_PPSTR && tok != TOK_STR) { MCC_TRACE("br\n");
		while (tok != ')' && tok != TOK_EOF && tok != TOK_LINEFEED)
			{ MCC_TRACE("br\n"); next(); }
		return;
	}
	raw = get_tok_str(tok, &tokc);
	while (*raw && *raw != '"')
		{ MCC_TRACE("br\n"); raw++; }
	if (*raw == '"')
		{ MCC_TRACE("br\n"); raw++; }
	content = mcc_malloc(strlen(raw) + 1);
	q = content;
	while (*raw && *raw != '"') { MCC_TRACE("br\n");
		if (*raw == '\\' && (raw[1] == '"' || raw[1] == '\\'))
			{ MCC_TRACE("br\n"); raw++; }
		*q++ = *raw++;
	}
	*q = 0;
	next();

	{ MCC_TRACE("br\n");
		const char *cc = content;
		int mut = 0;
		char b;
		while (*cc == ' ' || *cc == '\t')
			{ MCC_TRACE("br\n"); cc++; }
		if (!strncmp(cc, "push_macro", 10)) { MCC_TRACE("br\n");
			b = cc[10];
			if (!((b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') ||
					(b >= '0' && b <= '9') || b == '_'))
				{ MCC_TRACE("br\n"); mut = 1; }
		} else if (!strncmp(cc, "pop_macro", 9)) { MCC_TRACE("br\n");
			b = cc[9];
			if (!((b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') ||
					(b >= '0' && b <= '9') || b == '_'))
				{ MCC_TRACE("br\n"); mut = 1; }
		}
		if (mut) { MCC_TRACE("br\n");
			const int *saved_macro_ptr = macro_ptr;
			int saved_parse_flags = parse_flags;
			int n = (int)strlen(content);
			mcc_open_bf(s1, ":pragma:", n + 1);
			memcpy(file->buffer, content, n);
			file->buffer[n] = '\n';
			macro_ptr = NULL;
			parse_flags = PARSE_FLAG_PREPROCESS | PARSE_FLAG_TOK_NUM | PARSE_FLAG_TOK_STR |
					PARSE_FLAG_LINEFEED | (saved_parse_flags & PARSE_FLAG_ASM_FILE);
			pragma_parse(s1);
			parse_flags = saved_parse_flags;
			mcc_close();
			macro_ptr = saved_macro_ptr;
		}
	}

	if (*ptoken_seen != TOK_LINEFEED)
		{ MCC_TRACE("br\n"); fputc('\n', s1->ppfp); }
	fputs("#pragma ", s1->ppfp);
	fputs(content, s1->ppfp);
	fputc('\n', s1->ppfp);
	mcc_free(content);
	*ptoken_seen = TOK_LINEFEED;
}

ST_FUNC int mcc_preprocess(MCCState *s1) { MCC_TRACE("enter\n");
	MCC_TRACE("\n");
	BufferedFile **iptr;
	int token_seen, spcs, level, cmdline_pending;
	const char *p;
	char white[400];

	parse_flags = PARSE_FLAG_PREPROCESS | (parse_flags & PARSE_FLAG_ASM_FILE) | PARSE_FLAG_LINEFEED | PARSE_FLAG_SPACES | PARSE_FLAG_ACCEPT_STRAYS;
	if (s1->Pflag == LINE_MACRO_OUTPUT_FORMAT_P10)
		{ MCC_TRACE("br\n"); parse_flags |= PARSE_FLAG_TOK_NUM, s1->Pflag = 1; }

	if (s1->do_bench) { MCC_TRACE("br\n");
		do
			{ MCC_TRACE("br\n"); next(); }
		while (tok != TOK_EOF);
		return 0;
	}

	token_seen = TOK_LINEFEED, spcs = 0, level = 0, cmdline_pending = 0;
	if (file->prev) { MCC_TRACE("br\n");
		file->prev->line_num--;
		pp_line(s1, file->prev, level++);
		file->prev->line_ref = ++file->prev->line_num;
	}
	if (file->prev && s1->Pflag == LINE_MACRO_OUTPUT_FORMAT_GCC &&
			!(s1->dflag & 4)) { MCC_TRACE("br\n");
		fprintf(s1->ppfp, "# 0 \"<built-in>\"\n");
		file->line_num = 0;
		pp_line(s1, file, 0);
		file->line_ref = ++file->line_num;
		cmdline_pending = 1;
	} else { MCC_TRACE("br\n");
		pp_line(s1, file, level);
	}

	for (;;) { MCC_TRACE("br\n");
		iptr = s1->include_stack_ptr;
		next();
		if (tok == TOK_EOF)
			{ MCC_TRACE("br\n"); break; }

		level = s1->include_stack_ptr - iptr;
		if (level) { MCC_TRACE("br\n");
			if (level > 0)
				{ MCC_TRACE("br\n"); pp_line(s1, *iptr, 0); }
			if (level < 0 && cmdline_pending &&
					s1->include_stack_ptr == s1->include_stack) { MCC_TRACE("br\n");
				file->line_ref = 0;
				pp_line(s1, file, 0);
				cmdline_pending = 0;
			} else { MCC_TRACE("br\n");
				pp_line(s1, file, level);
			}
		}
		if (s1->dflag & 7) { MCC_TRACE("br\n");
			pp_debug_defines(s1);
			if (s1->dflag & 4)
				{ MCC_TRACE("br\n"); continue; }
		}

		if (tok == TOK__Pragma) { MCC_TRACE("br\n");
			spcs = 0;
			pp_pragma_operator(s1, &token_seen);
			continue;
		}

		if (is_space(tok)) { MCC_TRACE("br\n");
			if (pp_comment_ready) { MCC_TRACE("br\n");
				pp_comment_ready = 0;
				if (token_seen == TOK_LINEFEED)
					{ MCC_TRACE("br\n"); pp_line(s1, file, 0); }
				white[spcs] = 0, fputs(white, s1->ppfp), spcs = 0;
				fwrite(pp_comment_text.data, 1, pp_comment_text.size, s1->ppfp);
				token_seen = ' ';
				continue;
			}
			if (spcs < sizeof white - 1)
				{ MCC_TRACE("br\n"); white[spcs++] = tok; }
			continue;
		} else if (tok == TOK_LINEFEED) { MCC_TRACE("br\n");
			spcs = 0;
			if (token_seen == TOK_LINEFEED)
				{ MCC_TRACE("br\n"); continue; }
			++file->line_ref;
		} else if (token_seen == TOK_LINEFEED) { MCC_TRACE("br\n");
			pp_line(s1, file, 0);
		} else if (spcs == 0 && pp_need_space(token_seen, tok)) { MCC_TRACE("br\n");
			white[spcs++] = ' ';
		}

		white[spcs] = 0, fputs(white, s1->ppfp), spcs = 0;
		fputs(p = get_tok_str(tok, &tokc), s1->ppfp);
		token_seen = pp_check_he0xE(tok, p);
	}
	return 0;
}
