#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_STRCMP

int _platform_strcmp(const char *s1, const char *s2) {
	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

#if VARIANT_STATIC
int strcmp(const char *s1, const char *s2) {
	return _platform_strcmp(s1, s2);
}
#endif

#endif
