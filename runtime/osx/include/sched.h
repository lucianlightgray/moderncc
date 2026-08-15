#ifndef _MCC_OSX_SCHED_H
#define _MCC_OSX_SCHED_H

#ifdef __cplusplus
extern "C" {
#endif

#define SCHED_OTHER 1
#define SCHED_FIFO 4
#define SCHED_RR 2

struct sched_param {
	int sched_priority;
	char __opaque[4];
};

int sched_yield(void);
int sched_get_priority_min(int);
int sched_get_priority_max(int);

#ifdef __cplusplus
}
#endif

#endif
