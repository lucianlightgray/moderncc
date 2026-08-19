#ifndef _MCC_COOP_THREADS_H
#define _MCC_COOP_THREADS_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

extern void *malloc(size_t);
extern void *calloc(size_t, size_t);
extern void free(void *);
extern _Noreturn void abort(void);
extern _Noreturn void exit(int);
extern long write(int, const void *, size_t);

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
	void *blocked_on;
	struct __mcc_fiber *qnext;
	struct __mcc_fiber *all_next;
	void *tss[__MCC_COOP_MAX_TSS];
} __mcc_fiber;

typedef __mcc_fiber *thrd_t;
typedef int (*thrd_start_t)(void *);
typedef void (*tss_dtor_t)(void *);
typedef int tss_t;

typedef struct {
	int locked;
	int type;
	int rec;
	__mcc_fiber *owner;
} mtx_t;

typedef struct {
	int dummy;
} cnd_t;

typedef int once_flag;
#define ONCE_FLAG_INIT 0
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

#else
#error "mcc_coop_threads.h: no __mcc_ctx_swap/__mcc_ctx_make backend for this target -- add the per-target [X] context switch (T-lin-10001 [C] core; arm64/win/riscv64 owned by mac/win)."
#endif

static __mcc_fiber __mcc_main;
static __mcc_fiber *__mcc_cur = (__mcc_fiber *)0;
static __mcc_fiber *__mcc_ready_head = (__mcc_fiber *)0;
static __mcc_fiber *__mcc_ready_tail = (__mcc_fiber *)0;
static __mcc_fiber *__mcc_all = (__mcc_fiber *)0;
static __mcc_fiber *__mcc_zombie = (__mcc_fiber *)0;
static int __mcc_inited = 0;
static int __mcc_tss_next = 0;
static tss_dtor_t __mcc_tss_dtor[__MCC_COOP_MAX_TSS];

static void __mcc_need_init(void) {
	if (__mcc_inited)
		return;
	__mcc_inited = 1;
	__mcc_main.state = __MCC_F_RUNNABLE;
	__mcc_main.all_next = __mcc_all;
	__mcc_all = &__mcc_main;
	__mcc_cur = &__mcc_main;
}

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
	if (__mcc_zombie) {
		__mcc_all_remove(__mcc_zombie);
		free(__mcc_zombie->stack);
		free(__mcc_zombie);
		__mcc_zombie = (__mcc_fiber *)0;
	}
}

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
	__mcc_cur->state = __MCC_F_BLOCKED;
	__mcc_cur->blocked_on = __token;
	__next = __mcc_ready_pop();
	if (!__next) {
		if (__timed) {
			__mcc_cur->state = __MCC_F_RUNNABLE;
			__mcc_cur->blocked_on = (void *)0;
			return 0;
		}
		__mcc_deadlock();
	}
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
	__next = __mcc_ready_pop();
	if (!__next)
		return;
	__mcc_ready_push(__mcc_cur);
	__mcc_switch(__next);
}

static void __mcc_fiber_start(void) {
	__mcc_fiber *__self = __mcc_cur;
	__mcc_fiber *__next;
	__self->result = __self->fn(__self->arg);
	__self->state = __MCC_F_DONE;
	__mcc_wake_all(__self);
	if (__self->detached)
		__mcc_zombie = __self;
	__next = __mcc_ready_pop();
	if (!__next)
		__next = &__mcc_main;
	__mcc_cur = __next;
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
	__f->all_next = __mcc_all;
	__mcc_all = __f;
	__mcc_ready_push(__f);
	*__thr = __f;
	return thrd_success;
}

static int thrd_join(thrd_t __thr, int *__res) {
	__mcc_need_init();
	while (__thr->state != __MCC_F_DONE)
		__mcc_block_on(__thr, 0);
	if (__res)
		*__res = __thr->result;
	__mcc_all_remove(__thr);
	free(__thr->stack);
	free(__thr);
	return thrd_success;
}

