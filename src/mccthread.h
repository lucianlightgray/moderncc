#ifndef MCC_THREAD_H
#define MCC_THREAD_H

#include <string.h>

enum {
	MCC_THR_NONE = 0,
	MCC_THR_CREATE,
	MCC_THR_JOIN,
	MCC_THR_DETACH,
	MCC_THR_EXIT,
	MCC_THR_LOCK,
	MCC_THR_RDLOCK,
	MCC_THR_TRYLOCK,
	MCC_THR_UNLOCK,
	MCC_THR_WAIT,
	MCC_THR_SIGNAL,
	MCC_THR_BROADCAST,
	MCC_THR_BARRIER,
	MCC_THR_ONCE,
	MCC_THR_TLS,
	MCC_THR_SELF,
	MCC_THR_EQUAL,
	MCC_THR_YIELD,
	MCC_THR_NOP,
	MCC_THR_N
};

typedef struct MccThrName {
	const char *name;
	int op;
} MccThrName;

static const MccThrName mcc_thread_tab[] = {
		{"pthread_create", MCC_THR_CREATE},
		{"thrd_create", MCC_THR_CREATE},
		{"pthread_join", MCC_THR_JOIN},
		{"thrd_join", MCC_THR_JOIN},
		{"pthread_detach", MCC_THR_DETACH},
		{"thrd_detach", MCC_THR_DETACH},
		{"pthread_exit", MCC_THR_EXIT},
		{"thrd_exit", MCC_THR_EXIT},
		{"pthread_mutex_lock", MCC_THR_LOCK},
		{"mtx_lock", MCC_THR_LOCK},
		{"pthread_rwlock_wrlock", MCC_THR_LOCK},
		{"pthread_spin_lock", MCC_THR_LOCK},
		{"pthread_rwlock_rdlock", MCC_THR_RDLOCK},
		{"pthread_mutex_trylock", MCC_THR_TRYLOCK},
		{"mtx_trylock", MCC_THR_TRYLOCK},
		{"pthread_rwlock_tryrdlock", MCC_THR_TRYLOCK},
		{"pthread_rwlock_trywrlock", MCC_THR_TRYLOCK},
		{"pthread_spin_trylock", MCC_THR_TRYLOCK},
		{"pthread_mutex_unlock", MCC_THR_UNLOCK},
		{"mtx_unlock", MCC_THR_UNLOCK},
		{"pthread_rwlock_unlock", MCC_THR_UNLOCK},
		{"pthread_spin_unlock", MCC_THR_UNLOCK},
		{"pthread_cond_wait", MCC_THR_WAIT},
		{"pthread_cond_timedwait", MCC_THR_WAIT},
		{"cnd_wait", MCC_THR_WAIT},
		{"cnd_timedwait", MCC_THR_WAIT},
		{"pthread_cond_signal", MCC_THR_SIGNAL},
		{"cnd_signal", MCC_THR_SIGNAL},
		{"pthread_cond_broadcast", MCC_THR_BROADCAST},
		{"cnd_broadcast", MCC_THR_BROADCAST},
		{"pthread_barrier_wait", MCC_THR_BARRIER},
		{"pthread_once", MCC_THR_ONCE},
		{"call_once", MCC_THR_ONCE},
		{"pthread_key_create", MCC_THR_TLS},
		{"pthread_key_delete", MCC_THR_TLS},
		{"pthread_getspecific", MCC_THR_TLS},
		{"pthread_setspecific", MCC_THR_TLS},
		{"tss_create", MCC_THR_TLS},
		{"tss_delete", MCC_THR_TLS},
		{"tss_get", MCC_THR_TLS},
		{"tss_set", MCC_THR_TLS},
		{"pthread_self", MCC_THR_SELF},
		{"thrd_current", MCC_THR_SELF},
		{"pthread_equal", MCC_THR_EQUAL},
		{"thrd_equal", MCC_THR_EQUAL},
		{"sched_yield", MCC_THR_YIELD},
		{"pthread_yield", MCC_THR_YIELD},
		{"thrd_yield", MCC_THR_YIELD},
		{"thrd_sleep", MCC_THR_YIELD},
		{"pthread_mutex_init", MCC_THR_NOP},
		{"pthread_mutex_destroy", MCC_THR_NOP},
		{"pthread_mutexattr_init", MCC_THR_NOP},
		{"pthread_mutexattr_destroy", MCC_THR_NOP},
		{"pthread_mutexattr_settype", MCC_THR_NOP},
		{"pthread_cond_init", MCC_THR_NOP},
		{"pthread_cond_destroy", MCC_THR_NOP},
		{"pthread_condattr_init", MCC_THR_NOP},
		{"pthread_condattr_destroy", MCC_THR_NOP},
		{"pthread_rwlock_init", MCC_THR_NOP},
		{"pthread_rwlock_destroy", MCC_THR_NOP},
		{"pthread_barrier_init", MCC_THR_NOP},
		{"pthread_barrier_destroy", MCC_THR_NOP},
		{"pthread_attr_init", MCC_THR_NOP},
		{"pthread_attr_destroy", MCC_THR_NOP},
		{"pthread_attr_setdetachstate", MCC_THR_NOP},
		{"pthread_spin_init", MCC_THR_NOP},
		{"pthread_spin_destroy", MCC_THR_NOP},
		{"mtx_init", MCC_THR_NOP},
		{"mtx_destroy", MCC_THR_NOP},
		{"cnd_init", MCC_THR_NOP},
		{"cnd_destroy", MCC_THR_NOP}};

