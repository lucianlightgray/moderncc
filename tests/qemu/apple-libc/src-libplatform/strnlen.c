#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/string/strnlen.c,v 1.1 2009/02/28 06:00:58 das Exp $");

#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_STRNLEN

size_t
_platform_strnlen(const char *s, size_t maxlen) {
    size_t len;

    for (len = 0; len < maxlen; len++, s++) {
        if (!*s)
            break;
    }
    return (len);
}

#if VARIANT_STATIC
size_t
strnlen(const char *s, size_t maxlen) {
    return _platform_strnlen(s, maxlen);
}
#endif

#endif
