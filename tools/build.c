#include "toolsupport.h"

static const char *CC = "cc";
static const char *OUTDIR = "build-c";

static const char *RT_OBJS[] = {
		"mccrt", "alloca", "alloca-bt", "atomic", "stdatomic", "builtin",
		"va_list", "dsohandle", "tcov", 0};

static const char *ARCH_INCS[] = {
		"-Isrc", "-Iinclude", "-Isrc/formats", "-Isrc/objfmt",
		"-Isrc/arch/i386", "-Isrc/arch/x86_64", "-Isrc/arch/arm",
		"-Isrc/arch/arm64", "-Isrc/arch/riscv64", 0};

static void arg(Argv *v, const char *s) {
	ts_arg(v, s);
}

static void args(Argv *v, const char *const *set) {
	ts_args(v, set);
}

static const char *const *argz(Argv *v) {
	return ts_argz(v);
}

static const struct
{
	const char *cpu, *define;
} CPU_DEFS[] = {
		{"x86_64", "MCC_TARGET_X86_64"}, {"amd64", "MCC_TARGET_X86_64"}, {"i386", "MCC_TARGET_I386"}, {"i686", "MCC_TARGET_I386"}, {"arm64", "MCC_TARGET_ARM64"}, {"aarch64", "MCC_TARGET_ARM64"}, {"arm", "MCC_TARGET_ARM"}, {"riscv64", "MCC_TARGET_RISCV64"}, {0, 0}};

static const char *target_define(const char *cpu) {
	int i;
	for (i = 0; CPU_DEFS[i].cpu; ++i)
		if (!strcmp(cpu, CPU_DEFS[i].cpu))
			return CPU_DEFS[i].define;
	return 0;
}

static void cpu_from_machine(const char *mach, char *out, int osz) {
	int n = 0;
	while (mach[n] && mach[n] != '-' && n < osz - 1) {
		out[n] = mach[n];
		++n;
	}
	out[n] = 0;
}

static const char *fopt(int argc, char **argv, const char *k, const char *d);

static const char *map_cpu(const char *m) {
	if (!strcmp(m, "x86") || !strcmp(m, "i86pc") || !strcmp(m, "BePC") ||
			(m[0] == 'i' && m[1] >= '3' && m[1] <= '6' && !strcmp(m + 2, "86")))
		return "i386";
	if (!strcmp(m, "x86_64") || !strcmp(m, "amd64") || !strcmp(m, "x86-64") || !strcmp(m, "AMD64"))
		return "x86_64";
	if (!strcmp(m, "aarch64") || !strcmp(m, "arm64") || !strcmp(m, "ARM64"))
		return "arm64";
	if (!strncmp(m, "arm", 3))
		return "arm";
	if (!strcmp(m, "riscv64"))
		return "riscv64";
	return 0;
}

static void detect_triplet(char *out, int osz) {
	char dm[256] = "";
	char *v = NULL, *e = NULL, *nl;
	int isd;
	char cand[2][256];
	int nc = 0, i;
	const char *a[] = {CC, "-dumpmachine", 0};
	HostSpawnOpts o;
	memset(&o, 0, sizeof o);
	o.stdout_buf = &v;
	o.stderr_buf = &e;
	out[0] = 0;
	if (host_spawn_ex(a, &o) == 0 && v) {
		snprintf(dm, sizeof dm, "%s", v);
		if ((nl = strchr(dm, '\n')))
			*nl = 0;
	}
	free(v);
	free(e);
	if (!*dm)
		return;
	snprintf(cand[nc++], sizeof cand[0], "%s", dm);

	{
		char *d1 = strchr(dm, '-');
		if (d1) {
			char *d2 = strchr(d1 + 1, '-');
			if (d2 && (!strncmp(d1 + 1, "pc-", 3) || !strncmp(d1 + 1, "unknown-", 8)))
				snprintf(cand[nc++], sizeof cand[0], "%.*s-%s", (int)(d1 - dm), dm, d2 + 1);
		}
	}
	for (i = 0; i < nc; i++) {
		char p1[512], p2[512];
		snprintf(p1, sizeof p1, "/usr/lib/%.495s/crti.o", cand[i]);
		snprintf(p2, sizeof p2, "/usr/include/%.498s", cand[i]);
		if (host_stat(p1, &isd, NULL, NULL) == 0 || (host_stat(p2, &isd, NULL, NULL) == 0 && isd)) {
			snprintf(out, osz, "%.255s", cand[i]);
			return;
		}
	}
}