#define MCC_THREAD_TAB_N ((int)(sizeof mcc_thread_tab / sizeof mcc_thread_tab[0]))

static int mcc_thread_classify(const char *name) {
	int i;
	if (!name || !name[0])
		return MCC_THR_NONE;
	for (i = 0; i < MCC_THREAD_TAB_N; i++)
		if (!strcmp(name, mcc_thread_tab[i].name))
			return mcc_thread_tab[i].op;
	return MCC_THR_NONE;
}

static const char *mcc_thread_op_name(int op) {
	switch (op) {
	case MCC_THR_CREATE:
		return "create";
	case MCC_THR_JOIN:
		return "join";
	case MCC_THR_DETACH:
		return "detach";
	case MCC_THR_EXIT:
		return "exit";
	case MCC_THR_LOCK:
		return "lock";
	case MCC_THR_RDLOCK:
		return "rdlock";
	case MCC_THR_TRYLOCK:
		return "trylock";
	case MCC_THR_UNLOCK:
		return "unlock";
	case MCC_THR_WAIT:
		return "wait";
	case MCC_THR_SIGNAL:
		return "signal";
	case MCC_THR_BROADCAST:
		return "broadcast";
	case MCC_THR_BARRIER:
		return "barrier";
	case MCC_THR_ONCE:
		return "once";
	case MCC_THR_TLS:
		return "tls";
	case MCC_THR_SELF:
		return "self";
	case MCC_THR_EQUAL:
		return "equal";
	case MCC_THR_YIELD:
		return "yield";
	case MCC_THR_NOP:
		return "nop";
	default:
		return "none";
	}
}

static int mcc_thread_op_supported(int op) {
	switch (op) {
	case MCC_THR_CREATE:
	case MCC_THR_JOIN:
	case MCC_THR_DETACH:
	case MCC_THR_LOCK:
	case MCC_THR_RDLOCK:
	case MCC_THR_UNLOCK:
	case MCC_THR_WAIT:
	case MCC_THR_SIGNAL:
	case MCC_THR_BROADCAST:
	case MCC_THR_YIELD:
	case MCC_THR_NOP:
		return 1;
	default:
		return 0;
	}
}

#ifdef MCC_THREAD_ENGINE

#define MCC_DEP_MAXNODE 64
#define MCC_DEP_MAXSUCC 16
#define MCC_DEP_MAXCELL 32

enum {
	MCC_DEP_NEW = 0,
	MCC_DEP_QUEUED = 1,
	MCC_DEP_DONE = 2,
	MCC_DEP_FAILED = 3
};

enum {
	MCC_DEP_OK = 0,
	MCC_DEP_STUCK = 1,
	MCC_DEP_REFUSED = 2,
	MCC_DEP_FULL = 3
};

enum {
	MCC_DEP_R_NONE = 0,
	MCC_DEP_R_UNBOUNDED_DETACH = 1,
	MCC_DEP_R_INTERLEAVED_OUTPUT = 2,
	MCC_DEP_R_UNSUPPORTED_OP = 3
};

typedef int (*MccDepReadyFn)(void *user);

