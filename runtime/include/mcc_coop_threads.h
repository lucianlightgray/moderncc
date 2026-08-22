#ifndef _MCC_COOP_THREADS_H
#define _MCC_COOP_THREADS_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#if defined(MCC_COOP_MN) && !defined(MCC_COOP_MT)
#define MCC_COOP_MT 1
#endif

#ifdef MCC_COOP_MN
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#endif

extern void *malloc(size_t);
extern void *calloc(size_t, size_t);
extern void free(void *);
extern _Noreturn void abort(void);
extern _Noreturn void exit(int);
#if !(defined(_WIN32) && defined(MCC_COOP_MN))
extern long write(int, const void *, size_t);
#endif

#define thread_local _Thread_local

enum {
	thrd_success = 0,
	thrd_busy = 1,
	thrd_error = 2,
	thrd_nomem = 3,
	thrd_timedout = 4
};

enum {
	mtx_plain = 0,
	mtx_recursive = 1,
	mtx_timed = 2
};

#define __MCC_COOP_MAX_TSS 64
#define __MCC_COOP_STACK (256u * 1024u)

enum {
	__MCC_F_RUNNABLE = 0,
	__MCC_F_BLOCKED = 1,
	__MCC_F_DONE = 2
};

typedef struct __mcc_fiber {
	void *sp;
	void *stack;
	int (*fn)(void *);
	void *arg;
	int result;
	int state;
	int detached;
	int is_extern;
	int errno_save;
	void *blocked_on;
	struct __mcc_fiber *qnext;
	struct __mcc_fiber *all_next;
	void *tss[__MCC_COOP_MAX_TSS];
} __mcc_fiber;

typedef __mcc_fiber *thrd_t;
typedef int (*thrd_start_t)(void *);
typedef void (*tss_dtor_t)(void *);
typedef int tss_t;

/*
 * mtx_t/cnd_t are cooperative for every backend, including M:N. A pthread
 * mutex/cond would sink a blocking user wait into the kernel on the pthread
 * WORKER, pinning it; the whole point of the M:N scheduler is to park the
 * FIBER and hand the worker back to the run loop (see __mcc_mn_block). The
 * scheduler's own coordination still uses real pthread primitives below.
 */
typedef struct {
	int locked;
	int type;
	int rec;
	__mcc_fiber *owner;
} mtx_t;

typedef struct {
	int dummy;
} cnd_t;

/*
 * Define glibc's own once_flag guard so that a later <stdlib.h> (which, under
 * __GLIBC_USE(ISOC23) — mcc's default — pulls bits/types/once_flag.h) does not
 * re-typedef `once_flag` and collide with the coop definition below. Coop keeps
 * its own `int once_flag`; the guard just suppresses glibc's struct typedef.
 * Supported include order is <threads.h> before <stdlib.h> (T-lin-10421).
 */
#ifndef __once_flag_defined
#define __once_flag_defined 1
#endif
typedef int once_flag;
#ifndef ONCE_FLAG_INIT
#define ONCE_FLAG_INIT 0
#endif
#define TSS_DTOR_ITERATIONS 1

/*
 * Per-target [X] context-switch backend. Each target must provide two symbols:
 *
 *   void __mcc_ctx_swap(void **save_sp, void *to_sp);
 *       Save the callee-saved register state + stack pointer of the current
 *       context into *save_sp, then load to_sp and resume the context that was
 *       last saved there (returns in that other context, right after ITS swap).
 *
 *   void *__mcc_ctx_make(void *stack_base, unsigned long size, void (*entry)(void));
 *       Build an initial sp on [stack_base, stack_base+size) such that the first
 *       __mcc_ctx_swap INTO it begins executing entry() on an ABI-valid stack.
 *       entry never returns.
 *
 * entry is always __mcc_fiber_start (below): it reads the current fiber, runs
 * its start routine, then swaps away for good. The backend passes no arguments
 * to entry — the running fiber is found via the __mcc_cur global.
 */

#if defined(__x86_64__) && !defined(_WIN32)

__asm__(
	".text\n"
	".p2align 4\n"
	".globl __mcc_ctx_swap\n"
	"__mcc_ctx_swap:\n"
	"	pushq %rbp\n"
	"	pushq %rbx\n"
	"	pushq %r12\n"
	"	pushq %r13\n"
	"	pushq %r14\n"
	"	pushq %r15\n"
	"	movq %rsp, (%rdi)\n"
	"	movq %rsi, %rsp\n"
	"	popq %r15\n"
	"	popq %r14\n"
	"	popq %r13\n"
	"	popq %r12\n"
	"	popq %rbx\n"
	"	popq %rbp\n"
	"	ret\n"
);

extern void __mcc_ctx_swap(void **__save_sp, void *__to_sp);

static void *__mcc_ctx_make(void *__base, unsigned long __size, void (*__entry)(void)) {
	uintptr_t __top = (uintptr_t)__base + __size;
	__top &= ~(uintptr_t)15;
	void **__sp = (void **)__top - 8;
	__sp[0] = (void *)0;
	__sp[1] = (void *)0;
	__sp[2] = (void *)0;
	__sp[3] = (void *)0;
	__sp[4] = (void *)0;
	__sp[5] = (void *)0;
	__sp[6] = (void *)__entry;
	__sp[7] = (void *)0;
	return (void *)__sp;
}

#elif defined(__x86_64__) && defined(_WIN32)