static void parse_arm_abi(const char *v, char *eabi, char *vfp, char *hard, char *idiv, char *cpuver) {
	const char *p;
	*eabi = *vfp = *hard = *idiv = 0;
	cpuver[0] = 0;
	if (!v)
		return;
	if ((p = strstr(v, "__ARM_ARCH ")) || (p = strstr(v, "__ARM_ARCH\t"))) {
		int n = 0;
		p += 11;
		while (*p == ' ' || *p == '\t')
			p++;
		while (*p >= '0' && *p <= '9' && n < 7)
			cpuver[n++] = *p++;
		cpuver[n] = 0;
	}
	*eabi = strstr(v, "__ARM_EABI__") != 0;
	*vfp = (strstr(v, "__VFP_FP__") || strstr(v, "__ARM_FP ") || strstr(v, "__ARM_FP\t")) != 0;
	*hard = strstr(v, "__ARM_PCS_VFP") != 0;
	*idiv = strstr(v, "__ARM_FEATURE_IDIV") != 0;
}

static void detect_arm(char *eabi, char *vfp, char *hard, char *idiv, char *cpuver) {
	char *v = NULL, *e = NULL;
	char tmp[] = "_mcc_detect_empty.c";
	const char *a[] = {CC, "-dM", "-E", tmp, 0};
	HostSpawnOpts o;
	memset(&o, 0, sizeof o);
	o.stdout_buf = &v;
	o.stderr_buf = &e;
	{
		FILE *f = fopen(tmp, "w");
		if (f)
			fclose(f);
	}
	if (host_spawn_ex(a, &o) != 0) {
		free(v);
		v = NULL;
	}
	parse_arm_abi(v, eabi, vfp, hard, idiv, cpuver);
	free(v);
	free(e);
	remove(tmp);
}

static int arm_abi_selftest(void) {
	struct
	{
		const char *name, *dump;
		char eabi, vfp, hard, idiv;
		const char *cpuver;
	} cases[] = {
			{"armv7-hardfloat",
			 "#define __ARM_ARCH 7\n#define __ARM_EABI__ 1\n#define __ARM_FP 0xc\n"
			 "#define __ARM_PCS_VFP 1\n#define __VFP_FP__ 1\n",
			 1, 1, 1, 0, "7"},
			{"armv7-softfloat",
			 "#define __ARM_ARCH 7\n#define __ARM_EABI__ 1\n#define __VFP_FP__ 1\n", 1, 1, 0, 0, "7"},
			{"armv6-soft-nofp",
			 "#define __ARM_ARCH 6\n#define __ARM_EABI__ 1\n", 1, 0, 0, 0, "6"},
			{"armv7ve-idiv-hardfloat",
			 "#define __ARM_ARCH 7\n#define __ARM_ARCH_7VE__ 1\n#define __ARM_EABI__ 1\n"
			 "#define __ARM_FEATURE_IDIV 1\n#define __ARM_FP 0xe\n#define __ARM_PCS_VFP 1\n"
			 "#define __VFP_FP__ 1\n",
			 1, 1, 1, 1, "7"},
			{"aarch64-degrades",
			 "#define __ARM_ARCH 8\n#define __ARM_FEATURE_IDIV 1\n#define __ARM_FP 0xe\n", 0, 1, 0, 1, "8"},
			{0, 0, 0, 0, 0, 0, 0}};
	int i, fails = 0;
	for (i = 0; cases[i].name; i++) {
		char eabi, vfp, hard, idiv, cpuver[8];
		parse_arm_abi(cases[i].dump, &eabi, &vfp, &hard, &idiv, cpuver);
		if (eabi != cases[i].eabi || vfp != cases[i].vfp || hard != cases[i].hard ||
				idiv != cases[i].idiv || strcmp(cpuver, cases[i].cpuver)) {
			printf("FAIL %-22s eabi=%d/%d vfp=%d/%d hard=%d/%d idiv=%d/%d cpuver=%s/%s\n",
						 cases[i].name, eabi, cases[i].eabi, vfp, cases[i].vfp, hard, cases[i].hard,
						 idiv, cases[i].idiv, cpuver, cases[i].cpuver);
			fails++;
		} else {
			printf("ok   %-22s eabi=%d vfp=%d hard=%d idiv=%d cpuver=%s\n",
						 cases[i].name, eabi, vfp, hard, idiv, cpuver);
		}
	}
	if (fails) {
		printf("arm-abi-selftest: %d case(s) FAILED\n", fails);
		return 1;
	}
	printf("arm-abi-selftest: all %d cases faithful\n", i);
	return 0;
}

