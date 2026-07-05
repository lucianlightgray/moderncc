#ifndef _PTHREAD_H
#define _PTHREAD_H

/*
 * Minimal <pthread.h> for the mcc WIN32/PE target.
 *
 * msvcrt ships no POSIX threads, so this maps the small pthread subset the
 * exec/embed test harness needs -- thread creation and joining, propagating
 * the started routine's void* return value -- onto the Win32 thread API.
 * Threads are spawned with msvcrt's _beginthreadex (not raw CreateThread) so
 * that per-thread C runtime state is initialised and printf/malloc are safe
 * inside the thread; join waits with WaitForSingleObject.
 *
 * This is deliberately NOT a full pthreads implementation: no mutexes,
 * condition variables, TLS keys, cancellation, or thread attributes beyond an
 * ignored placeholder.  It exists so that thread-using conformance programs
 * (TLS codegen, C11 atomics under contention) can be exercised on the PE
 * target instead of being skipped for lack of a threads header.
 */

#include <process.h>
#include <stdlib.h>

typedef struct __mcc_pthread {
	void *__handle;
	void *(*__start)(void *);
	void *__arg;
	void *__ret;
} *pthread_t;

typedef struct {
	int __unused;
} pthread_attr_t;

/* kernel32 imports, declared here to avoid pulling in all of <windows.h>. */
unsigned long __stdcall WaitForSingleObject(void *__handle, unsigned long __ms);
int __stdcall CloseHandle(void *__handle);

static unsigned __stdcall __mcc_pthread_trampoline(void *__p)
{
	pthread_t __t = (pthread_t)__p;
	__t->__ret = __t->__start(__t->__arg);
	return 0;
}

static inline int pthread_create(pthread_t *__t, const pthread_attr_t *__attr,
				 void *(*__start)(void *), void *__arg)
{
	pthread_t __c;
	(void)__attr;
	__c = (pthread_t)malloc(sizeof(*__c));
	if (!__c)
		return 11 /* EAGAIN */;
	__c->__start = __start;
	__c->__arg = __arg;
	__c->__ret = (void *)0;
	__c->__handle =
	    (void *)_beginthreadex(0, 0, __mcc_pthread_trampoline, __c, 0, 0);
	if (!__c->__handle) {
		free(__c);
		return 11 /* EAGAIN */;
	}
	*__t = __c;
	return 0;
}

static inline int pthread_join(pthread_t __t, void **__retval)
{
	WaitForSingleObject(__t->__handle, 0xffffffffu /* INFINITE */);
	if (__retval)
		*__retval = __t->__ret;
	CloseHandle(__t->__handle);
	free(__t);
	return 0;
}

#endif /* _PTHREAD_H */
