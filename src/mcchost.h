/*
 *  mcchost.h - HOST-axis platform layer for the mcc compiler
 *
 *  Everything that depends on the machine mcc *runs on* (the HOST) is
 *  interpreted here and in mcchost.c; see HOST.md for the complete map.
 *  TARGET selection (MCC_TARGET_*, TARGETOS_*) stays in the backends.
 *
 *  Invariant: outside mcchost.{h,c}, no file under src/ may test raw host
 *  macros (_WIN32, __APPLE__, __linux__, the BSDs, _MSC_VER, ...).  Code
 *  that genuinely needs a host branch tests the normalized MCC_HOST_*
 *  predicates below or, preferably, calls a host_* function.
 */

#ifndef MCC_HOST_H
#define MCC_HOST_H

/* ------------------------------------------------------------------- */
/* normalized host identity - always defined, 0 or 1.  The ONLY place
   raw host macros are interpreted; everything else tests these. */

#ifdef _WIN32
# define MCC_HOST_WIN32 1
#else
# define MCC_HOST_WIN32 0
#endif

#ifdef _WIN64
# define MCC_HOST_WIN64 1
#else
# define MCC_HOST_WIN64 0
#endif

#ifdef __APPLE__
# define MCC_HOST_DARWIN 1
#else
# define MCC_HOST_DARWIN 0
#endif

#ifdef __linux__
# define MCC_HOST_LINUX 1
#else
# define MCC_HOST_LINUX 0
#endif

#if defined __FreeBSD__ || defined __FreeBSD_kernel__ || defined __NetBSD__ \
    || defined __OpenBSD__ || defined __DragonFly__
# define MCC_HOST_BSD 1
#else
# define MCC_HOST_BSD 0
#endif

#define MCC_HOST_POSIX (!MCC_HOST_WIN32)

/* ------------------------------------------------------------------- */
/* host system headers and compiler shims */

#ifndef _WIN32
# include <unistd.h>
# include <sys/time.h>
# ifndef CONFIG_MCC_STATIC
#  include <dlfcn.h>
# endif
extern float strtof (const char *__nptr, char **__endptr);
extern long double strtold (const char *__nptr, char **__endptr);
#endif

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN 1
# ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x502
# endif
# include <windows.h>
# include <io.h>
# include <direct.h>
# include <malloc.h>
# ifndef _MSC_VER
#  include <stdint.h>
# endif
# define inline __inline
# define snprintf _snprintf
# define vsnprintf _vsnprintf
# ifndef __GNUC__
#  define strtold (long double)strtod
#  define strtof (float)strtod
#  define strtoll _strtoi64
#  define strtoull _strtoui64
# endif
# ifdef LIBMCC_AS_DLL
#  define LIBMCCAPI __declspec(dllexport)
#  define PUB_FUNC LIBMCCAPI
# endif
# ifdef _MSC_VER
#  pragma warning (disable : 4244)
#  pragma warning (disable : 4267)
#  pragma warning (disable : 4996)
#  pragma warning (disable : 4018)
#  pragma warning (disable : 4146)
#  include <sys/types.h>
#  define ssize_t intptr_t
#  ifdef _X86_
#   define __i386__ 1
#  endif
#  ifdef _AMD64_
#   define __x86_64__ 1
#  endif
# endif
# if defined(_M_ARM64) && !defined(__aarch64__)
#  define __aarch64__ 1
# endif
# ifndef va_copy
#  define va_copy(a,b) a = b
# endif
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif

#ifndef offsetof
#ifdef __clang__
#define offsetof(type, field) __builtin_offsetof(type, field)
#else
#define offsetof(type, field) ((size_t) &((type *)0)->field)
#endif
#endif

#ifndef countof
#define countof(tab) (sizeof(tab) / sizeof((tab)[0]))
#endif

#ifdef _MSC_VER
# define NORETURN __declspec(noreturn)
# define ALIGNED(x) __declspec(align(x))
# define PRINTF_LIKE(x,y)
#else
# define NORETURN __attribute__((noreturn))
# define ALIGNED(x) __attribute__((aligned(x)))
# define PRINTF_LIKE(x,y) __attribute__ ((format (printf, (x), (y))))
#endif

#if defined(__GNUC__) && __GNUC__ >= 7
# define FALLTHROUGH __attribute__((fallthrough))
#elif defined(__clang__) && (__clang_major__ >= 10)
# define FALLTHROUGH __attribute__((fallthrough))
#else
# define FALLTHROUGH ((void)0)
#endif

/* MSVC x64 evaluates long double compile-time arithmetic in SSE double
   precision unless the operands are forced through memory */
#if defined _MSC_VER && defined __x86_64__
# define HOST_VOLATILE_LDOUBLE volatile
#else
# define HOST_VOLATILE_LDOUBLE
#endif

/* ------------------------------------------------------------------- */
/* linkage shims (needed below for the host_* prototypes) */