static int cmd_detect(int argc, char **argv) {
	char sysname[64], machine[64];
	const char *cpu, *os;
	char triplet[256], dm[256] = "", *v = NULL, *e = NULL, *nl;
	char eabi = 0, vfp = 0, hard = 0, idiv = 0, cpuver[8] = "", libc[16] = "";
	const char *xcpu = fopt(argc, argv, "--expect-cpu", NULL);
	const char *xos = fopt(argc, argv, "--expect-os", NULL);
	const char *xtri = fopt(argc, argv, "--expect-triplet", NULL);
	int bad = 0;

	host_sys_info(sysname, sizeof sysname, NULL, 0, machine, sizeof machine);
	cpu = map_cpu(machine);
	if (!cpu) {
		fprintf(stderr, "detect: unsupported CPU '%s'\n", machine);
		return 2;
	}
	os = sysname;
	detect_triplet(triplet, sizeof triplet);
	{
		const char *a[] = {CC, "-dumpmachine", 0};
		HostSpawnOpts o;
		memset(&o, 0, sizeof o);
		o.stdout_buf = &v;
		o.stderr_buf = &e;
		if (host_spawn_ex(a, &o) == 0 && v) {
			snprintf(dm, sizeof dm, "%s", v);
			if ((nl = strchr(dm, '\n')))
				*nl = 0;
		}
		free(v);
		free(e);
	}
	if (strstr(dm, "musl"))
		snprintf(libc, sizeof libc, "musl");
	if (!strcmp(cpu, "arm"))
		detect_arm(&eabi, &vfp, &hard, &idiv, cpuver);

	if (!xcpu && !xos && !xtri) {
		printf("MCC_CPU=%s\nMCC_OS=%s\nMCC_TRIPLET=%s\nMCC_LIBC=%s\n", cpu, os, triplet, libc);
		if (!strcmp(cpu, "arm"))
			printf("MCC_ARM_EABI=%d\nMCC_ARM_VFP=%d\nMCC_ARM_HARDFLOAT=%d\nMCC_ARM_IDIV=%d\nMCC_CPUVER=%s\n",
						 eabi, vfp, hard, idiv, cpuver);
		return 0;
	}
	if (xcpu && strcmp(xcpu, cpu)) {
		printf("MISMATCH cpu: detected %s, expected %s\n", cpu, xcpu);
		bad = 1;
	}
	if (xos && strcmp(xos, os)) {
		printf("MISMATCH os: detected %s, expected %s\n", os, xos);
		bad = 1;
	}
	if (xtri && strcmp(xtri, triplet)) {
		printf("MISMATCH triplet: detected '%s', expected '%s'\n", triplet, xtri);
		bad = 1;
	}
	if (bad)
		return 1;
	printf("detect: CPU=%s OS=%s triplet=%s matches CMake\n", cpu, os, triplet);
	return 0;
}

struct CrossTarget {
	const char *name, *cpu, *os, *cdefs, *triplet;
};

static const struct CrossTarget CROSS[] = {
		{"i386", "i386", "Linux", "MCC_TARGET_I386", "i686-linux-gnu"},
		{"x86_64", "x86_64", "Linux", "MCC_TARGET_X86_64", "x86_64-linux-gnu"},
		{"i386-win32", "i386", "WIN32", "MCC_TARGET_I386 MCC_TARGET_PE", ""},
		{"x86_64-win32", "x86_64", "WIN32", "MCC_TARGET_X86_64 MCC_TARGET_PE", ""},
		{"x86_64-osx", "x86_64", "Darwin", "MCC_TARGET_X86_64 MCC_TARGET_MACHO", ""},
		{"arm", "arm", "Linux", "MCC_TARGET_ARM MCC_ARM_VFP MCC_ARM_EABI MCC_ARM_HARDFLOAT", "arm-linux-gnueabihf"},
		{"arm64", "arm64", "Linux", "MCC_TARGET_ARM64", "aarch64-linux-gnu"},
		{"arm64-win32", "arm64", "WIN32", "MCC_TARGET_ARM64 MCC_TARGET_PE", ""},
		{"arm-wince", "arm", "WIN32", "MCC_TARGET_ARM MCC_ARM_VFP MCC_ARM_EABI MCC_ARM_HARDFLOAT MCC_TARGET_PE", ""},
		{"riscv64", "riscv64", "Linux", "MCC_TARGET_RISCV64", "riscv64-linux-gnu"},
		{"arm64-osx", "arm64", "Darwin", "MCC_TARGET_ARM64 MCC_TARGET_MACHO", ""},
		{0, 0, 0, 0, 0}};

static int mccrt_objs(const char *cpu, const char *os, const char **out, int max) {
	int n = 0;
	int win = !strcmp(os, "WIN32"), osx = !strcmp(os, "Darwin");
	static const char *COMMON[] = {"stdatomic", "atomic", "builtin", "alloca", "alloca-bt", "complex", 0};
	int i;
#define ADD(x)        \
	do {                \
		if (n < max)      \
			out[n++] = (x); \
	} while (0)
	if (!strcmp(cpu, "i386") || !strcmp(cpu, "x86_64"))
		ADD("mccrt");
	else if (!strcmp(cpu, "arm")) {
		ADD("mccrt");
		ADD("armeabi");
		ADD("armflush");
	} else {
		ADD("float128");
	}
	for (i = 0; COMMON[i]; i++)
		ADD(COMMON[i]);
	if (win) {
		ADD("crt1");
		ADD("crt1w");
		ADD("wincrt1");
		ADD("wincrt1w");
		ADD("dllcrt1");
		ADD("dllmain");
		ADD("winex");
		if (!strcmp(cpu, "i386") || !strcmp(cpu, "x86_64") || !strcmp(cpu, "arm64"))
			ADD("chkstk");
	} else if (osx) {
		if (!strcmp(cpu, "x86_64"))
			ADD("va_list");
	} else {
		if (!strcmp(cpu, "i386"))
			ADD("pic86");
		else if (!strcmp(cpu, "x86_64"))
			ADD("va_list");
		else if (!strcmp(cpu, "arm64"))
			ADD("armflush");
		else if (!strcmp(cpu, "riscv64"))
			ADD("lib-riscv");
		ADD("dsohandle");
	}
#undef ADD
	return n;
}

