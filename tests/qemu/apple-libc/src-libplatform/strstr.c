































#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)strstr.c	8.1 (Berkeley) 6/4/93";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/string/strstr.c,v 1.6 2009/02/03 17:58:20 danger Exp $");

#include <sys/_types/_null.h>
#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_STRSTR




char *
_platform_strstr(const char *s, const char *find)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = _platform_strlen(find);
		do {
			do {
				if ((sc = *s++) == '\0')
					return (NULL);
			} while (sc != c);
		} while (_platform_strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}

#if VARIANT_STATIC
char *
strstr(const char *s, const char *find)
{
	return _platform_strstr(s, find);
}
#endif

#endif
