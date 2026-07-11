#ifndef MCC_AMALGAMATED
#define MCC_AMALGAMATED 1
#endif

#include "mcc.h"
#if MCC_AMALGAMATED
#include "libmcc.c"
#endif
#include "mcctools.c"

static const char help[] =
		"Usage: mcc [options] file...\n"
		"       mcc [options] -run file [args...]\n"
		"Options:\n"
		"  -c                  Only compile and assemble, do not link\n"
		"  -S                  Only compile; emit assembly (.s), do not assemble or link\n"
		"  -E                  Only run the preprocessor\n"
		"  -o <file>           Write output to <file>\n"
		"  -run                Compile, then run the program (passing it [args...])\n"
		"  -x <type>           Set the type of the following files: c, asm, bin, none\n"
		"  -                   Read the program source from standard input\n"
		"  @<file>             Read further command-line options from <file>\n"
		"Preprocessor options:\n"
		"  -I <dir>            Add <dir> to the include search path\n"
		"  -D <macro>[=<val>]  Define <macro> to <val> (or 1 if <val> omitted)\n"
		"  -U <macro>          Undefine <macro>\n"
		"  -include <file>     Include <file> before parsing the rest of the input\n"
		"  -isystem <dir>      Add <dir> to the system include search path\n"
		"  --sysroot <dir>     Use <dir> as the root of the system search paths\n"
		"  -nostdinc           Do not search the standard system include directories\n"
		"  -M, -MM             Print make dependencies; -MM omits system headers\n"
		"  -MD, -MMD           Like -M / -MM, but also compile the input\n"
		"  -MF <file>          Write the generated dependencies to <file>\n"
		"  -MP                 Emit a phony target for each dependency\n"
		"Linker options:\n"
		"  -l <lib>            Link against library <lib>\n"
		"  -L <dir>            Add <dir> to the library search path\n"
		"  -nostdlib           Do not use the standard startup files or libraries\n"
		"  -shared             Create a shared library\n"
		"  -static             Link against static libraries\n"
		"  -pie, -no-pie       Force a position-independent (or plain) executable\n"
		"  -r                  Produce a relocatable object as the output\n"
		"  -rdynamic           Export all global symbols to the dynamic symbol table\n"
		"  -soname <name>      Set the DT_SONAME of the output shared library\n"
		"  -Wl,<arg>           Pass the comma-separated <arg> to the linker\n"
		"Debug options:\n"
		"  -g                  Generate debug information (stabs)\n"
		"  -gdwarf[-<n>]       Generate debug information in DWARF format\n"
#ifdef MCC_TARGET_PE
		"  -g.pdb              Generate a .pdb debug database\n"
#endif
#if MCC_CONFIG_DIAG_RT >= 2
		"  -b                  Enable the built-in memory and bounds checker (implies -g)\n"
#endif
#if MCC_CONFIG_DIAG_RT >= 1
		"  -bt[<n>]            Link with backtrace support (show up to <n> callers)\n"
#endif
#if defined(MCC_TARGET_X86_64) && !defined(MCC_TARGET_PE)
		"  -fsanitize=undefined  Trap on undefined behavior (signed overflow, out-of-range\n"
		"                        shift, divide-by-zero) with an illegal instruction\n"
#endif
#if MCC_CONFIG_DIAG_RT >= 2
		"  -fsanitize=address    Detect out-of-bounds/invalid memory access (via -b)\n"
#endif
		"Other options:\n"
		"  -f[no-]<flag>       Enable (or disable) a compiler flag; see -hh\n"
		"  -W[no-]<warn>       Enable (or disable) a warning; see -hh\n"
		"  -w                  Suppress all warnings\n"
		"  -std=<std>          Language standard to compile for (e.g. c11, gnu11)\n"
		"  -pthread            Support POSIX threads (-D_REENTRANT and -lpthread)\n"
		"  -B <dir>            Set mcc's private include/library directory to <dir>\n"
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
		"  -m32, -m64          Hand off to the i386 or x86_64 cross compiler\n"
#endif
		"  -v, --version       Display the version of the compiler\n"
		"  -vv                 Show the search paths and loaded files\n"
		"  -bench              Display compilation statistics\n"
		"  -h, --help          Display this help (-hh lists more options)\n"
		"Tools:\n"
		"  -ar [crstvx] <lib> [files...]   Create a static library\n"
#ifdef MCC_TARGET_PE
		"  -impdef <lib>.dll [-o <def>]    Create an import .def file\n"
#endif
		"For bug reporting instructions, see:\n"
		"  https://github.com/lucianlightgray/moderncc\n";

static const char help2[] =
		"More options:\n"
		"  -P, -P1                       With -E: suppress or use alternative #line output\n"
		"  -dD, -dM                      With -E: output #define directives\n"
		"  -Wp,<arg>                     Pass the comma-separated <arg> to the preprocessor\n"
		"  -O<n>                         Optimize: 1 = AST replay + const-fold, 2/s = + register promotion, 3 = + inlining;\n"
		"                                n>=4 = spend n seconds searching pass configs for the smallest object\n"
		"  --embed-jit, --no-embed-jit   Embed the always-on runtime self-optimizing JIT in the output (default on)\n"
		"  --jit-max-duration <sec>      Runtime JIT budget baked into the output (default 600; 0 = unlimited)\n"
		"  --jit-functions <syms>        Comma list of functions to JIT-optimize (default main; sites at their common ancestor)\n"
		"  --clear-cache                 Remove the per-user optimizer cache directory and exit\n"
		"  -pthread                      Support POSIX threads (-D_REENTRANT and -lpthread)\n"
		"  -include <file>               Include <file> before parsing each input file\n"
		"  -isystem <dir>                Add <dir> to the system include search path\n"
		"  -nostdlib                     Do not use the standard startup files or libraries\n"
		"  -static                       Link against static libraries\n"
		"  -dumpversion                  Display the version of the compiler\n"
		"  -dumpmachine                  Display the compiler's target machine\n"
		"  -print-search-dirs            Display the compiler's search directories\n"
		"  -rstdin <file>                With -run: use <file> as the program's stdin\n"
		"  -dt                           With -run / -E: auto-define test_... macros\n"
		"  --debug=<cat>[,<cat>...]      Enable internal trace categories:\n"
		"                                reloc inc pp struct tok pe ver asm sym\n"
		"  -d<num>                       Same, as a raw bitmask (bits in the order above)\n"
		"  -s                            Strip the symbol table from linked output\n"
		"  --lsp                         Record the concrete syntax tree of each compiled file\n"
		"  -pedantic                     Diagnose uses of non-ISO C extensions\n"
		"  -pedantic-errors              Make the -pedantic diagnostics hard errors\n"
		"Ignored options:\n"
		"  -arch -C --param -pipe -traditional\n"
		"Warnings (-W[no-]...):\n"
		"  all                           Enable a set of common warnings (marked *)\n"
		"  pedantic                      Same as -pedantic (use -Wno-pedantic to disable)\n"
		"  error[=<warn>]                Treat warnings as errors (all, or the named one)\n"
		"  write-strings                 Make string literals const\n"
		"  unsupported                   Warn about ignored options, pragmas, etc.\n"
		"  implicit-function-declaration Diagnose calls to undeclared functions (*, error by default)\n"
		"  return-type                   Diagnose return-value/type mismatches (*, error by default)\n"
		"  discarded-qualifiers          Warn when a type qualifier is discarded (*)\n"
		"Flags (-f[no-]...):\n"
		"  unsigned-char                 Make plain 'char' unsigned\n"
		"  signed-char                   Make plain 'char' signed\n"
		"  common                        Place uninitialized globals in the common section\n"
		"  leading-underscore            Prefix external symbols with an underscore\n"
		"  ms-extensions                 Allow anonymous struct/union members (MSVC)\n"
		"  dollars-in-identifiers        Allow '$' in identifiers\n"
		"  reverse-funcargs              Evaluate function arguments right to left\n"
		"  gnu89-inline                  Treat 'extern inline' as 'static inline' (gnu89)\n"
		"  asynchronous-unwind-tables    Emit an .eh_frame unwind section [on]\n"
		"  test-coverage                 Instrument the generated code for coverage\n"
		"  short-enums                   Use the smallest underlying type for enums\n"
		"  visibility=<v>                Default symbol visibility (default/hidden/...)\n"
		"  stack-protector[-all]         Emit stack canaries (x86_64 ELF/Mach-O, arm64 Mach-O)\n"
		"  fold-math                     Fold constant-arg libm calls (sin/cos/tan/exp/log/\n"
		"                                pow/sinh/...) and constant-arg mcc_fc_* time-series\n"
		"                                forecasts to a deterministic value; opt-in\n"
		"  [no-]pic, [no-]PIC, [no-]pie, [no-]PIE   Position-independent codegen\n"
		"  wrapv, builtin, omit-frame-pointer, (function|data)-sections   (accepted)\n"
		"Target options (-m...):\n"
		"  ms-bitfields                  Use the MSVC bitfield layout\n"
		"  arch=, tune=, cpu=, cmodel=   CPU selection (accepted; fixed codegen)\n"
