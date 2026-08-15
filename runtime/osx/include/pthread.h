#ifndef _MCC_OSX_PTHREAD_H
#define _MCC_OSX_PTHREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __LP64__
#define __PTHREAD_SIZE__ 8176
#define __PTHREAD_ATTR_SIZE__ 56
#define __PTHREAD_MUTEXATTR_SIZE__ 8
#define __PTHREAD_MUTEX_SIZE__ 56
#define __PTHREAD_CONDATTR_SIZE__ 8
#define __PTHREAD_COND_SIZE__ 40
#define __PTHREAD_ONCE_SIZE__ 8
#define __PTHREAD_RWLOCK_SIZE__ 192
#define __PTHREAD_RWLOCKATTR_SIZE__ 16
#else
#define __PTHREAD_SIZE__ 4088
#define __PTHREAD_ATTR_SIZE__ 36
#define __PTHREAD_MUTEXATTR_SIZE__ 8
#define __PTHREAD_MUTEX_SIZE__ 40
#define __PTHREAD_CONDATTR_SIZE__ 4
#define __PTHREAD_COND_SIZE__ 24
#define __PTHREAD_ONCE_SIZE__ 4
#define __PTHREAD_RWLOCK_SIZE__ 124
#define __PTHREAD_RWLOCKATTR_SIZE__ 12
#endif

struct _opaque_pthread_attr_t {
	long __sig;
	char __opaque[__PTHREAD_ATTR_SIZE__];
};
struct _opaque_pthread_cond_t {
	long __sig;
	char __opaque[__PTHREAD_COND_SIZE__];
};
struct _opaque_pthread_condattr_t {
	long __sig;
	char __opaque[__PTHREAD_CONDATTR_SIZE__];
};
struct _opaque_pthread_mutex_t {
	long __sig;
	char __opaque[__PTHREAD_MUTEX_SIZE__];
};
struct _opaque_pthread_mutexattr_t {
	long __sig;
	char __opaque[__PTHREAD_MUTEXATTR_SIZE__];
};
struct _opaque_pthread_once_t {
	long __sig;
	char __opaque[__PTHREAD_ONCE_SIZE__];
};
struct _opaque_pthread_rwlock_t {
	long __sig;
	char __opaque[__PTHREAD_RWLOCK_SIZE__];
};
struct _opaque_pthread_rwlockattr_t {
	long __sig;
	char __opaque[__PTHREAD_RWLOCKATTR_SIZE__];
};
struct _opaque_pthread_t;

typedef struct _opaque_pthread_attr_t pthread_attr_t;
typedef struct _opaque_pthread_cond_t pthread_cond_t;
typedef struct _opaque_pthread_condattr_t pthread_condattr_t;
typedef struct _opaque_pthread_mutex_t pthread_mutex_t;
typedef struct _opaque_pthread_mutexattr_t pthread_mutexattr_t;
typedef struct _opaque_pthread_once_t pthread_once_t;
typedef struct _opaque_pthread_rwlock_t pthread_rwlock_t;
typedef struct _opaque_pthread_rwlockattr_t pthread_rwlockattr_t;
typedef struct _opaque_pthread_t *pthread_t;
typedef unsigned long pthread_key_t;

#define _PTHREAD_ONCE_SIG_init 0x30B1BCBA
#define PTHREAD_ONCE_INIT {_PTHREAD_ONCE_SIG_init, {0}}

#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_ERRORCHECK 1
#define PTHREAD_MUTEX_RECURSIVE 2
#define PTHREAD_MUTEX_DEFAULT PTHREAD_MUTEX_NORMAL

#define PTHREAD_PROCESS_SHARED 1
#define PTHREAD_PROCESS_PRIVATE 2

#define PTHREAD_CREATE_JOINABLE 1
#define PTHREAD_CREATE_DETACHED 2

#define PTHREAD_DESTRUCTOR_ITERATIONS 4

struct timespec;

int pthread_create(pthread_t *, const pthread_attr_t *,
									 void *(*)(void *), void *);
int pthread_join(pthread_t, void **);
int pthread_detach(pthread_t);
int pthread_equal(pthread_t, pthread_t);
pthread_t pthread_self(void);
void pthread_exit(void *);
int pthread_once(pthread_once_t *, void (*)(void));

int pthread_key_create(pthread_key_t *, void (*)(void *));
int pthread_key_delete(pthread_key_t);
void *pthread_getspecific(pthread_key_t);
int pthread_setspecific(pthread_key_t, const void *);

int pthread_attr_init(pthread_attr_t *);
int pthread_attr_destroy(pthread_attr_t *);
int pthread_attr_setdetachstate(pthread_attr_t *, int);

int pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
int pthread_mutex_destroy(pthread_mutex_t *);
int pthread_mutex_lock(pthread_mutex_t *);
int pthread_mutex_trylock(pthread_mutex_t *);
int pthread_mutex_unlock(pthread_mutex_t *);
int pthread_mutexattr_init(pthread_mutexattr_t *);
int pthread_mutexattr_destroy(pthread_mutexattr_t *);
int pthread_mutexattr_settype(pthread_mutexattr_t *, int);

int pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
int pthread_cond_destroy(pthread_cond_t *);
int pthread_cond_signal(pthread_cond_t *);
int pthread_cond_broadcast(pthread_cond_t *);
int pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);
int pthread_cond_timedwait(pthread_cond_t *, pthread_mutex_t *,
													 const struct timespec *);
int pthread_condattr_init(pthread_condattr_t *);
int pthread_condattr_destroy(pthread_condattr_t *);
int pthread_condattr_setpshared(pthread_condattr_t *, int);

#ifdef __cplusplus
}
#endif

#endif
