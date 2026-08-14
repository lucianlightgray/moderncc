/* See mccrt.h for why this exists. */

#include "mccrt.h"

#include "mccgpu.h"

#ifdef MCC_EMBED_JIT
/* Declared here rather than pulled from a JIT header: this file is also
 * compiled into tools that link mccgpu.c without the JIT, and the declaration
 * must not drag one in. */
void mccjit_shutdown(void);
#endif

/* The compiler side owns the GPU opt-in and the probe dispatch that decides
 * whether a device is usable. Tools that link mccgpu.c directly manage their
 * own device and define no such hook, so this is weak-by-arrangement: the
 * amalgamation sets it, everyone else leaves it null. */
static void (*mcc_rt_gpu_boot)(void);

void mcc_rt_set_gpu_boot(void (*fn)(void)) {
	mcc_rt_gpu_boot = fn;
}

static int mcc_rt_entered;
static int mcc_rt_stopped;

int mcc_rt_running(void) {
	return mcc_rt_entered && !mcc_rt_stopped;
}

void mcc_rt_enter(void) {
	if (mcc_rt_entered)
		return;
	mcc_rt_entered = 1;
	/* The single point at which a device may come up. It is a no-op unless the
	 * GPU was asked for, so an ordinary compile pays nothing. */
	if (mcc_rt_gpu_boot)
		mcc_rt_gpu_boot();
}

void mcc_rt_exit(void) {
	if (!mcc_rt_entered || mcc_rt_stopped)
		return;
	mcc_rt_stopped = 1;

	/* Order is the whole point, and it is fixed here rather than emerging from
	 * the sequence in which atexit handlers happened to be registered.
	 *
	 * The pool joins first: a worker that is still running can be inside a
	 * dispatch, and destroying the device under it is exactly the race the old
	 * arrangement left to chance. */
#ifdef MCC_EMBED_JIT
	mccjit_shutdown();
#endif

	/* Then the device. Reached from main rather than from an atexit handler, so
	 * it cannot race the driver's own unload. Idempotent: mccjit_shutdown()
	 * quiesces too, and the second call is a no-op by construction. */
	mcc_gpu_quiesce();
}
