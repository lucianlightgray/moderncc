#include "toolsupport.h"

typedef struct {
	char **v;
	int n, cap;
} Vec;

static void vec_add(Vec *s, char *k) {
	if (s->n == s->cap) {
		s->cap = s->cap ? s->cap * 2 : 16;
		s->v = realloc(s->v, s->cap * sizeof *s->v);
	}
	s->v[s->n++] = k;
}

static char *json_string(const char **pp) {
	const char *p = *pp;
	char *out, *o;
	if (*p != '"')
		return NULL;
	++p;
	out = o = malloc(strlen(p) + 1);
	while (*p && *p != '"') {
		if (*p == '\\' && p[1]) {
			++p;
			switch (*p) {
			case 'n': *o++ = '\n'; break;
			case 't': *o++ = '\t'; break;
			case 'r': *o++ = '\r'; break;
			case 'b': *o++ = '\b'; break;
			case 'f': *o++ = '\f'; break;
			case 'u': {
				int i, v = 0;
				for (i = 0; i < 4 && p[1]; ++i) {
					int c = p[1];
					int d = c >= '0' && c <= '9' ? c - '0'
						: c >= 'a' && c <= 'f' ? c - 'a' + 10
						: c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
					if (d < 0)
						break;
					v = v * 16 + d;
					++p;
				}
				*o++ = v < 0x80 ? (char)v : '?';
				break;
			}
			default: *o++ = *p; break;
			}
			++p;
		} else {
			*o++ = *p++;
		}
	}
	*o = 0;
	*pp = *p == '"' ? p + 1 : p;
	return out;
}

