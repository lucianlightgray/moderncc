#include "toolsupport.h"

static int g_violations;

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

/* word starting at p equals kw (already known no id-char precedes p) */
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

static void report(const char *path, const char *base, const char *at,
									 const char *kw) {
	printf("  %s:%d: %s block does not open with MCC_TRACE\n", path,
				 lineno(base, at), kw);
	g_violations++;
}

/* returns pointer just past the matching ')' for a '(' at *p == '(' */
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

static void scan(const char *path, char *text) {
	const char *p = text;
	const char *prev = "";
	while (*p) {
		if (is_id(*p) && (p == text || !is_id(p[-1]))) {
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
				const char *at = p;
				const char *q = p + strlen(kw);
				const char *body;
				if (kw[0] != 'e' && kw[0] != 'd') { /* if/for/while/switch */
					q = skip_ws(q);
					if (*q != '(') {
						p += strlen(kw);
						prev = kw;
						continue;
					}
					q = match_paren(q);
					body = skip_ws(q);
				} else if (kw[0] == 'e') { /* else */
					body = skip_ws(q);
					if (word_is(body, "if")) { /* else if -> handled by the if */
						p += strlen(kw);
						prev = kw;
						continue;
					}
				} else { /* do */
					body = skip_ws(q);
				}
				if (*body == '{') {
					const char *t = skip_ws(body + 1);
					if (!word_is(t, "MCC_TRACE"))
						report(path, text, at, kw);
				}
				p += strlen(kw);
				prev = kw;
				continue;
			}
		}
		++p;
	}
	(void)prev;
}

static int scan_file(const char *path, int is_dir, void *ud) {
	char *text;
	int elen;
	(void)ud;
	if (is_dir)
		return 0;
	elen = (int)strlen(path);
	if (!(elen >= 2 && path[elen - 2] == '.' &&
				(path[elen - 1] == 'c' || path[elen - 1] == 'h')) &&
			!(elen >= 4 && !strcmp(path + elen - 4, ".inc")))
		return 0;
	if (!(text = ts_read_file(path, NULL)))
		return 0;
	/* opt-in: only files that use the br-idiom are held to it (raw text — the
	   literal is blanked by strip). */
	if (strstr(text, "MCC_TRACE(\"br")) {
		strip(text);
		scan(path, text);
	}
	free(text);
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
						"trace-gate invariant violated - a braced control block in an\n"
						"MCC_TRACE-instrumented file does not open with MCC_TRACE(\"br\\n\"):\n"
						"%d violation(s) above\n",
						g_violations);
		return 1;
	}
	printf("trace-gate invariant OK: every braced branch in instrumented files "
				 "opens with MCC_TRACE\n");
	return 0;
}
