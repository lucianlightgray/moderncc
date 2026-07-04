#ifndef MCC_HOST_H
#define MCC_HOST_H

#ifdef _WIN32
#define MCC_HOST_WIN32 1
#else
#define MCC_HOST_WIN32 0
#endif

#ifdef _WIN64
#define MCC_HOST_WIN64 1
#else
#define MCC_HOST_WIN64 0
#endif

#ifdef __APPLE__
#define MCC_HOST_DARWIN 1
#else
#define MCC_HOST_DARWIN 0
#endif

#ifdef __linux__
#define MCC_HOST_LINUX 1
#else
#define MCC_HOST_LINUX 0
#endif

#if defined __FreeBSD__ || defined __FreeBSD_kernel__ || defined __NetBSD__ || defined __OpenBSD__ || defined __DragonFly__
#define MCC_HOST_BSD 1
#else
#define MCC_HOST_BSD 0
#endif

#define MCC_HOST_POSIX (!MCC_HOST_WIN32)

#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h>
#ifndef CONFIG_MCC_STATIC
#include <dlfcn.h>
#endif
extern float strtof(const char *__nptr, char **__endptr);
extern long double strtold(const char *__nptr, char **__endptr);
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x502
#endif
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <malloc.h>
#ifndef _MSC_VER
#include <stdint.h>
#endif
#define inline __inline
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#ifndef __GNUC__
#define strtold (long double)strtod
#define strtof (float)strtod
#define strtoll _strtoi64
#define strtoull _strtoui64
#endif
#ifdef LIBMCC_AS_DLL
#define LIBMCCAPI __declspec(dllexport)
#define PUB_FUNC LIBMCCAPI
#endif
#ifdef _MSC_VER
#pragma warning(disable : 4244)
#pragma warning(disable : 4267)
#pragma warning(disable : 4996)
#pragma warning(disable : 4018)
#pragma warning(disable : 4146)
#include <sys/types.h>
#define ssize_t intptr_t
#define strtok_r strtok_s
#ifdef _X86_
#define __i386__ 1
#endif
#ifdef _AMD64_
#define __x86_64__ 1
#endif
#endif
#if defined(_M_ARM64) && !defined(__aarch64__)
#define __aarch64__ 1
#endif
#ifndef va_copy
#define va_copy(a, b) a = b
#endif
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef offsetof
#ifdef __clang__
#define offsetof(type, field) __builtin_offsetof(type, field)
#else
#define offsetof(type, field) ((size_t)&((type *)0)->field)
#endif
#endif

#ifndef countof
#define countof(tab) (sizeof(tab) / sizeof((tab)[0]))
#endif

#ifdef _MSC_VER
#define NORETURN __declspec(noreturn)
#define ALIGNED(x) __declspec(align(x))
#define PRINTF_LIKE(x, y)
#define MAYBE_UNUSED
#else
#define NORETURN __attribute__((noreturn))
#define ALIGNED(x) __attribute__((aligned(x)))
#define PRINTF_LIKE(x, y) __attribute__((format(printf, (x), (y))))

#define MAYBE_UNUSED __attribute__((unused))
#endif

#if defined(__GNUC__) && __GNUC__ >= 7
#define FALLTHROUGH __attribute__((fallthrough))
#elif defined(__clang__) && (__clang_major__ >= 10)
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH ((void)0)
#endif

#if defined _MSC_VER && defined __x86_64__
#define HOST_VOLATILE_LDOUBLE volatile
#else
#define HOST_VOLATILE_LDOUBLE
#endif

#ifndef PUB_FUNC
#define PUB_FUNC
#endif

#ifndef ONE_SOURCE
#define ONE_SOURCE 0
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

#ifdef _WIN32
#define HOST_IS_DIRSEP(c) (c == '/' || c == '\\')
#define HOST_IS_ABSPATH(p) (HOST_IS_DIRSEP(p[0]) || (p[0] && p[1] == ':' && HOST_IS_DIRSEP(p[2])))
#define HOST_PATHCMP stricmp
#define HOST_PATHSEP ";"
#else
#define HOST_IS_DIRSEP(c) (c == '/')
#define HOST_IS_ABSPATH(p) HOST_IS_DIRSEP(p[0])
#define HOST_PATHCMP strcmp
#define HOST_PATHSEP ":"
#endif

#if !defined CONFIG_MCCDIR && !MCC_HOST_WIN32
#define CONFIG_MCCDIR "/usr/local/lib/mcc"
#endif

#if MCC_HOST_WIN32 && !defined CONFIG_MCCDIR
#define MCC_HOST_AUTO_MCCDIR_W32 1
ST_FUNC char *host_w32_mccdir(char *path);
#define CONFIG_MCCDIR host_w32_mccdir(alloca(MAX_PATH))
#endif

#ifdef _WIN32
#define HOST_EXE_SUFFIX ".exe"
#else
#define HOST_EXE_SUFFIX ""
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

ST_FUNC char *host_path_normalize(char *path);
ST_FUNC char *host_path_canonical(const char *path);
ST_FUNC int host_path_hash_fold(int c);
ST_FUNC FILE *host_fopen(const char *path, const char *mode);
ST_FUNC int host_fclose(FILE *f);
ST_FUNC void host_set_exec_bits(const char *file);

ST_FUNC int host_exe_path(char *buf, int size);

ST_FUNC int host_system_dir(char *buf, int size);

ST_FUNC int host_spawn_wait(const char *const *argv);
ST_FUNC int host_exec_replace(char **argv);
ST_FUNC int host_find_tool(const char *name, const char *ext, char *buf, int size);
ST_FUNC int host_codesign_adhoc(const char *file);

