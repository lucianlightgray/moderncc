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

#ifdef __GNU__
#define MCC_HOST_HURD 1
#else
#define MCC_HOST_HURD 0
#endif

#define MCC_HOST_POSIX (!MCC_HOST_WIN32)

#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h>
#ifndef MCC_CONFIG_STATIC
#include <dlfcn.h>
#endif
extern float strtof(const char *__nptr, char **__endptr);
extern long double strtold(const char *__nptr, char **__endptr);
#endif

#include <errno.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
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
/* Include <stdlib.h> first so the CRT's own strtof/strtold/strtoll/strtoull
 * declarations are processed before these compatibility macros redefine the
 * names; otherwise a later <stdlib.h> (e.g. via mccgpu.h in the slicerun/libmcc
 * builds) is mangled -- 'float strtof(' becomes 'float (float)strtod(' and the
 * ucrt header fails to compile under MSVC. Behaviour is unchanged: mcc's own
 * strtof/strtold calls still route through strtod as before. */
#include <stdlib.h>
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
#if defined(__MCC__) && !defined(_MSC_VER)
#define strtok_r strtok_s
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
#define MCC_THREAD_LOCAL __declspec(thread)
#else
#define NORETURN __attribute__((noreturn))
#define ALIGNED(x) __attribute__((aligned(x)))
#define PRINTF_LIKE(x, y) __attribute__((format(printf, (x), (y))))

#define MAYBE_UNUSED __attribute__((unused))
#define MCC_THREAD_LOCAL _Thread_local
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

#ifndef MCC_AMALGAMATED
#define MCC_AMALGAMATED 0
#endif

#if MCC_AMALGAMATED
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

#if !defined MCC_CONFIG_MCCDIR && !MCC_HOST_WIN32
#define MCC_CONFIG_MCCDIR "/usr/local/lib/mcc"
#endif

#if MCC_HOST_WIN32 && !defined MCC_CONFIG_MCCDIR
#define MCC_HOST_AUTO_MCCDIR_W32 1
ST_FUNC char *host_w32_mccdir(char *path);
#define MCC_CONFIG_MCCDIR host_w32_mccdir(alloca(MAX_PATH))
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
ST_FUNC int host_stderr_isatty(void);

ST_FUNC int host_exe_path(char *buf, int size);
ST_FUNC FILE *host_temp_c_file(char *path, int size);
ST_FUNC int host_temp_file(char *path, int size);

ST_FUNC int host_system_dir(char *buf, int size);

ST_FUNC int host_spawn_wait(const char *const *argv);
ST_FUNC int host_exec_replace(char **argv);
ST_FUNC int host_find_tool(const char *name, const char *ext, char *buf, int size);

typedef struct HostSpawnOpts {
	const char *const *launcher;
	const char *cwd;
	const char *const *env;
	const char *stdout_file;
	const char *stderr_file;
	char **stdout_buf, **stderr_buf;
	unsigned timeout_ms;
} HostSpawnOpts;

#define HOST_SPAWN_TIMEOUT 124

ST_FUNC int host_spawn_ex(const char *const *argv, const HostSpawnOpts *o);

ST_FUNC MAYBE_UNUSED int host_spawn_timeout(const char *const *cv, unsigned timeout_ms,
																						const volatile int *stop);
ST_FUNC MAYBE_UNUSED int host_spawn_retry(const char *const *cv, unsigned timeout_ms,
																					int tries, const volatile int *stop);
ST_FUNC MAYBE_UNUSED int host_spawn_run(const char *const *cv, unsigned timeout_ms,
																				long *usec, long *rss_kb, const volatile int *stop);

ST_FUNC int host_find_tool_any(const char *const *names, const char *ext, char *buf, int size);

ST_FUNC MAYBE_UNUSED void host_install_interrupt(void (*fn)(int));
ST_FUNC MAYBE_UNUSED int host_setenv(const char *name, const char *val);
ST_FUNC MAYBE_UNUSED int host_unsetenv(const char *name);
ST_FUNC MAYBE_UNUSED void *host_file_lock(const char *path);
ST_FUNC MAYBE_UNUSED void host_file_unlock(void *h);
ST_FUNC MAYBE_UNUSED void host_fsync(FILE *f);
ST_FUNC MAYBE_UNUSED int host_rename(const char *src, const char *dst);

ST_FUNC int host_mkdirs(const char *path);
ST_FUNC int host_cache_dir(char *buf, int size);
ST_FUNC int host_rmrf(const char *path);
ST_FUNC int host_copy_file(const char *src, const char *dst, int preserve_exec);
ST_FUNC int host_stat(const char *path, int *is_dir, long long *size, long long *mtime);
typedef int (*host_walk_fn)(const char *path, int is_dir, void *ud);

ST_FUNC int host_dir_walk(const char *dir, int recursive, host_walk_fn fn, void *ud);