#ifndef PUB_FUNC
# define PUB_FUNC
#endif

#ifndef ONE_SOURCE
# define ONE_SOURCE 0
#endif

#if ONE_SOURCE
#define ST_INLN static inline
#define ST_FUNC static
#define ST_DATA static
#else
#define ST_INLN
#define ST_FUNC
#define ST_DATA extern
#endif

/* ------------------------------------------------------------------- */
/* filesystem & path semantics */

#ifdef _WIN32
# define HOST_IS_DIRSEP(c) (c == '/' || c == '\\')
# define HOST_IS_ABSPATH(p) (HOST_IS_DIRSEP(p[0]) || (p[0] && p[1] == ':' && HOST_IS_DIRSEP(p[2])))
# define HOST_PATHCMP stricmp
# define HOST_PATHSEP ";"
#else
# define HOST_IS_DIRSEP(c) (c == '/')
# define HOST_IS_ABSPATH(p) HOST_IS_DIRSEP(p[0])
# define HOST_PATHCMP strcmp
# define HOST_PATHSEP ":"
#endif

#if !defined CONFIG_MCCDIR && !MCC_HOST_WIN32
# define CONFIG_MCCDIR "/usr/local/lib/mcc"
#endif

/* On Windows without a configured CONFIG_MCCDIR, mccdir is the directory
   of the running mcc image (exe, or libmcc DLL - see DllMain) */
#if MCC_HOST_WIN32 && !defined CONFIG_MCCDIR
# define MCC_HOST_AUTO_MCCDIR_W32 1
ST_FUNC char *host_w32_mccdir(char *path); /* needs MAX_PATH bytes */
# define CONFIG_MCCDIR host_w32_mccdir(alloca(MAX_PATH))
#endif

#ifdef _WIN32
# define HOST_EXE_SUFFIX ".exe"
#else
# define HOST_EXE_SUFFIX ""
# define stricmp strcasecmp
# define strnicmp strncasecmp
#endif

ST_FUNC char *host_path_normalize(char *path);       /* '\'->'/'; no-op POSIX */
ST_FUNC char *host_path_canonical(const char *path); /* realpath/_fullpath, libc-malloc'd */
ST_FUNC int   host_path_hash_fold(int c);            /* include-cache case fold */
ST_FUNC FILE *host_fopen(const char *path, const char *mode);
ST_FUNC int   host_fclose(FILE *f);
ST_FUNC void  host_set_exec_bits(const char *file);  /* chmod 0777; no-op Win */

/* ------------------------------------------------------------------- */
/* self location */

/* path of the running mcc image (exe, or libmcc DLL); slash-normalized.
   Returns the length, or -1 if it cannot be determined */
ST_FUNC int host_exe_path(char *buf, int size);
/* Windows system directory; -1 on other hosts */
ST_FUNC int host_system_dir(char *buf, int size);

/* ------------------------------------------------------------------- */
/* process spawn / host tools */

ST_FUNC int host_spawn_wait(const char *const *argv);
ST_FUNC int host_exec_replace(char **argv);
ST_FUNC int host_find_tool(const char *name, const char *ext, char *buf, int size);
ST_FUNC int host_codesign_adhoc(const char *file);

/* ------------------------------------------------------------------- */
/* time / env */

ST_FUNC unsigned host_clock_ms(void);
ST_FUNC char   **host_environ(void);

/* ------------------------------------------------------------------- */
/* dynamic loading (CONFIG_MCC_STATIC builds get built-in-table stubs) */

ST_FUNC void       *host_dlopen(const char *name);
ST_FUNC void        host_dlclose(void *h);
ST_FUNC const char *host_dlerror(void);
ST_FUNC void       *host_dlsym(void *h, const char *symbol);
ST_FUNC void       *host_dlsym_process(const char *symbol); /* RTLD_DEFAULT */

/* ------------------------------------------------------------------- */
/* native toolchain services */

ST_FUNC const char *host_macos_sdk_root(void);      /* NULL if unknown */
ST_FUNC const char *host_elf_interp_override(void); /* getenv("LD_SO") */

/* ------------------------------------------------------------------- */
/* runnable memory / JIT (compiled only under MCC_IS_NATIVE) */

/* Darwin enforces W^X: writes to runnable memory need a protection
   toggle.  Overridable the old way via CONFIG_RUNMEM_RO. */
#ifndef CONFIG_RUNMEM_RO
# define CONFIG_RUNMEM_RO MCC_HOST_DARWIN
#endif
#define HOST_RUNMEM_RO CONFIG_RUNMEM_RO

/* protection modes; the numeric values are load-bearing (mcc_relocate_ex
   maps its section class k directly onto them) */
#define HOST_PROT_RX  0
#define HOST_PROT_RO  1
#define HOST_PROT_RW  2
#define HOST_PROT_RWX 3

#if MCC_HOST_WIN32
# define HOST_MPROTECT_FAILMSG "VirtualProtect failed"
#else
# define HOST_MPROTECT_FAILMSG "mprotect failed (did you mean to configure --with-selinux?)"
#endif

