#ifndef MCC_TEST_VLOG_H
#define MCC_TEST_VLOG_H

#ifdef VLOG_ENABLE
#include <stdio.h>

#if defined(__MCC__)
#define VLOG_CC "mcc"
#elif defined(__clang__)
#define VLOG_CC "clang"
#elif defined(__GNUC__)
#define VLOG_CC "gcc"
#else
#define VLOG_CC "cc"
#endif

#define VLOG(...)                                                         \
	do {                                                                  \
		fprintf(stderr, "[vlog %s %s:%d] ", VLOG_CC, __FILE__, __LINE__); \
		fprintf(stderr, __VA_ARGS__);                                     \
		fputc('\n', stderr);                                              \
	} while (0)

#define VLOG_IF(cond, ...)     \
	do {                       \
		if (cond)              \
			VLOG(__VA_ARGS__); \
	} while (0)

#define VLOG_ENTER(name) VLOG("enter %s", (name))

#define VLOG_VALUE(fmt, expr) VLOG(#expr " = " fmt, (expr))

#else

#define VLOG(...) ((void)0)
#define VLOG_IF(cond, ...) ((void)0)
#define VLOG_ENTER(name) ((void)0)
#define VLOG_VALUE(fmt, expr) ((void)0)

#endif

#endif