static int thrd_detach(thrd_t __thr) {
	__mcc_need_init();
	if (__thr->state == __MCC_F_DONE) {
		__mcc_all_remove(__thr);
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
	return __mcc_cur;
}

static void thrd_yield(void) {
	__mcc_yield();
}

static int thrd_sleep(const struct timespec *__dur, struct timespec *__rem) {
	(void)__dur;
	if (__rem) {
		__rem->tv_sec = 0;
		__rem->tv_nsec = 0;
	}
	__mcc_yield();
	return 0;
}

_Noreturn static void thrd_exit(int __res) {
	__mcc_fiber *__self;
	__mcc_fiber *__next;
	__mcc_need_init();
	__self = __mcc_cur;
	__self->result = __res;
	__self->state = __MCC_F_DONE;
	__mcc_wake_all(__self);
	if (__self->detached)
		__mcc_zombie = __self;
	__next = __mcc_ready_pop();
	if (!__next)
		__next = &__mcc_main;
	if (__self == &__mcc_main) {
		while (__next) {
			__mcc_switch(__next);
			__next = __mcc_ready_pop();
		}
		exit(__res);
	}
	__mcc_cur = __next;
	__mcc_ctx_swap(&__self->sp, __next->sp);
	for (;;)
		;
}

static void call_once(once_flag *__flag, void (*__func)(void)) {
	if (*__flag)
		return;
	*__flag = 1;
	__func();
}

static int mtx_init(mtx_t *__m, int __type) {
	__m->locked = 0;
	__m->type = __type;
	__m->rec = 0;
	__m->owner = (__mcc_fiber *)0;
	return thrd_success;
}

static int mtx_lock(mtx_t *__m) {
	__mcc_need_init();
	if (__m->locked && (__m->type & mtx_recursive) && __m->owner == __mcc_cur) {
		__m->rec++;
		return thrd_success;
	}
	while (__m->locked)
		__mcc_block_on(__m, 0);
	__m->locked = 1;
	__m->owner = __mcc_cur;
	__m->rec = 1;
	return thrd_success;
}

static int mtx_trylock(mtx_t *__m) {
	__mcc_need_init();
	if (__m->locked) {
		if ((__m->type & mtx_recursive) && __m->owner == __mcc_cur) {
			__m->rec++;
			return thrd_success;
		}
		return thrd_busy;
	}
	__m->locked = 1;
	__m->owner = __mcc_cur;
	__m->rec = 1;
	return thrd_success;
}

static int mtx_timedlock(mtx_t *__m, const struct timespec *__ts) {
	(void)__ts;
	__mcc_need_init();
	if (__m->locked && (__m->type & mtx_recursive) && __m->owner == __mcc_cur) {
		__m->rec++;
		return thrd_success;
	}
	while (__m->locked) {
		if (!__mcc_block_on(__m, 1))
			return thrd_timedout;
	}
	__m->locked = 1;
	__m->owner = __mcc_cur;
	__m->rec = 1;
	return thrd_success;
}

static int mtx_unlock(mtx_t *__m) {
	if ((__m->type & mtx_recursive) && __m->rec > 1) {
		__m->rec--;
		return thrd_success;
	}
	__m->locked = 0;
	__m->owner = (__mcc_fiber *)0;
	__m->rec = 0;
	__mcc_wake_one(__m);
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
	__mcc_wake_one(__c);
	return thrd_success;
}

static int cnd_broadcast(cnd_t *__c) {
	__mcc_wake_all(__c);
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

static int tss_create(tss_t *__key, tss_dtor_t __dtor) {
	if (__mcc_tss_next >= __MCC_COOP_MAX_TSS)
		return thrd_error;
	*__key = __mcc_tss_next++;
	__mcc_tss_dtor[*__key] = __dtor;
	return thrd_success;
}

static void *tss_get(tss_t __key) {
	__mcc_need_init();
	if (__key < 0 || __key >= __MCC_COOP_MAX_TSS)
		return (void *)0;
	return __mcc_cur->tss[__key];
}

static int tss_set(tss_t __key, void *__val) {
	__mcc_need_init();
	if (__key < 0 || __key >= __MCC_COOP_MAX_TSS)
		return thrd_error;
	__mcc_cur->tss[__key] = __val;
	return thrd_success;
}

static void tss_delete(tss_t __key) {
	if (__key >= 0 && __key < __MCC_COOP_MAX_TSS)
		__mcc_tss_dtor[__key] = (tss_dtor_t)0;
}

#endif