#ifdef MCC_TARGET_ARM
		"  float-abi                     Select the ARM float ABI (hard / softfp)\n"
#endif
#ifdef MCC_TARGET_X86_64
		"  no-sse                        Do not use SSE registers on x86_64\n"
#endif
		"Linker options (-Wl,...):\n"
		"  -nostdlib                     Do not search the standard library paths\n"
		"  -[no-]whole-archive           Load whole libraries / only as needed\n"
		"  -export-all-symbols           Same as -rdynamic\n"
		"  -export-dynamic               Same as -rdynamic\n"
		"  -image-base=, -Ttext=         Set the base address of the executable\n"
		"  -section-alignment=           Set the section alignment of the executable\n"
#ifdef MCC_TARGET_PE
		"  -file-alignment=              Set the PE file alignment\n"
		"  -stack=                       Set the PE stack reserve size\n"
		"  -large-address-aware          Set the related PE option\n"
		"  -subsystem=<sub>              Set the PE subsystem (console / windows)\n"
		"  -oformat=<fmt>                Set the output format (pe-* or binary)\n"
		"Predefined macros:\n"
		"  mcc -E -dM - < nul\n"
#else
		"  -rpath=                       Set the dynamic library search path (DT_RPATH)\n"
		"  -enable-new-dtags             Use DT_RUNPATH instead of DT_RPATH\n"
		"  -soname=                      Set the DT_SONAME ELF tag\n"
#if defined(MCC_TARGET_MACHO)
		"  -install_name=                Set the install name (macOS soname alias)\n"
		"  -mmacosx-version-min=a.b.c    Set the LC_BUILD_VERSION minos/sdk (default 10.6)\n"
#else
		"  -dynamic-linker=<path>        Set the ELF interpreter to <path>\n"
#endif
		"  -Bsymbolic                    Set the DT_SYMBOLIC ELF tag\n"
		"  -oformat=<fmt>                Set the output format (elf32/64-* or binary)\n"
		"  -init=, -fini=, -Map=, -as-needed, -O, -z= Accepted and ignored\n"
		"Predefined macros:\n"
		"  mcc -E -dM - < /dev/null\n"
#endif
		;

static const char version[] =
		"mcc version " MCC_VERSION_STR
#ifdef MCC_GITHASH
		" " MCC_GITHASH
#endif
		" ("
#ifdef MCC_TARGET_I386
		"i386"
#elif defined MCC_TARGET_X86_64
		"x86_64"
#elif defined MCC_TARGET_ARM
		"ARM"
#ifdef MCC_ARM_EABI
		" eabi"
#ifdef MCC_ARM_HARDFLOAT
		"hf"
#endif
#endif
#elif defined MCC_TARGET_ARM64
		"AArch64"
#elif defined MCC_TARGET_RISCV64
		"riscv64"
#endif
#ifdef MCC_TARGET_PE
		" Windows"
#elif defined(MCC_TARGET_MACHO)
		" Darwin"
#elif MCC_TARGETOS_FreeBSD || MCC_TARGETOS_FreeBSD_kernel
		" FreeBSD"
#elif MCC_TARGETOS_OpenBSD
		" OpenBSD"
#elif MCC_TARGETOS_NetBSD
		" NetBSD"
#else
		" Linux"
#endif
		")\n";

static void print_dirs(const char *msg, char **paths, int nb_paths) {
	printf("%s:\n%s", msg, nb_paths ? "" : "  -\n");
	for (int i = 0; i < nb_paths; i++)
		printf("  %s\n", paths[i]);
}

static void print_search_dirs(MCCState *s) {
	printf("install: %s\n", s->mcc_lib_path);
	print_dirs("include", s->sysinclude_paths, s->nb_sysinclude_paths);
	print_dirs("libraries", s->library_paths, s->nb_library_paths);
	printf("mccrt:\n  %s/%s\n", s->library_paths[0], MCC_CONFIG_CROSSPREFIX MCC_MCCRT);
#ifdef MCC_TARGET_UNIX
	print_dirs("crt", s->crt_paths, s->nb_crt_paths);
	printf("elfinterp:\n  %s\n", s->elfint);
#endif
}

static void set_environment(MCCState *s) {
	char *path;

	path = getenv("C_INCLUDE_PATH");
	if (path != NULL) {
		mcc_add_sysinclude_path(s, path);
	}
	path = getenv("CPATH");
	if (path != NULL) {
		mcc_add_include_path(s, path);
	}
	path = getenv("LIBRARY_PATH");
	if (path != NULL) {
		mcc_add_library_path(s, path);
	}
#ifdef MCC_TARGET_MACHO
	path = getenv("DYLD_FRAMEWORK_PATH");
	if (path != NULL) {
		mcc_add_framework_path(s, path);
	}
#endif
}

