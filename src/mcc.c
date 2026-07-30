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
		"  --embed-jit, --no-embed-jit   Bake the runtime JIT engine into a file output so it accepts --jit / MCC_JIT at its runtime (default off)\n"
		"  --jit, --no-jit               For -run: enable/disable the in-process JIT (default from build's MCC_CONFIG_JIT); output programs read the MCC_JIT env var (0/1) at their runtime\n"
		"  --jit-max-duration <sec>      Runtime JIT budget baked into the output (default 600; 0 = unlimited)\n"
		"  --jit-functions <syms>        Comma list of functions to JIT-optimize (default main; sites at their common ancestor)\n"
		"  --clear-cache                 Remove the per-user optimizer cache directory and exit\n"
		"  --stats[=N]                   Live optimizer/JIT stats panel; N is a bitmask (2=jit 4=strategy 8=combo 16=search; default all)\n"
		"  -pthread                      Support POSIX threads (-D_REENTRANT and -lpthread)\n"
		"  -include <file>               Include <file> before parsing each input file\n"
		"  -isystem <dir>                Add <dir> to the system include search path\n"
		"  -nostdlib                     Do not use the standard startup files or libraries\n"
		"  -static                       Link against static libraries\n"
		"  -dumpversion                  Display the version of the compiler\n"
		"  -dumpmachine                  Display the compiler's target machine\n"
		"  -print-search-dirs            Display the compiler's search directories\n"
		"  -print-prog-name=<prog>       Display the full path to <prog>\n"
		"  -print-file-name=<file>       Display the full path to library <file>\n"
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
		"  -init=, -fini=, -Map=, -version-script=, -as-needed, -O, -z= Accepted and ignored\n"
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

static void print_dirs(const char *msg, char **paths, int nb_paths) { MCC_TRACE("enter\n");
	printf("%s:\n%s", msg, nb_paths ? "" : "  -\n");
	for (int i = 0; i < nb_paths; i++)
		{ MCC_TRACE("br\n"); printf("  %s\n", paths[i]); }
}

static int print_found_in(const char *name, char **paths, int nb_paths) { MCC_TRACE("enter\n");
	char buf[4096];
	for (int i = 0; i < nb_paths; i++) { MCC_TRACE("br\n");
		FILE *f;
		if ((size_t)snprintf(buf, sizeof buf, "%s/%s", paths[i], name) >= sizeof buf)
			{ MCC_TRACE("br\n"); continue; }
		f = fopen(buf, "rb");
		if (f != NULL) { MCC_TRACE("br\n");
			fclose(f);
			printf("%s\n", buf);
			return 1;
		}
	}
	return 0;
}

static void print_prog_name(const char *name) { MCC_TRACE("enter\n");
	char buf[4096];
	if (host_find_tool(name, ".exe", buf, sizeof buf))
		{ MCC_TRACE("br\n"); printf("%s\n", buf); return; }
	printf("%s\n", name);
}

static void print_file_name(MCCState *s, const char *name) { MCC_TRACE("enter\n");
	if (print_found_in(name, s->library_paths, s->nb_library_paths))
		{ MCC_TRACE("br\n"); return; }
#ifdef MCC_TARGET_UNIX
	if (print_found_in(name, s->crt_paths, s->nb_crt_paths))
		{ MCC_TRACE("br\n"); return; }
#endif
	printf("%s\n", name);
}

static void print_search_dirs(MCCState *s) { MCC_TRACE("enter\n");
	printf("install: %s\n", s->mcc_lib_path);
	print_dirs("include", s->sysinclude_paths, s->nb_sysinclude_paths);
	print_dirs("libraries", s->library_paths, s->nb_library_paths);
	printf("mccrt:\n  %s/%s\n", s->library_paths[0], MCC_CONFIG_CROSSPREFIX MCC_MCCRT);
#ifdef MCC_TARGET_UNIX
	print_dirs("crt", s->crt_paths, s->nb_crt_paths);
	printf("elfinterp:\n  %s\n", s->elfint);
#endif
}

static void set_environment(MCCState *s) { MCC_TRACE("enter\n");
	char *path;

	path = getenv("C_INCLUDE_PATH");
	if (path != NULL) { MCC_TRACE("br\n");
		mcc_add_sysinclude_path(s, path);
	}
	path = getenv("CPATH");
	if (path != NULL) { MCC_TRACE("br\n");
		mcc_add_include_path(s, path);
	}
	path = getenv("LIBRARY_PATH");
	if (path != NULL) { MCC_TRACE("br\n");
		mcc_add_library_path(s, path);
	}
#ifdef MCC_TARGET_MACHO
	path = getenv("DYLD_FRAMEWORK_PATH");
	if (path != NULL) { MCC_TRACE("br\n");
		mcc_add_framework_path(s, path);
	}
#endif
}

static char *default_outputfile(MCCState *s, const char *first_file) { MCC_TRACE("enter\n");
	char buf[1024];
	char *ext;
	const char *name = "a";

	if (first_file && strcmp(first_file, "-"))
		{ MCC_TRACE("br\n"); name = mcc_basename(first_file); }
	if (strlen(name) + 4 >= sizeof buf)
		{ MCC_TRACE("br\n"); name = "a"; }
	strcpy(buf, name);
	ext = mcc_fileextension(buf);
	if ((s->just_deps || s->output_type == MCC_OUTPUT_OBJ) && !s->option_r && *ext)
		{ MCC_TRACE("br\n"); strcpy(ext, ".o"); }
	else if (s->output_type == MCC_OUTPUT_ASM && *ext)
		{ MCC_TRACE("br\n"); strcpy(ext, ".s"); }
#ifdef MCC_TARGET_PE
	else if (s->output_type == MCC_OUTPUT_DLL)
		{ MCC_TRACE("br\n"); strcpy(ext, ".dll"); }
	else if (s->output_type == MCC_OUTPUT_EXE)
		{ MCC_TRACE("br\n"); strcpy(ext, ".exe"); }
#endif
	else
		{ MCC_TRACE("br\n"); strcpy(buf, "a.out"); }
	return mcc_strdup(buf);
}

#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h>
#if MCC_HOST_POSIX
#include <unistd.h>
#endif

static volatile int so_stop;
static void so_on_stop(int sig) { MCC_TRACE("enter\n");
	(void)sig;
	so_stop = 1;
}