/*
 * x86_64-PE/Windows context switch (T-win-50033) via Win32 Fibers.
 *
 * A Fiber carries exactly the Win64 per-context state a hand-rolled swap would
 * otherwise have to shuffle by hand -- the TIB stack fields (StackBase /
 * StackLimit / DeallocationStack that __chkstk and SEH read), the SEH chain,
 * and the callee-saved xmm6-xmm15 -- so the OS primitives are both simpler and
 * more correct than inline asm here. The opaque `sp` slot the scheduler keeps
 * per fiber holds the fiber handle (it never dereferences sp), and the malloc'd
 * coop stack buffer is unused (CreateFiber allocates its own stack).
 *
 * GetCurrentFiber() is a TEB macro (reads gs:[0x20]), not a kernel32 export, so
 * it is unresolvable under `mcc -run`; we avoid it entirely. main's handle is
 * captured lazily on its first swap-away, when its stored sp is still 0 -- a
 * worker already carries the handle CreateFiber returned. IsThreadAFiber is
 * likewise skipped (also not reachable under -run): sp==0 uniquely means "this
 * context has no handle yet", which for the single OS thread is main pre-convert.
 *
 * Known limitation (documented follow-up, matches the timed-wait clock note):
 * the scheduler has no per-backend free hook, so a fiber handle is not
 * DeleteFiber'd when its struct is freed -- the OS fiber leaks until process
 * exit. Harmless for single-run programs and the gate.
 */

extern void *ConvertThreadToFiber(void *__param);
extern void *CreateFiber(unsigned long long __stack_size, void (*__start)(void *), void *__param);
extern void SwitchToFiber(void *__fiber);
extern void DeleteFiber(void *__fiber);

static void __mcc_fiber_proc(void *__entry) {
	((void (*)(void))__entry)();
}

static void __mcc_ctx_swap(void **__save_sp, void *__to_sp) {
	if (!*__save_sp)
		*__save_sp = ConvertThreadToFiber((void *)0);
	SwitchToFiber(__to_sp);
}

static void *__mcc_ctx_make(void *__base, unsigned long __size, void (*__entry)(void)) {
	(void)__base;
	return CreateFiber((unsigned long long)__size, __mcc_fiber_proc, (void *)__entry);
}

#elif defined(__aarch64__)

#ifdef __APPLE__
#define __MCC_CTX_SWAP_SYM "___mcc_ctx_swap"
#else
#define __MCC_CTX_SWAP_SYM "__mcc_ctx_swap"
#endif

__asm__(
	".text\n"
	".p2align 2\n"
	".globl " __MCC_CTX_SWAP_SYM "\n"
	__MCC_CTX_SWAP_SYM ":\n"
	"	stp x19, x20, [sp, #-160]!\n"
	"	stp x21, x22, [sp, #16]\n"
	"	stp x23, x24, [sp, #32]\n"
	"	stp x25, x26, [sp, #48]\n"
	"	stp x27, x28, [sp, #64]\n"
	"	stp x29, x30, [sp, #80]\n"
	"	stp d8,  d9,  [sp, #96]\n"
	"	stp d10, d11, [sp, #112]\n"
	"	stp d12, d13, [sp, #128]\n"
	"	stp d14, d15, [sp, #144]\n"
	"	mov x2, sp\n"
	"	str x2, [x0]\n"
	"	mov sp, x1\n"
	"	ldp x19, x20, [sp]\n"
	"	ldp x21, x22, [sp, #16]\n"
	"	ldp x23, x24, [sp, #32]\n"
	"	ldp x25, x26, [sp, #48]\n"
	"	ldp x27, x28, [sp, #64]\n"
	"	ldp x29, x30, [sp, #80]\n"
	"	ldp d8,  d9,  [sp, #96]\n"
	"	ldp d10, d11, [sp, #112]\n"
	"	ldp d12, d13, [sp, #128]\n"
	"	ldp d14, d15, [sp, #144]\n"
	"	add sp, sp, #160\n"
	"	ret\n"
);

extern void __mcc_ctx_swap(void **__save_sp, void *__to_sp);

static void *__mcc_ctx_make(void *__base, unsigned long __size, void (*__entry)(void)) {
	uintptr_t __top = ((uintptr_t)__base + __size) & ~(uintptr_t)15;
	void **__sp = (void **)(__top - 160);
	for (int __i = 0; __i < 20; __i++)
		__sp[__i] = (void *)0;
	__sp[11] = (void *)__entry;
	return (void *)__sp;
}

#elif defined(__riscv) && __riscv_xlen == 64