static char *default_outputfile(MCCState *s, const char *first_file) {
	char buf[1024];
	char *ext;
	const char *name = "a";

	if (first_file && strcmp(first_file, "-"))
		name = mcc_basename(first_file);
	if (strlen(name) + 4 >= sizeof buf)
		name = "a";
	strcpy(buf, name);
	ext = mcc_fileextension(buf);
	if ((s->just_deps || s->output_type == MCC_OUTPUT_OBJ) && !s->option_r && *ext)
		strcpy(ext, ".o");
	else if (s->output_type == MCC_OUTPUT_ASM && *ext)
		strcpy(ext, ".s");
#ifdef MCC_TARGET_PE
	else if (s->output_type == MCC_OUTPUT_DLL)
		strcpy(ext, ".dll");
	else if (s->output_type == MCC_OUTPUT_EXE)
		strcpy(ext, ".exe");
#endif
	else
		strcpy(buf, "a.out");
	return mcc_strdup(buf);
}

#if MCC_HOST_POSIX
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <errno.h>

static pid_t so_fork(void) {
	int tries;
	for (tries = 0; tries < 64; tries++) {
		pid_t pid = fork();
		if (pid >= 0)
			return pid;
		if (errno != EAGAIN && errno != EINTR)
			break;
		usleep(2000);
	}
	return -1;
}

static volatile sig_atomic_t so_stop;
static void so_on_stop(int sig) {
	(void)sig;
	so_stop = 1;
}

static int so_jitscore;
static long so_last_rss;
static const char **so_run_cv;
static int so_jit_env(void) {
	const char *e = getenv("MCC_AST_JITSCORE");
	return e && e[0] && e[0] != '0';
}

#define SO_INLINE_LIMIT_MAX 160
#define SO_CKPT_FMT 6u
#define SO_GATE_SPACE (16u * (SO_INLINE_LIMIT_MAX + 1))
#define SO_BUDGET_SPACE 144u
#define SO_LIMIT_SPACE 5u
#define SO_SLICE_FACTOR 8u
#define SO_CLAIM_CHUNK 64u

typedef struct {
	uint32_t fmt;
	uint32_t best_gate;
	uint32_t best_budget;
	uint32_t best_limit;
	uint32_t claim_gate;
	uint32_t budget_cursor;
	uint32_t limit_cursor;
	uint32_t round;
	int64_t best_text;
	uint64_t key;
} SoCkpt;

static uint64_t so_fnv(uint64_t h, const void *p, size_t n) {
	const unsigned char *b = (const unsigned char *)p;
	for (size_t i = 0; i < n; i++) {
		h ^= b[i];
		h *= 0x100000001b3u;
	}
	return h;
}

static uint64_t so_key(MCCState *s) {
	uint64_t h = 0xcbf29ce484222325u;
	if (s->nb_files >= 1 && s->files[0]->name) {
		FILE *f = host_fopen(s->files[0]->name, "rb");
		if (f) {
			char buf[65536];
			size_t r;
			while ((r = fread(buf, 1, sizeof buf, f)) > 0)
				h = so_fnv(h, buf, r);
			fclose(f);
		}
	}
#ifdef MCC_CONFIG_TRIPLET
	h = so_fnv(h, MCC_CONFIG_TRIPLET, strlen(MCC_CONFIG_TRIPLET));
#endif
	if (so_jitscore) {
		static const char tag[] = "jitscore";
		h = so_fnv(h, tag, sizeof tag - 1);
	}
	return h;
}

static int so_ckpt_path(char *buf, int cap, uint64_t key) {
	char dir[3072];
	if (host_cache_dir(dir, sizeof dir) != 0)
		return -1;
	snprintf(buf, cap, "%s/so-%016" PRIx64 ".ck", dir, key);
	return 0;
}

static int so_ckpt_read(const char *path, uint64_t key, SoCkpt *c) {
	FILE *f = host_fopen(path, "rb");
	SoCkpt t;
	if (!f)
		return -1;
	if (fread(&t, sizeof t, 1, f) != 1 || t.fmt != SO_CKPT_FMT || t.key != key) {
		fclose(f);
		return -1;
	}
	fclose(f);
	*c = t;
	return 0;
}

static void so_ckpt_write(const char *path, const SoCkpt *nw) {
	char lockp[1300], tmpp[1300];
	int lockfd, fd;
	SoCkpt out = *nw, b;
	FILE *f;
	snprintf(lockp, sizeof lockp, "%s.lock", path);
	lockfd = open(lockp, O_CREAT | O_RDWR, 0644);
	if (lockfd >= 0)
		flock(lockfd, LOCK_EX);
	if ((f = host_fopen(path, "rb"))) {
		if (fread(&b, sizeof b, 1, f) == 1 && b.fmt == nw->fmt && b.key == nw->key) {
			if (b.best_text >= 0 && (out.best_text < 0 || b.best_text < out.best_text)) {
				out.best_text = b.best_text;
				out.best_gate = b.best_gate;
				out.best_budget = b.best_budget;
				out.best_limit = b.best_limit;
			}
			if (b.claim_gate > out.claim_gate)
				out.claim_gate = b.claim_gate;
			if (b.budget_cursor > out.budget_cursor)
				out.budget_cursor = b.budget_cursor;
			if (b.limit_cursor > out.limit_cursor)
				out.limit_cursor = b.limit_cursor;
			if (b.round > out.round)
				out.round = b.round;
		}
		fclose(f);
	}
	snprintf(tmpp, sizeof tmpp, "%s.tmp", path);
	if ((f = host_fopen(tmpp, "wb"))) {
		fwrite(&out, sizeof out, 1, f);
		fflush(f);
		if ((fd = fileno(f)) >= 0)
			fsync(fd);
		fclose(f);
		rename(tmpp, path);
	}
	if (lockfd >= 0) {
		flock(lockfd, LOCK_UN);
		close(lockfd);
	}
}

static unsigned so_claim(const char *path, uint64_t key, SoCkpt *shared) {
	char lockp[1300], tmpp[1300];
	int lockfd, fd;
	unsigned start;
	SoCkpt c;
	FILE *f;
	memset(&c, 0, sizeof c);
	c.fmt = SO_CKPT_FMT;
	c.key = key;
	c.best_text = -1;
	snprintf(lockp, sizeof lockp, "%s.lock", path);
	lockfd = open(lockp, O_CREAT | O_RDWR, 0644);
	if (lockfd >= 0)
		flock(lockfd, LOCK_EX);
	if ((f = host_fopen(path, "rb"))) {
		SoCkpt b;
		if (fread(&b, sizeof b, 1, f) == 1 && b.fmt == SO_CKPT_FMT && b.key == key)
			c = b;
		fclose(f);
	}
	start = c.claim_gate;
	if (start < SO_GATE_SPACE) {
		c.claim_gate = start + SO_CLAIM_CHUNK;
		if (c.claim_gate > SO_GATE_SPACE)
			c.claim_gate = SO_GATE_SPACE;
		snprintf(tmpp, sizeof tmpp, "%s.tmp", path);
		if ((f = host_fopen(tmpp, "wb"))) {
			fwrite(&c, sizeof c, 1, f);
			fflush(f);
			if ((fd = fileno(f)) >= 0)
				fsync(fd);
			fclose(f);
			rename(tmpp, path);
		}
	}
	if (lockfd >= 0) {
		flock(lockfd, LOCK_UN);
		close(lockfd);
	}
	*shared = c;
	return start;
}

