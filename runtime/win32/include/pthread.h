#ifndef _PTHREAD_H
#define _PTHREAD_H

#include <process.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

typedef struct
{
	void *__ptr;
} __mcc_srwlock;

typedef struct
{
	void *__ptr;
} __mcc_condvar;

typedef struct
{
	void *__ptr;
} __mcc_initonce;

unsigned long __stdcall WaitForSingleObject(void *__h, unsigned long __ms);
int __stdcall CloseHandle(void *__h);
unsigned long __stdcall GetCurrentThreadId(void);
void __stdcall AcquireSRWLockExclusive(__mcc_srwlock *);
void __stdcall ReleaseSRWLockExclusive(__mcc_srwlock *);
unsigned char __stdcall TryAcquireSRWLockExclusive(__mcc_srwlock *);
void __stdcall InitializeConditionVariable(__mcc_condvar *);
void __stdcall WakeConditionVariable(__mcc_condvar *);
void __stdcall WakeAllConditionVariable(__mcc_condvar *);
int __stdcall SleepConditionVariableSRW(__mcc_condvar *, __mcc_srwlock *,
																				unsigned long, unsigned long);
int __stdcall InitOnceExecuteOnce(__mcc_initonce *,
																	int(__stdcall *)(__mcc_initonce *, void *, void **),
																	void *, void **);
unsigned long __stdcall TlsAlloc(void);
void *__stdcall TlsGetValue(unsigned long);
int __stdcall TlsSetValue(unsigned long, void *);
int __stdcall TlsFree(unsigned long);
void __stdcall Sleep(unsigned long);

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#endif
static inline int nanosleep(const struct timespec *__req, struct timespec *__rem) {
	(void)__rem;
	Sleep((unsigned long)(__req->tv_sec * 1000 + __req->tv_nsec / 1000000));
	return 0;
}

static inline int clock_gettime(int __clk, struct timespec *__ts) {
	(void)__clk;
	__ts->tv_sec = time((time_t *)0);
	__ts->tv_nsec = 0;
	return 0;
}

typedef struct __mcc_pthread {
	void *__handle;
	void *(*__start)(void *);
	void *__arg;
	void *__ret;
} *pthread_t;

typedef struct
{
	int __unused;
} pthread_attr_t;

static unsigned __stdcall __mcc_pthread_trampoline(void *__p) {
	pthread_t __t = (pthread_t)__p;
	__t->__ret = __t->__start(__t->__arg);
	return 0;
}

static inline int pthread_create(pthread_t *__t, const pthread_attr_t *__attr,
																 void *(*__start)(void *), void *__arg) {
	pthread_t __c;
	(void)__attr;
	__c = (pthread_t)malloc(sizeof(*__c));
	if (!__c)
		return EAGAIN;
	__c->__start = __start;
	__c->__arg = __arg;
	__c->__ret = (void *)0;
	__c->__handle =
			(void *)_beginthreadex(0, 0, __mcc_pthread_trampoline, __c, 0, 0);
	if (!__c->__handle) {
		free(__c);
		return EAGAIN;
	}
	*__t = __c;
	return 0;
}

static inline int pthread_join(pthread_t __t, void **__retval) {
	WaitForSingleObject(__t->__handle, 0xffffffffu);
	if (__retval)
		*__retval = __t->__ret;
	CloseHandle(__t->__handle);
	free(__t);
	return 0;
}

static inline int pthread_detach(pthread_t __t) {
	CloseHandle(__t->__handle);
	return 0;
}

static inline void pthread_exit(void *__ret) {
	_endthreadex((unsigned)(__UINTPTR_TYPE__)__ret);
}

static inline pthread_t pthread_self(void) {
	return (pthread_t)(__UINTPTR_TYPE__)GetCurrentThreadId();
}

static inline int pthread_equal(pthread_t __a, pthread_t __b) {
	return __a == __b;
}

typedef __mcc_srwlock pthread_mutex_t;

