#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../support/hostcompat.h"

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#endif

#include "goldens.h"

static char *xstrdup(const char *s) {
	char *p = malloc(strlen(s) + 1);
	strcpy(p, s);
	return p;
}

#define MCC_SKIP_RC 77

static int req_met(const char *req, char *reason, size_t rn) {
	if (!req || !*req)
		return 1;
	char buf[256];
	snprintf(buf, sizeof buf, "%s", req);
	const char *cpu = hc_envv("MCC_TEST_CPU", "unknown");
	const char *os = hc_envv("MCC_TEST_OS", "unknown");
	for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
		while (*tok == ' ')
			tok++;
		if (!strncmp(tok, "note:", 5)) {
			snprintf(reason, rn, "%s", tok + 5);
			return 0;
		} else if (!strncmp(tok, "cpu=", 4)) {
			const char *want = tok + 4;
			int ok;
			if (!strcmp(want, "x86"))
				ok = !strcmp(cpu, "i386") || !strcmp(cpu, "x86_64");
			else
				ok = !strcmp(cpu, want);
			if (!ok) {
				snprintf(reason, rn, "requires %s target (host target: %s)", want, cpu);
				return 0;
			}
		} else if (!strncmp(tok, "os=", 3)) {
			const char *want = tok + 3;
			if (strcmp(os, want)) {
				snprintf(reason, rn, "requires %s target OS (host: %s)", want, os);
				return 0;
			}
		} else if (!strcmp(tok, "elf")) {

			if (!strcmp(os, "Darwin") || !strcmp(os, "WIN32")) {
				snprintf(reason, rn, "requires an ELF target (host: %s)", os);
				return 0;
			}
		} else if (!strncmp(tok, "os!=", 4)) {

			const char *want = tok + 4;
			const char *colon = strchr(want, ':');
			char wbuf[64];
			size_t wl = colon ? (size_t)(colon - want) : strlen(want);
			if (wl >= sizeof wbuf)
				wl = sizeof wbuf - 1;
			memcpy(wbuf, want, wl);
			wbuf[wl] = 0;
			if (!strcmp(os, wbuf)) {
				if (colon && colon[1])
					snprintf(reason, rn, "%s", colon + 1);
				else
					snprintf(reason, rn, "not applicable to the %s target", wbuf);
				return 0;
			}
		} else if (!strncmp(tok, "skipon=", 7)) {
			const char *spec = tok + 7;
			const char *slash = strchr(spec, '/');
			const char *colon = strchr(spec, ':');
			if (slash) {
				char wcpu[32], wos[32];
				const char *oend = colon ? colon : spec + strlen(spec);
				size_t cl = (size_t)(slash - spec);
				size_t ol = (size_t)(oend - (slash + 1));
				if (cl >= sizeof wcpu)
					cl = sizeof wcpu - 1;
				if (ol >= sizeof wos)
					ol = sizeof wos - 1;
				memcpy(wcpu, spec, cl);
				wcpu[cl] = 0;
				memcpy(wos, slash + 1, ol);
				wos[ol] = 0;
				if (!strcmp(cpu, wcpu) && !strcmp(os, wos)) {
					if (colon && colon[1])
						snprintf(reason, rn, "%s", colon + 1);
					else
						snprintf(reason, rn, "not run on %s/%s", wcpu, wos);
					return 0;
				}
			}
		} else if (!strcmp(tok, "asm")) {
			if (strcmp(hc_envv("MCC_TEST_ASM", "1"), "1")) {
				snprintf(reason, rn, "requires integrated assembler (MCC_CONFIG_ASM)");
				return 0;
			}
		} else if (!strcmp(tok, "bcheck")) {
			if (strcmp(hc_envv("MCC_TEST_BCHECK", "0"), "1")) {
				snprintf(reason, rn, "requires bounds checker (MCC_CONFIG_BCHECK)");
				return 0;
			}
		} else if (!strcmp(tok, "backtrace")) {
			if (strcmp(hc_envv("MCC_TEST_BACKTRACE", "0"), "1")) {
				snprintf(reason, rn, "requires backtrace support (MCC_CONFIG_BACKTRACE)");
				return 0;
			}
		}
	}
	return 1;
}

