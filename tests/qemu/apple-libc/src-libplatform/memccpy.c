#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_MEMCCPY

#include <stdlib.h>

void *
_platform_memccpy(void *t, const void *f, int c, size_t n) {
    void *last;

    if (n == 0) {
        return NULL;
    }

    last = _platform_memchr(f, c, n);

    if (last == NULL) {
        _platform_memmove(t, f, n);
        return NULL;
    } else {
        n = (char *)last - (char *)f + 1;
        _platform_memmove(t, f, n);
        return (void *)((char *)t + n);
    }
}

#if VARIANT_STATIC
void *
memccpy(void *t, const void *f, int c, size_t n) {
    return _platform_memccpy(t, f, c, n);
}
#endif

#endif
