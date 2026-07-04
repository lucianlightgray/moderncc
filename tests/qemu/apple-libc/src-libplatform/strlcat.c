#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_STRLCAT

size_t
_platform_strlcat(char *restrict dst, const char *restrict src, size_t maxlen) {
    const size_t srclen = _platform_strlen(src);
    const size_t dstlen = _platform_strnlen(dst, maxlen);
    if (dstlen == maxlen)
        return maxlen + srclen;
    if (srclen < maxlen - dstlen) {
        _platform_memmove(dst + dstlen, src, srclen + 1);
    } else {
        _platform_memmove(dst + dstlen, src, maxlen - dstlen - 1);
        dst[maxlen - 1] = '\0';
    }
    return dstlen + srclen;
}

#if VARIANT_STATIC
size_t
strlcat(char *restrict dst, const char *restrict src, size_t maxlen) {
    return _platform_strlcat(dst, src, maxlen);
}
#endif

#endif