typedef struct MccDepNode {
	MccTask task;
	int id;
	int npred;
	int nsucc;
	int succ[MCC_DEP_MAXSUCC];
	MccDepReadyFn ready;
	void *ready_user;
	int world;
	uint32_t lane;
	int detached;
	int state;
} MccDepNode;

typedef struct MccDepGraph {
	MccSched s;
	MccDepNode *n[MCC_DEP_MAXNODE];
	int nn;
	long admitted;
	long stalls;
	long edges;
	long elided;
	long serialized;
	long refills;
	int verdict;
	int refusal;
	int (*world)(void *user);
	void *world_user;
	MccEffectLog *log;
	int64_t cell[MCC_DEP_MAXCELL];
} MccDepGraph;

static const char *mcc_dep_verdict_name(int v) {
	switch (v) {
	case MCC_DEP_OK:
		return "ok";
	case MCC_DEP_STUCK:
		return "stuck";
	case MCC_DEP_REFUSED:
		return "refused";
	case MCC_DEP_FULL:
		return "graph-full";
	default:
		return "?";
	}
}

static const char *mcc_dep_refusal_name(int r) {
	switch (r) {
	case MCC_DEP_R_UNBOUNDED_DETACH:
		return "detached-slice-not-provably-finite";
	case MCC_DEP_R_INTERLEAVED_OUTPUT:
		return "concurrent-output-on-one-channel";
	case MCC_DEP_R_UNSUPPORTED_OP:
		return "primitive-has-no-engine-translation";
	default:
		return "none";
	}
}

static void mcc_dep_init(MccDepGraph *g) {
	if (!g)
		return;
	memset(g, 0, sizeof *g);
	mcc_sched_init(&g->s);
}

static int mcc_dep_publish(MccDepGraph *g, MccDepNode *d,
													 int (*tick)(MccTask *), void *ctx) {
	if (!g || !d)
		return -1;
	if (g->nn >= MCC_DEP_MAXNODE) {
		g->verdict = MCC_DEP_FULL;
		return -1;
	}
	memset(d, 0, sizeof *d);
	mcc_task_init(&d->task, tick, ctx);
	d->id = g->nn;
	d->lane = (uint32_t)g->nn;
	d->state = MCC_DEP_NEW;
	g->n[g->nn++] = d;
	return d->id;
}

static MccDepNode *mcc_dep_get(MccDepGraph *g, int id) {
	if (!g || id < 0 || id >= g->nn)
		return NULL;
	return g->n[id];
}

static int mcc_dep_has_edge(MccDepGraph *g, int from, int to) {
	MccDepNode *f = mcc_dep_get(g, from);
	int i;
	if (!f)
		return 0;
	for (i = 0; i < f->nsucc; i++)
		if (f->succ[i] == to)
			return 1;
	return 0;
}

static int mcc_dep_edge(MccDepGraph *g, int from, int to) {
	MccDepNode *f = mcc_dep_get(g, from);
	MccDepNode *t = mcc_dep_get(g, to);
	if (!f || !t || from == to)
		return 0;
	if (mcc_dep_has_edge(g, from, to))
		return 1;
	if (f->nsucc >= MCC_DEP_MAXSUCC) {
		g->verdict = MCC_DEP_FULL;
		return 0;
	}
	f->succ[f->nsucc++] = to;
	t->npred++;
	g->edges++;
	return 1;
}

static int mcc_dep_reach(MccDepGraph *g, int from, int to, unsigned char *seen) {
	MccDepNode *f = mcc_dep_get(g, from);
	int i;
	if (!f || seen[from])
		return 0;
	seen[from] = 1;
	for (i = 0; i < f->nsucc; i++) {
		if (f->succ[i] == to)
			return 1;
		if (mcc_dep_reach(g, f->succ[i], to, seen))
			return 1;
	}
	return 0;
}

static int mcc_dep_reachable(MccDepGraph *g, int from, int to) {
	unsigned char seen[MCC_DEP_MAXNODE];
	if (!g || from == to)
		return 0;
	memset(seen, 0, sizeof seen);
	return mcc_dep_reach(g, from, to, seen);
}

static int mcc_dep_concurrent(MccDepGraph *g, int a, int b) {
	if (!g || a == b)
		return 0;
	return !mcc_dep_reachable(g, a, b) && !mcc_dep_reachable(g, b, a);
}

