#ifndef _MCC_OSX_SYS_MMAN_H
#define _MCC_OSX_SYS_MMAN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef long long off_t;

#define PROT_NONE 0x00
#define PROT_READ 0x01
#define PROT_WRITE 0x02
#define PROT_EXEC 0x04

#define MAP_SHARED 0x0001
#define MAP_PRIVATE 0x0002
#define MAP_FIXED 0x0010
#define MAP_NORESERVE 0x0040
#define MAP_ANON 0x1000
#define MAP_ANONYMOUS MAP_ANON

#define MAP_FAILED ((void *)-1)

void *mmap(void *, size_t, int, int, int, off_t);
int munmap(void *, size_t);
int mprotect(void *, size_t, int);
int madvise(void *, size_t, int);
int msync(void *, size_t, int);
int mlock(const void *, size_t);
int munlock(const void *, size_t);

#ifdef __cplusplus
}
#endif

#endif
