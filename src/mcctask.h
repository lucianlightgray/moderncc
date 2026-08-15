#ifndef MCC_TASK_H
#define MCC_TASK_H

enum {
	MCC_TASK_READY = 0,
	MCC_TASK_YIELDED = 1,
	MCC_TASK_DONE = 2,
	MCC_TASK_FAILED = 3,
	MCC_TASK_BLOCKED = 4
};

/* `blocked_on` is an opaque address the task names when it cannot proceed: the
 * lock, queue or flag it is waiting for. It is not dereferenced here and its
 * only property that matters is identity, so a caller may use any stable
 * address as a token.
 *
 * The distinction it buys is the one YIELDED cannot express. YIELDED means
 * "made progress, call me again"; without a third answer, a task waiting on a
 * condition had to say the same thing and burned a tick every round forever,
 * and a scheduler with nothing runnable was indistinguishable from a busy one.
 * That is why mccthread.h grew npred/ready/waiting_on_world beside MccTask
 * rather than inside it, and why the JIT pool cannot currently say whether it
 * is working or wedged. */
typedef struct MccTask {
	int (*tick)(struct MccTask *t);
	void *ctx;
	int state;
	int resume;
	long ticks;
	void *blocked_on;
	struct MccTask *next;
} MccTask;

typedef struct MccSched {
	MccTask *head;
	MccTask *tail;
	int quit;
	long rounds;
	long stalls;
} MccSched;

static void mcc_task_init(MccTask *t, int (*tick)(MccTask *), void *ctx) {
	if (!t)
		return;
	t->tick = tick;
	t->ctx = ctx;
	t->state = MCC_TASK_READY;
	t->resume = 0;
	t->ticks = 0;
	t->blocked_on = NULL;
	t->next = NULL;
}

/* Called by a tick that is about to return MCC_TASK_BLOCKED. Kept separate
 * from the return value so the address cannot be forgotten: a task that
 * returns BLOCKED without naming a blocker can never be woken by
 * mcc_sched_wake and would park forever. */
static void mcc_task_block(MccTask *t, void *on) {
	if (!t)
		return;
	t->blocked_on = on;
	t->state = MCC_TASK_BLOCKED;
}

static void mcc_sched_init(MccSched *s) {
	if (!s)
		return;
	s->head = NULL;
	s->tail = NULL;
	s->quit = 0;
	s->rounds = 0;
	s->stalls = 0;
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

static int mcc_sched_blocked(const MccSched *s) {
	const MccTask *t;
	int n = 0;
	if (!s)
		return 0;
	for (t = s->head; t; t = t->next)
		if (t->state == MCC_TASK_BLOCKED)
			n++;
	return n;
}

static int mcc_sched_runnable(const MccSched *s) {
	const MccTask *t;
	int n = 0;
	if (!s)
		return 0;
	for (t = s->head; t; t = t->next)
		if (t->state != MCC_TASK_BLOCKED)
			n++;
	return n;
}

/* Wake every task parked on `on` and report how many. Waking an address no
 * task named is not an error and wakes nothing -- otherwise every unblock site
 * would have to know whether anyone was listening, which is exactly the
 * bookkeeping a blocked-on address exists to remove. */
static int mcc_sched_wake(MccSched *s, void *on) {
	MccTask *t;
	int n = 0;
	if (!s)
		return 0;
	for (t = s->head; t; t = t->next) {
		if (t->state != MCC_TASK_BLOCKED || t->blocked_on != on)
			continue;
		t->blocked_on = NULL;
		t->state = MCC_TASK_READY;
		n++;
	}
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
		if (t->state == MCC_TASK_BLOCKED) {
			mcc_sched_add(s, t);
			continue;
		}
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
		if (mcc_sched_runnable(s) == 0) {
			s->stalls++;
			break;
		}
		n = mcc_sched_step(s);
		r++;
	}
	return n;
}

#endif
