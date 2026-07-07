#ifndef _SCHED_H
#define _SCHED_H

int __stdcall SwitchToThread(void);

static inline int sched_yield(void) {
	SwitchToThread();
	return 0;
}

#endif
