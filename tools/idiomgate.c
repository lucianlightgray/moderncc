#include "toolsupport.h"

static const char *VALUE_KIND[] = {
		"MCC_CONFIG_ASM", "MCC_CONFIG_DIAG_RT",
		"MCC_CONFIG_MACHO_CHAINED_FIXUPS", "MCC_CONFIG_CST", "MCC_CONFIG_AST",
		"MCC_CONFIG_PIE", "MCC_CONFIG_PIC", "MCC_CONFIG_NEW_DTAGS",
		"MCC_CONFIG_CODESIGN", "MCC_CONFIG_RUN_DUALMAP", "MCC_CONFIG_MUSL",
		"MCC_CONFIG_PREDEFS", 0};

static const char *FLAG_KIND[] = {
		"MCC_CONFIG_BACKTRACE_ONLY", "MCC_CONFIG_MCCBOOT", "MCC_CONFIG_STATIC",
		"MCC_CONFIG_TRIPLET", "MCC_CONFIG_OS_RELEASE", "MCC_CONFIG_TOOLHOST",
		"MCC_CONFIG_MCCDIR", "MCC_CONFIG_ELFINTERP", "MCC_CONFIG_SWITCHES", 0};

static int g_violations;

static int is_id(int c) {
	return isalnum((unsigned char)c) || c == '_';
}

static int in_list(const char *const *list, const char *tok, int len) {
	int i;
	for (i = 0; list[i]; ++i)
		if ((int)strlen(list[i]) == len && !strncmp(list[i], tok, len))
			return 1;
	return 0;
}

static int is_value_kind(const char *tok, int len) {
	return in_list(VALUE_KIND, tok, len);
}

static int is_flag_kind(const char *tok, int len) {
	if (len > 20 && !strncmp(tok, "MCC_CONFIG_ELFINTERP", 20))
		return 1;
	return in_list(FLAG_KIND, tok, len);
}

static void violate(const char *path, int ln, const char *line, const char *why) {
	while (*line == ' ' || *line == '\t')
		++line;
	printf("  %s:%d: %s [%s]\n", path, ln, line, why);
	g_violations++;
}

typedef enum { D_NONE, D_IF, D_ELIF, D_IFDEF, D_IFNDEF, D_DEFINE } DirKind;

static DirKind directive(const char *line, const char **rest) {
	const char *s = line;
	while (*s == ' ' || *s == '\t')
		++s;
	if (*s != '#')
		return D_NONE;
	++s;
	while (*s == ' ' || *s == '\t')
		++s;
	if (!strncmp(s, "ifdef", 5) && !is_id(s[5]))
		return *rest = s + 5, D_IFDEF;
	if (!strncmp(s, "ifndef", 6) && !is_id(s[6]))
		return *rest = s + 6, D_IFNDEF;
	if (!strncmp(s, "elif", 4) && !is_id(s[4]))
		return *rest = s + 4, D_ELIF;
	if (!strncmp(s, "if", 2) && !is_id(s[2]))
		return *rest = s + 2, D_IF;
	if (!strncmp(s, "define", 6) && !is_id(s[6]))
		return *rest = s + 6, D_DEFINE;
	return D_NONE;
}

static const char *first_ident(const char *s, int *len) {
	while (*s && !(isalpha((unsigned char)*s) || *s == '_'))
		++s;
	if (!*s)
		return NULL;
	*len = 0;
	while (is_id(s[*len]))
		++*len;
	return s;
}

