#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_MEMCMP

int _platform_memcmp(const void *s1, const void *s2, size_t n) {
	if (n != 0) {
		const unsigned char *p1 = s1, *p2 = s2;

		do {
			if (*p1++ != *p2++)
				return (*--p1 - *--p2);
		} while (--n != 0);
	}
	return (0);
}

#if VARIANT_STATIC
int memcmp(const void *s1, const void *s2, size_t n) {
	return _platform_memcmp(s1, s2, n);
}

int bcmp(const void *s1, const void *s2, size_t n) {
	return _platform_memcmp(s1, s2, n);
}
#endif

#endif