static int is_ws(int c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int shell_token(const char **pp, char *out, int outsz) {
	const char *p = *pp;
	int o = 0;
	while (is_ws((unsigned char)*p))
		++p;
	if (!*p) {
		*pp = p;
		return 0;
	}
	while (*p && !is_ws((unsigned char)*p)) {
		if (*p == '\\') {
			++p;
			if (*p == '\n')
				++p;
			else if (*p && o < outsz - 1)
				out[o++] = *p++;
		} else if (*p == '\'') {
			for (++p; *p && *p != '\''; ++p)
				if (o < outsz - 1)
					out[o++] = *p;
			if (*p == '\'')
				++p;
		} else if (*p == '"') {
			for (++p; *p && *p != '"';) {
				if (*p == '\\' &&
						(p[1] == '"' || p[1] == '\\' || p[1] == '$' || p[1] == '`' ||
						 p[1] == '\n')) {
					++p;
					if (*p == '\n')
						++p;
					else if (o < outsz - 1)
						out[o++] = *p++;
				} else if (o < outsz - 1) {
					out[o++] = *p++;
				} else {
					++p;
				}
			}
			if (*p == '"')
				++p;
		} else if (o < outsz - 1) {
			out[o++] = *p++;
		} else {
			++p;
		}
	}
	out[o] = 0;
	*pp = p;
	return 1;
}

static int ends_with(const char *s, const char *suf) {
	int ls = (int)strlen(s), lf = (int)strlen(suf);
	return ls >= lf && !strcmp(s + ls - lf, suf);
}

static int derive_defs(const char *json_path, Vec *defs, char *root, int rootsz) {
	char *text = ts_read_file(json_path, NULL);
	const char *p;
	char *file = NULL, *output = NULL, *command = NULL, *match = NULL;
	if (!text) {
		fprintf(stderr, "fixpointgate: cannot read %s\n", json_path);
		return -1;
	}
	for (p = text; *p;) {
		if (*p == '{') {
			file = output = command = NULL;
			++p;
		} else if (*p == '}') {
			if (file && output && command && ends_with(file, "src/mcc.c") &&
					strstr(output, "/mcc.dir/")) {
				match = command;
				command = NULL;
				free(file);
				free(output);
				break;
			}
			free(file);
			free(output);
			free(command);
			file = output = command = NULL;
			++p;
		} else if (*p == '"') {
			char *key = json_string(&p);
			char **slot;
			while (is_ws((unsigned char)*p))
				++p;
			if (*p == ':')
				++p;
			while (is_ws((unsigned char)*p))
				++p;
			slot = !strcmp(key, "file") ? &file
				: !strcmp(key, "output") ? &output
				: !strcmp(key, "command") ? &command : NULL;
			free(key);
			if (*p == '"') {
				char *val = json_string(&p);
				if (slot) {
					free(*slot);
					*slot = val;
				} else {
					free(val);
				}
			}
		} else {
			++p;
		}
	}
	if (!match) {
		fprintf(stderr,
						"fixpointgate: no src/mcc.c compile entry in %s\n", json_path);
		free(text);
		return -1;
	}
	{
		const char *fp = match;
		char tok[8192];
		while (shell_token(&fp, tok, sizeof tok))
			if (!strncmp(tok, "-D", 2) && !strstr(tok, "MCC_EMBED_MCCRT"))
				vec_add(defs, strdup(tok));
	}
	{
		int fl = -1, i;
		char tok[8192];
		const char *sp = match;
		while (shell_token(&sp, tok, sizeof tok))
			if (ends_with(tok, "src/mcc.c")) {
				snprintf(root, rootsz, "%s", tok);
				fl = (int)strlen(root);
				break;
			}
		if (fl < 0) {
			fprintf(stderr, "fixpointgate: no source path in compile command\n");
			free(match);
			free(text);
			return -1;
		}
		for (i = fl; i > 0; --i)
			if (root[i - 1] == '/') {
				root[i - 1] = 0;
				break;
			}
		if (ends_with(root, "/src"))
			root[strlen(root) - 4] = 0;
	}
	free(match);
	free(text);
	return 0;
}

static int compile_stage(const char *mcc, const char *builddir, const char *root,
												 const char *out, Vec *defs) {
	Argv v;
	char barg[4096], iarg[11][4096], src_c[4096];
	static const char *inc_sub[] = {
			NULL, "src", "src/arch/i386", "src/arch/x86_64", "src/arch/arm",
			"src/arch/arm64", "src/arch/riscv64", "src/objfmt", "src/formats",
			"include", NULL};
	int i, rc;
	v.n = 0;
	ts_arg(&v, mcc);
	snprintf(barg, sizeof barg, "-B%s", builddir);
	ts_arg(&v, barg);
	snprintf(src_c, sizeof src_c, "%s/src/mcc.c", root);
	ts_arg(&v, src_c);
	ts_arg(&v, "-o");
	ts_arg(&v, out);
	for (i = 0; i < 11; ++i) {
		if (i == 0)
			snprintf(iarg[i], sizeof iarg[i], "-I%s", builddir);
		else if (i == 10)
			snprintf(iarg[i], sizeof iarg[i], "-I%s", root);
		else
			snprintf(iarg[i], sizeof iarg[i], "-I%s/%s", root, inc_sub[i]);
		ts_arg(&v, iarg[i]);
	}
	for (i = 0; i < defs->n; ++i)
		ts_arg(&v, defs->v[i]);
	rc = host_spawn_wait(ts_argz(&v));
	return rc;
}

int main(int argc, char **argv) {
	const char *builddir = argc > 1 ? argv[1] : "cmake-debug";
	char json[4096], mcc[4096], root[4096];
	char stage2[4096], stage3[4096], stage4[4096];
	Vec defs = {0};
	int rc;

	snprintf(json, sizeof json, "%s/compile_commands.json", builddir);
	if (derive_defs(json, &defs, root, sizeof root))
		return 2;

	snprintf(mcc, sizeof mcc, "%s/mcc", builddir);
	snprintf(stage2, sizeof stage2, "%s/fixpoint-stage2", builddir);
	snprintf(stage3, sizeof stage3, "%s/fixpoint-stage3", builddir);
	snprintf(stage4, sizeof stage4, "%s/fixpoint-stage4", builddir);

	rc = compile_stage(mcc, builddir, root, stage2, &defs);
	if (!rc)
		rc = compile_stage(stage2, builddir, root, stage3, &defs);
	if (!rc)
		rc = compile_stage(stage3, builddir, root, stage4, &defs);
	if (rc) {
		fprintf(stderr, "fixpointgate: self-host compile failed (rc=%d)\n", rc);
		return 1;
	}

	if (ts_file_equal(stage2, stage3) != 1 ||
			ts_file_equal(stage3, stage4) != 1) {
		fprintf(stderr,
						"fixpointgate: FIXPOINT MISMATCH - stage2/stage3/stage4 differ\n");
		return 1;
	}

	remove(stage2);
	remove(stage3);
	remove(stage4);
	printf("FIXPOINT OK (stage2==stage3==stage4)\n");
	return 0;
}
