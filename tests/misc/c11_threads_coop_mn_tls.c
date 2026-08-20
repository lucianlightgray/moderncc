#include <threads.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#define NT 8

static mtx_t mtx;
static long counter = 0;

static int worker(void *arg) {
	int id = (int)(intptr_t)arg;
	int e0 = id + 1000;
	int tl0 = id + 500;
	int i;
	errno = e0;
	for (i = 0; i < 2000; i++) {
		mtx_lock(&mtx);
		counter++;
		mtx_unlock(&mtx);
		if (errno != e0)
			return 1;
	}
	if (errno != e0)
		return 1;
	(void)tl0;
	return 0;
}

int main(void) {
	thrd_t t[NT];
	int i, res, bad = 0;
	mtx_init(&mtx, mtx_plain);
	for (i = 0; i < NT; i++)
		thrd_create(&t[i], worker, (void *)(intptr_t)i);
	for (i = 0; i < NT; i++) {
		thrd_join(t[i], &res);
		if (res != 0)
			bad = 1;
	}
	mtx_destroy(&mtx);
	printf(bad ? "TLS-CORRUPT\n" : "OK\n");
	return bad;
}
