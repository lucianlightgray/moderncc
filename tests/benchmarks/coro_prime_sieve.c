/*
 * coro-prime-sieve, C11 <threads.h> channel-pipeline threading.
 *
 * Ported from the concurrent prime sieve used by the reference implementations
 * (vendor/plb/bench/algorithm/coro-prime-sieve/1.go, 1.rs). Those spawn one
 * coroutine/goroutine per stage connected by channels: a generator feeds the
 * sequence 2,3,4,... into the pipeline, and each newly discovered prime adds a
 * Filter stage that drops multiples of that prime and forwards the rest. The
 * Nth value to emerge from the daisy chain is the Nth prime.
 *
 * This port keeps that shape exactly but maps it onto C11 threads:
 *   - one thrd_t per pipeline stage (1 generator + one filter per prime found),
 *   - each channel is a single-slot bounded queue built from mtx_t + cnd_t
 *     (a "not empty" and a "not full" condition) plus a `closed` flag,
 *   - clean shutdown: once the Nth prime is read, main marks every channel in
 *     the chain closed. A closed channel makes both send and recv return a
 *     failure code, so every stage -- whether it was parked in a sender's
 *     cnd_wait (slot full) or a receiver's cnd_wait (slot empty) -- wakes, sees
 *     the close, and returns; each stage is then joined. The program always
 *     exits. (Closing only the head would leave a filter parked on a full
 *     downstream slot stuck, since forward close wakes receivers, not senders.)
 *
 * The mtx/cnd handoff is the point: under mcc's cooperative backend
 * (-DMCC_THREADS_COOP) every stage spends nearly all its life blocked in
 * cnd_wait, resumed one value at a time through the fiber scheduler -- the same
 * pattern the barrier in spectral_norm_barrier.c already validates under coop.
 *
 * <threads.h> must lead and libc is hand-declared rather than #included: mcc's
 * cooperative backend defines its own once_flag (mcc_coop_threads.h) which
 * collides with the once_flag glibc pulls in via <stdlib.h>. Declaring the few
 * functions we use keeps ONE source building under native, coop, gcc and clang.
 *
 * NT is accepted but ignored: this pipeline spawns one thread per prime, not NT
 * of them, and the Nth prime is a pure function of N, so the output is identical
 * for any NT (the runner's -DNT=1 reference matches).
 */
#include <threads.h>

extern void *malloc(__SIZE_TYPE__);
extern void free(void *);
extern int atoi(const char *);
extern int printf(const char *, ...);
extern _Noreturn void exit(int);

#ifndef NT
#define NT 4
#endif

typedef struct {
	mtx_t m;
	cnd_t not_empty;
	cnd_t not_full;
	int value;
	int has;
	int closed;
} chan_t;

static chan_t *chan_new(void) {
	chan_t *c = malloc(sizeof(chan_t));
	if (!c)
		exit(2);
	mtx_init(&c->m, mtx_plain);
	cnd_init(&c->not_empty);
	cnd_init(&c->not_full);
	c->value = 0;
	c->has = 0;
	c->closed = 0;
	return c;
}

static void chan_free(chan_t *c) {
	mtx_destroy(&c->m);
	cnd_destroy(&c->not_empty);
	cnd_destroy(&c->not_full);
	free(c);
}

static int chan_send(chan_t *c, int v) {
	mtx_lock(&c->m);
	while (c->has && !c->closed)
		cnd_wait(&c->not_full, &c->m);
	if (c->closed) {
		mtx_unlock(&c->m);
		return 0;
	}
	c->value = v;
	c->has = 1;
	cnd_signal(&c->not_empty);
	mtx_unlock(&c->m);
	return 1;
}

static int chan_recv(chan_t *c, int *out) {
	mtx_lock(&c->m);
	while (!c->has && !c->closed)
		cnd_wait(&c->not_empty, &c->m);
	if (!c->has) {
		mtx_unlock(&c->m);
		return 0;
	}
	*out = c->value;
	c->has = 0;
	cnd_signal(&c->not_full);
	mtx_unlock(&c->m);
	return 1;
}

static void chan_close(chan_t *c) {
	mtx_lock(&c->m);
	c->closed = 1;
	cnd_broadcast(&c->not_empty);
	cnd_broadcast(&c->not_full);
	mtx_unlock(&c->m);
}

static int gen_thread(void *arg) {
	chan_t *out = arg;
	int i = 2;
	while (chan_send(out, i))
		i++;
	chan_close(out);
	return 0;
}

typedef struct {
	chan_t *in;
	chan_t *out;
	int prime;
} filter_arg;

static int filter_thread(void *arg) {
	filter_arg *f = arg;
	int v;
	while (chan_recv(f->in, &v)) {
		if (v % f->prime != 0) {
			if (!chan_send(f->out, v))
				break;
		}
	}
	chan_close(f->out);
	return 0;
}

static int nth_prime(int n) {
	chan_t **chans = malloc((__SIZE_TYPE__)n * sizeof(chan_t *));
	thrd_t *threads = malloc((__SIZE_TYPE__)n * sizeof(thrd_t));
	filter_arg *args = malloc((__SIZE_TYPE__)n * sizeof(filter_arg));
	if (!chans || !threads || !args)
		exit(2);

	int nchan = 0;
	int nthr = 0;

	chan_t *head = chan_new();
	chans[nchan++] = head;
	if (thrd_create(&threads[nthr], gen_thread, head) != thrd_success)
		exit(2);
	nthr++;

	chan_t *cur = head;
	int last = 0;
	for (int i = 0; i < n; i++) {
		int prime;
		if (!chan_recv(cur, &prime))
			exit(2);
		last = prime;
		if (i == n - 1)
			break;
		chan_t *nxt = chan_new();
		chans[nchan++] = nxt;
		args[nthr - 1].in = cur;
		args[nthr - 1].out = nxt;
		args[nthr - 1].prime = prime;
		if (thrd_create(&threads[nthr], filter_thread, &args[nthr - 1]) !=
			thrd_success)
			exit(2);
		nthr++;
		cur = nxt;
	}

	for (int i = 0; i < nchan; i++)
		chan_close(chans[i]);
	for (int i = 0; i < nthr; i++)
		thrd_join(threads[i], (void *)0);
	for (int i = 0; i < nchan; i++)
		chan_free(chans[i]);

	free(chans);
	free(threads);
	free(args);
	return last;
}

int main(int argc, char *argv[]) {
	int n = argc >= 2 ? atoi(argv[1]) : 2000;
	if (n <= 0)
		n = 2000;
	printf("prime(%d) = %d\n", n, nth_prime(n));
	return 0;
}
