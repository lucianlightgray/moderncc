#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_STRCPY

char *
_platform_strcpy(char *restrict dst, const char *restrict src) {
    const size_t length = _platform_strlen(src);

    _platform_memmove(dst, src, length + 1);

    return dst;
}

#if VARIANT_STATIC
char *
strcpy(char *restrict dst, const char *restrict src) {
    return _platform_strcpy(dst, src);
}
#endif

#endif
