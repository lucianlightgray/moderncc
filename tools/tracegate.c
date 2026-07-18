#include "toolsupport.h"

static int g_violations;

static int is_id(int c) {
	return isalnum((unsigned char)c) || c == '_';
}

/* Blank comments, string/char literals, and preprocessor logical lines to
   spaces (length-preserving, newlines kept) so a stripped offset maps 1:1 back
   to the raw text. */
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
				if (*s == '\\' && s[1] == '\n') {
					*w++ = ' ', *w++ = '\n', s += 2;
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

static const char *skip_ws(const char *p) {
	while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
		++p;
	return p;
}

static int word_is(const char *p, const char *kw) {
	size_t n = strlen(kw);
	return !strncmp(p, kw, n) && !is_id(p[n]);
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

/* raw+off begins "MCC_TRACE"; verify its format arg starts with `want`
   ("enter" for functions, "br" for branches). */
static int arg_is(const char *rawtok, const char *want) {
	const char *p = rawtok + 9; /* past "MCC_TRACE" */
	while (*p == ' ' || *p == '\t')
		++p;
	if (*p != '(')
		return 0;
	++p;
	while (*p == ' ' || *p == '\t')
		++p;
	if (*p != '"')
		return 0;
	++p;
	return !strncmp(p, want, strlen(want));
}

/* `brace` points at '{' in the stripped buffer; `raw` is the untouched text at
   the same base. Require the block to open with MCC_TRACE(`want`...). */
static void check_open(const char *path, const char *s, const char *raw,
											 const char *brace, const char *kind, const char *want) {
	const char *t = skip_ws(brace + 1);
	if (!word_is(t, "MCC_TRACE")) {
		printf("  %s:%d: %s does not open with MCC_TRACE\n", path,
					 lineno(s, brace), kind);
		g_violations++;
		return;
	}
	if (!arg_is(raw + (t - s), want)) {
		printf("  %s:%d: %s opens with MCC_TRACE but not (\"%s...\")\n", path,
					 lineno(s, brace), kind, want);
		g_violations++;
	}
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

/* Control-flow branches: every braced if/else/for/while/switch/do block opens
   with MCC_TRACE("br\n"). */
static void scan_branches(const char *path, const char *s, const char *raw) {
	const char *p = s;
	while (*p) {
		if (is_id(*p) && (p == s || !is_id(p[-1]))) {
			const char *kw = NULL;
			if (word_is(p, "if"))
				kw = "if";
			else if (word_is(p, "for"))
				kw = "for";
			else if (word_is(p, "while"))
				kw = "while";
			else if (word_is(p, "switch"))
				kw = "switch";
			else if (word_is(p, "else"))
				kw = "else";
			else if (word_is(p, "do"))
				kw = "do";
			if (kw) {
				const char *q = p + strlen(kw);
				const char *body;
				if (kw[0] != 'e' && kw[0] != 'd') {
					q = skip_ws(q);
					if (*q != '(') {
						p += strlen(kw);
						continue;
					}
					q = match_paren(q);
					body = skip_ws(q);
				} else if (kw[0] == 'e') {
					body = skip_ws(q);
					if (word_is(body, "if")) {
						p += strlen(kw);
						continue;
					}
				} else {
					body = skip_ws(q);
				}
				if (*body == '{')
					check_open(path, s, raw, body, kw, "br");
				p += strlen(kw);
				continue;
			}
		}
		++p;
	}
}

/* Function definitions: a top-level `) {` opens a function body, which must
   open with MCC_TRACE("enter\n"). */
static void scan_functions(const char *path, const char *s, const char *raw) {
	const char *p;
	int depth = 0;
	for (p = s; *p; ++p) {
		if (*p == '{') {
			if (depth == 0) {
				const char *b = p;
				while (b > s && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\r' ||
												 b[-1] == '\n'))
					--b;
				if (b > s && b[-1] == ')')
					check_open(path, s, raw, p, "function", "enter");
			}
			++depth;
		} else if (*p == '}') {
			if (depth > 0)
				--depth;
		}
	}
}

static int scan_file(const char *path, int is_dir, void *ud) {
	char *raw, *s;
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
	{
		size_t len = strlen(raw);
		s = malloc(len + 1);
		if (s) {
			memcpy(s, raw, len + 1);
			strip(s);
			/* opt-in: only files that actually call MCC_TRACE (a real call
				 survives strip; a #define or a comment mention does not). */
			if (strstr(s, "MCC_TRACE(")) {
				scan_branches(path, s, raw);
				scan_functions(path, s, raw);
			}
			free(s);
		}
	}
	free(raw);
	return 0;
}

int main(int argc, char **argv) {
	static const char *defaults[] = {"src", 0};
	const char *const *roots = argc > 1 ? (const char *const *)(argv + 1) : defaults;
	int i, n = argc > 1 ? argc - 1 : 1;

	for (i = 0; i < n; ++i) {
		int isd;
		if (host_stat(roots[i], &isd, NULL, NULL) || !isd) {
			fprintf(stderr, "tracegate: not a directory: %s\n", roots[i]);
			return 2;
		}
		host_dir_walk(roots[i], 1, scan_file, NULL);
	}

	if (g_violations) {
		fprintf(stderr,
						"trace-gate invariant violated - a function body or braced branch in\n"
						"an MCC_TRACE-instrumented file does not open with the expected\n"
						"MCC_TRACE(\"enter\\n\")/MCC_TRACE(\"br\\n\"): %d violation(s) above\n",
						g_violations);
		return 1;
	}
	printf("trace-gate invariant OK: every function and braced branch in "
				 "instrumented files opens with MCC_TRACE\n");
	return 0;
}
