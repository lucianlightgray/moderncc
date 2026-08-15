#ifndef _MCC_OSX_TIME_H
#define _MCC_OSX_TIME_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _MCC_TIME_T_DEFINED
#define _MCC_TIME_T_DEFINED
typedef long time_t;
#endif

typedef unsigned long clock_t;

struct timespec {
	time_t tv_sec;
	long tv_nsec;
};

struct tm {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
	long tm_gmtoff;
	char *tm_zone;
};

typedef enum {
	_CLOCK_REALTIME = 0,
	_CLOCK_MONOTONIC = 6,
	_CLOCK_MONOTONIC_RAW = 4,
	_CLOCK_MONOTONIC_RAW_APPROX = 5,
	_CLOCK_UPTIME_RAW = 8,
	_CLOCK_UPTIME_RAW_APPROX = 9,
	_CLOCK_PROCESS_CPUTIME_ID = 12,
	_CLOCK_THREAD_CPUTIME_ID = 16
} clockid_t;

#define CLOCK_REALTIME _CLOCK_REALTIME
#define CLOCK_MONOTONIC _CLOCK_MONOTONIC
#define CLOCK_MONOTONIC_RAW _CLOCK_MONOTONIC_RAW
#define CLOCK_PROCESS_CPUTIME_ID _CLOCK_PROCESS_CPUTIME_ID
#define CLOCK_THREAD_CPUTIME_ID _CLOCK_THREAD_CPUTIME_ID

#define CLOCKS_PER_SEC ((clock_t)1000000)
#define TIME_UTC 1

clock_t clock(void);
time_t time(time_t *);
double difftime(time_t, time_t);
time_t mktime(struct tm *);
time_t timegm(struct tm *);
struct tm *localtime(const time_t *);
struct tm *gmtime(const time_t *);
struct tm *localtime_r(const time_t *, struct tm *);
struct tm *gmtime_r(const time_t *, struct tm *);
char *asctime(const struct tm *);
char *ctime(const time_t *);
size_t strftime(char *, size_t, const char *, const struct tm *);
int nanosleep(const struct timespec *, struct timespec *);
int clock_gettime(clockid_t, struct timespec *);
int clock_getres(clockid_t, struct timespec *);
int clock_settime(clockid_t, const struct timespec *);

#ifdef __cplusplus
}
#endif

#endif
