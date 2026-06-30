




























#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_STRCHR

#include <stdlib.h>

char *
_platform_strchr(const char *p, int ch)
{
	char c;

	c = ch;
	for (;; ++p) {
		if (*p == c)
			return ((char *)p);
		if (*p == '\0')
			return (NULL);
	}

}

#if VARIANT_STATIC
char *
strchr(const char *p, int ch)
{
	return _platform_strchr(p, ch);
}
#endif

#endif