static void obj_src(const char *obj, char *buf, int bsz) {
	if (!strcmp(obj, "crt1w"))
		snprintf(buf, bsz, "runtime/win32/lib/crt1.c");
	else if (!strcmp(obj, "wincrt1w"))
		snprintf(buf, bsz, "runtime/win32/lib/wincrt1.c");
	else if (!strcmp(obj, "crt1") || !strcmp(obj, "wincrt1") || !strcmp(obj, "dllcrt1") ||
					 !strcmp(obj, "dllmain") || !strcmp(obj, "winex") || !strcmp(obj, "chkstk"))
		snprintf(buf, bsz, "runtime/win32/lib/%s.c", obj);
	else
		snprintf(buf, bsz, "runtime/lib/%s.c", obj);
}

static int ensure_mccdefs(const char *out) {
	char hdr[4096], c2s[4096], src[4096];
	int isd;
	ts_path(hdr, sizeof hdr, out, "mccdefs_.h");
	if (host_stat(hdr, &isd, NULL, NULL) == 0)
		return 0;
	ts_path(c2s, sizeof c2s, out, "c2str_host");
	{
		const char *a[] = {CC, "-O2", "-o", c2s, "tools/c2str.c", 0};
		if (ts_run(a))
			return 1;
	}
	snprintf(src, sizeof src, "runtime/include/mccdefs.h");
	{
		const char *a[] = {c2s, src, hdr, 0};
		if (ts_run(a))
			return 1;
	}
	return 0;
}

static int cmd_cross(const char *name, const char *out) {
	const struct CrossTarget *t = 0;
	char mccpath[4096], archive[4096], barg[4096], Iout[4096];
	const char *objs[64];
	int nobj, i, win, osx;
	Argv v;

	for (i = 0; CROSS[i].name; ++i)
		if (!strcmp(CROSS[i].name, name)) {
			t = &CROSS[i];
			break;
		}
	if (!t) {
		fprintf(stderr, "build: unknown cross target '%s'\n", name);
		return 2;
	}
	win = !strcmp(t->os, "WIN32");
	osx = !strcmp(t->os, "Darwin");
	(void)osx;
	if (host_mkdirs(out))
		return 1;
	if (ensure_mccdefs(out))
		return 1;
	snprintf(Iout, sizeof Iout, "-I%s", out);
	ts_path(mccpath, sizeof mccpath, out, "mcc-%s", name);
	ts_path(archive, sizeof archive, out, "%s-libmccrt.a", name);

	printf("[cross %s] cc src/mcc.c -> mcc-%s\n", name, name);
	v.n = 0;
	arg(&v, CC);
	arg(&v, "-O2");
	arg(&v, "-DMCC_AMALGAMATED=1");
	{
		static char cdefs[512];
		snprintf(cdefs, sizeof cdefs, "%s", t->cdefs);
		char *tok, *sv;
		for (tok = strtok_r(cdefs, " ", &sv); tok; tok = strtok_r(NULL, " ", &sv)) {
			static char db[16][128];
			static int di;
			snprintf(db[di], sizeof db[0], "-D%s=1", tok);
			arg(&v, db[di]);
			di = (di + 1) & 15;
		}
	}
	{
		static char d[6][256];
		int k = 0;
		snprintf(d[k], sizeof d[0], "-DMCC_VERSION=20260706135200");
		arg(&v, d[k++]);

		snprintf(d[k], sizeof d[0], "-DMCC_CONFIG_PREDEFS=1");
		arg(&v, d[k++]);
		snprintf(d[k], sizeof d[0], "-DMCC_CONFIG_CROSSPREFIX=\"%s-\"", name);
		arg(&v, d[k++]);
		if (win) {
			snprintf(d[k], sizeof d[0], "-DMCC_CONFIG_MCCDIR=\"%s/win32\"", out);
			arg(&v, d[k++]);
		}
	}
	if (*t->triplet) {
		static char p[256];
		snprintf(p, sizeof p, "-DMCC_CONFIG_SYSROOT=\"/usr/%s\"", t->triplet);
		arg(&v, p);
	}
	arg(&v, Iout);
	args(&v, ARCH_INCS);
	arg(&v, "-o");
	arg(&v, mccpath);
	arg(&v, "src/mcc.c");
	if (MCC_HOST_POSIX) {
		arg(&v, "-lm");
		arg(&v, "-ldl");
		arg(&v, "-lpthread");
	}
	if (ts_run(argz(&v)))
		return 1;

	printf("[cross %s] mcc-%s -> %s-libmccrt.a\n", name, name, name);
	nobj = mccrt_objs(t->cpu, t->os, objs, 64);
	snprintf(barg, sizeof barg, "-B%s", win ? "runtime/win32" : "runtime");
	{
		static char objpaths[64][4096], srcs[64][4096];
		Argv ar;
		ar.n = 0;
		arg(&ar, mccpath);
		arg(&ar, "-ar");
		arg(&ar, "rcs");
		arg(&ar, archive);
		for (i = 0; i < nobj; ++i) {
			char Iout[4096];
			obj_src(objs[i], srcs[i], sizeof srcs[i]);
			ts_path(objpaths[i], sizeof objpaths[i], out, "%s-%s.o", name, objs[i]);
			snprintf(Iout, sizeof Iout, "-I%s", out);
			v.n = 0;
			arg(&v, mccpath);
			arg(&v, barg);
			if (win)
				arg(&v, "-Iruntime/include");
			else
				arg(&v, "-fPIC");
			arg(&v, Iout);
			args(&v, ARCH_INCS);
			arg(&v, "-c");
			arg(&v, srcs[i]);
			arg(&v, "-o");
			arg(&v, objpaths[i]);
			if (ts_run(argz(&v)))
				return 1;
			arg(&ar, objpaths[i]);
		}
		if (ts_run(argz(&ar)))
			return 1;
	}
	printf("[cross %s] done -- %s + %s\n", name, mccpath, archive);
	return 0;
}