#define SO_NNODE 3
#define SO_NGRAFT 3
#define SO_NBF 4
static const int so_nodes[SO_NNODE] = {64, 128, 256};
static const int so_graft[SO_NGRAFT] = {2048, 4096, 8192};
static const int so_bf[SO_NBF] = {0, 3, 5, 9};

static const int so_limits[SO_LIMIT_SPACE] = {-1, 64, 16, 4, 1};

static int so_gate_dead(unsigned gate) {
	return !((gate >> 2) & 1) && (gate >> 4) != 0;
}

static void so_setenv_cfg(unsigned gate, unsigned budget, unsigned limit_lvl) {
	char buf[32];
	unsigned limit = gate >> 4;
	int inl = (gate >> 2) & 1;
	int nsel = (int)(budget % SO_NNODE);
	int gsel = (int)((budget / SO_NNODE) % SO_NGRAFT);
	int bfsel = (int)((budget / (SO_NNODE * SO_NGRAFT)) % SO_NBF);
	int cpsel = (int)((budget / (SO_NNODE * SO_NGRAFT * SO_NBF)) % 2);
	int csesel = (int)((budget / (SO_NNODE * SO_NGRAFT * SO_NBF * 2)) % 2);
	int lv = so_limits[limit_lvl % SO_LIMIT_SPACE];
	setenv("MCC_SEARCH_WORKER", "1", 1);
	setenv("MCC_AST_TEMPLATES", (gate & 1) ? "1" : "0", 1);
	setenv("MCC_AST_PROMOTE", (gate >> 1) & 1 ? "1" : "0", 1);
	setenv("MCC_AST_INLINE", inl ? "1" : "0", 1);
	setenv("MCC_AST_NO_CALLFUL", (gate >> 3) & 1 ? "1" : "0", 1);
	snprintf(buf, sizeof buf, "%u", inl ? limit : 0u);
	setenv("MCC_AST_INLINE_LIMIT", buf, 1);
	snprintf(buf, sizeof buf, "%d", so_nodes[nsel]);
	setenv("MCC_AST_INLINE_NODES", buf, 1);
	snprintf(buf, sizeof buf, "%d", so_graft[gsel]);
	setenv("MCC_AST_GRAFT", buf, 1);
	snprintf(buf, sizeof buf, "%d", so_bf[bfsel]);
	setenv("MCC_AST_BITFLAG", buf, 1);
	setenv("MCC_AST_CPROP_JOIN", cpsel ? "1" : "0", 1);
	setenv("MCC_AST_CSE_JOIN", csesel ? "1" : "0", 1);
	snprintf(buf, sizeof buf, "%d", lv);
	setenv("MCC_AST_PROMOTE_LIMIT", buf, 1);
	setenv("MCC_AST_OPT_LIMIT", buf, 1);
}

static long so_filesize(const char *p) {
	struct stat st;
	return stat(p, &st) == 0 ? (long)st.st_size : -1;
}

static long so_textsize(const char *p) {
	FILE *f = host_fopen(p, "rb");
	unsigned char h[64];
	long total = 0;
	uint64_t shoff, flags, size;
	uint16_t shentsize, shnum, i;
	if (!f)
		return -1;
	if (fread(h, 1, sizeof h, f) < 64 || h[0] != 0x7f || h[1] != 'E' ||
			h[2] != 'L' || h[3] != 'F' || h[4] != 2) {
		fclose(f);
		return so_filesize(p);
	}
	memcpy(&shoff, h + 40, 8);
	memcpy(&shentsize, h + 58, 2);
	memcpy(&shnum, h + 60, 2);
	for (i = 0; i < shnum; i++) {
		unsigned char sh[64];
		if (fseek(f, (long)(shoff + (uint64_t)i * shentsize), SEEK_SET) != 0)
			break;
		if (fread(sh, 1, sizeof sh, f) < 40)
			break;
		memcpy(&flags, sh + 8, 8);
		memcpy(&size, sh + 32, 8);
		if (flags & 0x4u)
			total += (long)size;
	}
	fclose(f);
	return total > 0 ? total : so_filesize(p);
}

struct so_fn {
	char name[80];
	long size;
	unsigned cfg;
};

static int so_fn_sizes(const char *path, struct so_fn *out, int max) {
	FILE *f = host_fopen(path, "rb");
	unsigned char h[64], sh[64];
	uint64_t shoff, symoff = 0, symsz = 0, syment = 0, stroff = 0;
	uint16_t shentsize, shnum, i;
	uint32_t type, link = 0;
	long o;
	int n = 0, strtab_idx = -1;
	if (!f)
		return -1;
	if (fread(h, 1, 64, f) < 64 || h[0] != 0x7f || h[4] != 2) {
		fclose(f);
		return -1;
	}
	memcpy(&shoff, h + 40, 8);
	memcpy(&shentsize, h + 58, 2);
	memcpy(&shnum, h + 60, 2);
	for (i = 0; i < shnum; i++) {
		if (fseek(f, (long)(shoff + (uint64_t)i * shentsize), SEEK_SET) != 0 ||
				fread(sh, 1, 64, f) < 64)
			break;
		memcpy(&type, sh + 4, 4);
		if (type == 2) {
			memcpy(&symoff, sh + 24, 8);
			memcpy(&symsz, sh + 32, 8);
			memcpy(&link, sh + 40, 4);
			memcpy(&syment, sh + 56, 8);
			strtab_idx = (int)link;
		}
	}
	if (strtab_idx < 0 || syment == 0) {
		fclose(f);
		return -1;
	}
	if (fseek(f, (long)(shoff + (uint64_t)strtab_idx * shentsize), SEEK_SET) == 0 &&
			fread(sh, 1, 64, f) == 64)
		memcpy(&stroff, sh + 24, 8);
	for (o = 0; (uint64_t)o < symsz && n < max; o += (long)syment) {
		unsigned char sym[24];
		uint32_t stname;
		uint64_t stsize;
		char nm[80];
		int k = 0, c;
		if (fseek(f, (long)symoff + o, SEEK_SET) != 0 || fread(sym, 1, 24, f) < 24)
			break;
		memcpy(&stname, sym, 4);
		memcpy(&stsize, sym + 16, 8);
		if ((sym[4] & 0xf) != 2 || stsize == 0)
			continue;
		if (fseek(f, (long)stroff + stname, SEEK_SET) != 0)
			continue;
		while (k < 79 && (c = fgetc(f)) > 0)
			nm[k++] = (char)c;
		nm[k] = 0;
		if (!nm[0])
			continue;
		snprintf(out[n].name, sizeof out[n].name, "%s", nm);
		out[n].size = (long)stsize;
		out[n].cfg = 0;
		n++;
	}
	fclose(f);
	return n;
}