static int has_dot_run(const char *s) {
	int n = 0;
	for (; *s; s++) {
		if (*s == '.') {
			if (++n >= 3)
				return 1;
		} else
			n = 0;
	}
	return 0;
}

static int glob_eq(const char *pat, const char *str) {
	const char *s = str, *p = pat, *star_p = NULL, *star_s = NULL;
	while (*s) {
		if (*p == '.') {
			while (*p == '.')
				p++;
			star_p = p;
			star_s = s;
		} else if (*p && *p == *s) {
			p++;
			s++;
		} else if (star_p) {
			s = ++star_s;
			p = star_p;
		} else
			return 0;
	}
	while (*p == '.')
		p++;
	return *p == '\0';
}

HC_UNUSED static char *slurp(FILE *f, size_t *outlen) {
	size_t cap = 4096, len = 0;
	char *buf = malloc(cap);
	size_t n;
	while ((n = fread(buf + len, 1, cap - len, f)) > 0) {
		len += n;
		if (len == cap) {
			cap *= 2;
			buf = realloc(buf, cap);
		}
	}
	buf[len] = 0;
	if (outlen)
		*outlen = len;
	return buf;
}

/* Wall-clock cap (seconds) for a single captured command. A wedged child --
 * infinite-looping from a codegen miscompile, deadlocked, or crash-suspended
 * by macOS ReportCrash awaiting a report -- keeps the write end of the capture
 * pipe open, so a bare popen()+fread() would block until the CI job is killed
 * by hand (observed: a 48-min stall on select_branchless). The default matches
 * the CI job timeout-minutes and is overridable via MCC_TEST_TIMEOUT; 0 or
 * negative disables the cap. Belt to ctest's --timeout suspenders. */
static long run_capture_timeout(void) {
	const char *s = hc_envv("MCC_TEST_TIMEOUT", "1200");
	char *end;
	long v = strtol(s, &end, 10);
	return (end == s) ? 1200 : v;
}

#ifndef _WIN32
/* POSIX capture that can actually kill a hung child: run cmd via /bin/sh in its
 * OWN process group, poll the output pipe against a deadline, and killpg the
 * whole group on timeout (SIGKILL reaches grandchildren a bare pclose can't).
 * On timeout, *status is set nonzero and a marker is appended so the offending
 * golden fails loudly and by name instead of stalling the suite. */
static char *run_capture(const char *cmd, int *status) {
	int pipefd[2];
	if (pipe(pipefd) != 0) {
		if (status)
			*status = -1;
		return xstrdup("");
	}
	pid_t pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		if (status)
			*status = -1;
		return xstrdup("");
	}
	if (pid == 0) {
		setpgid(0, 0);
		dup2(pipefd[1], 1);
		dup2(pipefd[1], 2);
		close(pipefd[0]);
		close(pipefd[1]);
		execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
		_exit(127);
	}
	setpgid(pid, pid); /* race-free with the child's own setpgid */
	close(pipefd[1]);

	long timeout_s = run_capture_timeout();
	size_t cap = 4096, len = 0;
	char *buf = malloc(cap);
	time_t start = time(NULL);
	int killed = 0;
	for (;;) {
		if (timeout_s > 0 && (long)(time(NULL) - start) >= timeout_s) {
			killpg(pid, SIGKILL);
			killed = 1;
			break;
		}
		struct pollfd pfd = {pipefd[0], POLLIN, 0};
		int pr = poll(&pfd, 1, 1000); /* 1s slices; deadline re-checked each loop */
		if (pr < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (pr == 0)
			continue;
		ssize_t n = read(pipefd[0], buf + len, cap - len);
		if (n > 0) {
			len += (size_t)n;
			if (len == cap) {
				cap *= 2;
				buf = realloc(buf, cap);
			}
		} else if (n == 0) {
			break; /* EOF: child closed its output */
		} else if (errno != EINTR) {
			break;
		}
	}
	buf[len] = 0;
	close(pipefd[0]);

	int wstat = 0;
	waitpid(pid, &wstat, 0);
	if (killed) {
		char marker[96];
		int mn = snprintf(marker, sizeof marker,
						  "\n*** TIMEOUT: killed after %lds (MCC_TEST_TIMEOUT) ***\n",
						  timeout_s);
		if (mn > 0) {
			buf = realloc(buf, len + (size_t)mn + 1);
			memcpy(buf + len, marker, (size_t)mn + 1);
		}
		if (status)
			*status = -1;
	} else if (status) {
		*status = WIFEXITED(wstat) ? WEXITSTATUS(wstat)
								   : 128 + (WIFSIGNALED(wstat) ? WTERMSIG(wstat) : 0);
	}
	return buf;
}
#else
/* Windows keeps the popen path (the ctest --timeout 300 in ci.yml is the guard
 * there); killable process-group capture is POSIX-only. */