static int so_jitscore;
static long so_last_rss;
static const char **so_run_cv;
static int so_jit_env(void) { MCC_TRACE("enter\n");
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

static uint64_t so_fnv(uint64_t h, const void *p, size_t n) { MCC_TRACE("enter\n");
	const unsigned char *b = (const unsigned char *)p;
	for (size_t i = 0; i < n; i++) { MCC_TRACE("br\n");
		h ^= b[i];
		h *= 0x100000001b3u;
	}
	return h;
}

static uint64_t so_key(MCCState *s) { MCC_TRACE("enter\n");
	uint64_t h = 0xcbf29ce484222325u;
	if (s->nb_files >= 1 && s->files[0]->name) { MCC_TRACE("br\n");
		FILE *f = host_fopen(s->files[0]->name, "rb");
		if (f) { MCC_TRACE("br\n");
			char buf[65536];
			size_t r;
			while ((r = fread(buf, 1, sizeof buf, f)) > 0)
				{ MCC_TRACE("br\n"); h = so_fnv(h, buf, r); }
			fclose(f);
		}
	}
#ifdef MCC_CONFIG_TRIPLET
	h = so_fnv(h, MCC_CONFIG_TRIPLET, strlen(MCC_CONFIG_TRIPLET));
#endif
	/* The resolved -march. This key hashes the SOURCE BYTES, so without it a
	   winner measured where roundsd was legal is replayed on a target where it
	   is not -- the checkpoint records best_gate/best_text obtained under one
	   ISA and nothing else distinguishes them. Whole-file granularity here, so
	   the whole mask is the right term; the per-function AST cache uses a
	   narrower one (ast_isa_key_term) to keep generic slices shareable. */
	{
		uint32_t isa;
		mcc_isa_init(s); /* idempotent; resolves the default so an implicit
		                    baseline and an explicit -march=x86-64 agree */
		isa = s->isa_mask;
		h = so_fnv(h, &isa, sizeof isa);
	}
	if (so_jitscore) { MCC_TRACE("br\n");
		static const char tag[] = "jitscore";
		h = so_fnv(h, tag, sizeof tag - 1);
	}
	return h;
}

static int so_ckpt_path(char *buf, int cap, uint64_t key) { MCC_TRACE("enter\n");
	char dir[3072];
	if (host_cache_dir(dir, sizeof dir) != 0)
		{ MCC_TRACE("br\n"); return -1; }
	snprintf(buf, cap, "%s/so-%016" PRIx64 ".ck", dir, key);
	return 0;
}

static int so_ckpt_read(const char *path, uint64_t key, SoCkpt *c) { MCC_TRACE("enter\n");
	FILE *f = host_fopen(path, "rb");
	SoCkpt t;
	if (!f)
		{ MCC_TRACE("br\n"); return -1; }
	if (fread(&t, sizeof t, 1, f) != 1 || t.fmt != SO_CKPT_FMT || t.key != key) { MCC_TRACE("br\n");
		fclose(f);
		return -1;
	}
	fclose(f);
	*c = t;
	return 0;
}

static void so_ckpt_write(const char *path, const SoCkpt *nw) { MCC_TRACE("enter\n");
	char lockp[1300], tmpp[1300];
	void *lock;
	SoCkpt out = *nw, b;
	FILE *f;
	snprintf(lockp, sizeof lockp, "%s.lock", path);
	lock = host_file_lock(lockp);
	if ((f = host_fopen(path, "rb"))) { MCC_TRACE("br\n");
		if (fread(&b, sizeof b, 1, f) == 1 && b.fmt == nw->fmt && b.key == nw->key) { MCC_TRACE("br\n");
			if (b.best_text >= 0 && (out.best_text < 0 || b.best_text < out.best_text)) { MCC_TRACE("br\n");
				out.best_text = b.best_text;
				out.best_gate = b.best_gate;
				out.best_budget = b.best_budget;
				out.best_limit = b.best_limit;
			}
			if (b.claim_gate > out.claim_gate)
				{ MCC_TRACE("br\n"); out.claim_gate = b.claim_gate; }
			if (b.budget_cursor > out.budget_cursor)
				{ MCC_TRACE("br\n"); out.budget_cursor = b.budget_cursor; }
			if (b.limit_cursor > out.limit_cursor)
				{ MCC_TRACE("br\n"); out.limit_cursor = b.limit_cursor; }
			if (b.round > out.round)
				{ MCC_TRACE("br\n"); out.round = b.round; }
		}
		fclose(f);
	}
	snprintf(tmpp, sizeof tmpp, "%s.tmp", path);
	if ((f = host_fopen(tmpp, "wb"))) { MCC_TRACE("br\n");
		fwrite(&out, sizeof out, 1, f);
		host_fsync(f);
		fclose(f);
		host_rename(tmpp, path);
	}
	host_file_unlock(lock);
}

static unsigned so_claim(const char *path, uint64_t key, SoCkpt *shared) { MCC_TRACE("enter\n");
	char lockp[1300], tmpp[1300];
	void *lock;
	unsigned start;
	SoCkpt c;
	FILE *f;
	memset(&c, 0, sizeof c);
	c.fmt = SO_CKPT_FMT;
	c.key = key;
	c.best_text = -1;
	snprintf(lockp, sizeof lockp, "%s.lock", path);
	lock = host_file_lock(lockp);
	if ((f = host_fopen(path, "rb"))) { MCC_TRACE("br\n");
		SoCkpt b;
		if (fread(&b, sizeof b, 1, f) == 1 && b.fmt == SO_CKPT_FMT && b.key == key)
			{ MCC_TRACE("br\n"); c = b; }
		fclose(f);
	}
	start = c.claim_gate;
	if (start < SO_GATE_SPACE) { MCC_TRACE("br\n");
		c.claim_gate = start + SO_CLAIM_CHUNK;
		if (c.claim_gate > SO_GATE_SPACE)
			{ MCC_TRACE("br\n"); c.claim_gate = SO_GATE_SPACE; }
		snprintf(tmpp, sizeof tmpp, "%s.tmp", path);
		if ((f = host_fopen(tmpp, "wb"))) { MCC_TRACE("br\n");
			fwrite(&c, sizeof c, 1, f);
			host_fsync(f);
			fclose(f);
			host_rename(tmpp, path);
		}
	}
	host_file_unlock(lock);
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

static int so_gate_dead(unsigned gate) { MCC_TRACE("enter\n");
	return !((gate >> 2) & 1) && (gate >> 4) != 0;
}

typedef struct {
	const char *name;
	char *user;
} SoEnvAxis;

static SoEnvAxis so_axes[] = {
		{"MCC_AST_TEMPLATES", NULL},		 {"MCC_AST_PROMOTE", NULL},
		{"MCC_AST_INLINE", NULL},				 {"MCC_AST_NO_CALLFUL", NULL},
		{"MCC_AST_INLINE_LIMIT", NULL},	 {"MCC_AST_INLINE_NODES", NULL},
		{"MCC_AST_GRAFT", NULL},				 {"MCC_AST_BITFLAG", NULL},
		{"MCC_AST_CPROP_JOIN", NULL},		 {"MCC_AST_CSE_JOIN", NULL},
		{"MCC_AST_PROMOTE_LIMIT", NULL}, {"MCC_AST_OPT_LIMIT", NULL}};

#define SO_NAXES ((int)(sizeof so_axes / sizeof *so_axes))

static int so_axes_snapped;

static void so_axes_snapshot(void) { MCC_TRACE("enter\n");
	if (so_axes_snapped)
		{ MCC_TRACE("br\n"); return; }
	so_axes_snapped = 1;
	for (int i = 0; i < SO_NAXES; i++) { MCC_TRACE("br\n");
		const char *v = getenv(so_axes[i].name);
		if (v && *v) { MCC_TRACE("br\n");
			so_axes[i].user = mcc_strdup(v);
			MCC_TRACE("superopt: axis %s pinned by user to %s\n", so_axes[i].name, v);
		}
	}
}

#define SO_GATE_DEFAULT 0xFFFFFFFFu

static void so_setenv_axis(const char *name, const char *val) { MCC_TRACE("enter\n");
	for (int i = 0; i < SO_NAXES; i++)
		{ MCC_TRACE("br\n"); if (!strcmp(so_axes[i].name, name)) { MCC_TRACE("br\n");
			if (so_axes[i].user)
				{ MCC_TRACE("br\n"); val = so_axes[i].user; }
			break;
		} }
	host_setenv(name, val);
}

/* Restore an axis to the compiler's OWN default (or the user's pin) rather than
   forcing it off, so the search can only ever ADD that gate, never subtract it. */
static void so_unsetenv_axis(const char *name) { MCC_TRACE("enter\n");
	for (int i = 0; i < SO_NAXES; i++)
		{ MCC_TRACE("br\n"); if (!strcmp(so_axes[i].name, name)) { MCC_TRACE("br\n");
			if (so_axes[i].user) { MCC_TRACE("br\n");
				host_setenv(name, so_axes[i].user);
				return;
			}
			break;
		} }
	host_unsetenv(name);
}

static int so_promote_floor(void) { MCC_TRACE("enter\n");
	const char *e = getenv("MCC_SO_PROMOTE_FLOOR");
	return !e || !e[0] || strcmp(e, "0");
}

static void so_setenv_cfg(unsigned gate, unsigned budget, unsigned limit_lvl) { MCC_TRACE("enter\n");
	char buf[32];
	so_axes_snapshot();
	if (gate == SO_GATE_DEFAULT) { MCC_TRACE("br\n");
		host_setenv("MCC_SEARCH_WORKER", "1");
		for (int i = 0; i < SO_NAXES; i++)
			{ MCC_TRACE("br\n"); if (so_axes[i].user)
				{ MCC_TRACE("br\n"); host_setenv(so_axes[i].name, so_axes[i].user); }
			else
				{ MCC_TRACE("br\n"); host_unsetenv(so_axes[i].name); } }
		return;
	}
	unsigned limit = gate >> 4;
	int inl = (gate >> 2) & 1;
	int nsel = (int)(budget % SO_NNODE);
	int gsel = (int)((budget / SO_NNODE) % SO_NGRAFT);
	int bfsel = (int)((budget / (SO_NNODE * SO_NGRAFT)) % SO_NBF);
	int cpsel = (int)((budget / (SO_NNODE * SO_NGRAFT * SO_NBF)) % 2);
	int csesel = (int)((budget / (SO_NNODE * SO_NGRAFT * SO_NBF * 2)) % 2);
	int lv = so_limits[limit_lvl % SO_LIMIT_SPACE];
	host_setenv("MCC_SEARCH_WORKER", "1");
	so_setenv_axis("MCC_AST_TEMPLATES", (gate & 1) ? "1" : "0");
	if (((gate >> 1) & 1) || !so_promote_floor())
		{ MCC_TRACE("br\n"); so_setenv_axis("MCC_AST_PROMOTE", (gate >> 1) & 1 ? "1" : "0"); }
	else
		{ MCC_TRACE("br\n"); so_unsetenv_axis("MCC_AST_PROMOTE"); }
	so_setenv_axis("MCC_AST_INLINE", inl ? "1" : "0");
	so_setenv_axis("MCC_AST_NO_CALLFUL", (gate >> 3) & 1 ? "1" : "0");
	snprintf(buf, sizeof buf, "%u", inl ? limit : 0u);
	so_setenv_axis("MCC_AST_INLINE_LIMIT", buf);
	snprintf(buf, sizeof buf, "%d", so_nodes[nsel]);
	so_setenv_axis("MCC_AST_INLINE_NODES", buf);
	snprintf(buf, sizeof buf, "%d", so_graft[gsel]);
	so_setenv_axis("MCC_AST_GRAFT", buf);
	snprintf(buf, sizeof buf, "%d", so_bf[bfsel]);
	so_setenv_axis("MCC_AST_BITFLAG", buf);
	so_setenv_axis("MCC_AST_CPROP_JOIN", cpsel ? "1" : "0");
	so_setenv_axis("MCC_AST_CSE_JOIN", csesel ? "1" : "0");
	snprintf(buf, sizeof buf, "%d", lv);
	so_setenv_axis("MCC_AST_PROMOTE_LIMIT", buf);
	so_setenv_axis("MCC_AST_OPT_LIMIT", buf);
}

static long so_filesize(const char *p) { MCC_TRACE("enter\n");
	struct stat st;
	return stat(p, &st) == 0 ? (long)st.st_size : -1;
}

static int so_pe_is_machine(uint16_t m) { MCC_TRACE("enter\n");
	return m == 0x8664 || m == 0x014c || m == 0xaa64 || m == 0x01c0 ||
				 m == 0x01c4;
}

/* Offset of the COFF file header inside `f`: for a PE image ('MZ' + "PE\0\0")
   it is e_lfanew+4; for a bare COFF object (whose first word is a known machine
   id) it is 0. Returns -1 when `h` (the first 64 bytes) is neither. Leaves the
   stream position undefined. */
static long so_coff_off(FILE *f, const unsigned char *h) { MCC_TRACE("enter\n");
	if (h[0] == 'M' && h[1] == 'Z') { MCC_TRACE("br\n");
		uint32_t lfanew;
		unsigned char sig[4];
		memcpy(&lfanew, h + 0x3c, 4);
		if (fseek(f, (long)lfanew, SEEK_SET) != 0 || fread(sig, 1, 4, f) != 4 ||
				sig[0] != 'P' || sig[1] != 'E' || sig[2] || sig[3])
			{ MCC_TRACE("br\n"); return -1; }
		return (long)lfanew + 4;
	} else { MCC_TRACE("br\n");
		uint16_t machine;
		memcpy(&machine, h, 2);
		return so_pe_is_machine(machine) ? 0 : -1;
	}
}

/* Sum of the sizes of executable sections (IMAGE_SCN_CNT_CODE / _MEM_EXECUTE)
   in a PE image or COFF object; -1 if `f` is not PE/COFF. VirtualSize is the
   unpadded code size in an image; objects carry it in SizeOfRawData. */
static long so_pe_textsize(FILE *f, const unsigned char *h) { MCC_TRACE("enter\n");
	long coff_off = so_coff_off(f, h);
	unsigned char fh[20];
	uint16_t nsec, optsz, i;
	long total = 0;
	if (coff_off < 0)
		{ MCC_TRACE("br\n"); return -1; }
	if (fseek(f, coff_off, SEEK_SET) != 0 || fread(fh, 1, 20, f) != 20)
		{ MCC_TRACE("br\n"); return -1; }
	memcpy(&nsec, fh + 2, 2);
	memcpy(&optsz, fh + 16, 2);
	for (i = 0; i < nsec; i++) { MCC_TRACE("br\n");
		unsigned char sh[40];
		uint32_t vsize, rawsize, chars;
		if (fseek(f, coff_off + 20 + (long)optsz + (long)i * 40, SEEK_SET) != 0 ||
				fread(sh, 1, 40, f) != 40)
			{ MCC_TRACE("br\n"); break; }
		memcpy(&vsize, sh + 8, 4);
		memcpy(&rawsize, sh + 16, 4);
		memcpy(&chars, sh + 36, 4);
		if (chars & 0x20000020u)
			{ MCC_TRACE("br\n"); total += (long)(vsize ? vsize : rawsize); }
	}
	return total;
}

static long so_textsize(const char *p) { MCC_TRACE("enter\n");
	FILE *f = host_fopen(p, "rb");
	unsigned char h[64];
	long total = 0;
	if (!f)
		{ MCC_TRACE("br\n"); return -1; }
	if (fread(h, 1, sizeof h, f) < 64) { MCC_TRACE("br\n");
		fclose(f);
		return so_filesize(p);
	}
	if (h[0] == 0x7f && h[1] == 'E' && h[2] == 'L' && h[3] == 'F' &&
			h[4] == 2) { MCC_TRACE("br\n");
		uint64_t shoff, flags, size;
		uint16_t shentsize, shnum, i;
		memcpy(&shoff, h + 40, 8);
		memcpy(&shentsize, h + 58, 2);
		memcpy(&shnum, h + 60, 2);
		for (i = 0; i < shnum; i++) { MCC_TRACE("br\n");
			unsigned char sh[64];
			if (fseek(f, (long)(shoff + (uint64_t)i * shentsize), SEEK_SET) != 0)
				{ MCC_TRACE("br\n"); break; }
			if (fread(sh, 1, sizeof sh, f) < 40)
				{ MCC_TRACE("br\n"); break; }
			memcpy(&flags, sh + 8, 8);
			memcpy(&size, sh + 32, 8);
			if (flags & 0x4u)
				{ MCC_TRACE("br\n"); total += (long)size; }
		}
	} else { MCC_TRACE("br\n");
		long pe = so_pe_textsize(f, h);
		if (pe > 0)
			{ MCC_TRACE("br\n"); total = pe; }
	}
	fclose(f);
	return total > 0 ? total : so_filesize(p);
}

struct so_fn {
	char name[80];
	long size;
	unsigned cfg;
};

/* Per-function .text sizes from a COFF object/PE image. COFF symbols carry no
   size, so it is the gap from each function symbol to the next symbol in the
   same code section (last one runs to the section end). Best-effort; used only
   by the opt-in MCC_AST_PERFN mode. */
static int so_coff_fn_sizes(FILE *f, const unsigned char *h, struct so_fn *out,
														int max) { MCC_TRACE("enter\n");
	long coff_off = so_coff_off(f, h);
	unsigned char fh[20];
	uint32_t symptr, nsyms, strbase;
	uint16_t nsec, optsz, si;
	long secsz[256];
	unsigned char seccode[256];
	int n = 0, i, j;
	if (coff_off < 0)
		{ MCC_TRACE("br\n"); return -1; }
	if (fseek(f, coff_off, SEEK_SET) != 0 || fread(fh, 1, 20, f) != 20)
		{ MCC_TRACE("br\n"); return -1; }
	memcpy(&nsec, fh + 2, 2);
	memcpy(&symptr, fh + 8, 4);
	memcpy(&nsyms, fh + 12, 4);
	memcpy(&optsz, fh + 16, 2);
	if (!symptr || !nsyms || nsec == 0)
		{ MCC_TRACE("br\n"); return -1; }
	if (nsec > 256)
		{ MCC_TRACE("br\n"); nsec = 256; }
	for (si = 0; si < nsec; si++) { MCC_TRACE("br\n");
		unsigned char sh[40];
		uint32_t vsize, rawsize, chars;
		if (fseek(f, coff_off + 20 + (long)optsz + (long)si * 40, SEEK_SET) != 0 ||
				fread(sh, 1, 40, f) != 40)
			{ MCC_TRACE("br\n"); return -1; }
		memcpy(&vsize, sh + 8, 4);
		memcpy(&rawsize, sh + 16, 4);
		memcpy(&chars, sh + 36, 4);
		secsz[si] = (long)(vsize ? vsize : rawsize);
		seccode[si] = (chars & 0x20000020u) ? 1 : 0;
	}
	strbase = symptr + nsyms * 18u;
	for (i = 0; (uint32_t)i < nsyms && n < max;) { MCC_TRACE("br\n");
		unsigned char sym[18];
		uint32_t name0, name1, val;
		int16_t secnum;
		uint16_t type;
		unsigned char naux;
		char nm[80];
		int k, c;
		if (fseek(f, symptr + (long)i * 18, SEEK_SET) != 0 ||
				fread(sym, 1, 18, f) != 18)
			{ MCC_TRACE("br\n"); break; }
		memcpy(&name0, sym, 4);
		memcpy(&name1, sym + 4, 4);
		memcpy(&val, sym + 8, 4);
		memcpy(&secnum, sym + 12, 2);
		memcpy(&type, sym + 14, 2);
		naux = sym[17];
		i += 1 + (int)naux;
		if (secnum < 1 || secnum > (int)nsec || !seccode[secnum - 1])
			{ MCC_TRACE("br\n"); continue; }
		if ((type & 0x30) != 0x20) /* IMAGE_SYM_DTYPE_FUNCTION */
			{ MCC_TRACE("br\n"); continue; }
		nm[0] = 0;
		if (name0 == 0) { MCC_TRACE("br\n");
			long save = ftell(f);
			k = 0;
			if (fseek(f, (long)(strbase + name1), SEEK_SET) == 0)
				while (k < 79 && (c = fgetc(f)) > 0)
					{ MCC_TRACE("br\n"); nm[k++] = (char)c; }
			nm[k] = 0;
			if (save >= 0)
				{ MCC_TRACE("br\n"); fseek(f, save, SEEK_SET); }
		} else { MCC_TRACE("br\n");
			memcpy(nm, sym, 8);
			nm[8] = 0;
		}
		if (!nm[0])
			{ MCC_TRACE("br\n"); continue; }
		snprintf(out[n].name, sizeof out[n].name, "%s", nm);
		out[n].size = (long)val;      /* holds the symbol value for now */
		out[n].cfg = (unsigned)secnum; /* holds the section for now */
		n++;
	}
	for (i = 1; i < n; i++) { MCC_TRACE("br\n"); /* sort by (section, value) */
		struct so_fn key = out[i];
		j = i - 1;
		while (j >= 0 && (out[j].cfg > key.cfg ||
											(out[j].cfg == key.cfg && out[j].size > key.size)))
			{ MCC_TRACE("br\n"); out[j + 1] = out[j]; j--; }
		out[j + 1] = key;
	}
	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		long end = (i + 1 < n && out[i + 1].cfg == out[i].cfg)
									 ? out[i + 1].size
									 : secsz[out[i].cfg - 1];
		out[i].size = end > out[i].size ? end - out[i].size : 0;
		out[i].cfg = 0;
	}
	{
		int w = 0;
		for (i = 0; i < n; i++)
			{ MCC_TRACE("br\n"); if (out[i].size > 0) out[w++] = out[i]; }
		n = w;
	}
	return n;
}

static int so_fn_sizes(const char *path, struct so_fn *out, int max) { MCC_TRACE("enter\n");
	FILE *f = host_fopen(path, "rb");
	unsigned char h[64], sh[64];
	uint64_t shoff, symoff = 0, symsz = 0, syment = 0, stroff = 0;
	uint16_t shentsize, shnum, i;
	uint32_t type, link = 0;
	long o;
	int n = 0, strtab_idx = -1;
	if (!f)
		{ MCC_TRACE("br\n"); return -1; }
	if (fread(h, 1, 64, f) < 64) { MCC_TRACE("br\n");
		fclose(f);
		return -1;
	}
	if (!(h[0] == 0x7f && h[1] == 'E' && h[2] == 'L' && h[3] == 'F' &&
				h[4] == 2)) { MCC_TRACE("br\n"); /* not 64-bit ELF: try PE/COFF */
		n = so_coff_fn_sizes(f, h, out, max);
		fclose(f);
		return n;
	}
	memcpy(&shoff, h + 40, 8);
	memcpy(&shentsize, h + 58, 2);
	memcpy(&shnum, h + 60, 2);
	for (i = 0; i < shnum; i++) { MCC_TRACE("br\n");
		if (fseek(f, (long)(shoff + (uint64_t)i * shentsize), SEEK_SET) != 0 ||
				fread(sh, 1, 64, f) < 64)
			{ MCC_TRACE("br\n"); break; }
		memcpy(&type, sh + 4, 4);
		if (type == 2) { MCC_TRACE("br\n");
			memcpy(&symoff, sh + 24, 8);
			memcpy(&symsz, sh + 32, 8);
			memcpy(&link, sh + 40, 4);
			memcpy(&syment, sh + 56, 8);
			strtab_idx = (int)link;
		}
	}
	if (strtab_idx < 0 || syment == 0) { MCC_TRACE("br\n");
		fclose(f);
		return -1;
	}
	if (fseek(f, (long)(shoff + (uint64_t)strtab_idx * shentsize), SEEK_SET) == 0 &&
			fread(sh, 1, 64, f) == 64)
		{ MCC_TRACE("br\n"); memcpy(&stroff, sh + 24, 8); }
	for (o = 0; (uint64_t)o < symsz && n < max; o += (long)syment) { MCC_TRACE("br\n");
		unsigned char sym[24];
		uint32_t stname;
		uint64_t stsize;
		char nm[80];
		int k = 0, c;
		if (fseek(f, (long)symoff + o, SEEK_SET) != 0 || fread(sym, 1, 24, f) < 24)
			{ MCC_TRACE("br\n"); break; }
		memcpy(&stname, sym, 4);
		memcpy(&stsize, sym + 16, 8);
		if ((sym[4] & 0xf) != 2 || stsize == 0)
			{ MCC_TRACE("br\n"); continue; }
		if (fseek(f, (long)stroff + stname, SEEK_SET) != 0)
			{ MCC_TRACE("br\n"); continue; }
		while (k < 79 && (c = fgetc(f)) > 0)
			{ MCC_TRACE("br\n"); nm[k++] = (char)c; }
		nm[k] = 0;
		if (!nm[0])
			{ MCC_TRACE("br\n"); continue; }
		snprintf(out[n].name, sizeof out[n].name, "%s", nm);
		out[n].size = (long)stsize;
		out[n].cfg = 0;
		n++;
	}
	fclose(f);
	return n;
}

static int so_copy(const char *src, const char *dst) { MCC_TRACE("enter\n");
	return host_copy_file(src, dst, 1);
}

/* Copy argv[1..argc) into cv (advancing *pn), dropping the user's own output
   option (`-o file` or `-ofile`). The search appends its own `-o <candidate>`,
   so keeping the user's would make every worker warn "multiple -o option". */
static void so_copy_args_drop_o(const char **cv, int *pn, int argc,
																char **argv) { MCC_TRACE("enter\n");
	int i;
	for (i = 1; i < argc; i++) { MCC_TRACE("br\n");
		if (!strcmp(argv[i], "-o")) { MCC_TRACE("br\n");
			if (i + 1 < argc)
				{ MCC_TRACE("br\n"); i++; } /* skip the filename operand too */
			continue;
		}
		if (argv[i][0] == '-' && argv[i][1] == 'o' && argv[i][2])
			{ MCC_TRACE("br\n"); continue; } /* single-token -ofile form */
		cv[(*pn)++] = argv[i];
	}
}

static int so_spawn_timeout(const char **cv, unsigned timeout_ms) { MCC_TRACE("enter\n");
	return host_spawn_timeout((const char *const *)cv, timeout_ms,
														(const volatile int *)&so_stop);
}

static int so_spawn_must(const char **cv, unsigned timeout_ms, int tries) { MCC_TRACE("enter\n");
	return host_spawn_retry((const char *const *)cv, timeout_ms, tries,
													(const volatile int *)&so_stop);
}

static int so_spawn_run(const char **cv, unsigned timeout_ms, long *usec,
												long *rss_kb) { MCC_TRACE("enter\n");
	return host_spawn_run((const char *const *)cv, timeout_ms, usec, rss_kb,
												(const volatile int *)&so_stop);
}

static long so_run_score(unsigned timeout_ms) { MCC_TRACE("enter\n");
	long best = -1, rss_best = -1;
	int k;
	for (k = 0; k < 3 && !so_stop; k++) { MCC_TRACE("br\n");
		long usec = -1, rss = -1;
		if (so_spawn_run(so_run_cv, timeout_ms, &usec, &rss) != 0)
			{ MCC_TRACE("br\n"); return -1; }
		if (best < 0 || usec < best)
			{ MCC_TRACE("br\n"); best = usec; }
		if (rss_best < 0 || rss < rss_best)
			{ MCC_TRACE("br\n"); rss_best = rss; }
	}
	so_last_rss = rss_best;
	return best;
}

#define SO_SPILL_W_DEFAULT 48

static char so_spill_path[1216];
static long so_spill_w = SO_SPILL_W_DEFAULT;

static long so_spill_read(void) { MCC_TRACE("enter\n");
	FILE *f;
	long v = -1;
	if (!so_spill_path[0])
		{ MCC_TRACE("br\n"); return -1; }
	f = host_fopen(so_spill_path, "r");
	if (!f)
		{ MCC_TRACE("br\n"); return -1; }
	if (fscanf(f, "%ld", &v) != 1)
		{ MCC_TRACE("br\n"); v = -1; }
	fclose(f);
	remove(so_spill_path);
	return v;
}

static long so_eval(const char **cv, const char *cand_tmp, unsigned gate,
										unsigned budget, unsigned limit_lvl,
										unsigned timeout_ms) { MCC_TRACE("enter\n");
	long sz, sp;
	so_setenv_cfg(gate, budget, limit_lvl);
	if (so_spill_path[0])
		{ MCC_TRACE("br\n"); host_setenv("MCC_AST_SPILL_OUT", so_spill_path); }
	if (so_spawn_timeout(cv, timeout_ms) != 0)
		{ MCC_TRACE("br\n"); return -1; }
	if (so_jitscore && so_run_cv) { MCC_TRACE("br\n");
		long sc = so_run_score(timeout_ms);
		if (sc >= 0) { MCC_TRACE("br\n");
			so_spill_read();
			return sc;
		}
	}
	sz = so_textsize(cand_tmp);
	sp = so_spill_read();
	if (sz >= 0 && sp > 0) { MCC_TRACE("br\n");
		MCC_TRACE("superopt: gate %u text %ld stackrefs %ld\n", gate, sz, sp);
		sz += sp * so_spill_w;
	}
	return sz;
}

static void so_ckpt_save(const char *ckpt, uint64_t key, unsigned best_gate,
												 unsigned best_budget, unsigned best_limit,
												 unsigned budget_cur, unsigned limit_cur,
												 unsigned round, long best) { MCC_TRACE("enter\n");
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
static long so_fn_find(struct so_fn *a, int n, const char *name) { MCC_TRACE("enter\n");
	int i;
	for (i = 0; i < n; i++)
		{ MCC_TRACE("br\n"); if (!strcmp(a[i].name, name))
			{ MCC_TRACE("br\n"); return a[i].size; } }
	return -1;
}

#define SO_PF_FMT 2u

typedef struct {
	uint32_t fmt;
	uint32_t best_cfg;
	uint64_t key;
	int64_t best_size;
	uint32_t tried;
	uint32_t pad;
} SoPfCkpt;

static uint64_t so_pf_key(uint64_t fnhash) { MCC_TRACE("enter\n");
	uint64_t h = so_fnv(0xcbf29ce484222325u, &fnhash, sizeof fnhash);
	h = so_fnv(h, MCC_VERSION_STR, strlen(MCC_VERSION_STR));
#ifdef MCC_CONFIG_TRIPLET
	h = so_fnv(h, MCC_CONFIG_TRIPLET, strlen(MCC_CONFIG_TRIPLET));
#endif
	return h;
}

static int so_pf_path(char *buf, int cap, uint64_t key) { MCC_TRACE("enter\n");
	char dir[3072];
	if (host_cache_dir(dir, sizeof dir) != 0)
		{ MCC_TRACE("br\n"); return -1; }
	snprintf(buf, cap, "%s/pf-%016" PRIx64 ".ck", dir, key);
	return 0;
}

static int so_pf_read(uint64_t key, SoPfCkpt *c) { MCC_TRACE("enter\n");
	char path[3200];
	FILE *f;
	SoPfCkpt t;
	if (so_pf_path(path, sizeof path, key) != 0)
		{ MCC_TRACE("br\n"); return -1; }
	if (!(f = host_fopen(path, "rb")))
		{ MCC_TRACE("br\n"); return -1; }
	if (fread(&t, sizeof t, 1, f) != 1 || t.fmt != SO_PF_FMT || t.key != key) { MCC_TRACE("br\n");
		fclose(f);
		return -1;
	}
	fclose(f);
	*c = t;
	return 0;
}

static void so_pf_write(uint64_t key, const SoPfCkpt *nw) { MCC_TRACE("enter\n");
	char path[3200], lockp[3300], tmpp[3300];
	void *lock;
	SoPfCkpt out = *nw, b;
	FILE *f;
	if (so_pf_path(path, sizeof path, key) != 0)
		{ MCC_TRACE("br\n"); return; }
	snprintf(lockp, sizeof lockp, "%s.lock", path);
	lock = host_file_lock(lockp);
	if ((f = host_fopen(path, "rb"))) { MCC_TRACE("br\n");
		if (fread(&b, sizeof b, 1, f) == 1 && b.fmt == SO_PF_FMT &&
				b.key == key) { MCC_TRACE("br\n");
			if (b.best_size >= 0 &&
					(out.best_size < 0 || b.best_size < out.best_size)) { MCC_TRACE("br\n");
				out.best_size = b.best_size;
				out.best_cfg = b.best_cfg;
			}
			out.tried |= b.tried;
		}
		fclose(f);
	}
	snprintf(tmpp, sizeof tmpp, "%s.tmp", path);
	if ((f = host_fopen(tmpp, "wb"))) { MCC_TRACE("br\n");
		fwrite(&out, sizeof out, 1, f);
		host_fsync(f);
		fclose(f);
		host_rename(tmpp, path);
	}
	host_file_unlock(lock);
}

static int so_fn_hashes(const char *path, struct so_fn *fns, int nf,
												uint64_t *fnh) { MCC_TRACE("enter\n");
	FILE *f = host_fopen(path, "rb");
	char nm[80];
	unsigned long long h;
	int got = 0, i;
	for (i = 0; i < nf; i++)
		{ MCC_TRACE("br\n"); fnh[i] = 0; }
	if (!f)
		{ MCC_TRACE("br\n"); return 0; }
	while (fscanf(f, "%79s %llx", nm, &h) == 2)
		{ MCC_TRACE("br\n"); for (i = 0; i < nf; i++) { MCC_TRACE("br\n");
			const char *sn = fns[i].name;
			if (!strcmp(sn, nm) || (sn[0] == '_' && !strcmp(sn + 1, nm))) { MCC_TRACE("br\n");
				if (!fnh[i])
					{ MCC_TRACE("br\n"); got++; }
				fnh[i] = h;
				break;
			}
		} }
	fclose(f);
	return got;
}

static int mcc_superopt_perfn(int argc, char **argv, MCCState *s,
															const char *outfile) { MCC_TRACE("enter\n");
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
		{ MCC_TRACE("br\n"); pstrcpy(exe, sizeof exe, argv[0]); }
	snprintf(cand, sizeof cand, "%s.mcc-pf", outfile);
	snprintf(hashp, sizeof hashp, "%s.fnh", cand);
	cfg = mcc_malloc(SO_MAXFN * 96);
	cv = mcc_malloc((argc + 4) * sizeof *cv);
	argn = 0;
	cv[argn++] = exe;
	so_copy_args_drop_o(cv, &argn, argc, argv);
	cv[argn++] = "-o";
	cv[argn++] = cand;
	cv[argn] = NULL;
	so_stop = 0;
	host_install_interrupt(so_on_stop);
	host_setenv("MCC_SEARCH_WORKER", "1");
	so_axes_snapshot();
	so_setenv_axis("MCC_AST_TEMPLATES", "1");
	host_setenv("MCC_AST_FN_CONFIG", "");
	remove(hashp);
	host_setenv("MCC_AST_HASH_OUT", hashp);
	if (so_spawn_must(cv, 300000u, 4) != 0 ||
			(nf = so_fn_sizes(cand, fns, SO_MAXFN)) <= 0) { MCC_TRACE("br\n");
		host_unsetenv("MCC_AST_HASH_OUT");
		remove(hashp);
		remove(cand);
		mcc_free(cfg);
		mcc_free(cv);
		return -1;
	}
	host_unsetenv("MCC_AST_HASH_OUT");
	for (fi = 0; fi < nf; fi++) { MCC_TRACE("br\n");
		int mx = fi, j;
		for (j = fi + 1; j < nf; j++)
			{ MCC_TRACE("br\n"); if (fns[j].size > fns[mx].size)
				{ MCC_TRACE("br\n"); mx = j; } }
		if (mx != fi) { MCC_TRACE("br\n");
			struct so_fn t = fns[fi];
			fns[fi] = fns[mx];
			fns[mx] = t;
		}
	}
	so_fn_hashes(hashp, fns, nf, fnh);
	remove(hashp);
	for (fi = 0; fi < nf; fi++) { MCC_TRACE("br\n");
		best_cfg[fi] = 7;
		best_size[fi] = fns[fi].size;
		tried[fi] = 4;
		if (fnh[fi]) { MCC_TRACE("br\n");
			SoPfCkpt c;
			if (so_pf_read(so_pf_key(fnh[fi]), &c) == 0) { MCC_TRACE("br\n");
				tried[fi] |= c.tried & 7u;
				if (c.best_size > 0 && c.best_size <= best_size[fi] &&
						(c.best_cfg == 1 || c.best_cfg == 3 || c.best_cfg == 7)) { MCC_TRACE("br\n");
					best_cfg[fi] = c.best_cfg;
					best_size[fi] = c.best_size;
				}
				if (tried[fi] == 7)
					{ MCC_TRACE("br\n"); cached++; }
			}
		}
	}
	for (fi = 0; fi < nf && !so_stop && host_clock_ms() - start < budget_ms; fi++)
		{ MCC_TRACE("br\n"); for (ci = 0; ci < 3; ci++) { MCC_TRACE("br\n");
			int m, j;
			if (((tried[fi] >> ci) & 1) ||
					host_clock_ms() - start >= budget_ms || so_stop)
				{ MCC_TRACE("br\n"); continue; }
			for (p = 0, j = 0; j < nf; j++)
				{ MCC_TRACE("br\n"); p += snprintf(cfg + p, SO_MAXFN * 96 - p, "%s=%u;", fns[j].name,
											j == fi ? cfgs[ci] : best_cfg[j]); }
			host_setenv("MCC_AST_FN_CONFIG", cfg);
			if (so_spawn_timeout(cv, 300000u) != 0)
				{ MCC_TRACE("br\n"); continue; }
			m = so_fn_sizes(cand, cur, SO_MAXFN);
			sz = so_fn_find(cur, m, fns[fi].name);
			if (sz <= 0)
				{ MCC_TRACE("br\n"); continue; }
			tried[fi] |= 1u << ci;
			if (sz < best_size[fi]) { MCC_TRACE("br\n");
				best_size[fi] = sz;
				best_cfg[fi] = cfgs[ci];
			}
			if (fnh[fi]) { MCC_TRACE("br\n");
				SoPfCkpt c;
				memset(&c, 0, sizeof c);
				c.fmt = SO_PF_FMT;
				c.key = so_pf_key(fnh[fi]);
				c.best_cfg = best_cfg[fi];
				c.best_size = best_size[fi];
				c.tried = tried[fi];
				so_pf_write(c.key, &c);
			}
		} }
	for (p = 0, fi = 0; fi < nf; fi++)
		{ MCC_TRACE("br\n"); p += snprintf(cfg + p, SO_MAXFN * 96 - p, "%s=%u;", fns[fi].name,
									best_cfg[fi]); }
	host_setenv("MCC_AST_FN_CONFIG", cfg);
	if (so_spawn_must(cv, 300000u, 4) != 0) { MCC_TRACE("br\n");
		remove(cand);
		mcc_free(cfg);
		mcc_free(cv);
		return -1;
	}
	i = so_copy(cand, outfile);
	remove(cand);
	if (mcc_log_enabled_v(s->verbose, MCC_LOG_DEBUG)) { MCC_TRACE("br\n");
		long tot = 0;
		for (fi = 0; fi < nf; fi++)
			{ MCC_TRACE("br\n"); tot += best_size[fi]; }
		mcc_logf_v(s->verbose, MCC_LOG_DEBUG,
								"superopt-perfn: %d functions (%d cached) in %ums, total .text %ld\n",
								nf, cached, host_clock_ms() - start, tot);
	}
	mcc_free(cfg);
	mcc_free(cv);
	return i;
}

static int mcc_superopt_search(int argc, char **argv, MCCState *s,
															 const char *outfile) { MCC_TRACE("enter\n");
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
		{ MCC_TRACE("br\n"); if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "-S") ||
				!strcmp(argv[i], "-E") || !strcmp(argv[i], "-r") ||
				!strcmp(argv[i], "-shared"))
			{ MCC_TRACE("br\n"); links_exe = 0; } }
	so_jitscore = so_jit_env() && links_exe;
	key = so_key(s);
	if (host_exe_path(exe, sizeof exe) <= 0)
		{ MCC_TRACE("br\n"); pstrcpy(exe, sizeof exe, argv[0]); }
	snprintf(cand_tmp, sizeof cand_tmp, "%s.mcc-so-cand", outfile);
	{
		const char *sw = getenv("MCC_SO_SPILL_SCORE");
		if (sw && sw[0] && strcmp(sw, "0")) { MCC_TRACE("br\n");
			snprintf(so_spill_path, sizeof so_spill_path, "%s.mcc-so-spill", outfile);
			so_spill_w = atol(sw) > 1 ? atol(sw) : SO_SPILL_W_DEFAULT;
		}
	}
	{
		const char *ds = getenv("MCC_SO_DEFAULT_SEED");
		if (ds && ds[0] && strcmp(ds, "0"))
			{ MCC_TRACE("br\n"); best_gate = SO_GATE_DEFAULT; }
	}
	have_ckpt = so_ckpt_path(ckpt, sizeof ckpt, key) == 0;
	if (have_ckpt && so_ckpt_read(ckpt, key, &ck) == 0) { MCC_TRACE("br\n");
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
	so_copy_args_drop_o(cv, &argn, argc, argv);
	cv[argn++] = "-o";
	cv[argn++] = cand_tmp;
	cv[argn] = NULL;

	if (so_jitscore) { MCC_TRACE("br\n");
		int rn = 0;
		rv = mcc_malloc((argc + 4) * sizeof *rv);
		rv[rn++] = exe;
		for (i = 1; i < argc; i++) { MCC_TRACE("br\n");
			if (!strcmp(argv[i], "-o")) { MCC_TRACE("br\n");
				i++;
				continue;
			}
			if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "-S") ||
					!strcmp(argv[i], src))
				{ MCC_TRACE("br\n"); continue; }
			rv[rn++] = argv[i];
		}
		rv[rn++] = "-run";
		rv[rn++] = src;
		rv[rn] = NULL;
		so_run_cv = rv;
	}

	so_stop = 0;
	host_install_interrupt(so_on_stop);

	{
		unsigned t0 = host_clock_ms(), dt;
		best = so_eval(cv, cand_tmp, best_gate, best_budget, best_limit, 300000u);
		dt = host_clock_ms() - t0;
		base_ms = (dt ? dt : 1u) * SO_SLICE_FACTOR;
		cap_ms = dt * 4u < 2000u ? 2000u : dt * 4u;
		tried++;
		if (best < 0) { MCC_TRACE("br\n");
			remove(cand_tmp);
			mcc_free(cv);
			mcc_free(rv);
			so_run_cv = NULL;
			so_jitscore = 0;
			return -1;
		}
	}

	while (!so_stop && host_clock_ms() - start < budget_ms) { MCC_TRACE("br\n");
		unsigned slice = base_ms << (round < 16 ? round : 16);
		/* Each round explores three axes in turn (gate, budget, limit). A slice
		   wider than a third of the run's budget lets the FIRST axis consume the
		   whole run, so the later two are never reached at all -- measured on a
		   warm checkpoint after 8 compiles: claim_gate had advanced to 576 while
		   budget_cursor and limit_cursor were both still 0. Cap the slice so
		   every axis gets a turn. */
		{
			unsigned el = host_clock_ms() - start;
			unsigned rem = el < budget_ms ? budget_ms - el : 0u;
			unsigned share = rem / 3u;
			if (share && slice > share)
				{ MCC_TRACE("br\n"); slice = share; }
		}
		unsigned g_dead = host_clock_ms() + slice, b_dead, l_dead;
		unsigned gate_exhausted = 0;
		while (!so_stop && host_clock_ms() < g_dead &&
					 host_clock_ms() - start < budget_ms) { MCC_TRACE("br\n");
			unsigned cstart, cend, g;
			if (have_ckpt) { MCC_TRACE("br\n");
				SoCkpt sh;
				cstart = so_claim(ckpt, key, &sh);
				if (sh.best_text >= 0 && (best < 0 || sh.best_text < best)) { MCC_TRACE("br\n");
					best = sh.best_text;
					best_gate = sh.best_gate;
					best_budget = sh.best_budget;
					best_limit = sh.best_limit;
				}
			} else { MCC_TRACE("br\n");
				cstart = local_claim;
				local_claim += SO_CLAIM_CHUNK;
			}
			if (cstart >= SO_GATE_SPACE) { MCC_TRACE("br\n");
				gate_exhausted = 1;
				break;
			}
			cend = cstart + SO_CLAIM_CHUNK;
			if (cend > SO_GATE_SPACE)
				{ MCC_TRACE("br\n"); cend = SO_GATE_SPACE; }
			for (g = cstart; g < cend && !so_stop && host_clock_ms() < g_dead &&
											host_clock_ms() - start < budget_ms;
					 g++) { MCC_TRACE("br\n");
				long sz;
				if (so_gate_dead(g))
					{ MCC_TRACE("br\n"); continue; }
				sz = so_eval(cv, cand_tmp, g, best_budget, best_limit, cap_ms);
				tried++;
				if (sz >= 0 && sz < best) { MCC_TRACE("br\n");
					best = sz;
					best_gate = g;
					if (have_ckpt)
						{ MCC_TRACE("br\n"); so_ckpt_save(ckpt, key, best_gate, best_budget, best_limit,
												 budget_cur, limit_cur, round, best); }
				}
			}
		}
		b_dead = host_clock_ms() + slice;
		while (!so_stop && budget_cur < SO_BUDGET_SPACE && host_clock_ms() < b_dead &&
					 host_clock_ms() - start < budget_ms) { MCC_TRACE("br\n");
			unsigned b = budget_cur++;
			long sz = so_eval(cv, cand_tmp, best_gate, b, best_limit, cap_ms);
			tried++;
			if (sz >= 0 && sz < best) { MCC_TRACE("br\n");
				best = sz;
				best_budget = b;
			}
		}
		l_dead = host_clock_ms() + slice;
		while (!so_stop && limit_cur < SO_LIMIT_SPACE && host_clock_ms() < l_dead &&
					 host_clock_ms() - start < budget_ms) { MCC_TRACE("br\n");
			unsigned l = limit_cur++;
			long sz = so_eval(cv, cand_tmp, best_gate, best_budget, l, cap_ms);
			tried++;
			if (sz >= 0 && sz < best) { MCC_TRACE("br\n");
				best = sz;
				best_limit = l;
			}
		}
		round++;
		if (have_ckpt)
			{ MCC_TRACE("br\n"); so_ckpt_save(ckpt, key, best_gate, best_budget, best_limit, budget_cur,
									 limit_cur, round, best); }
		if (gate_exhausted && budget_cur >= SO_BUDGET_SPACE &&
				limit_cur >= SO_LIMIT_SPACE)
			{ MCC_TRACE("br\n"); break; }
	}

	if (so_eval(cv, cand_tmp, best_gate, best_budget, best_limit, 300000u) < 0) { MCC_TRACE("br\n");
		remove(cand_tmp);
		mcc_free(cv);
		mcc_free(rv);
		so_run_cv = NULL;
		so_jitscore = 0;
		return -1;
	}
	i = so_copy(cand_tmp, outfile);
	remove(cand_tmp);
	if (mcc_log_enabled_v(s->verbose, MCC_LOG_DEBUG)) { MCC_TRACE("br\n");
		if (so_jitscore)
			{ MCC_TRACE("br\n"); mcc_logf_v(s->verbose, MCC_LOG_DEBUG,
									"superopt: %u evals in %ums, best gate %u budget %u limit %u -> "
									"%ld us/run, peak RSS %ld KiB\n",
									tried, host_clock_ms() - start, best_gate, best_budget,
									best_limit, best, so_last_rss); }
		else
			{ MCC_TRACE("br\n"); mcc_logf_v(s->verbose, MCC_LOG_DEBUG,
									"superopt: %u evals in %ums, best gate %u budget %u limit %u -> "
									"%ld .text\n",
									tried, host_clock_ms() - start, best_gate, best_budget,
									best_limit, best); }
	}
	mcc_free(cv);
	mcc_free(rv);
	so_run_cv = NULL;
	so_jitscore = 0;
	return i;
}