static int so_copy(const char *src, const char *dst) {
	FILE *in, *out;
	char buf[8192];
	size_t n;
	int ok = 0;
	if (!(in = host_fopen(src, "rb")))
		return -1;
	if ((out = host_fopen(dst, "wb"))) {
		ok = 1;
		while ((n = fread(buf, 1, sizeof buf, in)) > 0)
			if (fwrite(buf, 1, n, out) != n) {
				ok = 0;
				break;
			}
		fclose(out);
	}
	fclose(in);
	if (ok) {
		struct stat st;
		if (stat(src, &st) == 0)
			chmod(dst, st.st_mode & 07777);
	}
	return ok ? 0 : -1;
}

static int so_spawn_timeout(const char **cv, unsigned timeout_ms) {
	unsigned t0;
	pid_t pid = so_fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		execvp(cv[0], (char *const *)cv);
		_exit(127);
	}
	t0 = host_clock_ms();
	for (;;) {
		int status;
		pid_t r = waitpid(pid, &status, WNOHANG);
		if (r == pid)
			return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
		if (r < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (so_stop || host_clock_ms() - t0 >= timeout_ms) {
			kill(pid, SIGKILL);
			waitpid(pid, &status, 0);
			return -1;
		}
		usleep(1000);
	}
}

static int so_spawn_must(const char **cv, unsigned timeout_ms, int tries) {
	int rc = -1, k;
	for (k = 0; k < tries && !so_stop; k++) {
		rc = so_spawn_timeout(cv, timeout_ms);
		if (rc >= 0)
			return rc;
		usleep((unsigned)(k + 1) * 5000u);
	}
	return rc;
}

static int so_spawn_run(const char **cv, unsigned timeout_ms, long *usec,
												long *rss_kb) {
	struct timeval t0, t1;
	struct rusage ru;
	pid_t pid = so_fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		int nul = open("/dev/null", O_RDWR);
		if (nul >= 0) {
			dup2(nul, 1);
			dup2(nul, 2);
			if (nul > 2)
				close(nul);
		}
		execvp(cv[0], (char *const *)cv);
		_exit(127);
	}
	gettimeofday(&t0, NULL);
	{
		unsigned tstart = host_clock_ms();
		for (;;) {
			int status;
			pid_t r = wait4(pid, &status, WNOHANG, &ru);
			if (r == pid) {
				int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
				gettimeofday(&t1, NULL);
				if (rc != 0)
					return -1;
				*usec = (long)(t1.tv_sec - t0.tv_sec) * 1000000L +
								(long)(t1.tv_usec - t0.tv_usec);
				*rss_kb = (long)ru.ru_maxrss;
#if MCC_HOST_DARWIN
				*rss_kb /= 1024;
#endif
				return 0;
			}
			if (r < 0) {
				if (errno == EINTR)
					continue;
				return -1;
			}
			if (so_stop || host_clock_ms() - tstart >= timeout_ms) {
				kill(pid, SIGKILL);
				wait4(pid, &status, 0, &ru);
				return -1;
			}
			usleep(1000);
		}
	}
}

static long so_run_score(unsigned timeout_ms) {
	long best = -1, rss_best = -1;
	int k;
	for (k = 0; k < 3 && !so_stop; k++) {
		long usec = -1, rss = -1;
		if (so_spawn_run(so_run_cv, timeout_ms, &usec, &rss) != 0)
			return -1;
		if (best < 0 || usec < best)
			best = usec;
		if (rss_best < 0 || rss < rss_best)
			rss_best = rss;
	}
	so_last_rss = rss_best;
	return best;
}

static long so_eval(const char **cv, const char *cand_tmp, unsigned gate,
										unsigned budget, unsigned limit_lvl,
										unsigned timeout_ms) {
	so_setenv_cfg(gate, budget, limit_lvl);
	if (so_spawn_timeout(cv, timeout_ms) != 0)
		return -1;
	if (so_jitscore && so_run_cv) {
		long sc = so_run_score(timeout_ms);
		if (sc >= 0)
			return sc;
	}
	return so_textsize(cand_tmp);
}

static void so_ckpt_save(const char *ckpt, uint64_t key, unsigned best_gate,
												 unsigned best_budget, unsigned best_limit,
												 unsigned budget_cur, unsigned limit_cur,
												 unsigned round, long best) {
	SoCkpt ck;
	memset(&ck, 0, sizeof ck);
	ck.fmt = SO_CKPT_FMT;
	ck.key = key;
	ck.best_gate = best_gate;
	ck.best_budget = best_budget;
	ck.best_limit = best_limit;
	ck.claim_gate = 0;
	ck.budget_cursor = budget_cur;
	ck.limit_cursor = limit_cur;
	ck.round = round;
	ck.best_text = best;
	so_ckpt_write(ckpt, &ck);
}

#define SO_MAXFN 400
static long so_fn_find(struct so_fn *a, int n, const char *name) {
	int i;
	for (i = 0; i < n; i++)
		if (!strcmp(a[i].name, name))
			return a[i].size;
	return -1;
}

#define SO_PF_FMT 1u

typedef struct {
	uint32_t fmt;
	uint32_t best_cfg;
	uint64_t key;
	int64_t best_size;
	uint32_t tried;
	uint32_t pad;
} SoPfCkpt;

static uint64_t so_pf_key(uint64_t fnhash) {
	uint64_t h = so_fnv(0xcbf29ce484222325u, &fnhash, sizeof fnhash);
	h = so_fnv(h, MCC_VERSION_STR, strlen(MCC_VERSION_STR));
#ifdef MCC_CONFIG_TRIPLET
	h = so_fnv(h, MCC_CONFIG_TRIPLET, strlen(MCC_CONFIG_TRIPLET));
#endif
	return h;
}

static int so_pf_path(char *buf, int cap, uint64_t key) {
	char dir[3072];
	if (host_cache_dir(dir, sizeof dir) != 0)
		return -1;
	snprintf(buf, cap, "%s/pf-%016" PRIx64 ".ck", dir, key);
	return 0;
}

static int so_pf_read(uint64_t key, SoPfCkpt *c) {
	char path[3200];
	FILE *f;
	SoPfCkpt t;
	if (so_pf_path(path, sizeof path, key) != 0)
		return -1;
	if (!(f = host_fopen(path, "rb")))
		return -1;
	if (fread(&t, sizeof t, 1, f) != 1 || t.fmt != SO_PF_FMT || t.key != key) {
		fclose(f);
		return -1;
	}
	fclose(f);
	*c = t;
	return 0;
}

