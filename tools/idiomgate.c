#include "toolsupport.h"

typedef enum { K_VALUE, K_FLAG, K_NOSUBJECT } Kind;

typedef struct {
	const char *name;
	Kind kind;
	const char *why;
} Entry;

static const Entry REGISTRY[] = {
		{"MCC_CONFIG_MACHO_CHAINED_FIXUPS", K_VALUE,
		 "numeric 0/1; CMakeLists.txt emits =0, src/mcc.h defaults it to 1"},
		{"MCC_CONFIG_PIE", K_VALUE,
		 "numeric 0/1; CMakeLists.txt emits =1, src/mccdefaults.h defaults it"},
		{"MCC_CONFIG_PIC", K_VALUE,
		 "numeric 0/1; CMakeLists.txt emits =1, src/mccdefaults.h defaults it"},
		{"MCC_CONFIG_NEW_DTAGS", K_VALUE,
		 "numeric 0/1; CMakeLists.txt emits =1, src/mcc.h defaults it to 0"},
		{"MCC_CONFIG_CODESIGN", K_VALUE,
		 "numeric 0/1; CMakeLists.txt emits =1, absent otherwise"},
		{"MCC_CONFIG_RUN_DUALMAP", K_VALUE,
		 "numeric 0/1; CMakeLists.txt emits =1, absent otherwise"},
		{"MCC_CONFIG_MUSL", K_VALUE,
		 "numeric 0/1; CMakeLists.txt and tools/build.c emit =1, absent otherwise"},
		{"MCC_CONFIG_PREDEFS", K_VALUE,
		 "numeric 0/1; always emitted by CMakeLists.txt and tools/build.c"},
		{"MCC_CONFIG_TRACE", K_VALUE,
		 "numeric 0/1; CMakeLists.txt and tools/build.c emit =1, absent otherwise"},
		{"MCC_CONFIG_CPUVER", K_VALUE,
		 "numeric ARM architecture level; src/arch/arm/arm-gen.h defaults it to 5"},
		{"MCC_CONFIG_DWARF_VERSION", K_VALUE,
		 "numeric DWARF level; src/mcc.h defaults it to 0"},
		{"MCC_CONFIG_SEMLOCK", K_VALUE,
		 "numeric 0/1; src/mcchost.h defaults it to 1"},
		{"MCC_CONFIG_RUNMEM_RO", K_VALUE,
		 "numeric 0/1; src/mcchost.h defaults it to MCC_HOST_DARWIN"},
		{"MCC_CONFIG_AUTO_MCCDIR", K_VALUE,
		 "numeric 0/1; CMakeLists.txt and tools/bench.c emit =1, absent otherwise"},

		{"MCC_CONFIG_BACKTRACE_ONLY", K_FLAG,
		 "presence only; runtime/lib/bt-exe.c defines it with no replacement list"},
		{"MCC_CONFIG_MCCBOOT", K_FLAG,
		 "presence only; supplied by a bootstrap command line, never by the build"},
		{"MCC_CONFIG_STATIC", K_FLAG,
		 "presence only; supplied by a packager command line, never by the build"},
		{"MCC_CONFIG_TOOLHOST", K_FLAG,
		 "presence only; tools/toolhost.h defines it to select the host-only build"},
		{"MCC_CONFIG_TRIPLET", K_FLAG,
		 "string literal; mcc_def_str emits it only when MCC_TRIPLET is non-empty"},
		{"MCC_CONFIG_OS_RELEASE", K_FLAG,
		 "string literal; mcc_def_str emits it only when MCC_OS_RELEASE is non-empty"},
		{"MCC_CONFIG_MCCDIR", K_FLAG,
		 "string literal; src/mcchost.h supplies a per-host default"},
		{"MCC_CONFIG_SYSROOT", K_FLAG,
		 "string literal; src/mccdefaults.h defaults it to the empty string"},
		{"MCC_CONFIG_CROSSPREFIX", K_FLAG,
		 "string literal; src/mccdefaults.h defaults it to the empty string"},
		{"MCC_CONFIG_CRTPREFIX", K_FLAG,
		 "string literal; src/mccdefaults.h supplies a per-target default"},
		{"MCC_CONFIG_LIBPATHS", K_FLAG,
		 "string literal; src/mccdefaults.h supplies a per-target default"},
		{"MCC_CONFIG_SYSINCLUDEPATHS", K_FLAG,
		 "string literal; src/mccdefaults.h supplies a per-target default"},
		{"MCC_CONFIG_ELFINTERP", K_FLAG,
		 "string literal; src/mccdefaults.h supplies a per-target default"},
		{"MCC_CONFIG_ELFINTERP_ARMHF", K_FLAG,
		 "string literal; src/mccdefaults.h defines it for armhf only"},
		{"MCC_CONFIG_SWITCHES", K_FLAG,
		 "string literal; src/mccdefaults.h defines it for Android only"},

		{"MCC_CONFIG_UCLIBC", K_NOSUBJECT,
		 "emitted by CMakeLists.txt and tools/build.c and read by no conditional; "
		 "ckconfig reports it DEAD and lists it in ALLOW_DEAD"},
		{"MCC_CONFIG_OPTIMIZER", K_NOSUBJECT,
		 "emitted by tools/build.c and read by no conditional"},
		{"MCC_CONFIG_JIT", K_NOSUBJECT,
		 "CMake cache variable; it selects MCC_JIT_DEFAULT and is never itself "
		 "emitted as a -D, so no conditional can test it"},
		{"MCC_CONFIG_LIBC", K_NOSUBJECT,
		 "CMake cache variable; it selects MCC_CONFIG_MUSL / MCC_CONFIG_UCLIBC and "
		 "is never itself emitted as a -D"},
		{"MCC_CONFIG_DWARF", K_NOSUBJECT,
		 "CMake cache variable; it selects MCC_CONFIG_DWARF_VERSION and is never "
		 "itself emitted as a -D"},
		{"MCC_CONFIG_NEW_MACHO", K_NOSUBJECT,
		 "CMake cache variable; it selects MCC_CONFIG_MACHO_CHAINED_FIXUPS and is "
		 "never itself emitted as a -D"},
		{"MCC_CONFIG_MINGW", K_NOSUBJECT,
		 "CMake cache variable; it selects the PE target and is never emitted as a -D"},
		{"MCC_CONFIG_AUTOCORRECT", K_NOSUBJECT,
		 "CMake cache variable; it relaxes mcc_validate_config and is never emitted "
		 "as a -D"}};