static char *run_capture(const char *cmd, int *status) {
	FILE *f = HC_POPEN_CMD(cmd);
	if (!f) {
		if (status)
			*status = -1;
		return xstrdup("");
	}
	char *out = slurp(f, NULL);
	int rc = pclose(f);
	if (status)
		*status = rc;
	return out;
}
#endif

static void strip_all(char *s, const char *needle) {
	size_t nl = strlen(needle);
	if (!nl)
		return;
	char *r = s, *w = s;
	while (*r) {
		if (!strncmp(r, needle, nl)) {
			r += nl;
			continue;
		}
		*w++ = *r++;
	}
	*w = 0;
}

static char *canon_line(const char *line, size_t len) {
	char *out = malloc(len + 1);
	size_t o = 0;
	int ws = 0, started = 0;
	for (size_t i = 0; i < len; i++) {
		char c = line[i];

		if (c == ' ' || c == '\t' || c == '\r') {
			ws = 1;
			continue;
		}
		if (ws && started)
			out[o++] = ' ';
		ws = 0;
		started = 1;
		out[o++] = c;
	}
	out[o] = 0;
	return out;
}

static int is_dig(char c) {
	return c >= '0' && c <= '9';
}

static char *mask_linenos(const char *s) {
	size_t n = strlen(s);
	char *out = malloc(n + 1);
	size_t o = 0, i = 0;
	while (s[i]) {
		char c = s[i];
		char prev = i >= 1 ? s[i - 1] : 0;
		if ((c == 'c' || c == 'h' || c == 'S' || c == 's') && prev == '.' &&
				s[i + 1] == ':' && is_dig(s[i + 2])) {
			out[o++] = c;
			out[o++] = ':';
			i += 2;
			while (is_dig(s[i]))
				i++;
			out[o++] = 'N';
			if (s[i] == ':' && is_dig(s[i + 1])) {
				out[o++] = ':';
				i++;
				while (is_dig(s[i]))
					i++;
				out[o++] = 'N';
			}
			continue;
		}
		out[o++] = c;
		i++;
	}
	out[o] = 0;
	return out;
}

static int texts_equal_raw(const char *a, const char *b) {
	const char *pa = a, *pb = b;
	for (;;) {

		const char *ea = strchr(pa, '\n');
		const char *eb = strchr(pb, '\n');
		size_t la = ea ? (size_t)(ea - pa) : strlen(pa);
		size_t lb = eb ? (size_t)(eb - pb) : strlen(pb);
		int a_end = (la == 0 && !ea && *pa == 0);
		int b_end = (lb == 0 && !eb && *pb == 0);

		char *ca = canon_line(pa, la);
		char *cb = canon_line(pb, lb);
		int eq = !strcmp(ca, cb) || (has_dot_run(ca) && glob_eq(ca, cb));
		free(ca);
		free(cb);
		if (!eq)
			return 0;
		if (!ea && !eb)
			return 1;
		pa = ea ? ea + 1 : pa + la;
		pb = eb ? eb + 1 : pb + lb;

		if (!ea && *pa == 0) {
			while (eb) {
				const char *n = strchr(pb, '\n');
				size_t l = n ? (size_t)(n - pb) : strlen(pb);
				char *c = canon_line(pb, l);
				int blank = (*c == 0);
				free(c);
				if (!blank)
					return 0;
				if (!n)
					break;
				pb = n + 1;
				if (*pb == 0)
					break;
			}
			return 1;
		}
		if (!eb && *pb == 0) {
			while (ea) {
				const char *n = strchr(pa, '\n');
				size_t l = n ? (size_t)(n - pa) : strlen(pa);
				char *c = canon_line(pa, l);
				int blank = (*c == 0);
				free(c);
				if (!blank)
					return 0;
				if (!n)
					break;
				pa = n + 1;
				if (*pa == 0)
					break;
			}
			return 1;
		}
		(void)a_end;
		(void)b_end;
	}
}