static void so_pf_write(uint64_t key, const SoPfCkpt *nw) {
	char path[3200], lockp[3300], tmpp[3300];
	int lockfd, fd;
	SoPfCkpt out = *nw, b;
	FILE *f;
	if (so_pf_path(path, sizeof path, key) != 0)
		return;
	snprintf(lockp, sizeof lockp, "%s.lock", path);
	lockfd = open(lockp, O_CREAT | O_RDWR, 0644);
	if (lockfd >= 0)
		flock(lockfd, LOCK_EX);
	if ((f = host_fopen(path, "rb"))) {
		if (fread(&b, sizeof b, 1, f) == 1 && b.fmt == SO_PF_FMT &&
				b.key == key) {
			if (b.best_size >= 0 &&
					(out.best_size < 0 || b.best_size < out.best_size)) {
				out.best_size = b.best_size;
				out.best_cfg = b.best_cfg;
			}
			out.tried |= b.tried;
		}
		fclose(f);
	}
	snprintf(tmpp, sizeof tmpp, "%s.tmp", path);
	if ((f = host_fopen(tmpp, "wb"))) {
		fwrite(&out, sizeof out, 1, f);
		fflush(f);
		if ((fd = fileno(f)) >= 0)
			fsync(fd);
		fclose(f);
		rename(tmpp, path);
	}
	if (lockfd >= 0) {
		flock(lockfd, LOCK_UN);
		close(lockfd);
	}
}

static int so_fn_hashes(const char *path, struct so_fn *fns, int nf,
												uint64_t *fnh) {
	FILE *f = host_fopen(path, "rb");
	char nm[80];
	unsigned long long h;
	int got = 0, i;
	for (i = 0; i < nf; i++)
		fnh[i] = 0;
	if (!f)
		return 0;
	while (fscanf(f, "%79s %llx", nm, &h) == 2)
		for (i = 0; i < nf; i++) {
			const char *sn = fns[i].name;
			if (!strcmp(sn, nm) || (sn[0] == '_' && !strcmp(sn + 1, nm))) {
				if (!fnh[i])
					got++;
				fnh[i] = h;
				break;
			}
		}
	fclose(f);
	return got;
}

static int mcc_superopt_perfn(int argc, char **argv, MCCState *s,
															const char *outfile) {
	unsigned budget_ms = s->optimize_search_seconds * 1000u;
	unsigned start = host_clock_ms();
	char exe[1024], cand[1200], hashp[1300], *cfg;
	const char **cv;
	int i, argn, nf, fi, ci, p, cached = 0;
	struct so_fn fns[SO_MAXFN], cur[SO_MAXFN];
	unsigned best_cfg[SO_MAXFN], tried[SO_MAXFN];
	uint64_t fnh[SO_MAXFN];
	long best_size[SO_MAXFN], sz;
	static const unsigned cfgs[3] = {1, 3, 7};
	if (host_exe_path(exe, sizeof exe) <= 0)
		pstrcpy(exe, sizeof exe, argv[0]);
	snprintf(cand, sizeof cand, "%s.mcc-pf", outfile);
	snprintf(hashp, sizeof hashp, "%s.fnh", cand);
	cfg = mcc_malloc(SO_MAXFN * 96);
	cv = mcc_malloc((argc + 4) * sizeof *cv);
	argn = 0;
	cv[argn++] = exe;
	for (i = 1; i < argc; i++)
		cv[argn++] = argv[i];
	cv[argn++] = "-o";
	cv[argn++] = cand;
	cv[argn] = NULL;
	so_stop = 0;
	signal(SIGTERM, so_on_stop);
	signal(SIGINT, so_on_stop);
	signal(SIGHUP, so_on_stop);
	setenv("MCC_SEARCH_WORKER", "1", 1);
	setenv("MCC_AST_TEMPLATES", "1", 1);
	setenv("MCC_AST_FN_CONFIG", "", 1);
	remove(hashp);
	setenv("MCC_AST_HASH_OUT", hashp, 1);
	if (so_spawn_must(cv, 300000u, 4) != 0 ||
			(nf = so_fn_sizes(cand, fns, SO_MAXFN)) <= 0) {
		unsetenv("MCC_AST_HASH_OUT");
		remove(hashp);
		remove(cand);
		mcc_free(cfg);
		mcc_free(cv);
		return -1;
	}
	unsetenv("MCC_AST_HASH_OUT");
	for (fi = 0; fi < nf; fi++) {
		int mx = fi, j;
		for (j = fi + 1; j < nf; j++)
			if (fns[j].size > fns[mx].size)
				mx = j;
		if (mx != fi) {
			struct so_fn t = fns[fi];
			fns[fi] = fns[mx];
			fns[mx] = t;
		}
	}
	so_fn_hashes(hashp, fns, nf, fnh);
	remove(hashp);
	for (fi = 0; fi < nf; fi++) {
		best_cfg[fi] = 7;
		best_size[fi] = fns[fi].size;
		tried[fi] = 4;
		if (fnh[fi]) {
			SoPfCkpt c;
			if (so_pf_read(so_pf_key(fnh[fi]), &c) == 0) {
				tried[fi] |= c.tried & 7u;
				if (c.best_size > 0 && c.best_size <= best_size[fi] &&
						(c.best_cfg == 1 || c.best_cfg == 3 || c.best_cfg == 7)) {
					best_cfg[fi] = c.best_cfg;
					best_size[fi] = c.best_size;
				}
				if (tried[fi] == 7)
					cached++;
			}
		}
	}
	for (fi = 0; fi < nf && !so_stop && host_clock_ms() - start < budget_ms; fi++)
		for (ci = 0; ci < 3; ci++) {
			int m, j;
			if (((tried[fi] >> ci) & 1) ||
					host_clock_ms() - start >= budget_ms || so_stop)
				continue;
			for (p = 0, j = 0; j < nf; j++)
				p += snprintf(cfg + p, SO_MAXFN * 96 - p, "%s=%u;", fns[j].name,
											j == fi ? cfgs[ci] : best_cfg[j]);
			setenv("MCC_AST_FN_CONFIG", cfg, 1);
			if (so_spawn_timeout(cv, 300000u) != 0)
				continue;
			m = so_fn_sizes(cand, cur, SO_MAXFN);
			sz = so_fn_find(cur, m, fns[fi].name);
			if (sz <= 0)
				continue;
			tried[fi] |= 1u << ci;
			if (sz < best_size[fi]) {
				best_size[fi] = sz;
				best_cfg[fi] = cfgs[ci];
			}
			if (fnh[fi]) {
				SoPfCkpt c;
				memset(&c, 0, sizeof c);
				c.fmt = SO_PF_FMT;
				c.key = so_pf_key(fnh[fi]);
				c.best_cfg = best_cfg[fi];
				c.best_size = best_size[fi];
				c.tried = tried[fi];
				so_pf_write(c.key, &c);
			}
		}
	for (p = 0, fi = 0; fi < nf; fi++)
		p += snprintf(cfg + p, SO_MAXFN * 96 - p, "%s=%u;", fns[fi].name,
									best_cfg[fi]);
	setenv("MCC_AST_FN_CONFIG", cfg, 1);
	if (so_spawn_must(cv, 300000u, 4) != 0) {
		remove(cand);
		mcc_free(cfg);
		mcc_free(cv);
		return -1;
	}
	i = so_copy(cand, outfile);
	remove(cand);
	if (s->verbose) {
		long tot = 0;
		for (fi = 0; fi < nf; fi++)
			tot += best_size[fi];
		printf("superopt-perfn: %d functions (%d cached) in %ums, total .text %ld\n",
					 nf, cached, host_clock_ms() - start, tot);
	}
	mcc_free(cfg);
	mcc_free(cv);
	return i;
}