ST_FUNC unsigned host_clock_ms(void);
ST_FUNC MAYBE_UNUSED char **host_environ(void);
ST_FUNC int host_nproc(void);
ST_FUNC double host_loadavg(void);

ST_FUNC void host_sys_info(char *sysname, int ssz, char *release, int rsz,
													 char *machine, int msz);

ST_FUNC void *host_dlopen(const char *name);
ST_FUNC MAYBE_UNUSED void host_dlclose(void *h);
ST_FUNC const char *host_dlerror(void);
ST_FUNC void *host_dlsym(void *h, const char *symbol);
ST_FUNC MAYBE_UNUSED void *host_dlsym_process(const char *symbol);

ST_FUNC const char *host_macos_sdk_root(void);
ST_FUNC const char *host_elf_interp_override(void);

#ifndef MCC_CONFIG_RUNMEM_RO
#define MCC_CONFIG_RUNMEM_RO MCC_HOST_DARWIN
#endif
#define HOST_RUNMEM_RO MCC_CONFIG_RUNMEM_RO

#define HOST_PROT_RX 0
#define HOST_PROT_RO 1
#define HOST_PROT_RW 2
#define HOST_PROT_RWX 3

#if MCC_HOST_WIN32
#define HOST_MPROTECT_FAILMSG "VirtualProtect failed"
#else
#define HOST_MPROTECT_FAILMSG "mprotect failed"
#endif

ST_FUNC MAYBE_UNUSED size_t host_pagesize(void);

ST_FUNC MAYBE_UNUSED int host_runmem_dual(void);
ST_FUNC MAYBE_UNUSED void *host_runmem_alloc(unsigned *psize, int *ptr_diff);
ST_FUNC MAYBE_UNUSED void host_runmem_free(void *ptr, unsigned size);
ST_FUNC MAYBE_UNUSED int host_runmem_protect(void *ptr, unsigned long length, int mode);
ST_FUNC MAYBE_UNUSED void host_icache_flush(void *ptr, unsigned long length);

#if defined(__linux__)
ST_FUNC MAYBE_UNUSED unsigned char *host_run_tls_slab_base(void);
ST_FUNC MAYBE_UNUSED unsigned long host_run_tls_slab_size(void);
ST_FUNC MAYBE_UNUSED unsigned long host_run_tls_slab_tpoff(void);
#endif

#if defined(_WIN32) && defined(MCC_TARGET_PE)
ST_FUNC MAYBE_UNUSED unsigned char *host_run_tls_slab_base(void);
ST_FUNC MAYBE_UNUSED unsigned long host_run_tls_slab_size(void);
ST_FUNC MAYBE_UNUSED unsigned long host_run_tls_slab_tpoff(void);
ST_FUNC MAYBE_UNUSED unsigned long host_run_tls_index(void);
#endif

ST_FUNC MAYBE_UNUSED void *host_unwind_register(void *table, unsigned size_bytes, size_t base);
ST_FUNC MAYBE_UNUSED void host_unwind_unregister(void *table);

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

#ifndef MCC_CONFIG_SEMLOCK
#define MCC_CONFIG_SEMLOCK 1
#endif

#if MCC_CONFIG_SEMLOCK
#if defined _WIN32
typedef struct
{
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
typedef struct
{
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

typedef struct
{
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

#include <stdlib.h>

static inline int mcc_env_on(const char *name) {
	const char *e = getenv(name);
	return e && e[0] && e[0] != '0';
}

static inline int mcc_env_flag(const char *name, int dflt) {
	const char *e = getenv(name);
	if (!e || !e[0])
		return dflt;
	return e[0] != '0';
}

static inline long mcc_env_num(const char *name, long dflt) {
	const char *e = getenv(name);
	long v;
	if (!e || !e[0])
		return dflt;
	v = strtol(e, NULL, 10);
	return v > 0 ? v : dflt;
}

static inline unsigned mcc_env_count(const char *name, unsigned dflt) {
	const char *e = getenv(name);
	long v;
	if (!e || !e[0])
		return dflt;
	v = strtol(e, NULL, 10);
	if (v < 0)
		return dflt;
	return (unsigned)v;
}

static inline unsigned mcc_search_ticks(unsigned dflt) {
	return mcc_env_count("MCC_SEARCH_TICKS", dflt);
}

static inline unsigned mcc_search_cap_ms(void) {
	return mcc_env_count("MCC_SEARCH_CAP_MS", 0u);
}

static inline void mcc_search_cap_notice(const char *where, unsigned elapsed_ms,
																				 unsigned cap_ms) {
	static int said;
	if (said)
		return;
	said = 1;
	fprintf(stderr,
					"mcc: warning: opt-search wall-clock cap fired in %s after %u ms "
					"(MCC_SEARCH_CAP_MS=%u); the search was cut where the clock landed, so "
					"this object is not reproducible. Raise or clear MCC_SEARCH_CAP_MS to "
					"get deterministic output.\n",
					where, elapsed_ms, cap_ms);
}

#endif