static int texts_equal(const char *a, const char *b) {
	char *ma = mask_linenos(a), *mb = mask_linenos(b);
	int r = texts_equal_raw(ma, mb);
	free(ma);
	free(mb);
	return r;
}

int main(int argc, char **argv) {
	if (argc < 6) {
		fprintf(stderr, "usage: %s <mcc> <bdir> <idir> <testroot> <workdir>\n", argv[0]);
		return 2;
	}
	const char *mcc = argv[1], *bdir = argv[2], *idir = argv[3];
	const char *root = argv[4], *work = argv[5];

	const char *emu = getenv("MCC_TEST_EMU");
	const char *optf = hc_envv("MCC_TEST_OPT", "-O0");
	if (!emu)
		emu = "";
	const char *runemu = getenv("MCC_TEST_RUNEMU");
	const char *xsysroot = getenv("MCC_TEST_SYSROOT");
	int cross = runemu && runemu[0];
	if (!runemu)
		runemu = "";
	if (!xsysroot)
		xsysroot = "";

	const char *only = NULL;
	int list_mode = 0;
	char *skipbuf[512];
	int nskip = 0;
	for (int i = 6; i < argc; i++) {
		if (!strcmp(argv[i], "--list"))
			list_mode = 1;
		else if (!strcmp(argv[i], "--only") && i + 1 < argc)
			only = argv[++i];
		else if (nskip < 512)
			skipbuf[nskip++] = argv[i];
	}
	char **skip = skipbuf;
	if (list_mode) {
		for (int i = 0; i < mcc_goldens_count; i++)
			printf("%s\n", mcc_goldens[i].name);
		return 0;
	}
	int pass = 0, fail = 0, skipped = 0;

	char cmd[32768], path[4096], srcdir[4096];

	if (HC_MKDIR(work) != 0 && errno != EEXIST) {
		fprintf(stderr, "cannot create workdir %s\n", work);
		return 2;
	}

	for (int i = 0; i < mcc_goldens_count; i++) {
		const mcc_golden_t *g = &mcc_goldens[i];
		if (only && strcmp(only, g->name))
			continue;
		int do_skip = 0;
		for (int s = 0; s < nskip; s++)
			if (!strcmp(skip[s], g->name)) {
				do_skip = 1;
				break;
			}
		if (do_skip) {
			printf("SKIP  %-32s -- excluded on command line\n", g->name);
			skipped++;
			continue;
		}

		char reason[256];
		if (!req_met(g->req, reason, sizeof reason)) {
			printf("SKIP  %-32s -- %s\n", g->name, reason);
			skipped++;
			continue;
		}

		if (cross && strcmp(g->mode, "run")) {
			printf("SKIP  %-32s -- cross run: only compile-and-execute goldens\n",
						 g->name);
			skipped++;
			continue;
		}

		if ((strstr(g->flags, "-b") || !strcmp(g->mode, "brun") || !strcmp(g->mode, "run2")) && strcmp(hc_envv("MCC_TEST_BCHECK", "0"), "1")) {
			printf("SKIP  %-32s -- requires bounds checker (MCC_CONFIG_BCHECK)\n",
						 g->name);
			skipped++;
			continue;
		}
		snprintf(path, sizeof path, "%s/%s", root, g->src);

		strcpy(srcdir, path);
		char *slash = strrchr(srcdir, '/');
		if (slash)
			*slash = 0;
		char *out;
		int rc;

		char xargs[8192];
		{
			const char *a = g->args;
			char *w = xargs;
			while (*a) {
				if (!strncmp(a, "{SELF}", 6)) {
					w += sprintf(w, "\"%s\"", path);
					a += 6;
				} else {
					char ch = *a++;
#ifdef _WIN32

					if (ch == '\'')
						ch = '"';
#endif
					*w++ = ch;
				}
			}
			*w = 0;
		}

		char sup[4096];
		snprintf(sup, sizeof sup, "%s/support", root);

		if (!strcmp(g->mode, "pp")) {
			snprintf(cmd, sizeof cmd,
							 "%s \"%s\" \"-B%s\" -fno-diagnostics-show-caret %s \"-I%s\" \"-I%s\" -E -P \"%s\" 2>&1",
							 emu, mcc, bdir, optf, idir, sup, path);
			out = run_capture(cmd, &rc);
		} else if (!strcmp(g->mode, "brun")) {

			snprintf(cmd, sizeof cmd,
							 "cd \"%s\" && %s \"%s\" \"-B%s\" -fno-diagnostics-show-caret %s \"-I%s\" \"-I%s\" -b -run \"%s\" %s 2>&1",
							 work, emu, mcc, bdir, optf, idir, sup, path, g->flags);
			out = run_capture(cmd, &rc);
		} else if (!strcmp(g->mode, "dt")) {

			snprintf(cmd, sizeof cmd,
							 "cd \"%s\" && %s \"%s\" \"-B%s\" -fno-diagnostics-show-caret %s \"-I%s\" \"-I%s\" -dt -run \"%s\" %s 2>&1",
							 work, emu, mcc, bdir, optf, idir, sup, path, g->flags);
			out = run_capture(cmd, &rc);
		} else if (!strcmp(g->mode, "run2")) {

			snprintf(cmd, sizeof cmd,
							 "cd \"%s\" && ( %s \"%s\" \"-B%s\" -fno-diagnostics-show-caret %s \"-I%s\" \"-I%s\" -run \"%s\" && "
							 "%s \"%s\" \"-B%s\" -fno-diagnostics-show-caret %s \"-I%s\" \"-I%s\" -b -run \"%s\" ) 2>&1",
							 work, emu, mcc, bdir, optf, idir, sup, path, emu, mcc, bdir, optf, idir, sup, path);
			out = run_capture(cmd, &rc);
		} else {
			char exe[4096];
			snprintf(exe, sizeof exe, "%s/t2_%s.exe", work, g->name);
			char xflags[6144] = "";
			if (cross)
				snprintf(xflags, sizeof xflags,
								 "\"--sysroot=%s\" \"-isystem\" \"%s/usr/include\" "
								 "\"-L%s/usr/lib64\" \"-L%s/lib64\" \"-L%s/usr/lib\" \"-L%s/lib\" ",
								 xsysroot, xsysroot, xsysroot, xsysroot, xsysroot, xsysroot);
			snprintf(cmd, sizeof cmd,
							 "%s \"%s\" \"-B%s\" -fno-diagnostics-show-caret %s \"-I%s\" \"-I%s\" %s\"%s\" %s -o \"%s\" 2>&1",
							 cross ? "" : emu, mcc, bdir, optf, idir, sup, xflags, path, g->flags, exe);
			char *cerr = run_capture(cmd, &rc);
			if (rc != 0) {
				printf("FAIL  %-32s (compile)\n%s", g->name, cerr);
				free(cerr);
				fail++;
				continue;
			}

			snprintf(cmd, sizeof cmd, "cd \"%s\" && %s \"%s\" %s", work,
							 cross ? runemu : emu, exe, xargs);
			char *prog = run_capture(cmd, &rc);

			out = malloc(strlen(cerr) + strlen(prog) + 1);
			strcpy(out, cerr);
			strcat(out, prog);
			free(cerr);
			free(prog);
		}

		char prefix[8192];
		snprintf(prefix, sizeof prefix, "%s/", srcdir);
		strip_all(out, prefix);

		const char *expect = g->expect;
		if (g->expect_win32 && !strcmp(hc_envv("MCC_TEST_OS", "unknown"), "WIN32"))
			expect = g->expect_win32;
		if (texts_equal(expect, out)) {
			pass++;
		} else {
			fail++;
			printf("FAIL  %-32s (mismatch)\n", g->name);
			printf("  --- expected ---\n%s\n  --- got ---\n%s\n", expect, out);
		}
		free(out);
	}
	printf("exec runner: %d passed, %d failed, %d skipped (of %d)\n",
				 pass, fail, skipped, mcc_goldens_count);
	if (only && pass + fail + skipped == 0) {
		fprintf(stderr, "exec runner: no golden named '%s'\n", only);
		return 2;
	}
	if (fail)
		return 1;
	if (pass == 0 && skipped > 0)
		return MCC_SKIP_RC;
	return 0;
}
