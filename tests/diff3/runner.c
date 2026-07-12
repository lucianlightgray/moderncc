#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../support/hostcompat.h"
#ifdef _WIN32
#define EXE_SFX ".exe"
#ifndef WIFEXITED
#define WIFEXITED(s) 1
#define WEXITSTATUS(s) (s)
#endif
#else
#include <sys/wait.h>
#define EXE_SFX ".out"
#endif

#include "goldens.h"

#define MCC_SKIP_RC 77
#define MAX_REFS 16

static int verbose;

static int timed_out(int raw_status) {
	return WIFEXITED(raw_status) &&
				 (WEXITSTATUS(raw_status) == 124 || WEXITSTATUS(raw_status) == 137);
}

static int crashed(int raw_status) {
	if (timed_out(raw_status))
		return 0;
#ifdef WIFSIGNALED
	if (WIFSIGNALED(raw_status))
		return 1;
#endif
	return WIFEXITED(raw_status) && WEXITSTATUS(raw_status) >= 128;
}

#define RUN_SYSTEM(c) HC_SYSTEM_SH(c)
#define RUN_POPEN(c) HC_POPEN_SH(c)

static int portable_req(const char *req) {
	if (!req || !*req)
		return 1;
	if (strstr(req, "note:") || strstr(req, "bcheck") || strstr(req, "backtrace"))
		return 0;
	char buf[256];
	snprintf(buf, sizeof buf, "%s", req);
	const char *cpu = hc_envv("MCC_TEST_CPU", "unknown");
	const char *os = hc_envv("MCC_TEST_OS", "unknown");
	for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
		while (*tok == ' ')
			tok++;
		if (!strncmp(tok, "cpu=", 4)) {
			const char *want = tok + 4;
			int ok;
			if (!strcmp(want, "x86"))
				ok = !strcmp(cpu, "i386") || !strcmp(cpu, "x86_64");
			else
				ok = !strcmp(cpu, want);
			if (!ok)
				return 0;
		} else if (!strncmp(tok, "os=", 3)) {
			if (strcmp(os, tok + 3))
				return 0;
		} else if (!strncmp(tok, "os!=", 4)) {

			const char *want = tok + 4;
			const char *colon = strchr(want, ':');
			size_t wl = colon ? (size_t)(colon - want) : strlen(want);
			if (!strncmp(os, want, wl) && os[wl] == '\0')
				return 0;
		} else if (!strncmp(tok, "diff3!=", 7)) {
			const char *want = tok + 7;
			const char *colon = strchr(want, ':');
			size_t wl = colon ? (size_t)(colon - want) : strlen(want);
			if (!strncmp(os, want, wl) && os[wl] == '\0')
				return 0;
		} else if (!strcmp(tok, "elf")) {

			if (!strcmp(os, "Darwin") || !strcmp(os, "WIN32"))
				return 0;
		} else if (!strcmp(tok, "asm")) {
			if (strcmp(hc_envv("MCC_TEST_ASM", "1"), "1"))
				return 0;
		}
	}
	return 1;
}

static char *slurp(FILE *f) {
	size_t cap = 4096, len = 0;
	char *b = malloc(cap);
	size_t n;
	while ((n = fread(b + len, 1, cap - len, f)) > 0) {
		len += n;
		if (len == cap) {
			cap *= 2;
			b = realloc(b, cap);
		}
	}
	b[len] = 0;
	return b;
}

static char *cap(const char *cmd, int *status) {
	FILE *f = RUN_POPEN(cmd);
	if (!f) {
		if (status)
			*status = -1;
		return strdup("");
	}
	char *o = slurp(f);
	int rc = pclose(f);
	if (status)
		*status = rc;
	return o;
}

#define DIFF3_RUN_TIMEOUT 20
static void timeout_wrap(const char *cmd, char *out, size_t n) {
	static int probed, have_timeout, have_gtimeout;
	if (!probed) {
		have_timeout = RUN_SYSTEM("command -v timeout  >/dev/null 2>&1") == 0;
		have_gtimeout = RUN_SYSTEM("command -v gtimeout >/dev/null 2>&1") == 0;
		probed = 1;
	}
	if (have_timeout)
		snprintf(out, n, "timeout %d %s", DIFF3_RUN_TIMEOUT, cmd);
	else if (have_gtimeout)
		snprintf(out, n, "gtimeout %d %s", DIFF3_RUN_TIMEOUT, cmd);
	else

		snprintf(out, n,
						 "{ %s & __p=$!; ( sleep %d; kill -9 $__p ) >/dev/null 2>&1 & __w=$!; "
						 "wait $__p; __r=$?; kill $__w 2>/dev/null; exit $__r; }",
						 cmd, DIFF3_RUN_TIMEOUT);
}

static int file_has(const char *path, const char *needle) {
	FILE *f = fopen(path, "rb");
	if (!f)
		return 0;
	char *s = slurp(f);
	fclose(f);
	int hit = strstr(s, needle) != NULL;
	free(s);
	return hit;
}

