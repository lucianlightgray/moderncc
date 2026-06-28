#ifndef ONE_SOURCE
# define ONE_SOURCE 1
#endif

#include "mcc.h"
#if ONE_SOURCE
# include "libmcc.c"
#endif
#include "mcctools.c"

static const char help[] =
    "Usage: mcc [options] file...\n"
    "       mcc [options] -run file [args...]\n"
    "Options:\n"
    "  -c                  Only compile and assemble, do not link\n"
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
#ifdef CONFIG_MCC_BCHECK
    "  -b                  Enable the built-in memory and bounds checker (implies -g)\n"
#endif
#ifdef CONFIG_MCC_BACKTRACE
    "  -bt[<n>]            Link with backtrace support (show up to <n> callers)\n"
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
    "  https://github.com/lucianlightgray/moderncc\n"
    ;

static const char help2[] =
    "More options:\n"
    "  -P, -P1                       With -E: suppress or use alternative #line output\n"
    "  -dD, -dM                      With -E: output #define directives\n"
    "  -Wp,<arg>                     Pass the comma-separated <arg> to the preprocessor\n"
    "  -O<n>                         Define __OPTIMIZE__ for n > 0 (no optimization is done)\n"
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
    "  -s                            Strip the symbol table from linked output\n"
    "Ignored options:\n"
    "  -arch -C --param -pedantic -pipe -traditional\n"
    "Warnings (-W[no-]...):\n"
    "  all                           Enable a set of common warnings (marked *)\n"
    "  error[=<warn>]                Treat warnings as errors (all, or the named one)\n"
    "  write-strings                 Make string literals const\n"
    "  unsupported                   Warn about ignored options, pragmas, etc.\n"
    "  implicit-function-declaration Warn about calls to undeclared functions (*)\n"
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
    "  stack-protector[-all]         Emit stack canaries (x86_64 ELF)\n"
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
    "mcc version "MCC_VERSION
#ifdef MCC_GITHASH
    " "MCC_GITHASH
#endif
    " ("
#ifdef MCC_TARGET_I386
        "i386"
#elif defined MCC_TARGET_X86_64
        "x86_64"
#elif defined MCC_TARGET_ARM
        "ARM"
# ifdef MCC_ARM_EABI
        " eabi"
#  ifdef MCC_ARM_HARDFLOAT
        "hf"
#  endif
# endif
#elif defined MCC_TARGET_ARM64
        "AArch64"
#elif defined MCC_TARGET_RISCV64
        "riscv64"
#endif
#ifdef MCC_TARGET_PE
        " Windows"
#elif defined(MCC_TARGET_MACHO)
        " Darwin"
#elif TARGETOS_FreeBSD || TARGETOS_FreeBSD_kernel
        " FreeBSD"
#elif TARGETOS_OpenBSD
        " OpenBSD"
#elif TARGETOS_NetBSD
        " NetBSD"
#else
        " Linux"
#endif
    ")\n"
    ;

static void print_dirs(const char *msg, char **paths, int nb_paths)
{
    printf("%s:\n%s", msg, nb_paths ? "" : "  -\n");
    for(int i = 0; i < nb_paths; i++)
        printf("  %s\n", paths[i]);
}

static void print_search_dirs(MCCState *s)
{
    printf("install: %s\n", s->mcc_lib_path);
    print_dirs("include", s->sysinclude_paths, s->nb_sysinclude_paths);
    print_dirs("libraries", s->library_paths, s->nb_library_paths);
    printf("libmcc1:\n  %s/%s\n", s->library_paths[0], CONFIG_MCC_CROSSPREFIX MCC_LIBMCC1);
#ifdef MCC_TARGET_UNIX
    print_dirs("crt", s->crt_paths, s->nb_crt_paths);
    printf("elfinterp:\n  %s\n",  s->elfint);
#endif
}

