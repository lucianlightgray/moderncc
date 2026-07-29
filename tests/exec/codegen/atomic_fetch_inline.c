#include <threads.h>
extern int printf(const char *, ...);
static int counter;
static long long lcounter;
static int spin;
static int cas_lock;
static long long cas_total;
static int worker(void *arg) {
	int i;
	(void)arg;
	for (i = 0; i < 50000; i++) {
		__atomic_fetch_add(&counter, 1, 5);
		__atomic_fetch_add(&lcounter, 2, 5);
		__atomic_fetch_sub(&counter, 1, 5);
		if (__atomic_load_n(&counter, 5) < 0)
			return 1;
	}
	__atomic_store_n(&spin, 1, 5);
	if (__atomic_exchange_n(&spin, 2, 5) < 1)
		return 2;
	for (i = 0; i < 20000; i++) {
		int expected = 0;
		while (!__atomic_compare_exchange_n(&cas_lock, &expected, 1, 0, 5, 5))
			expected = 0;
		cas_total += 3;
		__atomic_store_n(&cas_lock, 0, 5);
	}
	__atomic_fetch_add(&counter, 7, 5);
	return 0;
}
static int flag;
static long long lflag;

static void cas_checks(void)
{
	int v = 10, e = 10;

	if (!__atomic_compare_exchange_n(&v, &e, 20, 0, 5, 5) || v != 20 || e != 10)
		printf("BAD cas_ok\n");
	e = 99;
	if (__atomic_compare_exchange_n(&v, &e, 30, 0, 5, 5) || v != 20 || e != 20)
		printf("BAD cas_fail\n");
}

static void seq_checks(void)
{
	int v;
	long long lv;

	__atomic_store_n(&flag, 5, 5);
	if (__atomic_load_n(&flag, 5) != 5)
		printf("BAD store32\n");
	v = __atomic_exchange_n(&flag, 9, 5);
	if (v != 5 || __atomic_load_n(&flag, 5) != 9)
		printf("BAD xchg32\n");
	__atomic_store_n(&lflag, 50, 5);
	if (__atomic_load_n(&lflag, 5) != 50)
		printf("BAD store64\n");
	lv = __atomic_exchange_n(&lflag, 90, 5);
	if (lv != 50 || __atomic_load_n(&lflag, 5) != 90)
		printf("BAD xchg64\n");
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
	seq_checks();
	cas_checks();
	for (i = 0; i < 4; i++)
		thrd_create(&t[i], worker, 0);
	for (i = 0; i < 4; i++)
		thrd_join(t[i], 0);
	printf("counter=%d lcounter=%lld cas=%lld lock=%d\n", counter, lcounter,
				 cas_total, cas_lock);
	return 0;
}