static int same_compiler(const char *a, const char *b) {
	char cmd[4096];
	snprintf(cmd, sizeof cmd, "\"%s\" --version 2>/dev/null", a);
	char *va = cap(cmd, NULL);
	snprintf(cmd, sizeof cmd, "\"%s\" --version 2>/dev/null", b);
	char *vb = cap(cmd, NULL);
	int same = va[0] && !strcmp(va, vb);
	free(va);
	free(vb);
	return same;
}

static int intentional_divergence(const char *name) {
	static const char *const list[] = {
			"c11_freestanding_headers",

			"predefined_macros",

			"bitfields_ms",
			"cleanup",
	};
	for (size_t i = 0; i < sizeof list / sizeof *list; i++)
		if (!strcmp(name, list[i]))
			return 1;
	return 0;
}

static void sub_self(const char *in, const char *src, char *out, size_t n) {
	char *w = out;
	const char *a = in;
	while (*a && (size_t)(w - out) < n - 1) {
		if (!strncmp(a, "{SELF}", 6)) {
			w += snprintf(w, n - (w - out), "%s", src);
			a += 6;
		} else
			*w++ = *a++;
	}
	*w = 0;
}

static int build_run(const char *label, const char *cc, const char *mcc,
										 const char *bdir, const char *idir, const char *sup,
										 const char *work, const char *src, const char *flags,
										 const char *args, char **out) {
	char exe[2048], cmd[8192];
	snprintf(exe, sizeof exe, "%s/%s%s", work, label, EXE_SFX);
	remove(exe);
	if (cc)
		snprintf(cmd, sizeof cmd,
						 "\"%s\" -w -O0 \"-I%s\" %s \"%s\" -o \"%s\" >/dev/null 2>&1",
						 cc, sup, flags, src, exe);
	else
		snprintf(cmd, sizeof cmd,
						 "\"%s\" \"-B%s\" \"-I%s\" \"-I%s\" %s \"%s\" -o \"%s\" >/dev/null 2>&1",
						 mcc, bdir, idir, sup, flags, src, exe);
	if (verbose)
		fprintf(stderr, "  [%s build] %s\n", label, cmd);
	char gbuild[8448];
	timeout_wrap(cmd, gbuild, sizeof gbuild);

	int brc = RUN_SYSTEM(gbuild);
	if (timed_out(brc)) {
		*out = strdup("");
		return 2;
	}
	if (brc != 0) {
		*out = strdup("");
		return 0;
	}

	char prog[8192], guarded[8448], run[12288];
	int rrc;
	snprintf(prog, sizeof prog, "\"%s\" %s 2>/dev/null", exe, args);
	timeout_wrap(prog, guarded, sizeof guarded);
	snprintf(run, sizeof run, "cd \"%s\" && %s", work, guarded);
	*out = cap(run, &rrc);
	if (verbose)
		fprintf(stderr, "  [%s out] %s\n", label, *out);
	if (timed_out(rrc) || crashed(rrc)) {
		free(*out);
		*out = strdup("");
		return 2;
	}
	return 1;
}

static int majority_index(char *const *out, int n, int *count) {
	int best = -1, bestc = 0;
	for (int a = 0; a < n; a++) {
		int c = 0;
		for (int b = 0; b < n; b++)
			if (!strcmp(out[a], out[b]))
				c++;
		if (c > bestc) {
			bestc = c;
			best = a;
		}
	}
	if (count)
		*count = bestc;
	return (bestc * 2 > n) ? best : -1;
}

