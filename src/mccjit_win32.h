#ifndef MCCJIT_WIN32_H
#define MCCJIT_WIN32_H

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif
#ifndef _CRT_NONSTDC_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS 1
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#ifndef NOGDI
#define NOGDI 1
#endif

#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#if !defined(__GNUC__) && !defined(__clang__)
#include <intrin.h>
#endif

#ifndef SRWLOCK_INIT
typedef struct _MCCJIT_SRWLOCK { PVOID Ptr; } SRWLOCK, *PSRWLOCK;
#define SRWLOCK_INIT {0}
WINBASEAPI VOID WINAPI InitializeSRWLock(PSRWLOCK);
WINBASEAPI VOID WINAPI AcquireSRWLockExclusive(PSRWLOCK);
WINBASEAPI VOID WINAPI ReleaseSRWLockExclusive(PSRWLOCK);
#endif
#ifndef CONDITION_VARIABLE_INIT
typedef struct _MCCJIT_CONDVAR { PVOID Ptr; } CONDITION_VARIABLE, *PCONDITION_VARIABLE;
#define CONDITION_VARIABLE_INIT {0}
WINBASEAPI VOID WINAPI InitializeConditionVariable(PCONDITION_VARIABLE);
WINBASEAPI BOOL WINAPI SleepConditionVariableSRW(PCONDITION_VARIABLE, PSRWLOCK,
																								 DWORD, ULONG);
WINBASEAPI VOID WINAPI WakeConditionVariable(PCONDITION_VARIABLE);
WINBASEAPI VOID WINAPI WakeAllConditionVariable(PCONDITION_VARIABLE);
#endif
#ifndef INIT_ONCE_STATIC_INIT
typedef union _MCCJIT_INIT_ONCE { PVOID Ptr; } INIT_ONCE, *PINIT_ONCE;
#define INIT_ONCE_STATIC_INIT {0}
WINBASEAPI BOOL WINAPI InitOnceExecuteOnce(PINIT_ONCE,
																					 BOOL(WINAPI *)(PINIT_ONCE, PVOID,
																													PVOID *),
																					 PVOID, LPVOID *);
#endif

#if defined(__x86_64__) || defined(_M_X64)
#ifndef MCCJIT_X64
#define MCCJIT_X64 1
#endif
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
#ifndef MCCJIT_ARM64
#define MCCJIT_ARM64 1
#endif
#endif

#define PROT_NONE 0x0
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4

#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)0)
#endif

#define MS_ASYNC 1
#define MS_SYNC 4
#define MS_INVALIDATE 2

#define MCCJIT_WIN32_MAP_SLOTS 64
typedef struct MccjitWin32MapEnt {
	void *base;
	HANDLE hmap;
	size_t len;
} MccjitWin32MapEnt;
static MccjitWin32MapEnt mccjit_win32_maps[MCCJIT_WIN32_MAP_SLOTS];
static SRWLOCK mccjit_win32_map_lock = SRWLOCK_INIT;

static void mccjit_win32_map_register(void *base, HANDLE hmap, size_t len) {
	int i;
	AcquireSRWLockExclusive(&mccjit_win32_map_lock);
	for (i = 0; i < MCCJIT_WIN32_MAP_SLOTS; i++) {
		if (!mccjit_win32_maps[i].base) {
			mccjit_win32_maps[i].base = base;
			mccjit_win32_maps[i].hmap = hmap;
			mccjit_win32_maps[i].len = len;
			break;
		}
	}
	ReleaseSRWLockExclusive(&mccjit_win32_map_lock);
}

static HANDLE mccjit_win32_map_take(void *base) {
	HANDLE h = INVALID_HANDLE_VALUE;
	int i;
	AcquireSRWLockExclusive(&mccjit_win32_map_lock);
	for (i = 0; i < MCCJIT_WIN32_MAP_SLOTS; i++) {
		if (mccjit_win32_maps[i].base == base) {
			h = mccjit_win32_maps[i].hmap;
			mccjit_win32_maps[i].base = NULL;
			mccjit_win32_maps[i].hmap = NULL;
			mccjit_win32_maps[i].len = 0;
			break;
		}
	}
	ReleaseSRWLockExclusive(&mccjit_win32_map_lock);
	return h;
}

