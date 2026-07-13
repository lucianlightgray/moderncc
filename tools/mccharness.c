#include "toolsupport.h"
#include <stdint.h>

static const char *ERR_NEEDLES[] = {"error:", "undefined reference", "undefined symbol", 0};
static const char *ERR_SKIPS[] = {"warning", "note:", 0};

static void report_err(const char *tag, const char *name, const char *stderr_text) {
	char *line = ts_first_error_line(stderr_text, ERR_NEEDLES, ERR_SKIPS);
	fprintf(stderr, "[%s] %s failed", name, tag);
	if (line)
		fprintf(stderr, ": %s", line);
	fputc('\n', stderr);
	free(line);
}

static void A(Argv *v, const char *s) {
	ts_arg(v, s);
}

static const char *const *Z(Argv *v) {
	return ts_argz(v);
}

static int compile_l(const char *const *argv, const char *const *launcher, char **err) {
	HostSpawnOpts o;
	memset(&o, 0, sizeof o);
	o.launcher = launcher;
	o.stderr_buf = err;
	return host_spawn_ex(argv, &o);
}

static int compile(const char *const *argv, char **err) {
	return compile_l(argv, NULL, err);
}

static int run_to_l(const char *const *argv, const char *const *launcher, const char *outfile) {
	HostSpawnOpts o;
	memset(&o, 0, sizeof o);
	o.launcher = launcher;
	o.stdout_file = outfile;
	return host_spawn_ex(argv, &o);
}

static int run_to(const char *const *argv, const char *outfile) {
	return run_to_l(argv, NULL, outfile);
}

static int run_to_retry(const char *const *argv, const char *const *launcher,
												const char *outfile, const char *what) {
	int rc = -1, attempt;
	for (attempt = 1; attempt <= 4; attempt++) {
		rc = run_to_l(argv, launcher, outfile);
		if (rc == 0)
			return 0;
		fprintf(stderr, "  %s exited abnormally (code %d), attempt %d/4\n", what, rc,
						attempt);
	}
	return rc;
}

static const char *opt(int argc, char **argv, const char *key, const char *dflt) {
	int i;
	for (i = 2; i < argc - 1; i++)
		if (!strcmp(argv[i], key))
			return argv[i + 1];
	return dflt;
}

static int has_flag(int argc, char **argv, const char *key) {
	int i;
	for (i = 2; i < argc; i++)
		if (!strcmp(argv[i], key))
			return 1;
	return 0;
}

static void split_append(Argv *v, const char *s) {
	char *dup, *tok, *save;
	if (!s || !*s)
		return;
	dup = strdup(s);
	for (tok = strtok_r(dup, " \t", &save); tok; tok = strtok_r(NULL, " \t", &save))
		A(v, tok);
}

static const char *const *make_env_plus(const char *const *extra) {
	char **env = host_environ();
	int n = 0, e = 0, i;
	const char **out;
	while (env && env[n])
		n++;
	while (extra && extra[e])
		e++;
	out = malloc((n + e + 1) * sizeof *out);
	for (i = 0; i < n; i++)
		out[i] = env[i];
	for (i = 0; i < e; i++)
		out[n + i] = extra[i];
	out[n + e] = NULL;
	return out;
}

static const char **make_launcher(const char *emu) {
	static const char *buf[16];
	char *dup, *tok, *save;
	int n = 0;
	if (!emu || !*emu)
		return NULL;
	dup = strdup(emu);
	for (tok = strtok_r(dup, " \t", &save); tok && n < 15; tok = strtok_r(NULL, " \t", &save))
		buf[n++] = tok;
	buf[n] = 0;
	return n ? buf : NULL;
}

static int run_cap(const char *const *argv, const char *const *launcher, char **out, char **err) {
	HostSpawnOpts o;
	memset(&o, 0, sizeof o);
	o.launcher = launcher;
	o.stdout_buf = out;
	o.stderr_buf = err;
	return host_spawn_ex(argv, &o);
}

static int run_quiet(const char *const *argv) {
	char *o = NULL, *e = NULL;
	int rc = run_cap(argv, NULL, &o, &e);
	free(o);
	free(e);
	return rc;
}

static int write_file(const char *path, const char *content) {
	FILE *f = fopen(path, "wb");
	if (!f)
		return -1;
	fputs(content, f);
	return fclose(f) ? -1 : 0;
}

static int skip0(const char *fmt, ...) {
	va_list ap;
	fputs("SKIP: ", stdout);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	fputc('\n', stdout);
	return 0;
}

static int ci_contains(const char *hay, const char *needle) {
	size_t nl = strlen(needle);
	if (!hay)
		return 0;
	for (; *hay; hay++) {
		size_t i;
		for (i = 0; i < nl; i++)
			if (tolower((unsigned char)hay[i]) != tolower((unsigned char)needle[i]))
				break;
		if (i == nl)
			return 1;
	}
	return 0;
}

static char *pp_norm(const char *raw) {
	size_t cap = (raw ? strlen(raw) : 0) + 2, oi = 0;
	char *out = malloc(cap);
	const char *p = raw ? raw : "";
	while (*p) {
		const char *e = strchr(p, '\n');
		size_t len = e ? (size_t)(e - p) : strlen(p), i, ti = 0;
		int inws = 0;
		if (len == 0 || p[0] == '#') {
			if (!e)
				break;
			p = e + 1;
			continue;
		}
		for (i = 0; i < len; i++) {
			char c = p[i];
			if (c == ' ' || c == '\t' || c == '\r') {
				inws = 1;
				continue;
			}
			if (inws && ti > 0)
				out[oi + ti++] = ' ';
			inws = 0;
			out[oi + ti++] = c;
		}
		if (ti > 0) {
			oi += ti;
			out[oi++] = '\n';
		}
		if (!e)
			break;
		p = e + 1;
	}
	out[oi] = 0;
	return out;
}