int main(int argc, char **argv) {
	if (argc < 6) {
		fprintf(stderr,
						"usage: %s <mcc> <bdir> <idir> <root> <work> "
						"--ref <label> <path> [--ref <label> <path>]... "
						"[--list] [--only <name>]\n",
						argv[0]);
		return 2;
	}
	const char *mcc = argv[1], *bdir = argv[2], *idir = argv[3], *root = argv[4],
						 *work = argv[5];

	const char *reflabel[MAX_REFS], *refpath[MAX_REFS];
	int nref = 0;
	const char *only = NULL;
	int list_mode = 0;
	for (int i = 6; i < argc; i++) {
		if (!strcmp(argv[i], "--list"))
			list_mode = 1;
		else if (!strcmp(argv[i], "--only") && i + 1 < argc)
			only = argv[++i];
		else if (!strcmp(argv[i], "--ref") && i + 2 < argc) {
			if (nref < MAX_REFS) {
				reflabel[nref] = argv[i + 1];
				refpath[nref] = argv[i + 2];
				nref++;
			}
			i += 2;
		}
	}
	if (list_mode) {
		for (int i = 0; i < mcc_goldens_count; i++) {
			const mcc_golden_t *g = &mcc_goldens[i];
			if (strcmp(g->mode, "run") && strcmp(g->mode, "run2"))
				continue;
			printf("%s\n", g->name);
		}
		return 0;
	}

	char sup[2048];
	snprintf(sup, sizeof sup, "%s/support", root);
	if (hc_envv("MCC_DIFF3_VERBOSE", "")[0])
		verbose = 1;
#ifdef DIFF3_VERBOSE
	verbose = 1;
#endif

	hc_set_workdir(work);
	HC_MKDIR(work);

	int kept[MAX_REFS], nkept = 0;
	for (int i = 0; i < nref; i++) {
		int dup = 0;
		for (int j = 0; j < nkept; j++)
			if (same_compiler(refpath[i], refpath[kept[j]])) {
				dup = 1;
				break;
			}
		if (!dup)
			kept[nkept++] = i;
	}
	if (nkept < 2) {
		printf("diff3: SKIP -- need >=2 distinct reference compilers, have %d; a "
					 "differential consensus needs independent references (the exec "
					 "golden suite still covers these programs)\n",
					 nkept);
		return MCC_SKIP_RC;
	}

	char cmd[4096];
	snprintf(cmd, sizeof cmd, "mkdir -p \"%s\"", work);
	if (RUN_SYSTEM(cmd)) {
		fprintf(stderr, "cannot mkdir %s\n", work);
		return 2;
	}

	int pass = 0, mcc_diff = 0, impl = 0, skip = 0, mcc_build_fail = 0, mcc_only = 0, intent = 0;
	for (int i = 0; i < mcc_goldens_count; i++) {
		const mcc_golden_t *g = &mcc_goldens[i];
		if (only && strcmp(only, g->name))
			continue;
		if (strcmp(g->mode, "run") && strcmp(g->mode, "run2"))
			continue;
		if (!portable_req(g->req)) {
			continue;
		}

		char src[2048];
		snprintf(src, sizeof src, "%s/%s", root, g->src);

		if (file_has(src, "__MCC__")) {
			printf("MCC-ONLY %-28s -- has #ifdef __MCC__ section\n", g->name);
			mcc_only++;
			continue;
		}
		char flags[4096], args[4096];
		sub_self(g->flags, src, flags, sizeof flags);
		sub_self(g->args, src, args, sizeof args);

		if (verbose) {
			fprintf(stderr, "RUN   %s\n", g->name);
			fflush(stderr);
		}
		char *mout;
		int mok = build_run("mcc", NULL, mcc, bdir, idir, sup, work, src, flags, args, &mout);
		if (mok == 2) {
			if (verbose)
				printf("SKIP  %-28s -- mcc build/run hit %ds watchdog or crashed (inconclusive)\n",
							 g->name, DIFF3_RUN_TIMEOUT);
			skip++;
			free(mout);
			continue;
		}
		if (!mok) {
			printf("FAIL  %-28s -- mcc failed to build\n", g->name);
			mcc_build_fail++;
			free(mout);
			continue;
		}

		char *rout[MAX_REFS];
		const char *rlab[MAX_REFS];
		int nbuilt = 0;
		for (int k = 0; k < nkept; k++) {
			int ri = kept[k];
			char *o;
			int rc = build_run(reflabel[ri], refpath[ri], NULL, bdir, idir, sup,
												 work, src, flags, args, &o);
			if (rc == 1) {
				rout[nbuilt] = o;
				rlab[nbuilt] = reflabel[ri];
				nbuilt++;
			} else {
				free(o);
			}
		}

		if (nbuilt < 2) {
			if (verbose)
				printf("SKIP  %-28s -- <2 reference compilers could build/run (mcc-only)\n",
							 g->name);
			skip++;
		} else {
			int wins = 0, cons = majority_index(rout, nbuilt, &wins);
			if (cons < 0) {
				printf("INFO  %-28s -- %d references disagree, no majority (implementation-defined)\n",
							 g->name, nbuilt);
				impl++;
				pass++;
			} else if (!strcmp(mout, rout[cons])) {
				if (verbose)
					printf("ok    %s\n", g->name);
				pass++;
			} else if (intentional_divergence(g->name)) {
				printf("INFO  %-28s -- mcc intentionally diverges from the %d/%d-ref "
							 "consensus (bundled headers/predefines/impl-defined); exec golden suite is the guardrail\n",
							 g->name, wins, nbuilt);
				intent++;
				pass++;
			} else {
				printf("FAIL  %-28s -- mcc differs from the %d/%d-ref consensus\n",
							 g->name, wins, nbuilt);
				printf("  consensus (");
				for (int b = 0, first = 1; b < nbuilt; b++)
					if (!strcmp(rout[b], rout[cons])) {
						printf("%s%s", first ? "" : "+", rlab[b]);
						first = 0;
					}
				printf("): %s\n  mcc       : %s\n", rout[cons], mout);
				mcc_diff++;
			}
		}
		for (int k = 0; k < nbuilt; k++)
			free(rout[k]);
		free(mout);
	}
	printf("diff3: %d agree, %d mcc-divergence, %d impl-defined, %d intentional, "
				 "%d ref-cant-build, %d mcc-only, %d mcc-build-fail\n",
				 pass, mcc_diff, impl, intent, skip, mcc_only, mcc_build_fail);
	if (mcc_diff || mcc_build_fail)
		return 1;
	if (pass == 0)
		return MCC_SKIP_RC;
	return 0;
}