static int mcc_superopt_search(int argc, char **argv, MCCState *s,
															 const char *outfile) {
	unsigned budget_ms = s->optimize_search_seconds * 1000u;
	unsigned start = host_clock_ms();
	unsigned best_gate = 0, best_budget = 0, best_limit = 0;
	unsigned local_claim = 0, budget_cur = 0, limit_cur = 0, round = 0, tried = 0;
	unsigned base_ms, cap_ms;
	long best;
	char exe[1024], cand_tmp[1200], ckpt[3200];
	const char **cv, **rv = NULL;
	const char *src = s->nb_files >= 1 ? s->files[0]->name : NULL;
	int i, argn, have_ckpt, links_exe = src != NULL;
	uint64_t key;
	SoCkpt ck;
	for (i = 1; i < argc; i++)
		if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "-S") ||
				!strcmp(argv[i], "-E") || !strcmp(argv[i], "-r") ||
				!strcmp(argv[i], "-shared"))
			links_exe = 0;
	so_jitscore = so_jit_env() && links_exe;
	key = so_key(s);
	if (host_exe_path(exe, sizeof exe) <= 0)
		pstrcpy(exe, sizeof exe, argv[0]);
	snprintf(cand_tmp, sizeof cand_tmp, "%s.mcc-so-cand", outfile);
	have_ckpt = so_ckpt_path(ckpt, sizeof ckpt, key) == 0;
	if (have_ckpt && so_ckpt_read(ckpt, key, &ck) == 0) {
		best_gate = ck.best_gate;
		best_budget = ck.best_budget;
		best_limit = ck.best_limit;
		local_claim = ck.claim_gate;
		budget_cur = ck.budget_cursor;
		limit_cur = ck.limit_cursor;
		round = ck.round;
	}
	cv = mcc_malloc((argc + 4) * sizeof *cv);
	argn = 0;
	cv[argn++] = exe;
	for (i = 1; i < argc; i++)
		cv[argn++] = argv[i];
	cv[argn++] = "-o";
	cv[argn++] = cand_tmp;
	cv[argn] = NULL;

	if (so_jitscore) {
		int rn = 0;
		rv = mcc_malloc((argc + 4) * sizeof *rv);
		rv[rn++] = exe;
		for (i = 1; i < argc; i++) {
			if (!strcmp(argv[i], "-o")) {
				i++;
				continue;
			}
			if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "-S") ||
					!strcmp(argv[i], src))
				continue;
			rv[rn++] = argv[i];
		}
		rv[rn++] = "-run";
		rv[rn++] = src;
		rv[rn] = NULL;
		so_run_cv = rv;
	}

	so_stop = 0;
	signal(SIGTERM, so_on_stop);
	signal(SIGINT, so_on_stop);
	signal(SIGHUP, so_on_stop);

	{
		unsigned t0 = host_clock_ms(), dt;
		best = so_eval(cv, cand_tmp, best_gate, best_budget, best_limit, 300000u);
		dt = host_clock_ms() - t0;
		base_ms = (dt ? dt : 1u) * SO_SLICE_FACTOR;
		cap_ms = dt * 4u < 2000u ? 2000u : dt * 4u;
		tried++;
		if (best < 0) {
			remove(cand_tmp);
			mcc_free(cv);
			mcc_free(rv);
			so_run_cv = NULL;
			so_jitscore = 0;
			return -1;
		}
	}

	while (!so_stop && host_clock_ms() - start < budget_ms) {
		unsigned slice = base_ms << (round < 16 ? round : 16);
		unsigned g_dead = host_clock_ms() + slice, b_dead, l_dead;
		unsigned gate_exhausted = 0;
		while (!so_stop && host_clock_ms() < g_dead &&
					 host_clock_ms() - start < budget_ms) {
			unsigned cstart, cend, g;
			if (have_ckpt) {
				SoCkpt sh;
				cstart = so_claim(ckpt, key, &sh);
				if (sh.best_text >= 0 && (best < 0 || sh.best_text < best)) {
					best = sh.best_text;
					best_gate = sh.best_gate;
					best_budget = sh.best_budget;
					best_limit = sh.best_limit;
				}
			} else {
				cstart = local_claim;
				local_claim += SO_CLAIM_CHUNK;
			}
			if (cstart >= SO_GATE_SPACE) {
				gate_exhausted = 1;
				break;
			}
			cend = cstart + SO_CLAIM_CHUNK;
			if (cend > SO_GATE_SPACE)
				cend = SO_GATE_SPACE;
			for (g = cstart; g < cend && !so_stop && host_clock_ms() < g_dead &&
											host_clock_ms() - start < budget_ms;
					 g++) {
				long sz;
				if (so_gate_dead(g))
					continue;
				sz = so_eval(cv, cand_tmp, g, best_budget, best_limit, cap_ms);
				tried++;
				if (sz >= 0 && sz < best) {
					best = sz;
					best_gate = g;
					if (have_ckpt)
						so_ckpt_save(ckpt, key, best_gate, best_budget, best_limit,
												 budget_cur, limit_cur, round, best);
				}
			}
		}
		b_dead = host_clock_ms() + slice;
		while (!so_stop && budget_cur < SO_BUDGET_SPACE && host_clock_ms() < b_dead &&
					 host_clock_ms() - start < budget_ms) {
			unsigned b = budget_cur++;
			long sz = so_eval(cv, cand_tmp, best_gate, b, best_limit, cap_ms);
			tried++;
			if (sz >= 0 && sz < best) {
				best = sz;
				best_budget = b;
			}
		}
		l_dead = host_clock_ms() + slice;
		while (!so_stop && limit_cur < SO_LIMIT_SPACE && host_clock_ms() < l_dead &&
					 host_clock_ms() - start < budget_ms) {
			unsigned l = limit_cur++;
			long sz = so_eval(cv, cand_tmp, best_gate, best_budget, l, cap_ms);
			tried++;
			if (sz >= 0 && sz < best) {
				best = sz;
				best_limit = l;
			}
		}
		round++;
		if (have_ckpt)
			so_ckpt_save(ckpt, key, best_gate, best_budget, best_limit, budget_cur,
									 limit_cur, round, best);
		if (gate_exhausted && budget_cur >= SO_BUDGET_SPACE &&
				limit_cur >= SO_LIMIT_SPACE)
			break;
	}

	if (so_eval(cv, cand_tmp, best_gate, best_budget, best_limit, 300000u) < 0) {
		remove(cand_tmp);
		mcc_free(cv);
		mcc_free(rv);
		so_run_cv = NULL;
		so_jitscore = 0;
		return -1;
	}
	i = so_copy(cand_tmp, outfile);
	remove(cand_tmp);
	if (s->verbose) {
		if (so_jitscore)
			printf("superopt: %u evals in %ums, best gate %u budget %u limit %u -> "
						 "%ld us/run, peak RSS %ld KiB\n",
						 tried, host_clock_ms() - start, best_gate, best_budget, best_limit,
						 best, so_last_rss);
		else
			printf("superopt: %u evals in %ums, best gate %u budget %u limit %u -> %ld .text\n",
						 tried, host_clock_ms() - start, best_gate, best_budget, best_limit,
						 best);
	}
	mcc_free(cv);
	mcc_free(rv);
	so_run_cv = NULL;
	so_jitscore = 0;
	return i;
}
#endif