__asm__(
	".text\n"
	".p2align 2\n"
	".globl __mcc_ctx_swap\n"
	"__mcc_ctx_swap:\n"
	"	addi sp, sp, -208\n"
	"	sd ra,  0(sp)\n"
	"	sd s0,  8(sp)\n"
	"	sd s1,  16(sp)\n"
	"	sd s2,  24(sp)\n"
	"	sd s3,  32(sp)\n"
	"	sd s4,  40(sp)\n"
	"	sd s5,  48(sp)\n"
	"	sd s6,  56(sp)\n"
	"	sd s7,  64(sp)\n"
	"	sd s8,  72(sp)\n"
	"	sd s9,  80(sp)\n"
	"	sd s10, 88(sp)\n"
	"	sd s11, 96(sp)\n"
	"	fsd fs0,  104(sp)\n"
	"	fsd fs1,  112(sp)\n"
	"	fsd fs2,  120(sp)\n"
	"	fsd fs3,  128(sp)\n"
	"	fsd fs4,  136(sp)\n"
	"	fsd fs5,  144(sp)\n"
	"	fsd fs6,  152(sp)\n"
	"	fsd fs7,  160(sp)\n"
	"	fsd fs8,  168(sp)\n"
	"	fsd fs9,  176(sp)\n"
	"	fsd fs10, 184(sp)\n"
	"	fsd fs11, 192(sp)\n"
	"	sd sp, 0(a0)\n"
	"	mv sp, a1\n"
	"	ld ra,  0(sp)\n"
	"	ld s0,  8(sp)\n"
	"	ld s1,  16(sp)\n"
	"	ld s2,  24(sp)\n"
	"	ld s3,  32(sp)\n"
	"	ld s4,  40(sp)\n"
	"	ld s5,  48(sp)\n"
	"	ld s6,  56(sp)\n"
	"	ld s7,  64(sp)\n"
	"	ld s8,  72(sp)\n"
	"	ld s9,  80(sp)\n"
	"	ld s10, 88(sp)\n"
	"	ld s11, 96(sp)\n"
	"	fld fs0,  104(sp)\n"
	"	fld fs1,  112(sp)\n"
	"	fld fs2,  120(sp)\n"
	"	fld fs3,  128(sp)\n"
	"	fld fs4,  136(sp)\n"
	"	fld fs5,  144(sp)\n"
	"	fld fs6,  152(sp)\n"
	"	fld fs7,  160(sp)\n"
	"	fld fs8,  168(sp)\n"
	"	fld fs9,  176(sp)\n"
	"	fld fs10, 184(sp)\n"
	"	fld fs11, 192(sp)\n"
	"	addi sp, sp, 208\n"
	"	ret\n"
);

extern void __mcc_ctx_swap(void **__save_sp, void *__to_sp);

static void *__mcc_ctx_make(void *__base, unsigned long __size, void (*__entry)(void)) {
	uintptr_t __top = ((uintptr_t)__base + __size) & ~(uintptr_t)15;
	void **__sp = (void **)(__top - 208);
	for (int __i = 0; __i < 26; __i++)
		__sp[__i] = (void *)0;
	__sp[0] = (void *)__entry;
	return (void *)__sp;
}

#else
#error "mcc_coop_threads.h: no __mcc_ctx_swap/__mcc_ctx_make backend for this target -- add the per-target [X] context switch (T-lin-10001 [C] core; arm64/win/riscv64 owned by mac/win)."
#endif

static __mcc_fiber __mcc_main;
#ifndef MCC_COOP_MN
static __mcc_fiber *__mcc_cur = (__mcc_fiber *)0;
#endif
static __mcc_fiber *__mcc_ready_head = (__mcc_fiber *)0;
static __mcc_fiber *__mcc_ready_tail = (__mcc_fiber *)0;
static __mcc_fiber *__mcc_all = (__mcc_fiber *)0;
static __mcc_fiber *__mcc_zombie = (__mcc_fiber *)0;
static int __mcc_inited = 0;
static int __mcc_tss_next = 0;
static tss_dtor_t __mcc_tss_dtor[__MCC_COOP_MAX_TSS];

#ifdef MCC_COOP_MN

#define __MCC_MN_MAX_WORKERS 256

typedef struct __mcc_worker {
	pthread_t th;
	void *sched_sp;
	__mcc_fiber *cur;
	int is_main;
} __mcc_worker;

static pthread_mutex_t __mcc_mn_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t __mcc_mn_work = PTHREAD_COND_INITIALIZER;
static pthread_cond_t __mcc_mn_extern = PTHREAD_COND_INITIALIZER;
static __mcc_worker __mcc_mn_workers[__MCC_MN_MAX_WORKERS];
static int __mcc_mn_nworkers = 0;
static int __mcc_mn_started = 0;
static int __mcc_mn_quit = 0;
static _Thread_local __mcc_worker *__mcc_self = (__mcc_worker *)0;
static _Thread_local __mcc_fiber *__mcc_extern_self = (__mcc_fiber *)0;
#define __mcc_cur (__mcc_self->cur)

#define __MCC_LOCK() pthread_mutex_lock(&__mcc_mn_lock)
#define __MCC_UNLOCK() pthread_mutex_unlock(&__mcc_mn_lock)

#elif defined(MCC_COOP_MT)
static volatile int __mcc_sched = 0;
static void __mcc_lock(void) {
	while (__atomic_exchange_n(&__mcc_sched, 1, __ATOMIC_ACQUIRE))
		;
}
static void __mcc_unlock(void) {
	__atomic_store_n(&__mcc_sched, 0, __ATOMIC_RELEASE);
}
#define __MCC_LOCK() __mcc_lock()
#define __MCC_UNLOCK() __mcc_unlock()
#else
#define __MCC_LOCK()
#define __MCC_UNLOCK()
#endif

/*
 * Canonical "current schedulable entity" accessor. Under M:N the caller may be
 * a worker (running a fiber) or an external OS thread such as main; the latter
 * has no __mcc_self, so __mcc_current() lazily materializes a shadow fiber for
 * it. Every other backend has a single __mcc_cur.
 */
#ifdef MCC_COOP_MN
static __mcc_fiber *__mcc_current(void);
#define __MCC_SELF() __mcc_current()
#else
#define __MCC_SELF() __mcc_cur
#endif

#ifndef MCC_COOP_MN
static void __mcc_need_init(void) {
	if (__mcc_inited)
		return;
	__mcc_inited = 1;
	__mcc_main.state = __MCC_F_RUNNABLE;
	__mcc_main.all_next = __mcc_all;
	__mcc_all = &__mcc_main;
	__mcc_cur = &__mcc_main;
}
#endif