#define NREG ((int)(sizeof REGISTRY / sizeof REGISTRY[0]))

static const char *REGISTRY_HOLDERS[] = {"idiomgate.c", "ckretired.c", 0};

static int g_sites[NREG];
static int g_seen[NREG];
static int g_violations;
static int g_files;
static int g_dirs;
static int g_ruled;
static int g_subset;

#define SEEN_MAX 256
static char g_unknown[SEEN_MAX][64];
static int g_nunknown;

static int is_id(int c) {
	return isalnum((unsigned char)c) || c == '_';
}

static int reg_find(const char *tok, int len) {
	int i;
	for (i = 0; i < NREG; ++i)
		if ((int)strlen(REGISTRY[i].name) == len &&
				!strncmp(REGISTRY[i].name, tok, len))
			return i;
	return -1;
}

static int is_registry_holder(const char *base) {
	int i;
	for (i = 0; REGISTRY_HOLDERS[i]; ++i)
		if (!strcmp(REGISTRY_HOLDERS[i], base))
			return 1;
	return 0;
}

static void note_unknown(const char *tok, int len) {
	int i;
	if (len > 63)
		len = 63;
	for (i = 0; i < g_nunknown; ++i)
		if ((int)strlen(g_unknown[i]) == len && !strncmp(g_unknown[i], tok, len))
			return;
	if (g_nunknown == SEEN_MAX)
		return;
	memcpy(g_unknown[g_nunknown], tok, len);
	g_unknown[g_nunknown][len] = 0;
	++g_nunknown;
}

