#ifndef _SCHED_H
#define _SCHED_H

/*
 * Minimal <sched.h> for the mcc WIN32/PE target: just sched_yield, mapped onto
 * the Win32 SwitchToThread. Enough for the <pthread.h>/<threads.h> shims
 * (thrd_yield / sched_yield). Not a full POSIX scheduling interface.
 */

int __stdcall SwitchToThread(void);

static inline int sched_yield(void)
{
	SwitchToThread();
	return 0;
}

#endif /* _SCHED_H */