static void __mcc_ready_push(__mcc_fiber *__f) {
	__f->state = __MCC_F_RUNNABLE;
	__f->qnext = (__mcc_fiber *)0;
	if (__mcc_ready_tail)
		__mcc_ready_tail->qnext = __f;
	else
		__mcc_ready_head = __f;
	__mcc_ready_tail = __f;
}

static __mcc_fiber *__mcc_ready_pop(void) {
	__mcc_fiber *__f = __mcc_ready_head;
	if (!__f)
		return (__mcc_fiber *)0;
	__mcc_ready_head = __f->qnext;
	if (!__mcc_ready_head)
		__mcc_ready_tail = (__mcc_fiber *)0;
	__f->qnext = (__mcc_fiber *)0;
	return __f;
}

static void __mcc_all_remove(__mcc_fiber *__f) {
	__mcc_fiber **__pp = &__mcc_all;
	while (*__pp) {
		if (*__pp == __f) {
			*__pp = __f->all_next;
			return;
		}
		__pp = &(*__pp)->all_next;
	}
}

static void __mcc_reap(void) {
	__MCC_LOCK();
	if (__mcc_zombie) {
		__mcc_all_remove(__mcc_zombie);
#if defined(_WIN32) && defined(__x86_64__)
		if (__mcc_zombie->sp)
			DeleteFiber(__mcc_zombie->sp);
#endif
		free(__mcc_zombie->stack);
		free(__mcc_zombie);
		__mcc_zombie = (__mcc_fiber *)0;
	}
	__MCC_UNLOCK();
}

#ifdef MCC_COOP_MN
/*
 * Worker run loop. The scheduler lock is held continuously across the top of
 * the loop and is HANDED to the fiber across the context swap: the loop swaps
 * into a fiber with the lock held, and the fiber releases it once it is about
 * to run user code (see __mcc_fiber_start / __mcc_mn_block). When a fiber hands
 * control back it has re-taken the lock, so the loop always resumes holding it.
 * Because every fiber<->scheduler swap stays on ONE pthread, the pthread mutex
 * is only ever locked and unlocked by the same thread even though fibers
 * migrate between workers. A fiber returns here either DONE (finished) or
 * BLOCKED (parked on a wait token); a parked fiber has already recorded itself,
 * so the loop just picks the next ready fiber.
 */
static void *__mcc_mn_worker_main(void *__arg) {
	__mcc_worker *__w = (__mcc_worker *)__arg;
	__mcc_self = __w;
	pthread_mutex_lock(&__mcc_mn_lock);
	for (;;) {
		__mcc_fiber *__f = __mcc_ready_pop();
		if (__f) {
			__w->cur = __f;
			__f->state = __MCC_F_RUNNABLE;
			__mcc_ctx_swap(&__w->sched_sp, __f->sp);
			if (__f->state == __MCC_F_DONE && __f->detached) {
				__mcc_all_remove(__f);
#if defined(_WIN32) && defined(__x86_64__)
				if (__f->sp)
					DeleteFiber(__f->sp);
#endif
				free(__f->stack);
				free(__f);
			}
			__w->cur = (__mcc_fiber *)0;
			continue;
		}
		if (__mcc_mn_quit) {
			pthread_mutex_unlock(&__mcc_mn_lock);
			return (void *)0;
		}
		pthread_cond_wait(&__mcc_mn_work, &__mcc_mn_lock);
	}
}

__attribute__((destructor)) static void __mcc_mn_shutdown(void) {
	int __i, __n;
	pthread_mutex_lock(&__mcc_mn_lock);
	if (!__mcc_mn_started) {
		pthread_mutex_unlock(&__mcc_mn_lock);
		return;
	}
	__mcc_mn_quit = 1;
	pthread_cond_broadcast(&__mcc_mn_work);
	__n = __mcc_mn_nworkers;
	pthread_mutex_unlock(&__mcc_mn_lock);
	for (__i = 0; __i < __n; __i++)
		pthread_join(__mcc_mn_workers[__i].th, (void *)0);
}

