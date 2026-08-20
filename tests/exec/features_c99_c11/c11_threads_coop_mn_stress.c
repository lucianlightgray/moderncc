#include <threads.h>
#include <stdio.h>

#define NT 2

static volatile int arrived = 0;
static volatile int leave = 0;

static int spin_worker(void *arg) {
	(void)arg;
	__atomic_add_fetch(&arrived, 1, __ATOMIC_SEQ_CST);
	while (__atomic_load_n(&arrived, __ATOMIC_SEQ_CST) < NT)
		;
	__atomic_add_fetch(&leave, 1, __ATOMIC_SEQ_CST);
	return 7;
}

int main(void) {
	thrd_t t[NT];
	int i, res, sum = 0;
	for (i = 0; i < NT; i++)
		thrd_create(&t[i], spin_worker, (void *)0);
	for (i = 0; i < NT; i++) {
		thrd_join(t[i], &res);
		sum += res;
	}
	if (sum == NT * 7 && leave == NT)
		printf("OK\n");
	else
		printf("BAD sum=%d leave=%d\n", sum, leave);
	return 0;
}
