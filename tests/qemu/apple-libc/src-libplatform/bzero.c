






















#include <platform/string.h>

#if !VARIANT_STATIC

__attribute__((visibility("hidden")))
void *
memset(void *b, int c, size_t len)
{
	return _platform_memset(b, c, len);
}
#endif

#if !_PLATFORM_OPTIMIZED_MEMSET

void *
_platform_memset(void *b, int c, size_t len) {
	unsigned char pattern[4];

	pattern[0] = (unsigned char)c;
	pattern[1] = (unsigned char)c;
	pattern[2] = (unsigned char)c;
	pattern[3] = (unsigned char)c;

	_platform_memset_pattern4(b, pattern, len);
	return b;
}

#if VARIANT_STATIC
void *
memset(void *b, int c, size_t len) {
	return _platform_memset(b, c, len);
}
#endif

#endif


#if !_PLATFORM_OPTIMIZED_BZERO

void
_platform_bzero(void *s, size_t n)
{
	_platform_memset(s, 0, n);
}

#if VARIANT_STATIC
void
bzero(void *s, size_t n) {
	_platform_bzero(s, n);
}

void
__bzero(void *s, size_t n) {
	_platform_bzero(s, n);
}
#endif

#endif
