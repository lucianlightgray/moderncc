/*
 * T-lin-10421 regression: under -DMCC_THREADS_COOP a program must be able to
 * include <stdlib.h> alongside <threads.h>. Before the fix, glibc's <stdlib.h>
 * (ISOC23) re-typedef'd `once_flag` and collided with the coop backend's own
 * once_flag -> "incompatible redefinition of 'once_flag'", so this file did not
 * compile at all. It exercises malloc + call_once + thrd_create/join together.
 * Supported include order: <threads.h> before <stdlib.h>.
 */
#include <threads.h>
#include <stdlib.h>
#include <stdio.h>

static once_flag once = ONCE_FLAG_INIT;
static int once_count = 0;

static void once_fn(void) { once_count++; }

static int worker(void *arg) {
	call_once(&once, once_fn);
	int *p = arg;
	*p += 41;
	return 7;
}

int main(void) {
	call_once(&once, once_fn);

	int *x = malloc(sizeof *x);
	*x = 1;

	thrd_t t;
	int r = 0;
	if (thrd_create(&t, worker, x) != thrd_success)
		return 2;
	if (thrd_join(t, &r) != thrd_success)
		return 3;

	int ok = (*x == 42) && (r == 7) && (once_count == 1);
	free(x);
	printf(ok ? "OK\n" : "FAIL\n");
	return ok ? 0 : 1;
}
