#ifndef _MCC_OSX_SYS_TYPES_H
#define _MCC_OSX_SYS_TYPES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _MCC_PID_T_DEFINED
#define _MCC_PID_T_DEFINED
typedef int pid_t;
#endif

#ifndef _MCC_MODE_T_DEFINED
#define _MCC_MODE_T_DEFINED
typedef unsigned short mode_t;
#endif

typedef long ssize_t;
typedef long long off_t;

#ifndef _MCC_TIME_T_DEFINED
#define _MCC_TIME_T_DEFINED
typedef long time_t;
#endif
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef int dev_t;
typedef unsigned long long ino_t;
typedef unsigned int useconds_t;

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;

#ifdef __cplusplus
}
#endif

#endif
