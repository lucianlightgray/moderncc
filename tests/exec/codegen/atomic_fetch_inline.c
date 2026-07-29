#include <threads.h>
extern int printf(const char *, ...);
static int counter;
static long long lcounter;
static int worker(void *arg) {
	int i;
	(void)arg;
	for (i = 0; i < 50000; i++) {
		__atomic_fetch_add(&counter, 1, 5);
		__atomic_fetch_add(&lcounter, 2, 5);
		__atomic_fetch_sub(&counter, 1, 5);
	}
	__atomic_fetch_add(&counter, 7, 5);
	return 0;
}
static void single_thread_checks(void)
{
	int i = 10;
	long long l = 100;

	if (__atomic_fetch_add(&i, 5, 5) != 10 || i != 15)
		printf("BAD add32\n");
	if (__atomic_fetch_sub(&i, 3, 5) != 15 || i != 12)
		printf("BAD sub32\n");
	if (__atomic_fetch_add(&l, 7, 5) != 100 || l != 107)
		printf("BAD add64\n");
	if (__atomic_fetch_sub(&l, 4, 5) != 107 || l != 103)
		printf("BAD sub64\n");
}

int main(void)
{
	thrd_t t[4];
	int i;

	single_thread_checks();
	for (i = 0; i < 4; i++)
		thrd_create(&t[i], worker, 0);
	for (i = 0; i < 4; i++)
		thrd_join(t[i], 0);
	printf("counter=%d lcounter=%lld\n", counter, lcounter);
	return 0;
}
