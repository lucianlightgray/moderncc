/* C11 <threads.h> (7.26): thrd_, mtx_, cnd_, tss_, call_once and once_flag are
   supported via the hosted libc (glibc/musl). Exercises the whole API with a
   deterministic result; a non-atomic or broken mutex would corrupt the sum. */
#include <threads.h>
#include <stdint.h>
#include <stdio.h>

#define NT 4
#define STEPS 50000

static once_flag once = ONCE_FLAG_INIT;
static int once_count = 0;
static void once_fn(void) { once_count++; }

static tss_t key;
static mtx_t mtx;
static cnd_t cnd;
static long shared = 0;
static int started = 0;

static int worker(void *arg)
{
    intptr_t id = (intptr_t)arg;
    call_once(&once, once_fn);          /* runs the init exactly once */
    tss_set(key, (void *)(id + 1));

    /* signal that this thread has started */
    mtx_lock(&mtx);
    started++;
    cnd_broadcast(&cnd);
    mtx_unlock(&mtx);

    for (int i = 0; i < STEPS; i++) {
        mtx_lock(&mtx);
        shared += 2;                    /* mutex-protected accumulate */
        mtx_unlock(&mtx);
    }
    return (int)(intptr_t)tss_get(key); /* == id + 1 */
}

int main(void)
{
    tss_create(&key, NULL);
    mtx_init(&mtx, mtx_plain);
    cnd_init(&cnd);

    thrd_t t[NT];
    for (intptr_t i = 0; i < NT; i++)
        thrd_create(&t[i], worker, (void *)i);

    /* wait until all workers have started, via the condition variable */
    mtx_lock(&mtx);
    while (started < NT)
        cnd_wait(&cnd, &mtx);
    mtx_unlock(&mtx);

    int sum_res = 0, res;
    for (int i = 0; i < NT; i++) {
        thrd_join(t[i], &res);
        sum_res += res;                 /* sum of (id+1) = 1+2+3+4 = 10 */
    }

    struct timespec ts = { 0, 1000000 };
    thrd_sleep(&ts, NULL);
    thrd_t self = thrd_current();

    mtx_destroy(&mtx);
    cnd_destroy(&cnd);
    tss_delete(key);

    int ok = once_count == 1
          && shared == (long)NT * STEPS * 2
          && sum_res == 10
          && thrd_equal(self, self);
    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
