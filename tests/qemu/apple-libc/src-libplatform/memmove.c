#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_MEMMOVE

#include <sys/types.h>

typedef long word;

#define wsize sizeof(word)
#define wmask (wsize - 1)

void *
_platform_memmove(void *dst0, const void *src0, size_t length) {
    char *dst = dst0;
    const char *src = src0;
    size_t t;

    if (length == 0 || dst == src)
        goto done;

#define TLOOP(s) \
    if (t)       \
    TLOOP1(s)
#define TLOOP1(s) \
    do {          \
        s;        \
    } while (--t)

    if ((unsigned long)dst < (unsigned long)src) {

        t = (uintptr_t)src;
        if ((t | (uintptr_t)dst) & wmask) {

            if ((t ^ (uintptr_t)dst) & wmask || length < wsize)
                t = length;
            else
                t = wsize - (t & wmask);
            length -= t;
            TLOOP1(*dst++ = *src++);
        }

        t = length / wsize;
        TLOOP(*(word *)dst = *(word *)src; src += wsize; dst += wsize);
        t = length & wmask;
        TLOOP(*dst++ = *src++);
    } else {

        src += length;
        dst += length;
        t = (uintptr_t)src;
        if ((t | (uintptr_t)dst) & wmask) {
            if ((t ^ (uintptr_t)dst) & wmask || length <= wsize)
                t = length;
            else
                t &= wmask;
            length -= t;
            TLOOP1(*--dst = *--src);
        }
        t = length / wsize;
        TLOOP(src -= wsize; dst -= wsize; *(word *)dst = *(word *)src);
        t = length & wmask;
        TLOOP(*--dst = *--src);
    }
done:
    return (dst0);
}

#if VARIANT_STATIC
void *
memmove(void *dst0, const void *src0, size_t length) {
    return _platform_memmove(dst0, src0, length);
}

void *
memcpy(void *dst0, const void *src0, size_t length) {
    return _platform_memmove(dst0, src0, length);
}
#endif

#endif
