/* libdispatch (GCD): queues, semaphores, groups, barriers and dispatch_apply.
   Uses only the _f function-pointer entry points -- Apple blocks are a clang
   language extension mcc does not implement, and GCD ships both forms. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dispatch/dispatch.h>

static int fails;

#define CHECK(cond) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		fails++; \
	} \
} while (0)

#define NITEMS 4096

static long serial_total;
static int hits[NITEMS];
static dispatch_semaphore_t sem;

static void bump_serial(void *ctx) { serial_total += (long)ctx; }

static void mark(void *ctx, size_t i) { (void)ctx; hits[i]++; }

static void signal_sem(void *ctx) { dispatch_semaphore_signal((dispatch_semaphore_t)ctx); }

static void group_work(void *ctx) { __sync_fetch_and_add((int *)ctx, 1); }

int main(void) {
	/* A serial queue must apply every async in submission order, so the sum is
	   exact rather than merely "some of them ran". */
	dispatch_queue_t q = dispatch_queue_create("mcc.darwin.serial", NULL);
	CHECK(q != NULL);
	long expect = 0;
	for (long i = 1; i <= 1000; i++) {
		dispatch_async_f(q, (void *)i, bump_serial);
		expect += i;
	}
	dispatch_sync_f(q, NULL, bump_serial);
	CHECK(serial_total == expect);

	sem = dispatch_semaphore_create(0);
	CHECK(sem != NULL);
	dispatch_async_f(q, sem, signal_sem);
	CHECK(dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW,
													 5LL * NSEC_PER_SEC)) == 0);

	/* dispatch_apply_f is the concurrent path: every index exactly once. */
	dispatch_queue_t gq = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
	CHECK(gq != NULL);
	dispatch_apply_f(NITEMS, gq, NULL, mark);
	int wrong = 0;
	for (int i = 0; i < NITEMS; i++)
		wrong += (hits[i] != 1);
	CHECK(wrong == 0);

	int group_count = 0;
	dispatch_group_t grp = dispatch_group_create();
	CHECK(grp != NULL);
	for (int i = 0; i < 256; i++)
		dispatch_group_async_f(grp, gq, &group_count, group_work);
	CHECK(dispatch_group_wait(grp, dispatch_time(DISPATCH_TIME_NOW,
												 10LL * NSEC_PER_SEC)) == 0);
	CHECK(group_count == 256);

	/* A concurrent queue's barrier has to drain everything before it. */
	dispatch_queue_t cq = dispatch_queue_create(
			"mcc.darwin.concurrent", DISPATCH_QUEUE_CONCURRENT);
	CHECK(cq != NULL);
	int bar_count = 0;
	for (int i = 0; i < 512; i++)
		dispatch_async_f(cq, &bar_count, group_work);
	dispatch_barrier_sync_f(cq, NULL, bump_serial);
	CHECK(bar_count == 512);

	/* No dispatch_release here: its argument is dispatch_object_t, a
	   transparent_union, which mcc does not implement. Tracked in docs/TODO.
	   The objects are reclaimed at process exit. */

	if (fails) {
		fprintf(stderr, "libsystem_gcd: %d failure(s)\n", fails);
		return 1;
	}
	printf("libsystem_gcd: OK\n");
	return 0;
}
