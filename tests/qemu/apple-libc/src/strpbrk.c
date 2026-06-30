




























#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)strpbrk.c	8.1 (Berkeley) 6/4/93";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/string/strpbrk.c,v 1.6 2009/02/03 17:58:20 danger Exp $");

#include <string.h>




char *
strpbrk(const char *s1, const char *s2)
{
	const char *scanp;
	int c, sc;

	while ((c = *s1++) != 0) {
		for (scanp = s2; (sc = *scanp++) != '\0';)
			if (sc == c)
				return ((char *)(s1 - 1));
	}
	return (NULL);
}
