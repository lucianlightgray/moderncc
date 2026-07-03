/*
 *  mcchost.c - HOST-axis platform services for the mcc compiler
 *
 *  The single home for everything that depends on the machine mcc runs on:
 *  path semantics, self-location, process spawning, runnable memory, fault
 *  handling, dynamic loading, and native toolchain discovery.  See TODO.md
 *  ("Platform spec") for the complete map.
 *
 *  Compiled for every target triple (cross compilers still need paths,
 *  spawn, clocks, self-path); the JIT/fault sections compile only under
 *  MCC_IS_NATIVE.  Under CONFIG_MCC_BACKTRACE_ONLY (runtime/lib/bt-exe.c)
 *  only the fault section is compiled.
 */

#include "mcc.h"

#ifndef CONFIG_MCC_BACKTRACE_ONLY

#ifdef _WIN32
# include <process.h>
#else
# include <sys/stat.h>
# include <sys/wait.h>
#endif

/* ------------------------------------------------------------------- */
/* paths */

/* '\' -> '/' in place; no-op on POSIX hosts */
ST_FUNC char *host_path_normalize(char *path)
{
#ifdef _WIN32
    char *p;
    for (p = path; *p; ++p)
        if (*p == '\\')
            *p = '/';
#endif
    return path;
}

/* canonical absolute path in a libc-malloc'd buffer (free with libc_free),
   or NULL */
ST_FUNC char *host_path_canonical(const char *path)
{
#ifdef _WIN32
    return _fullpath(NULL, path, 260);
#else
    return realpath(path, NULL);
#endif
}

/* case fold for the include-cache hash (paths compare case-insensitively
   on Windows) */
ST_FUNC int host_path_hash_fold(int c)
{
#ifdef _WIN32
    return toup(c);
#else
    return c;
#endif
}

ST_FUNC FILE *host_fopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}

ST_FUNC int host_fclose(FILE *f)
{
    return fclose(f);
}

/* make an output file executable; no-op on Windows */
ST_FUNC void host_set_exec_bits(const char *file)
{
#ifndef _WIN32
    chmod(file, 0777);
#else
    (void)file;
#endif
}

/* ------------------------------------------------------------------- */
/* self location */

#if defined _WIN32
# if defined LIBMCC_AS_DLL && defined MCC_HOST_AUTO_MCCDIR_W32
static HMODULE mcc_module;
BOOL WINAPI DllMain (HINSTANCE hDll, DWORD dwReason, LPVOID lpReserved)
{
    if (DLL_PROCESS_ATTACH == dwReason)
        mcc_module = hDll;
    return TRUE;
}
# else
#  define mcc_module NULL
# endif
#elif defined __APPLE__
extern int _NSGetExecutablePath(char *buf, unsigned int *bufsize);
#endif

ST_FUNC int host_exe_path(char *buf, int size)
{
    int n = -1;
#if defined _WIN32
    n = GetModuleFileNameA(mcc_module, buf, size);
    if (n <= 0 || n >= size)
        return -1;
    host_path_normalize(buf);
    return n;
#else
# if defined __linux__ || defined __CYGWIN__
    n = readlink("/proc/self/exe", buf, size - 1);
# elif defined __NetBSD__
    n = readlink("/proc/curproc/exe", buf, size - 1);
# elif defined __FreeBSD__ || defined __DragonFly__
    n = readlink("/proc/curproc/file", buf, size - 1);
# elif defined __APPLE__
    {
        unsigned int sz = size;
        if (_NSGetExecutablePath(buf, &sz) == 0)
            n = (int)strlen(buf);
    }
# endif
    if (n <= 0 || n >= size)
        return -1;
    buf[n] = 0;
    return n;
#endif
}

#ifdef MCC_HOST_AUTO_MCCDIR_W32
ST_FUNC char *host_w32_mccdir(char *path)
{
    char *p;
    if (host_exe_path(path, MAX_PATH) < 0)
        path[0] = 0;
    p = mcc_basename(strlwr(path));
    if (p > path)
        --p;
    *p = 0;
    return path;
}
#endif