int main(int argc, char **argv) {
	MCCState *s, *s1;
	int ret, opt, n = 0, t = 0, done;
	unsigned start_time = 0, end_time = 0;
	const char *first_file;
	int argc0 = argc;
	char **argv0 = argv;
	FILE *ppfp = NULL;

redo:
	argc = argc0, argv = argv0;
	s = s1 = mcc_new();
	opt = mcc_parse_args(s, &argc, &argv);

	if (n == 0) {
		ret = 0;
		if (opt == OPT_HELP) {
			fputs(help, stdout);
			if (s->verbose)
				goto help2;
		} else if (opt == OPT_HELP2) {
		help2:
			fputs(help2, stdout);
		} else if (opt == OPT_M32 || opt == OPT_M64) {
			ret = mcc_tool_cross(argv, opt);
		} else if (s->verbose)
			printf("%s", version);

		if (opt == OPT_AR)
			ret = mcc_tool_ar(argc, argv);
#ifdef MCC_TARGET_PE
		if (opt == OPT_IMPDEF)
			ret = mcc_tool_impdef(argc, argv);
#endif
		if (opt == OPT_PRINT_DIRS) {
			set_environment(s);
			mcc_set_output_type(s, MCC_OUTPUT_MEMORY);
			print_search_dirs(s);
		}
		if (opt) {
			if (opt < 0)
			err:
				ret = 1;
			mcc_delete(s);
			return ret;
		}
		if (s->clear_cache) {
			char dir[3072];
			if (host_cache_dir(dir, sizeof dir) == 0) {
				host_rmrf(dir);
				printf("cleared %s\n", dir);
			}
			mcc_delete(s);
			return 0;
		}
		if (s->nb_files == 0) {
			mcc_error_noabort("no input files");
		} else if (s->output_type == MCC_OUTPUT_PREPROCESS) {
			if (s->outfile && 0 != strcmp("-", s->outfile)) {
				ppfp = host_fopen(s->outfile, "wb");
				if (!ppfp)
					mcc_error_noabort("could not write '%s'", s->outfile);
			}
		} else if ((s->output_type == MCC_OUTPUT_OBJ && !s->option_r) || s->output_type == MCC_OUTPUT_ASM) {
			const char *act = s->output_type == MCC_OUTPUT_ASM ? "-S" : "-c";
			if (s->nb_libraries)
				mcc_error_noabort("cannot specify libraries with %s", act);
			else if (s->nb_files > 1 && s->outfile)
				mcc_error_noabort("cannot specify output file with %s many files", act);
		}
		if (s->nb_errors)
			goto err;
		if (s->do_bench)
			start_time = host_clock_ms();
	}

	set_environment(s);
	if (s->output_type == 0)
		s->output_type = MCC_OUTPUT_EXE;
	ret = mcc_set_output_type(s, s->output_type);
	if (ppfp)
		s->ppfp = ppfp;

	if ((s->output_type == MCC_OUTPUT_MEMORY || s->output_type == MCC_OUTPUT_PREPROCESS) && (s->dflag & 16)) {
		if (t)
			s->dflag |= 32;
		s->run_test = ++t;
		if (n)
			--n;
	}

#if MCC_HOST_POSIX
	if (0 == ret && s->optimize_search_seconds && n == 0 &&
			!getenv("MCC_SEARCH_WORKER") &&
			(s->output_type == MCC_OUTPUT_OBJ || s->output_type == MCC_OUTPUT_EXE) &&
			s->nb_files >= 1 && s->files[0]->name && !(s->files[0]->type & AFF_TYPE_LIB)) {
		int (*so)(int, char **, MCCState *, const char *) =
				getenv("MCC_AST_PERFN") ? mcc_superopt_perfn : mcc_superopt_search;
		if (s->embed_jit && s->verbose)
			printf("embed-jit manifest: functions=%s max-duration=%us%s\n",
						 s->jit_functions ? s->jit_functions : "main", s->jit_max_duration,
						 s->jit_max_duration == 0 ? " (unlimited)" : "");
		if (!s->outfile)
			s->outfile = default_outputfile(s, s->files[0]->name);
		if (so(argc0, argv0, s, s->outfile) == 0) {
			mcc_delete(s);
			return 0;
		}
		s->optimize_search_seconds = 0;
	}
#endif

	first_file = NULL;
	while (0 == ret) {
		struct filespec *f = s->files[n];
		s->filetype = f->type;
#ifdef MCC_TARGET_MACHO
		if (f->type & AFF_TYPE_FRAMEWORK) {
			ret = mcc_add_framework(s, f->name);
		} else
#endif
				if (f->type & AFF_TYPE_LIB) {
			ret = mcc_add_library(s, f->name);
		} else {
			if (MCC_VTIER(s->verbose) == MCC_V1)
				printf("-> %s\n", f->name);
			if (!first_file)
				first_file = f->name;
			ret = mcc_add_file(s, f->name);
		}
		if (++n == s->nb_files)
			break;
		if ((s->output_type == MCC_OUTPUT_OBJ && !s->option_r) || s->output_type == MCC_OUTPUT_ASM)
			break;
	}

	if (s->do_bench)
		end_time = host_clock_ms();

	if (s->run_test) {
		t = 0;
	} else if (s->output_type == MCC_OUTPUT_PREPROCESS) {
		;
	} else if (0 == ret) {
		if (s->output_type == MCC_OUTPUT_MEMORY) {
#ifdef MCC_TARGET_IS_HOST
			ret = mcc_run(s, argc, argv);
#endif
		} else if (s->syntax_only) {
			;
		} else {
			if (!s->outfile)
				s->outfile = default_outputfile(s, first_file);
			if (!s->just_deps)
				ret = mcc_output_file(s, s->outfile);
			if (!ret && s->gen_deps)
				gen_makedeps(s, s->outfile, s->deps_outfile);
		}
	}

	done = 1;
	if (t)
		done = 0;
	else if (ret) {
		if (s->nb_errors)
			ret = 1;
	} else if (n < s->nb_files)
		done = 0;
	else if (s->do_bench)
		mcc_print_stats(s, end_time - start_time);

	mcc_delete(s);
	if (!done)
		goto redo;
	if (ppfp)
		host_fclose(ppfp);
	return ret;
}
