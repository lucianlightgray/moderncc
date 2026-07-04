#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_MEMSET_PATTERN4

void _platform_memset_pattern4(void *b, const void *pattern4, size_t len) {
    char *start = (char *)b;
    char *p = (char *)b;
    while ((start + len) - p >= 4) {
        _platform_memmove(p, pattern4, 4);
        p += 4;
    }
    if ((start + len) - p != 0) {
        _platform_memmove(p, pattern4, (start + len) - p);
    }
}

#if VARIANT_STATIC
void memset_pattern4(void *b, const void *pattern4, size_t len) {
    return _platform_memset_pattern4(b, pattern4, len);
}
#endif

#endif

#if !_PLATFORM_OPTIMIZED_MEMSET_PATTERN8

void _platform_memset_pattern8(void *b, const void *pattern8, size_t len) {
    char *start = (char *)b;
    char *p = (char *)b;
    while ((start + len) - p >= 8) {
        _platform_memmove(p, pattern8, 8);
        p += 8;
    }
    if ((start + len) - p != 0) {
        _platform_memmove(p, pattern8, (start + len) - p);
    }
}

#if VARIANT_STATIC
void memset_pattern8(void *b, const void *pattern8, size_t len) {
    return _platform_memset_pattern8(b, pattern8, len);
}
#endif

#endif

#if !_PLATFORM_OPTIMIZED_MEMSET_PATTERN16

void _platform_memset_pattern16(void *b, const void *pattern16, size_t len) {
    char *start = (char *)b;
    char *p = (char *)b;
    while ((start + len) - p >= 16) {
        _platform_memmove(p, pattern16, 16);
        p += 16;
    }
    if ((start + len) - p != 0) {
        _platform_memmove(p, pattern16, (start + len) - p);
    }
}

#if VARIANT_STATIC
void memset_pattern16(void *b, const void *pattern16, size_t len) {
    return _platform_memset_pattern16(b, pattern16, len);
}
#endif

#endif