static int mcc_dep_pred(MccDepGraph *g, int id, MccDepReadyFn fn, void *user,
												int world) {
	MccDepNode *d = mcc_dep_get(g, id);
	if (!d)
		return 0;
	d->ready = fn;
	d->ready_user = user;
	d->world = world;
	return 1;
}

static int mcc_dep_detach(MccDepGraph *g, int id, int bounded) {
	MccDepNode *d = mcc_dep_get(g, id);
	if (!d)
		return 0;
	if (!bounded) {
		g->verdict = MCC_DEP_REFUSED;
		g->refusal = MCC_DEP_R_UNBOUNDED_DETACH;
		return 0;
	}
	d->detached = 1;
	return 1;
}

static int mcc_dep_mutex(MccDepGraph *g, int a, int b, int disjoint) {
	if (!g)
		return 0;
	if (disjoint > 0) {
		g->elided++;
		return 0;
	}
	if (!mcc_dep_edge(g, a, b))
		return 0;
	g->serialized++;
	return 1;
}

static void mcc_dep_world_source(MccDepGraph *g, int (*fn)(void *), void *user) {
	if (!g)
		return;
	g->world = fn;
	g->world_user = user;
}

static int mcc_dep_admit(MccDepGraph *g) {
	int i, n = 0;
	for (i = 0; i < g->nn; i++) {
		MccDepNode *d = g->n[i];
		if (!d || d->state != MCC_DEP_NEW || d->npred)
			continue;
		if (d->ready && !d->ready(d->ready_user)) {
			g->stalls++;
			continue;
		}
		d->state = MCC_DEP_QUEUED;
		mcc_sched_add(&g->s, &d->task);
		g->admitted++;
		n++;
	}
	return n;
}

static void mcc_dep_retire(MccDepGraph *g) {
	int i, j;
	for (i = 0; i < g->nn; i++) {
		MccDepNode *d = g->n[i];
		if (!d || d->state != MCC_DEP_QUEUED)
			continue;
		if (d->task.state != MCC_TASK_DONE && d->task.state != MCC_TASK_FAILED)
			continue;
		d->state = d->task.state == MCC_TASK_DONE ? MCC_DEP_DONE : MCC_DEP_FAILED;
		for (j = 0; j < d->nsucc; j++) {
			MccDepNode *sc = mcc_dep_get(g, d->succ[j]);
			if (sc && sc->npred)
				sc->npred--;
		}
	}
}

static int mcc_dep_unfinished(const MccDepGraph *g) {
	int i, n = 0;
	if (!g)
		return 0;
	for (i = 0; i < g->nn; i++)
		if (g->n[i] && g->n[i]->state != MCC_DEP_DONE &&
				g->n[i]->state != MCC_DEP_FAILED)
			n++;
	return n;
}

static int mcc_dep_waiting_on_world(const MccDepGraph *g) {
	int i;
	if (!g)
		return 0;
	for (i = 0; i < g->nn; i++)
		if (g->n[i] && g->n[i]->state == MCC_DEP_NEW && g->n[i]->world)
			return 1;
	return 0;
}

static int mcc_dep_run(MccDepGraph *g, int maxrounds) {
	int r = 0, idle = 0;
	if (!g)
		return MCC_DEP_STUCK;
	if (g->verdict == MCC_DEP_REFUSED || g->verdict == MCC_DEP_FULL)
		return g->verdict;
	for (;;) {
		mcc_dep_admit(g);
		if (!mcc_sched_pending(&g->s)) {
			if (!mcc_dep_unfinished(g)) {
				g->verdict = MCC_DEP_OK;
				return g->verdict;
			}
			if (mcc_dep_waiting_on_world(g) && g->world &&
					g->world(g->world_user)) {
				g->refills++;
				idle = 0;
				continue;
			}
			if (++idle > 1) {
				g->verdict = MCC_DEP_STUCK;
				return g->verdict;
			}
			continue;
		}
		idle = 0;
		mcc_sched_step(&g->s);
		mcc_dep_retire(g);
		if (maxrounds && ++r >= maxrounds) {
			g->verdict = MCC_DEP_STUCK;
			return g->verdict;
		}
	}
}

static int mcc_dep_lane_node(MccDepGraph *g, uint32_t lane) {
	int i;
	if (!g)
		return -1;
	for (i = 0; i < g->nn; i++)
		if (g->n[i] && g->n[i]->lane == lane)
			return i;
	return -1;
}