static void violate(const char *path, int ln, const char *macro, const char *line,
										const char *why) {
	while (*line == ' ' || *line == '\t')
		++line;
	printf("  %s:%d: %s: %s [%s]\n", path, ln, macro, line, why);
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

static int classify(const char *path, int ln, const char *line, const char *tok,
										int len) {
	int idx = reg_find(tok, len);
	g_ruled++;
	if (idx < 0) {
		char name[64];
		int n = len > 63 ? 63 : len;
		memcpy(name, tok, n);
		name[n] = 0;
		violate(path, ln, name, line,
						"config macro in a conditional with no row in tools/idiomgate.c's "
						"REGISTRY");
		return -1;
	}
	g_sites[idx]++;
	if (REGISTRY[idx].kind == K_NOSUBJECT) {
		violate(path, ln, REGISTRY[idx].name, line,
						"registered as reachable by no conditional, yet here is one");
		return -1;
	}
	return idx;
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
			int len, was_defined = *pend_defined;
			while (is_id(*s))
				++s;
			len = (int)(s - b);
			*pend_defined = 0;
			if (!was_defined && len == 7 && !strncmp(b, "defined", 7)) {
				*pend_defined = 1;
			} else if (len > 11 && !strncmp(b, "MCC_CONFIG_", 11)) {
				int idx = classify(path, ln, line, b, len);
				if (idx >= 0 && was_defined && REGISTRY[idx].kind == K_VALUE)
					violate(path, ln, REGISTRY[idx].name, line,
									"defined() on value-kind config macro");
				else if (idx >= 0 && !was_defined && REGISTRY[idx].kind == K_FLAG)
					violate(path, ln, REGISTRY[idx].name, line,
									"flag-kind config macro tested as a value");
			}
		} else {
			++s;
		}
	}
}

static void harvest(const char *path, char *text) {
	char *q;
	(void)path;
	for (q = text; (q = strstr(q, "MCC_CONFIG_")) != NULL;) {
		char *r = q;
		int len;
		while (is_id(*r))
			++r;
		len = (int)(r - q);
		if (len > 11) {
			int idx = reg_find(q, len);
			if (idx >= 0)
				g_seen[idx]++;
			else
				note_unknown(q, len);
		}
		q = r;
	}
}

