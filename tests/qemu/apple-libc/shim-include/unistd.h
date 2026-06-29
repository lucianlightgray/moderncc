#ifndef _COMPAT_UNISTD_H
#define _COMPAT_UNISTD_H
#include <sys/types.h>
typedef long ssize_t;
ssize_t write(int, const void *, size_t);
#endif
