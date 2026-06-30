#ifndef _MCC_THREADS_H
#define _MCC_THREADS_H

/* C11 <threads.h> (N1570 §7.26).  When the hosting libc already provides one
   (glibc, musl) delegate to it; otherwise — notably on macOS, whose libc ships
   no <threads.h> / thrd_* runtime — provide a thin conforming shim over POSIX
   threads.  This mirrors the bundled <stdint.h> delegate-or-define pattern, so
   on every ELF target the system header still wins. */

/* Hosted: pull the system <threads.h> (glibc/musl) and let it win.  Mirrors the
   bundled <stdint.h>: try the next header, then self-provide only when its
   contents are absent — keyed on ONCE_FLAG_INIT, a macro every conforming
   <threads.h> defines.  This is robust even when the bundled directory appears
   twice on the search path (e.g. -I source plus the build-tree copy), where the
   include guard would otherwise turn the delegating copy into a no-op. */
#if defined __has_include_next && __has_include_next(<threads.h>)
# include_next <threads.h>
#endif

#ifndef ONCE_FLAG_INIT

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>

typedef pthread_t       thrd_t;
typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t  cnd_t;
typedef pthread_key_t   tss_t;
typedef pthread_once_t  once_flag;

typedef int  (*thrd_start_t)(void *);
typedef void (*tss_dtor_t)(void *);

#define ONCE_FLAG_INIT      PTHREAD_ONCE_INIT
/* §7.26.1p3: TSS_DTOR_ITERATIONS must be an integer constant expression (it is
   used in e.g. `int a[TSS_DTOR_ITERATIONS];`). PTHREAD_DESTRUCTOR_ITERATIONS is
   a glibc internal from <limits.h>, not reliably visible here, so fall back to
   the POSIX-required value (_POSIX_THREAD_DESTRUCTOR_ITERATIONS == 4). */
#ifdef PTHREAD_DESTRUCTOR_ITERATIONS
#define TSS_DTOR_ITERATIONS PTHREAD_DESTRUCTOR_ITERATIONS
#else
#define TSS_DTOR_ITERATIONS 4
#endif

/* §7.26.1p3: <threads.h> shall provide the convenience macro `thread_local`
   expanding to _Thread_local. (Only in the shim branch — when a system
   <threads.h> is delegated to above, it already defines this.) */
#define thread_local _Thread_local

enum {
    thrd_success  = 0,
    thrd_busy     = 1,
    thrd_error    = 2,
    thrd_nomem    = 3,
    thrd_timedout = 4
};

enum {
    mtx_plain     = 0,
    mtx_recursive = 1,
    mtx_timed     = 2
};

/* ---- threads ---------------------------------------------------------- */

struct __mcc_thrd_args { thrd_start_t __func; void *__arg; };

static inline void *__mcc_thrd_trampoline(void *__p)
{
    struct __mcc_thrd_args __a = *(struct __mcc_thrd_args *)__p;
    free(__p);
    return (void *)(__INTPTR_TYPE__)__a.__func(__a.__arg);
}

static inline int thrd_create(thrd_t *__thr, thrd_start_t __func, void *__arg)
{
    struct __mcc_thrd_args *__a =
        (struct __mcc_thrd_args *)malloc(sizeof *__a);
    if (!__a)
        return thrd_nomem;
    __a->__func = __func;
    __a->__arg  = __arg;
    if (pthread_create(__thr, (void *)0, __mcc_thrd_trampoline, __a) != 0) {
        free(__a);
        return thrd_error;
    }
    return thrd_success;
}

static inline int thrd_equal(thrd_t __a, thrd_t __b)
{
    return pthread_equal(__a, __b);
}

static inline thrd_t thrd_current(void) { return pthread_self(); }

static inline int thrd_sleep(const struct timespec *__dur, struct timespec *__rem)
{
    return nanosleep(__dur, __rem);
}

static inline void thrd_yield(void) { (void)sched_yield(); }

static inline void thrd_exit(int __res)
{
    pthread_exit((void *)(__INTPTR_TYPE__)__res);
}

static inline int thrd_detach(thrd_t __thr)
{
    return pthread_detach(__thr) ? thrd_error : thrd_success;
}

