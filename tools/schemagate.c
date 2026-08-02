#include "toolsupport.h"

#define SYM_MAX 512
#define TERM_MAX 256
#define SPAN_MAX 4096
#define NMAX 96
#define PMAX 512
#define GMAX 192

typedef struct {
	char name[NMAX];
	char file[PMAX];
	int line;
	unsigned long long val;
} Sym;

typedef struct {
	char kind[NMAX];
	char guard[GMAX];
	char file[PMAX];
	int line;
	int fn;
	unsigned mask;
} Term;

typedef struct {
	const char *b;
	const char *e;
	unsigned bits;
} Span;

static Sym g_op[SYM_MAX];
static int g_opn;
static Sym g_fb[SYM_MAX];
static int g_fbn;
static Sym g_rr[SYM_MAX];
static int g_rrn;
static Sym g_rm[SYM_MAX];
static int g_rmn;
static Sym g_rt[SYM_MAX];
static int g_rtn;
static Sym g_rdef[SYM_MAX];
static int g_rdefn;

static Term g_term[TERM_MAX];
static int g_termn;

static Span g_fnspan[SPAN_MAX];
static int g_fnn;
static Span g_blk[SPAN_MAX];
static int g_blkn;
static Span g_cond[SPAN_MAX];
static int g_condn;

static unsigned g_tag_all;
static unsigned g_fam_region;
static unsigned g_fam_mark;

static int g_violations;
static int g_phase;
static const char *g_path;

static int is_id(int c) {
	return isalnum((unsigned char)c) || c == '_';
}

static void strip(char *s) {
	char *w = s;
	int at_bol = 1;
	while (*s) {
		if (*s == '/' && s[1] == '/') {
			while (*s && *s != '\n')
				*w++ = ' ', ++s;
			continue;
		}
		if (*s == '/' && s[1] == '*') {
			*w++ = ' ', *w++ = ' ', s += 2;
			while (*s && !(*s == '*' && s[1] == '/'))
				*w++ = (*s == '\n' ? '\n' : ' '), ++s;
			if (*s)
				*w++ = ' ', *w++ = ' ', s += 2;
			at_bol = 0;
			continue;
		}
		if (*s == '#' && at_bol) {
			while (*s) {
				if (*s == '\\' && (s[1] == '\n' || (s[1] == '\r' && s[2] == '\n'))) {
					*w++ = ' ';
					if (s[1] == '\r')
						*w++ = '\r', ++s;
					*w++ = '\n', s += 2;
					continue;
				}
				if (*s == '\n') {
					*w++ = '\n', ++s;
					break;
				}
				*w++ = ' ', ++s;
			}
			at_bol = 1;
			continue;
		}
		if (*s == '"' || *s == '\'') {
			char q = *s;
			*w++ = ' ', ++s;
			while (*s && *s != q) {
				if (*s == '\\' && s[1])
					*w++ = ' ', *w++ = ' ', s += 2;
				else
					*w++ = (*s == '\n' ? '\n' : ' '), ++s;
			}
			if (*s)
				*w++ = ' ', ++s;
			at_bol = 0;
			continue;
		}
		if (*s == '\n')
			at_bol = 1;
		else if (*s != ' ' && *s != '\t')
			at_bol = 0;
		*w++ = *s++;
	}
	*w = 0;
}

static int lineno(const char *base, const char *p) {
	int ln = 1;
	while (base < p) {
		if (*base == '\n')
			++ln;
		++base;
	}
	return ln;
}

static const char *skip_ws(const char *p) {
	while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
		++p;
	return p;
}

static int tok_at(const char *s, const char *base, const char *kw) {
	size_t n = strlen(kw);
	return (s == base || !is_id((unsigned char)s[-1])) && !strncmp(s, kw, n) &&
				 !is_id((unsigned char)s[n]);
}

static const char *match_paren(const char *p) {
	int d = 0;
	while (*p) {
		if (*p == '(')
			++d;
		else if (*p == ')') {
			if (--d == 0)
				return p + 1;
		}
		++p;
	}
	return p;
}

