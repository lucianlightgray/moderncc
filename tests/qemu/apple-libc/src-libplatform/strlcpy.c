#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_STRLCPY

size_t
_platform_strlcpy(char *restrict dst, const char *restrict src, size_t maxlen) {
    const size_t srclen = _platform_strlen(src);
    if (srclen < maxlen) {
        _platform_memmove(dst, src, srclen + 1);
    } else if (maxlen != 0) {
        _platform_memmove(dst, src, maxlen - 1);
        dst[maxlen - 1] = '\0';
    }
    return srclen;
}

#if VARIANT_STATIC
size_t
strlcpy(char *restrict dst, const char *restrict src, size_t maxlen) {
    return _platform_strlcpy(dst, src, maxlen);
}
#endif

#endif
