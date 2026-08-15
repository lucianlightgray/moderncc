#ifndef _MCC_OSX_SEMAPHORE_H
#define _MCC_OSX_SEMAPHORE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int sem_t;

#define SEM_FAILED ((sem_t *)-1)

int sem_init(sem_t *, int, unsigned int);
int sem_destroy(sem_t *);
int sem_wait(sem_t *);
int sem_trywait(sem_t *);
int sem_post(sem_t *);
int sem_getvalue(sem_t *, int *);
sem_t *sem_open(const char *, int, ...);
int sem_close(sem_t *);
int sem_unlink(const char *);

#ifdef __cplusplus
}
#endif

#endif