typedef struct
{
	int __type;
} pthread_mutexattr_t;

#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_INITIALIZER {0}

static inline int pthread_mutexattr_init(pthread_mutexattr_t *__a) {
	__a->__type = 0;
	return 0;
}

static inline int pthread_mutexattr_destroy(pthread_mutexattr_t *__a) {
	(void)__a;
	return 0;
}

static inline int pthread_mutexattr_settype(pthread_mutexattr_t *__a, int __t) {
	__a->__type = __t;
	return 0;
}

static inline int pthread_mutex_init(pthread_mutex_t *__m, const pthread_mutexattr_t *__a) {
	(void)__a;
	__m->__ptr = 0;
	return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t *__m) {
	AcquireSRWLockExclusive(__m);
	return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *__m) {
	ReleaseSRWLockExclusive(__m);
	return 0;
}

static inline int pthread_mutex_trylock(pthread_mutex_t *__m) {
	return TryAcquireSRWLockExclusive(__m) ? 0 : EBUSY;
}

static inline int pthread_mutex_destroy(pthread_mutex_t *__m) {
	(void)__m;
	return 0;
}

typedef __mcc_condvar pthread_cond_t;

typedef struct
{
	int __unused;
} pthread_condattr_t;

#define PTHREAD_COND_INITIALIZER {0}

static inline int pthread_cond_init(pthread_cond_t *__c, const pthread_condattr_t *__a) {
	(void)__a;
	__c->__ptr = 0;
	InitializeConditionVariable(__c);
	return 0;
}

static inline int pthread_cond_signal(pthread_cond_t *__c) {
	WakeConditionVariable(__c);
	return 0;
}

static inline int pthread_cond_broadcast(pthread_cond_t *__c) {
	WakeAllConditionVariable(__c);
	return 0;
}

static inline int pthread_cond_wait(pthread_cond_t *__c, pthread_mutex_t *__m) {
	return SleepConditionVariableSRW(__c, __m, 0xffffffffu, 0) ? 0 : EINVAL;
}

static inline int pthread_cond_timedwait(pthread_cond_t *__c, pthread_mutex_t *__m,
																				 const struct timespec *__abstime) {
	struct timespec __now;
	long long __ms;
	clock_gettime(CLOCK_REALTIME, &__now);
	__ms = (long long)(__abstime->tv_sec - __now.tv_sec) * 1000 +
				 (__abstime->tv_nsec - __now.tv_nsec) / 1000000;
	if (__ms < 0)
		__ms = 0;
	if (SleepConditionVariableSRW(__c, __m, (unsigned long)__ms, 0))
		return 0;
	return ETIMEDOUT;
}

static inline int pthread_cond_destroy(pthread_cond_t *__c) {
	(void)__c;
	return 0;
}

typedef unsigned long pthread_key_t;

static inline int pthread_key_create(pthread_key_t *__k, void (*__dtor)(void *)) {
	(void)__dtor;
	*__k = TlsAlloc();
	return *__k == 0xffffffffu ? EAGAIN : 0;
}

static inline int pthread_key_delete(pthread_key_t __k) {
	TlsFree(__k);
	return 0;
}

static inline void *pthread_getspecific(pthread_key_t __k) {
	return TlsGetValue(__k);
}

static inline int pthread_setspecific(pthread_key_t __k, const void *__v) {
	return TlsSetValue(__k, (void *)__v) ? 0 : ENOMEM;
}

typedef __mcc_initonce pthread_once_t;
#define PTHREAD_ONCE_INIT {0}

static int __stdcall __mcc_once_tramp(__mcc_initonce *__io, void *__param, void **__ctx) {
	(void)__io;
	(void)__ctx;
	((void (*)(void))__param)();
	return 1;
}

static inline int pthread_once(pthread_once_t *__once, void (*__func)(void)) {
	InitOnceExecuteOnce(__once, __mcc_once_tramp, (void *)__func, 0);
	return 0;
}

#endif