ST_FUNC size_t host_pagesize(void);
/* allocate *psize bytes of to-be-runnable memory; may grow *psize (page
   slack, SELinux double mapping).  *ptr_diff receives the offset of the
   RW view from the RX view (SELinux), else 0.  NULL on failure */
ST_FUNC void *host_runmem_alloc(unsigned *psize, int *ptr_diff);
ST_FUNC void  host_runmem_free(void *ptr, unsigned size);
ST_FUNC int   host_runmem_protect(void *ptr, unsigned long length, int mode);
ST_FUNC void  host_icache_flush(void *ptr, unsigned long length);

/* Win64 SEH unwind tables; no-ops elsewhere.  `table` is the .pdata
   payload (RUNTIME_FUNCTION array), `size_bytes` its byte length.
   Returns a handle for host_unwind_unregister, NULL on failure */
ST_FUNC void *host_unwind_register(void *table, unsigned size_bytes, size_t base);
ST_FUNC void  host_unwind_unregister(void *table);

/* ------------------------------------------------------------------- */
/* faults / signal context (MCC_IS_NATIVE + CONFIG_MCC_BACKTRACE) */

enum {
    HOST_FAULT_MEM,      /* invalid memory access */
    HOST_FAULT_DIVZERO,  /* integer/float division by zero */
    HOST_FAULT_FPE,      /* other floating point exception (POSIX) */
    HOST_FAULT_ILL,      /* illegal instruction */
    HOST_FAULT_ABORT,    /* abort() (POSIX) */
    HOST_FAULT_STACK,    /* stack overflow (Windows) */
    HOST_FAULT_TRAP,     /* breakpoint / single-step (Windows) */
    HOST_FAULT_OTHER     /* anything else; see HOST_FAULT_OTHER_FMT */
};

#if MCC_HOST_WIN32
# define HOST_FAULT_OTHER_FMT "caught exception %08x"
#else
# define HOST_FAULT_OTHER_FMT "caught signal %d"
#endif

typedef struct HostFaultRegs { size_t pc, fp, sp; } HostFaultRegs;

/* fault callback: `detail` is the raw signal number / exception code
   (for HOST_FAULT_OTHER_FMT and host_fault_unblock).  Return nonzero
   to continue the OS handler search (Windows TRAP), otherwise do not
   return (longjmp/exit) or the process terminates */
typedef int (*host_fault_fn)(int code, unsigned detail, HostFaultRegs *r);

ST_FUNC void host_fault_install(host_fault_fn fn); /* sigaction | SEH */
ST_FUNC int  host_fault_regs(void *osctx, HostFaultRegs *r);
ST_FUNC void host_fault_unblock(unsigned detail);  /* sigprocmask; no-op Win */

/* ------------------------------------------------------------------- */
/* locks: MCCSem - CRITICAL_SECTION | dispatch_semaphore_t | sem_t */

#ifndef CONFIG_MCC_SEMLOCK
# define CONFIG_MCC_SEMLOCK 1
#endif

#if CONFIG_MCC_SEMLOCK
#if defined _WIN32
typedef struct { volatile LONG init; CRITICAL_SECTION cs; } MCCSem;
static inline void wait_sem(MCCSem *p) {
    if (InterlockedCompareExchange(&p->init, 1, 0) == 0) {
        InitializeCriticalSection(&p->cs);
        InterlockedExchange(&p->init, 2);
    } else {
        while (InterlockedCompareExchange(&p->init, 2, 2) != 2)
            Sleep(0);
    }
    EnterCriticalSection(&p->cs);
}
static inline void post_sem(MCCSem *p) {
    LeaveCriticalSection(&p->cs);
}
#elif defined __APPLE__
#include <dispatch/dispatch.h>
typedef struct { int init; dispatch_semaphore_t sem; } MCCSem;
static inline void wait_sem(MCCSem *p) {
    if (!p->init)
        p->sem = dispatch_semaphore_create(1), p->init = 1;
    dispatch_semaphore_wait(p->sem, DISPATCH_TIME_FOREVER);
}
static inline void post_sem(MCCSem *p) {
    dispatch_semaphore_signal(p->sem);
}
#else
#include <semaphore.h>
typedef struct { int init; sem_t sem; } MCCSem;
static inline void wait_sem(MCCSem *p) {
    if (!p->init)
        sem_init(&p->sem, 0, 1), p->init = 1;
    while (sem_wait(&p->sem) < 0 && errno == EINTR);
}
static inline void post_sem(MCCSem *p) {
    sem_post(&p->sem);
}
#endif
#define MCC_SEM(s) static MCCSem s
#define WAIT_SEM wait_sem
#define POST_SEM post_sem
#else
#define MCC_SEM(s)
#define WAIT_SEM(p)
#define POST_SEM(p)
#endif

#endif /* MCC_HOST_H */