typedef struct HostSpawnOpts {
	const char *const *launcher;
	const char *cwd;
	const char *const *env;
	const char *stdout_file;
	const char *stderr_file;
	char **stdout_buf, **stderr_buf;
} HostSpawnOpts;
ST_FUNC int host_spawn_ex(const char *const *argv, const HostSpawnOpts *o);

ST_FUNC int host_find_tool_any(const char *const *names, const char *ext, char *buf, int size);

ST_FUNC int host_mkdirs(const char *path);
ST_FUNC int host_copy_file(const char *src, const char *dst, int preserve_exec);
ST_FUNC int host_stat(const char *path, int *is_dir, long long *size, long long *mtime);
typedef int (*host_walk_fn)(const char *path, int is_dir, void *ud);

ST_FUNC int host_dir_walk(const char *dir, int recursive, host_walk_fn fn, void *ud);

ST_FUNC unsigned host_clock_ms(void);
ST_FUNC char **host_environ(void);
ST_FUNC int host_nproc(void);

ST_FUNC void host_sys_info(char *sysname, int ssz, char *release, int rsz,
						   char *machine, int msz);

ST_FUNC void *host_dlopen(const char *name);
ST_FUNC void host_dlclose(void *h);
ST_FUNC const char *host_dlerror(void);
ST_FUNC void *host_dlsym(void *h, const char *symbol);
ST_FUNC void *host_dlsym_process(const char *symbol);

ST_FUNC const char *host_macos_sdk_root(void);
ST_FUNC const char *host_elf_interp_override(void);

#ifndef CONFIG_RUNMEM_RO
#define CONFIG_RUNMEM_RO MCC_HOST_DARWIN
#endif
#define HOST_RUNMEM_RO CONFIG_RUNMEM_RO

#define HOST_PROT_RX 0
#define HOST_PROT_RO 1
#define HOST_PROT_RW 2
#define HOST_PROT_RWX 3

#if MCC_HOST_WIN32
#define HOST_MPROTECT_FAILMSG "VirtualProtect failed"
#else
#define HOST_MPROTECT_FAILMSG "mprotect failed (did you mean to configure --with-selinux?)"
#endif

ST_FUNC size_t host_pagesize(void);

ST_FUNC void *host_runmem_alloc(unsigned *psize, int *ptr_diff);
ST_FUNC void host_runmem_free(void *ptr, unsigned size);
ST_FUNC int host_runmem_protect(void *ptr, unsigned long length, int mode);
ST_FUNC void host_icache_flush(void *ptr, unsigned long length);

ST_FUNC void *host_unwind_register(void *table, unsigned size_bytes, size_t base);
ST_FUNC void host_unwind_unregister(void *table);

enum {
	HOST_FAULT_MEM,
	HOST_FAULT_DIVZERO,
	HOST_FAULT_FPE,
	HOST_FAULT_ILL,
	HOST_FAULT_ABORT,
	HOST_FAULT_STACK,
	HOST_FAULT_TRAP,
	HOST_FAULT_OTHER
};

#if MCC_HOST_WIN32
#define HOST_FAULT_OTHER_FMT "caught exception %08x"
#else
#define HOST_FAULT_OTHER_FMT "caught signal %d"
#endif

typedef struct HostFaultRegs {
	size_t pc, fp, sp;
} HostFaultRegs;

typedef int (*host_fault_fn)(int code, unsigned detail, HostFaultRegs *r);

ST_FUNC void host_fault_install(host_fault_fn fn);
ST_FUNC int host_fault_regs(void *osctx, HostFaultRegs *r);
ST_FUNC void host_fault_unblock(unsigned detail);

#ifndef CONFIG_MCC_SEMLOCK
#define CONFIG_MCC_SEMLOCK 1
#endif

#if CONFIG_MCC_SEMLOCK
#if defined _WIN32
typedef struct {
	volatile LONG init;
	CRITICAL_SECTION cs;
} HostSem;
static inline void host_sem_wait(HostSem *p) {
	if (InterlockedCompareExchange(&p->init, 1, 0) == 0) {
		InitializeCriticalSection(&p->cs);
		InterlockedExchange(&p->init, 2);
	} else {
		while (InterlockedCompareExchange(&p->init, 2, 2) != 2)
			Sleep(0);
	}
	EnterCriticalSection(&p->cs);
}
static inline void host_sem_post(HostSem *p) {
	LeaveCriticalSection(&p->cs);
}
#elif defined __APPLE__
#include <dispatch/dispatch.h>
typedef struct {
	int init;
	dispatch_semaphore_t sem;
} HostSem;
static inline void host_sem_wait(HostSem *p) {
	if (!p->init)
		p->sem = dispatch_semaphore_create(1), p->init = 1;
	dispatch_semaphore_wait(p->sem, DISPATCH_TIME_FOREVER);
}
static inline void host_sem_post(HostSem *p) {
	dispatch_semaphore_signal(p->sem);
}
#else
#include <semaphore.h>
typedef struct {
	int init;
	sem_t sem;
} HostSem;
static inline void host_sem_wait(HostSem *p) {
	if (!p->init)
		sem_init(&p->sem, 0, 1), p->init = 1;
	while (sem_wait(&p->sem) < 0 && errno == EINTR)
		;
}
static inline void host_sem_post(HostSem *p) {
	sem_post(&p->sem);
}
#endif
#define HOST_SEM(s) static HostSem s
#define HOST_SEM_WAIT host_sem_wait
#define HOST_SEM_POST host_sem_post
#else
#define HOST_SEM(s)
#define HOST_SEM_WAIT(p)
#define HOST_SEM_POST(p)
#endif

#endif
