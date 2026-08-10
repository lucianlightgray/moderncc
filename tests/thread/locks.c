#include <pthread.h>

pthread_mutex_t ma = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mb = PTHREAD_MUTEX_INITIALIZER;

int ga, gb, gc, gd;
int arr[64];
int *gp;

extern int sink(int);

void sec_a_ga(void) {
	pthread_mutex_lock(&ma);
	ga = ga + 1;
	pthread_mutex_unlock(&ma);
}

void sec_a_gb(void) {
	pthread_mutex_lock(&ma);
#ifdef THREAD_CENSUS_PERTURB
	ga = ga + 2;
#else
	gb = gb + 2;
#endif
	pthread_mutex_unlock(&ma);
}

void sec_a_ga_again(void) {
	pthread_mutex_lock(&ma);
	ga = ga + 3;
	pthread_mutex_unlock(&ma);
}

void sec_a_indirect(void) {
	pthread_mutex_lock(&ma);
	*gp = *gp + 1;
	pthread_mutex_unlock(&ma);
}

void sec_a_opaque(void) {
	pthread_mutex_lock(&ma);
	gc = sink(gc);
	pthread_mutex_unlock(&ma);
}

void sec_b_gc(void) {
	pthread_mutex_lock(&mb);
	gc = gc + 1;
	pthread_mutex_unlock(&mb);
}

void sec_b_gd(void) {
	pthread_mutex_lock(&mb);
	gd = gd + 1;
	pthread_mutex_unlock(&mb);
}

void sec_b_arr(void) {
	pthread_mutex_lock(&mb);
	gd = gd + arr[3];
	pthread_mutex_unlock(&mb);
}