static MAYBE_UNUSED void *mccjit_win32_mmap(void *addr, size_t len, int prot,
																						int flags, int fd, long long off) {
	(void)addr;
	if (flags & MAP_ANONYMOUS) {
		DWORD protect = (prot & PROT_EXEC) ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
		void *p = VirtualAlloc(NULL, len, MEM_RESERVE | MEM_COMMIT, protect);
		return p ? p : MAP_FAILED;
	} else {
		HANDLE fh = (HANDLE)_get_osfhandle(fd);
		DWORD protect = (prot & PROT_EXEC) ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
		DWORD access = FILE_MAP_READ | ((prot & PROT_WRITE) ? FILE_MAP_WRITE : 0) |
									 ((prot & PROT_EXEC) ? FILE_MAP_EXECUTE : 0);
		unsigned long long end = (unsigned long long)off + (unsigned long long)len;
		HANDLE hmap;
		void *view;
		if (fh == INVALID_HANDLE_VALUE)
			return MAP_FAILED;
		hmap = CreateFileMappingA(fh, NULL, protect, (DWORD)(end >> 32),
															(DWORD)(end & 0xffffffffu), NULL);
		if (!hmap)
			return MAP_FAILED;
		view = MapViewOfFile(hmap, access, (DWORD)((unsigned long long)off >> 32),
												 (DWORD)((unsigned long long)off & 0xffffffffu), len);
		if (!view) {
			CloseHandle(hmap);
			return MAP_FAILED;
		}
		mccjit_win32_map_register(view, hmap, len);
		return view;
	}
}

static MAYBE_UNUSED int mccjit_win32_munmap(void *addr, size_t len) {
	HANDLE hmap;
	(void)len;
	if (!addr)
		return 0;
	hmap = mccjit_win32_map_take(addr);
	if (hmap != INVALID_HANDLE_VALUE) {
		UnmapViewOfFile(addr);
		if (hmap)
			CloseHandle(hmap);
		return 0;
	}
	return VirtualFree(addr, 0, MEM_RELEASE) ? 0 : -1;
}

static MAYBE_UNUSED int mccjit_win32_msync(void *addr, size_t len, int flags) {
	(void)flags;
	return FlushViewOfFile(addr, len) ? 0 : -1;
}

#define mmap(a, l, p, f, fd, o) mccjit_win32_mmap((a), (l), (p), (f), (fd), (o))
#define munmap(a, l) mccjit_win32_munmap((a), (l))
#define msync(a, l, f) mccjit_win32_msync((a), (l), (f))

static MAYBE_UNUSED int mccjit_win32_open(const char *path, int flags, ...) {
	return _open(path, flags | _O_BINARY, _S_IREAD | _S_IWRITE);
}

static MAYBE_UNUSED ssize_t mccjit_win32_pread(int fd, void *buf, size_t count,
																							 long long off) {
	if (_lseeki64(fd, off, SEEK_SET) < 0)
		return -1;
	return _read(fd, buf, (unsigned)count);
}

static MAYBE_UNUSED int mccjit_win32_mkstemp(char *tmpl) {
	if (_mktemp_s(tmpl, strlen(tmpl) + 1) != 0)
		return -1;
	return _open(tmpl, _O_RDWR | _O_CREAT | _O_EXCL | _O_BINARY,
							 _S_IREAD | _S_IWRITE);
}

#define open(path, ...) mccjit_win32_open((path), __VA_ARGS__)
#define close(fd) _close(fd)
#define ftruncate(fd, len) _chsize_s((fd), (long long)(len))
#define pread(fd, buf, n, off) mccjit_win32_pread((fd), (buf), (n), (long long)(off))
#define mkstemp(tmpl) mccjit_win32_mkstemp(tmpl)

#ifndef O_RDWR
#define O_RDWR _O_RDWR
#endif
#ifndef O_CREAT
#define O_CREAT _O_CREAT
#endif

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

static MAYBE_UNUSED int mccjit_win32_clock_gettime(int id, struct timespec *ts) {
	LARGE_INTEGER freq, cnt;
	(void)id;
	if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0)
		return -1;
	QueryPerformanceCounter(&cnt);
	ts->tv_sec = (time_t)(cnt.QuadPart / freq.QuadPart);
	ts->tv_nsec =
			(long)(((cnt.QuadPart % freq.QuadPart) * 1000000000LL) / freq.QuadPart);
	return 0;
}

static MAYBE_UNUSED int mccjit_win32_nanosleep(const struct timespec *req,
																							 void *rem) {
	long long ns = (long long)req->tv_sec * 1000000000LL + req->tv_nsec;
	(void)rem;
	if (ns <= 0)
		return 0;
	if (ns < 2000000LL) {
		LARGE_INTEGER f, s, c;
		QueryPerformanceFrequency(&f);
		QueryPerformanceCounter(&s);
		for (;;) {
			SwitchToThread();
			QueryPerformanceCounter(&c);
			if ((c.QuadPart - s.QuadPart) * 1000000000LL / f.QuadPart >= ns)
				break;
		}
		return 0;
	}
	Sleep((DWORD)(ns / 1000000LL));
	return 0;
}

#define clock_gettime(clk, ts) mccjit_win32_clock_gettime((clk), (ts))
#define nanosleep(req, rem) mccjit_win32_nanosleep((req), (rem))

