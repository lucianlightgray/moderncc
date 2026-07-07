#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_STRNCMP

int _platform_strncmp(const char *s1, const char *s2, size_t n) {
	if (n == 0)
		return (0);
	do {
		if (*s1 != *s2++)
			return (*(const unsigned char *)s1 -
							*(const unsigned char *)(s2 - 1));
		if (*s1++ == '\0')
			break;
	} while (--n != 0);
	return (0);
}

#if VARIANT_STATIC
int strncmp(const char *s1, const char *s2, size_t n) {
	return _platform_strncmp(s1, s2, n);
}
#endif

#endif
