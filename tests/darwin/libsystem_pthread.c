/* Apple's libpthread: thread creation, the __ulock-backed mutex/cond/rwlock,
   pthread_once, and Darwin TSD (%gs / TPIDRRO_EL0) through pthread_key_t.
   Kernel-fused by definition (tests/qemu/apple-libc). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static int fails;

#define CHECK(cond) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		fails++; \
	} \
} while (0)

#define NTHREADS 8
#define NBUMPS 20000

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static pthread_key_t key;
static pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;

static long counter;
static int once_ran;
static int released;
static int key_made;

static void once_fn(void) { once_ran++; }

static void key_dtor(void *p) { free(p); }

static void *worker(void *arg) {
	long id = (long)arg;

	pthread_once(&once, once_fn);

	long *slot = malloc(sizeof *slot);
	*slot = id;
	CHECK(pthread_setspecific(key, slot) == 0);

	pthread_mutex_lock(&lock);
	while (!released)
		pthread_cond_wait(&cv, &lock);
	pthread_mutex_unlock(&lock);

	for (int i = 0; i < NBUMPS; i++) {
		pthread_mutex_lock(&lock);
		counter++;
		pthread_mutex_unlock(&lock);
	}

	for (int i = 0; i < 100; i++) {
		pthread_rwlock_rdlock(&rw);
		volatile long seen = counter;
		(void)seen;
		pthread_rwlock_unlock(&rw);
	}

	/* The TSD slot must still be this thread's own after all that contention. */
	long *back = pthread_getspecific(key);
	CHECK(back != NULL && *back == id);
	return (void *)(id * 2);
}

int main(void) {
	CHECK(pthread_key_create(&key, key_dtor) == 0);
	key_made = 1;

	pthread_t t[NTHREADS];
	for (long i = 0; i < NTHREADS; i++)
		CHECK(pthread_create(&t[i], NULL, worker, (void *)i) == 0);

	pthread_mutex_lock(&lock);
	released = 1;
	pthread_cond_broadcast(&cv);
	pthread_mutex_unlock(&lock);

	for (long i = 0; i < NTHREADS; i++) {
		void *r = NULL;
		CHECK(pthread_join(t[i], &r) == 0);
		CHECK((long)r == i * 2);
	}

	CHECK(counter == (long)NTHREADS * NBUMPS);
	CHECK(once_ran == 1);

	CHECK(pthread_self() != NULL);
	CHECK(pthread_equal(pthread_self(), pthread_self()));

	char nm[64] = {0};
	CHECK(pthread_setname_np("mcc-darwin-test") == 0);
	CHECK(pthread_getname_np(pthread_self(), nm, sizeof nm) == 0);
	CHECK(!strcmp(nm, "mcc-darwin-test"));

	CHECK(pthread_get_stacksize_np(pthread_self()) > 0);
	CHECK(pthread_get_stackaddr_np(pthread_self()) != NULL);

	if (key_made)
		pthread_key_delete(key);

	if (fails) {
		fprintf(stderr, "libsystem_pthread: %d failure(s)\n", fails);
		return 1;
	}
	printf("libsystem_pthread: OK\n");
	return 0;
}
