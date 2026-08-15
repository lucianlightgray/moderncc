#ifndef _MCC_OSX_WCHAR_H
#define _MCC_OSX_WCHAR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef __WCHAR_TYPE__ wchar_t;
typedef __WINT_TYPE__ wint_t;

typedef union {
	char __mbstate8[128];
	long long _mbstateL;
} mbstate_t;

#ifndef WEOF
#define WEOF ((wint_t)-1)
#endif
#define WCHAR_MAX 0x7fffffff
#define WCHAR_MIN (-0x7fffffff - 1)

size_t wcslen(const wchar_t *);
wchar_t *wcscpy(wchar_t *, const wchar_t *);
wchar_t *wcsncpy(wchar_t *, const wchar_t *, size_t);
wchar_t *wcscat(wchar_t *, const wchar_t *);
wchar_t *wcsncat(wchar_t *, const wchar_t *, size_t);
int wcscmp(const wchar_t *, const wchar_t *);
int wcsncmp(const wchar_t *, const wchar_t *, size_t);
wchar_t *wcschr(const wchar_t *, wchar_t);
wchar_t *wcsrchr(const wchar_t *, wchar_t);
wchar_t *wcsstr(const wchar_t *, const wchar_t *);

wchar_t *wmemcpy(wchar_t *, const wchar_t *, size_t);
wchar_t *wmemmove(wchar_t *, const wchar_t *, size_t);
wchar_t *wmemset(wchar_t *, wchar_t, size_t);
int wmemcmp(const wchar_t *, const wchar_t *, size_t);
wchar_t *wmemchr(const wchar_t *, wchar_t, size_t);

size_t mbrtowc(wchar_t *, const char *, size_t, mbstate_t *);
size_t wcrtomb(char *, wchar_t, mbstate_t *);
int mbsinit(const mbstate_t *);

long wcstol(const wchar_t *, wchar_t **, int);
unsigned long wcstoul(const wchar_t *, wchar_t **, int);
double wcstod(const wchar_t *, wchar_t **);

#ifdef __cplusplus
}
#endif

#endif