static int mcc_thread_output_conflict(MccDepGraph *g, const MccEffectLog *L,
																			int *pa, int *pb) {
	int i, j, n = 0;
	if (!g || !L)
		return 0;
	for (i = 0; i < L->n; i++) {
		const MccEffect *a = &L->e[i];
		if (!mcc_effect_output_kind(a->kind) ||
				a->space == MCC_EFFECT_SPACE_REGION)
			continue;
		for (j = i + 1; j < L->n; j++) {
			const MccEffect *b = &L->e[j];
			int na, nb;
			if (!mcc_effect_output_kind(b->kind) || b->space != a->space ||
					b->chan != a->chan || b->lane == a->lane)
				continue;
			na = mcc_dep_lane_node(g, a->lane);
			nb = mcc_dep_lane_node(g, b->lane);
			if (na < 0 || nb < 0 || !mcc_dep_concurrent(g, na, nb))
				continue;
			if (!n && pa && pb) {
				*pa = na;
				*pb = nb;
			}
			n++;
		}
	}
	if (n) {
		g->verdict = MCC_DEP_REFUSED;
		g->refusal = MCC_DEP_R_INTERLEAVED_OUTPUT;
	}
	return n;
}

static void mcc_thread_cell_fill(MccEffect *e, uint32_t kind, uint32_t lane,
																 int32_t cell, uint32_t site) {
	memset(e, 0, sizeof *e);
	e->kind = kind;
	e->space = MCC_EFFECT_SPACE_CELL;
	e->chan = cell;
	e->site = site;
	e->lane = lane;
	e->addr = cell * 8;
	e->width = 8;
	e->flags = MCC_EFFECT_F_VOLATILE;
}

static int mcc_thread_cell_store(MccDepGraph *g, uint32_t lane, int32_t cell,
																 int64_t v, uint32_t site) {
	MccEffect e;
	if (!g || cell < 0 || cell >= MCC_DEP_MAXCELL)
		return 0;
	mcc_thread_cell_fill(&e, MCC_EFFECT_STORE, lane, cell, site);
	e.value = v;
	e.prev = g->cell[cell];
	if (g->log && g->log->mode == MCC_EFFECT_REPLAY)
		return mcc_effect_replay(g->log, &e, NULL, 0);
	if (g->log && g->log->mode == MCC_EFFECT_RECORD &&
			!mcc_effect_record(g->log, &e, NULL, 0))
		return 0;
	g->cell[cell] = v;
	return 1;
}

static int64_t mcc_thread_cell_load(MccDepGraph *g, uint32_t lane, int32_t cell,
																		uint32_t site, int *ok) {
	MccEffect e;
	if (ok)
		*ok = 0;
	if (!g || cell < 0 || cell >= MCC_DEP_MAXCELL)
		return 0;
	mcc_thread_cell_fill(&e, MCC_EFFECT_LOAD, lane, cell, site);
	if (g->log && g->log->mode == MCC_EFFECT_REPLAY) {
		if (!mcc_effect_replay(g->log, &e, NULL, 0))
			return 0;
		if (ok)
			*ok = 1;
		return e.value;
	}
	e.value = g->cell[cell];
	if (g->log && g->log->mode == MCC_EFFECT_RECORD &&
			!mcc_effect_record(g->log, &e, NULL, 0))
		return 0;
	if (ok)
		*ok = 1;
	return e.value;
}

static int64_t mcc_thread_cell_rmw(MccDepGraph *g, uint32_t lane, int32_t cell,
																	 int64_t delta, uint32_t site, int *ok) {
	int lok = 0;
	int64_t old = mcc_thread_cell_load(g, lane, cell, site, &lok);
	if (ok)
		*ok = 0;
	if (!lok)
		return 0;
	if (!mcc_thread_cell_store(g, lane, cell, old + delta, site))
		return 0;
	if (ok)
		*ok = 1;
	return old;
}

static void mcc_thread_cell_undo(const MccEffect *e, const void *payload,
																 void *user) {
	MccDepGraph *g = (MccDepGraph *)user;
	(void)payload;
	if (!g || !e || e->space != MCC_EFFECT_SPACE_CELL ||
			e->kind != MCC_EFFECT_STORE)
		return;
	if (e->chan < 0 || e->chan >= MCC_DEP_MAXCELL)
		return;
	g->cell[e->chan] = e->prev;
}

#endif

#endif