ST_FUNC int host_system_dir(char *buf, int size)
{
#ifdef _WIN32
    GetSystemDirectoryA(buf, size);
    host_path_normalize(buf);
    return 0;
#else
    (void)buf; (void)size;
    return -1;
#endif
}

/* ------------------------------------------------------------------- */
/* process spawn / host tools */

#ifdef _WIN32
/* CreateProcess-style command-line quoting for one argument */
static char *host_quote_w32(const char *s)
{
    char *o, *r = mcc_malloc(2 * strlen(s) + 3);
    int cbs = 0, quoted = !*s;

    for (o = r; *s; *o++ = *s++) {
        quoted |= *s == ' ' || *s == '\t';
        if (*s == '\\' || *s == '"')
            *o++ = '\\';
        else
            o -= cbs;
        cbs = *s == '\\' ? cbs + 1 : 0;
    }
    if (quoted) {
        memmove(r + 1, r, o++ - r);
        *r = *o++ = '"';
    } else {
        o -= cbs;
    }

    *o = 0;
    return r;
}
#endif

/* spawn argv and wait: the child's exit code, or -1.  Quotes correctly
   per host (no shell involved) */
ST_FUNC int host_spawn_wait(const char *const *argv)
{
#ifdef _WIN32
    int i, n, ret;
    char **qv;
    for (n = 0; argv[n]; ++n);
    qv = mcc_malloc((n + 1) * sizeof *qv);
    for (i = 0; i < n; ++i)
        qv[i] = host_quote_w32(argv[i]);
    qv[n] = NULL;
    ret = _spawnvp(_P_WAIT, argv[0], (const char *const *)qv);
    for (i = 0; i < n; ++i)
        mcc_free(qv[i]);
    mcc_free(qv);
    return ret;
#else
    int pid = fork(), status;
    if (pid == 0) {
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    if (pid < 0 || waitpid(pid, &status, 0) != pid || !WIFEXITED(status))
        return -1;
    return WEXITSTATUS(status);
#endif
}

/* execvp semantics: only returns on failure (-1).  On Windows: quote,
   spawn, wait, and exit with the child's code */
ST_FUNC int host_exec_replace(char **argv)
{
#ifdef _WIN32
    int ret; char **p;
    for (p = argv; *p; ++p)
        *p = host_quote_w32(*p);
    ret = _spawnvp(_P_NOWAIT, argv[0], (const char *const *)argv);
    if (-1 == ret)
        return ret;
    _cwait(&ret, ret, _WAIT_CHILD);
    exit(ret);
#else
    return execvp(argv[0], argv);
#endif
}

/* locate a tool/library on the host search path (Windows SearchPath);
   returns 1 and fills buf if found, else 0 */
ST_FUNC int host_find_tool(const char *name, const char *ext, char *buf, int size)
{
#ifdef _WIN32
    return SearchPath(NULL, name, ext, size, buf, NULL) ? 1 : 0;
#else
    (void)name; (void)ext; (void)buf; (void)size;
    return 0;
#endif
}

/* ad-hoc codesign an output file; 0 on success (no-op unless the build
   was configured with CONFIG_CODESIGN, i.e. native Darwin) */
ST_FUNC int host_codesign_adhoc(const char *file)
{
#ifdef CONFIG_CODESIGN
    const char *argv[] = { "codesign", "-f", "-s", "-", file, NULL };
    return host_spawn_wait(argv);
#else
    (void)file;
    return 0;
#endif
}

/* ------------------------------------------------------------------- */
/* time / env */

ST_FUNC unsigned host_clock_ms(void)
{
#ifdef _WIN32
    return GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000 + (tv.tv_usec+500)/1000;
#endif
}

ST_FUNC char **host_environ(void)
{
#ifdef __APPLE__
    extern char ***_NSGetEnviron(void);
    return *_NSGetEnviron();
#else
    extern char **environ;
    return environ;
#endif
}

/* ------------------------------------------------------------------- */
/* dynamic loading */

#ifdef CONFIG_MCC_STATIC

/* fully static host builds have no dlopen; `-run` resolves symbols from
   this built-in libc table instead */
typedef struct MCCSyms {
    char *str;
    void *ptr;
} MCCSyms;

static MCCSyms mcc_syms[] = {
#if !defined(CONFIG_MCCBOOT)
#define MCCSYM(a) { #a, (void *)&a, },
    MCCSYM(stdin) MCCSYM(stdout) MCCSYM(stderr)
    MCCSYM(printf) MCCSYM(fprintf) MCCSYM(sprintf) MCCSYM(snprintf)
    MCCSYM(vprintf) MCCSYM(vfprintf) MCCSYM(vsnprintf)
    MCCSYM(puts) MCCSYM(fputs) MCCSYM(putchar) MCCSYM(fputc) MCCSYM(putc)
    MCCSYM(getchar) MCCSYM(fgetc) MCCSYM(getc) MCCSYM(fgets) MCCSYM(ungetc)
    MCCSYM(scanf) MCCSYM(fscanf) MCCSYM(sscanf)
    MCCSYM(fopen) MCCSYM(freopen) MCCSYM(fclose) MCCSYM(fflush)
    MCCSYM(fread) MCCSYM(fwrite) MCCSYM(fseek) MCCSYM(ftell) MCCSYM(rewind)
    MCCSYM(feof) MCCSYM(ferror) MCCSYM(clearerr) MCCSYM(fileno)
    MCCSYM(perror) MCCSYM(remove) MCCSYM(rename) MCCSYM(setvbuf) MCCSYM(setbuf)
    MCCSYM(malloc) MCCSYM(calloc) MCCSYM(realloc) MCCSYM(free)
    MCCSYM(exit) MCCSYM(_Exit) MCCSYM(abort) MCCSYM(atexit)
    MCCSYM(atoi) MCCSYM(atol) MCCSYM(atoll) MCCSYM(atof)
    MCCSYM(strtol) MCCSYM(strtoll) MCCSYM(strtoul) MCCSYM(strtoull)
    MCCSYM(strtod) MCCSYM(strtof) MCCSYM(strtold)
    MCCSYM(rand) MCCSYM(srand) MCCSYM(qsort) MCCSYM(bsearch)
    MCCSYM(abs) MCCSYM(labs) MCCSYM(llabs) MCCSYM(getenv) MCCSYM(system)
    MCCSYM(memcpy) MCCSYM(memmove) MCCSYM(memset) MCCSYM(memcmp) MCCSYM(memchr)
    MCCSYM(strlen) MCCSYM(strnlen) MCCSYM(strcmp) MCCSYM(strncmp)
    MCCSYM(strcpy) MCCSYM(strncpy) MCCSYM(strcat) MCCSYM(strncat)
    MCCSYM(strchr) MCCSYM(strrchr) MCCSYM(strstr) MCCSYM(strtok)
    MCCSYM(strspn) MCCSYM(strcspn) MCCSYM(strpbrk) MCCSYM(strerror)
    MCCSYM(sin) MCCSYM(cos) MCCSYM(tan) MCCSYM(asin) MCCSYM(acos) MCCSYM(atan)
    MCCSYM(atan2) MCCSYM(sinh) MCCSYM(cosh) MCCSYM(tanh)
    MCCSYM(exp) MCCSYM(log) MCCSYM(log10) MCCSYM(log2) MCCSYM(pow)
    MCCSYM(sqrt) MCCSYM(cbrt) MCCSYM(ceil) MCCSYM(floor) MCCSYM(round)
    MCCSYM(trunc) MCCSYM(fabs) MCCSYM(fmod) MCCSYM(fmin) MCCSYM(fmax)
    MCCSYM(hypot) MCCSYM(ldexp) MCCSYM(frexp) MCCSYM(modf)
#undef MCCSYM
#endif
    { NULL, NULL },
};

static void *host_static_sym(const char *symbol)
{
    MCCSyms *p = mcc_syms;
    while (p->str != NULL) {
        if (!strcmp(p->str, symbol))
            return p->ptr;
        p++;
    }
    return NULL;
}

ST_FUNC void *host_dlopen(const char *name)
{
    (void)name;
    return NULL;
}

ST_FUNC void host_dlclose(void *h)
{
    (void)h;
}

ST_FUNC const char *host_dlerror(void)
{
    return "error";
}

ST_FUNC void *host_dlsym(void *h, const char *symbol)
{
    (void)h;
    return host_static_sym(symbol);
}

ST_FUNC void *host_dlsym_process(const char *symbol)
{
    return host_static_sym(symbol);
}

#else /* !CONFIG_MCC_STATIC */

ST_FUNC void *host_dlopen(const char *name)
{
#ifdef _WIN32
    return (void*)LoadLibraryA(name);
#else
    return dlopen(name, RTLD_GLOBAL | RTLD_LAZY);
#endif
}

ST_FUNC void host_dlclose(void *h)
{
#ifdef _WIN32
    FreeLibrary((HMODULE)h);
#else
    dlclose(h);
#endif
}

ST_FUNC const char *host_dlerror(void)
{
#ifdef _WIN32
    return "error";
#else
    return dlerror();
#endif
}

ST_FUNC void *host_dlsym(void *h, const char *symbol)
{
#ifdef _WIN32
    return (void*)GetProcAddress((HMODULE)h, symbol);
#else
    return dlsym(h, symbol);
#endif
}

/* resolve a symbol against the whole process (RTLD_DEFAULT); NULL on
   Windows where import resolution goes through host_dlsym per module */
ST_FUNC void *host_dlsym_process(const char *symbol)
{
#ifdef _WIN32
    (void)symbol;
    return NULL;
#else
    return dlsym(RTLD_DEFAULT, symbol);
#endif
}

#endif /* CONFIG_MCC_STATIC */

/* ------------------------------------------------------------------- */
/* native toolchain services */

/* macOS SDK root (".../SDKs/MacOSX.sdk") via libxcselect; NULL if it
   cannot be determined (caller falls back to the fixed locations) */
ST_FUNC const char *host_macos_sdk_root(void)
{
#ifdef __APPLE__
    static char buf[1024];
    static int done;
    char *sdkroot = NULL, *pos = NULL;
    void* xcs;
    int (*f)(unsigned int, char**);

    if (!done) {
        done = 1;
        xcs = host_dlopen("libxcselect.dylib");
        f = xcs ? host_dlsym(xcs, "xcselect_host_sdk_path") : NULL;
        if (f)
            f(1, &sdkroot);
        if (sdkroot)
            pos = strstr(sdkroot, "SDKs/MacOSX");
        if (pos)
            snprintf(buf, sizeof buf, "%.*s.sdk", (int)(pos - sdkroot + 11), sdkroot);
#pragma push_macro("free")
#undef free
        free(sdkroot);
#pragma pop_macro("free")
    }
    return buf[0] ? buf : NULL;
#else
    return NULL;
#endif
}

/* BSD: the ELF interpreter can be overridden from the environment */
ST_FUNC const char *host_elf_interp_override(void)
{
    return getenv("LD_SO");
}

/* ------------------------------------------------------------------- */
/* runnable memory / JIT */

#ifdef MCC_IS_NATIVE

#ifndef _WIN32
# include <sys/mman.h>
#endif

ST_FUNC size_t host_pagesize(void)
{
#if defined _WIN32
    return 4096;
#elif defined _SC_PAGESIZE
    return sysconf(_SC_PAGESIZE);
#elif defined __APPLE__
    return getpagesize();
#else
    return 4096;
#endif
}

ST_FUNC void *host_runmem_alloc(unsigned *psize, int *ptr_diff)
{
    unsigned size = *psize;
    void *ptr;
    *ptr_diff = 0;
#ifdef CONFIG_SELINUX
    {
        void *prw;
        char tmpfname[] = "/tmp/.mccrunXXXXXX";
        int fd = mkstemp(tmpfname);
        unlink(tmpfname);
        ftruncate(fd, size);

        ptr = mmap(NULL, size * 2, PROT_READ|PROT_EXEC, MAP_SHARED, fd, 0);
        prw = mmap((char*)ptr + size, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, fd, 0);
        close(fd);
        if (ptr == MAP_FAILED || prw == MAP_FAILED)
            return NULL;
        *ptr_diff = (char*)prw - (char*)ptr;
        size *= 2;
    }
#elif defined _WIN32
    ptr = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    ptr = mcc_malloc(size += host_pagesize());
#endif
    *psize = size;
    return ptr;
}

ST_FUNC void host_runmem_free(void *ptr, unsigned size)
{
#ifdef CONFIG_SELINUX
    munmap(ptr, size);
#elif defined _WIN32
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    size_t page = host_pagesize();
    host_runmem_protect((void*)((size_t)ptr + (-(size_t)ptr & (page - 1))),
                        size - page, HOST_PROT_RW);
    mcc_free(ptr);
#endif
}

ST_FUNC void host_icache_flush(void *ptr, unsigned long length)
{
#if !defined _WIN32 && \
    ((defined MCC_TARGET_ARM && !TARGETOS_BSD) || defined MCC_TARGET_ARM64 \
     || defined MCC_TARGET_RISCV64)
    void __clear_cache(void *beginning, void *end);
    __clear_cache(ptr, (char *)ptr + length);
#else
    (void)ptr; (void)length;
#endif
}

ST_FUNC int host_runmem_protect(void *ptr, unsigned long length, int mode)
{
#ifdef _WIN32
    static const unsigned char protect[] = {
        PAGE_EXECUTE_READ,
        PAGE_READONLY,
        PAGE_READWRITE,
        PAGE_EXECUTE_READWRITE
        };
    DWORD old;
    if (!VirtualProtect(ptr, length, protect[mode], &old))
        return -1;
#else
    static const unsigned char protect[] = {
        PROT_READ | PROT_EXEC,
        PROT_READ,
        PROT_READ | PROT_WRITE,
        PROT_READ | PROT_WRITE | PROT_EXEC
        };
    if (mprotect(ptr, length, protect[mode]))
        return -1;
    if (mode == HOST_PROT_RX || mode == HOST_PROT_RWX)
        host_icache_flush(ptr, length);
#endif
    return 0;
}

ST_FUNC void *host_unwind_register(void *table, unsigned size_bytes, size_t base)
{
#ifdef _WIN64
    if (!RtlAddFunctionTable((RUNTIME_FUNCTION*)table,
                             size_bytes / sizeof (RUNTIME_FUNCTION), base))
        return NULL;
    return table;
#else
    (void)table; (void)size_bytes; (void)base;
    return NULL;
#endif
}

ST_FUNC void host_unwind_unregister(void *table)
{
#ifdef _WIN64
    if (table)
        RtlDeleteFunctionTable((RUNTIME_FUNCTION*)table);
#else
    (void)table;
#endif
}

#endif /* MCC_IS_NATIVE */

#endif /* !CONFIG_MCC_BACKTRACE_ONLY */

/* ------------------------------------------------------------------- */
/* faults / signal context */

#if defined MCC_IS_NATIVE && defined CONFIG_MCC_BACKTRACE

#ifndef _WIN32
# include <signal.h>
# ifndef __OpenBSD__
#  include <sys/ucontext.h>
# endif
#else
# define ucontext_t CONTEXT
#endif

/* pc/fp/sp extraction for every supported host OS x arch */
ST_FUNC int host_fault_regs(void *osctx, HostFaultRegs *rc)
{
    ucontext_t *uc = (ucontext_t *)osctx;
    rc->sp = 0;
#if defined _WIN64 && defined __aarch64__
    rc->pc = uc->Pc;
    rc->fp = uc->Fp;
    rc->sp = uc->Sp;
#elif defined _WIN64
    rc->pc = uc->Rip;
    rc->fp = uc->Rbp;
    rc->sp = uc->Rsp;
#elif defined _WIN32
    rc->pc = uc->Eip;
    rc->fp = uc->Ebp;
    rc->sp = uc->Esp;
#elif defined __i386__
# if defined(__APPLE__)
    rc->pc = uc->uc_mcontext->__ss.__eip;
    rc->fp = uc->uc_mcontext->__ss.__ebp;
# elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
    rc->pc = uc->uc_mcontext.mc_eip;
    rc->fp = uc->uc_mcontext.mc_ebp;
# elif defined(__dietlibc__)
    rc->pc = uc->uc_mcontext.eip;
    rc->fp = uc->uc_mcontext.ebp;
# elif defined(__NetBSD__)
    rc->pc = uc->uc_mcontext.__gregs[_REG_EIP];
    rc->fp = uc->uc_mcontext.__gregs[_REG_EBP];
# elif defined(__OpenBSD__)
    rc->pc = uc->sc_eip;
    rc->fp = uc->sc_ebp;
# elif !defined REG_EIP && defined EIP
    rc->pc = uc->uc_mcontext.gregs[EIP];
    rc->fp = uc->uc_mcontext.gregs[EBP];
# else
    rc->pc = uc->uc_mcontext.gregs[REG_EIP];
    rc->fp = uc->uc_mcontext.gregs[REG_EBP];
# endif
#elif defined(__x86_64__)
# if defined(__APPLE__)
    rc->pc = uc->uc_mcontext->__ss.__rip;
    rc->fp = uc->uc_mcontext->__ss.__rbp;
# elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
    rc->pc = uc->uc_mcontext.mc_rip;
    rc->fp = uc->uc_mcontext.mc_rbp;
# elif defined(__NetBSD__)
    rc->pc = uc->uc_mcontext.__gregs[_REG_RIP];
    rc->fp = uc->uc_mcontext.__gregs[_REG_RBP];
# elif defined(__OpenBSD__)
    rc->pc = uc->sc_rip;
    rc->fp = uc->sc_rbp;
# else
    rc->pc = uc->uc_mcontext.gregs[REG_RIP];
    rc->fp = uc->uc_mcontext.gregs[REG_RBP];
# endif
#elif defined(__arm__) && defined(__NetBSD__)
    rc->pc = uc->uc_mcontext.__gregs[_REG_PC];
    rc->fp = uc->uc_mcontext.__gregs[_REG_FP];
#elif defined(__arm__) && defined(__OpenBSD__)
    rc->pc = uc->sc_pc;
    rc->fp = uc->sc_r11;
#elif defined(__arm__) && defined(__FreeBSD__)
    rc->pc = uc->uc_mcontext.__gregs[_REG_PC];
    rc->fp = uc->uc_mcontext.__gregs[_REG_FP];
#elif defined(__arm__)
    rc->pc = uc->uc_mcontext.arm_pc;
    rc->fp = uc->uc_mcontext.arm_fp;
#elif defined(__aarch64__) && defined(__APPLE__)
    rc->pc = uc->uc_mcontext->__ss.__pc;
    rc->fp = uc->uc_mcontext->__ss.__fp;
#elif defined(__aarch64__) && defined(__FreeBSD__)
    rc->pc = uc->uc_mcontext.mc_gpregs.gp_elr;
    rc->fp = uc->uc_mcontext.mc_gpregs.gp_x[29];
#elif defined(__aarch64__) && defined(__NetBSD__)
    rc->pc = uc->uc_mcontext.__gregs[_REG_PC];
    rc->fp = uc->uc_mcontext.__gregs[_REG_FP];
#elif defined(__aarch64__) && defined(__OpenBSD__)
    rc->pc = uc->sc_elr;
    rc->fp = uc->sc_x[29];
#elif defined(__aarch64__)
    rc->pc = uc->uc_mcontext.pc;
    rc->fp = uc->uc_mcontext.regs[29];
#elif defined(__riscv) && defined(__OpenBSD__)
    rc->pc = uc->sc_sepc;
    rc->fp = uc->sc_s[0];
#elif defined(__riscv)
    rc->pc = uc->uc_mcontext.__gregs[REG_PC];
    rc->fp = uc->uc_mcontext.__gregs[REG_S0];
#endif
    return 0;
}

static host_fault_fn host_fault_cb;

#ifndef _WIN32

static void host_sig_handler(int signum, siginfo_t *siginf, void *puc)
{
    HostFaultRegs r;
    int code;

    host_fault_regs(puc, &r);
    switch (signum) {
    case SIGFPE:
        switch (siginf->si_code) {
        case FPE_INTDIV:
        case FPE_FLTDIV:
            code = HOST_FAULT_DIVZERO;
            break;
        default:
            code = HOST_FAULT_FPE;
            break;
        }
        break;
    case SIGBUS:
    case SIGSEGV:
        code = HOST_FAULT_MEM;
        break;
    case SIGILL:
        code = HOST_FAULT_ILL;
        break;
    case SIGABRT:
        code = HOST_FAULT_ABORT;
        break;
    default:
        code = HOST_FAULT_OTHER;
        break;
    }
    host_fault_cb(code, signum, &r);
}

#ifndef SA_SIGINFO
# define SA_SIGINFO 0x00000004u
#endif

ST_FUNC void host_fault_install(host_fault_fn fn)
{
    struct sigaction sigact;

    host_fault_cb = fn;
    sigemptyset (&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO;
    sigact.sa_sigaction = host_sig_handler;
    sigaction(SIGFPE, &sigact, NULL);
    sigaction(SIGILL, &sigact, NULL);
    sigaction(SIGSEGV, &sigact, NULL);
    sigaction(SIGBUS, &sigact, NULL);
    sigaction(SIGABRT, &sigact, NULL);
}

ST_FUNC void host_fault_unblock(unsigned signum)
{
    sigset_t s;
    sigemptyset(&s);
    sigaddset(&s, (int)signum);
    sigprocmask(SIG_UNBLOCK, &s, NULL);
}

#else

static long __stdcall host_seh_handler(EXCEPTION_POINTERS *ex_info)
{
    HostFaultRegs r;
    unsigned code = ex_info->ExceptionRecord->ExceptionCode;
    int fc;

    host_fault_regs(ex_info->ContextRecord, &r);
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        fc = HOST_FAULT_MEM;
        break;
    case EXCEPTION_STACK_OVERFLOW:
        fc = HOST_FAULT_STACK;
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        fc = HOST_FAULT_DIVZERO;
        break;
    case EXCEPTION_BREAKPOINT:
    case EXCEPTION_SINGLE_STEP:
        fc = HOST_FAULT_TRAP;
        break;
    default:
        fc = HOST_FAULT_OTHER;
        break;
    }
    if (host_fault_cb(fc, code, &r))
        return EXCEPTION_CONTINUE_SEARCH;
    return EXCEPTION_EXECUTE_HANDLER;
}

ST_FUNC void host_fault_install(host_fault_fn fn)
{
    host_fault_cb = fn;
#ifdef _WIN64
    AddVectoredExceptionHandler(1, host_seh_handler);
#else
    SetUnhandledExceptionFilter(host_seh_handler);
#endif
}

ST_FUNC void host_fault_unblock(unsigned detail)
{
    (void)detail;
}

#endif

#endif /* MCC_IS_NATIVE && CONFIG_MCC_BACKTRACE */
