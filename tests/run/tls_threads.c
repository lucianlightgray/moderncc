#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

__thread int32_t t_init = 42;
__thread int32_t t_zero;

extern __thread int32_t aux_t_init;
extern int32_t aux_t_get(void);
extern void aux_t_set(int32_t v);

static void *worker(void *arg) {
	(void)arg;
	printf("child_init=%d\n", t_init);
	printf("child_zero=%d\n", t_zero);
	printf("child_aux=%d\n", aux_t_get());
	t_init = 100;
	t_zero = 200;
	aux_t_set(300);
	printf("child_w1=%d\n", t_init);
	printf("child_w2=%d\n", t_zero);
	printf("child_w3=%d\n", aux_t_get());
	return (void *)0;
}

int main(void) {
	pthread_t th;
	void *ret = (void *)1;
	printf("main_init=%d\n", t_init);
	printf("main_zero=%d\n", t_zero);
	printf("main_aux=%d\n", aux_t_init);
	t_init = 7;
	aux_t_set(8);
	if (pthread_create(&th, 0, worker, 0) != 0) {
		printf("pthread_create failed\n");
		return 1;
	}
	pthread_join(th, &ret);
	printf("joined=%d\n", ret == 0);
	printf("after_init=%d\n", t_init);
	printf("after_zero=%d\n", t_zero);
	printf("after_aux=%d\n", aux_t_get());
	return 0;
}
