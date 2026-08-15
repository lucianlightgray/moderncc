#ifndef _MCC_OSX_FCNTL_H
#define _MCC_OSX_FCNTL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _MCC_MODE_T_DEFINED
#define _MCC_MODE_T_DEFINED
typedef unsigned short mode_t;
#endif

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR 0x0002
#define O_ACCMODE 0x0003
#define O_NONBLOCK 0x0004
#define O_APPEND 0x0008
#define O_CREAT 0x0200
#define O_TRUNC 0x0400
#define O_EXCL 0x0800
#define O_CLOEXEC 0x01000000

int open(const char *, int, ...);
int creat(const char *, mode_t);
int fcntl(int, int, ...);

#ifdef __cplusplus
}
#endif

#endif