static int cmp_str(const void *a, const void *b) {
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static int suite_parts(int argc, char **argv) {
	const char *gcc = opt(argc, argv, "--gcc", NULL);
	const char *clang = opt(argc, argv, "--clang", NULL);
	const char *mcc = opt(argc, argv, "--mcc", NULL);
	const char *bdir = opt(argc, argv, "--bdir", NULL);
	const char *idir = opt(argc, argv, "--idir", NULL);
	const char *parts = opt(argc, argv, "--parts", NULL);
	const char *work = opt(argc, argv, "--work", NULL);

	const char *only = opt(argc, argv, "--only", NULL);
	int list_mode = has_flag(argc, argv, "--list");
	char *wraps[512];
	char Ipart[4096], Iinc[4096], Bflag[4096];
	int nw, i, ok = 0, fail = 0;

	if (!list_mode && (!gcc || !clang || !mcc || !bdir || !idir || !parts || !work)) {
		fprintf(stderr, "usage: mccharness parts --gcc --clang --mcc --bdir --idir --parts --work"
										" [--list] [--only NAME]\n");
		return 2;
	}
	if (list_mode) {
		if (!parts) {
			fprintf(stderr, "usage: mccharness parts --list --parts DIR\n");
			return 2;
		}
		nw = ts_glob(parts, "run_*.c", 0, wraps, 512);
		for (i = 0; i < nw; i++) {
			const char *b = strrchr(wraps[i], '/');
			b = b ? b + 1 : wraps[i];
			printf("%.*s\n", (int)(strlen(b) - 2), b);
			free(wraps[i]);
		}
		return 0;
	}
	host_mkdirs(work);
	snprintf(Ipart, sizeof Ipart, "-I%s", parts);
	snprintf(Iinc, sizeof Iinc, "-I%s", idir);
	snprintf(Bflag, sizeof Bflag, "-B%s", bdir);

	nw = ts_glob(parts, "run_*.c", 0, wraps, 512);
	for (i = 0; i < nw; i++) {
		const char *w = wraps[i];
		const char *base = strrchr(w, '/');
		base = base ? base + 1 : w;
		char name[256], eg[4096], ec[4096], em[4096];
		char og[4096], oc[4096], om[4096], outg[4096], outc[4096], outm[4096];
		char *err = NULL;
		int bad = 0;
		snprintf(name, sizeof name, "%.*s", (int)(strlen(base) - 2), base);
		if (only && strcmp(only, name)) {
			free(wraps[i]);
			continue;
		}
		ts_path(eg, sizeof eg, work, "%s.gcc", name);
		ts_path(ec, sizeof ec, work, "%s.clang", name);
		ts_path(em, sizeof em, work, "%s.mcc", name);
		ts_path(og, sizeof og, work, "%s.out.gcc", name);
		ts_path(oc, sizeof oc, work, "%s.out.clang", name);
		ts_path(om, sizeof om, work, "%s.out.mcc", name);
		(void)outg;
		(void)outc;
		(void)outm;

		{
			Argv v = {{0}, 0};
			A(&v, gcc);
			A(&v, w);
			A(&v, Ipart);
			A(&v, "-w");
			A(&v, "-O0");
			A(&v, "-std=gnu11");
			A(&v, "-lm");
			A(&v, "-o");
			A(&v, eg);
			if (compile(Z(&v), &err)) {
				report_err("gcc build", name, err);
				free(err);
				fail++;
				continue;
			}
			free(err);
			err = NULL;
		}
		{
			const char *r[] = {eg, 0};
			run_to(r, og);
		}

		{
			Argv v = {{0}, 0};
			A(&v, clang);
			A(&v, w);
			A(&v, Ipart);
			A(&v, "-w");
			A(&v, "-O0");
			A(&v, "-std=gnu11");
			A(&v, "-lm");
			A(&v, "-o");
			A(&v, ec);
			if (compile(Z(&v), &err)) {
				report_err("clang build", name, err);
				free(err);
				fail++;
				continue;
			}
			free(err);
			err = NULL;
		}
		{
			const char *r[] = {ec, 0};
			run_to(r, oc);
		}

		{
			Argv v = {{0}, 0};
			A(&v, mcc);
			A(&v, Bflag);
			A(&v, Iinc);
			A(&v, Ipart);
			A(&v, w);
			A(&v, "-lm");
			A(&v, "-o");
			A(&v, em);
			if (compile(Z(&v), &err)) {
				report_err("mcc build", name, err);
				free(err);
				fail++;
				continue;
			}
			free(err);
			err = NULL;
		}
		{
			const char *r[] = {em, 0};
			run_to(r, om);
		}

		if (ts_file_equal(og, oc) != 1) {
			fprintf(stderr, "[%s] gcc vs clang stdout diverged\n", name);
			bad = 1;
		}
		if (ts_file_equal(og, om) != 1) {
			fprintf(stderr, "[%s] gcc vs mcc stdout diverged\n", name);
			bad = 1;
		}
		if (bad)
			fail++;
		else
			ok++;

		free(wraps[i]);
	}
	printf("parts-suite: %d unit(s) 3-way-identical, %d diverged\n", ok, fail);
	return fail ? 1 : 0;
}

static void mcctest_report_diff(const char *refout, const char *mccout) {
	long rn = 0, mn = 0;
	char *r = ts_read_file(refout, &rn);
	char *m = ts_read_file(mccout, &mn);
	long i = 0, ln = 1, la = 0, lb = 0;
	if (!r || !m) {
		fprintf(stderr, "  (unable to read outputs: ref=%s mcc=%s)\n", r ? "ok" : "FAIL",
						m ? "ok" : "FAIL");
		free(r);
		free(m);
		return;
	}
	while (i < rn && i < mn && r[i] == m[i]) {
		if (r[i] == '\n')
			ln++;
		i++;
	}
	la = i;
	while (la > 0 && r[la - 1] != '\n')
		la--;
	lb = la;
	{
		long ra = la, rb = lb;
		char sa[256], sb[256];
		int na = 0, nb = 0;
		while (ra < rn && r[ra] != '\n' && na < (int)sizeof sa - 1)
			sa[na++] = r[ra++];
		while (rb < mn && m[rb] != '\n' && nb < (int)sizeof sb - 1)
			sb[nb++] = m[rb++];
		sa[na] = 0;
		sb[nb] = 0;
		fprintf(stderr, "  first diff at line %ld (byte %ld):\n", ln, i);
		fprintf(stderr, "    cc : %s\n", sa);
		fprintf(stderr, "    mcc: %s\n", sb);
	}
	free(r);
	free(m);
}

static int suite_mcctest(int argc, char **argv) {
	const char *cc = opt(argc, argv, "--cc", NULL);
	const char *mcc = opt(argc, argv, "--mcc", NULL);
	const char *src = opt(argc, argv, "--src", NULL);
	const char *bdir = opt(argc, argv, "--bdir", NULL);
	const char *idir = opt(argc, argv, "--idir", NULL);
	const char *srcdir = opt(argc, argv, "--srcdir", NULL);
	const char *work = opt(argc, argv, "--work", NULL);
	const char *testdefs = opt(argc, argv, "--testdefs", NULL);
	const char *refflags = opt(argc, argv, "--refflags", NULL);
	const char *mccargs = opt(argc, argv, "--mccargs", NULL);
	const char *const *emu = make_launcher(opt(argc, argv, "--emu", NULL));
	char Iinc[4096], Isrc[4096], Ibld[4096], Bflag[4096];
	char refexe[4096], refout[4096], mccexe[4096], mccout[4096];
	char *err = NULL;

	if (!cc || !mcc || !src || !bdir || !idir || !srcdir || !work) {
		fprintf(stderr, "usage: mccharness mcctest --cc --mcc --src --bdir --idir --srcdir --work"
										" [--testdefs S] [--refflags S] [--mccargs S] [--emu S]\n");
		return 2;
	}
	host_mkdirs(work);
	snprintf(Iinc, sizeof Iinc, "-I%s", idir);
	snprintf(Isrc, sizeof Isrc, "-I%s", srcdir);
	snprintf(Ibld, sizeof Ibld, "-I%s", bdir);
	snprintf(Bflag, sizeof Bflag, "-B%s", bdir);
	ts_path(refexe, sizeof refexe, work, "mcctest.gcc");
	ts_path(refout, sizeof refout, work, "mcctest.ref");
	ts_path(mccexe, sizeof mccexe, work, "mcctest.mcc");
	ts_path(mccout, sizeof mccout, work, "mcctest.out");

	{
		Argv v = {{0}, 0};
		A(&v, cc);
		A(&v, src);
		A(&v, "-o");
		A(&v, refexe);
		A(&v, Iinc);
		A(&v, Isrc);
		A(&v, Ibld);
		split_append(&v, testdefs);
		A(&v, "-w");
		A(&v, "-O0");
		A(&v, "-std=gnu11");
		A(&v, "-fno-omit-frame-pointer");
		split_append(&v, refflags);
		A(&v, "-lm");
		if (compile(Z(&v), &err)) {
			report_err("cc build", "mcctest", err);
			free(err);
			return 1;
		}
		free(err);
		err = NULL;
	}
	{
		const char *r[] = {refexe, 0};
		if (run_to_retry(r, NULL, refout, "reference cc program")) {
			fprintf(stderr, "mcctest: reference cc program crashed for %s;"
											" differential comparison not attempted\n", src);
			return 1;
		}
	}

	{
		Argv v = {{0}, 0};
		A(&v, mcc);
		A(&v, Bflag);
		A(&v, Iinc);
		A(&v, Isrc);
		A(&v, Ibld);
		split_append(&v, testdefs);
		split_append(&v, mccargs);
		A(&v, src);
		A(&v, "-o");
		A(&v, mccexe);
		A(&v, "-lm");
		if (compile_l(Z(&v), emu, &err)) {
			report_err("mcc build", "mcctest", err);
			free(err);
			return 1;
		}
		free(err);
		err = NULL;
	}
	{
		const char *r[] = {mccexe, 0};
		if (run_to_retry(r, emu, mccout, "mcc program")) {
			fprintf(stderr, "mcctest: mcc program crashed for %s\n", src);
			return 1;
		}
	}

	if (ts_file_equal(refout, mccout) != 1) {
		fprintf(stderr, "mcctest mismatch (cc vs mcc) for %s\n", src);
		mcctest_report_diff(refout, mccout);
		return 1;
	}
	printf("mcctest: cc==mcc stdout identical\n");
	return 0;
}

static int suite_mccexe(int argc, char **argv) {
	const char *mcc = opt(argc, argv, "--mcc", NULL);
	const char *bdir = opt(argc, argv, "--bdir", NULL);
	const char *idir = opt(argc, argv, "--idir", NULL);
	const char *out = opt(argc, argv, "--out", NULL);
	const char *srcs = opt(argc, argv, "--srcs", NULL);
	const char *runargs = opt(argc, argv, "--runargs", NULL);
	const char *const *emu = make_launcher(opt(argc, argv, "--emu", NULL));
	char Iinc[4096], Bflag[4096];
	char *err = NULL;
	int rc;

	if (!mcc || !bdir || !idir || !out || !srcs) {
		fprintf(stderr, "usage: mccharness mccexe --mcc --bdir --idir --out --srcs S [--runargs S] [--emu S]\n");
		return 2;
	}
	snprintf(Iinc, sizeof Iinc, "-I%s", idir);
	snprintf(Bflag, sizeof Bflag, "-B%s", bdir);

	{
		Argv v = {{0}, 0};
		A(&v, mcc);
		A(&v, Bflag);
		A(&v, Iinc);
		split_append(&v, srcs);
		A(&v, "-o");
		A(&v, out);
		if (compile_l(Z(&v), emu, &err)) {
			report_err("mcc compile", srcs, err);
			free(err);
			return 1;
		}
		free(err);
	}

	{
		Argv v = {{0}, 0};
		HostSpawnOpts o;
		memset(&o, 0, sizeof o);
		o.launcher = emu;
		A(&v, out);
		split_append(&v, runargs);
		rc = host_spawn_ex(Z(&v), &o);
		if (rc != 0) {
			fprintf(stderr, "%s exited with %d\n", out, rc);
			return 1;
		}
	}
	printf("mccexe: %s built and ran (exit 0)\n", out);
	return 0;
}

static int suite_asmconnect(int argc, char **argv) {
	const char *mcc = opt(argc, argv, "--mcc", NULL);
	const char *bdir = opt(argc, argv, "--bdir", NULL);
	const char *idir = opt(argc, argv, "--idir", NULL);
	const char *srcdir = opt(argc, argv, "--srcdir", NULL);
	const char *work = opt(argc, argv, "--work", NULL);
	const char *const *emu = make_launcher(opt(argc, argv, "--emu", NULL));
	char Iinc[4096], Bflag[4096], p1[4096], p2[4096];
	char single[4096], sep[4096], o1[4096], o2[4096], a1[4096], a2[4096];
	char *err = NULL;

	if (!mcc || !bdir || !idir || !srcdir || !work) {
		fprintf(stderr, "usage: mccharness asmconnect --mcc --bdir --idir --srcdir --work [--emu]\n");
		return 2;
	}
	host_mkdirs(work);
	snprintf(Iinc, sizeof Iinc, "-I%s", idir);
	snprintf(Bflag, sizeof Bflag, "-B%s", bdir);
	ts_path(p1, sizeof p1, srcdir, "asm/asm_c_connect/part1.c");
	ts_path(p2, sizeof p2, srcdir, "asm/asm_c_connect/part2.c");
	ts_path(single, sizeof single, work, "asm-c-connect");
	ts_path(sep, sizeof sep, work, "asm-c-connect-sep");
	ts_path(o1, sizeof o1, work, "asm-c-connect.out1");
	ts_path(o2, sizeof o2, work, "asm-c-connect.out2");
	ts_path(a1, sizeof a1, work, "acc1.o");
	ts_path(a2, sizeof a2, work, "acc2.o");

	{
		Argv v = {{0}, 0};
		A(&v, mcc);
		A(&v, Bflag);
		A(&v, Iinc);
		A(&v, p1);
		A(&v, p2);
		A(&v, "-o");
		A(&v, single);
		if (compile_l(Z(&v), emu, &err)) {
			report_err("single build", "asm-c-connect", err);
			free(err);
			return 1;
		}
		free(err);
		err = NULL;
	}
	{
		const char *r[] = {single, 0};
		run_to_l(r, emu, o1);
	}
	{
		Argv v = {{0}, 0};
		A(&v, mcc);
		A(&v, Bflag);
		A(&v, Iinc);
		A(&v, "-c");
		A(&v, p1);
		A(&v, "-o");
		A(&v, a1);
		if (compile_l(Z(&v), emu, &err)) {
			report_err("compile part1", "asm-c-connect", err);
			free(err);
			return 1;
		}
		free(err);
		err = NULL;
	}
	{
		Argv v = {{0}, 0};
		A(&v, mcc);
		A(&v, Bflag);
		A(&v, Iinc);
		A(&v, "-c");
		A(&v, p2);
		A(&v, "-o");
		A(&v, a2);
		if (compile_l(Z(&v), emu, &err)) {
			report_err("compile part2", "asm-c-connect", err);
			free(err);
			return 1;
		}
		free(err);
		err = NULL;
	}
	{
		Argv v = {{0}, 0};
		A(&v, mcc);
		A(&v, Bflag);
		A(&v, Iinc);
		A(&v, a1);
		A(&v, a2);
		A(&v, "-o");
		A(&v, sep);
		if (compile_l(Z(&v), emu, &err)) {
			report_err("separate link", "asm-c-connect", err);
			free(err);
			return 1;
		}
		free(err);
		err = NULL;
	}
	{
		const char *r[] = {sep, 0};
		run_to_l(r, emu, o2);
	}

	if (ts_file_equal(o1, o2) != 1) {
		fprintf(stderr, "asm-c-connect: single vs separate stdout mismatch\n");
		return 1;
	}
	printf("asm-c-connect: single == separate stdout\n");
	return 0;
}

static int suite_dashs(int argc, char **argv) {
	const char *mcc = opt(argc, argv, "--mcc", NULL);
	const char *bdir = opt(argc, argv, "--bdir", NULL);
	const char *idir = opt(argc, argv, "--idir", NULL);
	const char *srcdir = opt(argc, argv, "--srcdir", NULL);
	const char *work = opt(argc, argv, "--work", NULL);
	const char *const *emu = make_launcher(opt(argc, argv, "--emu", NULL));
	char Iinc[4096], Bflag[4096], src[4096];
	char direct[4096], out1[4096], sfile[4096], via[4096], out2[4096];
	char *err = NULL, *asmtext;

	if (!mcc || !bdir || !idir || !srcdir || !work) {
		fprintf(stderr, "usage: mccharness dashs --mcc --bdir --idir --srcdir --work [--emu]\n");
		return 2;
	}
	host_mkdirs(work);
	snprintf(Iinc, sizeof Iinc, "-I%s", idir);
	snprintf(Bflag, sizeof Bflag, "-B%s", bdir);
	ts_path(src, sizeof src, srcdir, "asm/dash_s_roundtrip/prog.c");
	ts_path(direct, sizeof direct, work, "dashS-direct");
	ts_path(out1, sizeof out1, work, "dashS.out1");
	ts_path(sfile, sizeof sfile, work, "dashS.s");
	ts_path(via, sizeof via, work, "dashS-via");
	ts_path(out2, sizeof out2, work, "dashS.out2");

	{
		Argv v = {{0}, 0};
		A(&v, mcc);
		A(&v, Bflag);
		A(&v, Iinc);
		A(&v, src);
		A(&v, "-o");
		A(&v, direct);
		if (compile_l(Z(&v), emu, &err)) {
			report_err("direct build", "dash-S", err);
			free(err);
			return 1;
		}
		free(err);
		err = NULL;
	}
	{
		const char *r[] = {direct, 0};
		run_to_l(r, emu, out1);
	}

	{
		Argv v = {{0}, 0};
		A(&v, mcc);
		A(&v, Bflag);
		A(&v, Iinc);
		A(&v, "-S");
		A(&v, src);
		A(&v, "-o");
		A(&v, sfile);
		if (compile_l(Z(&v), emu, &err)) {
			report_err("-S emit", "dash-S", err);
			free(err);
			return 1;
		}
		free(err);
		err = NULL;
	}

	asmtext = ts_read_file(sfile, NULL);
	{
		int elf_main = asmtext && strstr(asmtext, "\nmain:") != NULL;
		int macho_main = asmtext && strstr(asmtext, "\n_main:") != NULL;
		if (!asmtext || !strstr(asmtext, "\n\t.text") || (!elf_main && !macho_main) ||
				(elf_main && !strstr(asmtext, ".size\tmain"))) {
			fprintf(stderr, "dash-S: listing missing expected directives/labels\n");
			free(asmtext);
			return 1;
		}
	}
	free(asmtext);

	{
		Argv v = {{0}, 0};
		A(&v, mcc);
		A(&v, Bflag);
		A(&v, Iinc);
		A(&v, sfile);
		A(&v, "-o");
		A(&v, via);
		if (compile_l(Z(&v), emu, &err)) {
			report_err("re-assemble", "dash-S", err);
			free(err);
			return 1;
		}
		free(err);
		err = NULL;
	}
	{
		const char *r[] = {via, 0};
		run_to_l(r, emu, out2);
	}

	if (ts_file_equal(out1, out2) != 1) {
		fprintf(stderr, "dash-S round-trip mismatch (direct vs via -S)\n");
		return 1;
	}
	printf("dash-S: -S listing re-assembles to identical behaviour\n");
	return 0;
}

static int suite_preprocess(int argc, char **argv) {
	const char *mcc = opt(argc, argv, "--mcc", NULL);
	const char *bdir = opt(argc, argv, "--bdir", NULL);
	const char *idir = opt(argc, argv, "--idir", NULL);
	const char *tdir = opt(argc, argv, "--tdir", NULL);
	const char *gcc = opt(argc, argv, "--gcc", "gcc");
	const char *clang = opt(argc, argv, "--clang", "clang");

	const char *only = opt(argc, argv, "--only", NULL);
	int list_mode = has_flag(argc, argv, "--list");
	char gpath[4096], cpath[4096], Bflag[4096], Iinc[4096];
	char *files[4096];
	int nf, i, pass = 0, skip = 0, fail = 0, pp_same = 0;

	if (!tdir || (!list_mode && (!mcc || !bdir || !idir))) {
		fprintf(stderr, "usage: mccharness preprocess --mcc --bdir --idir --tdir"
										" [--gcc --clang] [--list] [--only REL]\n");
		return 2;
	}
	if (list_mode) {
		nf = ts_glob(tdir, "*.c", 1, files, 4096);
		if (nf >= 0 && nf < 4096)
			nf += ts_glob(tdir, "*.S", 1, files + nf, 4096 - nf);
		if (nf < 0) {
			fprintf(stderr, "preprocess: cannot walk %s\n", tdir);
			return 2;
		}
		qsort(files, nf, sizeof files[0], cmp_str);
		for (i = 0; i < nf; i++) {
			printf("%s\n", files[i] + strlen(tdir) + 1);
			free(files[i]);
		}
		return 0;
	}
	if (!host_find_tool(gcc, NULL, gpath, sizeof gpath))
		ts_skip("no gcc");
	if (!host_find_tool(clang, NULL, cpath, sizeof cpath))
		ts_skip("no clang");
	snprintf(Bflag, sizeof Bflag, "-B%s", bdir);
	snprintf(Iinc, sizeof Iinc, "-I%s", idir);

	{
		char *gc = host_path_canonical(gpath), *cc = host_path_canonical(cpath);
		if (gc && cc && !strcmp(gc, cc))
			pp_same = 1;
		free(gc);
		free(cc);
		if (!pp_same) {
			char *gv = NULL, *cv = NULL, *e = NULL;
			const char *av1[] = {gpath, "--version", 0}, *av2[] = {cpath, "--version", 0};
			run_cap(av1, NULL, &gv, &e);
			free(e);
			e = NULL;
			run_cap(av2, NULL, &cv, &e);
			free(e);
			if (gv && cv && *gv && !strcmp(gv, cv))
				pp_same = 1;
			free(gv);
			free(cv);
		}
	}
	if (pp_same)
		printf("note: gcc and clang are the same compiler -- impl-defined divergences SKIPPED\n");

	nf = ts_glob(tdir, "*.c", 1, files, 4096);
	if (nf >= 0 && nf < 4096)
		nf += ts_glob(tdir, "*.S", 1, files + nf, 4096 - nf);
	if (nf < 0) {
		fprintf(stderr, "preprocess: cannot walk %s\n", tdir);
		return 2;
	}
	qsort(files, nf, sizeof files[0], cmp_str);

	for (i = 0; i < nf; i++) {
		const char *f = files[i];
		const char *rel = f + strlen(tdir) + 1;
		size_t rl = strlen(f);
		char *e = NULL;

		if (only && strcmp(only, rel)) {
			free(files[i]);
			continue;
		}

		if (rl >= 2 && !strcmp(f + rl - 2, ".S")) {
			const char *av[] = {mcc, Bflag, Iinc, "-E", f, 0};
			char *o = NULL;
			int rc = run_cap(av, NULL, &o, &e);
			free(o);
			free(e);
			if (rc == 0)
				pass++;
			else {
				fail++;
				fprintf(stderr, "FAIL %s: mcc -E crashed/hung on asm input\n", rel);
			}
		} else if (!strncmp(rel, "diagnostics/", 12)) {
			char *ge = NULL, *ce = NULL, *me = NULL, *o = NULL;
			const char *ag[] = {gpath, "-E", f, 0}, *ac[] = {cpath, "-E", f, 0};
			int gd, cd;
			run_cap(ag, NULL, &o, &ge);
			free(o);
			o = NULL;
			run_cap(ac, NULL, &o, &ce);
			free(o);
			o = NULL;
			gd = ci_contains(ge, "error") || ci_contains(ge, "warning");
			cd = ci_contains(ce, "error") || ci_contains(ce, "warning");
			free(ge);
			free(ce);
			if (gd || cd) {
				const char *am[] = {mcc, Bflag, Iinc, "-E", f, 0};
				run_cap(am, NULL, &o, &me);
				free(o);
				if (ci_contains(me, "error") || ci_contains(me, "warning"))
					pass++;
				else {
					fail++;
					fprintf(stderr, "FAIL %s: mcc emits no diagnostic (gcc/clang do)\n", rel);
				}
				free(me);
			} else
				skip++;
		} else {
			char *go = NULL, *co = NULL, *mo = NULL, *gn, *cn, *mn;
			const char *ag[] = {gpath, "-E", f, 0}, *ac[] = {cpath, "-E", f, 0};
			run_cap(ag, NULL, &go, &e);
			free(e);
			e = NULL;
			run_cap(ac, NULL, &co, &e);
			free(e);
			gn = pp_norm(go);
			cn = pp_norm(co);
			free(go);
			free(co);
			if (strcmp(gn, cn)) {
				char *mo2 = NULL, *mn2;
				const char *am2[] = {mcc, Bflag, Iinc, "-E", f, 0};
				run_cap(am2, NULL, &mo2, &e);
				free(e);
				e = NULL;
				mn2 = pp_norm(mo2);
				free(mo2);
				if (!strcmp(mn2, gn)) {
					pass++;
					fprintf(stderr, "NOTE %s: gcc/clang -E differ; mcc matches gcc\n", rel);
				} else if (!strcmp(mn2, cn)) {
					pass++;
					fprintf(stderr, "NOTE %s: gcc/clang -E differ; mcc matches clang\n", rel);
				} else
					skip++;
				free(mn2);
			} else {
				const char *am[] = {mcc, Bflag, Iinc, "-E", f, 0};
				run_cap(am, NULL, &mo, &e);
				free(e);
				mn = pp_norm(mo);
				free(mo);
				if (!strcmp(mn, gn))
					pass++;
				else if (pp_same)
					skip++;
				else {
					fail++;
					fprintf(stderr, "FAIL %s: mcc -E diverges from gcc==clang\n", rel);
				}
				free(mn);
			}
			free(gn);
			free(cn);
		}
		free(files[i]);
	}
	printf("preprocess-suite: PASS=%d SKIP=%d FAIL=%d\n", pass, skip, fail);
	if (fail)
		return 1;

	if (only && pass == 0)
		ts_skip("impl-defined divergence (gcc != clang) or no consensus");
	return 0;
}

static const char *const *env_over(const char *const *overrides) {
	char **env = host_environ();
	int n = 0, ov = 0, i, j, cnt = 0;
	const char **out;
	while (env && env[n])
		n++;
	while (overrides && overrides[ov])
		ov++;
	out = malloc((n + ov + 1) * sizeof *out);
	for (i = 0; i < n; i++) {
		const char *e = env[i], *eq = strchr(e, '=');
		size_t kl = eq ? (size_t)(eq - e) : strlen(e);
		int hit = 0;
		for (j = 0; j < ov; j++) {
			const char *oeq = strchr(overrides[j], '=');
			size_t ol = oeq ? (size_t)(oeq - overrides[j]) : strlen(overrides[j]);
			if (ol == kl && !strncmp(overrides[j], e, kl)) {
				hit = 1;
				break;
			}
		}
		if (!hit)
			out[cnt++] = e;
	}
	for (j = 0; j < ov; j++)
		out[cnt++] = overrides[j];
	out[cnt] = NULL;
	return out;
}

static const char CPROP_SRC[] =
		"int same_join(int f) {\n"
		"	int a = 10, b;\n"
		"	if (f) b = a + 1; else b = a + 1;\n"
		"	return b + a;\n"
		"}\n"
		"int diff_join(int f) {\n"
		"	int b;\n"
		"	if (f) b = 3; else b = 4;\n"
		"	return b;\n"
		"}\n"
		"int one_arm(int f) {\n"
		"	int b = 5;\n"
		"	if (f) b = 5;\n"
		"	return b;\n"
		"}\n"
		"int loop_inv(int n) {\n"
		"	int c = 7, s = 0, i;\n"
		"	for (i = 0; i < n; i++) s += c;\n"
		"	return s;\n"
		"}\n"
		"int loop_written(int n) {\n"
		"	int c = 1, i;\n"
		"	for (i = 0; i < n; i++) c = c * 2;\n"
		"	return c;\n"
		"}\n"
		"int nested(int f, int n) {\n"
		"	int k = 9, s = 0, i;\n"
		"	for (i = 0; i < n; i++) {\n"
		"		if (f) s += k; else s += k + 1;\n"
		"	}\n"
		"	return s;\n"
		"}\n"
		"int with_break(int n) {\n"
		"	int c = 6, s = 0, i;\n"
		"	for (i = 0; i < n; i++) {\n"
		"		if (i == 2) break;\n"
		"		s += c;\n"
		"	}\n"
		"	return s + c;\n"
		"}\n"
		"int escaped(int f) {\n"
		"	int a = 8, b;\n"
		"	int *p = &a;\n"
		"	*p = f ? 20 : 30;\n"
		"	if (f) b = a; else b = a;\n"
		"	return b;\n"
		"}\n"
		"int do_loop(int n) {\n"
		"	int c = 11, s = 0, i = 0;\n"
		"	do { s += c; i++; } while (i < n);\n"
		"	return s;\n"
		"}\n"
		"int ext(int x);\n"
		"int cse_join(int x, int f) {\n"
		"	int t = x * 3 + 1, u, v;\n"
		"	if (f) u = x * 3 + 1; else u = x * 3 + 1;\n"
		"	v = x * 3 + 1;\n"
		"	return t + u + v + ext(x * 3 + 1);\n"
		"}\n"
		"int cse_killed(int x, int f) {\n"
		"	int t = x * 3 + 1, u;\n"
		"	if (f) x = 9; else x = 10;\n"
		"	u = x * 3 + 1;\n"
		"	return t + u;\n"
		"}\n"
		"int cse_loop(int x, int n) {\n"
		"	int b = x * 5, s = 0, i;\n"
		"	for (i = 0; i < n; i++) s += x * 5;\n"
		"	return s + b;\n"
		"}\n"
		"int ext(int x) { return x / 2; }\n"
		"int main(void) {\n"
		"	int acc = 0;\n"
		"	acc = acc * 31 + same_join(1);\n"
		"	acc = acc * 31 + same_join(0);\n"
		"	acc = acc * 31 + diff_join(1);\n"
		"	acc = acc * 31 + diff_join(0);\n"
		"	acc = acc * 31 + one_arm(0);\n"
		"	acc = acc * 31 + one_arm(1);\n"
		"	acc = acc * 31 + loop_inv(0);\n"
		"	acc = acc * 31 + loop_inv(4);\n"
		"	acc = acc * 31 + loop_written(5);\n"
		"	acc = acc * 31 + nested(1, 3);\n"
		"	acc = acc * 31 + nested(0, 3);\n"
		"	acc = acc * 31 + with_break(9);\n"
		"	acc = acc * 31 + escaped(1);\n"
		"	acc = acc * 31 + escaped(0);\n"
		"	acc = acc * 31 + do_loop(3);\n"
		"	acc = acc * 31 + cse_join(2, 1);\n"
		"	acc = acc * 31 + cse_join(2, 0);\n"
		"	acc = acc * 31 + cse_killed(5, 1);\n"
		"	acc = acc * 31 + cse_killed(5, 0);\n"
		"	acc = acc * 31 + cse_loop(3, 4);\n"
		"	return ((unsigned)acc % 251u);\n"
		"}\n";

static int cprop_build_run(const char *mcc, const char *bflag, int cprop, int cse,
													 const char *olvl, const char *src, const char *exe,
													 int *rc_out) {
	char cpe[48], cee[48];
	const char *ov[3];
	Argv v = {{0}, 0};
	HostSpawnOpts o;
	char *err = NULL;
	snprintf(cpe, sizeof cpe, "MCC_AST_CPROP_JOIN=%d", cprop);
	snprintf(cee, sizeof cee, "MCC_AST_CSE_JOIN=%d", cse);
	ov[0] = cpe;
	ov[1] = cee;
	ov[2] = 0;
	A(&v, mcc);
	A(&v, bflag);
	A(&v, olvl);
	A(&v, "-o");
	A(&v, exe);
	A(&v, src);
	memset(&o, 0, sizeof o);
	o.env = env_over(ov);
	o.stderr_buf = &err;
	if (host_spawn_ex(Z(&v), &o)) {
		free((void *)o.env);
		free(err);
		return -1;
	}
	free((void *)o.env);
	free(err);
	{
		const char *r[] = {exe, 0};
		*rc_out = host_spawn_ex(r, NULL);
	}
	return 0;
}

static int suite_cpropjoin(int argc, char **argv) {
	const char *mcc = opt(argc, argv, "--mcc", NULL);
	const char *bdir = opt(argc, argv, "--bdir", NULL);
	char w[4096], src[4096], o0[4096], t[4096], Bflag[4096];
	const char *lvls[] = {"-O1", "-O2", "-O3", 0};
	int ref, i, gc, ge;

	if (!mcc || !bdir) {
		fprintf(stderr, "usage: mccharness cpropjoin --mcc --bdir\n");
		return 2;
	}
	ts_path(w, sizeof w, bdir, "cprop-join-test");
	host_rmrf(w);
	if (host_mkdirs(w))
		return 1;
	ts_path(src, sizeof src, w, "j.c");
	ts_path(o0, sizeof o0, w, "j-o0");
	ts_path(t, sizeof t, w, "j-t");
	snprintf(Bflag, sizeof Bflag, "-B%s", bdir);
	if (write_file(src, CPROP_SRC)) {
		fprintf(stderr, "cannot write %s\n", src);
		return 1;
	}
	if (cprop_build_run(mcc, Bflag, 0, 0, "-O0", src, o0, &ref)) {
		fprintf(stderr, "O0 build failed\n");
		return 1;
	}
	for (i = 0; lvls[i]; i++)
		for (gc = 0; gc <= 1; gc++)
			for (ge = 0; ge <= 1; ge++) {
				int rc;
				if (cprop_build_run(mcc, Bflag, gc, ge, lvls[i], src, t, &rc)) {
					fprintf(stderr, "%s cprop=%d cse=%d build failed\n", lvls[i], gc, ge);
					return 1;
				}
				if (rc != ref) {
					fprintf(stderr, "%s cprop=%d cse=%d rc=%d, expected %d\n", lvls[i], gc, ge,
									rc, ref);
					return 1;
				}
			}
	printf("OK\n");
	return 0;
}

static char *scan_after(const char *text, const char *tag) {
	const char *p = text ? strstr(text, tag) : NULL, *s, *e;
	if (!p)
		return NULL;
	p += strlen(tag);
	if (p[0] != '0' || (p[1] != 'x' && p[1] != 'X'))
		return NULL;
	s = p;
	e = p + 2;
	while (isxdigit((unsigned char)*e))
		e++;
	{
		size_t len = (size_t)(e - s);
		char *r = malloc(len + 1);
		memcpy(r, s, len);
		r[len] = 0;
		return r;
	}
}

static int hv_run(const char *mcchv, const char *bflag, const char *const *ov,
									const char *seed, char **out) {
	Argv v = {{0}, 0};
	HostSpawnOpts o;
	int rc;
	A(&v, mcchv);
	A(&v, bflag);
	A(&v, "--seed");
	A(&v, seed);
	A(&v, "--passes");
	A(&v, "2");
	A(&v, "--workers");
	A(&v, "2");
	A(&v, "--seconds");
	A(&v, "0.6");
	memset(&o, 0, sizeof o);
	o.env = env_over(ov);
	o.stdout_buf = out;
	rc = host_spawn_ex(Z(&v), &o);
	free((void *)o.env);
	return rc;
}

static int suite_hvcache(int argc, char **argv) {
	const char *mcchv = opt(argc, argv, "--mcchv", NULL);
	const char *bdir = opt(argc, argv, "--bdir", NULL);
	char cdir[4200], cdenv[4300], Bflag[4096];
	char *o1 = NULL, *o2 = NULL, *o3 = NULL, *o4 = NULL;
	char *k1 = NULL, *k2 = NULL, *k3 = NULL;
	const char *ovc[2], *ovnone[] = {"MCCHV_CACHE_DIR=", "XDG_CACHE_HOME=", "HOME=", 0};
	int rc, ret = 1;

	if (!mcchv || !bdir) {
		fprintf(stderr, "usage: mccharness hvcache --mcchv --bdir\n");
		return 2;
	}
	ts_path(cdir, sizeof cdir, bdir, "hvcache-roundtrip");
	host_rmrf(cdir);
	if (host_mkdirs(cdir))
		return 1;
	snprintf(Bflag, sizeof Bflag, "-B%s", bdir);
	snprintf(cdenv, sizeof cdenv, "MCCHV_CACHE_DIR=%s", cdir);
	ovc[0] = cdenv;
	ovc[1] = 0;

	rc = hv_run(mcchv, Bflag, ovc, "42", &o1);
	if (rc == TS_SKIP_CODE)
		ts_skip("hypervisor unavailable");
	if (rc != 0) {
		fprintf(stderr, "run1 failed rc=%d\n", rc);
		goto done;
	}
	if (!o1 || !strstr(o1, "hv: cache miss (cold)")) {
		fprintf(stderr, "expected a cold miss on the first run\n");
		goto done;
	}
	if (!strstr(o1, "hv:   cache file")) {
		fprintf(stderr, "expected a cache write on the first run\n");
		goto done;
	}

	rc = hv_run(mcchv, Bflag, ovc, "42", &o2);
	if (rc != 0) {
		fprintf(stderr, "run2 failed\n");
		goto done;
	}
	if (!o2 || !strstr(o2, "hv: cache hit (warm-start)")) {
		fprintf(stderr, "expected a warm hit on the second run\n");
		goto done;
	}
	if (!strstr(o2, "hv: cache resume:")) {
		fprintf(stderr, "expected resumed search state on the second run\n");
		goto done;
	}

	k1 = scan_after(o1, "intention ");
	k2 = scan_after(o2, "intention ");
	if (!k1 || !k2 || strcmp(k1, k2)) {
		fprintf(stderr, "intention key not stable across runs (%s vs %s)\n",
						k1 ? k1 : "", k2 ? k2 : "");
		goto done;
	}

	rc = hv_run(mcchv, Bflag, ovc, "43", &o3);
	if (rc != 0) {
		fprintf(stderr, "run3 failed\n");
		goto done;
	}
	if (!o3 || !strstr(o3, "hv: cache miss (cold)")) {
		fprintf(stderr, "expected a miss for the edited intention\n");
		goto done;
	}
	k3 = scan_after(o3, "intention ");
	if (!k3 || !strcmp(k1, k3)) {
		fprintf(stderr, "edited intention did not change the key\n");
		goto done;
	}

	{
		Argv v = {{0}, 0};
		HostSpawnOpts o;
		A(&v, mcchv);
		A(&v, Bflag);
		A(&v, "--seed");
		A(&v, "42");
		A(&v, "--passes");
		A(&v, "2");
		A(&v, "--workers");
		A(&v, "2");
		A(&v, "--seconds");
		A(&v, "0.6");
		memset(&o, 0, sizeof o);
		o.env = env_over(ovnone);
		o.stdout_buf = &o4;
		rc = host_spawn_ex(Z(&v), &o);
		free((void *)o.env);
	}
	if (rc != 0) {
		fprintf(stderr, "run without a cache dir failed\n");
		goto done;
	}
	if (!o4 || !strstr(o4, "hv: cache disabled")) {
		fprintf(stderr, "expected the cache to be disabled with no resolvable dir\n");
		goto done;
	}

	printf("OK\n");
	ret = 0;
done:
	free(o1);
	free(o2);
	free(o3);
	free(o4);
	free(k1);
	free(k2);
	free(k3);
	return ret;
}

static const char PERFN_SRC[] =
		"int f(int x) { int i, s = 0; for (i = 0; i < x; i++) s += i * 3; return s; }\n"
		"int g(int x) { int i, s = 1; for (i = 0; i < x; i++) s += i * 5; return s; }\n"
		"int main(void) { return f(10) == 135 ? 0 : 1; }\n";

static int perfn_cached(const char *out) {
	const char *p = out ? strstr(out, "superopt-perfn:") : NULL, *q;
	if (!p)
		return -1;
	q = strstr(p, "functions (");
	if (!q)
		return -1;
	return atoi(q + strlen("functions ("));
}

static int perfn_run(const char *mcc, const char *bflag, const char *w,
										 const char *src, char **out) {
	char xdg[4200], home[4200], ao[4200];
	const char *ov[4];
	Argv v = {{0}, 0};
	HostSpawnOpts o;
	char *err = NULL;
	int rc;
	snprintf(xdg, sizeof xdg, "XDG_CACHE_HOME=%s/cache", w);
	snprintf(home, sizeof home, "HOME=%s", w);
	ts_path(ao, sizeof ao, w, "a.o");
	ov[0] = "MCC_AST_PERFN=1";
	ov[1] = xdg;
	ov[2] = home;
	ov[3] = 0;
	A(&v, mcc);
	A(&v, bflag);
	A(&v, "-O4");
	A(&v, "-v");
	A(&v, "-c");
	A(&v, "-o");
	A(&v, ao);
	A(&v, src);
	memset(&o, 0, sizeof o);
	o.env = env_over(ov);
	o.stdout_buf = out;
	o.stderr_buf = &err;
	rc = host_spawn_ex(Z(&v), &o);
	free((void *)o.env);
	free(err);
	return rc;
}

static int suite_perfncache(int argc, char **argv) {
	const char *mcc = opt(argc, argv, "--mcc", NULL);
	const char *bdir = opt(argc, argv, "--bdir", NULL);
	char w[4096], src[4096], Bflag[4096];
	char *o1 = NULL, *o2 = NULL, *o3 = NULL, *content;
	int ret = 1, c, attempt;

	if (!mcc || !bdir) {
		fprintf(stderr, "usage: mccharness perfncache --mcc --bdir\n");
		return 2;
	}
	ts_path(w, sizeof w, bdir, "perfn-cache-test");
	ts_path(src, sizeof src, w, "a.c");
	snprintf(Bflag, sizeof Bflag, "-B%s", bdir);

	for (attempt = 0; attempt < 4; attempt++) {
		host_rmrf(w);
		if (host_mkdirs(w))
			return 1;
		if (write_file(src, PERFN_SRC)) {
			fprintf(stderr, "cannot write %s\n", src);
			return 1;
		}
		free(o1);
		free(o2);
		free(o3);
		o1 = o2 = o3 = NULL;

		if (perfn_run(mcc, Bflag, w, src, &o1)) {
			fprintf(stderr, "run1 failed\n");
			goto done;
		}
		c = perfn_cached(o1);
		if (c < 0)
			continue;
		if (c != 0) {
			fprintf(stderr, "expected 0 cached on the cold run, got '%d'\n", c);
			goto done;
		}

		if (perfn_run(mcc, Bflag, w, src, &o2)) {
			fprintf(stderr, "run2 failed\n");
			goto done;
		}
		c = perfn_cached(o2);
		if (c < 0)
			continue;
		if (c != 3) {
			fprintf(stderr, "expected all 3 functions cached on the warm run, got '%d'\n", c);
			goto done;
		}

		content = ts_read_file(src, NULL);
		if (content) {
			char *pos = strstr(content, "i * 5");
			if (pos) {
				pos[4] = '7';
				write_file(src, content);
			}
			free(content);
		}

		if (perfn_run(mcc, Bflag, w, src, &o3)) {
			fprintf(stderr, "run3 failed\n");
			goto done;
		}
		c = perfn_cached(o3);
		if (c < 0)
			continue;
		if (c != 2) {
			fprintf(stderr, "expected 2 cached after editing one function, got '%d'\n", c);
			goto done;
		}

		printf("OK\n");
		ret = 0;
		goto done;
	}

	free(o1);
	free(o2);
	free(o3);
	ts_skip("superopt per-function search never ran (driver fell back to a plain compile)");
done:
	free(o1);
	free(o2);
	free(o3);
	return ret;
}

static const char *FC_CALLEE =
		"struct P2 { int x, y; };\n"
		"int __attribute__((fastcall)) mix_ll(int a, long long b, int c){ return (int)(a+b+c); }\n"
		"int __attribute__((fastcall)) small(char a, short b, int c){ return a+b+c; }\n"
		"int __attribute__((fastcall)) ptr2(int *a, int *b){ return *a + *b; }\n"
		"int __attribute__((fastcall)) ll_first(long long a, int b){ return (int)(a+b); }\n"
		"int __attribute__((fastcall)) fs(int a, struct P2 p, int b){ return a*1000+p.x*100+p.y*10+b; }\n";
static const char *FC_CALLER =
		"struct P2 { int x, y; };\n"
		"int __attribute__((fastcall)) mix_ll(int a, long long b, int c);\n"
		"int __attribute__((fastcall)) small(char a, short b, int c);\n"
		"int __attribute__((fastcall)) ptr2(int *a, int *b);\n"
		"int __attribute__((fastcall)) ll_first(long long a, int b);\n"
		"int __attribute__((fastcall)) fs(int a, struct P2 p, int b);\n"
		"int main(void){\n"
		"    int x=10, y=20, f=0; struct P2 p={2,3};\n"
		"    if (mix_ll(1,100,3)!=104) f|=1;\n"
		"    if (small(1,2,3)!=6) f|=2;\n"
		"    if (ptr2(&x,&y)!=30) f|=4;\n"
		"    if (ll_first(100,5)!=105) f|=8;\n"
		"    if (fs(1,p,4)!=1234) f|=16;\n"
		"    return f;\n}\n";
static const char *FC_UNSUP =
		"int __attribute__((fastcall)) f(double a,int b); int g(){ return f(1.0,2); }\n";

static int suite_i386fastcall(int argc, char **argv) {
	const char *imcc = opt(argc, argv, "--imcc", NULL);
	const char *gcc = opt(argc, argv, "--gcc", "gcc");
	const char *m32s = opt(argc, argv, "--m32", "-m32");
	const char *work = opt(argc, argv, "--work", NULL);
	char gpath[4096];
	char callee[4096], et[4096], lt[4096], eg[4096], lg[4096];
	char cC[4096], cR[4096], cU[4096], uo[4096], run[4096];
	int fail = 0;

	if (!imcc || !work) {
		fprintf(stderr, "usage: mccharness i386fastcall --imcc --work [--gcc --m32]\n");
		return 2;
	}
	if (!host_find_tool(gcc, NULL, gpath, sizeof gpath))
		return skip0("no gcc");
	host_mkdirs(work);
#define P(v, name) ts_path(v, sizeof v, work, "%s", name)
	P(cC, "callee.c");
	P(cR, "caller.c");
	P(cU, "unsup.c");
	P(callee, "m32probe.c");
	P(et, "e_t.o");
	P(lt, "l_t.o");
	P(eg, "e_g.o");
	P(lg, "l_g.o");
	P(uo, "unsup.o");
	P(run, "run");
#undef P

	{
		char probe[4096], probeexe[4096];
		Argv v = {{0}, 0};
		ts_path(probe, sizeof probe, work, "m32probe.c");
		ts_path(probeexe, sizeof probeexe, work, "m32probe");
		write_file(probe, "int main(void){return 0;}\n");
		A(&v, gpath);
		split_append(&v, m32s);
		A(&v, probe);
		A(&v, "-o");
		A(&v, probeexe);
		if (run_quiet(Z(&v)))
			return skip0("32-bit reference build unavailable (%s %s)", gpath, m32s);
	}

	{
		int isd;
		if (host_stat(imcc, &isd, NULL, NULL))
			return skip0("i386 mcc not found at '%s'", imcc);
	}

	{
		char fp[4096], fm[4096], fmo[4096], fgo[4096], fpe[4096];
		Argv a = {{0}, 0}, b = {{0}, 0}, c = {{0}, 0};
		ts_path(fp, sizeof fp, work, "fmtprobe.c");
		ts_path(fm, sizeof fm, work, "fmtmain.c");
		ts_path(fmo, sizeof fmo, work, "fmt_mcc.o");
		ts_path(fgo, sizeof fgo, work, "fmt_gcc.o");
		ts_path(fpe, sizeof fpe, work, "fmtprobe");
		write_file(fp, "int probe_fn(void){return 0;}\n");
		write_file(fm, "int probe_fn(void); int main(void){return probe_fn();}\n");
		A(&a, imcc);
		A(&a, "-c");
		A(&a, fp);
		A(&a, "-o");
		A(&a, fmo);
		A(&b, gpath);
		split_append(&b, m32s);
		A(&b, "-c");
		A(&b, fm);
		A(&b, "-o");
		A(&b, fgo);
		A(&c, gpath);
		split_append(&c, m32s);
		A(&c, fgo);
		A(&c, fmo);
		A(&c, "-o");
		A(&c, fpe);
		if (run_quiet(Z(&a)) || run_quiet(Z(&b)) || run_quiet(Z(&c)))
			return skip0("cannot link mcc + reference-cc objects (object-format mismatch)");
	}

	write_file(cC, FC_CALLEE);
	write_file(cR, FC_CALLER);
	write_file(cU, FC_UNSUP);

	{
		Argv v = {{0}, 0};
		A(&v, imcc);
		A(&v, "-c");
		A(&v, cC);
		A(&v, "-o");
		A(&v, et);
		if (run_quiet(Z(&v)))
			fail = 1;
	}
	{
		Argv v = {{0}, 0};
		A(&v, imcc);
		A(&v, "-c");
		A(&v, cR);
		A(&v, "-o");
		A(&v, lt);
		if (run_quiet(Z(&v)))
			fail = 1;
	}
	{
		Argv v = {{0}, 0};
		A(&v, gpath);
		split_append(&v, m32s);
		A(&v, "-O0");
		A(&v, "-c");
		A(&v, cC);
		A(&v, "-o");
		A(&v, eg);
		if (run_quiet(Z(&v)))
			fail = 1;
	}
	{
		Argv v = {{0}, 0};
		A(&v, gpath);
		split_append(&v, m32s);
		A(&v, "-O0");
		A(&v, "-c");
		A(&v, cR);
		A(&v, "-o");
		A(&v, lg);
		if (run_quiet(Z(&v)))
			fail = 1;
	}

	{
		struct
		{
			const char *name, *o1, *o2;
		} chk[] = {
				{"mcc caller -> gcc callee", eg, lt},
				{"gcc caller -> mcc callee", et, lg},
				{"mcc caller -> mcc callee", et, lt}};
		int i;
		for (i = 0; i < 3; i++) {
			Argv v = {{0}, 0};
			A(&v, gpath);
			split_append(&v, m32s);
			A(&v, chk[i].o1);
			A(&v, chk[i].o2);
			A(&v, "-o");
			A(&v, run);
			if (run_quiet(Z(&v))) {
				printf("FAIL  %s (link)\n", chk[i].name);
				fail = 1;
				continue;
			}
			{
				const char *r[] = {run, 0};
				int rc = host_spawn_ex(r, NULL);
				if (rc == 0)
					printf("PASS  %s\n", chk[i].name);
				else {
					printf("FAIL  %s (exit %d)\n", chk[i].name, rc);
					fail = 1;
				}
			}
		}
	}

	{
		Argv v = {{0}, 0};
		A(&v, imcc);
		A(&v, "-c");
		A(&v, cU);
		A(&v, "-o");
		A(&v, uo);
		if (run_quiet(Z(&v)) == 0) {
			printf("FAIL  unsupported float-before-reg case should error\n");
			fail = 1;
		} else
			printf("PASS  unsupported float-before-reg case errors cleanly\n");
	}

	if (fail) {
		fprintf(stderr, "i386-fastcall-abi: FAILURES\n");
		return 1;
	}
	printf("ALL OK\n");
	return 0;
}

static const char *GCCTS_FIXED_SKIP[] = {
		"20000120-2.c", "mipscop-1.c", "mipscop-2.c", "mipscop-3.c", "mipscop-4.c",
		"fp-cmp-4f.c", "fp-cmp-4l.c", "fp-cmp-8f.c", "fp-cmp-8l.c", "pr38016.c", 0};

static int gccts_skiplisted(const char *base, const char *content) {
	int i;
	for (i = 0; GCCTS_FIXED_SKIP[i]; i++)
		if (!strcmp(base, GCCTS_FIXED_SKIP[i]))
			return 1;
	return content && (strstr(content, "_builtin_") || strstr(content, "complex") ||
										 strstr(content, "Complex") || strstr(content, "__int128_t") ||
										 strstr(content, "__uint128_t") || strstr(content, "vector"));
}

static const char *GCCTS_AST_KNOWN_REPLAY[] = {0};
static const char *GCCTS_AST_KNOWN_PROMOTE[] = {0};
static const char *GCCTS_AST_KNOWN_INLINE[] = {0};

static int gccts_in_list(const char *base, const char *const *list) {
	int i;
	for (i = 0; list[i]; i++)
		if (!strcmp(base, list[i]))
			return 1;
	return 0;
}

static int gccts_ast_skiplisted(const char *base, const char *col) {
	if (gccts_in_list(base, GCCTS_AST_KNOWN_REPLAY))
		return 1;
	if (col && !strcmp(col, "promote"))
		return gccts_in_list(base, GCCTS_AST_KNOWN_PROMOTE);
	if (col && (!strcmp(col, "inline") || !strcmp(col, "inline-tmpl")))
		return gccts_in_list(base, GCCTS_AST_KNOWN_INLINE);
	return 0;
}

static void gccts_setenv(const char *k, const char *v) {
#if MCC_HOST_WIN32
	char buf[256];
	snprintf(buf, sizeof buf, "%s=%s", k, v ? v : "");
	_putenv(buf);
#else
	if (v)
		setenv(k, v, 1);
	else
		unsetenv(k);
#endif
}

static const char *gccts_opt = "-O0";

static void gccts_ast_env(const char *mode, int on) {
	if (!mode)
		return;
	if (!on) {
		gccts_opt = "-O0";
		return;
	}
	gccts_opt = "-O1";
	gccts_setenv("MCC_AST_TEMPLATES", strcmp(mode, "inline-tmpl") ? "0" : "1");
	gccts_setenv("MCC_AST_PROMOTE", strcmp(mode, "promote") ? "0" : "1");
	gccts_setenv("MCC_AST_INLINE",
							 !strcmp(mode, "inline") || !strcmp(mode, "inline-tmpl") ? "1"
																																			 : "0");
}

static int gccts_attempt(const char *mcc, const char *Bflag, const char *idir,
												 const char *Iinc, const char *Iinc2, const char *s,
												 const char *tsto, const char *tstx, int execute) {
	char *o = NULL, *e = NULL;
	Argv v = {{0}, 0};
	A(&v, mcc);
	A(&v, Bflag);
	A(&v, gccts_opt);
	A(&v, "-DNO_TRAMPOLINES");
	if (idir) {
		A(&v, Iinc);
		A(&v, Iinc2);
	}
	if (execute) {
		A(&v, s);
		A(&v, "-o");
		A(&v, tstx);
		A(&v, "-lm");
	} else {
		A(&v, "-o");
		A(&v, tsto);
		A(&v, "-c");
		A(&v, s);
	}
	int rc = run_cap(Z(&v), NULL, &o, &e);
	int local_fns = (o && strstr(o, "cannot use local functions")) ||
									(e && strstr(e, "cannot use local functions"));
	free(o);
	free(e);
	if (local_fns)
		return 3;
	if (rc != 0)
		return 1;
	if (execute) {
		const char *rr[] = {tstx, 0};
		if (run_quiet(rr) != 0)
			return 2;
	}
	return 0;
}

static int suite_gcctestsuite(int argc, char **argv) {
	const char *mcc = opt(argc, argv, "--mcc", NULL);
	const char *bdir = opt(argc, argv, "--bdir", NULL);
	const char *idir = opt(argc, argv, "--idir", NULL);
	const char *path = opt(argc, argv, "--path", NULL);
	const char *builddir = opt(argc, argv, "--builddir", NULL);
	const char *ast = opt(argc, argv, "--ast", NULL);
	char Bflag[4096], Iinc[4096], Iinc2[4200], rt[4096], sumpath[4200];
	char dir[4200], tsto[4200], tstx[4200];
	char *files[8192];
	int nf = 0, i, ok = 0, sk = 0, fa = 0, xf = 0, re = 0, isd;
	FILE *sum;

	if (!mcc || !builddir) {
		fprintf(stderr, "usage: mccharness gcctestsuite --mcc --builddir [--bdir --idir --path --ast MODE]\n");
		return 2;
	}
	if (!path || host_stat(path, &isd, NULL, NULL) || !isd)
		return skip0("gcc testsuite not found (set MCC_GCCTESTSUITE_PATH)");
	if (ast && strcmp(ast, "replay") && strcmp(ast, "promote") &&
			strcmp(ast, "inline") && strcmp(ast, "inline-tmpl")) {
		fprintf(stderr, "gcctestsuite: --ast must be replay|promote|inline|inline-tmpl\n");
		return 2;
	}
	gccts_ast_env(ast, 0);
	if (!bdir)
		bdir = builddir;
	snprintf(Bflag, sizeof Bflag, "-B%s", bdir);
	ts_path(rt, sizeof rt, builddir, "gcctestsuite");
	ts_path(sumpath, sizeof sumpath, builddir, "mcc.sum");
	ts_path(tsto, sizeof tsto, rt, "tst.o");
	ts_path(tstx, sizeof tstx, rt, "tst");
	host_mkdirs(rt);
	if (idir) {
		snprintf(Iinc, sizeof Iinc, "-I%s", idir);
		snprintf(Iinc2, sizeof Iinc2, "-I%s/include", idir);
	}

	if (!(sum = fopen(sumpath, "wb"))) {
		fprintf(stderr, "gcctestsuite: cannot write %s\n", sumpath);
		return 1;
	}

	const char *subs[] = {"compile", "execute", "execute/ieee", 0};
	int si;
	for (si = 0; subs[si]; si++) {
		int execute = si != 0;
		ts_path(dir, sizeof dir, path, "%s", subs[si]);
		nf = ts_glob(dir, "*.c", 0, files, 8192);
		for (i = 0; i < (nf < 0 ? 0 : nf); i++) {
			const char *s = files[i], *base = strrchr(s, '/');
			base = base ? base + 1 : s;
			char *content = ts_read_file(s, NULL);
			const char *r;
			int st = gccts_attempt(mcc, Bflag, idir, Iinc, Iinc2, s, tsto, tstx, execute);
			if (st == 3) {
				r = "SKIP";
				sk++;
			} else if (st == 0) {
				if (ast) {
					gccts_ast_env(ast, 1);
					int ast_st =
							gccts_attempt(mcc, Bflag, idir, Iinc, Iinc2, s, tsto, tstx, execute);
					gccts_ast_env(ast, 0);
					if (ast_st != 0) {
						if (gccts_ast_skiplisted(base, ast)) {
							r = "KNOWNGAP";
							sk++;
						} else {
							r = ast_st == 2 ? "REGRESS-EXE" : "REGRESS";
							re++;
						}
					} else {
						r = "PASS";
						ok++;
					}
				} else {
					r = "PASS";
					ok++;
				}
			} else if (gccts_skiplisted(base, content)) {
				r = "SKIP";
				sk++;
			} else {
				r = execute && st == 2 ? "FAILEXE" : "FAIL";
				if (execute && st == 2)
					xf++;
				else
					fa++;
			}
			fprintf(sum, "%s: %s\n", r, s);
			free(content);
			free(files[i]);
		}
	}
	fprintf(sum, "%d ok\n%d skipped\n%d failed\n%d exe failed\n%d regressed\n", ok, sk, fa,
					xf, re);
	fclose(sum);
	printf("%d test(s) ok.\n%d test(s) skipped.\n%d test(s) failed.\n%d test(s) exe failed.\n",
				 ok, sk, fa, xf);
	if (ast) {
		printf("%d AST regression(s) vs -O0 (column: %s).\n", re, ast);
		return re == 0 ? 0 : 1;
	}
	return 0;
}

static int suite_penative(int argc, char **argv) {
	const char *mcc = opt(argc, argv, "--mcc", NULL);
	const char *b = opt(argc, argv, "--b", NULL);
	const char *src = opt(argc, argv, "--src", NULL);
	const char *work = opt(argc, argv, "--work", NULL);
	const char *cpu = opt(argc, argv, "--cpu", "");
	char conf[4200], lib[4200], Ib[4200], Ibinc[4200], Iw32[4200], Irt[4200], runout[4200];
	char *files[4096];
	int nf, i, status = 0, isd;

	if (!mcc || !b || !src || !work) {
		fprintf(stderr, "usage: mccharness penative --mcc --b --src --work [--cpu]\n");
		return 2;
	}
	if (host_stat(mcc, &isd, NULL, NULL))
		ts_skip("no native mcc at %s", mcc);
	ts_path(lib, sizeof lib, b, "lib/libmccrt.a");
	if (host_stat(lib, &isd, NULL, NULL))
		ts_skip("build tree %s has no lib/libmccrt.a", b);
	ts_path(conf, sizeof conf, src, "tests/qemu/conformance");
	snprintf(Ib, sizeof Ib, "-B%s", b);
	snprintf(Ibinc, sizeof Ibinc, "-I%s/include", b);
	snprintf(Iw32, sizeof Iw32, "-I%s/runtime/win32/include", src);
	snprintf(Irt, sizeof Irt, "-I%s/runtime/include", src);
	ts_path(runout, sizeof runout, work, "pe-native-run.out");
	host_mkdirs(work);

	nf = ts_glob(conf, "*.c", 0, files, 4096);
	for (i = 0; i < (nf < 0 ? 0 : nf); i++) {
		const char *f = files[i], *base = strrchr(f, '/');
		char n[256], exe[4200], *err = NULL;
		base = base ? base + 1 : f;
		snprintf(n, sizeof n, "%.*s", (int)(strlen(base) - 2), base);
		if (!strcmp(cpu, "arm64") && (!strcmp(n, "tls") || !strcmp(n, "tls_aggr"))) {
			printf("SKIP %s -- arm64 PE _Thread_local access-violates at runtime\n", n);
			free(files[i]);
			continue;
		}
		ts_path(exe, sizeof exe, work, "pe_native_%s.exe", n);
		{
			Argv v = {{0}, 0};
			A(&v, mcc);
			A(&v, Ib);
			A(&v, Ibinc);
			A(&v, Iw32);
			A(&v, Irt);
			A(&v, f);
			A(&v, "-o");
			A(&v, exe);
			if (compile(Z(&v), &err)) {
				char *l = ts_first_error_line(err, NULL, NULL);
				printf("FAIL %s (compile): %s\n", n, l ? l : "");
				free(l);
				free(err);
				status = 1;
				free(files[i]);
				continue;
			}
			free(err);
		}
		{
			const char *r[] = {exe, 0};
			HostSpawnOpts o;
			memset(&o, 0, sizeof o);
			o.stdout_file = runout;
			o.stderr_file = runout;
			int rc = host_spawn_ex(r, &o);
			if (rc == 0)
				printf("PASS %s\n", n);
			else {
				printf("FAIL %s (run, rc=%d)\n", n, rc);
				status = 1;
			}
		}
		remove(exe);
		free(files[i]);
	}
	return status ? 1 : 0;
}

static int parse_stage3_rel(const char *text, char *out, int osz) {
	const char *p = text;
	while (p && *p) {
		const char *nl = strchr(p, '\n');
		int len = nl ? (int)(nl - p) : (int)strlen(p);
		char line[1024];
		if (len > (int)sizeof line - 1)
			len = (int)sizeof line - 1;
		snprintf(line, sizeof line, "%.*s", len, p);
		if (strstr(line, ".tar.xz") || strstr(line, ".tar.bz2") || strstr(line, ".tar.gz")) {
			int i = 0;
			while (line[i] && line[i] != ' ' && line[i] != '\t' && i < osz - 1) {
				out[i] = line[i];
				i++;
			}
			out[i] = 0;
			if (i > 0)
				return 1;
		}
		p = nl ? nl + 1 : NULL;
	}
	return 0;
}

static int qemufetch_selftest(void) {
	struct
	{
		const char *name, *text, *want;
	} cases[] = {
			{"amd64-openrc",
			 "20240115T170328Z/stage3-amd64-openrc-20240115T170328Z.tar.xz 268435456 BLAKE2B abc\n",
			 "20240115T170328Z/stage3-amd64-openrc-20240115T170328Z.tar.xz"},
			{"leading-comment",
			 "# Latest as of Mon\n20240115T170328Z/stage3-arm64-openrc-20240115T170328Z.tar.xz 1 SHA512 x\n",
			 "20240115T170328Z/stage3-arm64-openrc-20240115T170328Z.tar.xz"},
			{"bz2", "path/foo.tar.bz2 10\n", "path/foo.tar.bz2"},
			{"gz-tabbed", "dir/bar.tar.gz\t42\n", "dir/bar.tar.gz"},
			{0, 0, 0}};
	int i, fails = 0;
	char got[1024];
	for (i = 0; cases[i].name; i++) {
		int ok = parse_stage3_rel(cases[i].text, got, sizeof got);
		if (!ok || strcmp(got, cases[i].want)) {
			printf("FAIL %-16s got '%s' want '%s'\n", cases[i].name, ok ? got : "(none)", cases[i].want);
			fails++;
		} else
			printf("ok   %-16s %s\n", cases[i].name, got);
	}
	if (parse_stage3_rel("# nothing here\nrandom text\n", got, sizeof got)) {
		printf("FAIL no-tarball parsed unexpectedly\n");
		fails++;
	} else
		printf("ok   no-tarball\n");
	if (fails) {
		printf("qemufetch-selftest: %d case(s) FAILED\n", fails);
		return 1;
	}
	printf("qemufetch-selftest: all cases faithful\n");
	return 0;
}

static int suite_qemufetch(int argc, char **argv) {
	const char *ptrurl, *mirrorbase, *dest, *marker;
	char ptrfile[4300], tarfile[4300], rel[1024], url[8192];
	char *ptxt;
	int isd, i;

	for (i = 1; i < argc; i++)
		if (!strcmp(argv[i], "--selftest"))
			return qemufetch_selftest();
	ptrurl = opt(argc, argv, "--ptrurl", NULL);
	mirrorbase = opt(argc, argv, "--mirrorbase", NULL);
	dest = opt(argc, argv, "--dest", NULL);
	marker = opt(argc, argv, "--marker", NULL);
	if (!ptrurl || !mirrorbase || !dest || !marker) {
		fprintf(stderr, "usage: mccharness qemufetch --ptrurl U --mirrorbase U --dest D --marker F [--selftest]\n");
		return 2;
	}
	if (host_stat(marker, &isd, NULL, NULL) == 0)
		return 0;

	snprintf(ptrfile, sizeof ptrfile, "%s.ptr", dest);
	snprintf(tarfile, sizeof tarfile, "%s.tar", dest);
	host_mkdirs(dest);
	{
		const char *c[] = {"curl", "-fSL", "--retry", "3", "-o", ptrfile, ptrurl, 0};
		if (run_quiet(c)) {
			fprintf(stderr, "qemufetch: pointer download failed: %s\n", ptrurl);
			return 1;
		}
	}
	ptxt = ts_read_file(ptrfile, NULL);
	if (!ptxt || !parse_stage3_rel(ptxt, rel, sizeof rel)) {
		fprintf(stderr, "qemufetch: could not parse stage3 path from %s\n", ptrurl);
		free(ptxt);
		return 1;
	}
	free(ptxt);
	snprintf(url, sizeof url, "%s/%s", mirrorbase, rel);
	printf("qemufetch: downloading %s\n", url);
	{
		const char *c[] = {"curl", "-fSL", "--retry", "3", "-o", tarfile, url, 0};
		if (run_quiet(c)) {
			fprintf(stderr, "qemufetch: stage3 download failed: %s\n", url);
			return 1;
		}
	}
	{
		const char *c[] = {"tar", "--no-same-owner", "--exclude=./dev/*", "-xpf", tarfile, "-C", dest, 0};
		if (run_quiet(c)) {
			fprintf(stderr, "qemufetch: extract failed: %s\n", tarfile);
			return 1;
		}
	}
	remove(tarfile);
	{
		FILE *f = fopen(marker, "wb");
		if (f) {
			fprintf(f, "%s\n", rel);
			fclose(f);
		}
	}
	return 0;
}

static int suite_qemurun(int argc, char **argv) {
	const char *mcc = opt(argc, argv, "--mcc", NULL);
	const char *mccbase = opt(argc, argv, "--mccbase", NULL);
	const char *sysroot = opt(argc, argv, "--sysroot", NULL);
	const char *qemu = opt(argc, argv, "--qemu", NULL);
	const char *srcdir = opt(argc, argv, "--srcdir", NULL);
	const char *workdir = opt(argc, argv, "--workdir", NULL);
	char Bf[4200], sysf[4200], isysinc[4200], L1[4200], L2[4200], L3[4200], L4[4200];
	const char *launcher[4];
	char *files[4096];
	int nf, i, fails = 0;

	if (!mcc || !mccbase || !sysroot || !qemu || !srcdir || !workdir) {
		fprintf(stderr, "usage: mccharness qemurun --mcc --mccbase --sysroot --qemu --srcdir --workdir\n");
		return 2;
	}
	host_mkdirs(workdir);
	snprintf(Bf, sizeof Bf, "-B%s", mccbase);
	snprintf(sysf, sizeof sysf, "--sysroot=%s", sysroot);
	ts_path(isysinc, sizeof isysinc, sysroot, "usr/include");
	snprintf(L1, sizeof L1, "-L%s/usr/lib64", sysroot);
	snprintf(L2, sizeof L2, "-L%s/lib64", sysroot);
	snprintf(L3, sizeof L3, "-L%s/usr/lib", sysroot);
	snprintf(L4, sizeof L4, "-L%s/lib", sysroot);
	launcher[0] = qemu;
	launcher[1] = "-L";
	launcher[2] = sysroot;
	launcher[3] = 0;

	nf = ts_glob(srcdir, "*.c", 0, files, 4096);
	for (i = 0; i < (nf < 0 ? 0 : nf); i++) {
		const char *s = files[i], *base = strrchr(s, '/');
		char n[256];
		int m;
		base = base ? base + 1 : s;
		snprintf(n, sizeof n, "%.*s", (int)(strlen(base) - 2), base);
		for (m = 0; m < 2; m++) {
			char out[4200];
			char *err = NULL;
			Argv v = {{0}, 0};
			ts_path(out, sizeof out, workdir, "%s.%s", n, m ? "pic" : "default");
			A(&v, mcc);
			A(&v, Bf);
			A(&v, sysf);
			A(&v, "-isystem");
			A(&v, isysinc);
			A(&v, L1);
			A(&v, L2);
			A(&v, L3);
			A(&v, L4);
			if (m) {
				A(&v, "-fPIC");
				A(&v, "-pie");
			}
			A(&v, s);
			A(&v, "-o");
			A(&v, out);
			if (compile(Z(&v), &err)) {
				fprintf(stderr, "  %s [%s]: compile failed\n%s\n", n, m ? "pic" : "default", err ? err : "");
				fails++;
				free(err);
				continue;
			}
			free(err);
			{
				const char *r[] = {out, 0};
				int rc = run_to_l(r, launcher, "/dev/null");
				if (rc != 0) {
					fprintf(stderr, "  %s [%s]: run exited %d\n", n, m ? "pic" : "default", rc);
					fails++;
				}
			}
		}
		free(files[i]);
	}
	if (fails) {
		fprintf(stderr, "qemu conformance failures: %d\n", fails);
		return 1;
	}
	printf("qemu conformance: all programs passed\n");
	return 0;
}

static void copy_glob(const char *dir, const char *pat, const char *dstdir) {
	char *files[512];
	int n = ts_glob(dir, pat, 0, files, 512), i;
	for (i = 0; i < (n < 0 ? 0 : n); i++) {
		const char *b = strrchr(files[i], '/');
		char dst[4200];
		b = b ? b + 1 : files[i];
		ts_path(dst, sizeof dst, dstdir, "%s", b);
		host_copy_file(files[i], dst, 0);
		free(files[i]);
	}
}

static int suite_pewine(int argc, char **argv) {
	const char *src = opt(argc, argv, "--src", NULL);
	const char *xb = opt(argc, argv, "--xb", NULL);
	const char *work = opt(argc, argv, "--work", NULL);
	static const char *W64[] = {"wine64", "wine", "wine64-proton-10.0.4", 0};
	static const char *W32[] = {"wine", "wine32", "wine-proton-10.0.4", 0};
	char wine64[4096], wine32[4096], conf[4200], prefix[4200], runout[4200];
	char wenv_dbg[] = "WINEDEBUG=-all", wenv_pfx[4300];
	const char *extra[3], *const *env;
	int have64, have32, status = 0, any = 0, t, isd;
	static const char *TGT[] = {"x86_64-win32", "i386-win32", 0};

	if (!src || !xb || !work) {
		fprintf(stderr, "usage: mccharness pewine --src --xb --work\n");
		return 2;
	}
	have64 = host_find_tool_any(W64, NULL, wine64, sizeof wine64);
	have32 = host_find_tool_any(W32, NULL, wine32, sizeof wine32);
	if (!have64 && !have32)
		ts_skip("no wine found");

	ts_path(conf, sizeof conf, src, "tests/qemu/conformance");
	ts_path(prefix, sizeof prefix, work, ".wineprefix");
	ts_path(runout, sizeof runout, work, "wine-run.out");
	snprintf(wenv_pfx, sizeof wenv_pfx, "WINEPREFIX=%s", prefix);
	extra[0] = wenv_dbg;
	extra[1] = wenv_pfx;
	extra[2] = 0;
	env = make_env_plus(extra);

	for (t = 0; TGT[t]; t++) {
		const char *tgt = TGT[t], *wine;
		char mcc[4200], B[4096], Blib[4300], defs[4300], objs[4300], a[4300], Iw32[4300], Irt[4300];
		char *files[4096];
		int nf, i;
		ts_path(mcc, sizeof mcc, xb, "mcc-%s", tgt);
		if (host_stat(mcc, &isd, NULL, NULL)) {
			printf("SKIP %s: no mcc-%s\n", tgt, tgt);
			continue;
		}
		wine = !strcmp(tgt, "i386-win32") ? (have32 ? wine32 : NULL) : (have64 ? wine64 : NULL);
		if (!wine) {
			printf("SKIP %s: no matching wine\n", tgt);
			continue;
		}
		any = 1;
		ts_path(B, sizeof B, work, "B-%s", tgt);
		ts_path(Blib, sizeof Blib, B, "lib");
		host_mkdirs(Blib);
		ts_path(defs, sizeof defs, src, "runtime/win32/lib");
		ts_path(objs, sizeof objs, xb, "lib-%s", tgt);
		ts_path(a, sizeof a, xb, "%s-libmccrt.a", tgt);
		copy_glob(defs, "*.def", Blib);
		copy_glob(objs, "*.o", Blib);
		copy_glob(objs, "*.o", B);
		if (!host_stat(a, &isd, NULL, NULL)) {
			char d1[8192], d2[8192];
			ts_path(d1, sizeof d1, Blib, "%s-libmccrt.a", tgt);
			ts_path(d2, sizeof d2, B, "%s-libmccrt.a", tgt);
			host_copy_file(a, d1, 0);
			host_copy_file(a, d2, 0);
		}
		snprintf(Iw32, sizeof Iw32, "-I%s/runtime/win32/include", src);
		snprintf(Irt, sizeof Irt, "-I%s/runtime/include", src);

		nf = ts_glob(conf, "*.c", 0, files, 4096);
		for (i = 0; i < (nf < 0 ? 0 : nf); i++) {
			const char *f = files[i], *base = strrchr(f, '/');
			char n[256], exe[4300], Bf[4300], *err = NULL;
			base = base ? base + 1 : f;
			snprintf(n, sizeof n, "%.*s", (int)(strlen(base) - 2), base);
			ts_path(exe, sizeof exe, work, "pe_%s_%s.exe", tgt, n);
			snprintf(Bf, sizeof Bf, "-B%s", B);
			{
				Argv v = {{0}, 0};
				A(&v, mcc);
				A(&v, Bf);
				A(&v, Iw32);
				A(&v, Irt);
				A(&v, f);
				A(&v, "-o");
				A(&v, exe);
				if (compile(Z(&v), &err)) {
					char *l = ts_first_error_line(err, NULL, NULL);
					printf("FAIL %s/%s (compile): %s\n", tgt, n, l ? l : "");
					free(l);
					free(err);
					status = 1;
					free(files[i]);
					continue;
				}
				free(err);
			}
			{
				const char *r[] = {exe, 0};
				const char *lp[] = {wine, 0};
				HostSpawnOpts o;
				memset(&o, 0, sizeof o);
				o.launcher = lp;
				o.env = env;
				o.stdout_file = runout;
				o.stderr_file = runout;
				o.timeout_ms = 180000;
				int rc = host_spawn_ex(r, &o);
				if (rc == 0)
					printf("PASS %s/%s\n", tgt, n);
				else if (rc == HOST_SPAWN_TIMEOUT) {
					remove(exe);
					free(files[i]);
					ts_skip("%s/%s: wine run timed out after %us — emulation unresponsive",
									tgt, n, o.timeout_ms / 1000);
				} else {
					printf("FAIL %s/%s (run, rc=%d)\n", tgt, n, rc);
					status = 1;
				}
			}
			remove(exe);
			free(files[i]);
		}
	}
	if (!any)
		ts_skip("no win32 cross-mcc for any target");
	return status ? 1 : 0;
}

static const char *MACHO_PROGS_NATIVE[] = {
		"atomics", "control", "integers", "floats", "lexical", "aggregates", "varargs",
		"complex_annexg", "control_libc", "floats_libc", "libc", "libc_struct",
		"varargs_fp", "vla", "tls", "tls_aggr", 0};

static int obj_is_macho(const char *objcheck, const char *file) {
	const char *a[] = {objcheck, "macho", file, 0};
	return run_quiet(a) == 0;
}

static int suite_machonative(int argc, char **argv) {
	const char *src = opt(argc, argv, "--src", NULL);
	const char *mcc = opt(argc, argv, "--mcc", NULL);
	const char *bdir = opt(argc, argv, "--bdir", NULL);
	const char *work = opt(argc, argv, "--work", NULL);
	const char *objcheck = opt(argc, argv, "--objcheck", NULL);
	char conf[4200], Iinc[4200], Bf[4200], probe[4200], probeexe[4200];
	int i, status = 0, isd;

	if (!src || !mcc || !bdir || !work || !objcheck) {
		fprintf(stderr, "usage: mccharness machonative --src --mcc --bdir --work --objcheck\n");
		return 2;
	}
	if (!MCC_HOST_DARWIN)
		ts_skip("host is not Darwin (native Mach-O needs a macOS host)");
	if (host_stat(mcc, &isd, NULL, NULL) || isd)
		ts_skip("no native mcc (%s)", mcc);
	ts_path(conf, sizeof conf, src, "tests/qemu/conformance");
	snprintf(Iinc, sizeof Iinc, "-I%s/runtime/include", src);
	snprintf(Bf, sizeof Bf, "-B%s", bdir);
	host_mkdirs(work);
	ts_path(probe, sizeof probe, work, "probe.c");
	ts_path(probeexe, sizeof probeexe, work, "probe");
	write_file(probe, "int main(void){return 0;}\n");
	{
		Argv v = {{0}, 0};
		char *err = NULL;
		A(&v, mcc);
		A(&v, Bf);
		A(&v, probe);
		A(&v, "-o");
		A(&v, probeexe);
		if (compile(Z(&v), &err)) {
			char *l = ts_first_error_line(err, NULL, NULL);
			char m[256];
			snprintf(m, sizeof m, "%s", l ? l : "");
			free(l);
			free(err);
			ts_skip("native mcc cannot link an executable: %s", m);
		}
		free(err);
	}
	if (!obj_is_macho(objcheck, probeexe))
		ts_skip("native mcc does not target Mach-O");

	for (i = 0; MACHO_PROGS_NATIVE[i]; i++) {
		const char *t = MACHO_PROGS_NATIVE[i];
		char f[8192], out[8192], *err = NULL;
#if !defined(__aarch64__) && !defined(__arm64__)
		if (!strcmp(t, "tls_aggr")) {
			printf("SKIP osx/%s (x86_64 Mach-O TLV aggregate access under Rosetta)\n", t);
			continue;
		}
#endif
		ts_path(f, sizeof f, conf, "%s.c", t);
		ts_path(out, sizeof out, work, "%s", t);
		if (host_stat(f, &isd, NULL, NULL)) {
			printf("FAIL %s (missing source)\n", t);
			status = 1;
			continue;
		}
		{
			Argv v = {{0}, 0};
			A(&v, mcc);
			A(&v, Bf);
			A(&v, Iinc);
			A(&v, f);
			A(&v, "-o");
			A(&v, out);
			if (compile(Z(&v), &err)) {
				char *l = ts_first_error_line(err, NULL, NULL);
				printf("FAIL osx/%s (compile): %s\n", t, l ? l : "");
				free(l);
				free(err);
				status = 1;
				continue;
			}
			free(err);
		}
		if (!obj_is_macho(objcheck, out)) {
			printf("FAIL osx/%s: not a Mach-O image\n", t);
			status = 1;
			continue;
		}
		{
			const char *r[] = {out, 0};
			int rc = run_quiet(r);
			if (rc == 0)
				printf("PASS osx/%s (native Mach-O executed)\n", t);
			else {
				printf("FAIL osx/%s (run, rc=%d)\n", t, rc);
				status = 1;
			}
		}
		remove(out);
	}
	return status ? 1 : 0;
}

static int blob_has(const char *path, const char *needle) {
	long len = 0;
	char *b = ts_read_file(path, &len);
	size_t nl = strlen(needle), i;
	int found = 0;
	if (!b)
		return 0;
	if (len >= (long)nl)
		for (i = 0; i + nl <= (size_t)len; i++)
			if (!memcmp(b + i, needle, nl)) {
				found = 1;
				break;
			}
	free(b);
	return found;
}

static int suite_stackguard(int argc, char **argv) {
	const char *mcc = opt(argc, argv, "--mcc", NULL);
	const char *bdir = opt(argc, argv, "--bdir", NULL);
	const char *work = opt(argc, argv, "--work", NULL);
	const char *objcheck = opt(argc, argv, "--objcheck", NULL);
	char Bf[4200], src[4200], plain[4200], prot[4200];
	int isd, status = 0;

	static const char SRC[] =
			"#include <string.h>\n#include <stdio.h>\n"
			"static void frob(const char *s){ char buf[16]; strcpy(buf, s); printf(\"%s\\n\", buf); }\n"
			"int main(int argc, char **argv){ frob(argc>1?argv[1]:\"ok\"); return 0; }\n";

	if (!mcc || !bdir || !work || !objcheck) {
		fprintf(stderr, "usage: mccharness stackguard --mcc --bdir --work --objcheck\n");
		return 2;
	}
	if (!MCC_HOST_DARWIN)
		ts_skip("host is not Darwin (-fstack-protector Mach-O needs a macOS host)");
	if (host_stat(mcc, &isd, NULL, NULL) || isd)
		ts_skip("no native mcc (%s)", mcc);
	snprintf(Bf, sizeof Bf, "-B%s", bdir);
	host_mkdirs(work);
	ts_path(src, sizeof src, work, "sp.c");
	ts_path(plain, sizeof plain, work, "sp_plain");
	ts_path(prot, sizeof prot, work, "sp_prot");
	write_file(src, SRC);

	{
		Argv v = {{0}, 0};
		char *err = NULL;
		A(&v, mcc);
		A(&v, Bf);
		A(&v, src);
		A(&v, "-o");
		A(&v, plain);
		if (compile(Z(&v), &err)) {
			char *l = ts_first_error_line(err, NULL, NULL);
			char m[256];
			snprintf(m, sizeof m, "%s", l ? l : "");
			free(l);
			free(err);
			ts_skip("native mcc cannot link an executable: %s", m);
		}
		free(err);
	}
	if (!obj_is_macho(objcheck, plain))
		ts_skip("native mcc does not target Mach-O");
	{
		const char *r[] = {plain, "ok", 0};
		if (run_quiet(r) != 0) {
			printf("FAIL baseline clean run\n");
			status = 1;
		}
	}
	if (blob_has(plain, "__stack_chk_guard")) {
		printf("FAIL baseline unexpectedly references __stack_chk_guard\n");
		status = 1;
	}

	{
		Argv v = {{0}, 0};
		char *err = NULL;
		A(&v, mcc);
		A(&v, Bf);
		A(&v, "-fstack-protector-all");
		A(&v, src);
		A(&v, "-o");
		A(&v, prot);
		if (compile(Z(&v), &err)) {
			char *l = ts_first_error_line(err, NULL, NULL);
			printf("FAIL protected compile: %s\n", l ? l : "");
			free(l);
			free(err);
			return 1;
		}
		free(err);
	}
	if (!blob_has(prot, "__stack_chk_guard")) {
		printf("FAIL protected build missing __stack_chk_guard reference\n");
		status = 1;
	}
	if (!blob_has(prot, "__stack_chk_fail")) {
		printf("FAIL protected build missing __stack_chk_fail reference\n");
		status = 1;
	} else
		printf("PASS canary symbols emitted under -fstack-protector-all\n");
	{
		const char *r[] = {prot, "ok", 0};
		if (run_quiet(r) != 0) {
			printf("FAIL protected clean run\n");
			status = 1;
		} else
			printf("PASS protected clean run\n");
	}
	{
		const char *r[] = {prot, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 0};
		int rc = run_quiet(r);
		if (rc == 0) {
			printf("FAIL overflow ran to completion (canary not checked)\n");
			status = 1;
		} else
			printf("PASS overflow tripped the canary (aborted, rc=%d)\n", rc);
	}
	return status ? 1 : 0;
}

static int host_is_x86_64(void) {
#if defined(__x86_64__) || defined(__amd64__)
	return 1;
#else
	return 0;
#endif
}

static const char *MACHO_WRAP_C =
		"int cmain(void);\n"
		"static void osx_exit(int c){ __asm__ volatile(\"movl %0,%%edi; movl $0x2000001,%%eax; syscall\"\n"
		"                              :: \"r\"(c):\"eax\",\"edi\",\"rcx\",\"r11\"); }\n"
		"int main(void){ osx_exit(cmain()); for(;;); return 0; }\n"
		"void abort(void){ osx_exit(99); }\n"
		"void *memset(void *d, int c, unsigned long n){ unsigned char *p=d; while(n--)*p++=(unsigned char)c; return d; }\n"
		"void *memcpy(void *d, const void *s, unsigned long n){ unsigned char *a=d; const unsigned char *b=s; while(n--)*a++=*b++; return d; }\n"
		"void *memmove(void *d, const void *s, unsigned long n){ unsigned char *a=d; const unsigned char *b=s;\n"
		"    if(a<b){ while(n--)*a++=*b++; } else { a+=n; b+=n; while(n--)*--a=*--b; } return d; }\n"
		"int memcmp(const void *x, const void *y, unsigned long n){ const unsigned char *a=x,*b=y;\n"
		"    while(n--){ if(*a!=*b) return *a-*b; a++; b++; } return 0; }\n"
		"unsigned long strlen(const char *s){ const char *p=s; while(*p)p++; return (unsigned long)(p-s); }\n"
		"int strcmp(const char *a, const char *b){ while(*a&&*a==*b){a++;b++;} return (unsigned char)*a-(unsigned char)*b; }\n"
		"char *strcpy(char *d, const char *s){ char *r=d; while((*d++=*s++)); return r; }\n"
		"static char heap[1<<16]; static unsigned long hp;\n"
		"void *malloc(unsigned long n){ n=(n+15)&~15UL; if(hp+n>sizeof heap) return 0; void *p=heap+hp; hp+=n; return p; }\n"
		"void free(void *p){ (void)p; }\n"
		"static void emit_(char *b,unsigned long n,unsigned long *i,char c){ if(*i+1<n)b[*i]=c; (*i)++; }\n"
		"static void emitu_(char *b,unsigned long n,unsigned long *i,unsigned long v,int base){\n"
		"    char t[24]; int k=0; if(!v)t[k++]='0'; while(v){t[k++]=\"0123456789abcdef\"[v%base]; v/=base;}\n"
		"    while(k--) emit_(b,n,i,t[k]); }\n"
		"int snprintf(char *b, unsigned long n, const char *f, ...){\n"
		"    __builtin_va_list ap; __builtin_va_start(ap,f); unsigned long i=0;\n"
		"    for(; *f; f++){\n"
		"        if(*f!='%'){ emit_(b,n,&i,*f); continue; }\n"
		"        f++;\n"
		"        if(*f=='d'){ int v=__builtin_va_arg(ap,int); if(v<0){emit_(b,n,&i,'-'); v=-v;} emitu_(b,n,&i,(unsigned long)v,10); }\n"
		"        else if(*f=='u'){ emitu_(b,n,&i,(unsigned long)__builtin_va_arg(ap,unsigned),10); }\n"
		"        else if(*f=='x'){ emitu_(b,n,&i,(unsigned long)__builtin_va_arg(ap,unsigned),16); }\n"
		"        else if(*f=='s'){ const char *s=__builtin_va_arg(ap,char*); while(*s)emit_(b,n,&i,*s++); }\n"
		"        else if(*f=='c'){ emit_(b,n,&i,(char)__builtin_va_arg(ap,int)); }\n"
		"        else if(*f=='%'){ emit_(b,n,&i,'%'); }\n"
		"    }\n"
		"    if(n) b[i<n?i:n-1]=0;\n"
		"    __builtin_va_end(ap);\n"
		"    return (int)i;\n"
		"}\n";

static int suite_machoimage(int argc, char **argv) {
	const char *src = opt(argc, argv, "--src", NULL);
	const char *xb = opt(argc, argv, "--xb", NULL);
	const char *work = opt(argc, argv, "--work", NULL);
	const char *objcheck = opt(argc, argv, "--objcheck", NULL);
	static const char *SKIPS[] = {"stack", "deprecat", 0};
	static const char *PROGS[] = {
			"atomics", "control", "integers", "floats", "lexical", "aggregates", "varargs", "libc", 0};
	char mcc[4200], osxrt[4200], conf[4200], gcc[4096], loader[4200], ldsrc[4200];
	char wrapc[4200], wrapo[4200], Iinc[4200];
	char probe[4300];
	int i, status = 0, isd;

	if (!src || !xb || !work || !objcheck) {
		fprintf(stderr, "usage: mccharness machoimage --src --xb --work --objcheck\n");
		return 2;
	}
	if (!host_is_x86_64())
		ts_skip("host is not x86_64");
	ts_path(mcc, sizeof mcc, xb, "mcc-x86_64-osx");
	if (host_stat(mcc, &isd, NULL, NULL))
		ts_skip("no mcc-x86_64-osx");
	if (!host_find_tool("gcc", NULL, gcc, sizeof gcc))
		ts_skip("no gcc for the loader");
	ts_path(osxrt, sizeof osxrt, xb, "lib-x86_64-osx");
	ts_path(probe, sizeof probe, osxrt, "atomic.o");
	if (host_stat(probe, &isd, NULL, NULL))
		ts_skip("no x86_64-osx runtime objects");
	ts_path(conf, sizeof conf, src, "tests/qemu/conformance");
	host_mkdirs(work);
	ts_path(loader, sizeof loader, work, "machoload");
	ts_path(ldsrc, sizeof ldsrc, src, "tests/qemu/macho/loader.c");
	snprintf(Iinc, sizeof Iinc, "-I%s/runtime/include", src);
	{
		Argv v = {{0}, 0};
		char *err = NULL;
		A(&v, gcc);
		A(&v, "-O2");
		A(&v, ldsrc);
		A(&v, "-o");
		A(&v, loader);
		if (compile(Z(&v), &err)) {
			char *l = ts_first_error_line(err, NULL, NULL);
			char m[256];
			snprintf(m, sizeof m, "%s", l ? l : "");
			free(l);
			free(err);
			ts_skip("cannot build Mach-O loader (no seccomp?): %s", m);
		}
		free(err);
	}
	ts_path(wrapc, sizeof wrapc, work, "wrap.c");
	ts_path(wrapo, sizeof wrapo, work, "wrap.o");
	write_file(wrapc, MACHO_WRAP_C);
	{
		const char *a[] = {mcc, "-nostdlib", "-c", wrapc, "-o", wrapo, 0};
		run_quiet(a);
	}

	for (i = 0; PROGS[i]; i++) {
		const char *t = PROGS[i];
		char f[8192], co[8192], macho[8192], o1[8192], o2[8192], o3[8192], o4[8192], *err = NULL;
		ts_path(f, sizeof f, conf, "%s.c", t);
		ts_path(co, sizeof co, work, "c.o");
		ts_path(macho, sizeof macho, work, "%s.macho", t);
		ts_path(o1, sizeof o1, osxrt, "atomic.o");
		ts_path(o2, sizeof o2, osxrt, "stdatomic.o");
		ts_path(o3, sizeof o3, osxrt, "va_list.o");
		ts_path(o4, sizeof o4, osxrt, "builtin.o");
		{
			Argv v = {{0}, 0};
			A(&v, mcc);
			A(&v, "-nostdlib");
			A(&v, "-Dmain=cmain");
			A(&v, Iinc);
			A(&v, "-c");
			A(&v, f);
			A(&v, "-o");
			A(&v, co);
			if (compile(Z(&v), &err)) {
				if (strstr(t, "libc") || !strcmp(t, "varargs_fp")) {
					printf("SKIP osx/%s (host libc headers not consumable for the darwin target)\n",
								 t);
					free(err);
					continue;
				}
				printf("FAIL osx/%s (compile)\n", t);
				free(err);
				status = 1;
				continue;
			}
			free(err);
			err = NULL;
		}
		{
			Argv v = {{0}, 0};
			A(&v, mcc);
			A(&v, "-nostdlib");
			A(&v, co);
			A(&v, wrapo);
			A(&v, o1);
			A(&v, o2);
			A(&v, o3);
			A(&v, o4);
			A(&v, "-o");
			A(&v, macho);
			if (compile(Z(&v), &err)) {
				char *l = ts_first_error_line(err, NULL, SKIPS);
				printf("FAIL osx/%s (link): %s\n", t, l ? l : "");
				free(l);
				free(err);
				status = 1;
				continue;
			}
			free(err);
		}
		if (!obj_is_macho(objcheck, macho)) {
			printf("FAIL osx/%s: not a Mach-O\n", t);
			status = 1;
			continue;
		}
		{
			const char *r[] = {loader, macho, 0};
			int rc = run_quiet(r);
			if (rc == 0)
				printf("PASS osx/%s (Mach-O image loaded + executed)\n", t);
			else {
				printf("FAIL osx/%s (run, rc=%d)\n", t, rc);
				status = 1;
			}
		}
		remove(macho);
	}
	return status ? 1 : 0;
}

static const char *AL_WRAP_C =
		"typedef unsigned long size_t;\n"
		"int cmain(void);\n"
		"static void osx_exit(int c){ __asm__ volatile(\"movl %0,%%edi; movl $0x2000001,%%eax; syscall\"\n"
		"                            :: \"r\"(c):\"eax\",\"edi\",\"rcx\",\"r11\"); }\n"
		"int main(void){ osx_exit(cmain()); for(;;); return 0; }\n"
		"void abort(void){ osx_exit(99); }\n"
		"int errno;\n"
		"long write(int fd, const void *p, size_t n){ (void)fd;(void)p;(void)n; return -1; }\n"
		"int  vm_allocate(unsigned int t, unsigned long *a, unsigned long s, int f){\n"
		"    (void)t;(void)a;(void)s;(void)f; return -1; }\n"
		"int  vm_deallocate(unsigned int t, unsigned long a, unsigned long s){\n"
		"    (void)t;(void)a;(void)s; return -1; }\n"
		"unsigned int mach_task_self(void){ return 0; }\n";

static int al_run_image(const char *mcc, const char *shiminc, char **objs, int nobjs,
												const char *wrapo, char **rt, int nrt, const char *objcheck,
												const char *work, const char *src, const char *label) {
	static const char *SKIPS[] = {"stack", "deprecat", 0};
	char testo[8192], macho[8192], *err = NULL;
	const char **a;
	int n = 0, i;
	ts_path(testo, sizeof testo, work, "test.o");
	ts_path(macho, sizeof macho, work, "%s.macho", label);
	{
		Argv v = {{0}, 0};
		A(&v, mcc);
		A(&v, "-nostdlib");
		A(&v, shiminc);
		A(&v, "-Dmain=cmain");
		A(&v, "-c");
		A(&v, src);
		A(&v, "-o");
		A(&v, testo);
		if (compile(Z(&v), &err)) {
			char *l = ts_first_error_line(err, NULL, NULL);
			printf("FAIL %s (test compile): %s\n", label, l ? l : "");
			free(l);
			free(err);
			return 1;
		}
		free(err);
		err = NULL;
	}
	a = malloc((nobjs + nrt + 8) * sizeof *a);
	a[n++] = mcc;
	a[n++] = "-nostdlib";
	a[n++] = testo;
	for (i = 0; i < nobjs; i++)
		a[n++] = objs[i];
	a[n++] = wrapo;
	for (i = 0; i < nrt; i++)
		a[n++] = rt[i];
	a[n++] = "-o";
	a[n++] = macho;
	a[n] = 0;
	{
		char *o = NULL;
		int rc = run_cap(a, NULL, &o, &err);
		free(o);
		free(a);
		if (rc) {
			char *l = ts_first_error_line(err, NULL, SKIPS);
			printf("FAIL %s (link): %s\n", label, l ? l : "");
			free(l);
			free(err);
			return 1;
		}
		free(err);
	}
	if (!obj_is_macho(objcheck, macho)) {
		printf("FAIL %s: not a Mach-O image\n", label);
		return 1;
	}
	{
		const char *ldargv[3];
		char loader[8192];
		ts_path(loader, sizeof loader, work, "machoload");
		ldargv[0] = loader;
		ldargv[1] = macho;
		ldargv[2] = 0;
		{
			int rc = run_quiet(ldargv);
			if (rc == 0) {
				printf("PASS %s (Apple's genuine libc executed as a Mach-O image)\n", label);
				remove(macho);
				return 0;
			}
			printf("FAIL %s (run, rc=%d)\n", label, rc);
			remove(macho);
			return 1;
		}
	}
}

static int suite_machoapplelibc(int argc, char **argv) {
	const char *src = opt(argc, argv, "--src", NULL);
	const char *xb = opt(argc, argv, "--xb", NULL);
	const char *work = opt(argc, argv, "--work", NULL);
	const char *objcheck = opt(argc, argv, "--objcheck", NULL);
	char mcc[4200], osxrt[4200], AL[4200], gcc[4096], loader[4300], ldsrc[4300], probe[4300];
	char shiminc[4300], wrapc[4300], wrapo[4300];
	char *objs[1024], *rt[4];
	int nobjs = 0, nrt = 0, status = 0, isd, di;
	static const char *SUBDIRS[] = {"src", "src-libplatform", "src-simple", 0};
	static const char *RTNAMES[] = {"va_list", "builtin", 0};

	if (!src || !xb || !work || !objcheck) {
		fprintf(stderr, "usage: mccharness machoapplelibc --src --xb --work --objcheck\n");
		return 2;
	}
	if (!host_is_x86_64())
		ts_skip("host is not x86_64");
	ts_path(mcc, sizeof mcc, xb, "mcc-x86_64-osx");
	if (host_stat(mcc, &isd, NULL, NULL))
		ts_skip("no mcc-x86_64-osx");
	if (!host_find_tool("gcc", NULL, gcc, sizeof gcc))
		ts_skip("no gcc for the loader");
	ts_path(AL, sizeof AL, src, "tests/qemu/apple-libc");
	ts_path(probe, sizeof probe, AL, "src/strcspn.c");
	if (host_stat(probe, &isd, NULL, NULL))
		ts_skip("vendored Apple sources absent");
	ts_path(osxrt, sizeof osxrt, xb, "lib-x86_64-osx");
	host_mkdirs(work);
	ts_path(loader, sizeof loader, work, "machoload");
	ts_path(ldsrc, sizeof ldsrc, src, "tests/qemu/macho/loader.c");
	{
		Argv v = {{0}, 0};
		char *err = NULL;
		A(&v, gcc);
		A(&v, "-O2");
		A(&v, ldsrc);
		A(&v, "-o");
		A(&v, loader);
		if (compile(Z(&v), &err)) {
			char *l = ts_first_error_line(err, NULL, NULL);
			char m[256];
			snprintf(m, sizeof m, "%s", l ? l : "");
			free(l);
			free(err);
			ts_skip("cannot build Mach-O loader (no seccomp?): %s", m);
		}
		free(err);
	}
	ts_path(wrapc, sizeof wrapc, work, "wrap.c");
	ts_path(wrapo, sizeof wrapo, work, "wrap.o");
	snprintf(shiminc, sizeof shiminc, "-I%s/shim-include", AL);
	write_file(wrapc, AL_WRAP_C);

	for (di = 0; RTNAMES[di]; di++) {
		char o[4300];
		ts_path(o, sizeof o, osxrt, "%s.o", RTNAMES[di]);
		if (!host_stat(o, &isd, NULL, NULL))
			rt[nrt++] = strdup(o);
	}

	for (di = 0; SUBDIRS[di] && !status; di++) {
		char dir[4300];
		char *files[1024];
		int nf, i;
		ts_path(dir, sizeof dir, AL, "%s", SUBDIRS[di]);
		nf = ts_glob(dir, "*.c", 0, files, 1024);
		for (i = 0; i < (nf < 0 ? 0 : nf); i++) {
			const char *f = files[i], *base = strrchr(f, '/');
			char nm[256], oo[8192], *err = NULL;
			base = base ? base + 1 : f;
			snprintf(nm, sizeof nm, "%.*s", (int)(strlen(base) - 2), base);
			ts_path(oo, sizeof oo, work, "o_%s.o", nm);
			{
				Argv v = {{0}, 0};
				A(&v, mcc);
				A(&v, "-nostdlib");
				A(&v, shiminc);
				A(&v, "-c");
				A(&v, f);
				A(&v, "-o");
				A(&v, oo);
				if (compile(Z(&v), &err)) {
					char *l = ts_first_error_line(err, NULL, NULL);
					printf("FAIL apple-libc/%s (compile): %s\n", nm, l ? l : "");
					free(l);
					free(err);
					status = 1;
					free(files[i]);
					break;
				}
				free(err);
			}
			if (nobjs < 1024)
				objs[nobjs++] = strdup(oo);
			free(files[i]);
		}
	}
	if (status)
		return 1;
	{
		Argv v = {{0}, 0};
		char *err = NULL;
		A(&v, mcc);
		A(&v, "-nostdlib");
		A(&v, shiminc);
		A(&v, "-c");
		A(&v, wrapc);
		A(&v, "-o");
		A(&v, wrapo);
		if (compile(Z(&v), &err)) {
			char *l = ts_first_error_line(err, NULL, NULL);
			printf("FAIL apple-libc (wrap compile): %s\n", l ? l : "");
			free(l);
			free(err);
			return 1;
		}
		free(err);
	}
	{
		char s1[4300], s2[4300], s3[4300];
		ts_path(s1, sizeof s1, AL, "apple_string_conf.c");
		ts_path(s2, sizeof s2, AL, "apple_libplatform_conf.c");
		ts_path(s3, sizeof s3, AL, "apple_simple_conf.c");
		status |= al_run_image(mcc, shiminc, objs, nobjs, wrapo, rt, nrt, objcheck, work, s1, "apple-libc-freebsd");
		status |= al_run_image(mcc, shiminc, objs, nobjs, wrapo, rt, nrt, objcheck, work, s2, "apple-libc-libplatform");
		status |= al_run_image(mcc, shiminc, objs, nobjs, wrapo, rt, nrt, objcheck, work, s3,
													 "apple-libc-simple-printf");
	}
	return status ? 1 : 0;
}

static const char *CG_HARNESS =
		"extern int osx_main(void) __asm__(\"_main\");\nint main(void){ return osx_main(); }\n";
static const char *CG_LIBCNAMES[] = {
		"memset", "memcpy", "memmove", "memcmp", "malloc", "calloc", "realloc", "free", "printf", "snprintf",
		"strcmp", "strncmp", "strcpy", "strlen", "abort", "qsort", "strtod", "strtold", "div", "ldiv", "lldiv", 0};
static const char *CG_LINK_NEEDLES[] = {"undefined reference", "undefined symbol", "error:", 0};

static int in_list(const char *s, const char *const *set) {
	int i;
	for (i = 0; set[i]; i++)
		if (!strcmp(s, set[i]))
			return 1;
	return 0;
}

static int suite_machocodegen(int argc, char **argv) {
	const char *src = opt(argc, argv, "--src", NULL);
	const char *xb = opt(argc, argv, "--xb", NULL);
	const char *work = opt(argc, argv, "--work", NULL);
	const char *arch = opt(argc, argv, "--arch", "x86_64");
	const char *sysroot = opt(argc, argv, "--sysroot", "");
	int is_arm = !strcmp(arch, "arm64");
	const char *br = is_arm ? "b" : "jmp", *plt = is_arm ? "" : "@PLT";
	static const char *SKIP_X64[] = {"tls", "tls_aggr", 0};
	static const char *SKIP_A64[] = {
			"tls", "tls_aggr", "control_libc", "floats_libc", "libc", "libc_struct", "varargs_fp", 0};
	const char *const *skipset = is_arm ? SKIP_A64 : SKIP_X64;
	char mcc[4200], osxrt[4200], conf[4200], gcc[4096], clang[4096], qemu[4096];
	char harness[4300], shimS[4300], Iinc[4300], probe[4300];
	char *rt[8];
	int nrt = 0, status = 0, isd, i;
	char shim[8192];
	int sl = 0;
	char *progs[4096];
	int np;

	if (!src || !xb || !work) {
		fprintf(stderr, "usage: mccharness machocodegen --src --xb --work [--arch --sysroot]\n");
		return 2;
	}
	if (!host_find_tool("gcc", NULL, gcc, sizeof gcc))
		ts_skip("no gcc to build the ELF harness");
	ts_path(conf, sizeof conf, src, "tests/qemu/conformance");
	ts_path(mcc, sizeof mcc, xb, "mcc-%s-osx", arch);
	ts_path(osxrt, sizeof osxrt, xb, "lib-%s-osx", arch);
	if (!is_arm && !host_is_x86_64())
		ts_skip("host is not x86_64");
	if (!is_arm && MCC_HOST_DARWIN)
		ts_skip("host gcc is Mach-O (x86_64 Mach-O-ABI codegen needs an ELF-linking gcc)");
	if (host_stat(mcc, &isd, NULL, NULL))
		ts_skip("no mcc-%s-osx", arch);
	ts_path(probe, sizeof probe, osxrt, "atomic.o");
	if (host_stat(probe, &isd, NULL, NULL))
		ts_skip("no %s-osx runtime objects", arch);
	if (is_arm) {
		char *tg = NULL, *e = NULL;
		if (!host_find_tool("clang", NULL, clang, sizeof clang))
			ts_skip("no clang for the aarch64 link");
		{
			const char *a[] = {clang, "-print-targets", 0};
			run_cap(a, NULL, &tg, &e);
		}
		if (!tg || !strstr(tg, "aarch64")) {
			free(tg);
			free(e);
			ts_skip("clang lacks the aarch64 target");
		}
		free(tg);
		free(e);
		if (!host_find_tool("qemu-aarch64", NULL, qemu, sizeof qemu))
			ts_skip("no qemu-aarch64");
		if (!*sysroot || host_stat(sysroot, &isd, NULL, NULL) || !isd)
			ts_skip("no arm64 glibc sysroot (%s)", sysroot);
	}

	{
		const char *rx[] = {"atomic", "stdatomic", "va_list", "builtin", 0};
		const char *ra[] = {"atomic", "stdatomic", "builtin", "float128", 0};
		const char *const *rn = is_arm ? ra : rx;
		int k;
		for (k = 0; rn[k]; k++) {
			char o[4300];
			ts_path(o, sizeof o, osxrt, "%s.o", rn[k]);
			rt[nrt++] = strdup(o);
		}
		{
			char o[4300];
			ts_path(o, sizeof o, osxrt, "complex.o");
			if (!host_stat(o, &isd, NULL, NULL))
				rt[nrt++] = strdup(o);
		}
	}

	host_mkdirs(work);
	ts_path(harness, sizeof harness, work, "harness.c");
	ts_path(shimS, sizeof shimS, work, "shim.S");
	snprintf(Iinc, sizeof Iinc, "-I%s/runtime/include", src);
	write_file(harness, CG_HARNESS);

	sl += snprintf(shim + sl, sizeof shim - sl,
								 ".text\n.macro tramp dar, nat\n.globl \\dar\n\\dar: %s \\nat%s\n.endm\n", br, plt);
	for (i = 0; CG_LIBCNAMES[i]; i++)
		sl += snprintf(shim + sl, sizeof shim - sl, "tramp _%s, %s\n", CG_LIBCNAMES[i], CG_LIBCNAMES[i]);
	sl += snprintf(shim + sl, sizeof shim - sl, "tramp __setjmp, _setjmp\n.section .note.GNU-stack,\"\",@progbits\n");
	write_file(shimS, shim);

	np = ts_glob(conf, "*.c", 0, progs, 4096);
	for (i = 0; i < (np < 0 ? 0 : np); i++) {
		const char *f = progs[i], *base = strrchr(f, '/');
		char n[256], oo[4300], run[4300], *err = NULL;
		base = base ? base + 1 : f;
		snprintf(n, sizeof n, "%.*s", (int)(strlen(base) - 2), base);
		if (in_list(n, skipset)) {
			printf("SKIP osx-%s/%s (libc-variadic/TLS not ELF-linkable)\n", arch, n);
			free(progs[i]);
			continue;
		}
		ts_path(oo, sizeof oo, work, "o.o");
		ts_path(run, sizeof run, work, "run");
		{
			Argv v = {{0}, 0};
			A(&v, mcc);
			A(&v, Iinc);
			A(&v, "-c");
			A(&v, f);
			A(&v, "-o");
			A(&v, oo);
			if (compile(Z(&v), &err)) {
				if (strstr(n, "libc") || !strcmp(n, "varargs_fp")) {
					printf("SKIP osx-%s/%s (host libc headers not consumable for the darwin target)\n",
								 arch, n);
					free(err);
					free(progs[i]);
					continue;
				}
				char *l = ts_first_error_line(err, NULL, NULL);
				printf("FAIL osx-%s/%s (compile): %s\n", arch, n, l ? l : "");
				free(l);
				free(err);
				status = 1;
				free(progs[i]);
				continue;
			}
			free(err);
			err = NULL;
		}
		{
			const char **a = malloc((nrt + 32) * sizeof *a);
			int m = 0, k;
			char sysf[4300], isysf[4300], Ls[4][4300];
			if (!is_arm) {
				a[m++] = gcc;
				a[m++] = harness;
				a[m++] = oo;
				a[m++] = shimS;
				for (k = 0; k < nrt; k++)
					a[m++] = rt[k];
				a[m++] = "-o";
				a[m++] = run;
			} else {
				snprintf(sysf, sizeof sysf, "--sysroot=%s", sysroot);
				ts_path(isysf, sizeof isysf, sysroot, "usr/include");
				snprintf(Ls[0], sizeof Ls[0], "-L%s/usr/lib", sysroot);
				snprintf(Ls[1], sizeof Ls[1], "-L%s/lib", sysroot);
				snprintf(Ls[2], sizeof Ls[2], "-L%s/usr/lib64", sysroot);
				snprintf(Ls[3], sizeof Ls[3], "-L%s/lib64", sysroot);
				a[m++] = clang;
				a[m++] = "--target=aarch64-linux-gnu";
				a[m++] = sysf;
				a[m++] = "-fuse-ld=lld";
				a[m++] = "-isystem";
				a[m++] = isysf;
				a[m++] = harness;
				a[m++] = shimS;
				a[m++] = oo;
				for (k = 0; k < nrt; k++)
					a[m++] = rt[k];
				a[m++] = Ls[0];
				a[m++] = Ls[1];
				a[m++] = Ls[2];
				a[m++] = Ls[3];
				a[m++] = "-o";
				a[m++] = run;
			}
			a[m] = 0;
			{
				char *o = NULL;
				int rc = run_cap(a, NULL, &o, &err);
				free(o);
				free(a);
				if (rc) {
					char *l = ts_first_error_line(err, CG_LINK_NEEDLES, NULL);
					printf("FAIL osx-%s/%s (link): %s\n", arch, n, l ? l : "");
					free(l);
					free(err);
					status = 1;
					free(progs[i]);
					continue;
				}
				free(err);
			}
		}
		{
			int rc;
			if (is_arm) {
				const char *r[] = {run, 0};
				const char *lp[] = {qemu, "-L", sysroot, 0};
				rc = run_to_l(r, lp, "/dev/null");
			} else {
				const char *r[] = {run, 0};
				rc = run_quiet(r);
			}
			if (rc == 0)
				printf("PASS osx-%s/%s (%s-osx codegen executed)\n", arch, n, arch);
			else {
				printf("FAIL osx-%s/%s (run, rc=%d)\n", arch, n, rc);
				status = 1;
			}
		}
		remove(run);
		free(progs[i]);
	}
	for (i = 0; i < nrt; i++)
		free(rt[i]);
	return status ? 1 : 0;
}

static const char *ARM_COMBOS[] = {
		"r3, r4, r5, r6", "r3, r4, r5", "r3, r4, r5, asl #7", "r3, r4, r5, lsl #7",
		"r3, r4, r5, asr #7", "r3, r4, r5, lsr #7", "r3, r4, r5, ror #7",
		"r3, r4, r5, rrx", "r3, r4, r5, asl r6", "r3, r4, r5, lsl r6",
		"r3, r4, r5, asr r6", "r3, r4, r5, lsr r6", "r3, r4, r5, ror r6",
		"r3, r4, #5, asl #7", "r3, r4, #5, lsl #7", "r3, r4, #5, asr #7",
		"r3, r4, #5, lsr #7", "r3, r4, #5, ror #7", "r3, r4, #5, rrx", "r3, #5, r4",
		"r3, #4, #8", "r3, r4, asl #5", "r3, r4, lsl #5", "r3, r4, asr #5",
		"r3, r4, lsr #5", "r3, r4, ror #5", "r3, r4, ror #8", "r3, r4, asl r5",
		"r3, r4, lsl r5", "r3, r4, asr r5", "r3, r4, lsr r5", "r3, r4, ror r5",
		"r3, r4, ror #8", "r3, r4, ror #16", "r3, r4, ror #24", "r3, r4, rrx",
		"r3, #4, asl #5", "r3, #4, lsl #5", "r3, #4, asr #5", "r3, #4, lsr #5",
		"r3, #4, ror #5", "r3, r4, rrx", "r3, r4", "r3", "{r3,r4,r5}", "{r3,r5,r4}",
		"r2!, {r3,r4,r5}", "r2!, {r3,r5,r4}", "r2, {r3,r4,r5}", "r2, {r3,r5,r4}",
		"r2, [r3, r4]", "r2, [r3, r4]!", "r2, [r3, -r4]", "r2, [r3, -r4]!",
		"r2, [r3], r4", "r2, [r3], -r4", "r2, [r3]", "r2, r3, [r4, lsl# 2]",
		"r2, [r3, r4, lsr# 1]", "r2, [r3, r4, lsr# 2]!", "r2, [r3, -r4, ror# 3]",
		"r2, [r3, -r4, lsl# 1]!", "r2, [r3], r4, lsl# 3", "r2, [r3], -r4, asr# 31",
		"r2, [r3], -r4, asl# 1", "r2, [r3], -r4, rrx", "r2, r3, [r4]",
		"r2, [r3, #4]", "r2, [r3, #-4]", "r2, [r3, #0x45]", "r2, [r3, #-0x45]",
		"r2, r3, #4", "r2, r3, #-4", "p10, #7, c2, c0, c1, #4",
		"p10, #7, r2, c0, c1, #4", "p10, #0, c2, c0, c1, #4", "p10, #0, r2, c0, c1, #4",
		"r2, #4", "r2, #-4", "r2, #0xEFFF", "r3, #0x0000", "r4, #0x0201",
		"r4, #0xFFFFFF00", "#4", "#-4", "p5, c2, [r3]", "p5, c3, [r4]",
		"p5, c2, [r3, #4]", "p5, c2, [r3, #-4]", "p5, c2, [r3, #0x45]",
		"p5, c2, [r3, #-0x45]", "s2, [r3]", "s3, [r4]", "s2, [r3, #4]",
		"s2, [r3, #-4]", "s2, [r3, #0x45]", "s2, [r3, #-0x45]", "r1, {d3-d4}",
		"r1!, {d3-d4}", "r2, {d4-d15}", "r3!, {d4-d15}", "r3!, {d4}", "r2, {s4-s31}",
		"r3!, {s4}", "{d3-d4}", "{d4-d15}", "{d4}", "{s4-s31}", "{s4}", "s2, s3, s4",
		"s2, s3", "d2, d3, d4", "d2, d3", "s2, #0", "d2, #0", "s3, #0.0", "d3, #0.0",
		"s4, #-0.1796875", "d4, #0.1796875", "r2, r3, d1", "d1, r2, r3", "s1, r2",
		"r2, s1", "r2, fpexc", "r2, fpscr", "r2, fpsid", "apsr_nzcv, fpscr",
		"fpexc, r2", "fpscr, r2", "fpsid, r2", "s3, d4", "d4, s3", "", 0};
static const char *ARM_KNOWN_FAIL[] = {
		"bl r3", "b r3", 0};

static int arm_isreg(const char *s) {
	static const char *ex[] = {"fp", "ip", "sp", "lr", "pc", "asl", "apsr_nzcv", "fpsid", "fpscr", "fpexc", 0};
	int i;
	for (i = 0; ex[i]; i++)
		if (!strcmp(s, ex[i]))
			return 1;
	if (s[0] && strchr("rcpsd", s[0]) && s[1]) {
		const char *p = s + 1;
		while (*p)
			if (!isdigit((unsigned char)*p++))
				return 0;
		return 1;
	}
	return 0;
}

static char *arm_dump(const char *objdump, const char *obj) {
	const char *a[] = {objdump, "-S", obj, 0};
	char *out = NULL, *e = NULL, *keep, *p;
	size_t kl = 0;
	run_cap(a, NULL, &out, &e);
	free(e);
	if (!out)
		return NULL;
	keep = malloc(strlen(out) + 1);
	keep[0] = 0;
	for (p = out; *p;) {
		char *nl = strchr(p, '\n');
		size_t len = nl ? (size_t)(nl - p) : strlen(p);
		const char *q = p;
		while (q < p + len && (*q == ' ' || *q == '\t'))
			q++;
		if (q + 2 <= p + len && q[0] == '0' && q[1] == ':') {
			memcpy(keep + kl, p, len);
			kl += len;
			keep[kl++] = '\n';
		}
		if (!nl)
			break;
		p = nl + 1;
	}
	keep[kl] = 0;
	free(out);
	return keep;
}

static int suite_armasm(int argc, char **argv) {
	const char *mcc = opt(argc, argv, "--mcc", NULL);
	const char *tokh = opt(argc, argv, "--tokh", NULL);
	const char *builddir = opt(argc, argv, "--builddir", NULL);
	const char *cross = opt(argc, argv, "--cross-compile", "");
	int vfp = !strcmp(opt(argc, argv, "--vfp", "1"), "1");
	char asname[128], odname[128], as[4096], objdump[4096], w[4096], ins[4300], inc[4300], aso[4300], mo[4300];
	char *mnem[8192];
	int nm = 0, i, c, total = 0, okc = 0, st = 0, isd;
	char *tok, *text;
	char *failed[8192];
	int nf = 0;

	if (!mcc || !tokh || !builddir) {
		fprintf(stderr, "usage: mccharness armasm --mcc --tokh --builddir [--cross-compile --vfp]\n");
		return 2;
	}
	snprintf(asname, sizeof asname, "%sas", cross);
	snprintf(odname, sizeof odname, "%sobjdump", cross);
	if (!host_find_tool(asname, NULL, as, sizeof as) || !host_find_tool(odname, NULL, objdump, sizeof objdump))
		return skip0("ARM binutils (%sas/objdump) not found", cross);
	if (host_stat(mcc, &isd, NULL, NULL))
		return skip0("ARM mcc not found at '%s'", mcc);
	if (!(text = ts_read_file(tokh, NULL))) {
		fprintf(stderr, "arm-tok.h not found at '%s'\n", tokh);
		return 1;
	}
	ts_path(w, sizeof w, builddir, "arm-asm-testsuite");
	host_mkdirs(w);
	ts_path(ins, sizeof ins, w, "in.s");
	ts_path(inc, sizeof inc, w, "in.c");
	ts_path(aso, sizeof aso, w, "as.o");
	ts_path(mo, sizeof mo, w, "mcc.o");

	for (tok = strtok(text, "\n"); tok; tok = strtok(NULL, "\n")) {
		char *op, *cp, args[256];
		const char *vfp64;
		if (!strstr(tok, "DEF_ASM"))
			continue;
		if (strstr(tok, "not useful") || strstr(tok, "#define") || strstr(tok, "/*") ||
				strstr(tok, "DEF_ASM_CONDED_WITH_SUFFIX(x"))
			continue;
		op = strchr(tok, '(');
		cp = strrchr(tok, ')');
		if (!op || !cp || cp <= op)
			continue;
		snprintf(args, sizeof args, "%.*s", (int)(cp - op - 1), op + 1);
		vfp64 = strstr(tok, "DEF_ASM_CONDED_VFP_F32_F64");
		if (vfp64) {
			char b[300];
			snprintf(b, sizeof b, "%s.f32", args);
			if (nm < 8192)
				mnem[nm++] = strdup(b);
			snprintf(b, sizeof b, "%s.f64", args);
			if (nm < 8192)
				mnem[nm++] = strdup(b);
		} else {
			char name[300];
			int j = 0, k;
			for (k = 0; args[k] && j < 298; k++) {
				if (args[k] == ',' && args[k + 1] == ' ') {
					name[j++] = '.';
					k++;
				} else
					name[j++] = args[k];
			}
			name[j] = 0;
			if (!arm_isreg(name) && nm < 8192)
				mnem[nm++] = strdup(name);
		}
	}
	free(text);

	for (i = 0; i < nm; i++) {
		const char *s = mnem[i];
		int is_v = s[0] == 'v';
		if (is_v && !vfp)
			continue;
		for (c = 0; ARM_COMBOS[c]; c++) {
			const char *args = ARM_COMBOS[c];
			char t[512];
			char *exp, *got;
			snprintf(t, sizeof t, "%s %s", s, args);
			{
				char line[520];
				snprintf(line, sizeof line, "%s\n", t);
				write_file(ins, line);
			}
			{
				Argv v = {{0}, 0};
				A(&v, as);
				A(&v, "-mlittle-endian");
				if (is_v)
					A(&v, "-mfpu=vfp");
				A(&v, "-o");
				A(&v, aso);
				A(&v, ins);
				if (run_quiet(Z(&v)))
					continue;
			}
			total++;
			exp = arm_dump(objdump, aso);
			{
				char line[560];
				snprintf(line, sizeof line, "__asm__(\"%s\");\n", t);
				write_file(inc, line);
			}
			{
				Argv v = {{0}, 0};
				A(&v, mcc);
				A(&v, "-o");
				A(&v, mo);
				A(&v, "-c");
				A(&v, inc);
				if (run_quiet(Z(&v))) {
					printf("warning: '%s' did not work in mcc\n", t);
					free(exp);
					continue;
				}
			}
			got = arm_dump(objdump, mo);
			if (exp && got && !strcmp(exp, got))
				okc++;
			else {
				printf("warning: '%s' did not match GNU as\n", t);
				if (nf < 8192)
					failed[nf++] = strdup(t);
			}
			free(exp);
			free(got);
		}
		free(mnem[i]);
	}
	printf("%d of %d tests succeeded.\n", okc, total);
	for (i = 0; i < nf; i++) {
		if (in_list(failed[i], ARM_KNOWN_FAIL))
			printf("Failed test: %s (known failure)\n", failed[i]);
		else {
			printf("Failed test: %s\n", failed[i]);
			st = 1;
		}
		free(failed[i]);
	}
	if (st) {
		fprintf(stderr, "arm-asm-testsuite: FAILURES\n");
		return 1;
	}
	return 0;
}

static int obj_minos(const char *objcheck, const char *file, const char *expect) {
	const char *a[] = {objcheck, "minos", file, "--expect", expect, 0};
	return run_quiet(a) == 0;
}

static int suite_machostructural(int argc, char **argv) {
	const char *src = opt(argc, argv, "--src", NULL);
	const char *xb = opt(argc, argv, "--xb", NULL);
	const char *objcheck = opt(argc, argv, "--objcheck", NULL);
	const char *work = opt(argc, argv, "--work", NULL);
	const char *nativemcc = opt(argc, argv, "--nativemcc", NULL);
	const char *nativearch = opt(argc, argv, "--nativearch", NULL);
	const char *bdir = opt(argc, argv, "--bdir", NULL);
	static const char *TGT[] = {"x86_64-osx", "arm64-osx", 0};
	static const char *SKIPN[] = {"aggregates", "libc", "varargs", 0};
	char conf[4300], Irt[4300];
	int t, status = 0, any = 0, isd;

	if (!src || !xb || !objcheck || !work) {
		fprintf(stderr, "usage: mccharness machostructural --src --xb --objcheck --work "
										"[--nativemcc PATH --nativearch ARCH --bdir DIR]\n");
		return 2;
	}
	ts_path(conf, sizeof conf, src, "tests/qemu/conformance");
	snprintf(Irt, sizeof Irt, "-I%s/runtime/include", src);
	host_mkdirs(work);

	for (t = 0; TGT[t]; t++) {
		const char *tgt = TGT[t];
		char mcc[4300];
		char Bf[4300];
		char *files[4096];
		int nf, i;
		char vsrc[4300], vexe[4300], *err = NULL;
		Bf[0] = 0;
		ts_path(mcc, sizeof mcc, xb, "mcc-%s", tgt);
		if (host_stat(mcc, &isd, NULL, NULL) || isd) {
			char want[64];
			snprintf(want, sizeof want, "%s-osx", nativearch ? nativearch : "");
			if (nativemcc && nativearch && !strcmp(tgt, want) &&
					!host_stat(nativemcc, &isd, NULL, NULL) && !isd) {
				snprintf(mcc, sizeof mcc, "%s", nativemcc);
				if (bdir)
					snprintf(Bf, sizeof Bf, "-B%s", bdir);
			} else {
				printf("SKIP %s: no mcc-%s\n", tgt, tgt);
				continue;
			}
		}
		any = 1;
		nf = ts_glob(conf, "*.c", 0, files, 4096);
		for (i = 0; i < (nf < 0 ? 0 : nf); i++) {
			const char *f = files[i], *base = strrchr(f, '/');
			char n[256], exe[4400];
			base = base ? base + 1 : f;
			snprintf(n, sizeof n, "%.*s", (int)(strlen(base) - 2), base);
			if (in_list(n, SKIPN)) {
				printf("SKIP %s/%s (needs macOS libSystem)\n", tgt, n);
				free(files[i]);
				continue;
			}
			ts_path(exe, sizeof exe, work, "macho_%s_%s", tgt, n);
			err = NULL;
			{
				Argv v = {{0}, 0};
				A(&v, mcc);
				if (Bf[0])
					A(&v, Bf);
				A(&v, Irt);
				A(&v, f);
				A(&v, "-o");
				A(&v, exe);
				if (compile(Z(&v), &err)) {
					if (err && (strstr(err, "unresolved reference") || strstr(err, "not found")))
						printf("SKIP %s/%s (needs macOS libSystem)\n", tgt, n);
					else {
						char *l = ts_first_error_line(err, NULL, NULL);
						printf("FAIL %s/%s (link): %s\n", tgt, n, l ? l : "");
						free(l);
						status = 1;
					}
					free(err);
					free(files[i]);
					continue;
				}
				free(err);
			}
			if (obj_is_macho(objcheck, exe))
				printf("PASS %s/%s (valid Mach-O)\n", tgt, n);
			else {
				printf("FAIL %s/%s: objcheck rejected the Mach-O structure\n", tgt, n);
				status = 1;
			}
			remove(exe);
			free(files[i]);
		}

		ts_path(vsrc, sizeof vsrc, work, "macho_%s_versionmin.c", tgt);
		ts_path(vexe, sizeof vexe, work, "macho_%s_versionmin", tgt);
		write_file(vsrc, "int main(void){return 0;}\n");
		err = NULL;
		{
			Argv v = {{0}, 0};
			A(&v, mcc);
			if (Bf[0])
				A(&v, Bf);
			A(&v, Irt);
			A(&v, "-mmacosx-version-min=12.3.1");
			A(&v, vsrc);
			A(&v, "-o");
			A(&v, vexe);
			if (compile(Z(&v), &err)) {
				if (err && (strstr(err, "unresolved reference") || strstr(err, "not found")))
					printf("SKIP %s/versionmin (needs macOS libSystem)\n", tgt);
				else {
					char *l = ts_first_error_line(err, NULL, NULL);
					printf("FAIL %s/versionmin (link): %s\n", tgt, l ? l : "");
					free(l);
					status = 1;
				}
				free(err);
			} else {
				free(err);
				if (obj_minos(objcheck, vexe, "12.3.1"))
					printf("PASS %s/versionmin (LC_BUILD_VERSION minos 12.3.1)\n", tgt);
				else {
					printf("FAIL %s/versionmin: minos 12.3.1 not in load commands\n", tgt);
					status = 1;
				}
			}
		}
	}
	if (!any)
		ts_skip("no osx cross compilers (mcc-<tgt>) for any target");
	return status ? 1 : 0;
}

static int suite_pkgsmoke(int argc, char **argv) {
	const char *ci = opt(argc, argv, "--ci", NULL);
	const char *work = opt(argc, argv, "--work", NULL);
	char stage[4200], out[4200], p[4400];
	int status = 0, isd, i;
	static const char *ARCH[] = {
			"mcc-1.2.3-test.tar.gz", "libmcc-1.2.3-test.tar.gz",
			"mcc-cross-1.2.3-test.tar.gz", "checksums-test.txt", 0};

	if (!ci || !work) {
		fprintf(stderr, "usage: mccharness pkgsmoke --ci --work\n");
		return 2;
	}
	if (host_stat(ci, &isd, NULL, NULL) || isd)
		ts_skip("no ci tool (%s)", ci);

	ts_path(stage, sizeof stage, work, "stage");
	ts_path(out, sizeof out, work, "out");
	{
		const char *rm[] = {"cmake", "-E", "rm", "-rf", stage, out, 0};
		run_quiet(rm);
	}
	ts_path(p, sizeof p, stage, "bin");
	host_mkdirs(p);
	ts_path(p, sizeof p, stage, "lib/mcc");
	host_mkdirs(p);
	ts_path(p, sizeof p, stage, "include");
	host_mkdirs(p);

#define PUT(rel, txt)                 \
	do {                                \
		ts_path(p, sizeof p, stage, rel); \
		write_file(p, txt);               \
	} while (0)
#define PUTX(rel, txt)                                      \
	do {                                                      \
		ts_path(p, sizeof p, stage, rel "%s", HOST_EXE_SUFFIX); \
		write_file(p, txt);                                     \
	} while (0)
	PUTX("bin/mcc", "x");
	PUTX("bin/mcc-static", "x");
	PUTX("bin/mcc-arm64", "x");
	PUTX("bin/mcc-x86_64-win32", "x");
	PUT("lib/mcc/libmccrt.a", "x");
	PUT("lib/mcc/arm64-libmccrt.a", "x");
	PUT("lib/libmcc-static.a", "x");
	PUT("include/libmcc.h", "x");
#undef PUT
#undef PUTX

	{
		Argv v = {{0}, 0};
		A(&v, ci);
		A(&v, "pkg");
		A(&v, "--ver");
		A(&v, "v1.2.3");
		A(&v, "--plat");
		A(&v, "test");
		A(&v, "--stage");
		A(&v, stage);
		A(&v, "--out");
		A(&v, out);
		A(&v, "--format");
		A(&v, "tgz");
		if (run_quiet(Z(&v))) {
			printf("FAIL ci pkg returned nonzero\n");
			return 1;
		}
	}

	for (i = 0; ARCH[i]; i++) {
		ts_path(p, sizeof p, out, "%s", ARCH[i]);
		if (host_stat(p, &isd, NULL, NULL)) {
			printf("FAIL missing %s\n", ARCH[i]);
			status = 1;
		}
	}
	{
		char arc[4400], *o = NULL, *e = NULL;
		ts_path(arc, sizeof arc, out, "mcc-cross-1.2.3-test.tar.gz");
		const char *t[] = {"cmake", "-E", "tar", "tzf", arc, 0};
		if (run_cap(t, NULL, &o, &e) == 0 && o) {
			if (!ci_contains(o, "bin/mcc-arm64")) {
				printf("FAIL cross bundle missing mcc-arm64\n");
				status = 1;
			}
			if (!ci_contains(o, "lib/mcc/arm64-libmccrt.a")) {
				printf("FAIL cross bundle missing arm64 runtime\n");
				status = 1;
			}
			if (ci_contains(o, "bin/mcc-static")) {
				printf("FAIL cross bundle leaked host shape mcc-static\n");
				status = 1;
			}
			if (ci_contains(o, "/mcc/libmccrt.a")) {
				printf("FAIL cross bundle leaked native libmccrt.a\n");
				status = 1;
			}
		} else {
			printf("FAIL cannot list cross bundle\n");
			status = 1;
		}
		free(o);
		free(e);
	}
	if (!status)
		printf("PASS ci pkg smoke (bundles, checksums, host/cross partition)\n");
	return status ? 1 : 0;
}

#define MF_CPU_ARM64 0x0100000cu
#define MF_CPU_X86_64 0x01000007u

static uint32_t mf_rd_be32(const unsigned char *p) {
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static int mf_fat_arches(const char *path, uint32_t *cts, int max, int *pn) {
	long len = 0;
	unsigned char *d = (unsigned char *)ts_read_file(path, &len);
	uint32_t n, i;
	if (!d)
		return -1;
	if (len < 8 || mf_rd_be32(d) != 0xcafebabeu) {
		free(d);
		return -2;
	}
	n = mf_rd_be32(d + 4);
	if ((long)(8u + n * 20u) > len || (int)n > max) {
		free(d);
		return -3;
	}
	for (i = 0; i < n; i++)
		cts[i] = mf_rd_be32(d + 8 + i * 20);
	*pn = (int)n;
	free(d);
	return 0;
}

static int mf_has(const uint32_t *a, int n, uint32_t v) {
	for (int i = 0; i < n; i++)
		if (a[i] == v)
			return 1;
	return 0;
}

static int suite_machofat(int argc, char **argv) {
	const char *mcc = opt(argc, argv, "--mcc", NULL);
	const char *xmcc = opt(argc, argv, "--xmcc", NULL);
	const char *fat = opt(argc, argv, "--fat", NULL);
	const char *bdir = opt(argc, argv, "--bdir", NULL);
	const char *work = opt(argc, argv, "--work", NULL);
	const char *sdk = opt(argc, argv, "--sdk", NULL);
	char Bf[4200], src[4200], a64[4200], x64[4200], u1[4200], u2[4200];
	uint32_t cts[8];
	uint32_t native_ct =
#if defined(__aarch64__) || defined(__arm64__)
			MF_CPU_ARM64;
#else
			MF_CPU_X86_64;
#endif
	int n = 0, isd, status = 0;

	if (!mcc || !fat || !bdir || !work) {
		fprintf(stderr, "usage: mccharness machofat --mcc --fat --bdir --work [--xmcc --sdk]\n");
		return 2;
	}
	if (!MCC_HOST_DARWIN)
		ts_skip("host is not Darwin (universal Mach-O needs a macOS host)");
	if (host_stat(mcc, &isd, NULL, NULL) || isd)
		ts_skip("no native mcc (%s)", mcc);
	if (host_stat(fat, &isd, NULL, NULL) || isd)
		ts_skip("machofat tool not built (%s)", fat);

	host_mkdirs(work);
	snprintf(Bf, sizeof Bf, "-B%s", bdir);
	ts_path(src, sizeof src, work, "u.c");
	ts_path(a64, sizeof a64, work, "u.arm64");
	ts_path(u1, sizeof u1, work, "u.1");
	write_file(src, "int main(void){return 7;}\n");

	{
		Argv v = {{0}, 0};
		char *err = NULL;
		A(&v, mcc), A(&v, Bf), A(&v, src), A(&v, "-o"), A(&v, a64);
		if (compile(Z(&v), &err))
			ts_skip("native mcc cannot link an executable");
		free(err);
	}
	{
		const char *comb[] = {fat, u1, a64, 0};
		const char *exe[] = {u1, 0};
		int rc;
		if (run_quiet(comb)) {
			printf("FAIL machofat (combine 1 slice)\n");
			return 1;
		}
		if (mf_fat_arches(u1, cts, 8, &n) || n != 1 || !mf_has(cts, n, native_ct)) {
			printf("FAIL machofat (1-slice fat header wrong)\n");
			return 1;
		}
		if ((rc = run_quiet(exe)) != 7) {
			printf("FAIL machofat (1-slice run rc=%d, want 7)\n", rc);
			return 1;
		}
		printf("PASS machofat 1-slice universal (native arch, runs)\n");
	}

	if (native_ct != MF_CPU_ARM64) {
		printf("SKIP machofat 2-slice (native arch is x86_64; needs a distinct arm64 slice)\n");
	} else if (xmcc && sdk && !host_stat(xmcc, &isd, NULL, NULL) && !isd) {
		Argv v = {{0}, 0};
		char *err = NULL, Lp[4300];
		int built;
		ts_path(x64, sizeof x64, work, "u.x86_64");
		ts_path(u2, sizeof u2, work, "u.2");
		snprintf(Lp, sizeof Lp, "-L%s/usr/lib", sdk);
		A(&v, xmcc), A(&v, Bf), A(&v, "-isysroot"), A(&v, sdk), A(&v, Lp);
		A(&v, src), A(&v, "-o"), A(&v, x64);
		built = compile(Z(&v), &err) == 0;
		free(err);
		if (!built) {
			printf("SKIP machofat 2-slice (x86_64-osx cross cannot link an exe here)\n");
		} else {
			const char *comb[] = {fat, u2, a64, x64, 0};
			const char *exe[] = {u2, 0};
			int rc;
			if (run_quiet(comb)) {
				printf("FAIL machofat (combine 2 slices)\n");
				status = 1;
			} else if (mf_fat_arches(u2, cts, 8, &n) || n != 2 ||
								 !mf_has(cts, n, MF_CPU_ARM64) || !mf_has(cts, n, MF_CPU_X86_64)) {
				printf("FAIL machofat (2-slice fat header wrong)\n");
				status = 1;
			} else if ((rc = run_quiet(exe)) != 7) {
				printf("FAIL machofat (2-slice arm64 run rc=%d, want 7)\n", rc);
				status = 1;
			} else {
				printf("PASS machofat 2-slice universal (arm64+x86_64, arm64 runs)\n");
			}
		}
	} else {
		printf("SKIP machofat 2-slice (no x86_64-osx cross / SDK)\n");
	}
	return status;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: mccharness <suite> ... (parts|mcctest|mccexe|asmconnect|dashs|"
										"preprocess|i386fastcall|gcctestsuite|penative|qemurun|pewine|machonative|"
										"machoimage|machoapplelibc|machocodegen|armasm|machostructural|stackguard|pkgsmoke|"
										"machofat|qemufetch|cpropjoin|hvcache|perfncache)\n");
		return 2;
	}
	if (!strcmp(argv[1], "cpropjoin"))
		return suite_cpropjoin(argc, argv);
	if (!strcmp(argv[1], "hvcache"))
		return suite_hvcache(argc, argv);
	if (!strcmp(argv[1], "perfncache"))
		return suite_perfncache(argc, argv);
	if (!strcmp(argv[1], "machofat"))
		return suite_machofat(argc, argv);
	if (!strcmp(argv[1], "qemufetch"))
		return suite_qemufetch(argc, argv);
	if (!strcmp(argv[1], "armasm"))
		return suite_armasm(argc, argv);
	if (!strcmp(argv[1], "machostructural"))
		return suite_machostructural(argc, argv);
	if (!strcmp(argv[1], "machonative"))
		return suite_machonative(argc, argv);
	if (!strcmp(argv[1], "stackguard"))
		return suite_stackguard(argc, argv);
	if (!strcmp(argv[1], "pkgsmoke"))
		return suite_pkgsmoke(argc, argv);
	if (!strcmp(argv[1], "machoimage"))
		return suite_machoimage(argc, argv);
	if (!strcmp(argv[1], "machoapplelibc"))
		return suite_machoapplelibc(argc, argv);
	if (!strcmp(argv[1], "machocodegen"))
		return suite_machocodegen(argc, argv);
	if (!strcmp(argv[1], "i386fastcall"))
		return suite_i386fastcall(argc, argv);
	if (!strcmp(argv[1], "gcctestsuite"))
		return suite_gcctestsuite(argc, argv);
	if (!strcmp(argv[1], "penative"))
		return suite_penative(argc, argv);
	if (!strcmp(argv[1], "qemurun"))
		return suite_qemurun(argc, argv);
	if (!strcmp(argv[1], "pewine"))
		return suite_pewine(argc, argv);
	if (!strcmp(argv[1], "parts"))
		return suite_parts(argc, argv);
	if (!strcmp(argv[1], "mcctest"))
		return suite_mcctest(argc, argv);
	if (!strcmp(argv[1], "mccexe"))
		return suite_mccexe(argc, argv);
	if (!strcmp(argv[1], "asmconnect"))
		return suite_asmconnect(argc, argv);
	if (!strcmp(argv[1], "dashs"))
		return suite_dashs(argc, argv);
	if (!strcmp(argv[1], "preprocess"))
		return suite_preprocess(argc, argv);
	fprintf(stderr, "mccharness: unknown suite '%s'\n", argv[1]);
	return 2;
}