static const char *match_brace(const char *p) {
	int d = 0;
	while (*p) {
		if (*p == '{')
			++d;
		else if (*p == '}') {
			if (--d == 0)
				return p + 1;
		}
		++p;
	}
	return p;
}

static void violate(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	fputs("  ", stdout);
	vprintf(fmt, ap);
	va_end(ap);
	fputc('\n', stdout);
	g_violations++;
}

static Sym *sym_find(Sym *a, int n, const char *name) {
	int i;
	for (i = 0; i < n; ++i)
		if (!strcmp(a[i].name, name))
			return &a[i];
	return NULL;
}

static Sym *sym_by_val(Sym *a, int n, unsigned long long v) {
	int i;
	for (i = 0; i < n; ++i)
		if (a[i].val == v)
			return &a[i];
	return NULL;
}

static void sym_add(Sym *a, int *n, const char *name, unsigned long long v,
										const char *file, int line) {
	Sym *s;
	if (*n >= SYM_MAX)
		return;
	s = &a[(*n)++];
	strncpy(s->name, name, NMAX - 1);
	s->name[NMAX - 1] = 0;
	strncpy(s->file, file, PMAX - 1);
	s->file[PMAX - 1] = 0;
	s->line = line;
	s->val = v;
}

static int parse_value(const char *t, unsigned long long *out) {
	const char *p = skip_ws(t);
	unsigned long long v;
	char *e;
	int paren = 0;
	if (*p == '(') {
		paren = 1;
		p = skip_ws(p + 1);
	}
	v = strtoull(p, &e, 0);
	if (e == p)
		return 0;
	p = e;
	while (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L')
		++p;
	p = skip_ws(p);
	if (p[0] == '<' && p[1] == '<') {
		unsigned long long k = strtoull(p + 2, &e, 0);
		if (e == p + 2)
			return 0;
		v <<= k;
		p = e;
		while (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L')
			++p;
		p = skip_ws(p);
	}
	if (paren) {
		if (*p != ')')
			return 0;
		p = skip_ws(p + 1);
	}
	if (*p && !(p[0] == '/' && (p[1] == '*' || p[1] == '/')))
		return 0;
	*out = v;
	return 1;
}

static void collect_define(const char *line, int ln, const char *prefix,
													 Sym *space, int *n, int values) {
	const char *p = skip_ws(line);
	const char *q;
	char name[NMAX];
	unsigned long long v = 0;
	size_t plen = strlen(prefix), nlen;
	if (*p != '#')
		return;
	p = skip_ws(p + 1);
	if (strncmp(p, "define", 6) || is_id((unsigned char)p[6]))
		return;
	p = skip_ws(p + 6);
	if (strncmp(p, prefix, plen))
		return;
	q = p;
	while (is_id((unsigned char)*q))
		++q;
	nlen = (size_t)(q - p);
	if (nlen >= NMAX || *q == '(')
		return;
	memcpy(name, p, nlen);
	name[nlen] = 0;
	if (values && !parse_value(q, &v)) {
		violate("%s:%d: %s has a value the gate cannot read as a literal", g_path,
						ln, name);
		return;
	}
	sym_add(space, n, name, v, g_path, ln);
}

static void collect_defines(const char *raw) {
	const char *p = raw;
	int ln = 0;
	char buf[1024];
	while (*p) {
		const char *e = strchr(p, '\n');
		size_t len = e ? (size_t)(e - p) : strlen(p);
		++ln;
		if (len >= sizeof(buf))
			len = sizeof(buf) - 1;
		memcpy(buf, p, len);
		buf[len] = 0;
		collect_define(buf, ln, "AST_OP_", g_op, &g_opn, 1);
		collect_define(buf, ln, "AST_FB_", g_fb, &g_fbn, 1);
		collect_define(buf, ln, "RIR_R_", g_rdef, &g_rdefn, 0);
		collect_define(buf, ln, "RIR_M_", g_rdef, &g_rdefn, 0);
		if (!e)
			break;
		p = e + 1;
	}
}

static void collect_enums(const char *s) {
	const char *p = s;
	while (*p) {
		const char *b;
		long long counter = 0;
		if (!tok_at(p, s, "enum")) {
			++p;
			continue;
		}
		b = skip_ws(p + 4);
		while (is_id((unsigned char)*b))
			++b;
		b = skip_ws(b);
		if (*b != '{') {
			p += 4;
			continue;
		}
		++b;
		while (*b && *b != '}') {
			const char *q;
			char name[NMAX];
			size_t nlen;
			long long v;
			b = skip_ws(b);
			if (*b == ',') {
				++b;
				continue;
			}
			if (!is_id((unsigned char)*b) || isdigit((unsigned char)*b))
				break;
			q = b;
			while (is_id((unsigned char)*q))
				++q;
			nlen = (size_t)(q - b);
			if (nlen >= NMAX)
				break;
			memcpy(name, b, nlen);
			name[nlen] = 0;
			v = counter;
			q = skip_ws(q);
			if (*q == '=') {
				char *e2;
				long long got = strtoll(skip_ws(q + 1), &e2, 0);
				const char *scan = skip_ws(q + 1);
				if (e2 != scan)
					v = got;
				while (*q && *q != ',' && *q != '}')
					++q;
			}
			counter = v + 1;
			if (!strncmp(name, "RIR_R_", 6))
				sym_add(g_rr, &g_rrn, name, (unsigned long long)v, g_path,
								lineno(s, b));
			else if (!strncmp(name, "RIR_M_", 6))
				sym_add(g_rm, &g_rmn, name, (unsigned long long)v, g_path,
								lineno(s, b));
			else if (!strncmp(name, "RIR_T_", 6))
				sym_add(g_rt, &g_rtn, name, (unsigned long long)v, g_path,
								lineno(s, b));
			b = q;
			if (*b == ',')
				++b;
		}
		p = b;
	}
}

static void check_space(const char *space, Sym *a, int n, int bits) {
	int i, j;
	for (i = 0; i < n; ++i)
		for (j = i + 1; j < n; ++j) {
			if (!strcmp(a[i].name, a[j].name))
				violate("%s:%d: %s redefines %s at %s:%d [%s]", a[j].file, a[j].line,
								a[j].name, a[i].name, a[i].file, a[i].line, space);
			else if (a[i].val == a[j].val)
				violate("%s:%d: %s = 0x%llx duplicates %s at %s:%d [%s]", a[j].file,
								a[j].line, a[j].name, a[j].val, a[i].name, a[i].file,
								a[i].line, space);
			else if (bits && (a[i].val & a[j].val))
				violate("%s:%d: %s = 0x%llx overlaps bits 0x%llx of %s at %s:%d [%s]",
								a[j].file, a[j].line, a[j].name, a[j].val,
								a[i].val & a[j].val, a[i].name, a[i].file, a[i].line, space);
		}
}

static int is_edge(const char *name) {
	size_t n = strlen(name);
	return (n > 6 && !strcmp(name + n - 6, "_COUNT")) ||
				 (n > 5 && !strcmp(name + n - 5, "_NONE"));
}

static void check_fallback(Sym *a, int n) {
	int i;
	for (i = 0; i < n; ++i)
		if (!is_edge(a[i].name) && !sym_find(g_rdef, g_rdefn, a[i].name))
			violate("%s:%d: %s has no #define in the non-replay branch and would "
							"silently read as 0",
							a[i].file, a[i].line, a[i].name);
}

static void trim(const char **b, const char **e) {
	while (*b < *e && (**b == ' ' || **b == '\t' || **b == '\r' || **b == '\n'))
		++*b;
	while (*e > *b && ((*e)[-1] == ' ' || (*e)[-1] == '\t' || (*e)[-1] == '\r' ||
										 (*e)[-1] == '\n'))
		--*e;
	while (*e - *b >= 2 && **b == '(' && match_paren(*b) == *e) {
		++*b;
		--*e;
		while (*b < *e && (**b == ' ' || **b == '\t' || **b == '\r' || **b == '\n'))
			++*b;
		while (*e > *b && ((*e)[-1] == ' ' || (*e)[-1] == '\t' ||
											 (*e)[-1] == '\r' || (*e)[-1] == '\n'))
			--*e;
	}
}

static const char *find_top(const char *b, const char *e, const char *op) {
	size_t n = strlen(op);
	int d = 0;
	const char *p;
	for (p = b; p + n <= e; ++p) {
		if (*p == '(' || *p == '[')
			++d;
		else if (*p == ')' || *p == ']')
			--d;
		else if (!d && !strncmp(p, op, n)) {
			if (n == 1 && (p[1] == op[0] || (p > b && p[-1] == op[0])))
				continue;
			if (n == 1 && *op == '?' && p + 1 < e && p[1] == ':')
				continue;
			return p;
		}
	}
	return NULL;
}

static unsigned tag_bit(const char *b, const char *e) {
	char name[NMAX];
	size_t n = (size_t)(e - b);
	Sym *s;
	if (n >= NMAX)
		return 0;
	memcpy(name, b, n);
	name[n] = 0;
	s = sym_find(g_rt, g_rtn, name);
	return s ? 1u << s->val : 0;
}

static unsigned leaf_tags(const char *b, const char *e, int want_true) {
	const char *p;
	for (p = b; p < e; ++p) {
		const char *q;
		unsigned bit;
		int eq;
		if (!tok_at(p, b, "tag"))
			continue;
		q = skip_ws(p + 3);
		if (q[0] == '=' && q[1] == '=')
			eq = 1;
		else if (q[0] == '!' && q[1] == '=')
			eq = 0;
		else
			continue;
		q = skip_ws(q + 2);
		p = q;
		while (p < e && is_id((unsigned char)*p))
			++p;
		bit = tag_bit(q, p);
		if (!bit)
			return g_tag_all;
		if (!want_true)
			eq = !eq;
		return eq ? bit : (g_tag_all & ~bit);
	}
	return g_tag_all;
}

static unsigned tagset(const char *b, const char *e, int want_true) {
	const char *cut;
	trim(&b, &e);
	if (b >= e)
		return g_tag_all;
	if ((cut = find_top(b, e, "||")) != NULL)
		return want_true ? (tagset(b, cut, 1) | tagset(cut + 2, e, 1))
										 : (tagset(b, cut, 0) & tagset(cut + 2, e, 0));
	if ((cut = find_top(b, e, "&&")) != NULL)
		return want_true ? (tagset(b, cut, 1) & tagset(cut + 2, e, 1))
										 : (tagset(b, cut, 0) | tagset(cut + 2, e, 0));
	return leaf_tags(b, e, want_true);
}

static unsigned constrain(const char *b, const char *e, const char *p, int pos) {
	const char *cut;
	unsigned acc = g_tag_all;
	trim(&b, &e);
	if (p < b || p >= e)
		return g_tag_all;
	if ((cut = find_top(b, e, "||")) != NULL) {
		if (p < cut)
			return (pos ? g_tag_all : tagset(cut + 2, e, 0)) & constrain(b, cut, p, pos);
		return (pos ? g_tag_all : tagset(b, cut, 0)) & constrain(cut + 2, e, p, pos);
	}
	if ((cut = find_top(b, e, "&&")) != NULL) {
		if (p < cut)
			return (pos ? tagset(cut + 2, e, 1) : g_tag_all) & constrain(b, cut, p, pos);
		return (pos ? tagset(b, cut, 1) : g_tag_all) & constrain(cut + 2, e, p, pos);
	}
	return acc;
}

static void note_span(Span *a, int *n, const char *b, const char *e,
											unsigned bits) {
	if (*n >= SPAN_MAX)
		return;
	a[*n].b = b;
	a[*n].e = e;
	a[*n].bits = bits;
	++*n;
}

static void map_file(const char *s) {
	const char *p;
	int depth = 0;
	g_fnn = g_blkn = g_condn = 0;
	for (p = s; *p; ++p) {
		if (*p == '{') {
			if (!depth) {
				const char *b = p;
				while (b > s && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\r' ||
												 b[-1] == '\n'))
					--b;
				if (b > s && b[-1] == ')') {
					const char *e = match_brace(p);
					unsigned bits = 0;
					const char *q;
					for (q = p; q < e; ++q) {
						if (!strncmp(q, "RIR_R_", 6))
							bits |= 1u;
						else if (!strncmp(q, "RIR_M_", 6))
							bits |= 2u;
					}
					note_span(g_fnspan, &g_fnn, p, e, bits);
				}
			}
			++depth;
		} else if (*p == '}') {
			if (depth > 0)
				--depth;
		}
	}
	for (p = s; *p; ++p) {
		const char *q, *ce, *r;
		if (!tok_at(p, s, "if") && !tok_at(p, s, "while"))
			continue;
		q = skip_ws(p + (*p == 'i' ? 2 : 5));
		if (*q != '(')
			continue;
		ce = match_paren(q);
		note_span(g_cond, &g_condn, q + 1, ce - 1, 0);
		r = skip_ws(ce);
		if (*r == '{')
			note_span(g_blk, &g_blkn, r, match_brace(r), tagset(q + 1, ce - 1, 1));
		p = q;
	}
}

static void other_space(unsigned long long v, unsigned fam, char *out, int n) {
	Sym *s = (fam == g_fam_region) ? sym_by_val(g_rm, g_rmn, v)
																 : sym_by_val(g_rr, g_rrn, v);
	if (s)
		snprintf(out, (size_t)n, "%s at %s:%d", s->name, s->file, s->line);
	else
		snprintf(out, (size_t)n, "no symbol");
}

static void check_site(const char *s, const char *p, const char *name, int pos) {
	unsigned fam, possible = g_tag_all;
	unsigned long long ord;
	const char *cb = NULL, *ce = NULL;
	Sym *sym;
	char other[PMAX];
	int i;
	if (!strncmp(name, "RIR_R_", 6)) {
		fam = g_fam_region;
		sym = sym_find(g_rr, g_rrn, name);
	} else {
		fam = g_fam_mark;
		sym = sym_find(g_rm, g_rmn, name);
	}
	if (!sym)
		return;
	ord = sym->val;
	for (i = 0; i < g_fnn; ++i)
		if (p >= g_fnspan[i].b && p < g_fnspan[i].e && g_fnspan[i].bits != 3u)
			return;
	for (i = 0; i < g_blkn; ++i)
		if (p >= g_blk[i].b && p < g_blk[i].e)
			possible &= g_blk[i].bits;
	for (i = 0; i < g_condn; ++i)
		if (p >= g_cond[i].b && p < g_cond[i].e && (!cb || g_cond[i].b > cb))
			cb = g_cond[i].b, ce = g_cond[i].e;
	if (cb)
		possible &= constrain(cb, ce, p, pos);
	if (!(possible & ~fam))
		return;
	other_space(ord, fam, other, PMAX);
	violate("%s:%d: %s (rkind %llu) tested without pinning tag to its own space; "
					"the same rkind is %s [rkind]",
					g_path, lineno(s, p), name, ord, other);
}

static void scan_discriminator(const char *s) {
	const char *p;
	map_file(s);
	for (p = s; *p; ++p) {
		const char *q, *r;
		char name[NMAX];
		size_t n;
		int pos;
		if (tok_at(p, s, "rkind")) {
			q = skip_ws(p + 5);
			if (q[0] == '=' && q[1] == '=')
				pos = 1;
			else if (q[0] == '!' && q[1] == '=')
				pos = 0;
			else
				continue;
			q = skip_ws(q + 2);
		} else if (tok_at(p, s, "case")) {
			q = skip_ws(p + 4);
			pos = 1;
		} else {
			continue;
		}
		if (strncmp(q, "RIR_R_", 6) && strncmp(q, "RIR_M_", 6))
			continue;
		r = q;
		while (is_id((unsigned char)*r))
			++r;
		n = (size_t)(r - q);
		if (n >= NMAX)
			continue;
		memcpy(name, q, n);
		name[n] = 0;
		check_site(s, p, name, pos);
	}
}

static int type_cast(const char *b, const char *e) {
	const char *p;
	int seen = 0;
	for (p = b; p < e; ++p) {
		if (!isalnum((unsigned char)*p) && *p != '_' && *p != ' ' && *p != '\t' &&
				*p != '*')
			return 0;
		if (!strncmp(p, "int", 3) || !strncmp(p, "unsigned", 8) ||
				!strncmp(p, "long", 4) || !strncmp(p, "short", 5) ||
				!strncmp(p, "char", 4) || !strncmp(p, "signed", 6) ||
				!strncmp(p, "_t", 2))
			seen = 1;
	}
	return seen;
}

static void strip_casts(const char **b, const char **e) {
	for (;;) {
		const char *close;
		trim(b, e);
		if (*b >= *e || **b != '(')
			return;
		close = match_paren(*b);
		if (close > *e || !type_cast(*b + 1, close - 1))
			return;
		*b = close;
	}
}

static void norm(const char *b, const char *e, char *out) {
	int n = 0;
	while (b < e && n < GMAX - 1) {
		if (*b != ' ' && *b != '\t' && *b != '\r' && *b != '\n')
			out[n++] = *b;
		++b;
	}
	out[n] = 0;
}

static void add_term(const char *kind, int fn, const char *b, const char *e,
										 int line) {
	Term *t;
	const char *cut;
	unsigned long long v;
	char lit[NMAX];
	if (g_termn >= TERM_MAX)
		return;
	t = &g_term[g_termn++];
	strncpy(t->kind, kind, NMAX - 1);
	t->kind[NMAX - 1] = 0;
	strncpy(t->file, g_path, PMAX - 1);
	t->file[PMAX - 1] = 0;
	t->line = line;
	t->fn = fn;
	t->mask = 0xffffffffu;
	strip_casts(&b, &e);
	if ((cut = find_top(b, e, "?")) != NULL) {
		const char *colon = find_top(cut + 1, e, ":");
		if (colon) {
			const char *vb = cut + 1, *ve = colon, *eb = colon + 1, *ee = e;
			unsigned long long ev;
			trim(&vb, &ve);
			trim(&eb, &ee);
			norm(eb, ee, lit);
			if (parse_value(lit, &ev) && !ev) {
				norm(vb, ve, lit);
				if (parse_value(lit, &v)) {
					t->mask = (unsigned)v;
					norm(b, cut, t->guard);
					return;
				}
			}
		}
		norm(b, e, t->guard);
		return;
	}
	if ((cut = find_top(b, e, "<<")) != NULL) {
		const char *sb = cut + 2, *se = e;
		trim(&sb, &se);
		norm(sb, se, lit);
		if (parse_value(lit, &v) && v < 32) {
			const char *lb = b, *le = cut;
			t->mask = 0xffffffffu << v;
			strip_casts(&lb, &le);
			norm(lb, le, t->guard);
			return;
		}
	}
	norm(b, e, t->guard);
	if (parse_value(t->guard, &v) && v <= 0xffffffffu)
		t->mask = (unsigned)v;
}

static void scan_rendval(const char *s) {
	const char *p;
	for (p = s; *p; ++p) {
		const char *q, *ae, *comma, *kb, *ke, *tb;
		char kind[NMAX];
		int fn;
		if (tok_at(p, s, "rir_rbegin_val"))
			fn = 0, q = p + 14;
		else if (tok_at(p, s, "rir_rend_to_val"))
			fn = 1, q = p + 15;
		else
			continue;
		q = skip_ws(q);
		if (*q != '(')
			continue;
		ae = match_paren(q) - 1;
		comma = find_top(q + 1, ae, ",");
		if (!comma)
			continue;
		kb = q + 1;
		ke = comma;
		trim(&kb, &ke);
		if (strncmp(kb, "RIR_R_", 6) || (size_t)(ke - kb) >= NMAX)
			continue;
		memcpy(kind, kb, (size_t)(ke - kb));
		kind[ke - kb] = 0;
		if (!sym_find(g_rr, g_rrn, kind))
			continue;
		tb = comma + 1;
		for (;;) {
			const char *bar = find_top(tb, ae, "|");
			add_term(kind, fn, tb, bar ? bar : ae, lineno(s, p));
			if (!bar)
				break;
			tb = bar + 1;
		}
		p = ae;
	}
}

static void check_terms(void) {
	int i, j;
	for (i = 0; i < g_termn; ++i)
		for (j = i + 1; j < g_termn; ++j) {
			Term *a = &g_term[i], *b = &g_term[j];
			if (a->fn != b->fn || strcmp(a->kind, b->kind))
				continue;
			if (!strcmp(a->guard, b->guard)) {
				if (a->mask != b->mask)
					violate("%s:%d: %s encodes %s at 0x%x but at 0x%x in %s:%d [rendval]",
									b->file, b->line, b->kind, b->guard, b->mask, a->mask,
									a->file, a->line);
			} else if (a->mask & b->mask) {
				violate("%s:%d: %s bits 0x%x carry %s here and %s at %s:%d [rendval]",
								b->file, b->line, b->kind, a->mask & b->mask, b->guard,
								a->guard, a->file, a->line);
			}
		}
}

static int scan_file(const char *path, int is_dir, void *ud) {
	char *raw, *s;
	size_t len;
	int elen;
	(void)ud;
	if (is_dir)
		return 0;
	elen = (int)strlen(path);
	if (!(elen >= 2 && path[elen - 2] == '.' &&
				(path[elen - 1] == 'c' || path[elen - 1] == 'h')) &&
			!(elen >= 4 && !strcmp(path + elen - 4, ".inc")))
		return 0;
	if (!(raw = ts_read_file(path, NULL)))
		return 0;
	g_path = path;
	len = strlen(raw);
	s = malloc(len + 1);
	if (s) {
		memcpy(s, raw, len + 1);
		strip(s);
		if (g_phase == 0) {
			collect_defines(raw);
			collect_enums(s);
		} else {
			scan_rendval(s);
			scan_discriminator(s);
		}
		free(s);
	}
	free(raw);
	return 0;
}

static unsigned fam_of(const char *name) {
	Sym *s = sym_find(g_rt, g_rtn, name);
	return s ? 1u << s->val : 0;
}

int main(int argc, char **argv) {
	static const char *defaults[] = {"src", 0};
	const char *const *roots = argc > 1 ? (const char *const *)(argv + 1) : defaults;
	int i, n = argc > 1 ? argc - 1 : 1;

	for (i = 0; i < n; ++i) {
		int isd;
		if (host_stat(roots[i], &isd, NULL, NULL) || !isd) {
			fprintf(stderr, "schemagate: not a directory: %s\n", roots[i]);
			return 2;
		}
	}
	for (g_phase = 0; g_phase < 2; ++g_phase) {
		if (g_phase == 1) {
			g_tag_all = g_rtn ? (unsigned)((1ull << g_rtn) - 1) : 0;
			g_fam_region = fam_of("RIR_T_RBEGIN") | fam_of("RIR_T_REND");
			g_fam_mark = fam_of("RIR_T_MARK");
			if (!g_fam_region || !g_fam_mark) {
				fprintf(stderr, "schemagate: RIR_T_RBEGIN/RIR_T_REND/RIR_T_MARK not "
												"found; the tag space moved\n");
				return 2;
			}
			check_space("AST_OP", g_op, g_opn, 0);
			check_space("AST_FB", g_fb, g_fbn, 1);
			check_space("RIR_R", g_rr, g_rrn, 0);
			check_space("RIR_M", g_rm, g_rmn, 0);
			check_fallback(g_rr, g_rrn);
			check_fallback(g_rm, g_rmn);
		}
		for (i = 0; i < n; ++i)
			host_dir_walk(roots[i], 1, scan_file, NULL);
	}
	check_terms();

	if (g_violations) {
		fprintf(stderr,
						"schema-gate invariant violated - two symbols in one number space\n"
						"carry the same value, one rend-value bit carries two meanings, or\n"
						"an rkind test does not pin the tag that tells RIR_R_ from RIR_M_:\n"
						"%d violation(s) above\n",
						g_violations);
		return 1;
	}
	printf("schema-gate invariant OK: %d AST_OP_, %d AST_FB_, %d RIR_R_, %d "
				 "RIR_M_ values distinct, %d region rend-value terms consistent\n",
				 g_opn, g_fbn, g_rrn, g_rmn, g_termn);
	return 0;
}