static void __mcc_need_init(void) {
	int __i, __n;
	pthread_mutex_lock(&__mcc_mn_lock);
	if (__mcc_mn_started) {
		pthread_mutex_unlock(&__mcc_mn_lock);
		return;
	}
	__mcc_mn_started = 1;
	__mcc_inited = 1;
#if defined(_WIN32)
	{
		extern char *getenv(const char *);
		extern int atoi(const char *);
		char *__np = getenv("NUMBER_OF_PROCESSORS");
		__n = __np ? atoi(__np) : 1;
	}
#else
	__n = (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
	if (__n < 1)
		__n = 1;
	if (__n > __MCC_MN_MAX_WORKERS)
		__n = __MCC_MN_MAX_WORKERS;
	for (__i = 0; __i < __n; __i++) {
		__mcc_mn_workers[__i].cur = (__mcc_fiber *)0;
		__mcc_mn_workers[__i].sched_sp = (void *)0;
		__mcc_mn_workers[__i].is_main = 0;
		if (pthread_create(&__mcc_mn_workers[__i].th, (void *)0,
											 __mcc_mn_worker_main, &__mcc_mn_workers[__i]) != 0)
			break;
		__mcc_mn_nworkers++;
	}
	pthread_mutex_unlock(&__mcc_mn_lock);
}

/*
 * Return the schedulable entity for the calling thread. A worker returns the
 * fiber it is running. An external thread (main, or any thread that did not
 * come from the worker pool) gets a lazily-allocated shadow fiber, flagged
 * is_extern, registered in __mcc_all so waiters can find it by token. The
 * shadow parks on __mcc_mn_extern rather than swapping stacks. MUST be called
 * without __mcc_mn_lock held (it takes the lock to publish a new shadow).
 */
static __mcc_fiber *__mcc_current(void) {
	__mcc_fiber *__f;
	if (__mcc_self)
		return __mcc_self->cur;
	if (__mcc_extern_self)
		return __mcc_extern_self;
	__f = (__mcc_fiber *)calloc(1, sizeof *__f);
	if (!__f)
		abort();
	__f->state = __MCC_F_RUNNABLE;
	__f->is_extern = 1;
	pthread_mutex_lock(&__mcc_mn_lock);
	__f->all_next = __mcc_all;
	__mcc_all = __f;
	pthread_mutex_unlock(&__mcc_mn_lock);
	__mcc_extern_self = __f;
	return __f;
}

/*
 * Move a blocked entity back to runnable and wake something to run it. A worker
 * fiber goes on the ready queue and one idle worker is signalled; an external
 * shadow is just flipped to RUNNABLE and the external condvar is broadcast so
 * its owning OS thread re-checks. Caller holds __mcc_mn_lock.
 */
static void __mcc_mn_make_ready(__mcc_fiber *__f) {
	__f->blocked_on = (void *)0;
	if (__f->is_extern) {
		__f->state = __MCC_F_RUNNABLE;
		pthread_cond_broadcast(&__mcc_mn_extern);
	} else {
		__mcc_ready_push(__f);
		pthread_cond_signal(&__mcc_mn_work);
	}
}

static void __mcc_mn_wake_one(void *__token) {
	__mcc_fiber *__f;
	for (__f = __mcc_all; __f; __f = __f->all_next)
		if (__f->state == __MCC_F_BLOCKED && __f->blocked_on == __token) {
			__mcc_mn_make_ready(__f);
			return;
		}
}

static void __mcc_mn_wake_all(void *__token) {
	__mcc_fiber *__f;
	for (__f = __mcc_all; __f; __f = __f->all_next)
		if (__f->state == __MCC_F_BLOCKED && __f->blocked_on == __token)
			__mcc_mn_make_ready(__f);
}

/*
 * Block the current entity on __token, then hand its worker back to the run
 * loop (fiber) or sleep its OS thread (external). Caller holds __mcc_mn_lock
 * and it is still held on return. A fiber saves/restores native errno around
 * the park so a per-thread errno survives migration to another worker.
 * Timed waits are treated as untimed here: no coop-mn test relies on a
 * wall-clock deadline, and the previous pthread-timed path is gone with the
 * pthread mtx/cnd it depended on.
 */
static void __mcc_mn_block(void *__token) {
	__mcc_fiber *__self;
	if (__mcc_self) {
		__self = __mcc_self->cur;
		__self->state = __MCC_F_BLOCKED;
		__self->blocked_on = __token;
		__self->errno_save = errno;
		__mcc_ctx_swap(&__self->sp, __mcc_self->sched_sp);
		errno = __self->errno_save;
		return;
	}
	__self = __mcc_current();
	__self->state = __MCC_F_BLOCKED;
	__self->blocked_on = __token;
	while (__self->state == __MCC_F_BLOCKED)
		pthread_cond_wait(&__mcc_mn_extern, &__mcc_mn_lock);
}

static void __mcc_fiber_start(void) {
	__mcc_fiber *__self = __mcc_self->cur;
	pthread_mutex_unlock(&__mcc_mn_lock);
	errno = 0;
	__self->result = __self->fn(__self->arg);
	pthread_mutex_lock(&__mcc_mn_lock);
	__self->state = __MCC_F_DONE;
	__mcc_mn_wake_all(__self);
	__mcc_ctx_swap(&__self->sp, __mcc_self->sched_sp);
}
#endif

static void __mcc_switch(__mcc_fiber *__next) {
	__mcc_fiber *__cur = __mcc_cur;
	__mcc_cur = __next;
	__mcc_ctx_swap(&__cur->sp, __next->sp);
	__mcc_reap();
}

_Noreturn static void __mcc_deadlock(void) {
	static const char __msg[] = "mcc coop threads: deadlock -- all fibers blocked, none runnable\n";
	write(2, __msg, sizeof __msg - 1);
	abort();
}

static int __mcc_block_on(void *__token, int __timed) {
	__mcc_fiber *__next;
	__MCC_LOCK();
	__mcc_cur->state = __MCC_F_BLOCKED;
	__mcc_cur->blocked_on = __token;
	__next = __mcc_ready_pop();
	if (!__next) {
		if (__timed) {
			__mcc_cur->state = __MCC_F_RUNNABLE;
			__mcc_cur->blocked_on = (void *)0;
			__MCC_UNLOCK();
			return 0;
		}
		__MCC_UNLOCK();
		__mcc_deadlock();
	}
	__MCC_UNLOCK();
	__mcc_switch(__next);
	return 1;
}

static void __mcc_wake_one(void *__token) {
	__mcc_fiber *__f;
	for (__f = __mcc_all; __f; __f = __f->all_next) {
		if (__f->state == __MCC_F_BLOCKED && __f->blocked_on == __token) {
			__f->blocked_on = (void *)0;
			__mcc_ready_push(__f);
			return;
		}
	}
}

static void __mcc_wake_all(void *__token) {
	__mcc_fiber *__f;
	for (__f = __mcc_all; __f; __f = __f->all_next) {
		if (__f->state == __MCC_F_BLOCKED && __f->blocked_on == __token) {
			__f->blocked_on = (void *)0;
			__mcc_ready_push(__f);
		}
	}
}

static void __mcc_yield(void) {
	__mcc_fiber *__next;
	__mcc_need_init();
	__MCC_LOCK();
	__next = __mcc_ready_pop();
	if (!__next) {
		__MCC_UNLOCK();
		return;
	}
	__mcc_ready_push(__mcc_cur);
	__MCC_UNLOCK();
	__mcc_switch(__next);
}

#ifndef MCC_COOP_MN
static void __mcc_fiber_start(void) {
	__mcc_fiber *__self = __mcc_cur;
	__mcc_fiber *__next;
	__self->result = __self->fn(__self->arg);
	__MCC_LOCK();
	__self->state = __MCC_F_DONE;
	__mcc_wake_all(__self);
	if (__self->detached)
		__mcc_zombie = __self;
	__next = __mcc_ready_pop();
	if (!__next)
		__next = &__mcc_main;
	__mcc_cur = __next;
	__MCC_UNLOCK();
	__mcc_ctx_swap(&__self->sp, __next->sp);
}

static int thrd_create(thrd_t *__thr, thrd_start_t __func, void *__arg) {
	__mcc_fiber *__f;
	int __i;
	__mcc_need_init();
	__f = (__mcc_fiber *)calloc(1, sizeof *__f);
	if (!__f)
		return thrd_nomem;
	__f->stack = malloc(__MCC_COOP_STACK);
	if (!__f->stack) {
		free(__f);
		return thrd_nomem;
	}
	__f->fn = __func;
	__f->arg = __arg;
	for (__i = 0; __i < __MCC_COOP_MAX_TSS; __i++)
		__f->tss[__i] = (void *)0;
	__f->sp = __mcc_ctx_make(__f->stack, __MCC_COOP_STACK, __mcc_fiber_start);
	__MCC_LOCK();
	__f->all_next = __mcc_all;
	__mcc_all = __f;
	__mcc_ready_push(__f);
	__MCC_UNLOCK();
	*__thr = __f;
	return thrd_success;
}

static int thrd_join(thrd_t __thr, int *__res) {
	__mcc_need_init();
	while (__thr->state != __MCC_F_DONE)
		__mcc_block_on(__thr, 0);
	if (__res)
		*__res = __thr->result;
	__MCC_LOCK();
	__mcc_all_remove(__thr);
	__MCC_UNLOCK();
	free(__thr->stack);
	free(__thr);
	return thrd_success;
}
#else
static int thrd_create(thrd_t *__thr, thrd_start_t __func, void *__arg) {
	__mcc_fiber *__f;
	int __i;
	__mcc_need_init();
	__f = (__mcc_fiber *)calloc(1, sizeof *__f);
	if (!__f)
		return thrd_nomem;
	__f->stack = malloc(__MCC_COOP_STACK);
	if (!__f->stack) {
		free(__f);
		return thrd_nomem;
	}
	__f->fn = __func;
	__f->arg = __arg;
	for (__i = 0; __i < __MCC_COOP_MAX_TSS; __i++)
		__f->tss[__i] = (void *)0;
	__f->sp = __mcc_ctx_make(__f->stack, __MCC_COOP_STACK, __mcc_fiber_start);
	pthread_mutex_lock(&__mcc_mn_lock);
	__f->all_next = __mcc_all;
	__mcc_all = __f;
	__mcc_ready_push(__f);
	pthread_cond_signal(&__mcc_mn_work);
	pthread_mutex_unlock(&__mcc_mn_lock);
	*__thr = __f;
	return thrd_success;
}

static int thrd_join(thrd_t __thr, int *__res) {
	__mcc_need_init();
	__mcc_current();
	__MCC_LOCK();
	while (__thr->state != __MCC_F_DONE)
		__mcc_mn_block(__thr);
	if (__res)
		*__res = __thr->result;
	__mcc_all_remove(__thr);
	__MCC_UNLOCK();
	free(__thr->stack);
	free(__thr);
	return thrd_success;
}
#endif

static int thrd_detach(thrd_t __thr) {
	__mcc_need_init();
	if (__thr->state == __MCC_F_DONE) {
		__MCC_LOCK();
		__mcc_all_remove(__thr);
		__MCC_UNLOCK();
		free(__thr->stack);
		free(__thr);
		return thrd_success;
	}
	__thr->detached = 1;
	return thrd_success;
}

static int thrd_equal(thrd_t __a, thrd_t __b) {
	return __a == __b;
}

static thrd_t thrd_current(void) {
	__mcc_need_init();
	return __MCC_SELF();
}

static void thrd_yield(void) {
#ifdef MCC_COOP_MN
	sched_yield();
#else
	__mcc_yield();
#endif
}

static int thrd_sleep(const struct timespec *__dur, struct timespec *__rem) {
	(void)__dur;
	if (__rem) {
		__rem->tv_sec = 0;
		__rem->tv_nsec = 0;
	}
#ifdef MCC_COOP_MN
	sched_yield();
#else
	__mcc_yield();
#endif
	return 0;
}

_Noreturn static void thrd_exit(int __res) {
	__mcc_fiber *__self;
	__mcc_fiber *__next;
	__mcc_need_init();
#ifdef MCC_COOP_MN
	(void)__next;
	if (__mcc_self) {
		__self = __mcc_self->cur;
		__self->result = __res;
		__MCC_LOCK();
		__self->state = __MCC_F_DONE;
		__mcc_mn_wake_all(__self);
		__mcc_ctx_swap(&__self->sp, __mcc_self->sched_sp);
		for (;;)
			;
	}
	exit(__res);
#else
	__self = __mcc_cur;
	__self->result = __res;
	__MCC_LOCK();
	__self->state = __MCC_F_DONE;
	__mcc_wake_all(__self);
	if (__self->detached)
		__mcc_zombie = __self;
	__next = __mcc_ready_pop();
	if (!__next)
		__next = &__mcc_main;
	if (__self == &__mcc_main) {
		while (__next) {
			__MCC_UNLOCK();
			__mcc_switch(__next);
			__MCC_LOCK();
			__next = __mcc_ready_pop();
		}
		__MCC_UNLOCK();
		exit(__res);
	}
	__mcc_cur = __next;
	__MCC_UNLOCK();
	__mcc_ctx_swap(&__self->sp, __next->sp);
	for (;;)
		;
#endif
}

static void call_once(once_flag *__flag, void (*__func)(void)) {
#ifdef MCC_COOP_MT
	int __st = __atomic_load_n(__flag, __ATOMIC_ACQUIRE);
	if (__st == 2)
		return;
	if (__st == 0) {
		int __exp = 0;
		if (__atomic_compare_exchange_n(__flag, &__exp, 1, 0, __ATOMIC_ACQ_REL,
																		__ATOMIC_ACQUIRE)) {
			__func();
			__atomic_store_n(__flag, 2, __ATOMIC_RELEASE);
			return;
		}
	}
	while (__atomic_load_n(__flag, __ATOMIC_ACQUIRE) != 2)
#ifdef MCC_COOP_MN
		sched_yield();
#else
		__mcc_yield();
#endif
#else
	if (*__flag)
		return;
	*__flag = 1;
	__func();
#endif
}

#ifndef MCC_COOP_MN
static int mtx_init(mtx_t *__m, int __type) {
	__m->locked = 0;
	__m->type = __type;
	__m->rec = 0;
	__m->owner = (__mcc_fiber *)0;
	return thrd_success;
}

static int mtx_lock(mtx_t *__m) {
	__mcc_need_init();
	__MCC_LOCK();
	if (__m->locked && (__m->type & mtx_recursive) && __m->owner == __mcc_cur) {
		__m->rec++;
		__MCC_UNLOCK();
		return thrd_success;
	}
	while (__m->locked) {
		__MCC_UNLOCK();
		__mcc_block_on(__m, 0);
		__MCC_LOCK();
	}
	__m->locked = 1;
	__m->owner = __mcc_cur;
	__m->rec = 1;
	__MCC_UNLOCK();
	return thrd_success;
}

static int mtx_trylock(mtx_t *__m) {
	__mcc_need_init();
	__MCC_LOCK();
	if (__m->locked) {
		if ((__m->type & mtx_recursive) && __m->owner == __mcc_cur) {
			__m->rec++;
			__MCC_UNLOCK();
			return thrd_success;
		}
		__MCC_UNLOCK();
		return thrd_busy;
	}
	__m->locked = 1;
	__m->owner = __mcc_cur;
	__m->rec = 1;
	__MCC_UNLOCK();
	return thrd_success;
}

static int mtx_timedlock(mtx_t *__m, const struct timespec *__ts) {
	(void)__ts;
	__mcc_need_init();
	__MCC_LOCK();
	if (__m->locked && (__m->type & mtx_recursive) && __m->owner == __mcc_cur) {
		__m->rec++;
		__MCC_UNLOCK();
		return thrd_success;
	}
	while (__m->locked) {
		__MCC_UNLOCK();
		if (!__mcc_block_on(__m, 1))
			return thrd_timedout;
		__MCC_LOCK();
	}
	__m->locked = 1;
	__m->owner = __mcc_cur;
	__m->rec = 1;
	__MCC_UNLOCK();
	return thrd_success;
}

static int mtx_unlock(mtx_t *__m) {
	__MCC_LOCK();
	if ((__m->type & mtx_recursive) && __m->rec > 1) {
		__m->rec--;
		__MCC_UNLOCK();
		return thrd_success;
	}
	__m->locked = 0;
	__m->owner = (__mcc_fiber *)0;
	__m->rec = 0;
	__mcc_wake_one(__m);
	__MCC_UNLOCK();
	return thrd_success;
}

static void mtx_destroy(mtx_t *__m) {
	(void)__m;
}

static int cnd_init(cnd_t *__c) {
	__c->dummy = 0;
	return thrd_success;
}

static int cnd_signal(cnd_t *__c) {
	__MCC_LOCK();
	__mcc_wake_one(__c);
	__MCC_UNLOCK();
	return thrd_success;
}

static int cnd_broadcast(cnd_t *__c) {
	__MCC_LOCK();
	__mcc_wake_all(__c);
	__MCC_UNLOCK();
	return thrd_success;
}

static int cnd_wait(cnd_t *__c, mtx_t *__m) {
	__mcc_need_init();
	mtx_unlock(__m);
	__mcc_block_on(__c, 0);
	mtx_lock(__m);
	return thrd_success;
}

static int cnd_timedwait(cnd_t *__c, mtx_t *__m, const struct timespec *__ts) {
	int __woke;
	(void)__ts;
	__mcc_need_init();
	mtx_unlock(__m);
	__woke = __mcc_block_on(__c, 1);
	mtx_lock(__m);
	return __woke ? thrd_success : thrd_timedout;
}

static void cnd_destroy(cnd_t *__c) {
	(void)__c;
}
#else
/*
 * M:N mtx/cnd. These are the cooperative primitives: a contended wait PARKS the
 * fiber (or the external shadow) via __mcc_mn_block and hands the pthread worker
 * back to its run loop, instead of sinking the worker into a kernel wait. All
 * shared state (the lock flag, the wait tokens, the ready queue) is guarded by
 * the single scheduler lock __mcc_mn_lock, which __mcc_mn_block keeps held
 * across the fiber<->scheduler swap, so there is no window for a lost wakeup.
 */
static int mtx_init(mtx_t *__m, int __type) {
	__m->locked = 0;
	__m->type = __type;
	__m->rec = 0;
	__m->owner = (__mcc_fiber *)0;
	return thrd_success;
}

static int mtx_lock(mtx_t *__m) {
	__mcc_fiber *__self;
	__mcc_need_init();
	__self = __mcc_current();
	__MCC_LOCK();
	if (__m->locked && (__m->type & mtx_recursive) && __m->owner == __self) {
		__m->rec++;
		__MCC_UNLOCK();
		return thrd_success;
	}
	while (__m->locked)
		__mcc_mn_block(__m);
	__m->locked = 1;
	__m->owner = __self;
	__m->rec = 1;
	__MCC_UNLOCK();
	return thrd_success;
}

static int mtx_trylock(mtx_t *__m) {
	__mcc_fiber *__self;
	__mcc_need_init();
	__self = __mcc_current();
	__MCC_LOCK();
	if (__m->locked) {
		if ((__m->type & mtx_recursive) && __m->owner == __self) {
			__m->rec++;
			__MCC_UNLOCK();
			return thrd_success;
		}
		__MCC_UNLOCK();
		return thrd_busy;
	}
	__m->locked = 1;
	__m->owner = __self;
	__m->rec = 1;
	__MCC_UNLOCK();
	return thrd_success;
}

static int mtx_timedlock(mtx_t *__m, const struct timespec *__ts) {
	(void)__ts;
	return mtx_lock(__m);
}

static int mtx_unlock(mtx_t *__m) {
	__MCC_LOCK();
	if ((__m->type & mtx_recursive) && __m->rec > 1) {
		__m->rec--;
		__MCC_UNLOCK();
		return thrd_success;
	}
	__m->locked = 0;
	__m->owner = (__mcc_fiber *)0;
	__m->rec = 0;
	__mcc_mn_wake_one(__m);
	__MCC_UNLOCK();
	return thrd_success;
}

static void mtx_destroy(mtx_t *__m) {
	(void)__m;
}

static int cnd_init(cnd_t *__c) {
	__c->dummy = 0;
	return thrd_success;
}

static int cnd_signal(cnd_t *__c) {
	__MCC_LOCK();
	__mcc_mn_wake_one(__c);
	__MCC_UNLOCK();
	return thrd_success;
}

static int cnd_broadcast(cnd_t *__c) {
	__MCC_LOCK();
	__mcc_mn_wake_all(__c);
	__MCC_UNLOCK();
	return thrd_success;
}

static int cnd_wait(cnd_t *__c, mtx_t *__m) {
	__mcc_need_init();
	__mcc_current();
	__MCC_LOCK();
	__m->locked = 0;
	__m->owner = (__mcc_fiber *)0;
	__m->rec = 0;
	__mcc_mn_wake_one(__m);
	__mcc_mn_block(__c);
	__MCC_UNLOCK();
	mtx_lock(__m);
	return thrd_success;
}

static int cnd_timedwait(cnd_t *__c, mtx_t *__m, const struct timespec *__ts) {
	(void)__ts;
	cnd_wait(__c, __m);
	return thrd_success;
}

static void cnd_destroy(cnd_t *__c) {
	(void)__c;
}
#endif

static int tss_create(tss_t *__key, tss_dtor_t __dtor) {
#ifdef MCC_COOP_MT
	int __k = __atomic_fetch_add(&__mcc_tss_next, 1, __ATOMIC_ACQ_REL);
	if (__k >= __MCC_COOP_MAX_TSS) {
		__atomic_fetch_sub(&__mcc_tss_next, 1, __ATOMIC_ACQ_REL);
		return thrd_error;
	}
	*__key = __k;
	__mcc_tss_dtor[__k] = __dtor;
	return thrd_success;
#else
	if (__mcc_tss_next >= __MCC_COOP_MAX_TSS)
		return thrd_error;
	*__key = __mcc_tss_next++;
	__mcc_tss_dtor[*__key] = __dtor;
	return thrd_success;
#endif
}

static void *tss_get(tss_t __key) {
	__mcc_need_init();
	if (__key < 0 || __key >= __MCC_COOP_MAX_TSS)
		return (void *)0;
	return __MCC_SELF()->tss[__key];
}

static int tss_set(tss_t __key, void *__val) {
	__mcc_need_init();
	if (__key < 0 || __key >= __MCC_COOP_MAX_TSS)
		return thrd_error;
	__MCC_SELF()->tss[__key] = __val;
	return thrd_success;
}

static void tss_delete(tss_t __key) {
	if (__key >= 0 && __key < __MCC_COOP_MAX_TSS)
		__mcc_tss_dtor[__key] = (tss_dtor_t)0;
}

#endif