static int truthy(const char *v) {
	return v && *v && strcmp(v, "OFF") && strcmp(v, "0") && strcmp(v, "FALSE") && strcmp(v, "NO");
}

static const char *fopt(int argc, char **argv, const char *k, const char *d) {
	int i;
	for (i = 2; i < argc - 1; i++)
		if (!strcmp(argv[i], k))
			return argv[i + 1];
	return d;
}

static int defcmp(const void *a, const void *b) {
	return strcmp(*(char *const *)a, *(char *const *)b);
}

static int cmd_emit_defines(int argc, char **argv) {
	static char L[128][512];
	int n = 0;
	const char *cpu = fopt(argc, argv, "--cpu", "x86_64");
	const char *os = fopt(argc, argv, "--targetos", "");
	const char *libc = fopt(argc, argv, "--libc", "");
	const char *td = target_define(cpu);
	const char *ref = fopt(argc, argv, "--check", NULL);
	int i;
	if (!td) {
		fprintf(stderr, "emit-defines: unknown cpu '%s'\n", cpu);
		return 2;
	}

#define EMIT(...) snprintf(L[n++], sizeof L[0], __VA_ARGS__)
	EMIT("%s=1", td);
	if (!strcmp(cpu, "arm")) {
		const char *cv = fopt(argc, argv, "--cpuver", "");
		if (*cv)
			EMIT("MCC_CONFIG_CPUVER=%s", cv);
		if (truthy(fopt(argc, argv, "--arm-eabi", "")))
			EMIT("MCC_ARM_EABI=1");
		if (truthy(fopt(argc, argv, "--arm-vfp", "")))
			EMIT("MCC_ARM_VFP=1");
		if (truthy(fopt(argc, argv, "--arm-hardfloat", "")))
			EMIT("MCC_ARM_HARDFLOAT=1");
		if (truthy(fopt(argc, argv, "--arm-idiv", "")))
			EMIT("__ARM_FEATURE_IDIV=1");
	}
	if (!strcmp(os, "WIN32"))
		EMIT("MCC_TARGET_PE=1");
	else if (!strcmp(os, "Darwin"))
		EMIT("MCC_TARGET_MACHO=1");
	else if (!strcmp(os, "Android"))
		EMIT("MCC_TARGETOS_ANDROID=1");
	else if (!strcmp(os, "FreeBSD") || !strcmp(os, "OpenBSD") || !strcmp(os, "NetBSD") || !strcmp(os, "DragonFly"))
		EMIT("TARGETOS_%s=1", os);
	if (!strcmp(libc, "musl"))
		EMIT("MCC_CONFIG_MUSL=1");
	else if (!strcmp(libc, "uClibc"))
		EMIT("MCC_CONFIG_UCLIBC=1");
	if (truthy(fopt(argc, argv, "--run-mmap-exec", "")))
		EMIT("MCC_CONFIG_RUN_DUALMAP=1");
	if (truthy(fopt(argc, argv, "--pie", "")))
		EMIT("MCC_CONFIG_PIE=1");
	if (truthy(fopt(argc, argv, "--pic", "")))
		EMIT("MCC_CONFIG_PIC=1");
	if (truthy(fopt(argc, argv, "--new-dtags", "")))
		EMIT("MCC_CONFIG_NEW_DTAGS=1");
	if (truthy(fopt(argc, argv, "--codesign", "")))
		EMIT("MCC_CONFIG_CODESIGN=1");
	if (truthy(fopt(argc, argv, "--cst", "")))
		EMIT("MCC_CONFIG_LSP=1");
	if (truthy(fopt(argc, argv, "--ast", "")))
		EMIT("MCC_CONFIG_OPTIMIZER=1");
	if (truthy(fopt(argc, argv, "--embed-jit", "")))
		EMIT("MCC_EMBED_JIT=1");
	if (truthy(fopt(argc, argv, "--ast-shadow", "")))
		EMIT("MCC_CONFIG_AST_SHADOW=1");
	if (truthy(fopt(argc, argv, "--trace", "")))
		EMIT("MCC_CONFIG_TRACE=1");
	{
		const char *dr = fopt(argc, argv, "--diag-rt", "bounds");
		EMIT("MCC_CONFIG_DIAG_RT=%d",
				 !strcmp(dr, "off") ? 0 : !strcmp(dr, "backtrace") ? 1 : 2);
	}
	if (!truthy(fopt(argc, argv, "--asm", "1")))
		EMIT("MCC_CONFIG_ASM=0");
	if (!strcmp(fopt(argc, argv, "--new-macho", ""), "no"))
		EMIT("MCC_CONFIG_MACHO_CHAINED_FIXUPS=0");
	{
		const char *dw = fopt(argc, argv, "--dwarf", "");
		if (*dw)
			EMIT("MCC_CONFIG_DWARF_VERSION=%s", dw);
	}
	{
		const char *sl = fopt(argc, argv, "--semlock", "");
		if (*sl)
			EMIT("MCC_CONFIG_SEMLOCK=%s", sl);
	}
	{
		struct
		{
			const char *flag, *name;
		} sd[] = {
				{"--triplet", "MCC_CONFIG_TRIPLET"}, {"--os-release", "MCC_CONFIG_OS_RELEASE"}};
		unsigned k;
		for (k = 0; k < sizeof sd / sizeof sd[0]; k++) {
			const char *v = fopt(argc, argv, sd[k].flag, "");
			if (*v)
				EMIT("%s=\"%s\"", sd[k].name, v);
		}
	}
	{
		const char *sr = fopt(argc, argv, "--sysroot", "");
		if (*sr)
			EMIT("MCC_CONFIG_SYSROOT=\"%s\"", sr);
	}
	if (strcmp(os, "WIN32")) {
		const char *md = fopt(argc, argv, "--mccdir", "");
		if (*md)
			EMIT("MCC_CONFIG_MCCDIR=\"%s\"", md);
	}
#undef EMIT

	if (!ref) {
		for (i = 0; i < n; i++)
			printf("%s\n", L[i]);
		return 0;
	}

	{
		char *text = ts_read_file(ref, NULL), *p;
		char cmk[128][512];
		int cn = 0, drift = 0, j;
		char *mine[128];
		if (!text) {
			fprintf(stderr, "emit-defines: cannot read %s\n", ref);
			return 2;
		}
		for (p = text; *p && cn < 128;) {
			char *e = strchr(p, '\n');
			int len = e ? (int)(e - p) : (int)strlen(p);
			while (len && (p[len - 1] == '\r' || p[len - 1] == ' '))
				--len;
			if (len) {
				snprintf(cmk[cn], sizeof cmk[0], "%.*s", len, p);
				cn++;
			}
			if (!e)
				break;
			p = e + 1;
		}
		free(text);
		for (i = 0; i < n; i++)
			mine[i] = L[i];
		qsort(mine, n, sizeof mine[0], defcmp);
		{
			char *cp[128];
			for (j = 0; j < cn; j++)
				cp[j] = cmk[j];
			qsort(cp, cn, sizeof cp[0], defcmp);
			for (i = 0; i < n; i++) {
				int f = 0;
				for (j = 0; j < cn; j++)
					if (!strcmp(mine[i], cp[j])) {
						f = 1;
						break;
					}
				if (!f) {
					printf("DRIFT: mccbuild emits '%s' but CMake _mccdefs does not\n", mine[i]);
					drift = 1;
				}
			}
			for (j = 0; j < cn; j++) {
				int f = 0;
				for (i = 0; i < n; i++)
					if (!strcmp(cp[j], mine[i])) {
						f = 1;
						break;
					}
				if (!f) {
					printf("DRIFT: CMake _mccdefs has '%s' but mccbuild does not\n", cp[j]);
					drift = 1;
				}
			}
		}
		if (drift) {
			printf("mccbuild --emit-defines diverged from CMake's _mccdefs (catalog #8)\n");
			return 1;
		}
		printf("mccbuild --emit-defines matches CMake _mccdefs (%d defines)\n", n);
		return 0;
	}
}