int main(int argc, char **argv) { MCC_TRACE("enter\n");
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
#ifdef MCC_EMBED_JIT
	{
		extern void mccjit_embed_reset(void);
		mccjit_embed_reset();
	}
#endif
	opt = mcc_parse_args(s, &argc, &argv);

	if (n == 0 && s->print_isa) { MCC_TRACE("br\n");
		/* Checked before the OPT_HELP dispatch: with no input files
		   mcc_parse_args reports OPT_HELP, which would print usage over the
		   answer the user asked for. */
		mcc_isa_print(s);
		mcc_delete(s);
		return 0;
	}
	if (n == 0) { MCC_TRACE("br\n");
		ret = 0;
		if (opt == OPT_HELP) { MCC_TRACE("br\n");
			fputs(help, stdout);
			if (s->verbose)
				{ MCC_TRACE("br\n"); goto help2; }
		} else if (opt == OPT_HELP2) { MCC_TRACE("br\n");
		help2:
			fputs(help2, stdout);
		} else if (opt == OPT_M32 || opt == OPT_M64) { MCC_TRACE("br\n");
			ret = mcc_tool_cross(argv, opt);
		} else if (s->verbose)
			{ MCC_TRACE("br\n"); printf("%s", version); }

		if (opt == OPT_AR)
			{ MCC_TRACE("br\n"); ret = mcc_tool_ar(argc, argv); }
#ifdef MCC_TARGET_PE
		if (opt == OPT_IMPDEF)
			{ MCC_TRACE("br\n"); ret = mcc_tool_impdef(argc, argv); }
#endif
		if (opt == OPT_PRINT_DIRS) { MCC_TRACE("br\n");
			set_environment(s);
			mcc_set_output_type(s, MCC_OUTPUT_MEMORY);
			print_search_dirs(s);
		}
		if (opt == OPT_PRINT_PROG) { MCC_TRACE("br\n");
			print_prog_name(s->print_query ? s->print_query : "");
		}
		if (opt == OPT_PRINT_FILE) { MCC_TRACE("br\n");
			set_environment(s);
			mcc_set_output_type(s, MCC_OUTPUT_MEMORY);
			print_file_name(s, s->print_query ? s->print_query : "");
		}
		if (opt) { MCC_TRACE("br\n");
			if (opt < 0)
			{ MCC_TRACE("br\n"); err:
				ret = 1; }
			mcc_delete(s);
			return ret;
		}
		if (s->clear_cache) { MCC_TRACE("br\n");
			char dir[3072];
			if (host_cache_dir(dir, sizeof dir) == 0) { MCC_TRACE("br\n");
				host_rmrf(dir);
				printf("cleared %s\n", dir);
			}
			mcc_delete(s);
			return 0;
		}
		if (s->nb_files == 0) { MCC_TRACE("br\n");
			mcc_error_noabort("no input files");
		} else if (s->output_type == MCC_OUTPUT_PREPROCESS) { MCC_TRACE("br\n");
			if (s->outfile && 0 != strcmp("-", s->outfile)) { MCC_TRACE("br\n");
				ppfp = host_fopen(s->outfile, "wb");
				if (!ppfp)
					{ MCC_TRACE("br\n"); mcc_error_noabort("could not write '%s'", s->outfile); }
			}
		} else if ((s->output_type == MCC_OUTPUT_OBJ && !s->option_r) || s->output_type == MCC_OUTPUT_ASM) { MCC_TRACE("br\n");
			const char *act = s->output_type == MCC_OUTPUT_ASM ? "-S" : "-c";
			if (s->nb_libraries)
				{ MCC_TRACE("br\n"); mcc_error_noabort("cannot specify libraries with %s", act); }
			else if (s->nb_files > 1 && s->outfile)
				{ MCC_TRACE("br\n"); mcc_error_noabort("cannot specify output file with %s many files", act); }
		}
		if (s->nb_errors)
			{ MCC_TRACE("br\n"); goto err; }
		if (s->do_bench)
			{ MCC_TRACE("br\n"); start_time = host_clock_ms(); }
	}

	set_environment(s);
	if (s->output_type == 0)
		{ MCC_TRACE("br\n"); s->output_type = MCC_OUTPUT_EXE; }
	ret = mcc_set_output_type(s, s->output_type);
	if (ppfp)
		{ MCC_TRACE("br\n"); s->ppfp = ppfp; }

	if ((s->output_type == MCC_OUTPUT_MEMORY || s->output_type == MCC_OUTPUT_PREPROCESS) && (s->dflag & 16)) { MCC_TRACE("br\n");
		if (t)
			{ MCC_TRACE("br\n"); s->dflag |= 32; }
		s->run_test = ++t;
		if (n)
			{ MCC_TRACE("br\n"); --n; }
	}

	if (0 == ret && s->optimize_search_seconds && n == 0 &&
			!mcc_env_on("MCC_SEARCH_WORKER") &&
			(s->output_type == MCC_OUTPUT_OBJ || s->output_type == MCC_OUTPUT_EXE) &&
			s->nb_files >= 1 && s->files[0]->name && !(s->files[0]->type & AFF_TYPE_LIB)) { MCC_TRACE("br\n");
		int (*so)(int, char **, MCCState *, const char *) =
				mcc_env_on("MCC_AST_PERFN") ? mcc_superopt_perfn : mcc_superopt_search;
#ifdef MCC_EMBED_JIT
		{
			extern int mccjit_embed_manifest(MCCState * s);
			if (s->embed_jit)
				{ MCC_TRACE("br\n"); mccjit_embed_manifest(s); }
		}
#else
		if (s->embed_jit && s->verbose)
			{ MCC_TRACE("br\n"); printf("embed-jit manifest: functions=%s max-duration=%us%s\n",
						 s->jit_functions ? s->jit_functions : "main", s->jit_max_duration,
						 s->jit_max_duration == 0 ? " (unlimited)" : ""); }
#endif
		if (!s->outfile)
			{ MCC_TRACE("br\n"); s->outfile = default_outputfile(s, s->files[0]->name); }
		MCC_TRACE_V(s->verbose, "superopt dispatch %s -> %s\n", s->files[0]->name, s->outfile);
		if (so(argc0, argv0, s, s->outfile) == 0) { MCC_TRACE("br\n");
			mcc_delete(s);
			return 0;
		}
		s->optimize_search_seconds = 0;
	}

	first_file = NULL;
	while (0 == ret) { MCC_TRACE("br\n");
		struct filespec *f = s->files[n];
		s->filetype = f->type;
#ifdef MCC_TARGET_MACHO
		if (f->type & AFF_TYPE_FRAMEWORK) { MCC_TRACE("br\n");
			ret = mcc_add_framework(s, f->name);
		} else
#endif
				if (f->type & AFF_TYPE_LIB) { MCC_TRACE("br\n");
			ret = mcc_add_library(s, f->name);
		} else { MCC_TRACE("br\n");
			if (MCC_VTIER(s->verbose) == MCC_V1)
				{ MCC_TRACE("br\n"); printf("-> %s\n", f->name); }
			if (!first_file)
				{ MCC_TRACE("br\n"); first_file = f->name; }
			ret = mcc_add_file(s, f->name);
		}
		if (++n == s->nb_files)
			{ MCC_TRACE("br\n"); break; }
		if ((s->output_type == MCC_OUTPUT_OBJ && !s->option_r) || s->output_type == MCC_OUTPUT_ASM)
			{ MCC_TRACE("br\n"); break; }
	}

	if (s->do_bench)
		{ MCC_TRACE("br\n"); end_time = host_clock_ms(); }

#ifdef MCC_EMBED_JIT
	{
		extern int mccjit_embed_have_fns(void);
		if (0 == ret && s->embed_jit && mccjit_embed_have_fns() &&
				(s->output_type == MCC_OUTPUT_EXE ||
				 (s->output_type == MCC_OUTPUT_OBJ && s->option_r))) { MCC_TRACE("br\n");
			int bs = find_elf_sym(s->symtab, "mccjit_boot_swap");
			int internal =
					bs && ((ElfW(Sym) *)s->symtab->data)[bs].st_shndx != SHN_UNDEF;
			if (!internal) { MCC_TRACE("br\n");
				const char *eng = getenv("MCC_EMBED_JIT_LIB");
				if (eng && eng[0]) { MCC_TRACE("br\n");
					int saved_ft = s->filetype;
					s->filetype |= AFF_WHOLE_ARCHIVE;
					ret = mcc_add_file(s, eng);
					s->filetype = saved_ft;
				} else { MCC_TRACE("br\n");
#ifdef MCC_EMBED_JIT_BLOB
					extern int mcc_add_jit_engine_embedded(MCCState *);
					ret = mcc_add_jit_engine_embedded(s);
#else
					char engbuf[1024], exe[1024], *sl;
					int saved_ft = s->filetype;
					s->filetype |= AFF_WHOLE_ARCHIVE;
					if (host_exe_path(exe, sizeof exe) > 0 && (sl = strrchr(exe, '/'))) { MCC_TRACE("br\n");
						*sl = 0;
						if (snprintf(engbuf, sizeof engbuf, "%s/libmcc-static.a", exe) <
								(int)sizeof engbuf) { MCC_TRACE("br\n");
							FILE *ef = fopen(engbuf, "rb");
							if (ef) { MCC_TRACE("br\n");
								fclose(ef);
								ret = mcc_add_file(s, engbuf);
								s->filetype = saved_ft;
								goto jit_engine_done;
							}
						}
					}
					ret = mcc_add_library(s, "mcc");
				jit_engine_done:
					s->filetype = saved_ft;
#endif
				}
			}
		}
	}
#endif

	if (s->run_test) { MCC_TRACE("br\n");
		t = 0;
	} else if (s->output_type == MCC_OUTPUT_PREPROCESS) { MCC_TRACE("br\n");
		;
	} else if (0 == ret) { MCC_TRACE("br\n");
		if (s->output_type == MCC_OUTPUT_MEMORY) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_IS_HOST
			ret = mcc_run(s, argc, argv);
#endif
		} else if (s->syntax_only) { MCC_TRACE("br\n");
			;
		} else { MCC_TRACE("br\n");
			if (!s->outfile)
				{ MCC_TRACE("br\n"); s->outfile = default_outputfile(s, first_file); }
			MCC_TRACE_V(s->verbose, "output_file %s type=%d\n", s->outfile, s->output_type);
			if (!s->just_deps)
				{ MCC_TRACE("br\n"); ret = mcc_output_file(s, s->outfile); }
			if (!ret && s->gen_deps)
				{ MCC_TRACE("br\n"); gen_makedeps(s, s->outfile, s->deps_outfile); }
			{
				const char *sp = getenv("MCC_AST_SPILL_OUT");
				if (!ret && sp && sp[0]) { MCC_TRACE("br\n");
					FILE *sf = host_fopen(sp, "w");
					if (sf) { MCC_TRACE("br\n");
						fprintf(sf, "%ld\n", mcc_stackref_count);
						fclose(sf);
					}
				}
			}
		}
	}

	done = 1;
	if (t)
		{ MCC_TRACE("br\n"); done = 0; }
	else if (ret) { MCC_TRACE("br\n");
		if (s->nb_errors)
			{ MCC_TRACE("br\n"); ret = 1; }
	} else if (n < s->nb_files)
		{ MCC_TRACE("br\n"); done = 0; }
	else if (s->do_bench)
		{ MCC_TRACE("br\n"); mcc_print_stats(s, end_time - start_time); }

	mcc_delete(s);
	if (!done)
		{ MCC_TRACE("br\n"); goto redo; }
	if (ppfp)
		{ MCC_TRACE("br\n"); host_fclose(ppfp); }
	return ret;
}
