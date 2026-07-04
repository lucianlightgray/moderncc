#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_MEMCHR

#include <stdlib.h>

void *
_platform_memchr(const void *s, int c, size_t n) {
    if (n != 0) {
        const unsigned char *p = s;

        do {
            if (*p++ == (unsigned char)c)
                return ((void *)(p - 1));
        } while (--n != 0);
    }
    return (NULL);
}

#if VARIANT_STATIC
void *
memchr(const void *s, int c, size_t n) {
    return _platform_memchr(s, c, n);
}
#endif

#endif