int main(int argc, char **argv) {
	int do_run = 0, i;
	char osrel[128];
	if (argc > 1 && !strcmp(argv[1], "--emit-defines"))
		return cmd_emit_defines(argc, argv);
	if (argc > 1 && !strcmp(argv[1], "--detect")) {
		const char *ecc = getenv("CC");
		if (ecc && *ecc)
			CC = ecc;
		for (i = 1; i < argc; i++)
			if (!strcmp(argv[i], "--arm-selftest"))
				return arm_abi_selftest();
		for (i = 1; i < argc - 1; i++)
			if (!strcmp(argv[i], "--cc"))
				CC = argv[i + 1];
		return cmd_detect(argc, argv);
	}

	{
		const char *xout = "build-cross", *xtgt = 0;
		int factory = 0;
		const char *ecc = getenv("CC");
		if (ecc && *ecc)
			CC = ecc;
		for (i = 1; i < argc; i++) {
			if (!strcmp(argv[i], "--cc") && i + 1 < argc)
				CC = argv[i + 1];
			else if (!strcmp(argv[i], "--out") && i + 1 < argc)
				xout = argv[i + 1];
			else if (!strcmp(argv[i], "--cross") && i + 1 < argc)
				xtgt = argv[i + 1];
			else if (!strcmp(argv[i], "--factory"))
				factory = 1;
		}
		if (factory) {
			int k, rc = 0;
			for (k = 0; CROSS[k].name; ++k)
				if ((rc = cmd_cross(CROSS[k].name, xout)))
					return rc;
			printf("\nbuild: cross factory complete -- 11 targets in %s\n", xout);
			return 0;
		}
		if (xtgt)
			return cmd_cross(xtgt, xout);
	}
	char defs[9][512], mccpath[4096], target[64] = "";
	const char *tdef;
	Argv v;
	const char *envcc = getenv("CC");

	if (envcc && *envcc)
		CC = envcc;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--cc") && i + 1 < argc)
			CC = argv[++i];
		else if (!strcmp(argv[i], "--out") && i + 1 < argc)
			OUTDIR = argv[++i];
		else if (!strcmp(argv[i], "--target") && i + 1 < argc)
			snprintf(target, sizeof target, "%s", argv[++i]);
		else if (!strcmp(argv[i], "--run"))
			do_run = 1;
		else if (!strcmp(argv[i], "--githash")) {
			char stamp[256];
			if (ts_git_stamp(stamp, sizeof stamp) == 0) {
				printf("%s\n", stamp);
				return 0;
			}
			fprintf(stderr, "build: not a git repo\n");
			return 1;
		} else {
			fprintf(stderr, "usage: build [--cc <cc>] [--out <dir>] [--target <cpu>] [--run] [--githash]\n");
			return 2;
		}
	}

	host_sys_info(NULL, 0, osrel, sizeof osrel, NULL, 0);
	{
		char mach[128] = "?", stamp[256] = "";
		ts_cc_probe(CC, mach, sizeof mach, NULL, 0);
		if (!*target)
			cpu_from_machine(mach, target, sizeof target);
		ts_git_stamp(stamp, sizeof stamp);
		printf("build: cc=%s out=%s target=%s host=%s (%s) %s\n",
					 CC, OUTDIR, target, mach, osrel, stamp);
	}
	if (!(tdef = target_define(target))) {
		fprintf(stderr, "build: unsupported target cpu '%s'\n", target);
		return 2;
	}

	{
		char hostmach[128] = "?", hostcpu[64];
		ts_cc_probe(CC, hostmach, sizeof hostmach, NULL, 0);
		cpu_from_machine(hostmach, hostcpu, sizeof hostcpu);
		if (target_define(hostcpu) != tdef) {
			fprintf(stderr, "build: --target %s != host %s; cross runtime staging needs the CMake cross factory\n",
							target, hostcpu);
			return 2;
		}
	}

	{
		char inc[4096];
		ts_path(inc, sizeof inc, OUTDIR, "include");
		if (host_mkdirs(inc)) {
			fprintf(stderr, "build: mkdir %s failed\n", inc);
			return 1;
		}
	}

	printf("[1/3] cc mcc.c -> mcc\n");
	snprintf(defs[0], sizeof defs[0], "-D%s=1", tdef);
	snprintf(defs[1], sizeof defs[1], "-DMCC_CONFIG_PREDEFS=0");
	snprintf(defs[2], sizeof defs[2], "-DMCC_CONFIG_DIAG_RT=2");
	snprintf(defs[3], sizeof defs[3], "-DMCC_CONFIG_PREDEFS=0");
	snprintf(defs[4], sizeof defs[4], "-DMCC_VERSION=20260706135200");
	snprintf(defs[5], sizeof defs[5], "-DMCC_CONFIG_MCCDIR=\"%s\"", OUTDIR);

	snprintf(defs[6], sizeof defs[6], "-DMCC_CONFIG_LIBPATHS=\"{B}:/usr/lib64:/usr/lib:/lib\"");
	snprintf(defs[7], sizeof defs[7], "-DMCC_CONFIG_CRTPREFIX=\"/usr/lib64:/usr/lib:/lib\"");
	snprintf(defs[8], sizeof defs[8], "-DMCC_CONFIG_OS_RELEASE=\"%s\"", osrel);
	ts_path(mccpath, sizeof mccpath, OUTDIR, "mcc");

	v.n = 0;
	arg(&v, CC);
	arg(&v, "-O2");
	arg(&v, "-DMCC_AMALGAMATED=1");
	for (i = 0; i < 9; ++i)
		arg(&v, defs[i]);
	args(&v, ARCH_INCS);
	arg(&v, "-o");
	arg(&v, mccpath);
	arg(&v, "src/mcc.c");
	if (MCC_HOST_POSIX) {
		arg(&v, "-lm");
		arg(&v, "-ldl");
		arg(&v, "-lpthread");
	}
	if (ts_run(argz(&v)))
		return 1;

	printf("[2/3] install runtime headers\n");
	{
		char *hdrs[128];
		int nh = ts_glob("runtime/include", "*.h", 0, hdrs, 128);
		for (i = 0; i < nh; ++i) {
			char dst[4096];
			const char *b = strrchr(hdrs[i], '/');
			b = b ? b + 1 : hdrs[i];
			ts_path(dst, sizeof dst, OUTDIR, "include/%s", b);
			if (host_copy_file(hdrs[i], dst, 0)) {
				fprintf(stderr, "build: copy %s failed\n", hdrs[i]);
				return 1;
			}
			free(hdrs[i]);
		}
	}

	printf("[3/3] mcc -> libmccrt.a\n");
	{
		char barg[4096], objpaths[9][4096], srcpath[4096];
		Argv ar;
		snprintf(barg, sizeof barg, "-B%s", OUTDIR);
		ar.n = 0;
		arg(&ar, mccpath);
		arg(&ar, "-ar");
		{
			static char lib[4096];
			ts_path(lib, sizeof lib, OUTDIR, "libmccrt.a");
			arg(&ar, lib);
		}
		for (i = 0; RT_OBJS[i]; ++i) {
			ts_path(objpaths[i], sizeof objpaths[i], OUTDIR, "%s.o", RT_OBJS[i]);
			snprintf(srcpath, sizeof srcpath, "runtime/lib/%s.c", RT_OBJS[i]);
			v.n = 0;
			arg(&v, mccpath);
			arg(&v, barg);
			arg(&v, "-c");
			arg(&v, srcpath);
			arg(&v, "-o");
			arg(&v, objpaths[i]);
			if (ts_run(argz(&v)))
				return 1;
			arg(&ar, objpaths[i]);
		}
		if (ts_run(argz(&ar)))
			return 1;
	}

	printf("      + bcheck/backtrace runtime objects\n");
	{
		char barg[4096], bco[4096];
		snprintf(barg, sizeof barg, "-B%s", OUTDIR);
		ts_path(bco, sizeof bco, OUTDIR, "bcheck.o");
		v.n = 0;
		arg(&v, mccpath);
		arg(&v, barg);
		arg(&v, "-c");
		arg(&v, "runtime/lib/bcheck.c");
		arg(&v, "-o");
		arg(&v, bco);
		arg(&v, "-bt");
		arg(&v, "-Iruntime/include");
		args(&v, ARCH_INCS);
		if (ts_run(argz(&v)))
			return 1;

		{
			static const char *bt[] = {"bt-exe", "bt-log", "runmain", 0};
			char src[3][4096], obj[3][4096];
			for (i = 0; bt[i]; ++i) {
				snprintf(src[i], sizeof src[i], "runtime/lib/%s.c", bt[i]);
				ts_path(obj[i], sizeof obj[i], OUTDIR, "%s.o", bt[i]);
				v.n = 0;
				arg(&v, mccpath);
				arg(&v, barg);
				arg(&v, "-c");
				arg(&v, src[i]);
				arg(&v, "-o");
				arg(&v, obj[i]);
				arg(&v, "-Iruntime/include");
				args(&v, ARCH_INCS);
				if (ts_run(argz(&v)))
					return 1;
			}
		}
	}

	printf("\nbuild: done -- %s/mcc (use: %s/mcc -B%s <file.c>)\n", OUTDIR, OUTDIR, OUTDIR);

	if (do_run) {
		char barg[4096], hello[4096], helloexe[4096];
		FILE *h;
		printf("\n[smoke] compile + run a hello program\n");
		snprintf(barg, sizeof barg, "-B%s", OUTDIR);
		ts_path(hello, sizeof hello, OUTDIR, "_bld_hello.c");
		ts_path(helloexe, sizeof helloexe, OUTDIR, "_bld_hello");
		h = fopen(hello, "w");
		if (!h) {
			fprintf(stderr, "build: cannot write %s\n", hello);
			return 1;
		}
		fputs("#include <stdio.h>\nint main(){printf(\"build.c ok: %d\\n\",6*7);return 0;}\n", h);
		fclose(h);
		v.n = 0;
		arg(&v, mccpath);
		arg(&v, barg);
		arg(&v, hello);
		arg(&v, "-o");
		arg(&v, helloexe);
		if (ts_run(argz(&v)))
			return 1;
		{
			const char *rv[] = {helloexe, 0};
			if (ts_run(rv))
				return 1;
		}
	}
	return 0;
}
