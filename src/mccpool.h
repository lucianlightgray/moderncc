#ifndef MCC_POOL_H
#define MCC_POOL_H

#if !MCC_HOST_WIN32
#include <pthread.h>
#endif
#include "mcctask.h"

#define MCC_POOL_MAX 64

typedef struct MccPoolJob {
	int (*tick)(void *ctx);
	void *ctx;
	void (*free_ctx)(void *ctx);
	struct MccPoolJob *next;
} MccPoolJob;

typedef struct MccPool {
	MccPoolJob *head, *tail;
	pthread_mutex_t qlock;
	pthread_cond_t qcond;
	int started;
	int nworkers;
	int quit;
	int hooked;
	int nth;
	pthread_t th[MCC_POOL_MAX];
	unsigned long nrun;
	unsigned long nenqueued;
	unsigned long nrefused;
	unsigned long nabandoned;
	unsigned long ndiscarded;
	unsigned long nticks_after_quit;
	int quit_seen;
	int verbose;
	const char *name;
	const char *cap_label;
	const char *cap_macro;
	void *(*job_begin)(void);
	void (*job_end)(void *token);
	void (*tick_lock)(void);
	void (*tick_unlock)(void);
} MccPool;

#define MCC_POOL_INIT(nm) \
	{ .qlock = PTHREAD_MUTEX_INITIALIZER, .qcond = PTHREAD_COND_INITIALIZER, \
		.name = (nm) }

static MccPoolJob *mcc_pool_job_new(int (*tick)(void *), void *ctx,
																		void (*free_ctx)(void *)) { MCC_TRACE("enter\n");
	MccPoolJob *job = mcc_malloc(sizeof *job);
	if (!job)
		{ MCC_TRACE("br\n"); return NULL; }
	job->tick = tick;
	job->ctx = ctx;
	job->free_ctx = free_ctx;
	job->next = NULL;
	return job;
}

static void *mcc_pool_worker(void *arg) { MCC_TRACE("enter\n");
	MccPool *p = (MccPool *)arg;
	for (;;) { MCC_TRACE("br\n");
		MccPoolJob *job;
		void *tok;
		pthread_mutex_lock(&p->qlock);
		while (!p->head && !p->quit)
			{ MCC_TRACE("br\n"); pthread_cond_wait(&p->qcond, &p->qlock); }
		job = p->head;
		if (!job) { MCC_TRACE("br\n");
			pthread_mutex_unlock(&p->qlock);
			break;
		}
		p->head = job->next;
		if (!p->head)
			{ MCC_TRACE("br\n"); p->tail = NULL; }
		pthread_mutex_unlock(&p->qlock);
		tok = p->job_begin ? p->job_begin() : NULL;
		for (;;) { MCC_TRACE("br\n");
			int tst, quit;
			pthread_mutex_lock(&p->qlock);
			quit = p->quit;
			pthread_mutex_unlock(&p->qlock);
			if (quit) { MCC_TRACE("br\n");
				pthread_mutex_lock(&p->qlock);
				p->nabandoned++;
				p->quit_seen = 1;
				pthread_mutex_unlock(&p->qlock);
				break;
			}
			if (p->tick_lock)
				{ MCC_TRACE("br\n"); p->tick_lock(); }
			tst = job->tick(job->ctx);
			if (p->tick_unlock)
				{ MCC_TRACE("br\n"); p->tick_unlock(); }
			pthread_mutex_lock(&p->qlock);
			if (p->quit)
				{ MCC_TRACE("br\n"); p->nticks_after_quit++; }
			pthread_mutex_unlock(&p->qlock);
			if (tst == MCC_TASK_DONE || tst == MCC_TASK_FAILED)
				{ MCC_TRACE("br\n"); break; }
		}
		if (p->job_end)
			{ MCC_TRACE("br\n"); p->job_end(tok); }
		if (job->free_ctx)
			{ MCC_TRACE("br\n"); job->free_ctx(job->ctx); }
		mcc_free(job);
		pthread_mutex_lock(&p->qlock);
		p->nrun++;
		pthread_mutex_unlock(&p->qlock);
	}
	return NULL;
}

