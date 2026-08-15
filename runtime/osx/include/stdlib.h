#ifndef _MCC_OSX_STDLIB_H
#define _MCC_OSX_STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 0x7fffffff

typedef struct {
	int quot;
	int rem;
} div_t;

typedef struct {
	long quot;
	long rem;
} ldiv_t;

typedef struct {
	long long quot;
	long long rem;
} lldiv_t;

void *malloc(size_t);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void free(void *);

void abort(void);
void exit(int);
void _Exit(int);
int atexit(void (*)(void));

int atoi(const char *);
long atol(const char *);
long long atoll(const char *);
double atof(const char *);
long strtol(const char *, char **, int);
unsigned long strtoul(const char *, char **, int);
long long strtoll(const char *, char **, int);
unsigned long long strtoull(const char *, char **, int);
double strtod(const char *, char **);
float strtof(const char *, char **);

int rand(void);
void srand(unsigned);

void qsort(void *, size_t, size_t, int (*)(const void *, const void *));
void *bsearch(const void *, const void *, size_t, size_t,
							int (*)(const void *, const void *));

int abs(int);
long labs(long);
long long llabs(long long);
div_t div(int, int);
ldiv_t ldiv(long, long);
lldiv_t lldiv(long long, long long);

char *getenv(const char *);
int system(const char *);

#ifdef __cplusplus
}
#endif

#endif