static inline int thrd_join(thrd_t __thr, int *__res)
{
    void *__r;
    if (pthread_join(__thr, &__r) != 0)
        return thrd_error;
    if (__res)
        *__res = (int)(__INTPTR_TYPE__)__r;
    return thrd_success;
}

/* ---- call_once -------------------------------------------------------- */

static inline void call_once(once_flag *__flag, void (*__func)(void))
{
    pthread_once(__flag, __func);
}

/* ---- mutexes ---------------------------------------------------------- */

static inline int mtx_init(mtx_t *__m, int __type)
{
    if (__type & mtx_recursive) {
        pthread_mutexattr_t __at;
        int __r;
        pthread_mutexattr_init(&__at);
        pthread_mutexattr_settype(&__at, PTHREAD_MUTEX_RECURSIVE);
        __r = pthread_mutex_init(__m, &__at);
        pthread_mutexattr_destroy(&__at);
        return __r ? thrd_error : thrd_success;
    }
    return pthread_mutex_init(__m, (void *)0) ? thrd_error : thrd_success;
}

static inline int mtx_lock(mtx_t *__m)
{
    return pthread_mutex_lock(__m) ? thrd_error : thrd_success;
}

static inline int mtx_trylock(mtx_t *__m)
{
    int __r = pthread_mutex_trylock(__m);
    if (__r == 0)     return thrd_success;
    if (__r == EBUSY) return thrd_busy;
    return thrd_error;
}

/* macOS has no pthread_mutex_timedlock; approximate it with a trylock spin so
   the API stays available (the relative interval is honoured coarsely). */
static inline int mtx_timedlock(mtx_t *__m, const struct timespec *__ts)
{
    for (;;) {
        int __r = pthread_mutex_trylock(__m);
        if (__r == 0)      return thrd_success;
        if (__r != EBUSY)  return thrd_error;
        {
            struct timespec __now;
            struct timespec __nap = { 0, 1000000 };  /* 1 ms */
            clock_gettime(CLOCK_REALTIME, &__now);
            if (__now.tv_sec > __ts->tv_sec ||
                (__now.tv_sec == __ts->tv_sec && __now.tv_nsec >= __ts->tv_nsec))
                return thrd_timedout;
            nanosleep(&__nap, (void *)0);
        }
    }
}

static inline int mtx_unlock(mtx_t *__m)
{
    return pthread_mutex_unlock(__m) ? thrd_error : thrd_success;
}

static inline void mtx_destroy(mtx_t *__m) { pthread_mutex_destroy(__m); }

/* ---- condition variables --------------------------------------------- */

static inline int cnd_init(cnd_t *__c)
{
    return pthread_cond_init(__c, (void *)0) ? thrd_error : thrd_success;
}

static inline int cnd_signal(cnd_t *__c)
{
    return pthread_cond_signal(__c) ? thrd_error : thrd_success;
}

static inline int cnd_broadcast(cnd_t *__c)
{
    return pthread_cond_broadcast(__c) ? thrd_error : thrd_success;
}

static inline int cnd_wait(cnd_t *__c, mtx_t *__m)
{
    return pthread_cond_wait(__c, __m) ? thrd_error : thrd_success;
}

static inline int cnd_timedwait(cnd_t *__c, mtx_t *__m, const struct timespec *__ts)
{
    int __r = pthread_cond_timedwait(__c, __m, __ts);
    if (__r == 0)         return thrd_success;
    if (__r == ETIMEDOUT) return thrd_timedout;
    return thrd_error;
}

static inline void cnd_destroy(cnd_t *__c) { pthread_cond_destroy(__c); }

/* ---- thread-specific storage ----------------------------------------- */

static inline int tss_create(tss_t *__key, tss_dtor_t __dtor)
{
    return pthread_key_create(__key, __dtor) ? thrd_error : thrd_success;
}

static inline void *tss_get(tss_t __key) { return pthread_getspecific(__key); }

static inline int tss_set(tss_t __key, void *__val)
{
    return pthread_setspecific(__key, __val) ? thrd_error : thrd_success;
}

static inline void tss_delete(tss_t __key) { (void)pthread_key_delete(__key); }

#endif /* ONCE_FLAG_INIT (no system <threads.h>) */
#endif /* _MCC_THREADS_H */