static int mcc_pool_start(MccPool *p, unsigned long workers) { MCC_TRACE("enter\n");
	int n;
	pthread_mutex_lock(&p->qlock);
	if (p->quit) { MCC_TRACE("br\n");
		pthread_mutex_unlock(&p->qlock);
		return 0;
	}
	if (!p->started) { MCC_TRACE("br\n");
		int want = (int)workers;
		int i;
		if (want < 1)
			{ MCC_TRACE("br\n"); want = 1; }
		if (want > MCC_POOL_MAX) { MCC_TRACE("br\n");
			fprintf(stderr,
							"mcc: %s worker pool capped at %d (requested %d); raise "
							"%s to ask for more\n",
							p->cap_label ? p->cap_label : (p->name ? p->name : "generic"),
							MCC_POOL_MAX, want,
							p->cap_macro ? p->cap_macro : "MCC_POOL_MAX");
			want = MCC_POOL_MAX;
		}
		p->started = 1;
		p->hooked = 1;
		for (i = 0; i < want; i++) { MCC_TRACE("br\n");
			pthread_t th;
			if (pthread_create(&th, NULL, mcc_pool_worker, p) != 0)
				{ MCC_TRACE("br\n"); break; }
			p->th[p->nth++] = th;
			p->nworkers++;
		}
		if (p->verbose)
			{ MCC_TRACE("br\n"); fprintf(stderr, "%s-pool[start]: requested=%d live=%d\n",
							p->name ? p->name : "mcc", want, p->nworkers); }
	}
	n = p->nworkers;
	pthread_mutex_unlock(&p->qlock);
	return n;
}

static int mcc_pool_ready(MccPool *p) { MCC_TRACE("enter\n");
	return p->nworkers > 0 && !p->quit;
}

static int mcc_pool_enqueue(MccPool *p, MccPoolJob *job) { MCC_TRACE("enter\n");
	pthread_mutex_lock(&p->qlock);
	if (p->quit || p->nworkers <= 0) { MCC_TRACE("br\n");
		p->nrefused++;
		pthread_mutex_unlock(&p->qlock);
		return 0;
	}
	job->next = NULL;
	if (p->tail)
		{ MCC_TRACE("br\n"); p->tail->next = job; }
	else
		{ MCC_TRACE("br\n"); p->head = job; }
	p->tail = job;
	p->nenqueued++;
	pthread_cond_signal(&p->qcond);
	pthread_mutex_unlock(&p->qlock);
	return 1;
}

static int mcc_pool_submit(MccPool *p, int (*tick)(void *), void *ctx,
													 void (*free_ctx)(void *)) { MCC_TRACE("enter\n");
	MccPoolJob *job = mcc_pool_job_new(tick, ctx, free_ctx);
	if (!job) { MCC_TRACE("br\n");
		if (free_ctx && ctx)
			{ MCC_TRACE("br\n"); free_ctx(ctx); }
		return 0;
	}
	if (!mcc_pool_enqueue(p, job)) { MCC_TRACE("br\n");
		if (free_ctx && ctx)
			{ MCC_TRACE("br\n"); free_ctx(ctx); }
		mcc_free(job);
		return 0;
	}
	return 1;
}

static void mcc_pool_shutdown(MccPool *p) { MCC_TRACE("enter\n");
	pthread_t th[MCC_POOL_MAX];
	int n, i;
	pthread_mutex_lock(&p->qlock);
	if (p->quit) { MCC_TRACE("br\n");
		pthread_mutex_unlock(&p->qlock);
		return;
	}
	p->quit = 1;
	p->quit_seen = 1;
	n = p->nth;
	for (i = 0; i < n; i++)
		{ MCC_TRACE("br\n"); th[i] = p->th[i]; }
	pthread_cond_broadcast(&p->qcond);
	pthread_mutex_unlock(&p->qlock);
	for (i = 0; i < n; i++)
		{ MCC_TRACE("br\n"); pthread_join(th[i], NULL); }
	pthread_mutex_lock(&p->qlock);
	while (p->head) { MCC_TRACE("br\n");
		MccPoolJob *drop = p->head;
		p->head = drop->next;
		if (drop->free_ctx)
			{ MCC_TRACE("br\n"); drop->free_ctx(drop->ctx); }
		mcc_free(drop);
		p->ndiscarded++;
	}
	p->tail = NULL;
	p->nworkers = 0;
	p->nth = 0;
	p->started = 0;
	pthread_mutex_unlock(&p->qlock);
	if (p->verbose)
		{ MCC_TRACE("br\n"); fprintf(stderr,
						"%s-pool[shutdown]: joined=%d ran=%lu enqueued=%lu refused=%lu\n",
						p->name ? p->name : "mcc", n, p->nrun, p->nenqueued, p->nrefused); }
}

static void mcc_pool_atfork_prepare(MccPool *p) { MCC_TRACE("enter\n");
	pthread_mutex_lock(&p->qlock);
}

static void mcc_pool_atfork_parent(MccPool *p) { MCC_TRACE("enter\n");
	pthread_mutex_unlock(&p->qlock);
}

static void mcc_pool_atfork_child(MccPool *p) { MCC_TRACE("enter\n");
	p->head = p->tail = NULL;
	p->started = 0;
	p->nworkers = 0;
	p->quit = 0;
	p->nth = 0;
	pthread_cond_init(&p->qcond, NULL);
	pthread_mutex_unlock(&p->qlock);
}

#endif