static void set_environment(MCCState *s)
{
    char * path;

    path = getenv("C_INCLUDE_PATH");
    if(path != NULL) {
        mcc_add_sysinclude_path(s, path);
    }
    path = getenv("CPATH");
    if(path != NULL) {
        mcc_add_include_path(s, path);
    }
    path = getenv("LIBRARY_PATH");
    if(path != NULL) {
        mcc_add_library_path(s, path);
    }
}

static char *default_outputfile(MCCState *s, const char *first_file)
{
    char buf[1024];
    char *ext;
    const char *name = "a";

    if (first_file && strcmp(first_file, "-"))
        name = mcc_basename(first_file);
    if (strlen(name) + 4 >= sizeof buf)
        name = "a";
    strcpy(buf, name);
    ext = mcc_fileextension(buf);
#ifdef MCC_TARGET_PE
    if (s->output_type == MCC_OUTPUT_DLL)
        strcpy(ext, ".dll");
    else
    if (s->output_type == MCC_OUTPUT_EXE)
        strcpy(ext, ".exe");
    else
#endif
    if ((s->just_deps || s->output_type == MCC_OUTPUT_OBJ) && !s->option_r && *ext)
        strcpy(ext, ".o");
    else
        strcpy(buf, "a.out");
    return mcc_strdup(buf);
}

static unsigned getclock_ms(void)
{
#ifdef _WIN32
    return GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000 + (tv.tv_usec+500)/1000;
#endif
}

int main(int argc, char **argv)
{
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
            help2: fputs(help2, stdout);
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
            if (opt < 0) err:
                ret = 1;
            mcc_delete(s);
            return ret;
        }
        if (s->nb_files == 0) {
            mcc_error_noabort("no input files");
        } else if (s->output_type == MCC_OUTPUT_PREPROCESS) {
            if (s->outfile && 0!=strcmp("-",s->outfile)) {
                ppfp = mcc_fopen(s->outfile, "wb");
                if (!ppfp)
                    mcc_error_noabort("could not write '%s'", s->outfile);
            }
        } else if (s->output_type == MCC_OUTPUT_OBJ && !s->option_r) {
            if (s->nb_libraries)
                mcc_error_noabort("cannot specify libraries with -c");
            else if (s->nb_files > 1 && s->outfile)
                mcc_error_noabort("cannot specify output file with -c many files");
        }
        if (s->nb_errors)
            goto err;
        if (s->do_bench)
            start_time = getclock_ms();
    }

    set_environment(s);
    if (s->output_type == 0)
        s->output_type = MCC_OUTPUT_EXE;
    ret = mcc_set_output_type(s, s->output_type);
    if (ppfp)
        s->ppfp = ppfp;

    if ((s->output_type == MCC_OUTPUT_MEMORY
      || s->output_type == MCC_OUTPUT_PREPROCESS)
        && (s->dflag & 16)) {
        if (t)
            s->dflag |= 32;
        s->run_test = ++t;
        if (n)
            --n;
    }

    first_file = NULL;
    while (0 == ret) {
        struct filespec *f = s->files[n];
        s->filetype = f->type;
        if (f->type & AFF_TYPE_LIB) {
            ret = mcc_add_library(s, f->name);
        } else {
            if (1 == s->verbose)
                printf("-> %s\n", f->name);
            if (!first_file)
                first_file = f->name;
            ret = mcc_add_file(s, f->name);
        }
        if (++n == s->nb_files)
            break;
        if (s->output_type == MCC_OUTPUT_OBJ && !s->option_r)
            break;
    }

    if (s->do_bench)
        end_time = getclock_ms();

    if (s->run_test) {
        t = 0;
    } else if (s->output_type == MCC_OUTPUT_PREPROCESS) {
        ;
    } else if (0 == ret) {
        if (s->output_type == MCC_OUTPUT_MEMORY) {
#ifdef MCC_IS_NATIVE
            ret = mcc_run(s, argc, argv);
#endif
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
        mcc_fclose(ppfp);
    return ret;
}