static void scan_expr(const char *path, int ln, const char *line, const char *s,
											int *pend_defined) {
	while (*s) {
		if (*s == '"') {
			for (++s; *s && *s != '"'; ++s)
				if (*s == '\\' && s[1])
					++s;
			if (*s)
				++s;
		} else if (*s == '\'') {
			for (++s; *s && *s != '\''; ++s)
				if (*s == '\\' && s[1])
					++s;
			if (*s)
				++s;
		} else if (*s == '/' && s[1] == '/') {
			break;
		} else if (*s == '/' && s[1] == '*') {
			for (s += 2; *s && !(*s == '*' && s[1] == '/'); ++s)
				;
			if (*s)
				s += 2;
		} else if (isalpha((unsigned char)*s) || *s == '_') {
			const char *b = s;
			int len;
			while (is_id(*s))
				++s;
			len = (int)(s - b);
			if (*pend_defined) {
				*pend_defined = 0;
				if (is_value_kind(b, len))
					violate(path, ln, line, "defined() on value-kind config macro");
			} else if (len == 7 && !strncmp(b, "defined", 7)) {
				*pend_defined = 1;
			} else if (is_flag_kind(b, len)) {
				violate(path, ln, line, "flag-kind config macro tested as a value");
			}
		} else {
			++s;
		}
	}
}

static int scan_file(const char *path, int is_dir, void *ud) {
	char *text, *p;
	int ln = 0, elen, cont = 0, pend_defined = 0;
	(void)ud;
	if (is_dir)
		return 0;
	elen = (int)strlen(path);
	if (!(elen >= 2 && path[elen - 2] == '.' &&
				(path[elen - 1] == 'c' || path[elen - 1] == 'h' ||
				 path[elen - 1] == 's' || path[elen - 1] == 'S')) &&
			!(elen >= 4 && !strcmp(path + elen - 4, ".inc")))
		return 0;
	if (!(text = ts_read_file(path, NULL)))
		return 0;
	for (p = text; *p;) {
		char *e = strchr(p, '\n');
		int len = e ? (int)(e - p) : (int)strlen(p);
		const char *rest;
		DirKind k;
		++ln;
		if (len && p[len - 1] == '\r')
			--len;
		p[len] = 0;
		if (cont) {
			cont = len && p[len - 1] == '\\';
			scan_expr(path, ln, p, p, &pend_defined);
		} else if ((k = directive(p, &rest)) == D_IF || k == D_ELIF) {
			cont = len && p[len - 1] == '\\';
			pend_defined = 0;
			scan_expr(path, ln, p, rest, &pend_defined);
		} else if (k == D_IFDEF || k == D_IFNDEF) {
			const char *name;
			int nlen;
			if ((name = first_ident(rest, &nlen)) && is_value_kind(name, nlen)) {
				if (k == D_IFDEF) {
					violate(path, ln, p, "#ifdef on value-kind config macro");
				} else {
					const char *dname = NULL;
					int dlen = 0, ok = 0;
					if (e) {
						char *n = e + 1, *ne = strchr(n, '\n');
						int nl = ne ? (int)(ne - n) : (int)strlen(n);
						char save = n[nl];
						n[nl] = 0;
						if (directive(n, &rest) == D_DEFINE &&
								(dname = first_ident(rest, &dlen)) != NULL &&
								dlen == nlen && !strncmp(dname, name, nlen))
							ok = 1;
						n[nl] = save;
					}
					if (!ok)
						violate(path, ln, p,
										"#ifndef on value-kind config macro without default #define");
				}
			}
		}
		if (!e)
			break;
		p = e + 1;
	}
	free(text);
	return 0;
}

int main(int argc, char **argv) {
	static const char *defaults[] = {"src", "tools", 0};
	const char *const *roots = argc > 1 ? (const char *const *)(argv + 1) : defaults;
	int i, n = argc > 1 ? argc - 1 : 2;

	for (i = 0; i < n; ++i) {
		int isd;
		if (host_stat(roots[i], &isd, NULL, NULL) || !isd) {
			fprintf(stderr, "idiomgate: not a directory: %s\n", roots[i]);
			return 2;
		}
		host_dir_walk(roots[i], 1, scan_file, NULL);
	}

	if (g_violations) {
		fprintf(stderr,
						"idiom-gate invariant violated - config macro tested with the\n"
						"wrong idiom (value-kind: '#if X' or '#ifndef X'+'#define X';\n"
						"flag-kind: '#ifdef'/'#ifndef'/'defined'): %d violation(s) above\n",
						g_violations);
		return 1;
	}
	printf("idiom-gate invariant OK: canonical config-macro test idioms hold\n");
	return 0;
}
