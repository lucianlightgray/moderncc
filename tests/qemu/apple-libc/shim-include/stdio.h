#ifndef _COMPAT_STDIO_H
#define _COMPAT_STDIO_H
/* strnstr.c includes <stdio.h> only for NULL; nothing else from it is used. */
#ifndef NULL
#define NULL ((void *)0)
#endif
#endif
