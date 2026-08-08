#ifndef MCC_TASK_H
#define MCC_TASK_H

enum {
	MCC_TASK_READY = 0,
	MCC_TASK_YIELDED = 1,
	MCC_TASK_DONE = 2,
	MCC_TASK_FAILED = 3
};

typedef struct MccTask {
	int (*tick)(struct MccTask *t);
	void *ctx;
	int state;
	int resume;
	long ticks;
	struct MccTask *next;
} MccTask;

typedef struct MccSched {
	MccTask *head;
	MccTask *tail;
	int quit;
	long rounds;
} MccSched;

static void mcc_task_init(MccTask *t, int (*tick)(MccTask *), void *ctx) {
	if (!t)
		return;
	t->tick = tick;
	t->ctx = ctx;
	t->state = MCC_TASK_READY;
	t->resume = 0;
	t->ticks = 0;
	t->next = NULL;
}

static void mcc_sched_init(MccSched *s) {
	if (!s)
		return;
	s->head = NULL;
	s->tail = NULL;
	s->quit = 0;
	s->rounds = 0;
}

static void mcc_sched_add(MccSched *s, MccTask *t) {
	if (!s || !t)
		return;
	t->next = NULL;
	if (s->tail)
		s->tail->next = t;
	else
		s->head = t;
	s->tail = t;
}

static int mcc_sched_pending(const MccSched *s) {
	const MccTask *t;
	int n = 0;
	if (!s)
		return 0;
	for (t = s->head; t; t = t->next)
		n++;
	return n;
}

static void mcc_sched_quit(MccSched *s) {
	if (s)
		s->quit = 1;
}

/* One round: every queued task is ticked exactly once, in queue order, and
 * survivors are re-appended in that same order. Detaching the whole list first
 * is what makes the order stable when a task completes mid-round. */
static int mcc_sched_step(MccSched *s) {
	MccTask *run, *t, *nx;
	if (!s)
		return 0;
	run = s->head;
	s->head = NULL;
	s->tail = NULL;
	for (t = run; t; t = nx) {
		nx = t->next;
		t->next = NULL;
		t->ticks++;
		t->state = t->tick ? t->tick(t) : MCC_TASK_FAILED;
		if (t->state == MCC_TASK_DONE || t->state == MCC_TASK_FAILED)
			continue;
		mcc_sched_add(s, t);
	}
	s->rounds++;
	return mcc_sched_pending(s);
}

/* The quit flag is read here, between ticks, never inside one. That is the
 * whole reason a task is a tick rather than a thread: a stop is exact, leaves
 * resumable state, and needs no cancellation point inside the work. */
static int mcc_sched_run(MccSched *s, int maxrounds) {
	int n, r = 0;
	if (!s)
		return 0;
	n = mcc_sched_pending(s);
	while (n > 0 && !s->quit && (!maxrounds || r < maxrounds)) {
		n = mcc_sched_step(s);
		r++;
	}
	return n;
}

#endif
