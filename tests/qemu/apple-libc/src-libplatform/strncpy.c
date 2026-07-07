#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_STRNCPY

char *
_platform_strncpy(char *restrict dst, const char *restrict src, size_t maxlen) {
	const size_t srclen = _platform_strnlen(src, maxlen);
	if (srclen < maxlen) {
		_platform_memmove(dst, src, srclen);

		_platform_memset(dst + srclen, 0, maxlen - srclen);
	} else {
		_platform_memmove(dst, src, maxlen);
	}

	return dst;
}

#if VARIANT_STATIC
char *
strncpy(char *restrict dst, const char *restrict src, size_t maxlen) {
	return _platform_strncpy(dst, src, maxlen);
}
#endif

#endif
