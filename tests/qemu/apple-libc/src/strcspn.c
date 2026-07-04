#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/string/strcspn.c,v 1.5 2005/04/02 18:52:44 das Exp $");

#include <sys/types.h>
#include <limits.h>
#include <string.h>

#define IDX(c) ((u_char)(c) / LONG_BIT)
#define BIT(c) ((u_long)1 << ((u_char)(c) % LONG_BIT))

size_t
strcspn(const char *s, const char *charset) {

    const char *s1;
    u_long bit;
    u_long tbl[(UCHAR_MAX + 1) / LONG_BIT];
    int idx;

    if (*s == '\0')
        return (0);

#if LONG_BIT == 64
    tbl[0] = 1;
    tbl[3] = tbl[2] = tbl[1] = 0;
#else
    for (tbl[0] = idx = 1; idx < sizeof(tbl) / sizeof(tbl[0]); idx++)
        tbl[idx] = 0;
#endif
    for (; *charset != '\0'; charset++) {
        idx = IDX(*charset);
        bit = BIT(*charset);
        tbl[idx] |= bit;
    }

    for (s1 = s;; s1++) {
        idx = IDX(*s1);
        bit = BIT(*s1);
        if ((tbl[idx] & bit) != 0)
            break;
    }
    return (s1 - s);
}