static int scan_file(const char *path, int is_dir, void *ud) {
	const char *base;
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
	base = strrchr(path, '/');
	base = base ? base + 1 : path;
	if (is_registry_holder(base))
		return 0;
	if (!(text = ts_read_file(path, NULL)))
		return 0;
	g_files++;
	harvest(path, text);
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
			g_dirs++;
			scan_expr(path, ln, p, rest, &pend_defined);
		} else if (k == D_IFDEF || k == D_IFNDEF) {
			const char *name;
			int nlen, idx;
			g_dirs++;
			name = first_ident(rest, &nlen);
			if (name && nlen > 11 && !strncmp(name, "MCC_CONFIG_", 11) &&
					(idx = classify(path, ln, p, name, nlen)) >= 0 &&
					REGISTRY[idx].kind == K_VALUE) {
				if (k == D_IFDEF) {
					violate(path, ln, REGISTRY[idx].name, p,
									"#ifdef on value-kind config macro");
				} else {
					const char *dname = NULL;
					int dlen = 0, ok = 0;
					if (e) {
						char *n = e + 1, *ne = strchr(n, '\n');
						int nl = ne ? (int)(ne - n) : (int)strlen(n);
						char save = n[nl];
						n[nl] = 0;
						if (directive(n, &rest) == D_DEFINE &&
								(dname = first_ident(rest, &dlen)) != NULL && dlen == nlen &&
								!strncmp(dname, name, nlen))
							ok = 1;
						n[nl] = save;
					}
					if (!ok)
						violate(path, ln, REGISTRY[idx].name, p,
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

static void print_registry(void) {
	static const char *KN[] = {"value-kind", "flag-kind ", "no-subject"};
	int i;
	printf("idiom-gate registry: %d MCC_CONFIG_* macro(s)\n", NREG);
	for (i = 0; i < NREG; ++i)
		printf("  %s  %-32s sites=%-4d text=%-4d %s\n", KN[REGISTRY[i].kind],
					 REGISTRY[i].name, g_sites[i], g_seen[i], REGISTRY[i].why);
	for (i = 0; REGISTRY_HOLDERS[i]; ++i)
		printf("  skipped     %-32s holds a name registry, not a use of one\n",
					 REGISTRY_HOLDERS[i]);
	printf("  the retired MCC_CONFIG_* names are tools/ckretired.c's subject and "
				 "must not appear here\n");
}

int main(int argc, char **argv) {
	static const char *defaults[] = {"src", "tools", "runtime", "include", 0};
	const char *roots[TS_ARGV_MAX];
	int i, n = 0, list = 0, checked = 0, reached = 0, nosubject = 0, holes = 0;

	for (i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "--subset"))
			g_subset = 1;
		else if (!strcmp(argv[i], "--registry"))
			list = 1;
		else if (n < TS_ARGV_MAX)
			roots[n++] = argv[i];
	}
	if (!n)
		for (n = 0; defaults[n]; ++n)
			roots[n] = defaults[n];

	for (i = 0; i < n; ++i) {
		int isd;
		if (host_stat(roots[i], &isd, NULL, NULL) || !isd) {
			fprintf(stderr, "idiomgate: not a directory: %s\n", roots[i]);
			return 2;
		}
		host_dir_walk(roots[i], 1, scan_file, NULL);
	}

	for (i = 0; i < NREG; ++i) {
		if (REGISTRY[i].kind == K_NOSUBJECT) {
			++nosubject;
			continue;
		}
		++checked;
		if (g_sites[i])
			++reached;
	}

	printf("idiom-gate subject: %d file(s) scanned, %d conditional(s) examined, "
				 "%d config-macro site(s) ruled on\n",
				 g_files, g_dirs, g_ruled);
	printf("idiom-gate coverage: %d of %d registered MCC_CONFIG_* macro(s) carry "
				 "an enforced idiom (%.1f%%), %d of those reached by a conditional "
				 "(%.1f%%); %d refused with a reason (--registry lists them)\n",
				 checked, NREG, 100.0 * checked / NREG, reached,
				 checked ? 100.0 * reached / checked : 0.0, nosubject);

	if (list)
		print_registry();

	if (!g_files || !g_dirs || !g_ruled) {
		fprintf(stderr,
						"idiom-gate FAIL - this run has no subject. A walk that read no\n"
						"file, or found no #if/#ifdef, or reached none of the %d checked\n"
						"macros in the REGISTRY, prints exactly the same 'OK' as a clean\n"
						"one. Nothing was checked, so nothing may be reported.\n",
						checked);
		return 1;
	}

	if (g_nunknown) {
		for (i = 0; i < g_nunknown; ++i)
			printf("  %s: appears in the walked tree and has no row in the "
						 "REGISTRY\n",
						 g_unknown[i]);
		fprintf(stderr,
						"idiom-gate FAIL - %d MCC_CONFIG_* name(s) above are outside the\n"
						"registry, so the denominator this gate prints is not the tree's.\n"
						"Give each one a row: K_VALUE if it is numeric, K_FLAG if it is a\n"
						"string literal or presence-only, K_NOSUBJECT with the reason no\n"
						"conditional can test it.\n",
						g_nunknown);
		return 1;
	}

	if (!g_subset) {
		for (i = 0; i < NREG; ++i) {
			if (REGISTRY[i].kind != K_NOSUBJECT && !g_sites[i]) {
				printf("  %s: registered as %s and enforced over zero conditional(s)\n",
							 REGISTRY[i].name,
							 REGISTRY[i].kind == K_VALUE ? "value-kind" : "flag-kind");
				++holes;
			}
			if (!g_seen[i]) {
				printf("  %s: registered and its name occurs nowhere in the walked "
							 "tree\n",
							 REGISTRY[i].name);
				++holes;
			}
		}
		if (holes) {
			fprintf(stderr,
							"idiom-gate FAIL - %d registry row(s) above hold nothing. A row\n"
							"whose macro has no conditional cannot fail, and a row whose\n"
							"macro has left the tree inflates the denominator. Move the first\n"
							"kind to K_NOSUBJECT with a reason, and delete the second.\n",
							holes);
			return 1;
		}
	}

	if (g_violations) {
		fprintf(stderr,
						"idiom-gate invariant violated - config macro tested with the\n"
						"wrong idiom (value-kind is numeric: '#if X', or '#ifndef X'\n"
						"immediately followed by '#define X'; flag-kind is a string literal\n"
						"or presence-only: '#ifdef'/'#ifndef'/'defined'): %d violation(s)\n"
						"above\n",
						g_violations);
		return 1;
	}
	printf("idiom-gate invariant OK: canonical config-macro test idioms hold\n");
	return 0;
}