static MAYBE_UNUSED int mccjit_win32_setenv(const char *name, const char *val,
																						int overwrite) {
	(void)overwrite;
	return _putenv_s(name, val);
}
static MAYBE_UNUSED int mccjit_win32_unsetenv(const char *name) {
	return _putenv_s(name, "");
}
#define setenv(n, v, o) mccjit_win32_setenv((n), (v), (o))
#define unsetenv(n) mccjit_win32_unsetenv(n)

typedef SRWLOCK pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;
typedef INIT_ONCE pthread_once_t;
typedef uintptr_t pthread_t;

#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT
#define PTHREAD_COND_INITIALIZER CONDITION_VARIABLE_INIT
#define PTHREAD_ONCE_INIT INIT_ONCE_STATIC_INIT

static MAYBE_UNUSED int pthread_mutex_init(pthread_mutex_t *m, const void *a) {
	(void)a;
	InitializeSRWLock(m);
	return 0;
}
static MAYBE_UNUSED int pthread_mutex_destroy(pthread_mutex_t *m) {
	(void)m;
	return 0;
}
static MAYBE_UNUSED int pthread_mutex_lock(pthread_mutex_t *m) {
	AcquireSRWLockExclusive(m);
	return 0;
}
static MAYBE_UNUSED int pthread_mutex_unlock(pthread_mutex_t *m) {
	ReleaseSRWLockExclusive(m);
	return 0;
}

static MAYBE_UNUSED int pthread_cond_init(pthread_cond_t *c, const void *a) {
	(void)a;
	InitializeConditionVariable(c);
	return 0;
}
static MAYBE_UNUSED int pthread_cond_destroy(pthread_cond_t *c) {
	(void)c;
	return 0;
}
static MAYBE_UNUSED int pthread_cond_wait(pthread_cond_t *c,
																					pthread_mutex_t *m) {
	SleepConditionVariableSRW(c, m, INFINITE, 0);
	return 0;
}
static MAYBE_UNUSED int pthread_cond_signal(pthread_cond_t *c) {
	WakeConditionVariable(c);
	return 0;
}
static MAYBE_UNUSED int pthread_cond_broadcast(pthread_cond_t *c) {
	WakeAllConditionVariable(c);
	return 0;
}

static void (*mccjit_win32_once_fn)(void);
static BOOL CALLBACK mccjit_win32_once_tramp(PINIT_ONCE io, PVOID param,
																						 PVOID *ctx) {
	(void)io;
	(void)ctx;
	((void (*)(void))param)();
	return TRUE;
}
static MAYBE_UNUSED int pthread_once(pthread_once_t *o, void (*fn)(void)) {
	mccjit_win32_once_fn = fn;
	InitOnceExecuteOnce(o, mccjit_win32_once_tramp, (PVOID)(void *)fn, NULL);
	return 0;
}

struct mccjit_win32_thr {
	void *(*fn)(void *);
	void *arg;
};
static unsigned __stdcall mccjit_win32_thr_tramp(void *p) {
	struct mccjit_win32_thr s = *(struct mccjit_win32_thr *)p;
	HeapFree(GetProcessHeap(), 0, p);
	s.fn(s.arg);
	return 0;
}
static MAYBE_UNUSED int pthread_create(pthread_t *t, const void *attr,
																			 void *(*fn)(void *), void *arg) {
	struct mccjit_win32_thr *s;
	uintptr_t h;
	(void)attr;
	s = (struct mccjit_win32_thr *)HeapAlloc(GetProcessHeap(), 0, sizeof *s);
	if (!s)
		return -1;
	s->fn = fn;
	s->arg = arg;
	h = _beginthreadex(NULL, 0, mccjit_win32_thr_tramp, s, 0, NULL);
	if (!h) {
		HeapFree(GetProcessHeap(), 0, s);
		return -1;
	}
	*t = h;
	return 0;
}
static MAYBE_UNUSED int pthread_detach(pthread_t t) {
	CloseHandle((HANDLE)t);
	return 0;
}
static MAYBE_UNUSED int pthread_join(pthread_t t, void **ret) {
	WaitForSingleObject((HANDLE)t, INFINITE);
	if (ret)
		*ret = NULL;
	CloseHandle((HANDLE)t);
	return 0;
}

#if !defined(__GNUC__) && !defined(__clang__)
#define __ATOMIC_RELAXED 0
#define __ATOMIC_CONSUME 1
#define __ATOMIC_ACQUIRE 2
#define __ATOMIC_RELEASE 3
#define __ATOMIC_ACQ_REL 4
#define __ATOMIC_SEQ_CST 5
#define __atomic_load_n(p, mo) \
	((unsigned long long)_InterlockedOr64((volatile __int64 *)(p), 0))
#define __atomic_store_n(p, v, mo) \
	((void)_InterlockedExchange64((volatile __int64 *)(p), (__int64)(v)))
#define __atomic_add_fetch(p, v, mo)                                       \
	((unsigned long long)(_InterlockedExchangeAdd64((volatile __int64 *)(p), \
																									(__int64)(v)) +          \
												(__int64)(v)))
#endif

#endif
