#include <threads.h>
#include <stdint.h>

extern long write(int, const void *, unsigned long);

#define NT 4

static mtx_t mtx;
static cnd_t go_cnd;
static cnd_t done_cnd;
static int go = 0;
static int done_count = 0;
static tss_t key;
static once_flag once = ONCE_FLAG_INIT;
static int once_count = 0;

static void once_fn(void) {
	once_count++;
}

static int worker(void *arg) {
	intptr_t id = (intptr_t)arg;
	call_once(&once, once_fn);
	tss_set(key, (void *)(id + 1));

	mtx_lock(&mtx);
	while (!go)
		cnd_wait(&go_cnd, &mtx);
	mtx_unlock(&mtx);

	mtx_lock(&mtx);
	done_count++;
	cnd_signal(&done_cnd);
	mtx_unlock(&mtx);

	return (int)(intptr_t)tss_get(key);
}

int main(void) {
	tss_create(&key, NULL);
	mtx_init(&mtx, mtx_plain);
	cnd_init(&go_cnd);
	cnd_init(&done_cnd);

	thrd_t t[NT];
	for (intptr_t i = 0; i < NT; i++)
		thrd_create(&t[i], worker, (void *)i);

	mtx_lock(&mtx);
	go = 1;
	cnd_broadcast(&go_cnd);
	mtx_unlock(&mtx);

	mtx_lock(&mtx);
	while (done_count < NT)
		cnd_wait(&done_cnd, &mtx);
	mtx_unlock(&mtx);

	int sum = 0, res;
	for (int i = 0; i < NT; i++) {
		thrd_join(t[i], &res);
		sum += res;
	}

	mtx_destroy(&mtx);
	cnd_destroy(&go_cnd);
	cnd_destroy(&done_cnd);
	tss_delete(key);

	int ok = once_count == 1 && done_count == NT && sum == 10;
	write(1, ok ? "OK\n" : "FAIL\n", ok ? 3 : 5);
	return ok ? 0 : 1;
}
